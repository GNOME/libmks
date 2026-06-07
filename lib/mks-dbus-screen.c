/* mks-dbus-screen.c
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

#include <errno.h>
#include <sys/socket.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "mks-device-private.h"
#include "mks-enums.h"
#include "mks-qemu.h"
#include "mks-dbus-keyboard-private.h"
#include "mks-dbus-mouse-private.h"
#include "mks-paintable-private.h"
#include "mks-screen-attributes-private.h"
#include "mks-dbus-screen-private.h"
#include "mks-util-private.h"
#include "mks-dbus-touchable-private.h"

struct _MksDBusScreenClass
{
  MksScreenClass parent_class;
};

struct _MksDBusScreen
{
  MksScreen                          parent_instance;
  MksQemuConsole                    *console;
  gulong                             console_notify_handler;
  GDBusConnection                   *activity_connection;
  MksQemuListener                   *activity_listener;
  MksQemuListenerUnixScanoutDMABUF2 *activity_listener_dmabuf2;
  MksQemuListenerUnixMap            *activity_listener_map;
  MksKeyboard                       *keyboard;
  MksMouse                          *mouse;
  MksTouchable                      *touchable;
  guint                              number;
  guint                              width;
  guint                              height;
  MksScreenKind                      kind : 2;
};

G_DEFINE_FINAL_TYPE (MksDBusScreen, mks_dbus_screen, MKS_TYPE_SCREEN)

static MksKeyboard   *mks_dbus_screen_get_keyboard       (MksScreen           *screen);
static MksMouse      *mks_dbus_screen_get_mouse          (MksScreen           *screen);
static MksTouchable  *mks_dbus_screen_get_touchable      (MksScreen           *screen);
static MksScreenKind  mks_dbus_screen_get_kind           (MksScreen           *screen);
static guint          mks_dbus_screen_get_width          (MksScreen           *screen);
static guint          mks_dbus_screen_get_height         (MksScreen           *screen);
static guint          mks_dbus_screen_get_number         (MksScreen           *screen);
static const char    *mks_dbus_screen_get_device_address (MksScreen           *screen);
static DexFuture     *mks_dbus_screen_configure          (MksScreen           *screen,
                                                          MksScreenAttributes *attributes);
static DexFuture     *mks_dbus_screen_attach             (MksScreen           *screen,
                                                          GdkDisplay          *display);


static void
mks_dbus_screen_mark_active (MksDBusScreen *self)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));

  _mks_screen_mark_active (MKS_SCREEN (self));
}

static gboolean
mks_dbus_screen_activity_listener_scanout (MksDBusScreen         *self,
                                           GDBusMethodInvocation *invocation,
                                           guint                  width,
                                           guint                  height,
                                           guint                  stride,
                                           guint                  pixman_format,
                                           GVariant              *data,
                                           MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_complete_scanout (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_update (MksDBusScreen         *self,
                                          GDBusMethodInvocation *invocation,
                                          int                    x,
                                          int                    y,
                                          int                    width,
                                          int                    height,
                                          guint                  stride,
                                          guint                  pixman_format,
                                          GVariant              *data,
                                          MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_complete_update (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_scanout_dmabuf (MksDBusScreen         *self,
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
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_complete_scanout_dmabuf (listener, invocation, NULL);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_update_dmabuf (MksDBusScreen         *self,
                                                 GDBusMethodInvocation *invocation,
                                                 int                    x,
                                                 int                    y,
                                                 int                    width,
                                                 int                    height,
                                                 MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_complete_update_dmabuf (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_scanout_dmabuf2 (MksDBusScreen                     *self,
                                                   GDBusMethodInvocation             *invocation,
                                                   GUnixFDList                       *unix_fd_list,
                                                   GVariant                          *dmabuf,
                                                   guint                              x,
                                                   guint                              y,
                                                   guint                              width,
                                                   guint                              height,
                                                   GVariant                          *offset,
                                                   GVariant                          *stride,
                                                   guint                              num_planes,
                                                   guint                              fourcc,
                                                   guint                              backing_w,
                                                   guint                              backing_h,
                                                   guint64                            modifier,
                                                   gboolean                           y0_top,
                                                   MksQemuListenerUnixScanoutDMABUF2 *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER_UNIX_SCANOUT_DMABUF2 (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_unix_scanout_dmabuf2_complete_scanout_dmabuf2 (listener,
                                                                   invocation,
                                                                   NULL);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_scanout_map (MksDBusScreen          *self,
                                               GDBusMethodInvocation  *invocation,
                                               GUnixFDList            *unix_fd_list,
                                               GVariant               *handle,
                                               guint                   offset,
                                               guint                   width,
                                               guint                   height,
                                               guint                   stride,
                                               guint                   pixman_format,
                                               MksQemuListenerUnixMap *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER_UNIX_MAP (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_unix_map_complete_scanout_map (listener, invocation, NULL);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_update_map (MksDBusScreen          *self,
                                              GDBusMethodInvocation  *invocation,
                                              int                     x,
                                              int                     y,
                                              int                     width,
                                              int                     height,
                                              MksQemuListenerUnixMap *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER_UNIX_MAP (listener));

  mks_dbus_screen_mark_active (self);
  mks_qemu_listener_unix_map_complete_update_map (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_disable (MksDBusScreen         *self,
                                           GDBusMethodInvocation *invocation,
                                           MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_qemu_listener_complete_disable (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_mouse_set (MksDBusScreen         *self,
                                             GDBusMethodInvocation *invocation,
                                             int                    x,
                                             int                    y,
                                             int                    on,
                                             MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_qemu_listener_complete_mouse_set (listener, invocation);

  return TRUE;
}

static gboolean
mks_dbus_screen_activity_listener_cursor_define (MksDBusScreen         *self,
                                                 GDBusMethodInvocation *invocation,
                                                 int                    width,
                                                 int                    height,
                                                 int                    hot_x,
                                                 int                    hot_y,
                                                 GVariant              *data,
                                                 MksQemuListener       *listener)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_LISTENER (listener));

  mks_qemu_listener_complete_cursor_define (listener, invocation);

  return TRUE;
}

static DexFuture *
mks_dbus_screen_activity_connection_cb (DexFuture *future,
                                        gpointer   user_data)
{
  MksDBusScreen *self = user_data;
  GDBusInterfaceSkeleton *skeleton;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_DBUS_SCREEN (self));

  if (!(value = dex_future_get_value (future, &error)))
    {
      g_warning ("Failed to create activity D-Bus connection: %s", error->message);
      return dex_future_new_true ();
    }

  g_set_object (&self->activity_connection, g_value_get_object (value));

  skeleton = G_DBUS_INTERFACE_SKELETON (self->activity_listener);
  if (!g_dbus_interface_skeleton_export (skeleton,
                                         self->activity_connection,
                                         "/org/qemu/Display1/Listener",
                                         &error))
    {
      g_warning ("Failed to export activity listener on D-Bus connection: %s",
                 error->message);
      return dex_future_new_true ();
    }

  skeleton = G_DBUS_INTERFACE_SKELETON (self->activity_listener_dmabuf2);
  if (!g_dbus_interface_skeleton_export (skeleton,
                                         self->activity_connection,
                                         "/org/qemu/Display1/Listener",
                                         &error))
    {
      g_warning ("Failed to export activity DMA-BUF2 listener on D-Bus connection: %s",
                 error->message);
      return dex_future_new_true ();
    }

  skeleton = G_DBUS_INTERFACE_SKELETON (self->activity_listener_map);
  if (!g_dbus_interface_skeleton_export (skeleton,
                                         self->activity_connection,
                                         "/org/qemu/Display1/Listener",
                                         &error))
    {
      g_warning ("Failed to export activity map listener on D-Bus connection: %s",
                 error->message);
      return dex_future_new_true ();
    }

  g_dbus_connection_start_message_processing (self->activity_connection);

  return dex_future_new_true ();
}

static void
mks_dbus_screen_start_activity_listener (MksDBusScreen *self)
{
  g_autoptr(GSocketConnection) io_stream = NULL;
  g_autoptr(GUnixFDList) unix_fd_list = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_autoptr(GError) error = NULL;
  g_autofd int us = -1;
  g_autofd int them = -1;
  gint64 begin_time;

  g_assert (MKS_IS_DBUS_SCREEN (self));

  if (self->console == NULL || self->activity_listener != NULL)
    return;

  if (!mks_socketpair_create (&us, &them, &error) ||
      !(socket = g_socket_new_from_fd (us, &error)))
    {
      g_warning ("Failed to create activity listener socket: %s", error->message);
      return;
    }

  us = -1;
  io_stream = g_socket_connection_factory_create_connection (socket);

  self->activity_listener = mks_qemu_listener_skeleton_new ();
  self->activity_listener_dmabuf2 = mks_qemu_listener_unix_scanout_dmabuf2_skeleton_new ();
  self->activity_listener_map = mks_qemu_listener_unix_map_skeleton_new ();
  mks_qemu_listener_set_interfaces (self->activity_listener,
                                    (const char * const[]) {
                                      "org.qemu.Display1.Listener.Unix.Map",
                                      "org.qemu.Display1.Listener.Unix.ScanoutDMABUF2",
                                      NULL
                                    });
  g_signal_connect_object (self->activity_listener,
                           "handle-scanout",
                           G_CALLBACK (mks_dbus_screen_activity_listener_scanout),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-update",
                           G_CALLBACK (mks_dbus_screen_activity_listener_update),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-scanout-dmabuf",
                           G_CALLBACK (mks_dbus_screen_activity_listener_scanout_dmabuf),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-update-dmabuf",
                           G_CALLBACK (mks_dbus_screen_activity_listener_update_dmabuf),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener_dmabuf2,
                           "handle-scanout-dmabuf2",
                           G_CALLBACK (mks_dbus_screen_activity_listener_scanout_dmabuf2),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener_map,
                           "handle-scanout-map",
                           G_CALLBACK (mks_dbus_screen_activity_listener_scanout_map),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener_map,
                           "handle-update-map",
                           G_CALLBACK (mks_dbus_screen_activity_listener_update_map),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-disable",
                           G_CALLBACK (mks_dbus_screen_activity_listener_disable),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-mouse-set",
                           G_CALLBACK (mks_dbus_screen_activity_listener_mouse_set),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->activity_listener,
                           "handle-cursor-define",
                           G_CALLBACK (mks_dbus_screen_activity_listener_cursor_define),
                           self,
                           G_CONNECT_SWAPPED);

  begin_time = MKS_TRACE_BEGIN_MARK ();
  dex_future_disown
    (dex_future_finally
       (mks_marked_future
          (mks_dbus_connection_new (G_IO_STREAM (io_stream),
                                    (G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING |
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                    NULL),
           begin_time,
           "screen.activity.dbus-connection"),
        mks_dbus_screen_activity_connection_cb,
        g_object_ref (self),
        g_object_unref));

  unix_fd_list = g_unix_fd_list_new_from_array (&them, 1), them = -1;
  begin_time = MKS_TRACE_BEGIN_MARK ();
  dex_future_disown
    (mks_logged_future
       (mks_marked_future
          (dex_dbus_connection_call_with_unix_fd_list
             (g_dbus_proxy_get_connection (G_DBUS_PROXY (self->console)),
              g_dbus_proxy_get_name (G_DBUS_PROXY (self->console)),
              g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->console)),
              "org.qemu.Display1.Console",
              "RegisterListener",
              g_variant_new ("(h)", 0),
              G_VARIANT_TYPE ("()"),
              G_DBUS_CALL_FLAGS_NONE,
              -1,
              unix_fd_list),
           begin_time,
           "screen.activity.register"),
        G_LOG_DOMAIN,
        G_LOG_LEVEL_WARNING,
        "Failed to register screen activity listener"));
}

static void
mks_dbus_screen_set_width (MksDBusScreen *self,
                           guint          width)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));

  if (self->width != width)
    {
      self->width = width;
      g_object_notify (G_OBJECT (self), "width");
    }
}

static void
mks_dbus_screen_set_height (MksDBusScreen *self,
                            guint          height)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));

  if (self->height != height)
    {
      self->height = height;
      g_object_notify (G_OBJECT (self), "height");
    }
}

static void
mks_dbus_screen_set_number (MksDBusScreen *self,
                            guint          number)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));

  if (self->number != number)
    {
      self->number = number;
      g_object_notify (G_OBJECT (self), "number");
    }
}

static void
mks_dbus_screen_set_type (MksDBusScreen *self,
                          const char    *type)
{
  MksScreenKind kind;

  g_assert (MKS_IS_DBUS_SCREEN (self));

  kind = MKS_SCREEN_KIND_TEXT;

  if (strcmp (type, "Graphic") == 0)
    kind = MKS_SCREEN_KIND_GRAPHIC;

  if (kind != self->kind)
    {
      self->kind = kind;
      g_object_notify (G_OBJECT (self), "kind");
    }
}

static void
mks_dbus_screen_console_notify_cb (MksDBusScreen  *self,
                                   GParamSpec     *pspec,
                                   MksQemuConsole *console)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_CONSOLE (console));

  if (strcmp (pspec->name, "label") == 0)
    _mks_device_set_name (MKS_DEVICE (self), mks_qemu_console_get_label (console));
  else if (strcmp (pspec->name, "device-address") == 0)
    g_object_notify (G_OBJECT (self), "device-address");
  else if (strcmp (pspec->name, "width") == 0)
    mks_dbus_screen_set_width (self, mks_qemu_console_get_width (console));
  else if (strcmp (pspec->name, "height") == 0)
    mks_dbus_screen_set_height (self, mks_qemu_console_get_height (console));
  else if (strcmp (pspec->name, "number") == 0)
    mks_dbus_screen_set_number (self, mks_qemu_console_get_head (console));
  else if (strcmp (pspec->name, "type") == 0)
    mks_dbus_screen_set_type (self, mks_qemu_console_get_type_ ((console)));
}

static void
mks_dbus_screen_set_console (MksDBusScreen  *self,
                             MksQemuConsole *console)
{
  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (!console || MKS_QEMU_IS_CONSOLE (console));

  if (self->console != NULL)
    return;

  if (g_set_object (&self->console, console))
    {
      _mks_device_set_name (MKS_DEVICE (self), mks_qemu_console_get_label (console));
      g_object_notify (G_OBJECT (self), "device-address");

      self->console_notify_handler =
        g_signal_connect_object (console,
                                 "notify",
                                 G_CALLBACK (mks_dbus_screen_console_notify_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

      mks_dbus_screen_set_type (self, mks_qemu_console_get_type_ ((console)));
      mks_dbus_screen_set_width (self, mks_qemu_console_get_width (console));
      mks_dbus_screen_set_height (self, mks_qemu_console_get_height (console));
      mks_dbus_screen_set_number (self, mks_qemu_console_get_head (console));
      mks_dbus_screen_start_activity_listener (self);
    }
}

static gboolean
mks_dbus_screen_setup (MksDevice *device,
                       GObject   *object)
{
  MksDBusScreen *self = (MksDBusScreen *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_SCREEN (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_CONSOLE (iface))
        mks_dbus_screen_set_console (self, MKS_QEMU_CONSOLE (iface));
      else if (MKS_QEMU_IS_KEYBOARD (iface))
        {
          g_autoptr(MksKeyboard) keyboard = NULL;

          keyboard = _mks_device_new (MKS_TYPE_DBUS_KEYBOARD, device->transport, object);
          if (g_set_object (&self->keyboard, keyboard))
            g_object_notify (G_OBJECT (self), "keyboard");
        }
      else if (MKS_QEMU_IS_MOUSE (iface))
        {
          g_autoptr(MksMouse) mouse = NULL;

          mouse = _mks_device_new (MKS_TYPE_DBUS_MOUSE, device->transport, object);
          if (g_set_object (&self->mouse, mouse))
            g_object_notify (G_OBJECT (self), "mouse");
        }
      else if (MKS_QEMU_IS_MULTI_TOUCH (iface))
        {
          g_autoptr(MksTouchable) touchable = NULL;

          touchable = _mks_device_new (MKS_TYPE_DBUS_TOUCHABLE, device->transport, object);
          if (g_set_object (&self->touchable, touchable))
            g_object_notify (G_OBJECT (self), "touchable");
        }
    }

  return self->console != NULL &&
         self->keyboard != NULL &&
         self->mouse != NULL;
}

static void
mks_dbus_screen_dispose (GObject *object)
{
  MksDBusScreen *self = (MksDBusScreen *)object;

  if (self->console != NULL)
    {
      g_clear_signal_handler (&self->console_notify_handler, self->console);
      g_clear_object (&self->console);
    }

  g_clear_object (&self->keyboard);
  g_clear_object (&self->mouse);
  g_clear_object (&self->touchable);
  g_clear_object (&self->activity_listener);
  g_clear_object (&self->activity_listener_dmabuf2);
  g_clear_object (&self->activity_listener_map);
  g_clear_object (&self->activity_connection);

  G_OBJECT_CLASS (mks_dbus_screen_parent_class)->dispose (object);
}

static void
mks_dbus_screen_class_init (MksDBusScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);
  MksScreenClass *screen_class = MKS_SCREEN_CLASS (klass);

  screen_class->get_kind = mks_dbus_screen_get_kind;
  screen_class->get_keyboard = mks_dbus_screen_get_keyboard;
  screen_class->get_mouse = mks_dbus_screen_get_mouse;
  screen_class->get_touchable = mks_dbus_screen_get_touchable;
  screen_class->get_width = mks_dbus_screen_get_width;
  screen_class->get_height = mks_dbus_screen_get_height;
  screen_class->get_number = mks_dbus_screen_get_number;
  screen_class->get_device_address = mks_dbus_screen_get_device_address;
  screen_class->configure = mks_dbus_screen_configure;
  screen_class->attach = mks_dbus_screen_attach;

  object_class->dispose = mks_dbus_screen_dispose;

  device_class->setup = mks_dbus_screen_setup;
}

static void
mks_dbus_screen_init (MksDBusScreen *self)
{
}

static MksKeyboard *
mks_dbus_screen_get_keyboard (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), NULL);

  return self->keyboard;
}

static MksMouse *
mks_dbus_screen_get_mouse (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), NULL);

  return self->mouse;
}

static MksTouchable *
mks_dbus_screen_get_touchable (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), NULL);

  return self->touchable;
}

static MksScreenKind
mks_dbus_screen_get_kind (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), MKS_SCREEN_KIND_TEXT);

  return self->kind;
}

static guint
mks_dbus_screen_get_width (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), 0);

  return self->width;
}

static guint
mks_dbus_screen_get_height (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), 0);

  return self->height;
}

static guint
mks_dbus_screen_get_number (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), 0);

  return self->number;
}

static const char *
mks_dbus_screen_get_device_address (MksScreen *screen)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);

  g_return_val_if_fail (MKS_IS_DBUS_SCREEN (self), NULL);

  if (self->console != NULL)
    return mks_qemu_console_get_device_address (self->console);

  return NULL;
}

static gboolean
check_console (MksDBusScreen  *self,
               GError        **error)
{
  if (self->console == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_CONNECTED,
                           "Not connected");
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
mks_dbus_screen_configure (MksScreen           *screen,
                           MksScreenAttributes *attributes)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);
  g_autoptr(GError) error = NULL;
  DexFuture *ret;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_SCREEN (self));
  dex_return_error_if_fail (attributes != NULL);

  if (!check_console (self, &error))
    ret = dex_future_new_for_error (g_steal_pointer (&error));
  else
    {
      begin_time = MKS_TRACE_BEGIN_MARK ();
      ret = mks_marked_future (mks_qemu_console_call_set_uiinfo_future (self->console,
                                                                        attributes->width_mm,
                                                                        attributes->height_mm,
                                                                        attributes->x_offset,
                                                                        attributes->y_offset,
                                                                        attributes->width,
                                                                        attributes->height),
                               begin_time,
                               "screen.configure");
    }

  mks_screen_attributes_free (attributes);

  return ret;
}

typedef struct _MksDBusScreenAttach
{
  GdkPaintable *paintable;
} MksDBusScreenAttach;

static void
mks_dbus_screen_attach_free (MksDBusScreenAttach *state)
{
  g_clear_object (&state->paintable);
  g_free (state);
}

static DexFuture *
mks_dbus_screen_attach_complete (DexFuture *future,
                                 gpointer   user_data)
{
  MksDBusScreenAttach *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_object (state->paintable);
}

static DexFuture *
mks_dbus_screen_attach (MksScreen  *screen,
                        GdkDisplay *display)
{
  MksDBusScreen *self = MKS_DBUS_SCREEN (screen);
  MksDBusScreenAttach *state;
  g_autoptr(GUnixFDList) unix_fd_list = NULL;
  g_autoptr(GdkPaintable) paintable = NULL;
  g_autoptr(GError) error = NULL;
  g_autofd int fd = -1;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_SCREEN (self));
  dex_return_error_if_fail (GDK_IS_DISPLAY (display));

  if (!check_console (self, &error) ||
      !(paintable = _mks_paintable_new (display, NULL, &fd, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (MksDBusScreenAttach, 1);
  state->paintable = g_object_ref (paintable);

  unix_fd_list = g_unix_fd_list_new_from_array (&fd, 1), fd = -1;
  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (dex_future_then (dex_dbus_connection_call_with_unix_fd_list (g_dbus_proxy_get_connection (G_DBUS_PROXY (self->console)),
                                                                                         g_dbus_proxy_get_name (G_DBUS_PROXY (self->console)),
                                                                                         g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->console)),
                                                                                         "org.qemu.Display1.Console",
                                                                                         "RegisterListener",
                                                                                         g_variant_new ("(h)", 0),
                                                                                         G_VARIANT_TYPE ("()"),
                                                                                         G_DBUS_CALL_FLAGS_NONE,
                                                                                         -1,
                                                                                         unix_fd_list),
                                             mks_dbus_screen_attach_complete,
                                             state,
                                             (GDestroyNotify) mks_dbus_screen_attach_free),
                            begin_time,
                            "screen.attach");
}
