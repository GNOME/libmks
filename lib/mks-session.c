/*
 * mks-session.c
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

#include "mks-device.h"
#include "mks-read-only-list-model-private.h"
#include "mks-qemu.h"
#include "mks-session.h"

/**
 * SECTION:mks-session
 * @Title: MksSession
 * @Short_description: Session connected to a Qemu VM
 *
 * The #MksSession represents a connection to a Qemu VM instance. It contains
 * devices such as the mouse, keyboard, and screen which can be used with GTK.
 *
 * You may monitor #MksSession:devices using #GListModel:items-changed to be
 * notified of changes to available devices in the session.
 *
 * # Connecting To Qemu
 *
 * To use #MksSession, you should create your Qemu instance using `dbus` for
 * the various devices that support it. You'll need to provide your P2P D-Bus
 * address when connecting to Qemu.
 *
 * Using the same #GDBusConnection, create a #MksSession with
 * mks_session_new_for_connection(). The #MksSession instance will negotiate
 * with the peer to determine what devices are available and expose them
 * via the #MksSession:devices #GListModel.
 *
 * # Creating Widgets
 *
 * You can create a new widget to embed in your application by waiting for
 * a #MksScreen to become available in the list model or connecting to the
 * #GObject::notify signal for the #MksSession:screen property.
 */

static gboolean mks_session_initable_init              (GInitable            *initable,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
static void     mks_session_async_initable_init_async  (GAsyncInitable       *async_initable,
                                                        int                   io_priority,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
static gboolean mks_session_async_initable_init_finish (GAsyncInitable       *async_initable,
                                                        GAsyncResult         *result,
                                                        GError              **error);

struct _MksSession
{
  GObject parent_instance;

  /* @connection is used to communicate with the Qemu instance. It is expected
   * to be a private point-to-point connection over a Unix socket, socketpair(),
   * or other channel capable of passing FDs between peers.
   */
  GDBusConnection *connection;

  /* As devices are discovered from the Qemu instance, MksDevice-based objects
   * are created for them and stored in @devices. Applications will work with
   * these objects to perform operations on the Qemu instance. When we discover
   * the devices have been removed, we drop them from the listmodel.
   */
  GListStore *devices;

  /* @devices_read_only is a #MksReadOnlyListModel of @devices so that we may
   * allow observation of the #GListModel, but without access to perturb it.
   * Such opaqueness provides us a bit more assurance we don't leak
   * implementation details into the API.
   */
  GListModel *devices_read_only;

  /* @vm contains our "top-level" proxy object to the Qemu instance. This is
   * where access to other devices begins. It is setup during either GInitable
   * or GAsyncInitable initialization.
   */
  MksQemuVM *vm;
};

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = mks_session_initable_init;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = mks_session_async_initable_init_async;
  iface->init_finish = mks_session_async_initable_init_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksSession, mks_session, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_CONNECTION,
  PROP_DEVICES,
  PROP_NAME,
  PROP_UUID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_session_set_connection (MksSession      *self,
                            GDBusConnection *connection)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (!connection || G_IS_DBUS_CONNECTION (connection));
  g_assert (self->connection == NULL);

  if (connection == NULL)
    {
      g_critical ("%s created without a GDBusConnection, this cannot work.",
                  G_OBJECT_TYPE_NAME (self));
      return;
    }

  self->connection = g_object_ref (connection);
}

static void
mks_session_vm_notify_cb (MksSession *self,
                          GParamSpec *pspec,
                          MksQemuVM  *vm)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_VM (vm));

  if (0) {}
  else if (strcmp (pspec->name, "name") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
  else if (strcmp (pspec->name, "uuid") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_UUID]);
}

