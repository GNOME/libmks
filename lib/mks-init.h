/*
 * mks-init.h
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

#include <glib.h>

#include "mks-version-macros.h"

G_BEGIN_DECLS

MKS_AVAILABLE_IN_ALL
void mks_init              (void);
MKS_AVAILABLE_IN_ALL
int  mks_get_major_version (void);
MKS_AVAILABLE_IN_ALL
int  mks_get_minor_version (void);
MKS_AVAILABLE_IN_ALL
int  mks_get_micro_version (void);

G_END_DECLS
