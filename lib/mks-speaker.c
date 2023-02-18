/*
 * mks-speaker.c
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
#include "mks-qemu.h"
#include "mks-speaker.h"

struct _MksSpeaker
{
  MksDevice parent_instance;
  guint muted : 1;
};

struct _MksSpeakerClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksSpeaker, mks_speaker, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MUTED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
mks_speaker_dispose (GObject *object)
{
  G_OBJECT_CLASS (mks_speaker_parent_class)->dispose (object);
}

static void
mks_speaker_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksSpeaker *self = MKS_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      g_value_set_boolean (value, mks_speaker_get_muted (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_speaker_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MksSpeaker *self = MKS_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      mks_speaker_set_muted (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_speaker_class_init (MksSpeakerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_speaker_dispose;
  object_class->get_property = mks_speaker_get_property;
  object_class->set_property = mks_speaker_set_property;

  /**
   * MksSpeaker:muted:
   *
   * The "muted" property denotes if audio received from the
   * instance is dropped and the remote sound device should
   * attempt to be set as muted.
   */
  properties [PROP_MUTED] =
    g_param_spec_boolean ("muted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_speaker_init (MksSpeaker *self)
{
}

/**
 * mks_speaker_get_muted:
 * @self: a #MksSpeaker
 *
 * Gets if the speaker is muted.
 *
 * Returns: %TRUE if the #MksSpeaker is muted.
 */
gboolean
mks_speaker_get_muted (MksSpeaker *self)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), FALSE);

  return self->muted;
}

/**
 * mks_speaker_set_muted:
 * @self: a #MksSpeaker
 * @muted: if the speaker should be muted
 *
 * Sets the #MksSpeaker:muted property.
 */
void
mks_speaker_set_muted (MksSpeaker *self,
                       gboolean    muted)
{
  g_return_if_fail (MKS_IS_SPEAKER (self));

  muted = !!muted;

  if (self->muted != muted)
    {
      self->muted = muted;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MUTED]);
    }
}
