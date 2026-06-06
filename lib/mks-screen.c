/* mks-screen.c
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

#include "mks-screen-private.h"
#include "mks-screen-attributes.h"
#include "mks-util-private.h"

G_DEFINE_ABSTRACT_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

static void
mks_screen_class_init (MksScreenClass *klass)
{
}

static void
mks_screen_init (MksScreen *self)
{
}

#define DELEGATE_OR_ZERO(method, fallback) \
  G_STMT_START { \
    g_return_val_if_fail (MKS_IS_SCREEN (self), fallback); \
    if (MKS_SCREEN_GET_CLASS (self)->method == NULL) \
      return fallback; \
    return MKS_SCREEN_GET_CLASS (self)->method (self); \
  } G_STMT_END

MksScreenKind
mks_screen_get_kind (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_kind, MKS_SCREEN_KIND_TEXT);
}

/**
 * mks_screen_get_keyboard:
 * @self: a `MksScreen`
 *
 * Gets the keyboard associated with @self.
 *
 * Returns: (transfer none) (nullable): a `MksKeyboard`.
 */
MksKeyboard *
mks_screen_get_keyboard (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_keyboard, NULL);
}

/**
 * mks_screen_get_mouse:
 * @self: a `MksScreen`
 *
 * Gets the mouse associated with @self.
 *
 * Returns: (transfer none) (nullable): a `MksMouse`.
 */
MksMouse *
mks_screen_get_mouse (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_mouse, NULL);
}

/**
 * mks_screen_get_touchable:
 * @self: a `MksScreen`
 *
 * Gets the touch device associated with @self.
 *
 * Returns: (transfer none) (nullable): a `MksTouchable`.
 */
MksTouchable *
mks_screen_get_touchable (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_touchable, NULL);
}

guint
mks_screen_get_width (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_width, 0);
}

guint
mks_screen_get_height (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_height, 0);
}

guint
mks_screen_get_number (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_number, 0);
}

const char *
mks_screen_get_device_address (MksScreen *self)
{
  DELEGATE_OR_ZERO (get_device_address, NULL);
}

/**
 * mks_screen_configure:
 * @self: a `MksScreen`
 * @attributes: (transfer full): the screen attributes
 *
 * Configures @self with @attributes.
 *
 * This function takes ownership of @attributes.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE.
 */
DexFuture *
mks_screen_configure (MksScreen           *self,
                      MksScreenAttributes *attributes)
{
  dex_return_error_if_fail (MKS_IS_SCREEN (self));
  dex_return_error_if_fail (attributes != NULL);

  if (MKS_SCREEN_GET_CLASS (self)->configure == NULL)
    {
      mks_screen_attributes_free (attributes);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "Not supported");
    }

  return MKS_SCREEN_GET_CLASS (self)->configure (self, attributes);
}

void
mks_screen_configure_async (MksScreen           *self,
                            MksScreenAttributes *attributes,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_screen_configure (self, attributes));
}

gboolean
mks_screen_configure_finish (MksScreen     *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_screen_attach:
 * @self: a `MksScreen`
 * @display: a `GdkDisplay`
 *
 * Creates a paintable that is updated with the contents of @self.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gdk.Paintable].
 */
DexFuture *
mks_screen_attach (MksScreen  *self,
                   GdkDisplay *display)
{
  dex_return_error_if_fail (MKS_IS_SCREEN (self));
  dex_return_error_if_fail (GDK_IS_DISPLAY (display));

  if (MKS_SCREEN_GET_CLASS (self)->attach == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return MKS_SCREEN_GET_CLASS (self)->attach (self, display);
}

void
mks_screen_attach_async (MksScreen           *self,
                         GdkDisplay          *display,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  mks_future_to_async_result (self, cancellable, callback, user_data, G_STRFUNC,
                              mks_screen_attach (self, display));
}

/**
 * mks_screen_attach_finish:
 * @self: a `MksScreen`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`, or %NULL
 *
 * Completes a request to attach @self.
 *
 * Returns: (transfer full) (nullable): a `GdkPaintable`.
 */
GdkPaintable *
mks_screen_attach_finish (MksScreen     *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);
}
