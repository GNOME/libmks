/* mks-dbus-remote.c
 *
 * Copyright 2026 Christian Hergert
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

#include "mks-dbus-remote-private.h"
#include "mks-device-private.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

struct _MksDBusRemote
{
  MksRemote     parent_instance;
  MksQemuRemote *remote;
};

struct _MksDBusRemoteClass
{
  MksRemoteClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksDBusRemote, mks_dbus_remote, MKS_TYPE_REMOTE)

static gboolean
mks_dbus_remote_setup (MksDevice *device,
                       GObject   *object)
{
  MksDBusRemote *self = MKS_DBUS_REMOTE (device);
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_REMOTE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_REMOTE (iface))
        g_set_object (&self->remote, MKS_QEMU_REMOTE (iface));
    }

  return self->remote != NULL;
}

static void
mks_dbus_remote_dispose (GObject *object)
{
  MksDBusRemote *self = MKS_DBUS_REMOTE (object);

  g_clear_object (&self->remote);

  G_OBJECT_CLASS (mks_dbus_remote_parent_class)->dispose (object);
}

static DexFuture *
mks_dbus_remote_press (MksRemote *remote,
                       guint      linux_keycode)
{
  MksDBusRemote *self = MKS_DBUS_REMOTE (remote);
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_REMOTE (self));

  if (self->remote == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_CONNECTED,
                                  "Not connected");

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_remote_call_press_future (self->remote,
                                                               linux_keycode),
                            begin_time,
                            "remote.press");
}

static DexFuture *
mks_dbus_remote_release (MksRemote *remote,
                         guint      linux_keycode)
{
  MksDBusRemote *self = MKS_DBUS_REMOTE (remote);
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_REMOTE (self));

  if (self->remote == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_CONNECTED,
                                  "Not connected");

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_remote_call_release_future (self->remote,
                                                                 linux_keycode),
                            begin_time,
                            "remote.release");
}

static void
mks_dbus_remote_class_init (MksDBusRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);
  MksRemoteClass *remote_class = MKS_REMOTE_CLASS (klass);

  object_class->dispose = mks_dbus_remote_dispose;

  device_class->setup = mks_dbus_remote_setup;

  remote_class->press = mks_dbus_remote_press;
  remote_class->release = mks_dbus_remote_release;
}

static void
mks_dbus_remote_init (MksDBusRemote *self)
{
}
