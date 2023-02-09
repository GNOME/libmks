/*
 * mks-connect.c
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

#include <locale.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libmks.h>

int
main (int argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = g_option_context_new ("DBUS_ADDRESS - Connect to Qemu at DBUS_ADDRESS");
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(MksSession) session = NULL;
  g_autoptr(GListModel) devices = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  setlocale (LC_ALL, "");
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  gtk_init ();
  mks_init ();

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  if (argc < 2)
    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  else
    connection = g_dbus_connection_new_for_address_sync (argv[1], 0, NULL, NULL, &error);

  if (connection == NULL)
    {
      g_printerr ("Failed to connect to D-Bus: %s\n",
                  error->message);
      return EXIT_FAILURE;
    }

  if (!(session = mks_session_new_for_connection_sync (connection, NULL, &error)))
    {
      g_printerr ("Failed to create MksSession: %s\n",
                  error->message);
      return EXIT_FAILURE;
    }

  g_print ("Session(uuid=\"%s\" name=\"%s\")\n",
           mks_session_get_uuid (session),
           mks_session_get_name (session));

  devices = mks_session_get_devices (session);
  n_items = g_list_model_get_n_items (devices);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksDevice) device = g_list_model_get_item (devices, i);

      g_print (" - %s(name=\"%s\"",
               G_OBJECT_TYPE_NAME (device),
               mks_device_get_name (device));

      if (MKS_IS_SCREEN (device))
        g_print (" number=%u width=%u height=%u",
                 mks_screen_get_number (MKS_SCREEN (device)),
                 mks_screen_get_width (MKS_SCREEN (device)),
                 mks_screen_get_height (MKS_SCREEN (device)));

      g_print (")\n");
    }

  return EXIT_SUCCESS;
}
