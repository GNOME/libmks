/* mks-dbus-mouse.c
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
#include "mks-dbus-mouse-private.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

/**
 * MksDBusMouse:
 * 
 * A virtualized QEMU mouse.
 */

struct _MksDBusMouse
{
  MksMouse      parent_instance;
  MksQemuMouse *mouse;
  double        last_known_x;
  double        last_known_y;

  guint is_absolute: 1;
};

struct _MksDBusMouseClass
{
  MksMouseClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksDBusMouse, mks_dbus_mouse, MKS_TYPE_MOUSE)

enum {
  PROP_0,
  PROP_IS_ABSOLUTE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean   mks_dbus_mouse_get_is_absolute (MksMouse       *mouse);
static DexFuture *mks_dbus_mouse_press           (MksMouse       *mouse,
                                                  MksMouseButton  button);
static DexFuture *mks_dbus_mouse_release         (MksMouse       *mouse,
                                                  MksMouseButton  button);
static DexFuture *mks_dbus_mouse_move_to         (MksMouse       *mouse,
                                                  guint           x,
                                                  guint           y);
static DexFuture *mks_dbus_mouse_move_by         (MksMouse       *mouse,
                                                  int             delta_x,
                                                  int             delta_y);


static void
mks_dbus_mouse_set_is_absolute (MksDBusMouse *self,
                                gboolean      is_absolute)
{
  g_assert (MKS_IS_DBUS_MOUSE (self));

  if (self->is_absolute != is_absolute)
    {
      self->is_absolute = is_absolute;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_ABSOLUTE]);
    }
}

static void
mks_dbus_mouse_mouse_notify_cb (MksDBusMouse *self,
                                GParamSpec   *pspec,
                                MksQemuMouse *mouse)
{
  g_assert (MKS_IS_DBUS_MOUSE (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_MOUSE (mouse));

  if (strcmp (pspec->name, "is-absolute") == 0)
    mks_dbus_mouse_set_is_absolute (self, mks_qemu_mouse_get_is_absolute (mouse));
}

static void
mks_dbus_mouse_set_mouse (MksDBusMouse *self,
                          MksQemuMouse *mouse)
{
  g_assert (MKS_IS_DBUS_MOUSE (self));
  g_assert (MKS_QEMU_IS_MOUSE (mouse));
  g_assert (self->mouse == NULL);

  if (g_set_object (&self->mouse, mouse))
    {
      g_signal_connect_object (self->mouse,
                               "notify",
                               G_CALLBACK (mks_dbus_mouse_mouse_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_dbus_mouse_set_is_absolute (self, mks_qemu_mouse_get_is_absolute (mouse));
    }
}

static gboolean
mks_dbus_mouse_setup (MksDevice *device,
                      GObject   *object)
{
  MksDBusMouse *self = (MksDBusMouse *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_MOUSE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_MOUSE (iface))
        mks_dbus_mouse_set_mouse (self, MKS_QEMU_MOUSE (iface));
    }

  return self->mouse != NULL;
}

static void
mks_dbus_mouse_dispose (GObject *object)
{
  MksDBusMouse *self = (MksDBusMouse *)object;

  g_clear_object (&self->mouse);

  G_OBJECT_CLASS (mks_dbus_mouse_parent_class)->dispose (object);
}

static void
mks_dbus_mouse_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (object);

  switch (prop_id)
    {
    case PROP_IS_ABSOLUTE:
      g_value_set_boolean (value, mks_dbus_mouse_get_is_absolute (MKS_MOUSE (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_dbus_mouse_class_init (MksDBusMouseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);
  MksMouseClass *mouse_class = MKS_MOUSE_CLASS (klass);

  mouse_class->get_is_absolute = mks_dbus_mouse_get_is_absolute;
  mouse_class->press = mks_dbus_mouse_press;
  mouse_class->release = mks_dbus_mouse_release;
  mouse_class->move_to = mks_dbus_mouse_move_to;
  mouse_class->move_by = mks_dbus_mouse_move_by;


  object_class->dispose = mks_dbus_mouse_dispose;
  object_class->get_property = mks_dbus_mouse_get_property;

  device_class->setup = mks_dbus_mouse_setup;

  /**
   * MksDBusMouse:is-absolute:
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
mks_dbus_mouse_init (MksDBusMouse *self)
{
}

/**
 * mks_dbus_mouse_get_is_absolute:
 * @self: A `MksDBusMouse`.
 * 
 * Whether the mouse is using absolute movements.
 */
static gboolean
mks_dbus_mouse_get_is_absolute (MksMouse *mouse)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (mouse);

  g_return_val_if_fail (MKS_IS_DBUS_MOUSE (self), FALSE);

  return self->is_absolute;
}

static gboolean
check_mouse (MksDBusMouse  *self,
             GError       **error)
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
 * mks_dbus_mouse_press:
 * @self: an #MksDBusMouse
 * @button: the #MksMouseButton that was pressed
 *
 * Presses a mouse button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_mouse_press (MksMouse       *mouse,
                      MksMouseButton  button)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (mouse);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_MOUSE (self));

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_press_future (self->mouse, button),
                            begin_time,
                            "mouse.press");
}

/**
 * mks_dbus_mouse_release:
 * @self: an #MksDBusMouse
 * @button: the #MksMouseButton that was released
 *
 * Releases a mouse button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_mouse_release (MksMouse       *mouse,
                        MksMouseButton  button)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (mouse);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_MOUSE (self));

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_release_future (self->mouse, button),
                            begin_time,
                            "mouse.release");
}

/**
 * mks_dbus_mouse_move_to:
 * @self: an #MksDBusMouse
 * @x: the x coordinate
 * @y: the y coordinate
 *
 * Moves to the absolute position at coordinates (x,y).
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_mouse_move_to (MksMouse *mouse,
                        guint     x,
                        guint     y)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (mouse);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_MOUSE (self));

  self->last_known_x = x;
  self->last_known_y = y;

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_set_abs_position_future (self->mouse, x, y),
                            begin_time,
                            "mouse.move-to");
}

/**
 * mks_dbus_mouse_move_by:
 * @self: an #MksDBusMouse
 * @delta_x: the x coordinate delta
 * @delta_y: the y coordinate delta
 *
 * Moves the mouse by delta_x and delta_y.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_mouse_move_by (MksMouse *mouse,
                        int       delta_x,
                        int       delta_y)
{
  MksDBusMouse *self = MKS_DBUS_MOUSE (mouse);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_MOUSE (self));

  self->last_known_x += delta_x;
  self->last_known_y += delta_y;

  if (!check_mouse (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_mouse_call_rel_motion_future (self->mouse, delta_x, delta_y),
                            begin_time,
                            "mouse.move-by");
}
