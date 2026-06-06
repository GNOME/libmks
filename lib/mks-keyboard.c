/* mks-keyboard.c
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
#include "mks-keyboard.h"
#include "mks-util-private.h"

#include "mks-keymap-xorgevdev2qnum-private.h"

/**
 * MksKeyboard:
 *
 * A virtualized QEMU keyboard.
 */

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
mks_keyboard_set_modifiers (MksKeyboard         *self,
                            MksKeyboardModifier  modifiers)
{
  g_assert (MKS_IS_KEYBOARD (self));

  if (self->modifiers != modifiers)
    {
      self->modifiers = modifiers;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODIFIERS]);
    }
}

static void
mks_keyboard_keyboard_notify_cb (MksKeyboard     *self,
                                 GParamSpec      *pspec,
                                 MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_KEYBOARD (self));
  g_assert (pspec != NULL);
  g_assert (MKS_QEMU_IS_KEYBOARD (keyboard));

  if (strcmp (pspec->name, "modifiers") == 0)
    mks_keyboard_set_modifiers (self, mks_qemu_keyboard_get_modifiers (keyboard));
}

static void
mks_keyboard_set_keyboard (MksKeyboard     *self,
                           MksQemuKeyboard *keyboard)
{
  g_assert (MKS_IS_KEYBOARD (self));
  g_assert (!keyboard || MKS_QEMU_IS_KEYBOARD (keyboard));
  g_assert (self->keyboard == NULL);

  if (g_set_object (&self->keyboard, keyboard))
    {
      g_signal_connect_object (self->keyboard,
                               "notify",
                               G_CALLBACK (mks_keyboard_keyboard_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);
      mks_keyboard_set_modifiers (self, mks_qemu_keyboard_get_modifiers (keyboard));
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

  /**
   * MksKeyboard:modifiers:
   *
   * Active keyboard modifiers.
   */
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

/**
 * mks_keyboard_get_modifiers:
 * @self: an #MksKeyboard
 *
 * Get the active keyboard modifiers.
 */
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

/**
 * mks_keyboard_press:
 * @self: an #MksKeyboard
 * @keycode: the hardware keycode
 *
 * Presses @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_keyboard_press (MksKeyboard         *self,
                    guint                keycode)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_KEYBOARD (self));

  if (!check_keyboard (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_keyboard_call_press_future (self->keyboard, keycode),
                            begin_time,
                            "keyboard.press");
}

void
mks_keyboard_press_async (MksKeyboard         *self,
                          guint                keycode,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_keyboard_press (self, keycode));
}

/**
 * mks_keyboard_press_finish:
 * @self: a `MksKeyboard`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Keyboard.press].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_keyboard_press_finish (MksKeyboard   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_keyboard_release:
 * @self: an #MksKeyboard
 * @keycode: the hardware keycode
 *
 * Releases @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_keyboard_release (MksKeyboard         *self,
                      guint                keycode)
{
  g_autoptr(GError) error = NULL;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_KEYBOARD (self));

  if (!check_keyboard (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (mks_qemu_keyboard_call_release_future (self->keyboard, keycode),
                            begin_time,
                            "keyboard.release");
}

void
mks_keyboard_release_async (MksKeyboard         *self,
                            guint                keycode,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_keyboard_release (self, keycode));
}

/**
 * mks_keyboard_release_finish:
 * @self: a `MksKeyboard`
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a call to [method@Mks.Keyboard.release].
 *
 * Returns: %TRUE if the operation completed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
mks_keyboard_release_finish (MksKeyboard   *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  return ret;
}

/**
 * mks_keyboard_translate:
 * @keyval: the keyval
 * @keycode: the hardware keycode
 * @translated: (out): the translated keycode
 *
 * Translate a keycode to a QEMU compatible one.
 */
void
mks_keyboard_translate (guint              keyval,
                        guint              keycode,
                        guint             *translated)
{
  g_assert (translated != NULL);

  if (keycode < xorgevdev_to_qnum_len &&
      xorgevdev_to_qnum[keycode] != 0)
    *translated = xorgevdev_to_qnum[keycode];
  else
    *translated = keycode;
}
