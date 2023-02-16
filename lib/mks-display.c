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
#include "mks-paintable-private.h"
#include "mks-screen.h"
#include "mks-util-private.h"

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
  gulong notify_cursor_handler;
  gulong mouse_set_handler;

  /* Tracking the last known positions of mouse events so that we may
   * do something "reasonable" if the pointer is not absolute.
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
                                    MksPaintable *paintable)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (MKS_IS_PAINTABLE (paintable));

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
mks_display_invalidate_size_cb (MksDisplay   *self,
                                MksPaintable *paintable)
{
  g_assert (MKS_IS_DISPLAY (self));
  g_assert (MKS_IS_PAINTABLE (paintable));

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
mks_display_notify_cursor_cb (MksDisplay   *self,
                              GParamSpec   *pspec,
                              MksPaintable *paintable)
{
  GdkCursor *cursor;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (MKS_IS_PAINTABLE (paintable));

  cursor = _mks_paintable_get_cursor (paintable);

  gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
}

static void
mks_display_mouse_set_cb (MksDisplay   *self,
                          int           x,
                          int           y,
                          MksPaintable *paintable)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (MKS_IS_PAINTABLE (paintable));

  priv->last_mouse_x = x;
  priv->last_mouse_y = y;
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
      g_clear_signal_handler (&priv->notify_cursor_handler, priv->paintable);
      g_clear_signal_handler (&priv->mouse_set_handler, priv->paintable);
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
      priv->notify_cursor_handler =
        g_signal_connect_object (paintable,
                                 "notify::cursor",
                                 G_CALLBACK (mks_display_notify_cursor_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
      priv->notify_cursor_handler =
        g_signal_connect_object (paintable,
                                 "mouse-set",
                                 G_CALLBACK (mks_display_mouse_set_cb),
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

static gboolean
mks_display_legacy_event_cb (MksDisplay               *self,
                             GdkEvent                 *event,
                             GtkEventControllerLegacy *controller)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  GdkEventType event_type;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_CONTROLLER_LEGACY (controller));

  if (priv->screen == NULL || priv->paintable == NULL)
    return GDK_EVENT_PROPAGATE;

  event_type = gdk_event_get_event_type (event);

  switch ((int)event_type)
    {
    case GDK_MOTION_NOTIFY:
      {
        MksMouse *mouse = mks_screen_get_mouse (priv->screen);
        GdkSurface *surface = gdk_event_get_surface (event);
        GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));
        int guest_width = gdk_paintable_get_intrinsic_width (priv->paintable);
        int guest_height = gdk_paintable_get_intrinsic_height (priv->paintable);
        graphene_rect_t area;
        double translate_x;
        double translate_y;

        g_assert (MKS_IS_MOUSE (mouse));
        g_assert (GDK_IS_SURFACE (surface));

        mks_display_get_paintable_area (self, &area);

        gtk_native_get_surface_transform (native, &translate_x, &translate_y);

        if (mks_mouse_get_is_absolute (mouse))
          {
            gdouble x, y;

            if (gdk_event_get_position (event, &x, &y))
              {
                x -= translate_x;
                y -= translate_y;

                gtk_widget_translate_coordinates (GTK_WIDGET (native),
                                                  GTK_WIDGET (self),
                                                  x, y, &x, &y);

                if (graphene_rect_contains_point (&area, &GRAPHENE_POINT_INIT (x, y)))
                  {
                    double guest_x = floor (x - area.origin.x) / area.size.width * guest_width;
                    double guest_y = floor (y - area.origin.y) / area.size.height * guest_height;

                    if (guest_x < 0 || guest_y < 0 ||
                        guest_x >= guest_width || guest_y >= guest_height)
                      return GDK_EVENT_PROPAGATE;

                    mks_mouse_move_to (mouse,
                                       guest_x,
                                       guest_y,
                                       NULL,
                                       mks_display_mouse_move_to_cb,
                                       g_object_ref (self));

                    return GDK_EVENT_STOP;
                  }
              }
          }
        else
          {
            double x, y;

            if (gdk_event_get_axis (event, GDK_AXIS_X, &x) &&
                gdk_event_get_axis (event, GDK_AXIS_Y, &y))
              {
                double delta_x = priv->last_mouse_x - (x / area.size.width) * guest_width;
                double delta_y = priv->last_mouse_y - (y / area.size.height) * guest_height;

                mks_mouse_move_by (mouse,
                                   delta_x,
                                   delta_y,
                                   NULL,
                                   mks_display_mouse_move_by_cb,
                                   g_object_ref (self));

                return GDK_EVENT_STOP;
              }
          }

        break;
      }

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      {
        MksMouse *mouse = mks_screen_get_mouse (priv->screen);
        int button = gdk_button_event_get_button (event);

        mks_display_translate_button (self, &button);

        if (event_type == GDK_BUTTON_PRESS)
          mks_mouse_press (mouse,
                           button,
                           NULL,
                           mks_display_mouse_press_cb,
                           g_object_ref (self));
        else
          mks_mouse_release (mouse,
                             button,
                             NULL,
                             mks_display_mouse_release_cb,
                             g_object_ref (self));

        return GDK_EVENT_STOP;
      }

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      {
        MksKeyboard *keyboard = mks_screen_get_keyboard (priv->screen);
        guint keycode = gdk_key_event_get_keycode (event);
        guint keyval = gdk_key_event_get_keycode (event);
        guint qkeycode;

        mks_display_translate_keycode (self, keyval, keycode, &qkeycode);

        if (event_type == GDK_KEY_PRESS)
          mks_keyboard_press (keyboard,
                              qkeycode,
                              NULL,
                              mks_display_keyboard_press_cb,
                              g_object_ref (self));
        else
          mks_keyboard_release (keyboard,
                                qkeycode,
                                NULL,
                                mks_display_keyboard_release_cb,
                                g_object_ref (self));

        return GDK_EVENT_STOP;
      }

    case GDK_SCROLL:
      {
        MksMouse *mouse = mks_screen_get_mouse (priv->screen);
        GdkScrollDirection direction = gdk_scroll_event_get_direction (event);
        gboolean inverted = mks_scroll_event_is_inverted (event);
        int button = -1;

        switch (direction)
          {
          case GDK_SCROLL_UP:
            button = MKS_MOUSE_BUTTON_WHEEL_UP;
            break;

          case GDK_SCROLL_DOWN:
            button = MKS_MOUSE_BUTTON_WHEEL_DOWN;
            break;

          case GDK_SCROLL_LEFT:
          case GDK_SCROLL_RIGHT:
          case GDK_SCROLL_SMOOTH:
          default:
            break;
          }

        if (button != -1)
          {
            if (inverted)
              {
                if (button == MKS_MOUSE_BUTTON_WHEEL_UP)
                  button = MKS_MOUSE_BUTTON_WHEEL_DOWN;
                else if (button == MKS_MOUSE_BUTTON_WHEEL_DOWN)
                  button = MKS_MOUSE_BUTTON_WHEEL_UP;
              }

            mks_mouse_press (mouse,
                             button,
                             NULL,
                             mks_display_mouse_press_cb,
                             g_object_ref (self));

            return GDK_EVENT_STOP;
          }

        break;
      }

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
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

  if (default_width <= 0)
    default_width = 640;

  if (default_height <= 0)
    default_height = 480;

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
      *minimum = 0;
      *natural = ceil (nat_width);
    }
  else
    {
      gdk_paintable_compute_concrete_size (priv->paintable,
                                           for_size < 0 ? 0 : for_size,
                                           0,
                                           default_width, default_height,
                                           &nat_width, &nat_height);
      *minimum = 0;
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

  _mks_css_init ();
}

static void
mks_display_init (MksDisplay *self)
{
  GtkEventController *controller;

  controller = gtk_event_controller_legacy_new ();
  g_signal_connect_object (controller,
                           "event",
                           G_CALLBACK (mks_display_legacy_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
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
