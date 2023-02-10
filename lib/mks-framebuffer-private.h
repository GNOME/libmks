/*
 * mks-framebuffer-private.h
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

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define MKS_TYPE_FRAMEBUFFER (mks_framebuffer_get_type())

G_DECLARE_FINAL_TYPE (MksFramebuffer, mks_framebuffer, MKS, FRAMEBUFFER, GObject)

MksFramebuffer *mks_framebuffer_new    (guint            width,
                                        guint            height,
                                        cairo_format_t   format);
void            mks_framebuffer_update (MksFramebuffer  *self,
                                        guint            x,
                                        guint            y,
                                        guint            width,
                                        guint            height,
                                        guint            stride,
                                        cairo_format_t   format,
                                        const guint8    *data,
                                        gsize            data_len);

G_END_DECLS
