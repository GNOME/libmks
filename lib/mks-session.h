/* mks-session.h
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

#define MKS_TYPE_SESSION (mks_session_get_type())

MKS_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (MksSession, mks_session, MKS, SESSION, GObject)

MKS_AVAILABLE_IN_ALL
DexFuture    *mks_session_new           (MksTransport *transport);
MKS_AVAILABLE_IN_ALL
MksTransport *mks_session_dup_transport (MksSession   *self);
MKS_AVAILABLE_IN_ALL
GListModel   *mks_session_get_devices   (MksSession   *self);
MKS_AVAILABLE_IN_ALL
MksClipboard *mks_session_dup_clipboard (MksSession   *self);
MKS_AVAILABLE_IN_ALL
const char   *mks_session_get_name      (MksSession   *self);
MKS_AVAILABLE_IN_ALL
const char   *mks_session_get_uuid      (MksSession   *self);
MKS_AVAILABLE_IN_ALL
MksScreen    *mks_session_dup_screen    (MksSession   *self);

G_END_DECLS
