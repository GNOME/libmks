/* mks-clipboard.c
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

#include "config.h"

#include "mks-clipboard-private-base.h"
#include "mks-util-private.h"

struct _MksClipboardContent
{
  int     ref_count;
  char   *mime_type;
  GBytes *bytes;
};

G_DEFINE_ABSTRACT_TYPE (MksClipboard, mks_clipboard, G_TYPE_OBJECT)
G_DEFINE_BOXED_TYPE (MksClipboardContent,
                     mks_clipboard_content,
                     mks_clipboard_content_ref,
                     mks_clipboard_content_unref)

static void
mks_clipboard_class_init (MksClipboardClass *klass)
{
}

static void
mks_clipboard_init (MksClipboard *self)
{
}

void
_mks_clipboard_set_read_func (MksClipboard         *self,
                              MksClipboardReadFunc  read_func,
                              gpointer              user_data,
                              GDestroyNotify        destroy)
{
  g_return_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->set_read_func != NULL)
    MKS_CLIPBOARD_GET_CLASS (self)->set_read_func (self, read_func, user_data, destroy);
}

MksClipboardContent *
mks_clipboard_content_new (const char *mime_type,
                           GBytes     *bytes)
{
  MksClipboardContent *self;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_new0 (MksClipboardContent, 1);
  self->ref_count = 1;
  self->mime_type = g_strdup (mime_type);
  self->bytes = g_bytes_ref (bytes);

  return self;
}

MksClipboardContent *
mks_clipboard_content_ref (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  self->ref_count++;

  return self;
}

void
mks_clipboard_content_unref (MksClipboardContent *self)
{
  if (self == NULL)
    return;

  g_return_if_fail (self->ref_count > 0);

  if (--self->ref_count == 0)
    {
      g_clear_pointer (&self->mime_type, g_free);
      g_clear_pointer (&self->bytes, g_bytes_unref);
      g_free (self);
    }
}

const char *
mks_clipboard_content_get_mime_type (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->mime_type;
}

GBytes *
mks_clipboard_content_dup_bytes (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_bytes_ref (self->bytes);
}

gboolean
mks_clipboard_get_registered (MksClipboard *self)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);

  if (MKS_CLIPBOARD_GET_CLASS (self)->get_registered == NULL)
    return FALSE;

  return MKS_CLIPBOARD_GET_CLASS (self)->get_registered (self);
}

MksClipboardOwner
mks_clipboard_get_owner (MksClipboard          *self,
                         MksClipboardSelection  selection)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), MKS_CLIPBOARD_OWNER_NONE);

  if (MKS_CLIPBOARD_GET_CLASS (self)->get_owner == NULL)
    return MKS_CLIPBOARD_OWNER_NONE;

  return MKS_CLIPBOARD_GET_CLASS (self)->get_owner (self, selection);
}

/**
 * mks_clipboard_dup_mime_types:
 * @self: a `MksClipboard`
 * @selection: a clipboard selection
 *
 * Gets the MIME types available for @selection.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): the MIME types.
 */
char **
mks_clipboard_dup_mime_types (MksClipboard          *self,
                              MksClipboardSelection  selection)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), NULL);

  if (MKS_CLIPBOARD_GET_CLASS (self)->dup_mime_types == NULL)
    return NULL;

  return MKS_CLIPBOARD_GET_CLASS (self)->dup_mime_types (self, selection);
}

/**
 * mks_clipboard_register:
 * @self: a `MksClipboard`
 *
 * Registers for clipboard redirection.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_clipboard_register (MksClipboard *self)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->register_ == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CLIPBOARD_GET_CLASS (self)->register_ (self);
}

void
mks_clipboard_register_async (MksClipboard        *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_clipboard_register (self));
}

gboolean
mks_clipboard_register_finish (MksClipboard  *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_clipboard_unregister:
 * @self: a `MksClipboard`
 *
 * Unregisters clipboard redirection.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_clipboard_unregister (MksClipboard *self)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->unregister == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CLIPBOARD_GET_CLASS (self)->unregister (self);
}

void
mks_clipboard_unregister_async (MksClipboard        *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_clipboard_unregister (self));
}

gboolean
mks_clipboard_unregister_finish (MksClipboard  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_clipboard_claim:
 * @self: a `MksClipboard`
 * @selection: a clipboard selection
 * @mime_types: (array zero-terminated=1): the available MIME types
 *
 * Claims @selection for the local peer.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_clipboard_claim (MksClipboard          *self,
                     MksClipboardSelection  selection,
                     const char * const    *mime_types)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->claim == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CLIPBOARD_GET_CLASS (self)->claim (self, selection, mime_types);
}

/**
 * mks_clipboard_release:
 * @self: a `MksClipboard`
 * @selection: a clipboard selection
 *
 * Releases @selection.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_clipboard_release (MksClipboard          *self,
                       MksClipboardSelection  selection)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->release == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CLIPBOARD_GET_CLASS (self)->release (self, selection);
}

/**
 * mks_clipboard_request:
 * @self: a `MksClipboard`
 * @selection: a clipboard selection
 * @mime_types: (array zero-terminated=1): MIME types in preference order
 *
 * Requests clipboard content.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   [struct@Mks.ClipboardContent].
 */
DexFuture *
mks_clipboard_request (MksClipboard          *self,
                       MksClipboardSelection  selection,
                       const char * const    *mime_types)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (MKS_CLIPBOARD_GET_CLASS (self)->request == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CLIPBOARD_GET_CLASS (self)->request (self, selection, mime_types);
}

void
mks_clipboard_request_async (MksClipboard          *self,
                             MksClipboardSelection  selection,
                             const char * const    *mime_types,
                             GCancellable          *cancellable,
                             GAsyncReadyCallback    callback,
                             gpointer               user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_clipboard_request (self, selection, mime_types));
}

MksClipboardContent *
mks_clipboard_request_finish (MksClipboard  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), NULL);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), NULL);

  return dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);
}
