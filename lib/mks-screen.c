/*
 * mks-screen.c
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

#include "mks-device-private.h"
#include "mks-enums.h"
#include "mks-qemu.h"
#include "mks-keyboard-private.h"
#include "mks-mouse-private.h"
#include "mks-screen-private.h"

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

  guint           number;
  guint           width;
  guint           height;

  MksScreenKind   kind : 2;
};

G_DEFINE_FINAL_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

enum {
  PROP_0,
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
_mks_screen_set_width (MksScreen *self,
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
_mks_screen_set_height (MksScreen *self,
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
_mks_screen_set_number (MksScreen *self,
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
    _mks_screen_set_width (self, mks_qemu_console_get_width (console));
  else if (strcmp (pspec->name, "height") == 0)
    _mks_screen_set_height (self, mks_qemu_console_get_height (console));
  else if (strcmp (pspec->name, "number") == 0)
    _mks_screen_set_number (self, mks_qemu_console_get_head (console));
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
      const char *type;

      _mks_device_set_name (MKS_DEVICE (self), mks_qemu_console_get_label (console));

      self->console_notify_handler =
        g_signal_connect_object (console,
                                 "notify",
                                 G_CALLBACK (mks_screen_console_notify_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

      if ((type = mks_qemu_console_get_type_ ((console))))
        {
          if (strcmp (type, "Graphic") == 0)
            self->kind = MKS_SCREEN_KIND_GRAPHIC;
        }

      self->width = mks_qemu_console_get_width (console);
      self->height = mks_qemu_console_get_height (console);
      self->number = mks_qemu_console_get_head (console);
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
        self->keyboard = _mks_device_new (MKS_TYPE_KEYBOARD, object);
      else if (MKS_QEMU_IS_MOUSE (iface))
        self->mouse = _mks_device_new (MKS_TYPE_MOUSE, object);
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
