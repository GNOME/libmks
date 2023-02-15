/* mks-dmabuf-paintable.c
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

#define G_LOG_DOMAIN "mks-dmabuf-paintable"

#include "config.h"

#include <gtk/gtk.h>

#include "mks-dmabuf-paintable-private.h"
#include "mks-gl-context-private.h"

/*
 * MksDmabufPaintable is a GdkPaintable for a single dmabuf_fd which is then
 * imported into OpenGL. This has an advantage over just using a single
 * GdkGLTexture for such a situation in that we can take advantage of how
 * GskGLRenderer and GdkTexture work.
 *
 * First, by swapping between 2 GdkGLTextures, gsk_render_node_diff()
 * will see a pointer difference and ensure the tile region is damaged.
 * Since it is a dmabuf, we can already assume the contents are available
 * to the GPU by layers beneath us.
 */

#define TILE_WIDTH  128
#define TILE_HEIGHT 128

struct _MksDmabufPaintable
{
  GObject parent_instance;
  GdkTexture *textures[2];
  GArray *tiles;
  guint width;
  guint height;
};

typedef struct _MksDmabufTextureData
{
  GdkGLContext *gl_context;
  GLuint texture_id;
} MksDmabufTextureData;

typedef struct _MksDmabufTile
{
  graphene_rect_t area;
  guint16 texture : 1;
} MksDmabufTile;

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

  gtk_snapshot_save (snapshot);
  gtk_snapshot_scale (snapshot,
                      width / (double)self->width,
                      height / (double)self->height);

  area = GRAPHENE_RECT_INIT (0, 0, self->width, self->height);

  for (guint i = 0; i < self->tiles->len; i++)
    {
      MksDmabufTile *tile = &g_array_index (self->tiles, MksDmabufTile, i);
      GdkTexture *texture = self->textures[tile->texture];

      gtk_snapshot_push_clip (snapshot, &tile->area);
      gtk_snapshot_append_texture (snapshot, texture, &area);
      gtk_snapshot_pop (snapshot);
    }

  gtk_snapshot_restore (snapshot);
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

  g_clear_pointer (&self->tiles, g_array_unref);
  g_clear_object (&self->textures[0]);
  g_clear_object (&self->textures[1]);

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
  self->tiles = g_array_new (FALSE, FALSE, sizeof (MksDmabufTile));
}

MksDmabufPaintable *
mks_dmabuf_paintable_new (GdkGLContext  *gl_context,
                          int            dmabuf_fd,
                          guint          width,
                          guint          height,
                          guint          stride,
                          guint          fourcc,
                          guint64        modifier,
                          gboolean       y0_top,
                          GError       **error)
{
  g_autoptr(MksDmabufTextureData) texture_data = NULL;
  g_autoptr(MksDmabufPaintable) self = NULL;
  GLuint texture_id;
  guint zero = 0;

  if (dmabuf_fd < 0)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid dmabuf_fd (%d)",
                    dmabuf_fd);
      return NULL;
    }

  if (width == 0 || height == 0 || stride == 0)
    {
      g_set_error  (error,
                    G_IO_ERROR,
                    G_IO_ERROR_INVALID_ARGUMENT,
                    "invalid width/height/stride (%u/%u/%u)",
                    width, height, stride);
      return NULL;
    }

  self = g_object_new (MKS_TYPE_DMABUF_PAINTABLE, NULL);
  self->width = width;
  self->height = height;

  if (!(texture_id = mks_gl_context_import_dmabuf (gl_context,
                                                   fourcc, width, height,
                                                   1, &dmabuf_fd, &stride, &zero, &modifier)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to import dmabuf into GL texture");
      return NULL;
    }

  texture_data = mks_dmabuf_texture_data_new (gl_context, texture_id);

  self->textures[0] = gdk_gl_texture_new (gl_context, texture_id, width, height,
                                          (GDestroyNotify) mks_dmabuf_texture_data_unref,
                                          mks_dmabuf_texture_data_ref (texture_data));
  self->textures[1] = gdk_gl_texture_new (gl_context, texture_id, width, height,
                                          (GDestroyNotify) mks_dmabuf_texture_data_unref,
                                          mks_dmabuf_texture_data_ref (texture_data));

  for (guint y = 0; y < height; y += TILE_HEIGHT)
    {
      guint tile_height = MIN (TILE_HEIGHT, height - y);

      for (guint x = 0; x < width; x += TILE_WIDTH)
        {
          MksDmabufTile tile;
          guint tile_width = MIN (TILE_WIDTH, width - x);

          tile.area = GRAPHENE_RECT_INIT (x, y, tile_width, tile_height);
          tile.texture = 0;

          g_array_append_val (self->tiles, tile);
        }
    }

  return g_steal_pointer (&self);
}

void
mks_dmabuf_paintable_invalidate (MksDmabufPaintable *self,
                                 guint               x,
                                 guint               y,
                                 guint               width,
                                 guint               height)
{
  graphene_rect_t area;

  g_return_if_fail (MKS_IS_DMABUF_PAINTABLE (self));

  if (width == 0 || height == 0)
    return;

  area = GRAPHENE_RECT_INIT (x, y, width, height);

  for (guint i = 0; i < self->tiles->len; i++)
    {
      MksDmabufTile *tile = &g_array_index (self->tiles, MksDmabufTile, i);
      G_GNUC_UNUSED graphene_rect_t res;

      if (graphene_rect_intersection (&area, &tile->area, &res))
        tile->texture = !tile->texture;
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}
