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

  /* The paintable containing the screen content */
  GdkPaintable *paintable;
  gulong invalidate_contents_handler;
  gulong invalidate_size_handler;

  /* Used to update the cursor position by calling into the MksMouse
   * API using move_to/move_by.
   */
  GtkEventControllerMotion *motion;

  /* Used to send key press/release events by calling into MksKeyboard
   * API using press/release and the hardware keycode.
   */
  GtkEventControllerKey *key;

  /* Used to send mouse press/release events translated from the current
   * button in the gesture. X,Y coordinates are expected to already be
   * updated from GtkEventControllerMotion::motion events.
   */
  GtkGestureClick *click;

  /* Tracking the last known positions of mouse events so that we may
   * emulate mks_mouse_move_by() using GtkEventControllerMotion.
   */
  double last_mouse_x;
  double last_mouse_y;
} MksDisplayPrivate;

enum {
  PROP_0,
  PROP_PAINTABLE,
  PROP_SCREEN,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (MksDisplay, mks_display, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
mks_display_get_paintable_area (MksDisplay      *self,
                                graphene_rect_t *area)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  int x, y, width, height;
  double display_ratio;
  double ratio;
  double w, h;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (area != NULL);

  width = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));
  display_ratio = (double)width / (double)height;
  ratio = gdk_paintable_get_intrinsic_aspect_ratio (priv->paintable);

  if (ratio > display_ratio)
    {
      w = width;
      h = width / ratio;
    }
  else
    {
      w = height * ratio;
      h = height;
    }

  x = (width - ceil (w)) / 2;
  y = floor(height - ceil (h)) / 2;

  *area = GRAPHENE_RECT_INIT (x, y, w, h);
}

static void
mks_display_invalidate_contents_cb (MksDisplay   *self,
                                    GdkPaintable *paintable)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GDK_IS_PAINTABLE (paintable));

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
mks_display_invalidate_size_cb (MksDisplay   *self,
                                GdkPaintable *paintable)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GDK_IS_PAINTABLE (paintable));

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
mks_display_set_paintable (MksDisplay   *self,
                           GdkPaintable *paintable)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (!paintable || GDK_IS_PAINTABLE (paintable));

  if (priv->paintable == paintable)
    return;

  if (priv->paintable != NULL)
    {
      g_clear_signal_handler (&priv->invalidate_contents_handler, priv->paintable);
      g_clear_signal_handler (&priv->invalidate_size_handler, priv->paintable);
      g_clear_object (&priv->paintable);
    }

  if (paintable != NULL)
    {
      priv->paintable = g_object_ref (paintable);
      priv->invalidate_contents_handler =
        g_signal_connect_object (paintable,
                                 "invalidate-contents",
                                 G_CALLBACK (mks_display_invalidate_contents_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
      priv->invalidate_size_handler =
        g_signal_connect_object (paintable,
                                 "invalidate-size",
                                 G_CALLBACK (mks_display_invalidate_size_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAINTABLE]);
}

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

static gboolean
mks_display_translate_coordinate (MksDisplay *self,
                                  double     *x,
                                  double     *y)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  graphene_rect_t area;
  int width;
  int height;

  g_assert (MKS_IS_DISPLAY (self));

  if (priv->paintable == NULL)
    return FALSE;

  mks_display_get_paintable_area (self, &area);

  if (!graphene_rect_contains_point (&area, &GRAPHENE_POINT_INIT (*x, *y)))
    return FALSE;

  *x -= area.origin.x;
  *y -= area.origin.y;

  width = gdk_paintable_get_intrinsic_width (priv->paintable);
  height = gdk_paintable_get_intrinsic_height (priv->paintable);

  *x = *x / area.size.width * width;
  *y = *y / area.size.height * height;

  return TRUE;
}

static void
mks_display_mouse_move_to_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  MksMouse *mouse = (MksMouse *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_mouse_move_to_finish (mouse, result, &error))
    g_warning ("Failed move_to: %s", error->message);
}

static void
mks_display_mouse_move_by_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  MksMouse *mouse = (MksMouse *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_mouse_move_by_finish (mouse, result, &error))
    g_warning ("Failed move_by: %s", error->message);
}

static void
mks_display_motion (MksDisplay *self,
                    double      x,
                    double      y)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksMouse *mouse;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (!priv->screen || MKS_IS_SCREEN (priv->screen));

  if (priv->screen == NULL)
    return;

  /* TODO:
   *
   * This is pretty crappy right now because as you enter and
   * leave you never really reset your position within the remote
   * display.
   *
   * To fix this, we need real grabs or some other mechanism so
   * that we can hide the local cursor and warp it to where we
   * discover the cursor in the remote display upon entering
   * the picture widget.
   */

  mouse = mks_screen_get_mouse (priv->screen);

  if (mks_mouse_get_is_absolute (mouse))
    mks_mouse_move_to (mouse,
                       x, y,
                       NULL,
                       mks_display_mouse_move_to_cb,
                       g_object_ref (self));
  else
    mks_mouse_move_by (mouse,
                       x - priv->last_mouse_x,
                       y - priv->last_mouse_y,
                       NULL,
                       mks_display_mouse_move_by_cb,
                       g_object_ref (self));

  priv->last_mouse_x = x;
  priv->last_mouse_y = y;
}

