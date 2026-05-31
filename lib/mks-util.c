/*
 * mks-util.c
 *
 * Copyright 2023 Christian Hergert <christian@sourceandstack.com>
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

#include <glib/gstdio.h>
#include <sys/socket.h>

#include "mks-util-private.h"

static GSettings *mouse_settings;
static GSettings *touchpad_settings;
static gsize initialized;

typedef struct
{
  gint64 begin_time;
  const char *message;
} MksMarkedFuture;

typedef struct
{
  const char *log_domain;
  GLogLevelFlags level;
  char *message_prefix;
} MksLoggedFuture;

static void
mks_marked_future_free (MksMarkedFuture *state)
{
  g_free (state);
}

static void
mks_logged_future_free (MksLoggedFuture *state)
{
  g_clear_pointer (&state->message_prefix, g_free);
  g_free (state);
}

static GSettings *
load_gsettings (const char *schema_id)
{
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  g_autoptr(GSettingsSchema) schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  if (schema != NULL)
    return g_settings_new (schema_id);

  return NULL;
}

static void
_mks_util_init (void)
{
  if (g_once_init_enter (&initialized))
    {
      mouse_settings = load_gsettings ("org.gnome.desktop.peripherals.mouse");
      touchpad_settings = load_gsettings ("org.gnome.desktop.peripherals.touchpad");
      g_once_init_leave (&initialized, TRUE);
    }
}

/* This is abstracted in a way that as soon as GdkEvent contains enough
 * information to know if the GdkScrollEvent contains inverted axis
 * directoin we can use that instead of checking the GSetting.
 *
 * TODO: This won't handle Flatpak because we won't have access to the
 * host setting for the GSetting. Additionally, it won't work with jhbuild
 * for the same reasons (likely using alternate GSettings/dconf).
 *
 * But this is better than nothing for the time being and provides an
 * abstraction point once support for wayland!183 lands.
 */
gboolean
mks_scroll_event_is_inverted (GdkEvent *event)
{
  GdkScrollUnit unit;

  g_return_val_if_fail (gdk_event_get_event_type (event) == GDK_SCROLL, FALSE);

  _mks_util_init ();

  if (mouse_settings == NULL || touchpad_settings == NULL)
    return FALSE;

  unit = gdk_scroll_event_get_unit (event);

  switch (unit)
    {
    case GDK_SCROLL_UNIT_WHEEL:
      return g_settings_get_boolean (mouse_settings, "natural-scroll");

    case GDK_SCROLL_UNIT_SURFACE:
      return g_settings_get_boolean (touchpad_settings, "natural-scroll");

    default:
      return FALSE;
    }
}

gboolean
mks_socketpair_create (int     *us,
                       int     *them,
                       GError **error)
{
  int fds[2];
  int rv;

  rv = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, fds);

  if (rv != 0)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  *us = fds[0];
  *them = fds[1];

  return TRUE;
}

G_DEFINE_BOXED_TYPE (MksSocketpairConnection,
                     mks_socketpair_connection,
                     mks_socketpair_connection_ref,
                     mks_socketpair_connection_unref)

MksSocketpairConnection *
mks_socketpair_connection_ref (MksSocketpairConnection *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
mks_socketpair_connection_unref (MksSocketpairConnection *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_clear_object (&self->connection);
      if (self->peer_fd != -1)
        close (self->peer_fd);
      g_free (self);
    }
}

int
mks_socketpair_connection_steal_fd (MksSocketpairConnection *self)
{
  g_return_val_if_fail (self != NULL, -1);

  return g_steal_fd (&self->peer_fd);
}

void
mks_future_to_async_result (gpointer             source_object,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data,
                            const char          *static_name,
                            DexFuture           *future)
{
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (!source_object || G_IS_OBJECT (source_object));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (DEX_IS_FUTURE (future));

  result = dex_async_result_new (source_object, cancellable, callback, user_data);
  dex_async_result_set_static_name (result, static_name);
  dex_async_result_await (result, future);
}

static DexFuture *
mks_marked_future_cb (DexFuture *future,
                      gpointer   user_data)
{
  MksMarkedFuture *state = user_data;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);
  g_assert (state->message != NULL);

  MKS_TRACE_END_MARK (state->begin_time, "future", "%s", state->message);

  return NULL;
}

