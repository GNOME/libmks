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

struct _MksScreen
{
  MksDevice       parent_instance;

  MksQemuConsole *console;
  gulong          console_notify_handler;

  MksKeyboard    *keyboard;
  MksMouse       *mouse;

  MksScreenKind   kind : 2;
};

G_DEFINE_FINAL_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_KIND,
  PROP_KEYBOARD,
  PROP_MOUSE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

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
}

static void
mks_screen_set_console (MksScreen      *self,
                        MksQemuConsole *console)
{
  g_assert (MKS_IS_SCREEN (self));
  g_assert (!console || MKS_QEMU_IS_CONSOLE (console));
  g_assert (self->console == NULL);

  if (g_set_object (&self->console, console))
    {
      const char *type;

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
    }
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_screen_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  MksScreen *self = MKS_SCREEN (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_screen_class_init (MksScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_screen_dispose;
  object_class->get_property = mks_screen_get_property;
  object_class->set_property = mks_screen_set_property;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_screen_init (MksScreen *self)
{
}

static void
mks_screen_new_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(MksQemuConsole) console = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  MksScreen *self;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  console = mks_qemu_console_proxy_new_finish (result, &error);

  g_assert (MKS_IS_SCREEN (self));
  g_assert (!console || MKS_QEMU_IS_CONSOLE (console));

  mks_screen_set_console (self, console);

  if (error)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (self), g_object_unref);
}

void
_mks_screen_new (GDBusConnection      *connection,
                 const char           *object_path,
                 GCancellable         *cancellable,
                 GAsyncReadyCallback   callback,
                 gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(MksScreen) self = NULL;

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (object_path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self = g_object_new (MKS_TYPE_SCREEN, NULL);
  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, _mks_screen_new);

  mks_qemu_console_proxy_new (connection,
                              G_DBUS_PROXY_FLAGS_NONE,
                              "org.qemu",
                              object_path,
                              cancellable,
                              mks_screen_new_cb,
                              g_steal_pointer (&task));
}

MksScreen *
_mks_screen_new_finish (GAsyncResult  *result,
                        GError       **error)
{
  MksScreen *ret;

  g_return_val_if_fail (G_IS_TASK (result), NULL);
  ret = g_task_propagate_pointer (G_TASK (result), error);
  g_return_val_if_fail (!ret || MKS_IS_SCREEN (ret), NULL);

  return ret;
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

MksScreenKind
mks_screen_get_kind (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), MKS_SCREEN_KIND_TEXT);

  return self->kind;
}
