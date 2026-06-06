/* mks-screen.c
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
#include "mks-keyboard.h"
#include "mks-mouse.h"
#include "mks-paintable-private.h"
#include "mks-screen-attributes-private.h"
#include "mks-screen.h"
#include "mks-util-private.h"
#include "mks-touchable.h"

struct _MksScreenClass
{
  MksDeviceClass parent_class;
};

struct _MksScreen
{
  MksDevice       parent_instance;

  MksQemuConsole *console;
  gulong          console_notify_handler;

  MksKeyboard    *keyboard;
  MksMouse       *mouse;
  MksTouchable   *touchable;

  guint           number;
  guint           width;
  guint           height;

  MksScreenKind   kind : 2;
};

G_DEFINE_FINAL_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE_ADDRESS,
  PROP_HEIGHT,
  PROP_KIND,
  PROP_KEYBOARD,
  PROP_MOUSE,
  PROP_NUMBER,
  PROP_WIDTH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_screen_set_width (MksScreen *self,
                      guint      width)
{
  g_assert (MKS_IS_SCREEN (self));

  if (self->width != width)
    {
      self->width = width;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WIDTH]);
    }
}

static void
mks_screen_set_height (MksScreen *self,
                       guint      height)
{
  g_assert (MKS_IS_SCREEN (self));

  if (self->height != height)
    {
      self->height = height;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HEIGHT]);
    }
}

static void
mks_screen_set_number (MksScreen *self,
                       guint      number)
{
  g_assert (MKS_IS_SCREEN (self));

  if (self->number != number)
    {
      self->number = number;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NUMBER]);
    }
}

static void
mks_screen_set_type (MksScreen  *self,
                     const char *type)
{
  MksScreenKind kind;

  g_assert (MKS_IS_SCREEN (self));

  kind = MKS_SCREEN_KIND_TEXT;

  if (strcmp (type, "Graphic") == 0)
    kind = MKS_SCREEN_KIND_GRAPHIC;

  if (kind != self->kind)
    {
      self->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

static void
mks_screen_console_notify_cb (MksScreen      *self,
                              GParamSpec     *pspec,
                              MksQemuConsole *console)
{
  g_assert (MKS_IS_SCREEN (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_CONSOLE (console));

  if (strcmp (pspec->name, "label") == 0)
    _mks_device_set_name (MKS_DEVICE (self), mks_qemu_console_get_label (console));
  else if (strcmp (pspec->name, "width") == 0)
    mks_screen_set_width (self, mks_qemu_console_get_width (console));
  else if (strcmp (pspec->name, "height") == 0)
    mks_screen_set_height (self, mks_qemu_console_get_height (console));
  else if (strcmp (pspec->name, "number") == 0)
    mks_screen_set_number (self, mks_qemu_console_get_head (console));
  else if (strcmp (pspec->name, "type") == 0)
    mks_screen_set_type (self, mks_qemu_console_get_type_ ((console)));
}

static void
mks_screen_set_console (MksScreen      *self,
                        MksQemuConsole *console)
{
  g_assert (MKS_IS_SCREEN (self));
  g_assert (!console || MKS_QEMU_IS_CONSOLE (console));

  if (self->console != NULL)
    return;

  if (g_set_object (&self->console, console))
    {
      _mks_device_set_name (MKS_DEVICE (self), mks_qemu_console_get_label (console));

      self->console_notify_handler =
        g_signal_connect_object (console,
                                 "notify",
                                 G_CALLBACK (mks_screen_console_notify_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

      mks_screen_set_type (self, mks_qemu_console_get_type_ ((console)));
      mks_screen_set_width (self, mks_qemu_console_get_width (console));
      mks_screen_set_height (self, mks_qemu_console_get_height (console));
      mks_screen_set_number (self, mks_qemu_console_get_head (console));
    }
}

static gboolean
mks_screen_setup (MksDevice     *device,
                  MksQemuObject *object)
{
  MksScreen *self = (MksScreen *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_SCREEN (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_CONSOLE (iface))
        mks_screen_set_console (self, MKS_QEMU_CONSOLE (iface));
      else if (MKS_QEMU_IS_KEYBOARD (iface))
        self->keyboard = _mks_device_new (MKS_TYPE_KEYBOARD, device->session, object);
      else if (MKS_QEMU_IS_MOUSE (iface))
        self->mouse = _mks_device_new (MKS_TYPE_MOUSE, device->session, object);
      else if (MKS_QEMU_IS_MULTI_TOUCH (iface))
        self->touchable = _mks_device_new (MKS_TYPE_TOUCHABLE, device->session, object);
    }

  return self->console != NULL &&
         self->keyboard != NULL &&
         self->mouse != NULL;
}

static void
mks_screen_dispose (GObject *object)
{
  MksScreen *self = (MksScreen *)object;

  if (self->console != NULL)
    {
      g_clear_signal_handler (&self->console_notify_handler, self->console);
      g_clear_object (&self->console);
    }

  g_clear_object (&self->keyboard);
  g_clear_object (&self->mouse);
  g_clear_object (&self->touchable);

  G_OBJECT_CLASS (mks_screen_parent_class)->dispose (object);
}

static void
mks_screen_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MksScreen *self = MKS_SCREEN (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ADDRESS:
      g_value_set_string (value, mks_screen_get_device_address (self));
      break;

    case PROP_KEYBOARD:
      g_value_set_object (value, mks_screen_get_keyboard (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, mks_screen_get_kind (self));
      break;

    case PROP_MOUSE:
      g_value_set_object (value, mks_screen_get_mouse (self));
      break;

    case PROP_NUMBER:
      g_value_set_uint (value, mks_screen_get_number (self));
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, mks_screen_get_width (self));
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, mks_screen_get_height (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_screen_class_init (MksScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_screen_dispose;
  object_class->get_property = mks_screen_get_property;

  device_class->setup = mks_screen_setup;

  properties [PROP_DEVICE_ADDRESS] =
    g_param_spec_string ("device-address", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEYBOARD] =
    g_param_spec_object ("keyboard", NULL, NULL,
                         MKS_TYPE_KEYBOARD,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       MKS_TYPE_SCREEN_KIND,
                       MKS_SCREEN_KIND_TEXT,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MOUSE] =
    g_param_spec_object ("mouse", NULL, NULL,
                         MKS_TYPE_MOUSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NUMBER] =
    g_param_spec_uint ("number", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WIDTH] =
    g_param_spec_uint ("width", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_screen_init (MksScreen *self)
{
}

/**
 * mks_screen_get_keyboard:
 * @self: a #MksScreen
 *
 * Gets the #MksScreen:keyboard property.
 *
 * Returns: (transfer none): a #MksKeyboard
 */
