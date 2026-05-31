/*
 * mks-chardev-pty.c
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

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include <glib-unix.h>
#include <glib/gstdio.h>

#include "mks-chardev.h"
#include "mks-util-private.h"

#define BUFFER_SIZE (16 * 1024)
#define MKS_TYPE_CHARDEV_PTY (mks_chardev_pty_get_type ())
#define MKS_CHARDEV_PTY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_CHARDEV_PTY, MksChardevPty))
#define MKS_IS_CHARDEV_PTY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKS_TYPE_CHARDEV_PTY))

typedef struct _MksChardevPty MksChardevPty;
typedef struct _MksChardevPtyClass MksChardevPtyClass;

GType mks_chardev_pty_get_type (void) G_GNUC_CONST;
G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksChardevPty, g_object_unref)

struct _MksChardevPtyClass
{
  GObjectClass parent_class;
};

struct _MksChardevPty
{
  GObject parent_instance;

  GMainContext *context;
  GIOChannel   *socket_channel;
  GIOChannel   *slave_channel;
  GBytes       *to_socket;
  GBytes       *to_slave;

  guint         socket_read_id;
  guint         socket_write_id;
  guint         slave_read_id;
  guint         slave_write_id;

  int           master_fd;
  int           slave_fd;
  int           socket_fd;
};

G_DEFINE_FINAL_TYPE (MksChardevPty, mks_chardev_pty, G_TYPE_OBJECT)

static void mks_chardev_pty_start_socket_read (MksChardevPty *self);
static void mks_chardev_pty_start_slave_read  (MksChardevPty *self);
static void mks_chardev_pty_close             (MksChardevPty *self);
static int  mks_chardev_pty_dup_fd            (MksChardevPty *self,
                                               GError       **error);
static gboolean socket_write_cb               (GIOChannel    *channel,
                                               GIOCondition   condition,
                                               gpointer       user_data);
static gboolean slave_write_cb                (GIOChannel    *channel,
                                               GIOCondition   condition,
                                               gpointer       user_data);

static void
mks_chardev_pty_setup_channels (MksChardevPty *self)
{
  g_assert (MKS_IS_CHARDEV_PTY (self));
  g_assert (self->socket_fd > -1);
  g_assert (self->slave_fd > -1);

  self->socket_channel = g_io_channel_unix_new (self->socket_fd);
  self->slave_channel = g_io_channel_unix_new (self->slave_fd);
  g_io_channel_set_close_on_unref (self->socket_channel, FALSE);
  g_io_channel_set_close_on_unref (self->slave_channel, FALSE);
  g_io_channel_set_encoding (self->socket_channel, NULL, NULL);
  g_io_channel_set_encoding (self->slave_channel, NULL, NULL);
  g_io_channel_set_buffered (self->socket_channel, FALSE);
  g_io_channel_set_buffered (self->slave_channel, FALSE);

  mks_chardev_pty_start_socket_read (self);
  mks_chardev_pty_start_slave_read (self);
}

typedef struct _MksChardevPtyCreate
{
  MksChardevPty *self;
} MksChardevPtyCreate;

static void
mks_chardev_pty_create_free (MksChardevPtyCreate *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
mks_chardev_pty_create_complete (DexFuture *future,
                                 gpointer   user_data)
{
  MksChardevPtyCreate *state = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(VtePty) pty = NULL;
  int fd;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);
  g_assert (MKS_IS_CHARDEV_PTY (state->self));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (-1 == (fd = mks_chardev_pty_dup_fd (state->self, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(pty = vte_pty_new_foreign_sync (fd, NULL, &error)))
    {
      close (fd);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  mks_chardev_pty_setup_channels (state->self);
  g_object_set_data_full (G_OBJECT (pty),
                          "mks-chardev-pty",
                          g_object_ref (state->self),
                          g_object_unref);

  return dex_future_new_for_object (pty);
}

static void
clear_fd (int *fd)
{
  g_assert (fd != NULL);

  if (*fd != -1)
    {
      close (*fd);
      *fd = -1;
    }
}

static gboolean
is_blocking_errno (int errnum)
{
  return errnum == EAGAIN
#if EAGAIN != EWOULDBLOCK
         || errnum == EWOULDBLOCK
#endif
         ;
}

static gboolean
set_fd_flags (int      fd,
              gboolean nonblock,
              GError **error)
{
  int flags;

  g_assert (fd > -1);

  if ((flags = fcntl (fd, F_GETFD, 0)) == -1 ||
      fcntl (fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  if (nonblock)
    {
      if ((flags = fcntl (fd, F_GETFL, 0)) == -1 ||
          fcntl (fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               g_io_error_from_errno (errno),
                               g_strerror (errno));
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
set_raw_mode (int      fd,
              GError **error)
{
  struct termios t;

  g_assert (fd > -1);

  if (tcgetattr (fd, &t) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  cfmakeraw (&t);

  if (tcsetattr (fd, TCSANOW, &t) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

static void
remove_source (guint *source_id)
{
  g_assert (source_id != NULL);

  if (*source_id != 0)
    {
      g_source_remove (*source_id);
      *source_id = 0;
    }
}

static guint
attach_watch (MksChardevPty *self,
              GIOChannel    *channel,
              GIOCondition   condition,
              GIOFunc        callback,
              int            priority)
{
  GSource *source;
  guint source_id;

  g_assert (MKS_IS_CHARDEV_PTY (self));
  g_assert (channel != NULL);
  g_assert (callback != NULL);

  source = g_io_create_watch (channel, condition);
  g_source_set_callback (source, (GSourceFunc)callback, self, NULL);
  g_source_set_priority (source, priority);
  source_id = g_source_attach (source, self->context);
  g_source_unref (source);

  return source_id;
}

static gboolean
fd_write_bytes (int      fd,
                GBytes  *bytes,
                GBytes **remaining,
                GError **error)
{
  const guint8 *begin;
  gconstpointer data;
  gsize size;
  ssize_t written;

  g_assert (fd > -1);
  g_assert (bytes != NULL);
  g_assert (remaining != NULL);
  g_assert (*remaining == NULL);

  data = g_bytes_get_data (bytes, &size);
  begin = data;

  while (size > 0)
    {
      written = write (fd, data, size);

      if (written == -1)
        {
          if (errno == EINTR)
            continue;

          if (is_blocking_errno (errno))
            {
              *remaining = g_bytes_new_from_bytes (bytes, (const guint8 *)data - begin, size);
              return TRUE;
            }

          g_set_error_literal (error,
                               G_IO_ERROR,
                               g_io_error_from_errno (errno),
                               g_strerror (errno));
          return FALSE;
        }

      if (written == 0)
        {
          *remaining = g_bytes_new_from_bytes (bytes, (const guint8 *)data - begin, size);
          return TRUE;
        }

      data = (const guint8 *)data + written;
      size -= written;
    }

  return TRUE;
}

static gboolean
fd_write_pending (int      fd,
                  GBytes **pending,
                  GError **error)
{
  g_autoptr(GBytes) bytes = NULL;
  gconstpointer data;
  gsize size;
  ssize_t written;

  g_assert (fd > -1);
  g_assert (pending != NULL);
  g_assert (*pending != NULL);

  bytes = g_steal_pointer (pending);
  data = g_bytes_get_data (bytes, &size);

  while (size > 0)
    {
      written = write (fd, data, size);

      if (written == -1)
        {
          if (errno == EINTR)
            continue;

          if (is_blocking_errno (errno))
            {
              *pending = g_bytes_new_from_bytes (bytes,
                                                 (const guint8 *)data -
                                                 (const guint8 *)g_bytes_get_data (bytes, NULL),
                                                 size);
              return TRUE;
            }

          g_set_error_literal (error,
                               G_IO_ERROR,
                               g_io_error_from_errno (errno),
                               g_strerror (errno));
          return FALSE;
        }

      if (written == 0)
        {
          *pending = g_bytes_new_from_bytes (bytes,
                                             (const guint8 *)data -
                                             (const guint8 *)g_bytes_get_data (bytes, NULL),
                                             size);
          return TRUE;
        }

      data = (const guint8 *)data + written;
      size -= written;
    }

  return TRUE;
}

static gboolean
socket_read_cb (GIOChannel   *channel,
                GIOCondition  condition,
                gpointer      user_data)
{
  MksChardevPty *self = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  guint8 buf[BUFFER_SIZE];
  ssize_t n_read;

  g_assert (MKS_IS_CHARDEV_PTY (self));

  if ((condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) != 0)
    goto close_bridge;

  n_read = read (self->socket_fd, buf, sizeof buf);

  if (n_read == -1)
    {
      if (is_blocking_errno (errno) || errno == EINTR)
        return G_SOURCE_CONTINUE;

      goto close_bridge;
    }

  if (n_read == 0)
    goto close_bridge;

  bytes = g_bytes_new (buf, n_read);

  if (!fd_write_bytes (self->slave_fd, bytes, &self->to_slave, &error))
    goto close_bridge;

  if (self->to_slave != NULL)
    {
      self->socket_read_id = 0;
      self->slave_write_id = attach_watch (self,
                                           self->slave_channel,
                                           G_IO_OUT,
                                           slave_write_cb,
                                           G_PRIORITY_DEFAULT_IDLE);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;

close_bridge:
  self->socket_read_id = 0;
  mks_chardev_pty_close (self);
  return G_SOURCE_REMOVE;
}

static gboolean
slave_read_cb (GIOChannel   *channel,
               GIOCondition  condition,
               gpointer      user_data)
{
  MksChardevPty *self = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  guint8 buf[BUFFER_SIZE];
  ssize_t n_read;

  g_assert (MKS_IS_CHARDEV_PTY (self));

  if ((condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) != 0)
    goto close_bridge;

  n_read = read (self->slave_fd, buf, sizeof buf);

  if (n_read == -1)
    {
      if (is_blocking_errno (errno) || errno == EINTR)
        return G_SOURCE_CONTINUE;

      goto close_bridge;
    }

  if (n_read == 0)
    goto close_bridge;

  bytes = g_bytes_new (buf, n_read);

  if (!fd_write_bytes (self->socket_fd, bytes, &self->to_socket, &error))
    goto close_bridge;

  if (self->to_socket != NULL)
    {
      self->slave_read_id = 0;
      self->socket_write_id = attach_watch (self,
                                            self->socket_channel,
                                            G_IO_OUT,
                                            socket_write_cb,
                                            G_PRIORITY_DEFAULT);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;

close_bridge:
  self->slave_read_id = 0;
  mks_chardev_pty_close (self);
  return G_SOURCE_REMOVE;
}

static gboolean
socket_write_cb (GIOChannel   *channel,
                 GIOCondition  condition,
                 gpointer      user_data)
{
  MksChardevPty *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_CHARDEV_PTY (self));

  if ((condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) != 0 ||
      !fd_write_pending (self->socket_fd, &self->to_socket, &error))
    {
      self->socket_write_id = 0;
      mks_chardev_pty_close (self);
      return G_SOURCE_REMOVE;
    }

  if (self->to_socket == NULL)
    {
      self->socket_write_id = 0;
      mks_chardev_pty_start_slave_read (self);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static gboolean
slave_write_cb (GIOChannel   *channel,
                GIOCondition  condition,
                gpointer      user_data)
{
  MksChardevPty *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MKS_IS_CHARDEV_PTY (self));

  if ((condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) != 0 ||
      !fd_write_pending (self->slave_fd, &self->to_slave, &error))
    {
      self->slave_write_id = 0;
      mks_chardev_pty_close (self);
      return G_SOURCE_REMOVE;
    }

  if (self->to_slave == NULL)
    {
      self->slave_write_id = 0;
      mks_chardev_pty_start_socket_read (self);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
mks_chardev_pty_start_socket_read (MksChardevPty *self)
{
  g_assert (MKS_IS_CHARDEV_PTY (self));

  if (self->socket_fd != -1 &&
      self->slave_fd != -1 &&
      self->socket_read_id == 0 &&
      self->to_slave == NULL)
    self->socket_read_id = attach_watch (self,
                                         self->socket_channel,
                                         G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                                         socket_read_cb,
                                         G_PRIORITY_DEFAULT);
}

static void
mks_chardev_pty_start_slave_read (MksChardevPty *self)
{
  g_assert (MKS_IS_CHARDEV_PTY (self));

  if (self->socket_fd != -1 &&
      self->slave_fd != -1 &&
      self->slave_read_id == 0 &&
      self->to_socket == NULL)
    self->slave_read_id = attach_watch (self,
                                        self->slave_channel,
                                        G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                                        slave_read_cb,
                                        G_PRIORITY_DEFAULT);
}

static gboolean
open_pty (int      *master_fd,
          int      *slave_fd,
          GError  **error)
{
  g_autofree char *name = NULL;
  g_autofd int master = -1;
  g_autofd int slave = -1;

  g_assert (master_fd != NULL);
  g_assert (slave_fd != NULL);

  if (-1 == (master = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC)) ||
      grantpt (master) == -1 ||
      unlockpt (master) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  if (!(name = ptsname (master)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  name = g_strdup (name);

  if (-1 == (slave = g_open (name, O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK, 0)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  if (!set_fd_flags (master, FALSE, error) ||
      !set_fd_flags (slave, TRUE, error) ||
      !set_raw_mode (slave, error))
    return FALSE;

  *master_fd = g_steal_fd (&master);
  *slave_fd = g_steal_fd (&slave);

  return TRUE;
}

static gboolean
open_socketpair (int      fds[2],
                 GError **error)
{
  g_assert (fds != NULL);

  fds[0] = -1;
  fds[1] = -1;

  if (socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, fds) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  if (!set_fd_flags (fds[0], TRUE, error) ||
      !set_fd_flags (fds[1], TRUE, error))
    {
      clear_fd (&fds[0]);
      clear_fd (&fds[1]);
      return FALSE;
    }

  return TRUE;
}

static void
mks_chardev_pty_dispose (GObject *object)
{
  MksChardevPty *self = (MksChardevPty *)object;

  mks_chardev_pty_close (self);

  G_OBJECT_CLASS (mks_chardev_pty_parent_class)->dispose (object);
}

static void
mks_chardev_pty_finalize (GObject *object)
{
  MksChardevPty *self = (MksChardevPty *)object;

  g_clear_pointer (&self->context, g_main_context_unref);

  G_OBJECT_CLASS (mks_chardev_pty_parent_class)->finalize (object);
}

static void
mks_chardev_pty_class_init (MksChardevPtyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_chardev_pty_dispose;
  object_class->finalize = mks_chardev_pty_finalize;
}

static void
mks_chardev_pty_init (MksChardevPty *self)
{
  self->master_fd = -1;
  self->slave_fd = -1;
  self->socket_fd = -1;
  self->context = g_main_context_ref_thread_default ();
}

/**
 * mks_chardev_create_pty:
 * @self: a `MksChardev`
 * Creates a PTY bridge connected to @self.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   [class@Vte.Pty].
 */
