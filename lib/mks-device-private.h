/*
 * mks-device-private.h
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

#pragma once

#include "mks-device.h"
#include "mks-qemu.h"

G_BEGIN_DECLS

struct _MksDevice
{
  GObject        parent_instance;
  MksQemuObject *object;
  char          *name;
};

struct _MksDeviceClass
{
  GObjectClass parent_class;

  gboolean (*setup) (MksDevice     *self,
                     MksQemuObject *object);
};

gpointer _mks_device_new      (GType          device_type,
                               MksQemuObject *object);
void     _mks_device_set_name (MksDevice     *self,
                               const char    *name);

G_END_DECLS
