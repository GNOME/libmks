/* mks-display.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <stdlib.h>

#include "mks-css-private.h"
#include "mks-display.h"
#include "mks-keyboard.h"
#include "mks-mouse.h"
#include "mks-screen.h"

#include "mks-keymap-xorgevdev2qnum-private.h"

typedef struct
{
  /* The screen being displayed. We've gotten a GdkPaintable from it
   * which is connected to @picture for display.
   */
  MksScreen *screen;

  /* The picture widget in our template which is rendering the screens
   * painable when available.
   */
  GtkPicture *picture;

  /* Used to update the cursor position by calling into the MksMouse
   * API using move_to/move_by.
   */
  GtkEventControllerMotion *motion;

  /* Used to send key press/release events by calling into MksKeyboard
   * API using press/release and the hardware keycode.
   */
  GtkEventControllerKey *key;
} MksDisplayPrivate;

enum {
  PROP_0,
  PROP_CONTENT_FIT,
  PROP_SCREEN,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (MksDisplay, mks_display, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
mks_display_translate_keycode (MksDisplay *self,
                               guint       keyval,
                               guint       keycode,
                               guint      *translated)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (translated != NULL);

  if (keycode < xorgevdev_to_qnum_len &&
      xorgevdev_to_qnum[keycode] != 0)
    *translated = xorgevdev_to_qnum[keycode];
  else
    *translated = keycode;
}

static void
mks_display_keyboard_press_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  MksKeyboard *keyboard = (MksKeyboard *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_KEYBOARD (keyboard));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_keyboard_press_finish (keyboard, result, &error))
    g_warning ("Keyboard press failed: %s", error->message);
}

static void
mks_display_key_key_pressed_cb (MksDisplay            *self,
                                guint                  keyval,
                                guint                  keycode,
                                GdkModifierType        state,
                                GtkEventControllerKey *key)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksKeyboard *keyboard;
  guint qkeycode;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  if (priv->screen == NULL)
    return;

  keyboard = mks_screen_get_keyboard (priv->screen);
  mks_display_translate_keycode (self, keyval, keycode, &qkeycode);
  mks_keyboard_press (keyboard,
                      qkeycode,
                      NULL,
                      mks_display_keyboard_press_cb,
                      g_object_ref (self));
}

static void
mks_display_keyboard_release_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  MksKeyboard *keyboard = (MksKeyboard *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_KEYBOARD (keyboard));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_keyboard_release_finish (keyboard, result, &error))
    g_warning ("Keyboard release failed: %s", error->message);
}

static void
mks_display_key_key_released_cb (MksDisplay            *self,
                                 guint                  keyval,
                                 guint                  keycode,
                                 GdkModifierType        state,
                                 GtkEventControllerKey *key)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksKeyboard *keyboard;
  guint qkeycode;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  if (priv->screen == NULL)
    return;

  keyboard = mks_screen_get_keyboard (priv->screen);
  mks_display_translate_keycode (self, keyval, keycode, &qkeycode);
  mks_keyboard_release (keyboard,
                        qkeycode,
                        NULL,
                        mks_display_keyboard_release_cb,
                        g_object_ref (self));
}

static void
mks_display_translate_coordinate (MksDisplay *self,
                                  double     *x,
                                  double     *y)
{
  g_assert (MKS_IS_DISPLAY (self));

}

static void
mks_display_motion_enter_cb (MksDisplay               *self,
                             double                    x,
                             double                    y,
                             GtkEventControllerMotion *motion)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksMouse *mouse;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  if (priv->screen == NULL)
    return;

  mouse = mks_screen_get_mouse (priv->screen);
  mks_display_translate_coordinate (self, &x, &y);
  mks_mouse_move_to (mouse, x, y, NULL, NULL, NULL);
}

static void
mks_display_motion_motion_cb (MksDisplay               *self,
                              double                    x,
                              double                    y,
                              GtkEventControllerMotion *motion)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksMouse *mouse;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  if (priv->screen == NULL)
    return;

  mouse = mks_screen_get_mouse (priv->screen);
  mks_display_translate_coordinate (self, &x, &y);
  mks_mouse_move_to (mouse, x, y, NULL, NULL, NULL);
}

