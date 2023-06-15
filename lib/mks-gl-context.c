/*
 * mks-gl-context.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
# include <gdk/wayland/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkx.h>
#endif

#include "mks-gl-context-private.h"

GLuint
mks_gl_context_import_dmabuf (GdkGLContext  *context,
                              guint32        format,
                              guint          width,
                              guint          height,
                              guint32        n_planes,
                              const int     *fds,
                              const guint32 *strides,
                              const guint32 *offsets,
                              const guint64 *modifiers)
{
  GdkDisplay *display;
  EGLDisplay egl_display;
  EGLint attribs[2 * (3 + 4 * 5) + 1];
  EGLImage image;
  guint texture_id;
  int i;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), 0);
  g_return_val_if_fail (0 < n_planes && n_planes <= 4, 0);

  display = gdk_gl_context_get_display (context);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    egl_display = gdk_wayland_display_get_egl_display (display);
  else
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    egl_display = gdk_x11_display_get_egl_display (display);
  else
#endif
    egl_display = NULL;

  if (egl_display == NULL)
    {
      g_warning ("Can't import dmabufs when not using EGL");
      return 0;
    }

  i = 0;
  attribs[i++] = EGL_WIDTH;
  attribs[i++] = width;
  attribs[i++] = EGL_HEIGHT;
  attribs[i++] = height;
  attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
  attribs[i++] = format;

  if (n_planes > 0)
    {
      attribs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
      attribs[i++] = fds[0];
      attribs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      attribs[i++] = offsets[0];
      attribs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      attribs[i++] = strides[0];
      if (modifiers)
        {
          attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
          attribs[i++] = modifiers[0] & 0xFFFFFFFF;
          attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
          attribs[i++] = modifiers[0] >> 32;
        }
    }

  if (n_planes > 1)
    {
      attribs[i++] = EGL_DMA_BUF_PLANE1_FD_EXT;
      attribs[i++] = fds[1];
      attribs[i++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
      attribs[i++] = offsets[1];
      attribs[i++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
      attribs[i++] = strides[1];
      if (modifiers)
        {
          attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
          attribs[i++] = modifiers[1] & 0xFFFFFFFF;
          attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
          attribs[i++] = modifiers[1] >> 32;
        }
    }

  if (n_planes > 2)
    {
      attribs[i++] = EGL_DMA_BUF_PLANE2_FD_EXT;
      attribs[i++] = fds[2];
      attribs[i++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
      attribs[i++] = offsets[2];
      attribs[i++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
      attribs[i++] = strides[2];
      if (modifiers)
        {
          attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
          attribs[i++] = modifiers[2] & 0xFFFFFFFF;
          attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
          attribs[i++] = modifiers[2] >> 32;
        }
    }
  if (n_planes > 3)
    {
      attribs[i++] = EGL_DMA_BUF_PLANE3_FD_EXT;
      attribs[i++] = fds[3];
      attribs[i++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
      attribs[i++] = offsets[3];
      attribs[i++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
      attribs[i++] = strides[3];
      if (modifiers)
        {
          attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
          attribs[i++] = modifiers[3] & 0xFFFFFFFF;
          attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
          attribs[i++] = modifiers[3] >> 32;
        }
    }

  attribs[i++] = EGL_NONE;

  image = eglCreateImageKHR (egl_display,
                             EGL_NO_CONTEXT,
                             EGL_LINUX_DMA_BUF_EXT,
                             (EGLClientBuffer)NULL,
                             attribs);
  if (image == EGL_NO_IMAGE)
    {
      g_warning ("Failed to create EGL image: %d\n", eglGetError ());
      return 0;
    }

  gdk_gl_context_make_current (context);

  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glEGLImageTargetTexture2DOES (GL_TEXTURE_2D, image);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  eglDestroyImageKHR (egl_display, image);

  return texture_id;
}
