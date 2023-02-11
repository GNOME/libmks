/*
 * mks-paintable.c
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

#include <errno.h>
#include <sys/socket.h>

#include <glib/gstdio.h>

#include "mks-paintable-private.h"
#include "mks-qemu.h"

struct _MksPaintable
{
  GObject          parent_instance;

  guint            width;
  guint            height;

  MksQemuListener *listener;
  GDBusConnection *connection;
};

static int
mks_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return MKS_PAINTABLE (paintable)->height;
}

static int
mks_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return MKS_PAINTABLE (paintable)->width;
}

static double
mks_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  MksPaintable *self = MKS_PAINTABLE (paintable);

  if (self->width == 0 || self->height == 0)
    return 1.;

  return (double)self->width / (double)self->height;
}

static void
mks_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_height = mks_paintable_get_intrinsic_height;
  iface->get_intrinsic_width = mks_paintable_get_intrinsic_width;
  iface->get_intrinsic_aspect_ratio = mks_paintable_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_paintable_snapshot;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksPaintable, mks_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

static void
mks_paintable_dispose (GObject *object)
{
  MksPaintable *self = (MksPaintable *)object;

  g_clear_object (&self->connection);
  g_clear_object (&self->listener);

  G_OBJECT_CLASS (mks_paintable_parent_class)->dispose (object);
}

static void
mks_paintable_class_init (MksPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_paintable_dispose;
}

static void
mks_paintable_init (MksPaintable *self)
{
}

static gboolean
mks_paintable_listener_update_dmabuf (MksPaintable          *self,
                                      GDBusMethodInvocation *invocation,
                                      int                    x,
                                      int                    y,
                                      int                    width,
                                      int                    height,
                                      MksQemuListener       *listener)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  return FALSE;
}

static gboolean
mks_paintable_listener_scanout_dmabuf (MksPaintable          *self,
                                       GDBusMethodInvocation *invocation,
                                       GUnixFDList           *unix_fd_list,
                                       GVariant              *dmabuf,
                                       guint                  width,
                                       guint                  height,
                                       guint                  stride,
                                       guint                  fourcc,
                                       guint64                modifier,
                                       gboolean               y0_top,
                                       MksQemuListener       *listener)
{
  gboolean size_changed;

  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  size_changed = width != self->width || height != self->height;

  if (size_changed)
    gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  else
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return FALSE;
}

static gboolean
mks_paintable_listener_update (MksPaintable          *self,
                               GDBusMethodInvocation *invocation,
                               int                    x,
                               int                    y,
                               int                    width,
                               int                    height,
                               guint                  stride,
                               guint                  pixman_format,
                               GVariant              *bytes,
                               MksQemuListener       *listener)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  return FALSE;
}

static gboolean
mks_paintable_listener_scanout (MksPaintable          *self,
                                GDBusMethodInvocation *invocation,
                                guint                  width,
                                guint                  height,
                                guint                  stride,
                                guint                  pixman_format,
                                GVariant              *bytes,
                                MksQemuListener       *listener)
{
  gboolean size_changed;

  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  size_changed = width != self->width || height != self->height;

  if (size_changed)
    gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  else
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return FALSE;
}

static gboolean
mks_paintable_listener_cursor_define (MksPaintable          *self,
                                      GDBusMethodInvocation *invocation,
                                      int                    width,
                                      int                    height,
                                      int                    hot_x,
                                      int                    hot_y,
                                      GVariant              *bytes,
                                      MksQemuListener       *listener)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  return FALSE;
}

static gboolean
mks_paintable_listener_mouse_set (MksPaintable          *self,
                                  GDBusMethodInvocation *invocation,
                                  int                    x,
                                  int                    y,
                                  int                    on,
                                  MksQemuListener       *listener)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  return FALSE;
}

static gboolean
mks_paintable_listener_disable (MksPaintable          *self,
                                GDBusMethodInvocation *invocation,
                                MksQemuListener       *listener)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  return FALSE;
}

static gboolean
create_socketpair (int     *us,
                   int     *them,
                   GError **error)
{
  int fds[2];
  int rv;

  rv = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, fds);

  if (rv != 0)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  *us = fds[0];
  *them = fds[1];

  return TRUE;
}

GdkPaintable *
_mks_paintable_new (GCancellable  *cancellable,
                    int           *peer_fd,
                    GError       **error)
{
  g_autoptr(MksPaintable) self = NULL;
  g_autoptr(MksQemuListener) listener = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GSocketConnection) io_stream = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_autofd int us = -1;
  g_autofd int them = -1;

  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (peer_fd != NULL, NULL);

  *peer_fd = -1;

  self = g_object_new (MKS_TYPE_PAINTABLE, NULL);

  /* Create a socketpair() to use for D-Bus P2P protocol. We will be receiving
   * DMA-BUF FDs over this.
   */
  if (!create_socketpair (&us, &them, error))
    return NULL;

  /* Create socket for our side of the socket pair */
  if (!(socket = g_socket_new_from_fd (us, error)))
    return NULL;
  us = -1;

  /* And convert that socket into a GIOStream */
  io_stream = g_socket_connection_factory_create_connection (socket);

  /* Setup our GDBusConnection. We can do this synchronously because we are
   * not connecting to a message bus, therefore nothing to process up front.
   */
  if (!(connection = g_dbus_connection_new_sync (G_IO_STREAM (io_stream),
                                                 NULL,
                                                 G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                                 NULL,
                                                 cancellable,
                                                 error)))
    return NULL;

  /* Setup our listener and callbacks to process requests */
  listener = mks_qemu_listener_skeleton_new ();
  g_signal_connect_object (listener,
                           "handle-scanout",
                           G_CALLBACK (mks_paintable_listener_scanout),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-update",
                           G_CALLBACK (mks_paintable_listener_update),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-scanout-dmabuf",
                           G_CALLBACK (mks_paintable_listener_scanout_dmabuf),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-update-dmabuf",
                           G_CALLBACK (mks_paintable_listener_update_dmabuf),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-disable",
                           G_CALLBACK (mks_paintable_listener_disable),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-cursor-define",
                           G_CALLBACK (mks_paintable_listener_cursor_define),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (listener,
                           "handle-mouse-set",
                           G_CALLBACK (mks_paintable_listener_mouse_set),
                           self,
                           G_CONNECT_SWAPPED);

  /* Export our listener before we return back so we know that when the peer
   * tries to connect, we're guaranteed to already be available.
   */
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (listener),
                                         connection,
                                         "/org/qemu/Display1/Listener",
                                         error))
    return NULL;

  self->connection = g_object_ref (connection);
  self->listener = g_object_ref (listener);

  g_dbus_connection_start_message_processing (connection);

  *peer_fd = g_steal_fd (&them);

  return GDK_PAINTABLE (g_steal_pointer (&self));
}
