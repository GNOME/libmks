/* mks-cairo-framebuffer.c
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

#include <cairo-gobject.h>
#include <gtk/gtk.h>

#include "mks-cairo-framebuffer-private.h"
#include "mks-util-private.h"

struct _MksCairoFramebuffer
{
  GObject parent_instance;

  /* The underlying surface we'll draw to */
  cairo_surface_t *surface;

  /* A GBytes that we can use to reference slices which ultimately
   * references the cairo surface. This goes against the internal design
   * of GdkTexture (which are supposed to be immutable) so that we can
   * avoid additional copies beyond the one to the GPU.
   *
   * We somewhat abuse the GdkSnapshot diffing here by giving a new memory
   * texture for updates even though they point to the same memory. That
   * way the renderer uploads the new contents for the damaged area instead
   * of using the previously cached texture.
   */
  GBytes *content;

  /* A GdkMemoryTexture we export and refresh with update regions. */
  GdkTexture *texture;

  /* The format our framebuffer uses and corresponding format
   * the uploaded textures will use.
   */
  cairo_format_t  format;
  GdkMemoryFormat memory_format;

  /* The stride for the framebuffer so that the memory texture
   * can skip past the rest of the framebuffer data.
   */
  guint stride;

  /* Number of bytes per-pixel */
  guint bpp;

  /* The height and width the framebuffer was created with */
  guint height;
  guint width;

  /* The real width and height when tiling is taken into account */
  guint real_height;
  guint real_width;

  cairo_region_t *update_region;
};

enum {
  PROP_0,
  PROP_FORMAT,
  PROP_HEIGHT,
  PROP_WIDTH,
  N_PROPS
};

static cairo_user_data_key_t invalidate_key;

static int
mks_cairo_framebuffer_get_intrinsic_width (GdkPaintable *paintable)
{
  return mks_cairo_framebuffer_get_width (MKS_CAIRO_FRAMEBUFFER (paintable));
}

static int
mks_cairo_framebuffer_get_intrinsic_height (GdkPaintable *paintable)
{
  return mks_cairo_framebuffer_get_height (MKS_CAIRO_FRAMEBUFFER (paintable));
}

static double
mks_cairo_framebuffer_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  double width = gdk_paintable_get_intrinsic_width (paintable);
  double height = gdk_paintable_get_intrinsic_height (paintable);

  return width / height;
}

static void
mks_cairo_framebuffer_rebuild_texture (MksCairoFramebuffer *self)
{
  g_autoptr(GdkMemoryTextureBuilder) builder = NULL;
  g_autoptr(GdkTexture) texture = NULL;

  g_assert (MKS_IS_CAIRO_FRAMEBUFFER (self));
  g_assert (self->content != NULL);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, self->content);
  gdk_memory_texture_builder_set_format (builder, self->memory_format);
  gdk_memory_texture_builder_set_width (builder, self->width);
  gdk_memory_texture_builder_set_height (builder, self->height);
  gdk_memory_texture_builder_set_stride_for_plane (builder, 0, self->stride);

  if (self->texture != NULL)
    gdk_memory_texture_builder_set_update_texture (builder, self->texture);

  if (self->update_region != NULL)
    gdk_memory_texture_builder_set_update_region (builder, self->update_region);

  texture = gdk_memory_texture_builder_build (builder);
  g_set_object (&self->texture, texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);
}

static void
mks_cairo_framebuffer_snapshot_internal (MksCairoFramebuffer *self,
                                         GtkSnapshot         *snapshot,
                                         double               width,
                                         double               height,
                                         double               surface_x,
                                         double               surface_y,
                                         int                  scale)
{
  graphene_rect_t bounds;

  g_assert (MKS_IS_CAIRO_FRAMEBUFFER (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));
  g_assert (scale > 0);

  if (self->texture == NULL)
    mks_cairo_framebuffer_rebuild_texture (self);

  bounds = GRAPHENE_RECT_INIT (0, 0, width, height);
  bounds.origin.x = floor ((bounds.origin.x + surface_x) * scale) / scale - surface_x;
  bounds.origin.y = floor ((bounds.origin.y + surface_y) * scale) / scale - surface_y;
  bounds.size.width = ceil ((width + surface_x) * scale) / scale - surface_x - bounds.origin.x;
  bounds.size.height = ceil ((height + surface_y) * scale) / scale - surface_y - bounds.origin.y;

  gtk_snapshot_append_scaled_texture (snapshot,
                                      self->texture,
                                      GSK_SCALING_FILTER_NEAREST,
                                      &bounds);
}

