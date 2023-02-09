/*
 * mks-mouse.c
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

#include "mks-device-private.h"
#include "mks-mouse.h"

struct _MksMouse
{
  MksDevice parent_instance;
  MksQemuMouse *mouse;
};

struct _MksMouseClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksMouse, mks_mouse, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_IS_ABSOLUTE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_mouse_set_mouse (MksMouse     *self,
                     MksQemuMouse *mouse)
{
  g_assert (MKS_IS_MOUSE (self));
  g_assert (MKS_QEMU_IS_MOUSE (mouse));

  g_set_object (&self->mouse, mouse);
}

static gboolean
mks_mouse_setup (MksDevice     *device,
                 MksQemuObject *object)
{
  MksMouse *self = (MksMouse *)device;
  g_autolist(GDBusInterface) interfaces = NULL;

  g_assert (MKS_IS_MOUSE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (object));

  for (const GList *iter = interfaces; iter; iter = iter->next)
    {
      GDBusInterface *iface = iter->data;

      if (MKS_QEMU_IS_MOUSE (iface))
        mks_mouse_set_mouse (self, MKS_QEMU_MOUSE (iface));
    }

  return self->mouse != NULL;
}

static void
mks_mouse_dispose (GObject *object)
{
  MksMouse *self = (MksMouse *)object;

  g_clear_object (&self->mouse);

  G_OBJECT_CLASS (mks_mouse_parent_class)->dispose (object);
}

static void
mks_mouse_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MksMouse *self = MKS_MOUSE (object);

  switch (prop_id)
    {
    case PROP_IS_ABSOLUTE:
      g_value_set_boolean (value, mks_mouse_get_is_absolute (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_mouse_class_init (MksMouseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_mouse_dispose;
  object_class->get_property = mks_mouse_get_property;

  device_class->setup = mks_mouse_setup;

  properties [PROP_IS_ABSOLUTE] =
    g_param_spec_boolean ("is-absolute", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_mouse_init (MksMouse *self)
{
}

gboolean
mks_mouse_get_is_absolute (MksMouse *self)
{
  g_return_val_if_fail (MKS_IS_MOUSE (self), FALSE);

  if (self->mouse)
    return mks_qemu_mouse_get_is_absolute (self->mouse);

  return FALSE;
}
