/*
 * mks-mouse.h
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

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_MOUSE            (mks_mouse_get_type ())
#define MKS_MOUSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_MOUSE, MksMouse))
#define MKS_MOUSE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_MOUSE, MksMouse const))
#define MKS_MOUSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MKS_TYPE_MOUSE, MksMouseClass))
#define MKS_IS_MOUSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKS_TYPE_MOUSE))
#define MKS_IS_MOUSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MKS_TYPE_MOUSE))
#define MKS_MOUSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MKS_TYPE_MOUSE, MksMouseClass))

typedef struct _MksMouseClass MksMouseClass;

MKS_AVAILABLE_IN_ALL
GType    mks_mouse_get_type        (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
gboolean mks_mouse_get_is_absolute (MksMouse *self);

G_END_DECLS
