/*
 * mks-keyboard.c
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
#include "mks-keyboard.h"

struct _MksKeyboard
{
  MksDevice        parent_instance;
  MksQemuKeyboard *keyboard;
  guint            modifiers;
};

struct _MksKeyboardClass
{
  MksDeviceClass parent_instance;
};

G_DEFINE_FINAL_TYPE (MksKeyboard, mks_keyboard, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MODIFIERS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_keyboard_keyboard_notify_cb (MksKeyboard     *self,
                                 GParamSpec      *pspec,
                                 MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_KEYBOARD (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_KEYBOARD (keyboard));

  if (FALSE) {}
  else if (strcmp (pspec->name, "modifiers") == 0)
    {
      self->modifiers = mks_qemu_keyboard_get_modifiers (keyboard);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODIFIERS]);
    }
}

static void
mks_keyboard_set_keyboard (MksKeyboard     *self,
                           MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_KEYBOARD (self));
  g_assert (!keyboard || MKS_QEMU_IS_KEYBOARD (keyboard));

  if (g_set_object (&self->keyboard, keyboard))
    {
      g_signal_connect_object (self->keyboard,
                               "notify",
                               G_CALLBACK (mks_keyboard_keyboard_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      self->modifiers = mks_qemu_keyboard_get_modifiers (keyboard);
    }
}

static gboolean
mks_keyboard_setup (MksDevice     *device,
                    MksQemuObject *object)
{
  MksKeyboard *self = (MksKeyboard *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_KEYBOARD (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_KEYBOARD (iface))
        mks_keyboard_set_keyboard (self, MKS_QEMU_KEYBOARD (iface));
    }

  return self->keyboard != NULL;
}

static void
mks_keyboard_dispose (GObject *object)
{
  MksKeyboard *self = (MksKeyboard *)object;

  g_clear_object (&self->keyboard);

  G_OBJECT_CLASS (mks_keyboard_parent_class)->dispose (object);
}

static void
mks_keyboard_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MksKeyboard *self = MKS_KEYBOARD (object);

  switch (prop_id)
    {
    case PROP_MODIFIERS:
      g_value_set_flags (value, mks_keyboard_get_modifiers (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_keyboard_class_init (MksKeyboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  device_class->setup = mks_keyboard_setup;

  object_class->dispose = mks_keyboard_dispose;
  object_class->get_property = mks_keyboard_get_property;

  properties [PROP_MODIFIERS] =
    g_param_spec_flags ("modifiers", NULL, NULL,
                        MKS_TYPE_KEYBOARD_MODIFIER,
                        MKS_KEYBOARD_MODIFIER_NONE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_keyboard_init (MksKeyboard *self)
{
}

MksKeyboardModifier
mks_keyboard_get_modifiers (MksKeyboard *self)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), 0);

  return self->modifiers;
}

static gboolean
check_keyboard (MksKeyboard  *self,
                GError      **error)
{
  if (self->keyboard == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_CONNECTED,
                           "Not connected");
      return FALSE;
    }

  return TRUE;
}

static void
mks_keyboard_press_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  MksQemuKeyboard *keyboard = (MksQemuKeyboard *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_KEYBOARD (keyboard));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_keyboard_call_press_finish (keyboard, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_keyboard_press:
 * @self: an #MksKeyboard
 * @keycode: the hardware keycode
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously presses @keycode.
 */
void
mks_keyboard_press (MksKeyboard         *self,
                    guint                keycode,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_KEYBOARD (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_keyboard_press);

  if (!check_keyboard (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_keyboard_call_press (self->keyboard,
                                  keycode,
                                  cancellable,
                                  mks_keyboard_press_cb,
                                  g_steal_pointer (&task));
}

gboolean
mks_keyboard_press_sync (MksKeyboard   *self,
                         guint          keycode,
                         GCancellable  *cancellable,
                         GError       **error)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_keyboard (self, error))
    return FALSE;

  return mks_qemu_keyboard_call_press_sync (self->keyboard, keycode, cancellable, error);
}

static void
mks_keyboard_release_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  MksQemuKeyboard *keyboard = (MksQemuKeyboard *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_QEMU_IS_KEYBOARD (keyboard));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mks_qemu_keyboard_call_release_finish (keyboard, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * mks_keyboard_release:
 * @self: an #MksKeyboard
 * @keycode: the hardware keycode
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously releases @keycode.
 */
void
mks_keyboard_release (MksKeyboard         *self,
                      guint                keycode,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MKS_IS_KEYBOARD (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mks_keyboard_release);

  if (!check_keyboard (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    mks_qemu_keyboard_call_release (self->keyboard,
                                    keycode,
                                    cancellable,
                                    mks_keyboard_release_cb,
                                    g_steal_pointer (&task));
}

gboolean
mks_keyboard_release_sync (MksKeyboard   *self,
                           guint          keycode,
                           GCancellable  *cancellable,
                           GError       **error)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!check_keyboard (self, error))
    return FALSE;

  return mks_qemu_keyboard_call_release_sync (self->keyboard, keycode, cancellable, error);
}