MksKeyboard *
mks_screen_get_keyboard (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), NULL);

  return self->keyboard;
}

/**
 * mks_screen_get_mouse:
 * @self: a #MksScreen
 *
 * Gets the #MksScreen:mouse property.
 *
 * Returns: (transfer none): a #MksMouse
 */
MksMouse *
mks_screen_get_mouse (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), NULL);

  return self->mouse;
}

/**
 * mks_screen_get_touchable:
 * @self: a #MksScreen
 *
 * Gets the #MksScreen:touchable property.
 *
 * Returns: (transfer none): a #MksTouchable
 */
MksTouchable *
mks_screen_get_touchable (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), NULL);

  return self->touchable;
}

/**
 * mks_screen_get_kind:
 * @self: a #MksScreen
 *
 * Gets the "kind" property.
 *
 * Returns: a #MksScreenKind
 */
MksScreenKind
mks_screen_get_kind (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), MKS_SCREEN_KIND_TEXT);

  return self->kind;
}

/**
 * mks_screen_get_width:
 * @self: a #MksScreen
 *
 * Gets the "width" property.
 *
 * Returns: The width of the screen in pixels.
 */
guint
mks_screen_get_width (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), 0);

  return self->width;
}

/**
 * mks_screen_get_height:
 * @self: a #MksScreen
 *
 * Gets the "height" property.
 *
 * Returns: The height of the screen in pixels.
 */