static void
mks_session_set_vm (MksSession *self,
                    MksQemuVM  *vm)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_QEMU_IS_VM (vm));

  g_signal_connect_object (vm,
                           "notify",
                           G_CALLBACK (mks_session_vm_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->vm = g_object_ref (vm);
}

static void
mks_session_dispose (GObject *object)
{
  MksSession *self = (MksSession *)object;

  if (self->devices != NULL)
    g_list_store_remove_all (self->devices);

  G_OBJECT_CLASS (mks_session_parent_class)->dispose (object);
}

static void
mks_session_finalize (GObject *object)
{
  MksSession *self = (MksSession *)object;

  g_clear_object (&self->vm);
  g_clear_object (&self->devices);
  g_clear_object (&self->devices_read_only);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (mks_session_parent_class)->finalize (object);
}

static void
mks_session_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksSession *self = MKS_SESSION (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, mks_session_get_connection (self));
      break;

    case PROP_DEVICES:
      g_value_set_object (value, mks_session_get_devices (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, mks_session_get_name (self));
      break;

    case PROP_UUID:
      g_value_set_string (value, mks_session_get_uuid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_session_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MksSession *self = MKS_SESSION (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      mks_session_set_connection (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_session_class_init (MksSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_session_dispose;
  object_class->finalize = mks_session_finalize;
  object_class->get_property = mks_session_get_property;
  object_class->set_property = mks_session_set_property;

  /**
   * MksSession:connection:
   *
   * The "connection" property contains the #GDBusConnection that is used
   * to communicate with Qemu.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         G_TYPE_DBUS_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:devices:
   *
   * The "devices" property contains a #GListModel of devices that have been
   * discovered on the #GDBusConnection to Qemu.
   */
  properties [PROP_DEVICES] =
    g_param_spec_object ("devices", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:name:
   *
   * The "name" property is the named of the VM as specified by the
   * Qemu instance.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:uuid:
   *
   * The "uuid" property is the unique identifier as specified by the
   * Qemu instance for the VM.
   */
  properties [PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_session_init (MksSession *self)
{
  self->devices = g_list_store_new (MKS_TYPE_DEVICE);
  self->devices_read_only = mks_read_only_list_model_new (G_LIST_MODEL (self->devices));
}

static gboolean
mks_session_initable_init (GInitable     *initable,
                           GCancellable  *cancellable,
                           GError       **error)
{
  MksSession *self = (MksSession *)initable;
  g_autoptr(MksQemuVM) vm = NULL;

  g_assert (MKS_IS_SESSION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->connection == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Not connected");
      return FALSE;
    }

  vm = mks_qemu_vm_proxy_new_sync (self->connection,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "org.qemu",
                                   "/org/qemu/Display1/VM",
                                   cancellable,
                                   error);

  if (vm != NULL)
    mks_session_set_vm (self, vm);

  return vm != NULL;
}

static void
mks_session_async_initable_vm_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(MksQemuVM) vm = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  MksSession *self;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  vm = mks_qemu_vm_proxy_new_finish (result, &error);

  g_assert (MKS_IS_SESSION (self));
  g_assert (!vm || MKS_QEMU_IS_VM (vm));

  if (vm != NULL)
    mks_session_set_vm (self, vm);

  if (error)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
mks_session_async_initable_init_async (GAsyncInitable      *async_initable,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  MksSession *self = (MksSession *)async_initable;
  g_autoptr(GTask) task = NULL;

  g_assert (MKS_IS_SESSION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_session_async_initable_init_async);
  g_task_set_priority (task, io_priority);

  if (self->connection == NULL)
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_CONNECTED,
                             "Not connected");
  else
    mks_qemu_vm_proxy_new (self->connection,
                           G_DBUS_PROXY_FLAGS_NONE,
                           "org.qemu",
                           "/org/qemu/Display1/VM",
                           cancellable,
                           mks_session_async_initable_vm_cb,
                           g_steal_pointer (&task));
}

static gboolean
mks_session_async_initable_init_finish (GAsyncInitable  *async_initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  MksSession *self = (MksSession *)async_initable;

  g_assert (MKS_IS_SESSION (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * mks_session_get_devices:
 * @self: a #MksSession
 *
 * Gets a #GListModel of devices connected to the session.
 *
 * Returns: (transfer none): a #GListModel of #MksDevice
 */
GListModel *
mks_session_get_devices (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->devices_read_only;
}

static void
mks_session_new_for_connection_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  MksSession *self = (MksSession *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (MKS_IS_SESSION (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (self), result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (self), g_object_unref);
}

/**
 * mks_session_new_for_connection:
 * @connection: a #GDBusConnection
 * @io_priority: priority for IO operations
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion of the operation
 * @user_data: closure data for @callback
 *
 * Creates a #MksSession which communicates using @connection.
 *
 * The #GDBusConnection should be a private D-Bus connection to a Qemu
 * instance which has devices created using the "dbus" backend.

 * @callback will be executed when the session has been created or
 * failed to create.
 *
 * This function will not block the calling thread.
 *
 * use mks_session_new_for_connection_finish() to get the result of
 * this operation.
 */
void
mks_session_new_for_connection (GDBusConnection     *connection,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(MksSession) self = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self = g_object_new (MKS_TYPE_SESSION,
                       "connection", connection,
                       NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_session_new_for_connection);
  g_task_set_priority (task, io_priority);

  g_async_initable_init_async (G_ASYNC_INITABLE (self),
                               io_priority,
                               cancellable,
                               mks_session_new_for_connection_cb,
                               g_steal_pointer (&task));
}

/**
 * mks_session_new_for_connection_finish:
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a location for a #GError or %NULL
 *
 * Completes a request to create a #MksSession for a #GDBusConnection.
 *
 * Returns: (transfer full): a #MksSession if successful; otherwise %NULL
 *   and @error is set.
 */
MksSession *
mks_session_new_for_connection_finish (GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * mks_session_new_for_connection_sync:
 * @connection: a private #GDBusConnetion to a Qemu instance
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: (nullable): a location for a #GError, or %NULL
 *
 * Synchronously creates a new #MksSession instance.
 *
 * This may block while the Qemu instance is contacted to query for
 * initial devices and VM status.
 *
 * Returns: (transfer full): a #MksSession if successful; otherwise %NULL
 *   and @error is set.
 */
MksSession *
mks_session_new_for_connection_sync (GDBusConnection  *connection,
                                     GCancellable     *cancellable,
                                     GError          **error)
{
  g_autoptr(MksSession) self = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  self = g_object_new (MKS_TYPE_SESSION,
                       "connection", connection,
                       NULL);

  if (g_initable_init (G_INITABLE (self), cancellable, error))
    return g_steal_pointer (&self);

  return NULL;
}

/**
 * mks_session_get_connection:
 * @self: a #MksSession
 *
 * Gets the #MksSession:connection property.
 *
 * Returns: (transfer none) (nullable): a #GDBusConnection or %NULL if
 *   the connection has not been set, or was disposed.
 */
GDBusConnection *
mks_session_get_connection (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->connection;
}

/**
 * mks_session_get_uuid:
 * @self: a #MksSession
 *
 * Gets the "uuid" property.
 */
const char *
mks_session_get_uuid (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  if (self->vm != NULL)
    return mks_qemu_vm_get_uuid (self->vm);

  return NULL;
}

/**
 * mks_session_get_name:
 * @self: a #MksSession
 *
 * Gets the "name" property.
 */
const char *
mks_session_get_name (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  if (self->vm != NULL)
    return mks_qemu_vm_get_name (self->vm);

  return NULL;
}
