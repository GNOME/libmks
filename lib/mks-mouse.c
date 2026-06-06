/* mks-mouse.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "mks-device-private.h"
#include "mks-mouse.h"
#include "mks-util-private.h"

/**
 * MksMouse:
 * 
 * A virtualized QEMU mouse.
 */

struct _MksMouse
{
  MksDevice parent_instance;
  MksQemuMouse *mouse;
  double last_known_x;
  double last_known_y;

  guint is_absolute: 1;
};

struct _MksMouseClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksMouse, mks_mouse, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_IS_ABSOLUTE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_mouse_set_is_absolute (MksMouse *self,
                           gboolean  is_absolute)
{
  g_assert (MKS_IS_MOUSE (self));

  if (self->is_absolute != is_absolute)
    {
      self->is_absolute = is_absolute;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_ABSOLUTE]);
    }
}

static void
mks_mouse_mouse_notify_cb (MksMouse     *self,
                           GParamSpec   *pspec,
                           MksQemuMouse *mouse)
{
  g_assert (MKS_IS_MOUSE (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_MOUSE (mouse));

  if (strcmp (pspec->name, "is-absolute") == 0)
    mks_mouse_set_is_absolute (self, mks_qemu_mouse_get_is_absolute (mouse));
}

static void
mks_mouse_set_mouse (MksMouse     *self,
                     MksQemuMouse *mouse)
{
  g_assert (MKS_IS_MOUSE (self));
  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (self->mouse == NULL);

  if (g_set_object (&self->mouse, mouse))
    {
      g_signal_connect_object (self->mouse,
                               "notify",
                               G_CALLBACK (mks_mouse_mouse_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_mouse_set_is_absolute (self, mks_qemu_mouse_get_is_absolute (mouse));
    }
}

static gboolean
mks_mouse_setup (MksDevice     *device,
                 MksQemuObject *object)
{
  MksMouse *self = (MksMouse *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_MOUSE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_MOUSE (iface))
        mks_mouse_set_mouse (self, MKS_QEMU_MOUSE (iface));
    }

  return self->mouse != NULL;
}

static void
mks_mouse_dispose (GObject *object)
{
  MksMouse *self = (MksMouse *)object;

  g_clear_object (&self->mouse);

  G_OBJECT_CLASS (mks_mouse_parent_class)->dispose (object);
}

static void
mks_mouse_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MksMouse *self = MKS_MOUSE (object);

  switch (prop_id)
    {
    case PROP_IS_ABSOLUTE:
      g_value_set_boolean (value, mks_mouse_get_is_absolute (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_mouse_class_init (MksMouseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_mouse_dispose;
  object_class->get_property = mks_mouse_get_property;

  device_class->setup = mks_mouse_setup;

  /**
   * MksMouse:is-absolute:
   * 
   * Whether the mouse is using absolute movements.
   */
  properties [PROP_IS_ABSOLUTE] =
    g_param_spec_boolean ("is-absolute", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_mouse_init (MksMouse *self)
{
}

/**
 * mks_mouse_get_is_absolute:
 * @self: A `MksMouse`.
 * 
 * Whether the mouse is using absolute movements.
 */
gboolean
mks_mouse_get_is_absolute (MksMouse *self)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);

  return self->is_absolute;
}

static gboolean
check_mouse (MksMouse  *self,
             GError   **error)
{
  if (self->mouse == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_CONNECTED,
                           "Not connected");
      return FALSE;
    }

  return TRUE;
}

/**
 * mks_mouse_press:
 * @self: an #MksMouse
 * @button: the #MksMouseButton that was pressed
 *
 * Presses a mouse button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_press (MksMouse            *self,
                 MksMouseButton       button)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_press_future (self->mouse, button),
                            begin_time,
                            "mouse.press");
}

void
mks_mouse_press_async (MksMouse            *self,
                       MksMouseButton       button,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_mouse_press (self, button));
}

/**
 * mks_mouse_press_finish:
 * @self: a `MksMouse`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Mouse.press].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_mouse_press_finish (MksMouse      *self,
                        GAsyncResult  *result,
                        GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_mouse_release:
 * @self: an #MksMouse
 * @button: the #MksMouseButton that was released
 *
 * Releases a mouse button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_release (MksMouse            *self,
                   MksMouseButton       button)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_release_future (self->mouse, button),
                            begin_time,
                            "mouse.release");
}

void
mks_mouse_release_async (MksMouse            *self,
                         MksMouseButton       button,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_mouse_release (self, button));
}

/**
 * mks_mouse_release_finish:
 * @self: a `MksMouse`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Mouse.release].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_mouse_release_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_mouse_move_to:
 * @self: an #MksMouse
 * @x: the x coordinate
 * @y: the y coordinate
 *
 * Moves to the absolute position at coordinates (x,y).
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_move_to (MksMouse            *self,
                   guint                x,
                   guint                y)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  self->last_known_x = x;
  self->last_known_y = y;

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_set_abs_position_future (self->mouse, x, y),
                            begin_time,
                            "mouse.move-to");
}

void
mks_mouse_move_to_async (MksMouse            *self,
                         guint                x,
                         guint                y,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_mouse_move_to (self, x, y));
}

/**
 * mks_mouse_move_to_finish:
 * @self: a `MksMouse`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Mouse.move_to].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_mouse_move_to_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_mouse_move_by:
 * @self: an #MksMouse
 * @delta_x: the x coordinate delta
 * @delta_y: the y coordinate delta
 *
 * Moves the mouse by delta_x and delta_y.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_move_by (MksMouse            *self,
                   int                  delta_x,
                   int                  delta_y)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  self->last_known_x += delta_x;
  self->last_known_y += delta_y;

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_rel_motion_future (self->mouse, delta_x, delta_y),
                            begin_time,
                            "mouse.move-by");
}

void
mks_mouse_move_by_async (MksMouse            *self,
                         int                  delta_x,
                         int                  delta_y,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_mouse_move_by (self, delta_x, delta_y));
}

/**
 * mks_mouse_move_by_finish:
 * @self: a `MksMouse`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Mouse.move_by].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_mouse_move_by_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}
