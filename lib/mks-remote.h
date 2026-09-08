/* mks-remote.h
 *
 * Copyright 2026 Christian Hergert
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

#define MKS_TYPE_REMOTE (mks_remote_get_type())

MKS_AVAILABLE_IN_ALL
MKS_DECLARE_INTERNAL_TYPE (MksRemote, mks_remote, MKS, REMOTE, MksDevice)

MKS_AVAILABLE_IN_ALL
DexFuture *mks_remote_press          (MksRemote            *self,
                                      guint                 linux_keycode);
MKS_AVAILABLE_IN_ALL
void       mks_remote_press_async    (MksRemote            *self,
                                      guint                 linux_keycode,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean   mks_remote_press_finish   (MksRemote            *self,
                                      GAsyncResult         *result,
                                      GError              **error);
MKS_AVAILABLE_IN_ALL
DexFuture *mks_remote_release        (MksRemote            *self,
                                      guint                 linux_keycode);
MKS_AVAILABLE_IN_ALL
void       mks_remote_release_async  (MksRemote            *self,
                                      guint                 linux_keycode,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean   mks_remote_release_finish (MksRemote            *self,
                                      GAsyncResult         *result,
                                      GError              **error);

G_END_DECLS
