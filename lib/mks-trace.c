/*
 * mks-trace.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <stdarg.h>

#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
# include <sysprof-collector.h>
# include <sysprof-capture-types.h>
#endif

#include "mks-trace-private.h"

#define MKS_TRACE_DOMAIN "Mks"
#define MKS_TRACE_GROUP "LibMKS"

struct _MksTraceScope
{
  gint64 start_time;
  char *name;
  char *message;
};

#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
static gsize trace_initialized;
static gboolean trace_active;
#endif

static gboolean
mks_trace_ensure_active (void)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  sysprof_collector_init ();

  if (!sysprof_collector_is_active ())
    return FALSE;

  if (g_once_init_enter (&trace_initialized))
    {
      trace_active = TRUE;
      g_once_init_leave (&trace_initialized, 1);
    }

  return trace_active;
#else
  return FALSE;
#endif
}

gint64
mks_trace_now (void)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  return SYSPROF_CAPTURE_CURRENT_TIME;
#else
  return g_get_monotonic_time ();
#endif
}

void
mks_trace_init (void)
{
  (void) mks_trace_ensure_active ();
}

MksTraceScope *
mks_trace_scope_new (const char *name,
                     const char *message_format,
                     ...)
{
  MksTraceScope *scope;

  g_return_val_if_fail (name != NULL, NULL);

  if (!mks_trace_ensure_active ())
    return NULL;

  scope = g_new0 (MksTraceScope, 1);
  scope->start_time = mks_trace_now ();
  scope->name = g_strdup (name);

  if (message_format != NULL)
    {
      va_list args;

      va_start (args, message_format);
      scope->message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }

  return scope;
}

void
mks_trace_scope_free (MksTraceScope *scope)
{
  if (scope == NULL)
    return;

  mks_trace_mark_printf (scope->start_time,
                         mks_trace_now () - scope->start_time,
                         scope->name,
                         "%s",
                         scope->message != NULL ? scope->message : "");

  g_clear_pointer (&scope->message, g_free);
  g_clear_pointer (&scope->name, g_free);
  g_free (scope);
}

void
mks_trace_mark_printf (gint64      start_time,
                       gint64      duration,
                       const char *name,
                       const char *message_format,
                       ...)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  va_list args;
  g_autofree char *mark_name = NULL;

  if (!mks_trace_ensure_active ())
    return;

  mark_name = g_strconcat ("mks.", name, NULL);

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (start_time,
                                  duration,
                                  MKS_TRACE_GROUP,
                                  mark_name,
                                  message_format,
                                  args);
  va_end (args);
#else
  (void) start_time;
  (void) duration;
  (void) name;
  (void) message_format;
#endif
}

void
mks_trace_log_printf (int         severity,
                      const char *domain,
                      const char *message_format,
                      ...)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  va_list args;
  g_autofree char *message = NULL;

  if (!mks_trace_ensure_active ())
    return;

  va_start (args, message_format);
  message = g_strdup_vprintf (message_format, args);
  va_end (args);

  sysprof_collector_log (severity, domain != NULL ? domain : MKS_TRACE_DOMAIN, message);
#else
  (void) severity;
  (void) domain;
  (void) message_format;
#endif
}
