/* mks-screen.h
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

#include <gdk/gdk.h>
#include <libdex.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_SCREEN            (mks_screen_get_type())

MKS_AVAILABLE_IN_ALL
MKS_DECLARE_INTERNAL_TYPE (MksScreen, mks_screen, MKS, SCREEN, MksDevice)

/**
 * MksScreenKind:
 * @MKS_SCREEN_KIND_TEXT: A text only screen.
 * @MKS_SCREEN_KIND_GRAPHIC: A graphical screen.
 * 
 * A screen kind.
 */
typedef enum _MksScreenKind
{
  MKS_SCREEN_KIND_TEXT    = 0,
  MKS_SCREEN_KIND_GRAPHIC = 1,
} MksScreenKind;

MKS_AVAILABLE_IN_ALL
MksScreenKind  mks_screen_get_kind           (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
MksKeyboard   *mks_screen_get_keyboard       (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
MksMouse      *mks_screen_get_mouse          (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
MksTouchable  *mks_screen_get_touchable      (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_width          (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_height         (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
guint          mks_screen_get_number         (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
const char    *mks_screen_get_device_address (MksScreen            *self);
MKS_AVAILABLE_IN_ALL
DexFuture     *mks_screen_configure          (MksScreen            *self,
                                              MksScreenAttributes  *attributes);
MKS_AVAILABLE_IN_ALL
void           mks_screen_configure_async    (MksScreen            *self,
                                              MksScreenAttributes  *attributes,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean       mks_screen_configure_finish   (MksScreen            *self,
                                              GAsyncResult         *result,
                                              GError              **error);
MKS_AVAILABLE_IN_ALL
DexFuture     *mks_screen_attach             (MksScreen            *self,
                                              GdkDisplay           *display);
MKS_AVAILABLE_IN_ALL
void           mks_screen_attach_async       (MksScreen            *self,
                                              GdkDisplay           *display,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
MKS_AVAILABLE_IN_ALL
GdkPaintable  *mks_screen_attach_finish      (MksScreen            *self,
                                              GAsyncResult         *result,
                                              GError              **error);

G_END_DECLS
