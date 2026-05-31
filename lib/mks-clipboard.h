/*
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <libdex.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_CLIPBOARD         (mks_clipboard_get_type())
#define MKS_TYPE_CLIPBOARD_CONTENT (mks_clipboard_content_get_type())

typedef enum _MksClipboardSelection
{
  MKS_CLIPBOARD_SELECTION_CLIPBOARD = 0,
  MKS_CLIPBOARD_SELECTION_PRIMARY = 1,
  MKS_CLIPBOARD_SELECTION_SECONDARY = 2,
} MksClipboardSelection;

typedef enum _MksClipboardOwner
{
  MKS_CLIPBOARD_OWNER_NONE,
  MKS_CLIPBOARD_OWNER_LOCAL,
  MKS_CLIPBOARD_OWNER_REMOTE,
} MksClipboardOwner;

MKS_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (MksClipboard, mks_clipboard, MKS, CLIPBOARD, GObject)

MKS_AVAILABLE_IN_ALL
GType                 mks_clipboard_content_get_type      (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
MksClipboardContent  *mks_clipboard_content_new           (const char             *mime_type,
                                                           GBytes                 *bytes);
MKS_AVAILABLE_IN_ALL
MksClipboardContent  *mks_clipboard_content_ref           (MksClipboardContent    *self);
MKS_AVAILABLE_IN_ALL
void                  mks_clipboard_content_unref         (MksClipboardContent    *self);
MKS_AVAILABLE_IN_ALL
const char           *mks_clipboard_content_get_mime_type (MksClipboardContent    *self);
MKS_AVAILABLE_IN_ALL
GBytes               *mks_clipboard_content_ref_bytes     (MksClipboardContent    *self);
MKS_AVAILABLE_IN_ALL
gboolean              mks_clipboard_get_registered        (MksClipboard           *self);
MKS_AVAILABLE_IN_ALL
MksClipboardOwner     mks_clipboard_get_owner             (MksClipboard           *self,
                                                           MksClipboardSelection   selection);
MKS_AVAILABLE_IN_ALL
char                **mks_clipboard_dup_mime_types        (MksClipboard           *self,
                                                           MksClipboardSelection   selection);
MKS_AVAILABLE_IN_ALL
DexFuture            *mks_clipboard_register              (MksClipboard           *self);
MKS_AVAILABLE_IN_ALL
void                  mks_clipboard_register_async        (MksClipboard           *self,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
MKS_AVAILABLE_IN_ALL
gboolean              mks_clipboard_register_finish       (MksClipboard           *self,
                                                           GAsyncResult           *result,
                                                           GError                **error);
MKS_AVAILABLE_IN_ALL
DexFuture            *mks_clipboard_unregister            (MksClipboard           *self);
MKS_AVAILABLE_IN_ALL
void                  mks_clipboard_unregister_async      (MksClipboard           *self,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
MKS_AVAILABLE_IN_ALL
gboolean              mks_clipboard_unregister_finish     (MksClipboard           *self,
                                                           GAsyncResult           *result,
                                                           GError                **error);
MKS_AVAILABLE_IN_ALL
DexFuture            *mks_clipboard_claim                 (MksClipboard           *self,
                                                           MksClipboardSelection   selection,
                                                           const char * const     *mime_types);
MKS_AVAILABLE_IN_ALL
DexFuture            *mks_clipboard_release               (MksClipboard           *self,
                                                           MksClipboardSelection   selection);
MKS_AVAILABLE_IN_ALL
DexFuture            *mks_clipboard_request               (MksClipboard           *self,
                                                           MksClipboardSelection   selection,
                                                           const char * const     *mime_types);
MKS_AVAILABLE_IN_ALL
void                  mks_clipboard_request_async         (MksClipboard           *self,
                                                           MksClipboardSelection   selection,
                                                           const char * const     *mime_types,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
MKS_AVAILABLE_IN_ALL
MksClipboardContent  *mks_clipboard_request_finish        (MksClipboard           *self,
                                                           GAsyncResult           *result,
                                                           GError                **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksClipboardContent, mks_clipboard_content_unref)

G_END_DECLS
