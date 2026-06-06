/* mks-util-private.h
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

#pragma once

#include <cairo.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <libdex.h>

#include "mks-trace-private.h"

G_BEGIN_DECLS

#define MKS_TYPE_SOCKETPAIR_CONNECTION (mks_socketpair_connection_get_type())

typedef struct _MksSocketpairConnection MksSocketpairConnection;

struct _MksSocketpairConnection
{
  int              ref_count;
  GDBusConnection *connection;
  int              peer_fd;
};

#define _CAIRO_CHECK_VERSION(major, minor, micro) \
  (CAIRO_VERSION_MAJOR > (major) || \
   (CAIRO_VERSION_MAJOR == (major) && CAIRO_VERSION_MINOR > (minor)) || \
   (CAIRO_VERSION_MAJOR == (major) && CAIRO_VERSION_MINOR == (minor) && \
    CAIRO_VERSION_MICRO >= (micro)))

gboolean                 mks_socketpair_create              (int                      *us,
                                                             int                      *them,
                                                             GError                  **error);
gboolean                 mks_scroll_event_is_inverted       (GdkEvent                 *event);
GType                    mks_socketpair_connection_get_type (void) G_GNUC_CONST;
MksSocketpairConnection *mks_socketpair_connection_ref      (MksSocketpairConnection  *self);
void                     mks_socketpair_connection_unref    (MksSocketpairConnection  *self);
int                      mks_socketpair_connection_steal_fd (MksSocketpairConnection  *self);
DexFuture               *mks_dbus_connection_new            (GIOStream                *stream,
                                                             GDBusConnectionFlags      flags,
                                                             GCancellable             *cancellable);
void                     mks_future_to_async_result         (gpointer                  source_object,
                                                             GCancellable             *cancellable,
                                                             GAsyncReadyCallback       callback,
                                                             gpointer                  user_data,
                                                             const char               *static_name,
                                                             DexFuture                *future);
DexFuture               *mks_socketpair_connection_new      (GDBusConnectionFlags      flags);
DexFuture               *mks_marked_future                  (DexFuture                *future,
                                                             gint64                    begin_time,
                                                             const char               *message) G_GNUC_WARN_UNUSED_RESULT;
DexFuture               *mks_logged_future                  (DexFuture                *future,
                                                             const char               *log_domain,
                                                             GLogLevelFlags            level,
                                                             const char               *message_prefix) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
