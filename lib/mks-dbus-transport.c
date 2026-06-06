/* mks-dbus-transport.c
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

#include "mks-transport-private.h"
#include "mks-dbus-chardev-private.h"
#include "mks-dbus-clipboard-private.h"
#include "mks-dbus-transport.h"
#include "mks-dbus-keyboard-private.h"
#include "mks-dbus-microphone-private.h"
#include "mks-dbus-mouse-private.h"
#include "mks-dbus-screen-private.h"
#include "mks-dbus-speaker-private.h"
#include "mks-dbus-touchable-private.h"
#include "mks-device-private.h"
#include "mks-qemu.h"

#define QEMU_BUS_NAME "org.qemu"

struct _MksDBusTransport
{
  MksTransport parent_instance;

  GDBusConnection    *connection;
  GDBusObjectManager *object_manager;
  GListStore         *devices;
  MksQemuObject      *clipboard_object;
  MksClipboard       *clipboard;
  MksQemuObject      *vm_object;
  MksQemuVM          *vm;
  char               *bus_name;
};

struct _MksDBusTransportClass
{
  MksTransportClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksDBusTransport, mks_dbus_transport, MKS_TYPE_TRANSPORT)

enum {
  PROP_0,
  PROP_CONNECTION,
  PROP_BUS_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_dbus_transport_set_name (MksDBusTransport *self,
                             const char       *name)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));

  _mks_transport_set_name (MKS_TRANSPORT (self), name);
}

static void
mks_dbus_transport_set_uuid (MksDBusTransport *self,
                             const char       *uuid)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));

  _mks_transport_set_uuid (MKS_TRANSPORT (self), uuid);
}

static void
mks_dbus_transport_vm_notify_cb (MksDBusTransport *self,
                                 GParamSpec       *pspec,
                                 MksQemuVM        *vm)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_VM (vm));

  if (strcmp (pspec->name, "name") == 0)
    mks_dbus_transport_set_name (self, mks_qemu_vm_get_name (vm));
  else if (strcmp (pspec->name, "uuid") == 0)
    mks_dbus_transport_set_uuid (self, mks_qemu_vm_get_uuid (vm));
}

static void
mks_dbus_transport_set_vm (MksDBusTransport *self,
                           MksQemuObject    *vm_object,
                           MksQemuVM        *vm)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (vm_object));
  g_assert (MKS_QEMU_IS_VM (vm));

  if (self->vm != NULL)
    return;

  g_set_object (&self->vm_object, vm_object);
  g_set_object (&self->vm, vm);

  g_signal_connect_object (vm,
                           "notify",
                           G_CALLBACK (mks_dbus_transport_vm_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  mks_dbus_transport_set_name (self, mks_qemu_vm_get_name (vm));
  mks_dbus_transport_set_uuid (self, mks_qemu_vm_get_uuid (vm));
}

static gboolean
mks_dbus_transport_has_device (MksDBusTransport *self,
                               GType             device_type,
                               MksQemuObject    *object)
{
  GListModel *model;
  guint n_items;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (g_type_is_a (device_type, MKS_TYPE_DEVICE));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  model = G_LIST_MODEL (self->devices);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksDevice) device = g_list_model_get_item (model, i);

      if (G_TYPE_CHECK_INSTANCE_TYPE (device, device_type) &&
          _mks_device_get_object (device) == G_OBJECT (object))
        return TRUE;
    }

  return FALSE;
}

static void
mks_dbus_transport_add_device_once (MksDBusTransport *self,
                                    GType             device_type,
                                    MksQemuObject    *object)
{
  g_autoptr(MksDevice) device = NULL;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (g_type_is_a (device_type, MKS_TYPE_DEVICE));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  if (mks_dbus_transport_has_device (self, device_type, object))
    return;

  if (!(device = _mks_device_new (device_type, MKS_TRANSPORT (self), G_OBJECT (object))))
    return;

  g_list_store_append (self->devices, device);
  _mks_transport_emit_device_added (MKS_TRANSPORT (self), device);
}

static void
mks_dbus_transport_add_interface (MksDBusTransport *self,
                                  MksQemuObject    *object,
                                  GDBusInterface   *iface)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));
  g_assert (G_IS_DBUS_INTERFACE (iface));

  if (MKS_QEMU_IS_VM (iface))
    mks_dbus_transport_set_vm (self, object, MKS_QEMU_VM (iface));
  else if (MKS_QEMU_IS_CLIPBOARD (iface))
    {
      if (self->clipboard == NULL)
        {
          self->clipboard_object = g_object_ref (object);
          self->clipboard = _mks_dbus_clipboard_new (MKS_TRANSPORT (self), object);
          _mks_transport_set_clipboard (MKS_TRANSPORT (self), self->clipboard);
        }
    }
  else if (MKS_QEMU_IS_AUDIO (iface))
    {
      mks_dbus_transport_add_device_once (self, MKS_TYPE_DBUS_SPEAKER, object);
      mks_dbus_transport_add_device_once (self, MKS_TYPE_DBUS_MICROPHONE, object);
    }
  else if (MKS_QEMU_IS_CONSOLE (iface))
    mks_dbus_transport_add_device_once (self, MKS_TYPE_DBUS_SCREEN, object);
  else if (MKS_QEMU_IS_CHARDEV (iface))
    mks_dbus_transport_add_device_once (self, MKS_TYPE_DBUS_CHARDEV, object);
}

static void
mks_dbus_transport_object_added_cb (MksDBusTransport   *self,
                                    MksQemuObject      *object,
                                    GDBusObjectManager *manager)
{
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));
  g_assert (G_IS_DBUS_OBJECT_MANAGER (manager));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    mks_dbus_transport_add_interface (self, object, iter->data);
}

static void
mks_dbus_transport_interface_added_cb (MksDBusTransport   *self,
                                       MksQemuObject      *object,
                                       GDBusInterface     *iface,
                                       GDBusObjectManager *manager)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));
  g_assert (G_IS_DBUS_INTERFACE (iface));
  g_assert (G_IS_DBUS_OBJECT_MANAGER (manager));

  mks_dbus_transport_add_interface (self, object, iface);
}

static void
mks_dbus_transport_remove_devices_for_object (MksDBusTransport *self,
                                              MksQemuObject    *object)
{
  guint n_items;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(MksDevice) device = g_list_model_get_item (G_LIST_MODEL (self->devices), i - 1);

      if (_mks_device_get_object (device) == G_OBJECT (object))
        {
          _mks_transport_emit_device_removed (MKS_TRANSPORT (self), device);
          g_list_store_remove (self->devices, i - 1);
        }
    }
}

static void
mks_dbus_transport_remove_all_devices (MksDBusTransport *self)
{
  guint n_items;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(MksDevice) device = g_list_model_get_item (G_LIST_MODEL (self->devices), i - 1);

      _mks_transport_emit_device_removed (MKS_TRANSPORT (self), device);
      g_list_store_remove (self->devices, i - 1);
    }
}

static void
mks_dbus_transport_object_removed_cb (MksDBusTransport   *self,
                                      MksQemuObject      *object,
                                      GDBusObjectManager *manager)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));
  g_assert (G_IS_DBUS_OBJECT_MANAGER (manager));

  if (object == self->vm_object)
    {
      g_clear_object (&self->vm);
      g_clear_object (&self->vm_object);
      mks_dbus_transport_set_name (self, NULL);
      mks_dbus_transport_set_uuid (self, NULL);
      mks_dbus_transport_remove_all_devices (self);
      return;
    }
  else if (object == self->clipboard_object)
    {
      g_clear_object (&self->clipboard);
      g_clear_object (&self->clipboard_object);
      _mks_transport_set_clipboard (MKS_TRANSPORT (self), NULL);
    }

  mks_dbus_transport_remove_devices_for_object (self, object);
}

static void
mks_dbus_transport_set_object_manager (MksDBusTransport   *self,
                                       GDBusObjectManager *object_manager)
{
  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (!object_manager || MKS_QEMU_IS_OBJECT_MANAGER_CLIENT (object_manager));
  g_assert (self->object_manager == NULL);

  if (g_set_object (&self->object_manager, object_manager))
    {
      g_autolist(GDBusObjectProxy) objects = NULL;

      g_signal_connect_object (object_manager,
                               "object-added",
                               G_CALLBACK (mks_dbus_transport_object_added_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (object_manager,
                               "object-removed",
                               G_CALLBACK (mks_dbus_transport_object_removed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (object_manager,
                               "interface-added",
                               G_CALLBACK (mks_dbus_transport_interface_added_cb),
                               self,
                               G_CONNECT_SWAPPED);

      objects = g_dbus_object_manager_get_objects (object_manager);
      for (const GList *iter = objects; iter; iter = iter->next)
        mks_dbus_transport_object_added_cb (self, iter->data, object_manager);
    }
}

typedef struct _MksDBusTransportStart
{
  MksDBusTransport *self;
  DexPromise       *promise;
} MksDBusTransportStart;

static void
mks_dbus_transport_start_free (MksDBusTransportStart *state)
{
  g_clear_object (&state->self);
  dex_clear (&state->promise);
  g_free (state);
}

static void
mks_dbus_transport_start_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GDBusObjectManager) object_manager = NULL;
  g_autoptr(GError) error = NULL;
  MksDBusTransportStart *state = user_data;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (MKS_IS_DBUS_TRANSPORT (state->self));

  object_manager = mks_qemu_object_manager_client_new_finish (result, &error);

  if (error != NULL)
    dex_promise_reject (state->promise, g_steal_pointer (&error));
  else
    {
      mks_dbus_transport_set_object_manager (state->self, object_manager);
      dex_promise_resolve_boolean (state->promise, TRUE);
    }

  mks_dbus_transport_start_free (state);
}

static DexFuture *
mks_dbus_transport_start (MksTransport *transport)
{
  MksDBusTransport *self = MKS_DBUS_TRANSPORT (transport);
  MksDBusTransportStart *state;
  DexPromise *promise;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));

  if (self->object_manager != NULL)
    return dex_future_new_true ();

  if (self->connection == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_CONNECTED,
                                  "Not connected");

  promise = dex_promise_new_cancellable ();
  state = g_new0 (MksDBusTransportStart, 1);
  state->self = g_object_ref (self);
  state->promise = dex_ref (promise);

  mks_qemu_object_manager_client_new (self->connection,
                                      G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
                                      self->bus_name,
                                      "/org/qemu/Display1",
                                      dex_promise_get_cancellable (promise),
                                      mks_dbus_transport_start_cb,
                                      state);

  return DEX_FUTURE (promise);
}

static void
mks_dbus_transport_add_observer (MksTransport               *transport,
                                 const MksTransportObserver *observer)
{
  MksDBusTransport *self = MKS_DBUS_TRANSPORT (transport);
  guint n_items;

  g_assert (MKS_IS_DBUS_TRANSPORT (self));
  g_assert (observer != NULL);

  if (g_ptr_array_find (transport->observers, observer, NULL))
    return;

  MKS_TRANSPORT_CLASS (mks_dbus_transport_parent_class)->add_observer (transport, observer);

  if (observer->device_added == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksDevice) device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);

      observer->device_added (observer->user_data, device);
    }
}

static void
mks_dbus_transport_dispose (GObject *object)
{
  MksDBusTransport *self = (MksDBusTransport *)object;

  g_clear_object (&self->object_manager);
  if (self->devices != NULL)
    g_list_store_remove_all (self->devices);
  g_clear_object (&self->clipboard);
  g_clear_object (&self->clipboard_object);
  g_clear_object (&self->vm);
  g_clear_object (&self->vm_object);
  g_clear_object (&self->connection);
  g_clear_pointer (&self->bus_name, g_free);

  G_OBJECT_CLASS (mks_dbus_transport_parent_class)->dispose (object);
}

static void
mks_dbus_transport_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MksDBusTransport *self = MKS_DBUS_TRANSPORT (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_BUS_NAME:
      g_value_set_string (value, self->bus_name);
      break;

    default:
      G_OBJECT_CLASS (mks_dbus_transport_parent_class)->get_property (object, prop_id, value, pspec);
    }
}

static void
mks_dbus_transport_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MksDBusTransport *self = MKS_DBUS_TRANSPORT (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    case PROP_BUS_NAME:
      self->bus_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_dbus_transport_class_init (MksDBusTransportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksTransportClass *transport_class = MKS_TRANSPORT_CLASS (klass);

  object_class->dispose = mks_dbus_transport_dispose;
  object_class->get_property = mks_dbus_transport_get_property;
  object_class->set_property = mks_dbus_transport_set_property;

  transport_class->start = mks_dbus_transport_start;
  transport_class->add_observer = mks_dbus_transport_add_observer;

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         G_TYPE_DBUS_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", NULL, NULL,
                         QEMU_BUS_NAME,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_dbus_transport_init (MksDBusTransport *self)
{
  self->devices = g_list_store_new (MKS_TYPE_DEVICE);
}

/**
 * mks_dbus_transport_new:
 * @connection: a #GDBusConnection
 * @bus_name: (nullable): the name to connect to, or %NULL for peer connections
 *
 * Creates a transport using @connection.
 *
 * Returns: (transfer full): a #MksTransport
 */
MksTransport *
mks_dbus_transport_new (GDBusConnection *connection,
                        const char      *bus_name)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  return g_object_new (MKS_TYPE_DBUS_TRANSPORT,
                       "connection", connection,
                       "bus-name", bus_name,
                       NULL);
}

/**
 * mks_dbus_transport_get_connection:
 * @self: a #MksDBusTransport
 *
 * Gets the D-Bus connection used by the transport.
 *
 * Returns: (transfer none): a #GDBusConnection
 */
GDBusConnection *
mks_dbus_transport_get_connection (MksDBusTransport *self)
{
  g_return_val_if_fail (MKS_IS_DBUS_TRANSPORT (self), NULL);

  return self->connection;
}

/**
 * mks_dbus_transport_get_bus_name:
 * @self: a #MksDBusTransport
 *
 * Gets the D-Bus name used by the transport.
 *
 * Returns: (nullable): the bus name
 */
const char *
mks_dbus_transport_get_bus_name (MksDBusTransport *self)
{
  g_return_val_if_fail (MKS_IS_DBUS_TRANSPORT (self), NULL);

  return self->bus_name;
}
