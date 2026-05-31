/*
 * mks-dmabuf-paintable.c
 *
 * Copyright 2023 Christian Hergert <christian@sourceandstack.com>
 * Copyright 2023 Bilal Elmoussaoui <belmouss@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>

#include <glib-unix.h>
#include <gtk/gtk.h>

#include "mks-dmabuf-paintable-private.h"
#include "mks-util-private.h"

/*
 * MksDmabufPaintable is a GdkPaintable that gets created the first time
 * `ScanoutDMABUF` is called.
 *
 * The scanout data is then stored until we receive a `UpdateDMABUF` call
 * so we can build the next texture from the full backing buffer.
 */

struct _MksDmabufPaintable
{
  GObject parent_instance;
  GdkTexture *texture;
  GdkDmabufTextureBuilder *builder;
  MksDmabufScanoutData *builder_data;
  guint x;
  guint y;
  guint width;
  guint height;
  guint backing_width;
  guint backing_height;
  guint dmabuf_updated : 1;
};

static int
mks_dmabuf_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return MKS_DMABUF_PAINTABLE (paintable)->width;
}

static int
mks_dmabuf_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return MKS_DMABUF_PAINTABLE (paintable)->height;
}

static double
mks_dmabuf_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  MksDmabufPaintable *self = MKS_DMABUF_PAINTABLE (paintable);

  if (self->height != 0)
    return (double)self->width / (double)self->height;
  else
    return 0.;
}

static void
mks_dmabuf_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  MksDmabufPaintable *self = (MksDmabufPaintable *)paintable;
  g_autoptr(GdkTexture) texture = NULL;
  g_autoptr(GError) error = NULL;
  graphene_rect_t area;
  graphene_rect_t clip;
  double scale_x;
  double scale_y;

  g_assert (MKS_IS_DMABUF_PAINTABLE (self));
  g_assert (GDK_IS_SNAPSHOT (snapshot));

  /**
   * If the widget gets resized, snapshot would be called even
   * if we didn't receive a new DMABufUpdate call.
   * So only create a new DmabufTexture when that happens
   */
  if (self->dmabuf_updated)
    {
      MKS_TRACE_SCOPE ("dmabuf.build-texture",
                       "width=%u height=%u",
                       self->width,
                       self->height);

      gdk_dmabuf_texture_builder_set_update_texture (self->builder, self->texture);
      texture = gdk_dmabuf_texture_builder_build (self->builder,
                                                  (GDestroyNotify)mks_dmabuf_scanout_data_free,
                                                  self->builder_data,
                                                  &error);
      if (error != NULL)
        {
          g_warning ("Failed to build texture: %s", error->message);
          return;
        }
      g_assert (texture != NULL);
      self->builder_data = NULL;
      /* Clear the update region to avoid unioning it with the next UpdateDMABUF call. */
      gdk_dmabuf_texture_builder_set_update_region (self->builder, NULL);
      g_set_object (&self->texture, texture);
      self->dmabuf_updated = FALSE;
    }

  if (self->width == 0 || self->height == 0 || self->texture == NULL)
    return;

  scale_x = width / self->width;
  scale_y = height / self->height;
  area = GRAPHENE_RECT_INIT (-(double)self->x * scale_x,
                             -(double)self->y * scale_y,
                             self->backing_width * scale_x,
                             self->backing_height * scale_y);
  clip = GRAPHENE_RECT_INIT (0, 0, width, height);
  gtk_snapshot_push_clip (snapshot, &clip);
  gtk_snapshot_append_texture (snapshot, self->texture, &area);
  gtk_snapshot_pop (snapshot);
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = mks_dmabuf_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = mks_dmabuf_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = mks_dmabuf_paintable_get_intrinsic_aspect_ratio;
  iface->snapshot = mks_dmabuf_paintable_snapshot;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksDmabufPaintable, mks_dmabuf_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

static void
mks_dmabuf_paintable_dispose (GObject *object)
{
  MksDmabufPaintable *self = (MksDmabufPaintable *)object;

  g_clear_object (&self->texture);
  g_clear_object (&self->builder);
  g_clear_pointer (&self->builder_data, mks_dmabuf_scanout_data_free);

  G_OBJECT_CLASS (mks_dmabuf_paintable_parent_class)->dispose (object);
}

static void
mks_dmabuf_paintable_class_init (MksDmabufPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_dmabuf_paintable_dispose;
}

static void
mks_dmabuf_paintable_init (MksDmabufPaintable *self)
{
}

void
mks_dmabuf_scanout_data_free (MksDmabufScanoutData *data)
{
  guint i;

  g_return_if_fail (data != NULL);

  for (i = 0; i < data->n_planes; i++)
    g_clear_fd (&data->dmabuf_fd[i], NULL);

  g_free (data);
}

