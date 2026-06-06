/* mks-mapped-paintable.c
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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <glib-unix.h>
#include <gtk/gtk.h>
#include <pixman.h>

#include "mks-mapped-paintable-private.h"
#include "mks-util-private.h"

typedef struct
{
  gpointer data;
  gsize    length;
  int      fd;
} MksMappedBytes;

struct _MksMappedPaintable
{
  GObject         parent_instance;
  GBytes         *bytes;
  GdkTexture     *texture;
  cairo_region_t *update_region;
  guint           width;
  guint           height;
  guint           stride;
  guint           pixman_format;
  guint           dirty : 1;
};

static void
mks_mapped_bytes_free (gpointer data)
{
  MksMappedBytes *mapped = data;

  if (mapped == NULL)
    return;

  if (mapped->data != NULL && mapped->length > 0)
    munmap (mapped->data, mapped->length);
  g_clear_fd (&mapped->fd, NULL);
  g_free (mapped);
}

static GdkMemoryFormat
pixman_to_memory_format (guint pixman_format)
{
  switch (pixman_format)
    {
    case PIXMAN_a8r8g8b8:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      return GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
#else
      return GDK_MEMORY_A8R8G8B8_PREMULTIPLIED;
#endif
    case PIXMAN_x8r8g8b8:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      return GDK_MEMORY_B8G8R8X8;
#else
      return GDK_MEMORY_X8R8G8B8;
#endif
    case PIXMAN_a8:
      return GDK_MEMORY_A8;
    default:
      return GDK_MEMORY_N_FORMATS;
    }
}

static int
mks_mapped_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return MKS_MAPPED_PAINTABLE (paintable)->width;
}

static int
mks_mapped_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return MKS_MAPPED_PAINTABLE (paintable)->height;
}

static double
mks_mapped_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  MksMappedPaintable *self = MKS_MAPPED_PAINTABLE (paintable);

  return self->height ? (double) self->width / (double) self->height : 0.0;
}

static void
mks_mapped_paintable_rebuild_texture (MksMappedPaintable *self)
{
  g_autoptr(GdkMemoryTextureBuilder) builder = NULL;
  g_autoptr(GdkTexture) texture = NULL;

  if (self->bytes == NULL)
    return;

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, self->bytes);
  gdk_memory_texture_builder_set_format (builder, pixman_to_memory_format (self->pixman_format));
  gdk_memory_texture_builder_set_width (builder, self->width);
  gdk_memory_texture_builder_set_height (builder, self->height);
  gdk_memory_texture_builder_set_stride (builder, self->stride);

  if (self->texture != NULL)
    gdk_memory_texture_builder_set_update_texture (builder, self->texture);

  if (self->update_region != NULL)
    gdk_memory_texture_builder_set_update_region (builder, self->update_region);

  texture = gdk_memory_texture_builder_build (builder);
  g_set_object (&self->texture, texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);
  self->dirty = FALSE;
}

static void
mks_mapped_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  MksMappedPaintable *self = MKS_MAPPED_PAINTABLE (paintable);
  graphene_rect_t area;

  if (self->dirty)
    mks_mapped_paintable_rebuild_texture (self);

  if (self->texture == NULL || self->width == 0 || self->height == 0)
    return;

  area = GRAPHENE_RECT_INIT (0, 0, width, height);
  gtk_snapshot_append_scaled_texture (snapshot,
                                      self->texture,
                                      GSK_SCALING_FILTER_NEAREST,
                                      &area);
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = mks_mapped_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = mks_mapped_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = mks_mapped_paintable_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_mapped_paintable_snapshot;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksMappedPaintable, mks_mapped_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

static void
mks_mapped_paintable_dispose (GObject *object)
{
  MksMappedPaintable *self = MKS_MAPPED_PAINTABLE (object);

  g_clear_pointer (&self->bytes, g_bytes_unref);
  g_clear_object (&self->texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (mks_mapped_paintable_parent_class)->dispose (object);
}

static void
mks_mapped_paintable_class_init (MksMappedPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = mks_mapped_paintable_dispose;
}

static void
mks_mapped_paintable_init (MksMappedPaintable *self)
{
}

MksMappedPaintable *
mks_mapped_paintable_new (void)
{
  return g_object_new (MKS_TYPE_MAPPED_PAINTABLE, NULL);
}

static gboolean
map_fd (int        fd,
        gsize      length,
        gpointer  *data,
        GError   **error)
{
  gpointer mapped;

  mapped = mmap (NULL, length, PROT_READ, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "Failed to mmap shared buffer: %s",
                   g_strerror (errno));
      return FALSE;
    }

  *data = mapped;
  return TRUE;
}

gboolean
mks_mapped_paintable_import (MksMappedPaintable  *self,
                             int                  fd,
                             guint                offset,
                             guint                width,
                             guint                height,
                             guint                stride,
                             guint                pixman_format,
                             cairo_region_t      *region,
                             GError             **error)
{
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) local_error = NULL;
  MksMappedBytes *mapped;
  gsize length;
  int dup_fd;

  g_return_val_if_fail (MKS_IS_MAPPED_PAINTABLE (self), FALSE);

  if (width == 0 || height == 0 || stride == 0 || fd < 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Invalid shared map");
      return FALSE;
    }

  if (pixman_to_memory_format (pixman_format) == GDK_MEMORY_N_FORMATS)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Unsupported pixman format");
      return FALSE;
    }

  length = (gsize) stride * height + offset;
  dup_fd = fcntl (fd, F_DUPFD_CLOEXEC, 3);

  if (dup_fd < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "Failed to duplicate shared map fd: %s",
                   g_strerror (errno));
      return FALSE;
    }

  mapped = g_new0 (MksMappedBytes, 1);
  mapped->fd = dup_fd;
  mapped->length = length;

  if (!map_fd (dup_fd, length, &mapped->data, &local_error))
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      mks_mapped_bytes_free (mapped);
      return FALSE;
    }

  bytes = g_bytes_new_with_free_func ((guint8 *) mapped->data + offset,
                                      length - offset,
                                      mks_mapped_bytes_free,
                                      mapped);

  if (self->width != width || self->height != height)
    {
      self->width = width;
      self->height = height;
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
    }

  self->stride = stride;
  self->pixman_format = pixman_format;
  g_clear_pointer (&self->bytes, g_bytes_unref);
  self->bytes = g_steal_pointer (&bytes);

  if (region != NULL)
    {
      if (self->update_region == NULL)
        self->update_region = cairo_region_copy (region);
      else
        cairo_region_union (self->update_region, region);
    }

  self->dirty = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return TRUE;
}

void
mks_mapped_paintable_clear (MksMappedPaintable *self)
{
  g_return_if_fail (MKS_IS_MAPPED_PAINTABLE (self));

  g_clear_pointer (&self->bytes, g_bytes_unref);
  g_clear_object (&self->texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);
  self->dirty = FALSE;
  self->width = 0;
  self->height = 0;
  self->stride = 0;
  self->pixman_format = 0;
}

void
mks_mapped_paintable_damage (MksMappedPaintable *self,
                             cairo_region_t     *region)
{
  g_return_if_fail (MKS_IS_MAPPED_PAINTABLE (self));
  g_return_if_fail (region != NULL);

  if (self->update_region == NULL)
    self->update_region = cairo_region_copy (region);
  else
    cairo_region_union (self->update_region, region);

  self->dirty = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}
