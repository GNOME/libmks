/*
 * mks-framebuffer.c
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

#include <cairo-gobject.h>
#include <gtk/gtk.h>

#include "mks-framebuffer-private.h"

/* The surface we're drawing to. The framebuffer isn't our fast path,
 * (that is DMA-BUF) but we should still try to make it reasonably
 * fast for the situations we may need to support.
 *
 * We use a single surface and update it as new content comes in, which
 * would race against the upload to the GPU except that we're on the
 * same thread and therefore the GPU upload has already happened for the
 * last frame if we're here already.
 *
 * The @damage region is updated as content changes so that we can
 * calculate how many pixels were damaged since the last snapshot. If it's
 * beyond our threshold ratio, then we snapshot with one big texture to
 * update (the whole surface) rather than the whole surface + damage
 * rectangles.
 *
 * The reason for this is that the GL renderer will likely already have
 * our "full framebuffer" (minus recent damages) in VRAM so we can reuse
 * it and then draw small damage rectangles after that (which get uploaded
 * on every frame).
 *
 * But again, what we really are hoping for us the DMA-BUF paintable to
 * get used instead.
 */

/* The percentage of the framebuffer that must be damaged before a new
 * scanout is performed instead of using damage rectangles.
 */
#define THRESHOLD_RATIO (.5)

struct _MksFramebuffer
{
  GObject parent_instance;
  cairo_surface_t *surface;
  cairo_region_t *damage;
  GdkPaintable *base_texture;
  guint width;
  guint height;
  guint threshold;
  cairo_format_t format;
};

static int
mks_framebuffer_get_intrinsic_width (GdkPaintable *paintable)
{
  return MKS_FRAMEBUFFER (paintable)->width;
}

static int
mks_framebuffer_get_intrinsic_height (GdkPaintable *paintable)
{
  return MKS_FRAMEBUFFER (paintable)->height;
}

static double
mks_framebuffer_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  double width = MKS_FRAMEBUFFER (paintable)->width;
  double height = MKS_FRAMEBUFFER (paintable)->height;

  return width / height;
}

static inline gboolean
mks_framebuffer_damage_over_threshold (MksFramebuffer *self)
{
  guint area = 0;
  guint n_rects;

  g_assert (MKS_IS_FRAMEBUFFER (self));

  n_rects = cairo_region_num_rectangles (self->damage);

  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (self->damage, i, &rect);
      area += rect.width * rect.height;
    }

  return area > self->threshold;
}

static void
mks_framebuffer_snapshot (GdkPaintable *paintable,
                          GdkSnapshot  *snapshot,
                          double        width,
                          double        height)
{
  MksFramebuffer *self = MKS_FRAMEBUFFER (paintable);
  guint n_rects;

  g_assert (GDK_IS_PAINTABLE (paintable));
  g_assert (GDK_IS_SNAPSHOT (snapshot));

  if G_UNLIKELY (mks_framebuffer_damage_over_threshold (self))
    {
      cairo_region_destroy (self->damage);
      self->damage = cairo_region_create ();
      g_clear_object (&self->base_texture);
    }

  if G_UNLIKELY (self->base_texture == NULL)
    {
      GtkSnapshot *texture_snapshot = gtk_snapshot_new ();
      cairo_t *cr;

      cr = gtk_snapshot_append_cairo (texture_snapshot,
                                      &GRAPHENE_RECT_INIT (0, 0, self->width, self->height));
      cairo_set_source_surface (cr, self->surface, 0, 0);
      cairo_rectangle (cr, 0, 0, self->width, self->height);
      cairo_fill (cr);
      cairo_destroy (cr);

      self->base_texture = gtk_snapshot_free_to_paintable (texture_snapshot,
                                                           &GRAPHENE_SIZE_INIT (self->width, self->height));
    }

  /* Always draw our "base texture" even though it's going to be
   * composited over on the GPU. It saves us a large GPU upload since
   * the GL renderer will cache the texture in VRAM in many cases.
   */
  gdk_paintable_snapshot (GDK_PAINTABLE (self->base_texture), snapshot, width, height);

  /* Now draw our damage rectangles which are going to require an upload
   * since we can't reuse them between frames without a lot of tracking.
   * You could do that though, if you reset the damage each snapshot and
   * then go and and hash/index them for re-use.
   */
  n_rects = cairo_region_num_rectangles (self->damage);
  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_t *cr;

      cairo_region_get_rectangle (self->damage, i, &rect);
      cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (rect.x, rect.y, rect.width, rect.height));
      cairo_set_source_surface (cr, self->surface, -rect.x, -rect.y);
      cairo_rectangle (cr, 0, 0, rect.width, rect.height);
      cairo_destroy (cr);
    }
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = mks_framebuffer_get_intrinsic_width;
  iface->get_intrinsic_height = mks_framebuffer_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = mks_framebuffer_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_framebuffer_snapshot;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksFramebuffer, mks_framebuffer, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

MksFramebuffer *
mks_framebuffer_new (guint           width,
                     guint           height,
                     cairo_format_t  format)
{
  g_autoptr(MksFramebuffer) self = NULL;
  cairo_t *cr;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (format != 0, NULL);

  self = g_object_new (MKS_TYPE_FRAMEBUFFER, NULL);
  self->width = width;
  self->height = height;
  self->format = format;
  self->damage = cairo_region_create ();
  self->threshold = (width * height) * THRESHOLD_RATIO;

  if (!(self->surface = cairo_image_surface_create (format, width, height)))
    return NULL;

  cr = cairo_create (self->surface);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);
  cairo_destroy (cr);

  return g_steal_pointer (&self);
}

void
mks_framebuffer_update (MksFramebuffer *self,
                        guint           x,
                        guint           y,
                        guint           width,
                        guint           height,
                        guint           stride,
                        cairo_format_t  format,
                        const guint8   *data,
                        gsize           data_len)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  g_return_if_fail (MKS_IS_FRAMEBUFFER (self));
  g_return_if_fail (data != NULL || data_len == 0);

  if G_UNLIKELY (data == NULL || data_len == 0)
    return;

  if (stride < width ||
      stride < cairo_format_stride_for_width (format, width) ||
      ((guint64)stride * (guint64)height) > data_len)
    return;

  surface = cairo_image_surface_create_for_data ((guint8 *)data, format, width, height, stride);
  cr = cairo_create (self->surface);
  cairo_set_source_surface (cr, surface, x, y);
  cairo_rectangle (cr, x, y, width, height);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  if (x == 0 && y == 0 && width == self->width && height == self->height)
    {
      g_clear_pointer (&self->damage, cairo_region_destroy);
      self->damage = cairo_region_create ();
    }
  else
    {
      cairo_region_union_rectangle (self->damage,
                                    &(cairo_rectangle_int_t) { x, y, width, height });
    }
}

static void
mks_framebuffer_finalize (GObject *object)
{
  MksFramebuffer *self = (MksFramebuffer *)object;

  g_clear_pointer (&self->surface, cairo_surface_destroy);
  g_clear_pointer (&self->damage, cairo_region_destroy);

  G_OBJECT_CLASS (mks_framebuffer_parent_class)->finalize (object);
}

static void
mks_framebuffer_class_init (MksFramebufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mks_framebuffer_finalize;
}

static void
mks_framebuffer_init (MksFramebuffer *self)
{
}
