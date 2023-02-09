/*
 * mks-device.c
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

typedef struct
{
  char *name;
} MksDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MksDevice, mks_device, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_device_finalize (GObject *object)
{
  MksDevice *self = (MksDevice *)object;
  MksDevicePrivate *priv = mks_device_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (mks_device_parent_class)->finalize (object);
}

static void
mks_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MksDevice *self = MKS_DEVICE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, mks_device_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_device_class_init (MksDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mks_device_finalize;
  object_class->get_property = mks_device_get_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_device_init (MksDevice *self)
{
}

const char *
mks_device_get_name (MksDevice *self)
{
  MksDevicePrivate *priv = mks_device_get_instance_private (self);

  g_return_val_if_fail (MKS_IS_DEVICE (self), NULL);

  return priv->name;
}

void
_mks_device_set_name (MksDevice  *self,
                      const char *name)
{
  MksDevicePrivate *priv = mks_device_get_instance_private (self);

  g_return_if_fail (MKS_IS_DEVICE (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
}
