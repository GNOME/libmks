/* test-mks-transport.c
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

#include <libmks.h>

#include "lib/mks-screen-private.h"
#include "lib/mks-transport-private.h"

typedef struct _MksTestTransport      MksTestTransport;
typedef struct _MksTestTransportClass MksTestTransportClass;

#define MKS_TYPE_TEST_TRANSPORT (mks_test_transport_get_type())

GType mks_test_transport_get_type (void);

struct _MksTestTransport
{
  MksTransport parent_instance;
};

struct _MksTestTransportClass
{
  MksTransportClass parent_class;
};

G_DEFINE_TYPE (MksTestTransport, mks_test_transport, MKS_TYPE_TRANSPORT)

static void
mks_test_transport_class_init (MksTestTransportClass *klass)
{
}

static void
mks_test_transport_init (MksTestTransport *self)
{
}

static void
mks_test_transport_emit_device_added (MksTransport *transport,
                                      MksDevice    *device)
{
  g_assert (MKS_IS_TRANSPORT (transport));
  g_assert (MKS_IS_DEVICE (device));

  for (guint i = 0; i < transport->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (transport->observers, i);

      if (observer->device_added != NULL)
        observer->device_added (observer->user_data, device);
    }
}

static void
mks_test_transport_emit_device_removed (MksTransport *transport,
                                        MksDevice    *device)
{
  g_assert (MKS_IS_TRANSPORT (transport));
  g_assert (MKS_IS_DEVICE (device));

  for (guint i = 0; i < transport->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (transport->observers, i);

      if (observer->device_removed != NULL)
        observer->device_removed (observer->user_data, device);
    }
}

typedef struct _MksTestScreen      MksTestScreen;
typedef struct _MksTestScreenClass MksTestScreenClass;

#define MKS_TYPE_TEST_SCREEN (mks_test_screen_get_type())

GType mks_test_screen_get_type (void);

struct _MksTestScreen
{
  MksScreen parent_instance;

  char *device_address;
  MksScreenKind kind;
  guint width;
  guint height;
  guint number;
};

struct _MksTestScreenClass
{
  MksScreenClass parent_class;
};

G_DEFINE_TYPE (MksTestScreen, mks_test_screen, MKS_TYPE_SCREEN)

static MksScreenKind
mks_test_screen_get_kind (MksScreen *screen)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  return self->kind;
}

static guint
mks_test_screen_get_width (MksScreen *screen)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  return self->width;
}

static guint
mks_test_screen_get_height (MksScreen *screen)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  return self->height;
}

static guint
mks_test_screen_get_number (MksScreen *screen)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  return self->number;
}

static const char *
mks_test_screen_get_device_address (MksScreen *screen)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  return self->device_address;
}

static void
mks_test_screen_set_size (MksScreen *screen,
                          guint      width,
                          guint      height)
{
  MksTestScreen *self = (MksTestScreen *)screen;

  g_assert (MKS_IS_SCREEN (screen));

  if (self->width != width)
    {
      self->width = width;
      g_object_notify (G_OBJECT (self), "width");
    }

  if (self->height != height)
    {
      self->height = height;
      g_object_notify (G_OBJECT (self), "height");
    }
}

static void
mks_test_screen_mark_active (MksScreen *screen)
{
  gint64 now;

  g_assert (MKS_IS_SCREEN (screen));

  now = g_get_monotonic_time ();

  if (screen->last_active_time != now)
    {
      screen->last_active_time = now;
      g_object_notify (G_OBJECT (screen), "last-active-time");
    }
}

static void
mks_test_screen_finalize (GObject *object)
{
  MksTestScreen *self = (MksTestScreen *)object;

  g_clear_pointer (&self->device_address, g_free);

  G_OBJECT_CLASS (mks_test_screen_parent_class)->finalize (object);
}

static void
mks_test_screen_class_init (MksTestScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksScreenClass *screen_class = MKS_SCREEN_CLASS (klass);

  object_class->finalize = mks_test_screen_finalize;

  screen_class->get_kind = mks_test_screen_get_kind;
  screen_class->get_width = mks_test_screen_get_width;
  screen_class->get_height = mks_test_screen_get_height;
  screen_class->get_number = mks_test_screen_get_number;
  screen_class->get_device_address = mks_test_screen_get_device_address;
}

static void
mks_test_screen_init (MksTestScreen *self)
{
}

static MksSession *
mks_test_session_new (MksTransport **transport)
{
  g_autoptr(MksTransport) local_transport = NULL;

  local_transport = g_object_new (MKS_TYPE_TEST_TRANSPORT, NULL);

  if (transport != NULL)
    *transport = g_object_ref (local_transport);

  return g_object_new (MKS_TYPE_SESSION,
                       "transport", local_transport,
                       NULL);
}

static MksScreen *
mks_test_screen_new (MksScreenKind  kind,
                     guint          width,
                     guint          height,
                     guint          number,
                     const char    *device_address)
{
  MksTestScreen *self;

  self = g_object_new (MKS_TYPE_TEST_SCREEN, NULL);
  self->kind = kind;
  self->width = width;
  self->height = height;
  self->number = number;
  self->device_address = g_strdup (device_address);

  return MKS_SCREEN (self);
}

static void
test_mks_session_list_devices (void)
{
  g_autoptr(MksTransport) transport = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksDevice) device = NULL;
  g_autoptr(MksScreen) screen = NULL;
  g_autoptr(GListModel) devices = NULL;
  g_autoptr(MksDevice) item = NULL;

  session = mks_test_session_new (&transport);
  device = g_object_new (MKS_TYPE_DEVICE, NULL);
  screen = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 800, 600, 0, "pci.0");

  mks_test_transport_emit_device_added (transport, device);
  mks_test_transport_emit_device_added (transport, MKS_DEVICE (screen));

  devices = mks_session_list_devices (session);

  g_assert_cmpint (g_list_model_get_item_type (devices), ==, MKS_TYPE_DEVICE);
  g_assert_cmpint (g_list_model_get_n_items (devices), ==, 2);

  item = g_list_model_get_item (devices, 0);
  g_assert_true (item == device);

  g_clear_object (&item);
  item = g_list_model_get_item (devices, 1);
  g_assert_true (item == MKS_DEVICE (screen));
}

static void
test_mks_session_list_screens (void)
{
  g_autoptr(MksTransport) transport = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksDevice) device = NULL;
  g_autoptr(MksScreen) screen = NULL;
  g_autoptr(GListModel) screens = NULL;
  g_autoptr(MksScreen) item = NULL;
  g_autofree char *device_address = NULL;
  MksScreenKind kind = MKS_SCREEN_KIND_TEXT;
  guint width = 0;
  guint height = 0;
  guint number = G_MAXUINT;

  session = mks_test_session_new (&transport);
  device = g_object_new (MKS_TYPE_DEVICE, NULL);
  screen = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 800, 600, 0, "pci.0");

  mks_test_transport_emit_device_added (transport, device);
  mks_test_transport_emit_device_added (transport, MKS_DEVICE (screen));

  screens = mks_session_list_screens (session);

  g_assert_cmpint (g_list_model_get_item_type (screens), ==, MKS_TYPE_SCREEN);
  g_assert_cmpint (g_list_model_get_n_items (screens), ==, 1);

  item = g_list_model_get_item (screens, 0);
  g_assert_true (item == screen);

  g_object_get (screen,
                "device-address", &device_address,
                "height", &height,
                "kind", &kind,
                "number", &number,
                "width", &width,
                NULL);

  g_assert_cmpstr (device_address, ==, "pci.0");
  g_assert_cmpint (kind, ==, MKS_SCREEN_KIND_GRAPHIC);
  g_assert_cmpuint (number, ==, 0);
  g_assert_cmpuint (width, ==, 800);
  g_assert_cmpuint (height, ==, 600);
}

static void
test_mks_session_primary_screen (void)
{
  g_autoptr(MksTransport) transport = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksScreen) text = NULL;
  g_autoptr(MksScreen) graphic_head_1 = NULL;
  g_autoptr(MksScreen) graphic_head_0 = NULL;
  g_autoptr(MksScreen) primary = NULL;
  g_autoptr(MksScreen) property_primary = NULL;

  session = mks_test_session_new (&transport);
  text = mks_test_screen_new (MKS_SCREEN_KIND_TEXT, 1024, 768, 0, "pci.0");
  graphic_head_1 = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 1024, 768, 1, "pci.1");
  graphic_head_0 = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 1024, 768, 0, "pci.2");

  mks_test_transport_emit_device_added (transport, MKS_DEVICE (text));
  mks_test_transport_emit_device_added (transport, MKS_DEVICE (graphic_head_1));
  mks_test_transport_emit_device_added (transport, MKS_DEVICE (graphic_head_0));

  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == graphic_head_1);

  mks_test_screen_mark_active (graphic_head_0);
  g_assert_true (mks_screen_get_last_active_time (graphic_head_0) > 0);

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == graphic_head_0);

  g_object_get (session,
                "primary-screen", &property_primary,
                NULL);
  g_assert_true (property_primary == graphic_head_0);

  mks_test_transport_emit_device_removed (transport, MKS_DEVICE (graphic_head_0));

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == graphic_head_1);
}

static void
test_mks_session_primary_screen_waits_for_new_graphic_activity (void)
{
  g_autoptr(MksTransport) transport = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksScreen) vga = NULL;
  g_autoptr(MksScreen) virtio = NULL;
  g_autoptr(MksScreen) primary = NULL;

  session = mks_test_session_new (&transport);
  vga = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 1024, 768, 0, "pci.0");
  virtio = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 1920, 1080, 0, "pci.1");

  mks_test_transport_emit_device_added (transport, MKS_DEVICE (vga));

  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == vga);

  mks_test_transport_emit_device_added (transport, MKS_DEVICE (virtio));

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == vga);

  mks_test_screen_mark_active (virtio);

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == virtio);
}

static void
test_mks_session_primary_screen_waits_for_settled_graphic_activity (void)
{
  g_autoptr(MksTransport) transport = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksScreen) vga = NULL;
  g_autoptr(MksScreen) virtio = NULL;
  g_autoptr(MksScreen) primary = NULL;

  session = mks_test_session_new (&transport);
  vga = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 1024, 768, 0, "pci.0");
  virtio = mks_test_screen_new (MKS_SCREEN_KIND_GRAPHIC, 0, 0, 0, "pci.1");

  mks_test_transport_emit_device_added (transport, MKS_DEVICE (vga));
  mks_test_transport_emit_device_added (transport, MKS_DEVICE (virtio));

  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == vga);

  mks_test_screen_set_size (virtio, 1920, 1080);

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == vga);

  mks_test_screen_mark_active (virtio);

  g_clear_object (&primary);
  primary = mks_session_dup_primary_screen (session);
  g_assert_true (primary == virtio);
}

static void
test_mks_transport_add_tests (void)
{
  g_test_add_func ("/Mks/session/list-devices", test_mks_session_list_devices);
  g_test_add_func ("/Mks/session/list-screens", test_mks_session_list_screens);
  g_test_add_func ("/Mks/session/primary-screen", test_mks_session_primary_screen);
  g_test_add_func ("/Mks/session/primary-screen/waits-for-new-graphic-activity",
                   test_mks_session_primary_screen_waits_for_new_graphic_activity);
  g_test_add_func ("/Mks/session/primary-screen/waits-for-settled-graphic-activity",
                   test_mks_session_primary_screen_waits_for_settled_graphic_activity);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  test_mks_transport_add_tests ();

  return g_test_run ();
}