static MksDmabufScanoutData *
mks_dmabuf_scanout_data_copy_fds (MksDmabufScanoutData  *data,
                                  GError               **error)
{
  MksDmabufScanoutData *copy;
  guint i;

  g_assert (data != NULL);

  copy = g_new0 (MksDmabufScanoutData, 1);
  *copy = *data;

  for (i = 0; i < MKS_DMABUF_MAX_PLANES; i++)
    copy->dmabuf_fd[i] = -1;

  for (i = 0; i < data->n_planes; i++)
    {
      copy->dmabuf_fd[i] = fcntl (data->dmabuf_fd[i], F_DUPFD_CLOEXEC, 3);

      if (copy->dmabuf_fd[i] == -1)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       "Failed to duplicate DMA-BUF fd: %s",
                       g_strerror (errno));
          mks_dmabuf_scanout_data_free (copy);
          return NULL;
        }
    }

  return copy;
}

gboolean
mks_dmabuf_paintable_import (MksDmabufPaintable    *self,
                             GdkDisplay            *display,
                             MksDmabufScanoutData  *data,
                             GError               **error)
{
  g_autoptr(MksDmabufScanoutData) builder_data = NULL;
  cairo_region_t *update_region;
  g_autoptr(MksTraceScope) trace_scope = NULL;
  guint i;

  g_return_val_if_fail (MKS_IS_DMABUF_PAINTABLE (self), FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (data->n_planes == 0 || data->n_planes > MKS_DMABUF_MAX_PLANES)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid number of DMA-BUF planes (%u)",
                    data->n_planes);
      return FALSE;
    }

  if (data->width == 0 || data->height == 0 ||
      data->backing_width == 0 || data->backing_height == 0)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid width/height/backing-width/backing-height (%u/%u/%u/%u)",
                    data->width, data->height, data->backing_width, data->backing_height);
      return FALSE;
    }

  for (i = 0; i < data->n_planes; i++)
    {
      if (data->dmabuf_fd[i] < 0 || data->stride[i] == 0)
        {
          g_set_error  (error,
                        G_IO_ERROR,
                        G_IO_ERROR_INVALID_ARGUMENT,
                        "invalid DMA-BUF plane %u fd/stride (%d/%u)",
                        i, data->dmabuf_fd[i], data->stride[i]);
          return FALSE;
        }
    }

  if (!(builder_data = mks_dmabuf_scanout_data_copy_fds (data, error)))
    return FALSE;

  trace_scope =
    mks_trace_scope_new ("dmabuf.import",
                         "width=%u height=%u backing_width=%u backing_height=%u "
                         "fourcc=0x%x modifier=%" G_GUINT64_FORMAT,
                         data->width,
                         data->height,
                         data->backing_width,
                         data->backing_height,
                         data->fourcc,
                         data->modifier);

  if (self->width != data->width || self->height != data->height)
    {
      self->width = data->width;
      self->height = data->height;
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
    }

  self->x = data->x;
  self->y = data->y;
  self->backing_width = data->backing_width;
  self->backing_height = data->backing_height;

  update_region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
    0,
    0,
    data->backing_width,
    data->backing_height
  });

  g_clear_object (&self->builder);
  g_clear_pointer (&self->builder_data, mks_dmabuf_scanout_data_free);
  self->builder_data = g_steal_pointer (&builder_data);

  self->builder = gdk_dmabuf_texture_builder_new ();
  gdk_dmabuf_texture_builder_set_modifier (self->builder, data->modifier);
  gdk_dmabuf_texture_builder_set_fourcc (self->builder, data->fourcc);
  gdk_dmabuf_texture_builder_set_width (self->builder, data->backing_width);
  gdk_dmabuf_texture_builder_set_height (self->builder, data->backing_height);
  gdk_dmabuf_texture_builder_set_display (self->builder, display);
  gdk_dmabuf_texture_builder_set_n_planes (self->builder, data->n_planes);

  for (i = 0; i < data->n_planes; i++)
    {
      gdk_dmabuf_texture_builder_set_fd (self->builder,
                                         i,
                                         self->builder_data->dmabuf_fd[i]);
      gdk_dmabuf_texture_builder_set_offset (self->builder, i, data->offset[i]);
      gdk_dmabuf_texture_builder_set_stride (self->builder, i, data->stride[i]);
    }

  gdk_dmabuf_texture_builder_set_update_region (self->builder, update_region);

  g_clear_pointer (&update_region, cairo_region_destroy);
  self->dmabuf_updated = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  return TRUE;
}


MksDmabufPaintable *
mks_dmabuf_paintable_new (void)
{
  g_autoptr(MksDmabufPaintable) self = NULL;

  self = g_object_new (MKS_TYPE_DMABUF_PAINTABLE, NULL);
  self->dmabuf_updated = FALSE;
  self->x = 0;
  self->y = 0;
  self->width = 0;
  self->height = 0;
  self->backing_width = 0;
  self->backing_height = 0;

  return g_steal_pointer (&self);
}
