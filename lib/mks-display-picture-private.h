/* mks-display-picture.h
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

#include <gtk/gtk.h>

#include "mks-paintable-private.h"
#include "mks-types.h"

G_BEGIN_DECLS

#define MKS_TYPE_DISPLAY_PICTURE (mks_display_picture_get_type())

G_DECLARE_FINAL_TYPE (MksDisplayPicture, mks_display_picture, MKS, DISPLAY_PICTURE, GtkWidget)

GtkWidget    *mks_display_picture_new           (void);
MksPaintable *mks_display_picture_get_paintable (MksDisplayPicture *self);
void          mks_display_picture_set_paintable (MksDisplayPicture *self,
                                                 MksPaintable      *paintable);
MksMouse     *mks_display_picture_get_mouse     (MksDisplayPicture *self);
void          mks_display_picture_set_mouse     (MksDisplayPicture *self,
                                                 MksMouse          *mouse);
MksKeyboard  *mks_display_picture_get_keyboard  (MksDisplayPicture *self);
void          mks_display_picture_set_keyboard  (MksDisplayPicture *self,
                                                 MksKeyboard       *keyboard);

G_END_DECLS
