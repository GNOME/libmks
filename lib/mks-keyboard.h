/* mks-keyboard.h
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

#pragma once

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <libdex.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_KEYBOARD            (mks_keyboard_get_type())

MKS_AVAILABLE_IN_ALL
MKS_DECLARE_INTERNAL_TYPE (MksKeyboard, mks_keyboard, MKS, KEYBOARD, MksDevice)

/**
 * MksKeyboardModifier:
 * @MKS_KEYBOARD_MODIFIER_NONE: No modifier.
 * @MKS_KEYBOARD_MODIFIER_SCROLL_LOCK: Scroll lock.
 * @MKS_KEYBOARD_MODIFIER_NUM_LOCK: Numeric lock.
 * @MKS_KEYBOARD_MODIFIER_CAPS_LOCK: Caps lock.
 * 
 * The active keyboard modifiers.
 */
typedef enum _MksKeyboardModifier
{
  MKS_KEYBOARD_MODIFIER_NONE        = 0,
  MKS_KEYBOARD_MODIFIER_SCROLL_LOCK = 1 << 0,
  MKS_KEYBOARD_MODIFIER_NUM_LOCK    = 1 << 1,
  MKS_KEYBOARD_MODIFIER_CAPS_LOCK   = 1 << 2,
} MksKeyboardModifier;

MKS_AVAILABLE_IN_ALL
MksKeyboardModifier  mks_keyboard_get_modifiers  (MksKeyboard          *self);
MKS_AVAILABLE_IN_ALL
DexFuture           *mks_keyboard_press          (MksKeyboard          *self,
                                                  guint                 keycode);
MKS_AVAILABLE_IN_ALL
void                 mks_keyboard_press_async    (MksKeyboard          *self,
                                                  guint                 keycode,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean             mks_keyboard_press_finish   (MksKeyboard          *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
MKS_AVAILABLE_IN_ALL
DexFuture           *mks_keyboard_release        (MksKeyboard          *self,
                                                  guint                 keycode);
MKS_AVAILABLE_IN_ALL
void                 mks_keyboard_release_async  (MksKeyboard          *self,
                                                  guint                 keycode,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean             mks_keyboard_release_finish (MksKeyboard          *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
MKS_AVAILABLE_IN_ALL
void                 mks_keyboard_translate      (guint                 keyval,
                                                  guint                 keycode,
                                                  guint                *translated);

G_END_DECLS
