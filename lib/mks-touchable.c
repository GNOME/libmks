/* mks-touchable.c
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

#include "mks-touchable-private.h"
#include "mks-util-private.h"

G_DEFINE_ABSTRACT_TYPE (MksTouchable, mks_touchable, MKS_TYPE_DEVICE)

static void
mks_touchable_class_init (MksTouchableClass *klass)
{
}

static void
mks_touchable_init (MksTouchable *self)
{
}

/**
 * mks_touchable_send_event:
 * @self: a `MksTouchable`
 * @kind: the kind of touch event
 * @num_slot: the slot number
 * @x: the x absolute coordinate
 * @y: the y absolute coordinate
 *
 * Sends a touch event.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_touchable_send_event (MksTouchable      *self,
                          MksTouchEventKind  kind,
                          guint64            num_slot,
                          double             x,
                          double             y)
{
  dex_return_error_if_fail (MKS_IS_TOUCHABLE (self));

  if (MKS_TOUCHABLE_GET_CLASS (self)->send_event == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_TOUCHABLE_GET_CLASS (self)->send_event (self, kind, num_slot, x, y);
}

void
mks_touchable_send_event_async (MksTouchable        *self,
                                MksTouchEventKind    kind,
                                guint64              num_slot,
                                double               x,
                                double               y,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_touchable_send_event (self, kind, num_slot, x, y));
}

gboolean
mks_touchable_send_event_finish (MksTouchable  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (MKS_IS_TOUCHABLE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

int
mks_touchable_get_max_slots (MksTouchable *self)
{
  g_return_val_if_fail (MKS_IS_TOUCHABLE (self), 0);

  if (MKS_TOUCHABLE_GET_CLASS (self)->get_max_slots == NULL)
    return 0;

  return MKS_TOUCHABLE_GET_CLASS (self)->get_max_slots (self);
}
