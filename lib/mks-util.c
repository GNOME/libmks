/* mks-util.c
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

#include "mks-util-private.h"

static GSettings *mouse_settings;
static GSettings *touchpad_settings;
static gsize initialized;

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
 * host setting for the GSetting. Additionally, it wont work with jhbuild
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
