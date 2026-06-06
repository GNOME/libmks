/* mks-chardev.h
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
#include <vte/vte.h>

#include "mks-device.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_CHARDEV            (mks_chardev_get_type())
#define MKS_CHARDEV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_CHARDEV, MksChardev))
#define MKS_CHARDEV_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_CHARDEV, MksChardev const))
#define MKS_CHARDEV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MKS_TYPE_CHARDEV, MksChardevClass))
#define MKS_IS_CHARDEV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKS_TYPE_CHARDEV))
#define MKS_IS_CHARDEV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MKS_TYPE_CHARDEV))
#define MKS_CHARDEV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MKS_TYPE_CHARDEV, MksChardevClass))

typedef struct _MksChardevClass MksChardevClass;

MKS_AVAILABLE_IN_ALL
GType       mks_chardev_get_type           (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
const char *mks_chardev_get_name           (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
gboolean    mks_chardev_get_fe_opened      (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
gboolean    mks_chardev_get_echo           (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
char       *mks_chardev_dup_owner          (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
DexFuture  *mks_chardev_register_fd        (MksChardev           *self,
                                            int                   fd);
MKS_AVAILABLE_IN_ALL
void        mks_chardev_register_fd_async  (MksChardev           *self,
                                            int                   fd,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean    mks_chardev_register_fd_finish (MksChardev           *self,
                                            GAsyncResult         *result,
                                            GError              **error);
MKS_AVAILABLE_IN_ALL
DexFuture  *mks_chardev_create_pty         (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
void        mks_chardev_create_pty_async   (MksChardev           *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
MKS_AVAILABLE_IN_ALL
VtePty     *mks_chardev_create_pty_finish  (MksChardev           *self,
                                            GAsyncResult         *result,
                                            GError              **error);
MKS_AVAILABLE_IN_ALL
DexFuture  *mks_chardev_send_break         (MksChardev           *self);
MKS_AVAILABLE_IN_ALL
void        mks_chardev_send_break_async   (MksChardev           *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
MKS_AVAILABLE_IN_ALL
gboolean    mks_chardev_send_break_finish  (MksChardev           *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksChardev, g_object_unref)

G_END_DECLS