DexFuture *
mks_chardev_create_pty (MksChardev *self)
{
  g_autoptr(MksChardevPty) pty = NULL;
  g_autoptr(GError) error = NULL;
  MksChardevPtyCreate *state;
  int fds[2] = { -1, -1 };

  dex_return_error_if_fail (MKS_IS_CHARDEV (self));

  pty = g_object_new (MKS_TYPE_CHARDEV_PTY, NULL);

  if (!open_pty (&pty->master_fd, &pty->slave_fd, &error) ||
      !open_socketpair (fds, &error))
    {
      clear_fd (&fds[0]);
      clear_fd (&fds[1]);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  pty->socket_fd = fds[0];
  fds[0] = -1;

  state = g_new0 (MksChardevPtyCreate, 1);
  state->self = g_object_ref (pty);

  return dex_future_then (mks_chardev_register_fd (self, fds[1]),
                          mks_chardev_pty_create_complete,
                          state,
                          (GDestroyNotify) mks_chardev_pty_create_free);
}

void
mks_chardev_create_pty_async (MksChardev          *chardev,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  mks_future_to_async_result (chardev,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_chardev_create_pty (chardev));
}

/**
 * mks_chardev_create_pty_finish:
 * @self: a `MksChardev`
 * @result: a `GAsyncResult`
 * @error: return location for a `GError`
 *
 * Completes a request to create a PTY bridge connected to a chardev.
 *
 * Returns: (transfer full): a new [class@Vte.Pty]
 */
VtePty *
mks_chardev_create_pty_finish (MksChardev    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (MKS_IS_CHARDEV (self), NULL);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), NULL);

  return dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);
}

static int
mks_chardev_pty_dup_fd (MksChardevPty  *self,
                        GError        **error)
{
  int fd;

  g_return_val_if_fail (MKS_IS_CHARDEV_PTY (self), -1);

  if (self->master_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CLOSED,
                   "PTY is closed");
      return -1;
    }

  if (-1 == (fd = fcntl (self->master_fd, F_DUPFD_CLOEXEC, 3)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return -1;
    }

  return fd;
}

static void
mks_chardev_pty_close (MksChardevPty *self)
{
  g_return_if_fail (MKS_IS_CHARDEV_PTY (self));

  remove_source (&self->socket_read_id);
  remove_source (&self->socket_write_id);
  remove_source (&self->slave_read_id);
  remove_source (&self->slave_write_id);

  g_clear_pointer (&self->to_socket, g_bytes_unref);
  g_clear_pointer (&self->to_slave, g_bytes_unref);

  g_clear_pointer (&self->socket_channel, g_io_channel_unref);
  g_clear_pointer (&self->slave_channel, g_io_channel_unref);

  clear_fd (&self->socket_fd);
  clear_fd (&self->slave_fd);
  clear_fd (&self->master_fd);
}
