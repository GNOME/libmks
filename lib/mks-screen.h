/*
 * mks-screen.h
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

#pragma once

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <glib-object.h>

#include "mks-device.h"

G_BEGIN_DECLS

#define MKS_TYPE_SCREEN (mks_screen_get_type())

typedef enum _MksScreenKind
{
  MKS_SCREEN_KIND_TEXT = 0,
  MKS_SCREEN_KIND_GRAPHIC = 1,
} MksScreenKind;

MKS_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (MksScreen, mks_screen, MKS, SCREEN, MksDevice)

MKS_AVAILABLE_IN_ALL
MksScreenKind  mks_screen_get_kind     (MksScreen *self);
MKS_AVAILABLE_IN_ALL
MksKeyboard   *mks_screen_get_keyboard (MksScreen *self);
MKS_AVAILABLE_IN_ALL
MksMouse      *mks_screen_get_mouse    (MksScreen *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_width    (MksScreen *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_height   (MksScreen *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_number   (MksScreen *self);

G_END_DECLS
