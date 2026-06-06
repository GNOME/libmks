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

#include "mks-keyboard-private.h"
#include "mks-util-private.h"

#include "mks-keymap-xorgevdev2qnum-private.h"

G_DEFINE_ABSTRACT_TYPE (MksKeyboard, mks_keyboard, MKS_TYPE_DEVICE)

static void
mks_keyboard_class_init (MksKeyboardClass *klass)
{
}

static void
mks_keyboard_init (MksKeyboard *self)
{
}

MksKeyboardModifier
mks_keyboard_get_modifiers (MksKeyboard *self)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), 0);

  if (MKS_KEYBOARD_GET_CLASS (self)->get_modifiers == NULL)
    return 0;

  return MKS_KEYBOARD_GET_CLASS (self)->get_modifiers (self);
}

/**
 * mks_keyboard_press:
 * @self: a `MksKeyboard`
 * @keycode: the hardware keycode
 *
 * Presses @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_keyboard_press (MksKeyboard *self,
                    guint        keycode)
{
  dex_return_error_if_fail (MKS_IS_KEYBOARD (self));

  if (MKS_KEYBOARD_GET_CLASS (self)->press == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_KEYBOARD_GET_CLASS (self)->press (self, keycode);
}

void
mks_keyboard_press_async (MksKeyboard         *self,
                          guint                keycode,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_keyboard_press (self, keycode));
}

gboolean
mks_keyboard_press_finish (MksKeyboard   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_keyboard_release:
 * @self: a `MksKeyboard`
 * @keycode: the hardware keycode
 *
 * Releases @keycode.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_keyboard_release (MksKeyboard *self,
                      guint        keycode)
{
  dex_return_error_if_fail (MKS_IS_KEYBOARD (self));

  if (MKS_KEYBOARD_GET_CLASS (self)->release == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_KEYBOARD_GET_CLASS (self)->release (self, keycode);
}

void
mks_keyboard_release_async (MksKeyboard         *self,
                            guint                keycode,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_keyboard_release (self, keycode));
}

gboolean
mks_keyboard_release_finish (MksKeyboard   *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (MKS_IS_KEYBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

void
mks_keyboard_translate (guint  keyval,
                        guint  keycode,
                        guint *translated)
{
  g_return_if_fail (translated != NULL);

  if (keycode < xorgevdev_to_qnum_len &&
      xorgevdev_to_qnum[keycode] != 0)
    *translated = xorgevdev_to_qnum[keycode];
  else
    *translated = keycode;
}
