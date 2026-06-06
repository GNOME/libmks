/* mks-dbus-keyboard.c
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

#include "mks-device-private.h"
#include "mks-enums.h"
#include "mks-dbus-keyboard-private.h"
#include "mks-qemu.h"
#include "mks-util-private.h"


/**
 * MksDBusKeyboard:
 *
 * A virtualized QEMU keyboard.
 */

struct _MksDBusKeyboard
{
  MksKeyboard      parent_instance;
  MksQemuKeyboard *keyboard;
  guint            modifiers;
};

struct _MksDBusKeyboardClass
{
  MksKeyboardClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksDBusKeyboard, mks_dbus_keyboard, MKS_TYPE_KEYBOARD)

enum {
  PROP_0,
  PROP_MODIFIERS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static MksKeyboardModifier  mks_dbus_keyboard_get_modifiers (MksKeyboard *keyboard);
static DexFuture           *mks_dbus_keyboard_press         (MksKeyboard *keyboard,
                                                             guint        keycode);
static DexFuture           *mks_dbus_keyboard_release       (MksKeyboard *keyboard,
                                                             guint        keycode);


static void
mks_dbus_keyboard_set_modifiers (MksDBusKeyboard     *self,
                                 MksKeyboardModifier  modifiers)
{
  g_assert (MKS_IS_DBUS_KEYBOARD (self));

  if (self->modifiers != modifiers)
    {
      self->modifiers = modifiers;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODIFIERS]);
    }
}

static void
mks_dbus_keyboard_keyboard_notify_cb (MksDBusKeyboard *self,
                                      GParamSpec      *pspec,
                                      MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_DBUS_KEYBOARD (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_KEYBOARD (keyboard));

  if (strcmp (pspec->name, "modifiers") == 0)
    mks_dbus_keyboard_set_modifiers (self, mks_qemu_keyboard_get_modifiers (keyboard));
}

static void
mks_dbus_keyboard_set_keyboard (MksDBusKeyboard *self,
                                MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_DBUS_KEYBOARD (self));
  g_assert (!keyboard || MKS_QEMU_IS_KEYBOARD (keyboard));
  g_assert (self->keyboard == NULL);

  if (g_set_object (&self->keyboard, keyboard))
    {
      g_signal_connect_object (self->keyboard,
                               "notify",
                               G_CALLBACK (mks_dbus_keyboard_keyboard_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_dbus_keyboard_set_modifiers (self, mks_qemu_keyboard_get_modifiers (keyboard));
    }
}

static gboolean
mks_dbus_keyboard_setup (MksDevice *device,
                         GObject   *object)
{
  MksDBusKeyboard *self = (MksDBusKeyboard *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_DBUS_KEYBOARD (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_KEYBOARD (iface))
        mks_dbus_keyboard_set_keyboard (self, MKS_QEMU_KEYBOARD (iface));
    }

  return self->keyboard != NULL;
}

static void
mks_dbus_keyboard_dispose (GObject *object)
{
  MksDBusKeyboard *self = (MksDBusKeyboard *)object;

  g_clear_object (&self->keyboard);

  G_OBJECT_CLASS (mks_dbus_keyboard_parent_class)->dispose (object);
}

static void
mks_dbus_keyboard_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MksDBusKeyboard *self = MKS_DBUS_KEYBOARD (object);

  switch (prop_id)
    {
    case PROP_MODIFIERS:
      g_value_set_flags (value, mks_dbus_keyboard_get_modifiers (MKS_KEYBOARD (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_dbus_keyboard_class_init (MksDBusKeyboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);
  MksKeyboardClass *keyboard_class = MKS_KEYBOARD_CLASS (klass);

  keyboard_class->get_modifiers = mks_dbus_keyboard_get_modifiers;
  keyboard_class->press = mks_dbus_keyboard_press;
  keyboard_class->release = mks_dbus_keyboard_release;


  device_class->setup = mks_dbus_keyboard_setup;

  object_class->dispose = mks_dbus_keyboard_dispose;
  object_class->get_property = mks_dbus_keyboard_get_property;

  /**
   * MksDBusKeyboard:modifiers:
   *
   * Active keyboard modifiers.
   */
  properties [PROP_MODIFIERS] =
    g_param_spec_flags ("modifiers", NULL, NULL,
                        MKS_TYPE_KEYBOARD_MODIFIER, MKS_KEYBOARD_MODIFIER_NONE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_dbus_keyboard_init (MksDBusKeyboard *self)
{
}

/**
 * mks_dbus_keyboard_get_modifiers:
 * @self: an #MksDBusKeyboard
 *
 * Get the active keyboard modifiers.
 */
static MksKeyboardModifier
mks_dbus_keyboard_get_modifiers (MksKeyboard *keyboard)
{
  MksDBusKeyboard *self = MKS_DBUS_KEYBOARD (keyboard);

  g_return_val_if_fail (MKS_IS_DBUS_KEYBOARD (self), 0);

  return self->modifiers;
}

static gboolean
check_keyboard (MksDBusKeyboard  *self,
                GError          **error)
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

/**
 * mks_dbus_keyboard_press:
 * @self: an #MksDBusKeyboard
 * @keycode: the hardware keycode
 *
 * Presses @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_keyboard_press (MksKeyboard *keyboard,
                         guint        keycode)
{
  MksDBusKeyboard *self = MKS_DBUS_KEYBOARD (keyboard);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_KEYBOARD (self));

  if (!check_keyboard (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_keyboard_call_press_future (self->keyboard, keycode),
                            begin_time,
                            "keyboard.press");
}

/**
 * mks_dbus_keyboard_release:
 * @self: an #MksDBusKeyboard
 * @keycode: the hardware keycode
 *
 * Releases @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
static DexFuture *
mks_dbus_keyboard_release (MksKeyboard *keyboard,
                           guint        keycode)
{
  MksDBusKeyboard *self = MKS_DBUS_KEYBOARD (keyboard);
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_DBUS_KEYBOARD (self));

  if (!check_keyboard (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_keyboard_call_release_future (self->keyboard, keycode),
                            begin_time,
                            "keyboard.release");
}
