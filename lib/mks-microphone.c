/* mks-microphone.c
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

#include "mks-microphone-private.h"

G_DEFINE_ABSTRACT_TYPE (MksMicrophone, mks_microphone, MKS_TYPE_DEVICE)

static void
mks_microphone_class_init (MksMicrophoneClass *klass)
{
}

static void
mks_microphone_init (MksMicrophone *self)
{
}

gboolean
mks_microphone_get_muted (MksMicrophone *self)
{
  g_return_val_if_fail (MKS_IS_MICROPHONE (self), FALSE);

  if (MKS_MICROPHONE_GET_CLASS (self)->get_muted == NULL)
    return FALSE;

  return MKS_MICROPHONE_GET_CLASS (self)->get_muted (self);
}

void
mks_microphone_set_muted (MksMicrophone *self,
                          gboolean       muted)
{
  g_return_if_fail (MKS_IS_MICROPHONE (self));

  if (MKS_MICROPHONE_GET_CLASS (self)->set_muted != NULL)
    MKS_MICROPHONE_GET_CLASS (self)->set_muted (self, muted);
}

MksAudioFormat *
mks_microphone_dup_format (MksMicrophone *self,
                           guint64        stream_id)
{
  g_return_val_if_fail (MKS_IS_MICROPHONE (self), NULL);

  if (MKS_MICROPHONE_GET_CLASS (self)->dup_format == NULL)
    return NULL;

  return MKS_MICROPHONE_GET_CLASS (self)->dup_format (self, stream_id);
}

void
mks_microphone_queue_pcm (MksMicrophone *self,
                          guint64        stream_id,
                          GBytes        *bytes)
{
  g_return_if_fail (MKS_IS_MICROPHONE (self));
  g_return_if_fail (bytes != NULL);

  if (MKS_MICROPHONE_GET_CLASS (self)->queue_pcm != NULL)
    MKS_MICROPHONE_GET_CLASS (self)->queue_pcm (self, stream_id, bytes);
}

/**
 * mks_microphone_create_gst_sink:
 * @self: a `MksMicrophone`
 * @stream_id: an audio stream id
 *
 * Creates a GStreamer sink for PCM frames for @stream_id.
 *
 * Returns: (transfer floating) (nullable): a new `GstElement`.
 */
GstElement *
mks_microphone_create_gst_sink (MksMicrophone *self,
                                guint64        stream_id)
{
  g_return_val_if_fail (MKS_IS_MICROPHONE (self), NULL);

  if (MKS_MICROPHONE_GET_CLASS (self)->create_gst_sink == NULL)
    return NULL;

  return MKS_MICROPHONE_GET_CLASS (self)->create_gst_sink (self, stream_id);
}