static void
mks_display_motion_enter_cb (MksDisplay               *self,
                             double                    x,
                             double                    y,
                             GtkEventControllerMotion *motion)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  if (mks_display_translate_coordinate (self, &x, &y))
    mks_display_motion (self, x, y);
}

static void
mks_display_motion_motion_cb (MksDisplay               *self,
                              double                    x,
                              double                    y,
                              GtkEventControllerMotion *motion)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  if (mks_display_translate_coordinate (self, &x, &y))
    mks_display_motion (self, x, y);
}

static void
mks_display_motion_leave_cb (MksDisplay               *self,
                             GtkEventControllerMotion *motion)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));
}

static void
mks_display_translate_button (MksDisplay *self,
                              int        *button)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (button != NULL);

  switch (*button)
    {
    case 1: *button = MKS_MOUSE_BUTTON_LEFT;   break;
    case 2: *button = MKS_MOUSE_BUTTON_MIDDLE; break;
    case 3: *button = MKS_MOUSE_BUTTON_RIGHT;  break;
    case 8: *button = MKS_MOUSE_BUTTON_SIDE;   break;
    case 9: *button = MKS_MOUSE_BUTTON_EXTRA;  break;
    default: break;
    }
}

static void
mks_display_mouse_press_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  MksMouse *mouse = (MksMouse *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_mouse_press_finish (mouse, result, &error))
    g_warning ("Mouse press failed: %s", error->message);
}

static void
mks_display_click_pressed_cb (MksDisplay      *self,
                              int              n_press,
                              double           x,
                              double           y,
                              GtkGestureClick *click)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksMouse *mouse;
  int button;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (priv->screen == NULL)
    return;

  mouse = mks_screen_get_mouse (priv->screen);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
  mks_display_translate_button (self, &button);
  mks_mouse_press (mouse,
                   button,
                   NULL,
                   mks_display_mouse_press_cb,
                   g_object_ref (self));
}

static void
mks_display_mouse_release_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  MksMouse *mouse = (MksMouse *)object;
  g_autoptr(MksDisplay) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_MOUSE (mouse));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (MKS_IS_DISPLAY (self));

  if (!mks_mouse_release_finish (mouse, result, &error))
    g_warning ("Mouse release failed: %s", error->message);
}

static void
mks_display_click_released_cb (MksDisplay      *self,
                               int              n_press,
                               double           x,
                               double           y,
                               GtkGestureClick *click)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  MksMouse *mouse;
  int button;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (priv->screen == NULL)
    return;

  mouse = mks_screen_get_mouse (priv->screen);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
  mks_display_translate_button (self, &button);
  mks_mouse_release (mouse,
                     button,
                     NULL,
                     mks_display_mouse_release_cb,
                     g_object_ref (self));
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

  mks_display_set_paintable (self, paintable);
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

  mks_display_set_paintable (self, NULL);

  g_clear_object (&priv->screen);
}

static void
mks_display_dispose (GObject *object)
{
  MksDisplay *self = (MksDisplay *)object;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  if (priv->screen != NULL)
    mks_display_disconnect (self);

  G_OBJECT_CLASS (mks_display_parent_class)->dispose (object);
}

static void
mks_display_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  MksDisplay *self = (MksDisplay *)widget;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  graphene_rect_t area;

  if (priv->paintable == NULL)
    return;

  mks_display_get_paintable_area (self, &area);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &area.origin);
  gdk_paintable_snapshot (priv->paintable, snapshot, area.size.width, area.size.height);
  gtk_snapshot_restore (snapshot);
}

static GtkSizeRequestMode
mks_display_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
mks_display_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int            for_size,
                     int           *minimum,
                     int           *natural,
                     int           *minimum_baseline,
                     int           *natural_baseline)
{
  MksDisplay *self = (MksDisplay *)widget;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  double min_width, min_height, nat_width, nat_height;
  int default_width;
  int default_height;

  g_assert (MKS_IS_DISPLAY (self));

  if (priv->paintable == NULL || for_size == 0)
    {
      *minimum = 0;
      *natural = 0;
      return;
    }

  default_width = gdk_paintable_get_intrinsic_width (priv->paintable);
  default_height = gdk_paintable_get_intrinsic_width (priv->paintable);

  gdk_paintable_compute_concrete_size (priv->paintable,
                                       0, 0,
                                       default_width, default_height,
                                       &min_width, &min_height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gdk_paintable_compute_concrete_size (priv->paintable,
                                           0,
                                           for_size < 0 ? 0 : for_size,
                                           default_width, default_height,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_width);
      *natural = ceil (nat_width);
    }
  else
    {
      gdk_paintable_compute_concrete_size (priv->paintable,
                                           for_size < 0 ? 0 : for_size,
                                           0,
                                           default_width, default_height,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_height);
      *natural = ceil (nat_height);
    }
}

static void
mks_display_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksDisplay *self = MKS_DISPLAY (object);
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, priv->paintable);
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

  widget_class->get_request_mode = mks_display_get_request_mode;
  widget_class->measure = mks_display_measure;
  widget_class->snapshot = mks_display_snapshot;

  properties [PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SCREEN] =
    g_param_spec_object ("screen", NULL, NULL,
                         MKS_TYPE_SCREEN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "MksDisplay");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libmks/mks-display.ui");
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, click);
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, key);
  gtk_widget_class_bind_template_child_private (widget_class, MksDisplay, motion);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, mks_display_click_released_cb);
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