static void
mks_display_motion_leave_cb (MksDisplay               *self,
                             GtkEventControllerMotion *motion)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));
}

static void
mks_display_attach_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(MksDisplay) self = user_data;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksScreen *screen = (MksScreen *)object;
  g_autoptr(GdkPaintable) paintable = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_SCREEN (screen));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  paintable = mks_screen_attach_finish (screen, result, &error);

  if (priv->screen != screen)
    return;

  gtk_picture_set_paintable (priv->picture, paintable);
}

static void
mks_display_connect (MksDisplay *self,
                     MksScreen  *screen)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (priv->screen == NULL);

  g_set_object (&priv->screen, screen);

  mks_screen_attach (screen,
                     NULL,
                     mks_display_attach_cb,
                     g_object_ref (self));
}

static void
mks_display_disconnect (MksDisplay *self)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (priv->screen != NULL);

  gtk_picture_set_paintable (priv->picture, NULL);
  g_clear_object (&priv->screen);
}

static void
mks_display_dispose (GObject *object)
{
  MksDisplay *self = (MksDisplay *)object;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  if (priv->screen != NULL)
    mks_display_disconnect (self);

  g_clear_pointer ((GtkWidget **)&priv->picture, gtk_widget_unparent);

  G_OBJECT_CLASS (mks_display_parent_class)->dispose (object);
}

static void
mks_display_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksDisplay *self = MKS_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTENT_FIT:
      g_value_set_enum (value, mks_display_get_content_fit (self));
      break;

    case PROP_SCREEN:
      g_value_set_object (value, mks_display_get_screen (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_display_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MksDisplay *self = MKS_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTENT_FIT:
      mks_display_set_content_fit (self, g_value_get_enum (value));
      break;

    case PROP_SCREEN:
      mks_display_set_screen (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_display_class_init (MksDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = mks_display_dispose;
  object_class->get_property = mks_display_get_property;
  object_class->set_property = mks_display_set_property;

  properties [PROP_CONTENT_FIT] =
    g_param_spec_enum ("content-fit", NULL, NULL,
                       GTK_TYPE_CONTENT_FIT,
                       GTK_CONTENT_FIT_SCALE_DOWN,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SCREEN] =
    g_param_spec_object ("screen", NULL, NULL,
                         MKS_TYPE_SCREEN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "MksDisplay");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libmks/mks-display.ui");
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, key);
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, motion);
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, picture);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_motion_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_motion_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_motion_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_key_key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_key_key_released_cb);

  _mks_css_init ();
}

static void
mks_display_init (MksDisplay *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
mks_display_new (void)
{
  return g_object_new (MKS_TYPE_DISPLAY, NULL);
}

/**
 * mks_display_get_screen:
 * @self: a #MksDisplay
 *
 * Gets the screen connected to the display.
 *
 * Returns: (transfer none): a #MksScreen
 */
MksScreen *
mks_display_get_screen (MksDisplay *self)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_return_val_if_fail (MKS_IS_DISPLAY (self), NULL);

  return priv->screen;
}

void
mks_display_set_screen (MksDisplay *self,
                        MksScreen  *screen)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_return_if_fail (MKS_IS_DISPLAY (self));

  if (priv->screen == screen)
    return;

  if (priv->screen != NULL)
    mks_display_disconnect (self);

  if (screen != NULL)
    mks_display_connect (self, screen);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCREEN]);
}

GtkContentFit
mks_display_get_content_fit (MksDisplay *self)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_return_val_if_fail (MKS_IS_DISPLAY (self), 0);

  return gtk_picture_get_content_fit (priv->picture);
}

void
mks_display_set_content_fit (MksDisplay    *self,
                             GtkContentFit  content_fit)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_return_if_fail (MKS_IS_DISPLAY (self));

  gtk_picture_set_content_fit (priv->picture, content_fit);
}
