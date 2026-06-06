/* mks-mapped-paintable-private.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MKS_TYPE_MAPPED_PAINTABLE (mks_mapped_paintable_get_type())

G_DECLARE_FINAL_TYPE (MksMappedPaintable, mks_mapped_paintable, MKS, MAPPED_PAINTABLE, GObject)

MksMappedPaintable *mks_mapped_paintable_new    (void);
gboolean            mks_mapped_paintable_import (MksMappedPaintable  *self,
                                                 int                  fd,
                                                 guint                offset,
                                                 guint                width,
                                                 guint                height,
                                                 guint                stride,
                                                 guint                pixman_format,
                                                 cairo_region_t      *region,
                                                 GError             **error);
void                mks_mapped_paintable_damage (MksMappedPaintable  *self,
                                                 cairo_region_t      *region);
void                mks_mapped_paintable_clear  (MksMappedPaintable  *self);

G_END_DECLS
