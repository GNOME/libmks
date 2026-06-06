/* mks-chardev.c
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

#include <unistd.h>

#include <gio/gunixfdlist.h>

#include "mks-chardev.h"
#include "mks-device-private.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

struct _MksChardevClass
{
  MksDeviceClass parent_class;
};

struct _MksChardev
{
  MksDevice parent_instance;

  MksQemuChardev *chardev;
  gulong          chardev_notify_handler;
};

G_DEFINE_FINAL_TYPE (MksChardev, mks_chardev, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_NAME,
  PROP_FE_OPENED,
  PROP_ECHO,
  PROP_OWNER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_chardev_notify_cb (MksChardev    *self,
                       GParamSpec    *pspec,
                       MksQemuChardev *chardev)
{
  g_assert (MKS_IS_CHARDEV (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_CHARDEV (chardev));

  if (strcmp (pspec->name, "name") == 0)
    {
      _mks_device_set_name (MKS_DEVICE (self), mks_qemu_chardev_get_name (chardev));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
  else if (strcmp (pspec->name, "feopened") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FE_OPENED]);
  else if (strcmp (pspec->name, "echo") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ECHO]);
  else if (strcmp (pspec->name, "owner") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OWNER]);
}

static void
mks_chardev_set_chardev (MksChardev    *self,
                         MksQemuChardev *chardev)
{
  g_assert (MKS_IS_CHARDEV (self));
  g_assert (!chardev || MKS_QEMU_IS_CHARDEV (chardev));

  if (self->chardev != NULL)
    return;

  if (g_set_object (&self->chardev, chardev))
    {
      _mks_device_set_name (MKS_DEVICE (self), mks_qemu_chardev_get_name (chardev));

      self->chardev_notify_handler =
        g_signal_connect_object (chardev,
                                 "notify",
                                 G_CALLBACK (mks_chardev_notify_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
    }
}

static gboolean
mks_chardev_setup (MksDevice     *device,
                   MksQemuObject *object)
{
  MksChardev *self = (MksChardev *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_CHARDEV (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_CHARDEV (iface))
        mks_chardev_set_chardev (self, MKS_QEMU_CHARDEV (iface));
    }

  return self->chardev != NULL;
}

static void
mks_chardev_dispose (GObject *object)
{
  MksChardev *self = (MksChardev *)object;

  if (self->chardev != NULL)
    {
      g_clear_signal_handler (&self->chardev_notify_handler, self->chardev);
      g_clear_object (&self->chardev);
    }

  G_OBJECT_CLASS (mks_chardev_parent_class)->dispose (object);
}

static void
mks_chardev_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksChardev *self = MKS_CHARDEV (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, mks_chardev_get_name (self));
      break;

    case PROP_FE_OPENED:
      g_value_set_boolean (value, mks_chardev_get_fe_opened (self));
      break;

    case PROP_ECHO:
      g_value_set_boolean (value, mks_chardev_get_echo (self));
      break;

    case PROP_OWNER:
      g_value_take_string (value, mks_chardev_dup_owner (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_chardev_class_init (MksChardevClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_chardev_dispose;
  object_class->get_property = mks_chardev_get_property;

  device_class->setup = mks_chardev_setup;

  /**
   * MksChardev:name:
   *
   * The chardev name.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksChardev:fe-opened:
   *
   * If the frontend side of the chardev is open.
   */
  properties [PROP_FE_OPENED] =
    g_param_spec_boolean ("fe-opened", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksChardev:echo:
   *
   * If echo is enabled for the chardev.
   */
  properties [PROP_ECHO] =
    g_param_spec_boolean ("echo", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksChardev:owner:
   *
   * The chardev owner.
   */
  properties [PROP_OWNER] =
    g_param_spec_string ("owner", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_chardev_init (MksChardev *self)
{
}

static gboolean
check_chardev (MksChardev  *self,
               GError     **error)
{
  g_assert (MKS_IS_CHARDEV (self));

  if (self->chardev == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Chardev is not connected");
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
mks_chardev_complete_boolean (DexFuture *future,
                              gpointer   user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

/**
 * mks_chardev_get_name:
 * @self: a `MksChardev`
 *
 * Gets the QEMU chardev name.
 *
 * Returns: (nullable): the chardev name
 */
const char *
mks_chardev_get_name (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), NULL);

  if (self->chardev == NULL)
    return NULL;

  return mks_qemu_chardev_get_name (self->chardev);
}

/**
 * mks_chardev_get_fe_opened:
 * @self: a `MksChardev`
 *
 * Gets if the frontend side of the chardev is open.
 */
gboolean
mks_chardev_get_fe_opened (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);

  return self->chardev != NULL && mks_qemu_chardev_get_feopened (self->chardev);
}

/**
 * mks_chardev_get_echo:
 * @self: a `MksChardev`
 *
 * Gets if echo is enabled for the chardev.
 */
gboolean
mks_chardev_get_echo (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);

  return self->chardev != NULL && mks_qemu_chardev_get_echo (self->chardev);
}

/**
 * mks_chardev_dup_owner:
 * @self: a `MksChardev`
 *
 * Gets a copy of the chardev owner.
 *
 * Returns: (transfer full) (nullable): the chardev owner
 */
char *
mks_chardev_dup_owner (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), NULL);

  if (self->chardev == NULL)
    return NULL;

  return mks_qemu_chardev_dup_owner (self->chardev);
}

