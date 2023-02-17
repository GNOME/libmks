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
#include "mks-display-picture-private.h"
#include "mks-inhibitor-private.h"
#include "mks-keyboard.h"
#include "mks-mouse.h"
#include "mks-paintable-private.h"
#include "mks-screen.h"

typedef struct
{
  MksScreen         *screen;
  MksDisplayPicture *picture;
  MksInhibitor      *inhibitor;
} MksDisplayPrivate;

enum {
  PROP_0,
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
  MksPaintable *paintable;
  int x, y, width, height;
  double display_ratio;
  double ratio;
  double w, h;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (area != NULL);

  width = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));
  display_ratio = (double)width / (double)height;

  if ((paintable = mks_display_picture_get_paintable (priv->picture)))
    ratio = gdk_paintable_get_intrinsic_aspect_ratio (GDK_PAINTABLE (paintable));
  else
    ratio = 1.;

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

  mks_display_picture_set_paintable (priv->picture, MKS_PAINTABLE (paintable));
}

static void
mks_display_connect (MksDisplay *self,
                     MksScreen  *screen)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (!screen || MKS_IS_SCREEN (screen));

  if (g_set_object (&priv->screen, screen))
    {
      mks_display_picture_set_keyboard (priv->picture, mks_screen_get_keyboard (screen));
      mks_display_picture_set_mouse (priv->picture, mks_screen_get_mouse (screen));

      mks_screen_attach (screen,
                         NULL,
                         mks_display_attach_cb,
                         g_object_ref (self));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCREEN]);
    }
}

static void
mks_display_disconnect (MksDisplay *self)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));

  g_clear_object (&priv->screen);
  g_clear_object (&priv->inhibitor);

  if (priv->picture != NULL)
    {
      mks_display_picture_set_paintable (priv->picture, NULL);
      mks_display_picture_set_keyboard (priv->picture, NULL);
      mks_display_picture_set_mouse (priv->picture, NULL);
    }
}

static gboolean
mks_display_legacy_event_cb (MksDisplay               *self,
                             GdkEvent                 *event,
                             GtkEventControllerLegacy *legacy)
{
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  GdkEventType event_type;

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_LEGACY (legacy));

  event_type = gdk_event_get_event_type (event);

  if (priv->screen == NULL)
    return GDK_EVENT_PROPAGATE;

  if (event_type == GDK_BUTTON_PRESS)
    {
      if (priv->inhibitor == NULL)
        priv->inhibitor = mks_inhibitor_new (GTK_WIDGET (priv->picture), event);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
mks_display_dispose (GObject *object)
{
  MksDisplay *self = (MksDisplay *)object;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  mks_display_disconnect (self);

  g_clear_pointer ((GtkWidget **)&priv->picture, gtk_widget_unparent);

  G_OBJECT_CLASS (mks_display_parent_class)->dispose (object);
}

static void
mks_display_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  MksDisplay *self = (MksDisplay *)widget;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  gtk_widget_snapshot_child (widget, GTK_WIDGET (priv->picture), snapshot);
}

static GtkSizeRequestMode
mks_display_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
mks_display_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  MksDisplay *self = (MksDisplay *)widget;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);

  g_assert (MKS_IS_DISPLAY (self));

  gtk_widget_measure (GTK_WIDGET (priv->picture), orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
mks_display_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  MksDisplay *self = (MksDisplay *)widget;
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  graphene_rect_t area;

  g_assert (MKS_IS_DISPLAY (self));

  GTK_WIDGET_CLASS (mks_display_parent_class)->size_allocate (widget, width, height, baseline);

  mks_display_get_paintable_area (self, &area);

  gtk_widget_size_allocate (GTK_WIDGET (priv->picture),
                            &(GtkAllocation) {
                              area.origin.x,
                              area.origin.y,
                              area.size.width,
                              area.size.height
                            },
                            -1);
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
  widget_class->size_allocate = mks_display_size_allocate;
  widget_class->snapshot = mks_display_snapshot;

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
  MksDisplayPrivate *priv = mks_display_get_instance_private (self);
  GtkEventController *controller;

  priv->picture = g_object_new (MKS_TYPE_DISPLAY_PICTURE, NULL);
  gtk_widget_set_parent (GTK_WIDGET (priv->picture), GTK_WIDGET (self));

  controller = gtk_event_controller_legacy_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect_object (controller,
                           "event",
                           G_CALLBACK (mks_display_legacy_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
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
