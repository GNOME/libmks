/*
 * mks-dmabuf-paintable.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "mks-dmabuf-paintable-private.h"
#include "mks-gl-context-private.h"

/*
 * MksDmabufPaintable is a GdkPaintable that gets created the first time
 * `ScanoutDMABUF` is called.
 *
 * The scanout data is then stored until we receive a `UpdateDMABUF` call
 * so we can pass the damage region to `GdkGLTextureBuilder`.
 */

struct _MksDmabufPaintable
{
  GObject parent_instance;
  GdkTexture *texture;
  guint width;
  guint height;
};

typedef struct _MksDmabufTextureData
{
  GdkGLContext *gl_context;
  GLuint texture_id;
} MksDmabufTextureData;

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

  return (double)self->width / (double)self->height;
}

static void
mks_dmabuf_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  MksDmabufPaintable *self = (MksDmabufPaintable *)paintable;
  graphene_rect_t area;

  g_assert (MKS_IS_DMABUF_PAINTABLE (self));
  g_assert (GDK_IS_SNAPSHOT (snapshot));

  area = GRAPHENE_RECT_INIT (0, 0, width, height);
  gtk_snapshot_append_texture (snapshot, self->texture, &area);
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

static MksDmabufTextureData *
mks_dmabuf_texture_data_new (GdkGLContext *gl_context,
                             GLuint        texture_id)
{
  MksDmabufTextureData *texture_data;

  g_assert (GDK_IS_GL_CONTEXT (gl_context));
  g_assert (texture_id > 0);

  texture_data = g_atomic_rc_box_new0 (MksDmabufTextureData);
  texture_data->gl_context = g_object_ref (gl_context);
  texture_data->texture_id = texture_id;

  return texture_data;
}

static MksDmabufTextureData *
mks_dmabuf_texture_data_ref (MksDmabufTextureData *texture_data)
{
  return g_atomic_rc_box_acquire (texture_data);
}

static void
mks_dmabuf_texture_data_finalize (gpointer data)
{
  MksDmabufTextureData *texture_data = data;

  gdk_gl_context_make_current (texture_data->gl_context);
  glDeleteTextures (1, &texture_data->texture_id);

  texture_data->texture_id = 0;
  g_clear_object (&texture_data->gl_context);
}

static void
mks_dmabuf_texture_data_unref (MksDmabufTextureData *texture_data)
{
  g_atomic_rc_box_release_full (texture_data, mks_dmabuf_texture_data_finalize);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksDmabufTextureData, mks_dmabuf_texture_data_unref)

static void
mks_dmabuf_paintable_dispose (GObject *object)
{
  MksDmabufPaintable *self = (MksDmabufPaintable *)object;

  g_clear_object (&self->texture);

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

gboolean
mks_dmabuf_paintable_import (MksDmabufPaintable   *self,
                             GdkGLContext         *gl_context,
                             MksDmabufScanoutData *data,
                             cairo_region_t       *region,
                             GError              **error)
{
  g_autoptr(GdkGLTextureBuilder) builder = NULL;
  g_autoptr(GdkTexture) texture = NULL;
  GLuint texture_id;
  guint zero = 0;

  g_return_val_if_fail (MKS_IS_DMABUF_PAINTABLE (self), FALSE);
  g_return_val_if_fail (!gl_context || GDK_IS_GL_CONTEXT (gl_context), FALSE);

  if (data->dmabuf_fd < 0)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid dmabuf_fd (%d)",
                    data->dmabuf_fd);
      return FALSE;
    }

  if (data->width == 0 || data->height == 0 || data->stride == 0)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid width/height/stride (%u/%u/%u)",
                    data->width, data->height, data->stride);
      return FALSE;
    }

  if (self->width != data->width || self->height != data->height)
    {
      self->width = data->width;
      self->height = data->height;
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
    }

  if (!(texture_id = mks_gl_context_import_dmabuf (gl_context,
                                                   data->fourcc, data->width, data->height,
                                                   1, &data->dmabuf_fd, &data->stride, &zero,
                                                   &data->modifier)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to import dmabuf into GL texture");
      return FALSE;
    }

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_id (builder, texture_id);
  gdk_gl_texture_builder_set_width (builder, self->width);
  gdk_gl_texture_builder_set_height (builder, self->height);
  gdk_gl_texture_builder_set_context (builder, gl_context);

  if (region != NULL)
    {
      gdk_gl_texture_builder_set_update_region (builder, region);
      gdk_gl_texture_builder_set_update_texture (builder, self->texture);
    }

  texture = gdk_gl_texture_builder_build (builder,
                                          (GDestroyNotify)mks_dmabuf_texture_data_unref,
                                          mks_dmabuf_texture_data_new (gl_context, texture_id));

  g_set_object (&self->texture, texture);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return TRUE;
}


MksDmabufPaintable *
mks_dmabuf_paintable_new (void)
{
  g_autoptr(MksDmabufPaintable) self = NULL;

  self = g_object_new (MKS_TYPE_DMABUF_PAINTABLE, NULL);
  self->width = 0;
  self->height = 0;
  return g_steal_pointer (&self);
}
