/* test-mks-remote.c
 *
 * Copyright 2026 Christian Hergert
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

#include "config.h"

#include <libmks.h>

#include "lib/mks-remote-private.h"
#include "lib/mks-screen-private.h"

typedef struct _MksTestRemote      MksTestRemote;
typedef struct _MksTestRemoteClass MksTestRemoteClass;

#define MKS_TYPE_TEST_REMOTE (mks_test_remote_get_type ())

GType mks_test_remote_get_type (void);

struct _MksTestRemote
{
  MksRemote parent_instance;
  guint last_keycode;
  guint press_count;
  guint release_count;
};

struct _MksTestRemoteClass
{
  MksRemoteClass parent_class;
};

G_DEFINE_TYPE (MksTestRemote, mks_test_remote, MKS_TYPE_REMOTE)

typedef struct _MksUnsupportedRemote      MksUnsupportedRemote;
typedef struct _MksUnsupportedRemoteClass MksUnsupportedRemoteClass;

#define MKS_TYPE_UNSUPPORTED_REMOTE (mks_unsupported_remote_get_type ())

GType mks_unsupported_remote_get_type (void);

struct _MksUnsupportedRemote
{
  MksRemote parent_instance;
};

struct _MksUnsupportedRemoteClass
{
  MksRemoteClass parent_class;
};

G_DEFINE_TYPE (MksUnsupportedRemote, mks_unsupported_remote, MKS_TYPE_REMOTE)

static void
mks_unsupported_remote_class_init (MksUnsupportedRemoteClass *klass)
{
}

static void
mks_unsupported_remote_init (MksUnsupportedRemote *self)
{
}

static DexFuture *
mks_test_remote_press (MksRemote *remote,
                       guint      linux_keycode)
{
  MksTestRemote *self = (MksTestRemote *)remote;

  self->last_keycode = linux_keycode;
  self->press_count++;

  return dex_future_new_true ();
}

static DexFuture *
mks_test_remote_release (MksRemote *remote,
                         guint      linux_keycode)
{
  MksTestRemote *self = (MksTestRemote *)remote;

  self->last_keycode = linux_keycode;
  self->release_count++;

  return dex_future_new_true ();
}

static void
mks_test_remote_class_init (MksTestRemoteClass *klass)
{
  MksRemoteClass *remote_class = MKS_REMOTE_CLASS (klass);

  remote_class->press = mks_test_remote_press;
  remote_class->release = mks_test_remote_release;
}

static void
mks_test_remote_init (MksTestRemote *self)
{
}

typedef struct _MksTestScreen      MksTestScreen;
typedef struct _MksTestScreenClass MksTestScreenClass;

#define MKS_TYPE_TEST_SCREEN (mks_test_screen_get_type ())

GType mks_test_screen_get_type (void);

struct _MksTestScreen
{
  MksScreen parent_instance;
  MksRemote *remote;
};

struct _MksTestScreenClass
{
  MksScreenClass parent_class;
};

G_DEFINE_TYPE (MksTestScreen, mks_test_screen, MKS_TYPE_SCREEN)

static MksRemote *
mks_test_screen_get_remote (MksScreen *screen)
{
  return ((MksTestScreen *)screen)->remote;
}

static void
mks_test_screen_dispose (GObject *object)
{
  MksTestScreen *self = (MksTestScreen *)object;

  g_clear_object (&self->remote);

  G_OBJECT_CLASS (mks_test_screen_parent_class)->dispose (object);
}

static void
mks_test_screen_class_init (MksTestScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksScreenClass *screen_class = MKS_SCREEN_CLASS (klass);

  object_class->dispose = mks_test_screen_dispose;
  screen_class->get_remote = mks_test_screen_get_remote;
}

static void
mks_test_screen_init (MksTestScreen *self)
{
}

static void
test_remote_futures (void)
{
  g_autoptr(MksRemote) remote = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  MksTestRemote *test_remote;

  remote = g_object_new (MKS_TYPE_TEST_REMOTE, NULL);
  test_remote = (MksTestRemote *)remote;

  future = mks_remote_press (remote, 0x160);
  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_true (g_value_get_boolean (value));
  g_assert_cmpuint (test_remote->last_keycode, ==, 0x160);
  g_assert_cmpuint (test_remote->press_count, ==, 1);

  dex_clear (&future);
  future = mks_remote_release (remote, 0x160);
  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_true (g_value_get_boolean (value));
  g_assert_cmpuint (test_remote->release_count, ==, 1);
}

static void
test_remote_unsupported (void)
{
  g_autoptr(MksRemote) remote = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GError) error = NULL;

  remote = g_object_new (MKS_TYPE_UNSUPPORTED_REMOTE, NULL);
  future = mks_remote_press (remote, 0x160);
  g_assert_null (dex_future_get_value (future, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);
  dex_clear (&future);
  future = mks_remote_release (remote, 0x160);
  g_assert_null (dex_future_get_value (future, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

static void
remote_press_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert_true (mks_remote_press_finish (MKS_REMOTE (object), result, &error));
  g_assert_no_error (error);
  g_main_loop_quit (user_data);
}

static void
remote_release_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert_true (mks_remote_release_finish (MKS_REMOTE (object), result, &error));
  g_assert_no_error (error);
  g_main_loop_quit (user_data);
}

static void
test_remote_async (void)
{
  g_autoptr(GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(MksRemote) remote = g_object_new (MKS_TYPE_TEST_REMOTE, NULL);
  MksTestRemote *test_remote = (MksTestRemote *)remote;

  mks_remote_press_async (remote, 0x172, NULL, remote_press_cb, loop);
  g_main_loop_run (loop);
  g_assert_cmpuint (test_remote->last_keycode, ==, 0x172);
  g_assert_cmpuint (test_remote->press_count, ==, 1);

  mks_remote_release_async (remote, 0x172, NULL, remote_release_cb, loop);
  g_main_loop_run (loop);
  g_assert_cmpuint (test_remote->last_keycode, ==, 0x172);
  g_assert_cmpuint (test_remote->release_count, ==, 1);
}

static void
test_screen_optional_remote (void)
{
  g_autoptr(MksScreen) screen = NULL;
  g_autoptr(MksRemote) remote = NULL;
  g_autoptr(MksRemote) property_remote = NULL;
  MksTestScreen *test_screen;

  screen = g_object_new (MKS_TYPE_TEST_SCREEN, NULL);
  g_assert_null (mks_screen_get_remote (screen));

  remote = g_object_new (MKS_TYPE_TEST_REMOTE, NULL);
  test_screen = (MksTestScreen *)screen;
  test_screen->remote = g_object_ref (remote);
  g_object_notify (G_OBJECT (screen), "remote");

  g_assert_true (mks_screen_get_remote (screen) == remote);
  g_object_get (screen, "remote", &property_remote, NULL);
  g_assert_true (property_remote == remote);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  dex_init ();
  mks_init ();

  g_test_add_func ("/Mks/remote/futures", test_remote_futures);
  g_test_add_func ("/Mks/remote/async", test_remote_async);
  g_test_add_func ("/Mks/remote/unsupported", test_remote_unsupported);
  g_test_add_func ("/Mks/screen/optional-remote", test_screen_optional_remote);

  return g_test_run ();
}
