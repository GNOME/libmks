/*
 * mks-paintable-listener.c
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

#include "mks-paintable-listener-private.h"

struct _MksPaintableListener
{
  MksQemuListenerSkeleton parent_instance;
};

static gboolean
mks_paintable_listener_update_dmabuf (MksQemuListener       *listener,
                                      GDBusMethodInvocation *invocation,
                                      int                    x,
                                      int                    y,
                                      int                    width,
                                      int                    height)
{
  return FALSE;
}

static gboolean
mks_paintable_listener_scanout_dmabuf (MksQemuListener       *listener,
                                       GDBusMethodInvocation *invocation,
                                       GUnixFDList           *unix_fd_list,
                                       GVariant              *dmabuf,
                                       guint                  width,
                                       guint                  height,
                                       guint                  stride,
                                       guint                  fourcc,
                                       guint64                modifier,
                                       gboolean               y0_top)
{
  return FALSE;
}

static gboolean
mks_paintable_listener_update (MksQemuListener       *listener,
                               GDBusMethodInvocation *invocation,
                               int                    x,
                               int                    y,
                               int                    width,
                               int                    height,
                               guint                  stride,
                               guint                  pixman_format,
                               GVariant              *bytes)
{
  return TRUE;
}

static gboolean
mks_paintable_listener_scanout (MksQemuListener       *listener,
                                GDBusMethodInvocation *invocation,
                                guint                  width,
                                guint                  height,
                                guint                  stride,
                                guint                  pixman_format,
                                GVariant              *bytes)
{
  return TRUE;
}

static gboolean
mks_paintable_listener_cursor_define (MksQemuListener       *listener,
                                      GDBusMethodInvocation *invocation,
                                      int                    width,
                                      int                    height,
                                      int                    hot_x,
                                      int                    hot_y,
                                      GVariant              *bytes)
{
  return FALSE;
}

static gboolean
mks_paintable_listener_mouse_set (MksQemuListener       *listener,
                                  GDBusMethodInvocation *invocation,
                                  int                    x,
                                  int                    y,
                                  int                    on)
{
  return FALSE;
}

static gboolean
mks_paintable_listener_disable  (MksQemuListener       *listener,
                                 GDBusMethodInvocation *invocation)
{
  return TRUE;
}

static void
listener_iface_init (MksQemuListenerIface *iface)
{
  iface->handle_update = mks_paintable_listener_update;
  iface->handle_scanout = mks_paintable_listener_scanout;
  iface->handle_update_dmabuf = mks_paintable_listener_update_dmabuf;
  iface->handle_scanout_dmabuf = mks_paintable_listener_scanout_dmabuf;
  iface->handle_cursor_define = mks_paintable_listener_cursor_define;
  iface->handle_mouse_set = mks_paintable_listener_mouse_set;
  iface->handle_disable = mks_paintable_listener_disable;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksPaintableListener, mks_paintable_listener, MKS_QEMU_TYPE_LISTENER_SKELETON,
                               G_IMPLEMENT_INTERFACE (MKS_QEMU_TYPE_LISTENER, listener_iface_init))

MksPaintableListener *
mks_paintable_listener_new (void)
{
  return g_object_new (MKS_TYPE_PAINTABLE_LISTENER, NULL);
}

static void
mks_paintable_listener_finalize (GObject *object)
{
  G_OBJECT_CLASS (mks_paintable_listener_parent_class)->finalize (object);
}

static void
mks_paintable_listener_class_init (MksPaintableListenerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mks_paintable_listener_finalize;
}

static void
mks_paintable_listener_init (MksPaintableListener *self)
{
}
