/* mks.c
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

#include <unistd.h>

#include <gtk/gtk.h>
#include <libmks.h>

static GDBusConnection *
create_connection (int      argc,
                   char   **argv,
                   GError **error)
{
  if (argc < 2)
    return g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  else
    return g_dbus_connection_new_for_address_sync (argv[1], 0, NULL, NULL, error);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GdkPaintable) paintable = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(MksScreen) screen = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GError) error = NULL;
  GtkWindow *window;
  GtkPicture *picture;

  gtk_init ();
  mks_init ();

  if (!(connection = create_connection (argc, argv, &error)))
    {
      g_printerr ("Failed to connect to D-Bus: %s\n", error->message);
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 640,
                         "default-height", 480,
                         "title", "Mouse, Keyboard, Screen",
                         NULL);
  picture = g_object_new (GTK_TYPE_PICTURE,
                          "content-fit", GTK_CONTENT_FIT_SCALE_DOWN,
                          NULL);
  gtk_window_set_child (window, GTK_WIDGET (picture));
  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);

  if (!(session = mks_session_new_for_connection_sync (connection, NULL, &error)))
    {
      g_printerr ("Failed to create MksSession: %s\n", error->message);
      return EXIT_FAILURE;
    }

  if (!(screen = mks_session_ref_screen (session)))
    {
      g_printerr ("No screen attached to session!\n");
      return EXIT_FAILURE;
    }

  if (!(paintable = mks_screen_attach_sync (screen, NULL, &error)))
    {
      g_printerr ("Failed to create paintable for screen: %s\n", error->message);
      return EXIT_FAILURE;
    }

  gtk_picture_set_paintable (picture, paintable);

  gtk_window_present (window);
  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
