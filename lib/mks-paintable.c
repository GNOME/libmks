/*
 * mks-paintable.c
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

#include "mks-paintable.h"
#include "mks-screen.h"

struct _MksPaintable
{
  GObject    parent_instance;
  MksScreen *screen;
};

static void paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (MksPaintable, mks_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

enum {
  PROP_0,
  PROP_SCREEN,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GdkPaintable *
mks_paintable_new (MksScreen *screen)
{
  g_return_val_if_fail (MKS_IS_SCREEN (screen), NULL);

  return g_object_new (MKS_TYPE_PAINTABLE,
                       "screen", screen,
                       NULL);
}

static void
mks_paintable_set_screen (MksPaintable *self,
                          MksScreen    *screen)
{
  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (MKS_IS_SCREEN (screen));

  if (g_set_object (&self->screen, screen))
    {
      g_signal_connect_object (self->screen,
                               "notify::width",
                               G_CALLBACK (gdk_paintable_invalidate_size),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->screen,
                               "notify::height",
                               G_CALLBACK (gdk_paintable_invalidate_size),
                               self,
                               G_CONNECT_SWAPPED);
    }
}

static void
mks_paintable_dispose (GObject *object)
{
  MksPaintable *self = (MksPaintable *)object;

  g_clear_object (&self->screen);

  G_OBJECT_CLASS (mks_paintable_parent_class)->dispose (object);
}

static void
mks_paintable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MksPaintable *self = MKS_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, mks_paintable_get_screen (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_paintable_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MksPaintable *self = MKS_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      mks_paintable_set_screen (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_paintable_class_init (MksPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_paintable_dispose;
  object_class->get_property = mks_paintable_get_property;
  object_class->set_property = mks_paintable_set_property;

  properties [PROP_SCREEN] =
    g_param_spec_object ("screen", NULL, NULL,
                         MKS_TYPE_SCREEN,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_paintable_init (MksPaintable *self)
{
}

/**
 * mks_paintable_get_screen:
 * @self: a #MksPaintable
 *
 * Gets the #MksScreen displayed in the paintable.
 *
 * Returns: (nullable) (transfer none): a #MksScreen or %NULL
 */
MksScreen *
mks_paintable_get_screen (MksPaintable *self)
{
  g_return_val_if_fail (MKS_IS_PAINTABLE (self), NULL);

  return self->screen;
}

static int
mks_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  MksPaintable *self = MKS_PAINTABLE (paintable);

  return self->screen ? mks_screen_get_height (self->screen) : 0;
}

static int
mks_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  MksPaintable *self = MKS_PAINTABLE (paintable);

  return self->screen ? mks_screen_get_width (self->screen) : 0;
}

static double
mks_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  MksPaintable *self = MKS_PAINTABLE (paintable);
  double width;
  double height;

  if (self->screen == NULL)
    return 1.;

  width = mks_screen_get_width (self->screen);
  height = mks_screen_get_height (self->screen);

  if (width == 0 || height == 0)
    return 1;

  return width / height;
}

static GdkPaintableFlags
mks_paintable_get_flags (GdkPaintable *paintable)
{
  return 0;
}

static void
mks_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  MksPaintable *self = (MksPaintable *)paintable;

  g_assert (MKS_IS_PAINTABLE (self));
  g_assert (GDK_IS_SNAPSHOT (snapshot));

}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_height = mks_paintable_get_intrinsic_height;
  iface->get_intrinsic_width = mks_paintable_get_intrinsic_width;
  iface->get_flags = mks_paintable_get_flags;
  iface->get_intrinsic_aspect_ratio = mks_paintable_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_paintable_snapshot;
}