static void
mks_cairo_framebuffer_paintable_snapshot (GdkPaintable *paintable,
                                          GdkSnapshot  *snapshot,
                                          double        width,
                                          double        height)
{
  mks_cairo_framebuffer_snapshot_internal (MKS_CAIRO_FRAMEBUFFER (paintable),
                                           GTK_SNAPSHOT (snapshot),
                                           width,
                                           height,
                                           0,
                                           0,
                                           1);
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = mks_cairo_framebuffer_get_intrinsic_width;
  iface->get_intrinsic_height = mks_cairo_framebuffer_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = mks_cairo_framebuffer_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_cairo_framebuffer_paintable_snapshot;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksCairoFramebuffer, mks_cairo_framebuffer, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

static GParamSpec *properties [N_PROPS];

static void
mks_cairo_framebuffer_constructed (GObject *object)
{
  MksCairoFramebuffer *self = (MksCairoFramebuffer *)object;

  G_OBJECT_CLASS (mks_cairo_framebuffer_parent_class)->constructed (object);

  switch (self->format)
    {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      self->memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
#else
      self->memory_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED;
#endif
      break;

    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB30:
#if _CAIRO_CHECK_VERSION(1, 17, 2)
    case CAIRO_FORMAT_RGB96F:
    case CAIRO_FORMAT_RGBA128F:
#endif
    case CAIRO_FORMAT_INVALID:
    default:
      g_warning ("Unsupported memory format from cairo format: 0x%x",
                 self->format);
      return;
    }

  self->real_width = self->width;
  self->real_height = self->height;

  self->surface = cairo_image_surface_create (self->format, self->real_width, self->real_height);

  if (self->surface == NULL)
    {
      g_warning ("Cairo surface creation failed: format=0x%x width=%u height=%u",
                 self->format, self->real_width, self->real_height);
      return;
    }

  self->stride = cairo_format_stride_for_width (self->format, self->real_width);
  self->bpp = self->stride / self->real_width;

  /* Currently only 4bbp are supported */
  g_assert (self->bpp == 4);

  self->content = g_bytes_new_with_free_func (cairo_image_surface_get_data (self->surface),
                                              self->stride * self->real_height,
                                              (GDestroyNotify) cairo_surface_destroy,
                                              cairo_surface_reference (self->surface));

  self->texture = NULL;
}

static void
mks_cairo_framebuffer_dispose (GObject *object)
{
  MksCairoFramebuffer *self = (MksCairoFramebuffer *)object;

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_pointer (&self->surface, cairo_surface_destroy);
  g_clear_object (&self->texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (mks_cairo_framebuffer_parent_class)->dispose (object);
}

static void
mks_cairo_framebuffer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  MksCairoFramebuffer *self = MKS_CAIRO_FRAMEBUFFER (object);

  switch (prop_id)
    {
    case PROP_FORMAT:
      g_value_set_enum (value, mks_cairo_framebuffer_get_format (self));
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, mks_cairo_framebuffer_get_height (self));
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, mks_cairo_framebuffer_get_width (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_cairo_framebuffer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  MksCairoFramebuffer *self = MKS_CAIRO_FRAMEBUFFER (object);

  switch (prop_id)
    {
    case PROP_FORMAT:
      self->format = g_value_get_enum (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_uint (value);
      break;

    case PROP_WIDTH:
      self->width = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_cairo_framebuffer_class_init (MksCairoFramebufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = mks_cairo_framebuffer_constructed;
  object_class->dispose = mks_cairo_framebuffer_dispose;
  object_class->get_property = mks_cairo_framebuffer_get_property;
  object_class->set_property = mks_cairo_framebuffer_set_property;

  properties[PROP_FORMAT] =
    g_param_spec_enum ("format", NULL, NULL,
                       CAIRO_GOBJECT_TYPE_FORMAT, CAIRO_FORMAT_RGB24,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_WIDTH] =
    g_param_spec_uint ("width", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_cairo_framebuffer_init (MksCairoFramebuffer *self)
{
  self->format = CAIRO_FORMAT_RGB24;
}

MksCairoFramebuffer *
mks_cairo_framebuffer_new (cairo_format_t format,
                           guint          width,
                           guint          height)
{
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  return g_object_new (MKS_TYPE_CAIRO_FRAMEBUFFER,
                       "format", format,
                       "height", height,
                       "width", width,
                       NULL);
}

static void
flush_and_invalidate_on_destroy (gpointer data)
{
  g_autoptr(MksCairoFramebuffer) self = data;

  cairo_surface_flush (self->surface);
  mks_cairo_framebuffer_rebuild_texture (self);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

cairo_t *
mks_cairo_framebuffer_update (MksCairoFramebuffer *self,
                              guint                x,
                              guint                y,
                              guint                width,
                              guint                height)
{
  cairo_t *cr;
  cairo_rectangle_int_t update_area;

  g_return_val_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self), NULL);
  g_return_val_if_fail (self->surface != NULL, NULL);

  update_area = (cairo_rectangle_int_t) { x, y, width, height };

  if (self->update_region == NULL)
    self->update_region = cairo_region_create_rectangle (&update_area);
  else
    cairo_region_union_rectangle (self->update_region, &update_area);

  cr = cairo_create (self->surface);
  cairo_translate (cr, x, y);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_clip (cr);

  cairo_set_user_data (cr,
                       &invalidate_key,
                       g_object_ref (self),
                       flush_and_invalidate_on_destroy);

  return cr;
}

void
mks_cairo_framebuffer_clear (MksCairoFramebuffer *self)
{
  cairo_t *cr;
  cairo_rectangle_int_t update_area;

  g_return_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self));

  cr = cairo_create (self->surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_rectangle (cr, 0, 0,
                   self->real_width,
                   self->real_height);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_flush (self->surface);

  update_area = (cairo_rectangle_int_t) { 0, 0, self->width, self->height };
  if (self->update_region == NULL)
    self->update_region = cairo_region_create_rectangle (&update_area);
  else
    cairo_region_union_rectangle (self->update_region, &update_area);

  mks_cairo_framebuffer_rebuild_texture (self);
}

void
mks_cairo_framebuffer_snapshot (MksCairoFramebuffer *self,
                                GtkSnapshot         *snapshot,
                                double               width,
                                double               height,
                                double               surface_x,
                                double               surface_y,
                                int                  scale)
{
  g_return_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self));
  g_return_if_fail (GTK_IS_SNAPSHOT (snapshot));
  g_return_if_fail (scale > 0);

  mks_cairo_framebuffer_snapshot_internal (self,
                                           snapshot,
                                           width,
                                           height,
                                           surface_x,
                                           surface_y,
                                           scale);
}

cairo_format_t
mks_cairo_framebuffer_get_format (MksCairoFramebuffer *self)
{
  g_return_val_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self), 0);

  return self->format;
}

guint
mks_cairo_framebuffer_get_height (MksCairoFramebuffer *self)
{
  g_return_val_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self), 0);

  return self->height;
}

guint
mks_cairo_framebuffer_get_width (MksCairoFramebuffer *self)
{
  g_return_val_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self), 0);

  return self->width;
}

void
mks_cairo_framebuffer_copy_to (MksCairoFramebuffer *self,
                               MksCairoFramebuffer *dest)
{
  cairo_t *cr;

  g_return_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (self));
  g_return_if_fail (MKS_IS_CAIRO_FRAMEBUFFER (dest));

  cr = cairo_create (dest->surface);
  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_rectangle (cr, 0, 0, self->width, self->height);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_fill (cr);
  cairo_destroy (cr);

  cairo_surface_flush (dest->surface);
}
