/* mks-chardev.c
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

#include "mks-chardev-private.h"
#include "mks-util-private.h"

G_DEFINE_ABSTRACT_TYPE (MksChardev, mks_chardev, MKS_TYPE_DEVICE)

static void
mks_chardev_class_init (MksChardevClass *klass)
{
}

static void
mks_chardev_init (MksChardev *self)
{
}

const char *
mks_chardev_get_name (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), NULL);

  if (MKS_CHARDEV_GET_CLASS (self)->get_name == NULL)
    return NULL;

  return MKS_CHARDEV_GET_CLASS (self)->get_name (self);
}

gboolean
mks_chardev_get_fe_opened (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);

  if (MKS_CHARDEV_GET_CLASS (self)->get_fe_opened == NULL)
    return FALSE;

  return MKS_CHARDEV_GET_CLASS (self)->get_fe_opened (self);
}

gboolean
mks_chardev_get_echo (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);

  if (MKS_CHARDEV_GET_CLASS (self)->get_echo == NULL)
    return FALSE;

  return MKS_CHARDEV_GET_CLASS (self)->get_echo (self);
}

char *
mks_chardev_dup_owner (MksChardev *self)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), NULL);

  if (MKS_CHARDEV_GET_CLASS (self)->dup_owner == NULL)
    return NULL;

  return MKS_CHARDEV_GET_CLASS (self)->dup_owner (self);
}

/**
 * mks_chardev_register_fd:
 * @self: a `MksChardev`
 * @fd: a Unix file descriptor
 *
 * Registers @fd for chardev stream handling.
 *
 * This function takes ownership of @fd immediately.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_chardev_register_fd (MksChardev *self,
                         int         fd)
{
  dex_return_error_if_fail (MKS_IS_CHARDEV (self));

  if (MKS_CHARDEV_GET_CLASS (self)->register_fd == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CHARDEV_GET_CLASS (self)->register_fd (self, fd);
}

void
mks_chardev_register_fd_async (MksChardev          *self,
                               int                  fd,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_chardev_register_fd (self, fd));
}

gboolean
mks_chardev_register_fd_finish (MksChardev    *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_chardev_send_break:
 * @self: a `MksChardev`
 *
 * Sends a break event to the chardev.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_chardev_send_break (MksChardev *self)
{
  dex_return_error_if_fail (MKS_IS_CHARDEV (self));

  if (MKS_CHARDEV_GET_CLASS (self)->send_break == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_CHARDEV_GET_CLASS (self)->send_break (self);
}

void
mks_chardev_send_break_async (MksChardev          *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_chardev_send_break (self));
}

gboolean
mks_chardev_send_break_finish (MksChardev    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}
