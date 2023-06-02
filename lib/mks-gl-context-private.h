/*
 * mks-gl-context-private.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <epoxy/egl.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GLuint mks_gl_context_import_dmabuf (GdkGLContext   *context,
                                     uint32_t        format,
                                     unsigned int    width,
                                     unsigned int    height,
                                     uint32_t        n_planes,
                                     const int      *fds,
                                     const uint32_t *strides,
                                     const uint32_t *offsets,
                                     const uint64_t *modifiers);

G_END_DECLS