DexFuture *
mks_marked_future (DexFuture  *future,
                   gint64      begin_time,
                   const char *message)
{
  MksMarkedFuture *state;

  dex_return_error_if_fail (DEX_IS_FUTURE (future));
  dex_return_error_if_fail (message != NULL);

  state = g_new0 (MksMarkedFuture, 1);
  state->begin_time = begin_time;
  state->message = g_intern_string (message);

  return dex_future_finally (future,
                             mks_marked_future_cb,
                             state,
                             (GDestroyNotify) mks_marked_future_free);
}

static DexFuture *
mks_logged_future_cb (DexFuture *future,
                      gpointer   user_data)
{
  MksLoggedFuture *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);

  if (!dex_future_get_value (future, &error))
    {
      g_log (state->log_domain,
             state->level,
             "%s: %s",
             state->message_prefix,
             error->message);
      MKS_TRACE_LOG (state->level,
                     state->log_domain,
                     "%s: %s",
                     state->message_prefix,
                     error->message);
    }

  return NULL;
}

DexFuture *
mks_logged_future (DexFuture      *future,
                   const char     *log_domain,
                   GLogLevelFlags  level,
                   const char     *message_prefix)
{
  MksLoggedFuture *state;

  dex_return_error_if_fail (DEX_IS_FUTURE (future));
  dex_return_error_if_fail (message_prefix != NULL);

  state = g_new0 (MksLoggedFuture, 1);
  state->log_domain = log_domain != NULL ? g_intern_string (log_domain) : NULL;
  state->level = level;
  state->message_prefix = g_strdup (message_prefix);

  return dex_future_catch (future,
                           mks_logged_future_cb,
                           state,
                           (GDestroyNotify) mks_logged_future_free);
}

static void
mks_dbus_connection_new_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DexPromise *promise = user_data;
  g_autoptr(GDBusConnection) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(ret = g_dbus_connection_new_finish (result, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&ret));

  dex_unref (promise);
}

DexFuture *
mks_dbus_connection_new (GIOStream            *stream,
                         GDBusConnectionFlags  flags,
                         GCancellable         *cancellable)
{
  DexPromise *promise;

  dex_return_error_if_fail (G_IS_IO_STREAM (stream));
  dex_return_error_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  promise = dex_promise_new_cancellable ();

  g_dbus_connection_new (stream,
                         NULL,
                         flags,
                         NULL,
                         cancellable ? cancellable : dex_promise_get_cancellable (promise),
                         mks_dbus_connection_new_cb,
                         dex_ref (promise));

  return DEX_FUTURE (promise);
}

static DexFuture *
mks_socketpair_connection_complete (DexFuture *future,
                                    gpointer   user_data)
{
  MksSocketpairConnection *state = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);

  if (!(value = dex_future_get_value (future, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state->connection = g_value_dup_object (value);

  {
    GValue boxed = G_VALUE_INIT;
    DexFuture *ret;

    g_value_init (&boxed, MKS_TYPE_SOCKETPAIR_CONNECTION);
    g_value_set_boxed (&boxed, state);
    ret = dex_future_new_for_value (&boxed);
    g_value_unset (&boxed);

    return ret;
  }
}

DexFuture *
mks_socketpair_connection_new (GDBusConnectionFlags flags)
{
  MksSocketpairConnection *state;
  g_autoptr(GSocketConnection) io_stream = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_autoptr(GError) error = NULL;
  g_autofd int us = -1;
  g_autofd int them = -1;

  if (!mks_socketpair_create (&us, &them, &error) ||
      !(socket = g_socket_new_from_fd (us, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  io_stream = g_socket_connection_factory_create_connection (socket);
  state = g_new0 (MksSocketpairConnection, 1);
  state->ref_count = 1;
  state->peer_fd = g_steal_fd (&them);

  us = -1;

  return dex_future_then (mks_dbus_connection_new (G_IO_STREAM (io_stream),
                                                   flags | G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                                   NULL),
                          mks_socketpair_connection_complete,
                          state,
                          (GDestroyNotify) mks_socketpair_connection_unref);
}
