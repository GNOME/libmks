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

#include "mks-qemu.h"
#include "mks-screen-private.h"

struct _MksScreen
{
  MksDevice       parent_instance;
  MksQemuConsole *console;
};

G_DEFINE_FINAL_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_screen_dispose (GObject *object)
{
  MksScreen *self = (MksScreen *)object;

  g_clear_object (&self->console);

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

  self->console = g_steal_pointer (&console);

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
