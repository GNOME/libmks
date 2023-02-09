/* libmks.c
 *
 * Copyright 2023 Christian Hergert
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

#include "gconstructor.h"

#include "mks-device.h"
#include "mks-init.h"
#include "mks-keyboard.h"
#include "mks-mouse.h"
#include "mks-paintable.h"
#include "mks-qemu.h"
#include "mks-read-only-list-model-private.h"
#include "mks-screen.h"
#include "mks-session.h"
#include "mks-version.h"

static void
mks_init_gtypes (void)
{
  /* First register GTypes for Qemu IPC */
  g_type_ensure (MKS_QEMU_TYPE_AUDIO);
  g_type_ensure (MKS_QEMU_TYPE_AUDIO_IN_LISTENER);
  g_type_ensure (MKS_QEMU_TYPE_AUDIO_OUT_LISTENER);
  g_type_ensure (MKS_QEMU_TYPE_CHARDEV);
  g_type_ensure (MKS_QEMU_TYPE_CLIPBOARD);
  g_type_ensure (MKS_QEMU_TYPE_CONSOLE);
  g_type_ensure (MKS_QEMU_TYPE_LISTENER);
  g_type_ensure (MKS_QEMU_TYPE_MOUSE);
  g_type_ensure (MKS_QEMU_TYPE_VM);

  /* Internal types not exposed in public API */
  g_type_ensure (MKS_TYPE_READ_ONLY_LIST_MODEL);

  /* GTypes that are part of our public API */
  g_type_ensure (MKS_TYPE_DEVICE);
  g_type_ensure (MKS_TYPE_KEYBOARD);
  g_type_ensure (MKS_TYPE_MOUSE);
  g_type_ensure (MKS_TYPE_PAINTABLE);
  g_type_ensure (MKS_TYPE_SCREEN);
  g_type_ensure (MKS_TYPE_SESSION);
}

void
mks_init (void)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      mks_init_gtypes ();
      g_once_init_leave (&initialized, TRUE);
    }
}

G_DEFINE_CONSTRUCTOR (_mks_init)

static void
_mks_init (void)
{
  mks_init ();
}

int
mks_get_major_version (void)
{
  return MKS_MAJOR_VERSION;
}

int
mks_get_minor_version (void)
{
  return MKS_MINOR_VERSION;
}

int
mks_get_micro_version (void)
{
  return MKS_MICRO_VERSION;
}
