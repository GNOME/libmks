/* mks-mouse.c
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

#include "mks-mouse-private.h"
#include "mks-util-private.h"

G_DEFINE_ABSTRACT_TYPE (MksMouse, mks_mouse, MKS_TYPE_DEVICE)

static void
mks_mouse_class_init (MksMouseClass *klass)
{
}

static void
mks_mouse_init (MksMouse *self)
{
}

gboolean
mks_mouse_get_is_absolute (MksMouse *self)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);

  if (MKS_MOUSE_GET_CLASS (self)->get_is_absolute == NULL)
    return FALSE;

  return MKS_MOUSE_GET_CLASS (self)->get_is_absolute (self);
}

static DexFuture *
mks_mouse_not_supported (void)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

/**
 * mks_mouse_press:
 * @self: a `MksMouse`
 * @button: the mouse button
 *
 * Presses @button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_press (MksMouse       *self,
                 MksMouseButton  button)
{
  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (MKS_MOUSE_GET_CLASS (self)->press == NULL)
    return mks_mouse_not_supported ();

  return MKS_MOUSE_GET_CLASS (self)->press (self, button);
}

void
mks_mouse_press_async (MksMouse            *self,
                       MksMouseButton       button,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_mouse_press (self, button));
}

gboolean
mks_mouse_press_finish (MksMouse      *self,
                        GAsyncResult  *result,
                        GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_mouse_release:
 * @self: a `MksMouse`
 * @button: the mouse button
 *
 * Releases @button.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_release (MksMouse       *self,
                   MksMouseButton  button)
{
  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (MKS_MOUSE_GET_CLASS (self)->release == NULL)
    return mks_mouse_not_supported ();

  return MKS_MOUSE_GET_CLASS (self)->release (self, button);
}

void
mks_mouse_release_async (MksMouse            *self,
                         MksMouseButton       button,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_mouse_release (self, button));
}

gboolean
mks_mouse_release_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_mouse_move_to:
 * @self: a `MksMouse`
 * @x: the x coordinate
 * @y: the y coordinate
 *
 * Moves the mouse to the absolute position at @x and @y.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_move_to (MksMouse *self,
                   guint     x,
                   guint     y)
{
  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (MKS_MOUSE_GET_CLASS (self)->move_to == NULL)
    return mks_mouse_not_supported ();

  return MKS_MOUSE_GET_CLASS (self)->move_to (self, x, y);
}

void
mks_mouse_move_to_async (MksMouse            *self,
                         guint                x,
                         guint                y,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_mouse_move_to (self, x, y));
}

gboolean
mks_mouse_move_to_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_mouse_move_by:
 * @self: a `MksMouse`
 * @delta_x: the x coordinate delta
 * @delta_y: the y coordinate delta
 *
 * Moves the mouse by @delta_x and @delta_y.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_mouse_move_by (MksMouse *self,
                   int       delta_x,
                   int       delta_y)
{
  dex_return_error_if_fail (MKS_IS_MOUSE (self));

  if (MKS_MOUSE_GET_CLASS (self)->move_by == NULL)
    return mks_mouse_not_supported ();

  return MKS_MOUSE_GET_CLASS (self)->move_by (self, delta_x, delta_y);
}

void
mks_mouse_move_by_async (MksMouse            *self,
                         int                  delta_x,
                         int                  delta_y,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_mouse_move_by (self, delta_x, delta_y));
}

gboolean
mks_mouse_move_by_finish (MksMouse      *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}
