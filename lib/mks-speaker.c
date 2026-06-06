/* mks-speaker.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "config.h"

#include "mks-speaker-private.h"

G_DEFINE_ABSTRACT_TYPE (MksSpeaker, mks_speaker, MKS_TYPE_DEVICE)

static void
mks_speaker_class_init (MksSpeakerClass *klass)
{
}

static void
mks_speaker_init (MksSpeaker *self)
{
}

gboolean
mks_speaker_get_muted (MksSpeaker *self)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), FALSE);

  if (MKS_SPEAKER_GET_CLASS (self)->get_muted == NULL)
    return FALSE;

  return MKS_SPEAKER_GET_CLASS (self)->get_muted (self);
}

void
mks_speaker_set_muted (MksSpeaker *self,
                       gboolean    muted)
{
  g_return_if_fail (MKS_IS_SPEAKER (self));

  if (MKS_SPEAKER_GET_CLASS (self)->set_muted != NULL)
    MKS_SPEAKER_GET_CLASS (self)->set_muted (self, muted);
}

MksAudioFormat *
mks_speaker_dup_format (MksSpeaker *self,
                        guint64     stream_id)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), NULL);

  if (MKS_SPEAKER_GET_CLASS (self)->dup_format == NULL)
    return NULL;

  return MKS_SPEAKER_GET_CLASS (self)->dup_format (self, stream_id);
}

guint
mks_speaker_add_pcm_observer (MksSpeaker        *self,
                              MksSpeakerPcmFunc  callback,
                              gpointer           user_data,
                              GDestroyNotify     user_data_destroy)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), 0);
  g_return_val_if_fail (callback != NULL, 0);

  if (MKS_SPEAKER_GET_CLASS (self)->add_pcm_observer == NULL)
    return 0;

  return MKS_SPEAKER_GET_CLASS (self)->add_pcm_observer (self, callback, user_data, user_data_destroy);
}

void
mks_speaker_remove_pcm_observer (MksSpeaker *self,
                                 guint       observer_id)
{
  g_return_if_fail (MKS_IS_SPEAKER (self));

  if (MKS_SPEAKER_GET_CLASS (self)->remove_pcm_observer != NULL)
    MKS_SPEAKER_GET_CLASS (self)->remove_pcm_observer (self, observer_id);
}

/**
 * mks_speaker_create_gst_source:
 * @self: a `MksSpeaker`
 * @stream_id: an audio stream id
 *
 * Creates a GStreamer source for PCM frames from @stream_id.
 *
 * Returns: (transfer floating) (nullable): a new `GstElement`.
 */
GstElement *
mks_speaker_create_gst_source (MksSpeaker *self,
                               guint64     stream_id)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), NULL);

  if (MKS_SPEAKER_GET_CLASS (self)->create_gst_source == NULL)
    return NULL;

  return MKS_SPEAKER_GET_CLASS (self)->create_gst_source (self, stream_id);
}