/**
 * mks_chardev_register_fd:
 * @self: a `MksChardev`
 * @fd: a Unix file descriptor
 *
 * Registers @fd with QEMU for chardev stream handling.
 *
 * This function takes ownership of @fd immediately.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_chardev_register_fd (MksChardev *self,
                         int         fd)
{
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_CHARDEV (self));
  dex_return_error_if_fail (fd > -1);

  if (!check_chardev (self, &error))
    {
      close (fd);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  fd_list = g_unix_fd_list_new_from_array (&fd, 1), fd = -1;
  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (dex_future_then (dex_dbus_connection_call_with_unix_fd_list (g_dbus_proxy_get_connection (G_DBUS_PROXY (self->chardev)),
                                                                                         g_dbus_proxy_get_name (G_DBUS_PROXY (self->chardev)),
                                                                                         g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->chardev)),
                                                                                         "org.qemu.Display1.Chardev",
                                                                                         "Register",
                                                                                         g_variant_new ("(h)", 0),
                                                                                         G_VARIANT_TYPE ("()"),
                                                                                         G_DBUS_CALL_FLAGS_NONE,
                                                                                         -1,
                                                                                         fd_list),
                                             mks_chardev_complete_boolean,
                                             NULL,
                                             NULL),
                            begin_time,
                            "chardev.register-fd");
}

void
mks_chardev_register_fd_async (MksChardev          *self,
                               int                  fd,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_chardev_register_fd (self, fd));
}

/**
 * mks_chardev_register_fd_finish:
 * @self: a `MksChardev`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`
 *
 * Completes a request to register a chardev fd.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 */
gboolean
mks_chardev_register_fd_finish (MksChardev   *self,
                                GAsyncResult *result,
                                GError      **error)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_chardev_send_break:
 * @self: a `MksChardev`
 *
 * Sends a break event to the chardev.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_chardev_send_break (MksChardev *self)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_CHARDEV (self));

  if (!check_chardev (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_chardev_call_send_break_future (self->chardev),
                            begin_time,
                            "chardev.send-break");
}

void
mks_chardev_send_break_async (MksChardev          *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_chardev_send_break (self));
}

/**
 * mks_chardev_send_break_finish:
 * @self: a `MksChardev`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`
 *
 * Completes a request to send a break event.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 */
gboolean
mks_chardev_send_break_finish (MksChardev   *self,
                               GAsyncResult *result,
                               GError      **error)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}
