/*
 * mks-mouse.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "mks-device-private.h"
#include "mks-mouse.h"

struct _MksMouse
{
  MksDevice parent_instance;
  MksQemuMouse *mouse;
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
mks_mouse_set_mouse (MksMouse     *self,
                     MksQemuMouse *mouse)
{
  g_assert (MKS_IS_MOUSE (self));
  g_assert (MKS_QEMU_IS_MOUSE (mouse));

  g_set_object (&self->mouse, mouse);
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

gboolean
mks_mouse_get_is_absolute (MksMouse *self)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);

  if (self->mouse)
    return mks_qemu_mouse_get_is_absolute (self->mouse);

  return FALSE;
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

static void
mks_mouse_press_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  MksQemuMouse *mouse = (MksQemuMouse *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_mouse_call_press_finish (mouse, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_mouse_press:
 * @self: an #MksMouse
 * @button: the #MksMouseButton that was pressed
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Presses a mouse button.
 */
void
mks_mouse_press (MksMouse            *self,
                 MksMouseButton       button,
                 GCancellable        *cancellable,
                 GAsyncReadyCallback  callback,
                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_MOUSE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_mouse_press);

  if (!check_mouse (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_mouse_call_press (self->mouse,
                               button,
                               cancellable,
                               mks_mouse_press_cb,
                               g_steal_pointer (&task));
}

gboolean
mks_mouse_press_finish (MksMouse      *self,
                        GAsyncResult  *result,
                        GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
mks_mouse_press_sync (MksMouse        *self,
                      MksMouseButton   button,
                      GCancellable    *cancellable,
                      GError         **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_mouse (self, error))
    return FALSE;

  return mks_qemu_mouse_call_press_sync (self->mouse, button, cancellable, error);
}

static void
mks_mouse_release_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  MksQemuMouse *mouse = (MksQemuMouse *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_mouse_call_release_finish (mouse, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_mouse_release:
 * @self: an #MksMouse
 * @button: the #MksMouseButton that was releaseed
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Releases a mouse button.
 */
void
mks_mouse_release (MksMouse            *self,
                   MksMouseButton       button,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_MOUSE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_mouse_release);

  if (!check_mouse (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_mouse_call_release (self->mouse,
                                 button,
                                 cancellable,
                                 mks_mouse_release_cb,
                                 g_steal_pointer (&task));
}

gboolean
mks_mouse_release_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
mks_mouse_release_sync (MksMouse        *self,
                        MksMouseButton   button,
                        GCancellable    *cancellable,
                        GError         **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_mouse (self, error))
    return FALSE;

  return mks_qemu_mouse_call_release_sync (self->mouse, button, cancellable, error);
}

static void
mks_mouse_move_to_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  MksQemuMouse *mouse = (MksQemuMouse *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_mouse_call_set_abs_position_finish (mouse, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_mouse_move_to:
 * @self: an #MksMouse
 * @x: the x coordinate
 * @y: the y coordinate
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Moves to the absolute position at coordinates (x,y).
 */
void
mks_mouse_move_to (MksMouse            *self,
                   guint                x,
                   guint                y,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_MOUSE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_mouse_move_to);

  if (!check_mouse (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_mouse_call_set_abs_position (self->mouse,
                                          x, y,
                                          cancellable,
                                          mks_mouse_move_to_cb,
                                          g_steal_pointer (&task));
}

gboolean
mks_mouse_move_to_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * mks_mouse_move_to:
 * @self: an #MksMouse
 * @x: the x coordinate
 * @y: the y coordinate
 * @cancellable: (nullable): a #GCancellable
 * @error: a location for a #GError, or %NULL
 *
 * Moves to the absolute position at coordinates (x,y).
 *
 * Returns: %TRUE if the operation was acknowledged by the Qemu instance;
 *   otherwise %FALSE and @error is set.
 */
gboolean
mks_mouse_move_to_sync (MksMouse      *self,
                        guint          x,
                        guint          y,
                        GCancellable  *cancellable,
                        GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_mouse (self, error))
    return FALSE;

  return mks_qemu_mouse_call_set_abs_position_sync (self->mouse, x, y, cancellable, error);
}

static void
mks_mouse_move_by_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  MksQemuMouse *mouse = (MksQemuMouse *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_mouse_call_rel_motion_finish (mouse, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_mouse_move_by:
 * @self: an #MksMouse
 * @delta_x: the x coordinate delta
 * @delta_y: the y coordinate delta
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Moves the mouse by delta_x and delta_y.
 */
void
mks_mouse_move_by (MksMouse            *self,
                   int                  delta_x,
                   int                  delta_y,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_MOUSE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_mouse_move_by);

  if (!check_mouse (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_mouse_call_rel_motion (self->mouse,
                                    delta_x, delta_y,
                                    cancellable,
                                    mks_mouse_move_by_cb,
                                    g_steal_pointer (&task));
}

gboolean
mks_mouse_move_by_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * mks_mouse_move_by:
 * @self: an #MksMouse
 * @delta_x: the x coordinate delta
 * @delta_y: the y coordinate delta
 * @cancellable: (nullable): a #GCancellable
 * @error: a location for a #GError, or %NULL
 *
 * Moves the mouse by delta_x and delta_y.
 *
 * Returns: %TRUE if the operation was acknowledged by the Qemu instance;
 *   otherwise %FALSE and @error is set.
 */
gboolean
mks_mouse_move_by_sync (MksMouse      *self,
                        int            delta_x,
                        int            delta_y,
                        GCancellable  *cancellable,
                        GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_mouse (self, error))
    return FALSE;

  return mks_qemu_mouse_call_rel_motion_sync (self->mouse, delta_x, delta_y, cancellable, error);
}
