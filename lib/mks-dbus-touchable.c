/* mks-dbus-touchable.c
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
#include "mks-dbus-touchable-private.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

/**
 * MksDBusTouchable:
 * 
 * A virtualized QEMU touch device.
 */
struct _MksDBusTouchable
{
  MksDevice          parent_instance;
  MksQemuMultiTouch *touch;
  int                max_slots;
};

struct _MksDBusTouchableClass
{
  MksTouchableClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksDBusTouchable, mks_dbus_touchable, MKS_TYPE_TOUCHABLE)

enum {
  PROP_0,
  PROP_MAX_SLOTS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static DexFuture *mks_dbus_touchable_send_event    (MksTouchable      *touchable,
                                                    MksTouchEventKind  kind,
                                                    guint64            num_slot,
                                                    double             x,
                                                    double             y);
static int        mks_dbus_touchable_get_max_slots (MksTouchable      *touchable);



static void
mks_dbus_touchable_set_max_slots (MksDBusTouchable *self,
                                  int               max_slots)
{
  g_assert (MKS_IS_DBUS_TOUCHABLE (self));
  // Per INPUT_EVENT_SLOTS_MIN / INPUT_EVENT_SLOTS_MAX in QEMU
  g_assert (max_slots >= 0 && max_slots <= 10);

  if (self->max_slots != max_slots)
    {
      self->max_slots = max_slots;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_SLOTS]);
    }
}

static void
mks_dbus_touchable_touch_notify_cb (MksDBusTouchable  *self,
                                    GParamSpec        *pspec,
                                    MksQemuMultiTouch *touch)
{
  g_assert (MKS_IS_DBUS_TOUCHABLE (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_MULTI_TOUCH (touch));

  if (strcmp (pspec->name, "max-slots") == 0)
    mks_dbus_touchable_set_max_slots (self, mks_qemu_multi_touch_get_max_slots (touch));
}

static void
mks_dbus_touchable_set_touch (MksDBusTouchable  *self,
                              MksQemuMultiTouch *touch)
{
  g_assert (MKS_IS_DBUS_TOUCHABLE (self));
  g_assert (!touch || MKS_QEMU_IS_MULTI_TOUCH (touch));
  g_assert (self->touch == NULL);

  if (g_set_object (&self->touch, touch))
    {
      g_signal_connect_object (self->touch,
                               "notify",
                               G_CALLBACK (mks_dbus_touchable_touch_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_dbus_touchable_set_max_slots (self, mks_qemu_multi_touch_get_max_slots (touch));
    }
}

static gboolean
mks_dbus_touchable_setup (MksDevice *device,
                          GObject   *object)
{
  MksDBusTouchable *self = (MksDBusTouchable *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_TOUCHABLE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_MULTI_TOUCH (iface))
        mks_dbus_touchable_set_touch (self, MKS_QEMU_MULTI_TOUCH (iface));
    }

  return self->touch != NULL;
}

static void
mks_dbus_touchable_dispose (GObject *object)
{
  MksDBusTouchable *self = (MksDBusTouchable *)object;

  g_clear_object (&self->touch);

  G_OBJECT_CLASS (mks_dbus_touchable_parent_class)->dispose (object);
}

static void
mks_dbus_touchable_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MksDBusTouchable *self = MKS_DBUS_TOUCHABLE (object);

  switch (prop_id)
    {
    case PROP_MAX_SLOTS:
      g_value_set_int (value, mks_dbus_touchable_get_max_slots (MKS_TOUCHABLE (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_dbus_touchable_class_init (MksDBusTouchableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);
  MksTouchableClass *touchable_class = MKS_TOUCHABLE_CLASS (klass);

  touchable_class->send_event = mks_dbus_touchable_send_event;
  touchable_class->get_max_slots = mks_dbus_touchable_get_max_slots;


  device_class->setup = mks_dbus_touchable_setup;

  object_class->dispose = mks_dbus_touchable_dispose;
  object_class->get_property = mks_dbus_touchable_get_property;

  /**
   * MksDBusTouchable:max-slots:
   * 
   * The maximum number of slots.
   */
  properties [PROP_MAX_SLOTS] =
    g_param_spec_int ("max-slots", NULL, NULL,
                      0, 10, 0,
                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_dbus_touchable_init (MksDBusTouchable *self)
{
}

static gboolean
check_touch (MksDBusTouchable  *self,
             GError           **error)
{
  if (self->touch == NULL)
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
 * mks_dbus_touchable_send_event:
 * @self: an #MksDBusTouchable
 * @num_slot: the slot number
 * @x: the x absolute coordinate
 * @y: the y absolute coordinate
 *
 * Send a touch event.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_touchable_send_event (MksTouchable      *touchable,
                               MksTouchEventKind  kind,
                               guint64            num_slot,
                               double             x,
                               double             y)
{
  MksDBusTouchable *self = MKS_DBUS_TOUCHABLE (touchable);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_TOUCHABLE (self));

  if (!check_touch (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_multi_touch_call_send_event_future (self->touch,
                                                                         kind,
                                                                         num_slot,
                                                                         x,
                                                                         y),
                            begin_time,
                            "touchable.send-event");
}

/**
 * mks_dbus_touchable_get_max_slots:
 * @self: A `MksDBusTouchable`.
 * 
 * Returns the maximum number of slots.
 */
static int
mks_dbus_touchable_get_max_slots (MksTouchable *touchable)
{
  MksDBusTouchable *self = MKS_DBUS_TOUCHABLE (touchable);

  g_return_val_if_fail (MKS_IS_DBUS_TOUCHABLE (self), 0);

  return self->max_slots;
}
