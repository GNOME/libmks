/* mks-trace-private.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _MksTraceScope MksTraceScope;

void           mks_trace_init        (void);
gint64         mks_trace_now         (void);
MksTraceScope *mks_trace_scope_new   (const char    *name,
                                      const char    *message_format,
                                      ...) G_GNUC_PRINTF (2, 3);
void           mks_trace_scope_free  (MksTraceScope *scope);
void           mks_trace_mark_printf (gint64         start_time,
                                      gint64         duration,
                                      const char    *name,
                                      const char    *message_format,
                                      ...) G_GNUC_PRINTF (4, 5);
void           mks_trace_log_printf  (int            severity,
                                      const char    *domain,
                                      const char    *message_format,
                                      ...) G_GNUC_PRINTF (3, 4);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksTraceScope, mks_trace_scope_free)

#define MKS_TRACE_SCOPE(name, ...) \
  g_autoptr(MksTraceScope) G_PASTE (__mks_trace_scope_, __LINE__) = \
    mks_trace_scope_new ((name), __VA_ARGS__)

#define MKS_TRACE_BEGIN_MARK() \
  (mks_trace_now ())

#define MKS_TRACE_END_MARK(start_time, name, ...) \
  mks_trace_mark_printf ((start_time), mks_trace_now () - (start_time), (name), __VA_ARGS__)

#define MKS_TRACE_MARK(name, ...) \
  mks_trace_mark_printf (mks_trace_now (), 0, (name), __VA_ARGS__)

#define MKS_TRACE_LOG(severity, domain, ...) \
  mks_trace_log_printf ((severity), (domain), __VA_ARGS__)

G_END_DECLS
