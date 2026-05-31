/*
 * mks-touchable.c
 *
 * Copyright 2023 Bilal Elmoussaoui <belmouss@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#include "config.h"

#include "mks-device-private.h"
#include "mks-touchable.h"
#include "mks-util-private.h"

/**
 * MksTouchable:
 * 
 * A virtualized QEMU touch device.
 */
struct _MksTouchable
{
  MksDevice          parent_instance;
  MksQemuMultiTouch *touch;
  int                max_slots;
};

struct _MksTouchableClass
{
  MksDeviceClass parent_instance;
};

G_DEFINE_FINAL_TYPE (MksTouchable, mks_touchable, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MAX_SLOTS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];


static void
mks_touchable_set_max_slots (MksTouchable *self,
                             int           max_slots)
{
  g_assert (MKS_IS_TOUCHABLE (self));
  // Per INPUT_EVENT_SLOTS_MIN / INPUT_EVENT_SLOTS_MAX in QEMU
  g_assert (max_slots >= 0 && max_slots <= 10);

  if (self->max_slots != max_slots)
    {
      self->max_slots = max_slots;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_SLOTS]);
    }
}

static void
mks_touchable_touch_notify_cb (MksTouchable      *self,
                               GParamSpec        *pspec,
                               MksQemuMultiTouch *touch)
{
  g_assert (MKS_IS_TOUCHABLE (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_MULTI_TOUCH (touch));

  if (strcmp (pspec->name, "max-slots") == 0)
    mks_touchable_set_max_slots (self, mks_qemu_multi_touch_get_max_slots (touch));
}

static void
mks_touchable_set_touch (MksTouchable      *self,
                         MksQemuMultiTouch *touch)
{
  g_assert (MKS_IS_TOUCHABLE (self));
  g_assert (!touch || MKS_QEMU_IS_MULTI_TOUCH (touch));
  g_assert (self->touch == NULL);

  if (g_set_object (&self->touch, touch))
    {
      g_signal_connect_object (self->touch,
                               "notify",
                               G_CALLBACK (mks_touchable_touch_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_touchable_set_max_slots (self, mks_qemu_multi_touch_get_max_slots (touch));
    }
}

static gboolean
mks_touchable_setup (MksDevice     *device,
                     MksQemuObject *object)
{
  MksTouchable *self = (MksTouchable *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_TOUCHABLE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_MULTI_TOUCH (iface))
        mks_touchable_set_touch (self, MKS_QEMU_MULTI_TOUCH (iface));
    }

  return self->touch != NULL;
}

static void
mks_touchable_dispose (GObject *object)
{
  MksTouchable *self = (MksTouchable *)object;

  g_clear_object (&self->touch);

  G_OBJECT_CLASS (mks_touchable_parent_class)->dispose (object);
}

static void
mks_touchable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MksTouchable *self = MKS_TOUCHABLE (object);

  switch (prop_id)
    {
    case PROP_MAX_SLOTS:
      g_value_set_int (value, mks_touchable_get_max_slots (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_touchable_class_init (MksTouchableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  device_class->setup = mks_touchable_setup;

  object_class->dispose = mks_touchable_dispose;
  object_class->get_property = mks_touchable_get_property;

  /**
   * MksTouchable:max-slots:
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
mks_touchable_init (MksTouchable *self)
{
}

static gboolean
check_touch (MksTouchable  *self,
             GError       **error)
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
 * mks_touchable_send_event:
 * @self: an #MksTouchable
 * @num_slot: the slot number
 * @x: the x absolute coordinate
 * @y: the y absolute coordinate
 *
 * Send a touch event.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_touchable_send_event (MksTouchable       *self,
                          MksTouchEventKind   kind,
                          guint64             num_slot,
                          double              x,
                          double              y)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_TOUCHABLE (self));

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

void
mks_touchable_send_event_async (MksTouchable       *self,
                                MksTouchEventKind   kind,
                                guint64             num_slot,
                                double              x,
                                double              y,
                                GCancellable       *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer            user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_touchable_send_event (self, kind, num_slot, x, y));
}

/**
 * mks_touchable_send_event_finish:
 * @self: a `MksTouchable`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Touchable.send_event].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_touchable_send_event_finish (MksTouchable  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_TOUCHABLE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_touchable_get_max_slots:
 * @self: A `MksTouchable`.
 * 
 * Returns the maximum number of slots.
 */
int
mks_touchable_get_max_slots (MksTouchable *self)
{
  g_return_val_if_fail (MKS_IS_TOUCHABLE (self), 0);

  return self->max_slots;
}
