/*
 * mks-dmabuf-texture.c
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

#include "mks-dmabuf-texture-private.h"

GdkTexture *
mks_dmabuf_texture_new (int        dmabuf_fd,
                        guint      width,
                        guint      height,
                        guint      stride,
                        guint      fourcc,
                        guint64    modifier,
                        gboolean   y0_top,
                        GError   **error)
{
  if G_UNLIKELY (dmabuf_fd < 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Invalid FD for DMA-BUF");
      return NULL;
    }

  if G_UNLIKELY (width == 0 || height == 0 || stride == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Invalid width/height/stride");
      return NULL;
    }

  /* TODO: use dmabuf importing code here */

  return NULL;
}
