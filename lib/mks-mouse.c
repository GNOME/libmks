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
};

struct _MksMouseClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksMouse, mks_mouse, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

MksMouse *
mks_mouse_new (void)
{
  return g_object_new (MKS_TYPE_MOUSE, NULL);
}

static void
mks_mouse_finalize (GObject *object)
{
  MksMouse *self = (MksMouse *)object;

  G_OBJECT_CLASS (mks_mouse_parent_class)->finalize (object);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_mouse_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MksMouse *self = MKS_MOUSE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_mouse_class_init (MksMouseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mks_mouse_finalize;
  object_class->get_property = mks_mouse_get_property;
  object_class->set_property = mks_mouse_set_property;
}

static void
mks_mouse_init (MksMouse *self)
{

}
