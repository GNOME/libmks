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

#include "mks-enums.h"
#include "mks-keyboard.h"
#include "mks-mouse.h"
#include "mks-screen-private.h"
#include "mks-screen-attributes.h"
#include "mks-touchable.h"
#include "mks-util-private.h"

G_DEFINE_ABSTRACT_TYPE (MksScreen, mks_screen, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE_ADDRESS,
  PROP_HEIGHT,
  PROP_KIND,
  PROP_KEYBOARD,
  PROP_LAST_ACTIVE_TIME,
  PROP_MOUSE,
  PROP_NUMBER,
  PROP_TOUCHABLE,
  PROP_WIDTH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_screen_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MksScreen *self = MKS_SCREEN (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ADDRESS:
      g_value_set_string (value, mks_screen_get_device_address (self));
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, mks_screen_get_height (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, mks_screen_get_kind (self));
      break;

    case PROP_KEYBOARD:
      g_value_set_object (value, mks_screen_get_keyboard (self));
      break;

    case PROP_LAST_ACTIVE_TIME:
      g_value_set_int64 (value, mks_screen_get_last_active_time (self));
      break;

    case PROP_MOUSE:
      g_value_set_object (value, mks_screen_get_mouse (self));
      break;

    case PROP_NUMBER:
      g_value_set_uint (value, mks_screen_get_number (self));
      break;

    case PROP_TOUCHABLE:
      g_value_set_object (value, mks_screen_get_touchable (self));
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, mks_screen_get_width (self));
      break;

    default:
      G_OBJECT_CLASS (mks_screen_parent_class)->get_property (object, prop_id, value, pspec);
    }
}

static void
mks_screen_class_init (MksScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mks_screen_get_property;

  /**
   * MksScreen:device-address:
   *
   * The display device address.
   */
  properties [PROP_DEVICE_ADDRESS] =
    g_param_spec_string ("device-address", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:height:
   *
   * The screen height in pixels.
   */
  properties [PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:kind:
   *
   * The screen kind.
   */
  properties [PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       MKS_TYPE_SCREEN_KIND, MKS_SCREEN_KIND_TEXT,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:keyboard:
   *
   * The keyboard associated with the screen.
   */
  properties [PROP_KEYBOARD] =
    g_param_spec_object ("keyboard", NULL, NULL,
                         MKS_TYPE_KEYBOARD,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:last-active-time:
   *
   * The last time that display contents were observed for the screen.
   *
   * The value is in monotonic time, comparable to
   * [func@GLib.get_monotonic_time]. A value of 0 means no display content has
   * been observed yet.
   */
  properties [PROP_LAST_ACTIVE_TIME] =
    g_param_spec_int64 ("last-active-time", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:mouse:
   *
   * The mouse associated with the screen.
   */
  properties [PROP_MOUSE] =
    g_param_spec_object ("mouse", NULL, NULL,
                         MKS_TYPE_MOUSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:number:
   *
   * The screen number.
   */
  properties [PROP_NUMBER] =
    g_param_spec_uint ("number", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:touchable:
   *
   * The touch device associated with the screen.
   */
  properties [PROP_TOUCHABLE] =
    g_param_spec_object ("touchable", NULL, NULL,
                         MKS_TYPE_TOUCHABLE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksScreen:width:
   *
   * The screen width in pixels.
   */
  properties [PROP_WIDTH] =
    g_param_spec_uint ("width", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
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

void
_mks_screen_mark_active (MksScreen *self)
{
  gint64 now;

  g_return_if_fail (MKS_IS_SCREEN (self));

  now = g_get_monotonic_time ();

  if (self->last_active_time != now)
    {
      self->last_active_time = now;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_ACTIVE_TIME]);
    }
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

/**
 * mks_screen_get_last_active_time:
 * @self: a `MksScreen`
 *
 * Gets the last time that display contents were observed for @self.
 *
 * The value is in monotonic time, comparable to [func@GLib.get_monotonic_time].
 * A value of 0 means no display content has been observed yet.
 *
 * Returns: the last active time for @self, or 0
 */
gint64
mks_screen_get_last_active_time (MksScreen *self)
{
  g_return_val_if_fail (MKS_IS_SCREEN (self), 0);

  return self->last_active_time;
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