guint
mks_screen_get_height (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), 0);

  return self->height;
}

/**
 * mks_screen_get_number:
 * @self: a #MksScreen
 *
 * Gets the "number" property.
 *
 * Returns: the screen number
 */
guint
mks_screen_get_number (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), 0);

  return self->number;
}

const char *
mks_screen_get_device_address (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), NULL);

  if (self->console != NULL)
    return mks_qemu_console_get_device_address (self->console);

  return NULL;
}

static gboolean
check_console (MksScreen  *self,
               GError    **error)
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

/**
 * mks_screen_configure:
 * @self: an #MksScreen
 * @attributes: (transfer full): a #MksScreenAttributes
 *
 * Requests the QEMU instance reconfigure the screen with @attributes.
 *
 * This function takes ownership of @attributes.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_screen_configure (MksScreen           *self,
                      MksScreenAttributes *attributes)
{
  g_autoptr(GError) error = NULL;
  DexFuture *ret;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_SCREEN (self));
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

void
mks_screen_configure_async (MksScreen           *self,
                            MksScreenAttributes *attributes,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_screen_configure (self, attributes));
}

/**
 * mks_screen_configure_finish:
 * @self: an #MksScreen
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to mks_screen_configure().
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_screen_configure_finish (MksScreen     *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

typedef struct _MksScreenAttach
{
  GdkPaintable *paintable;
} MksScreenAttach;

static void
mks_screen_attach_free (MksScreenAttach *state)
{
  g_clear_object (&state->paintable);
  g_free (state);
}

static DexFuture *
mks_screen_attach_complete (DexFuture *future,
                            gpointer   user_data)
{
  MksScreenAttach *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_object (state->paintable);
}

/**
 * mks_screen_attach:
 * @self: an #MksScreen
 * @display: a #GdkDisplay
 *
 * Creates a #GdkPaintable that is updated with the contents of the screen.
 *
 * This function registers a new `socketpair()` which is shared with
 * the QEMU instance to receive rendering updates. Those updates are
 * propagated to the resulting #GdkPaintable.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   #GdkPaintable.
 */
DexFuture *
mks_screen_attach (MksScreen           *self,
                   GdkDisplay          *display)
{
  MksScreenAttach *state;
  g_autoptr(GUnixFDList) unix_fd_list = NULL;
  g_autoptr(GdkPaintable) paintable = NULL;
  g_autoptr(GError) error = NULL;
  g_autofd int fd = -1;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_SCREEN (self));
  dex_return_error_if_fail (GDK_IS_DISPLAY (display));

  if (!check_console (self, &error) ||
      !(paintable = _mks_paintable_new (display, NULL, &fd, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (MksScreenAttach, 1);
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
                                             mks_screen_attach_complete,
                                             state,
                                             (GDestroyNotify) mks_screen_attach_free),
                            begin_time,
                            "screen.attach");
}

void
mks_screen_attach_async (MksScreen           *self,
                         GdkDisplay          *display,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_screen_attach (self, display));
}

/**
 * mks_screen_attach_finish:
 * @self: an #MksScreen
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to create a [iface@Gdk.Paintable] containing
 * the contents of #MksScreen in the QEMU instance.
 *
 * The resulting [iface@Gdk.Paintable] will be updated as changes are delivered
 * from QEMU over a private `socketpair()`. In the typical case, those
 * changes are propagated using a DMA-BUF and damage notifications.
 *
 * Returns: (transfer full): a #GdkPainable if successful; otherwise %NULL
 *   and @error is set.
 */
GdkPaintable *
mks_screen_attach_finish (MksScreen     *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);
}
