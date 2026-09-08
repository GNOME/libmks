/* mks-remote.c
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

#include "config.h"

#include "mks-remote-private.h"
#include "mks-util-private.h"

/**
 * MksRemote:
 *
 * A remote control which sends exact Linux input keycodes to a virtual
 * machine.
 */
G_DEFINE_ABSTRACT_TYPE (MksRemote, mks_remote, MKS_TYPE_DEVICE)

static void
mks_remote_class_init (MksRemoteClass *klass)
{
}

static void
mks_remote_init (MksRemote *self)
{
}

/**
 * mks_remote_press:
 * @self: a `MksRemote`
 * @linux_keycode: a Linux input keycode
 *
 * Presses @linux_keycode on the guest remote control.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE
 */
DexFuture *
mks_remote_press (MksRemote *self,
                  guint      linux_keycode)
{
  dex_return_error_if_fail (MKS_IS_REMOTE (self));

  if (MKS_REMOTE_GET_CLASS (self)->press == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_REMOTE_GET_CLASS (self)->press (self, linux_keycode);
}

/**
 * mks_remote_press_async:
 * @self: a `MksRemote`
 * @linux_keycode: a Linux input keycode
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (nullable) (scope async): callback to invoke on completion
 * @user_data: (nullable): data for @callback
 *
 * Asynchronously presses @linux_keycode on the guest remote control.
 */
void
mks_remote_press_async (MksRemote           *self,
                        guint                linux_keycode,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_remote_press (self, linux_keycode));
}

/**
 * mks_remote_press_finish:
 * @self: a `MksRemote`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`, or %NULL
 *
 * Completes an asynchronous remote-control key press.
 *
 * Returns: %TRUE if the key press was delivered; otherwise %FALSE and @error
 *   is set
 */
gboolean
mks_remote_press_finish (MksRemote    *self,
                         GAsyncResult *result,
                         GError      **error)
{
  g_return_val_if_fail (MKS_IS_REMOTE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_remote_release:
 * @self: a `MksRemote`
 * @linux_keycode: a Linux input keycode
 *
 * Releases @linux_keycode on the guest remote control.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE
 */
DexFuture *
mks_remote_release (MksRemote *self,
                    guint      linux_keycode)
{
  dex_return_error_if_fail (MKS_IS_REMOTE (self));

  if (MKS_REMOTE_GET_CLASS (self)->release == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_REMOTE_GET_CLASS (self)->release (self, linux_keycode);
}

/**
 * mks_remote_release_async:
 * @self: a `MksRemote`
 * @linux_keycode: a Linux input keycode
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (nullable) (scope async): callback to invoke on completion
 * @user_data: (nullable): data for @callback
 *
 * Asynchronously releases @linux_keycode on the guest remote control.
 */
void
mks_remote_release_async (MksRemote           *self,
                          guint                linux_keycode,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_remote_release (self, linux_keycode));
}

/**
 * mks_remote_release_finish:
 * @self: a `MksRemote`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`, or %NULL
 *
 * Completes an asynchronous remote-control key release.
 *
 * Returns: %TRUE if the key release was delivered; otherwise %FALSE and
 *   @error is set
 */
gboolean
mks_remote_release_finish (MksRemote    *self,
                           GAsyncResult *result,
                           GError      **error)
{
  g_return_val_if_fail (MKS_IS_REMOTE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}
