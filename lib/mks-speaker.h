/* mks-speaker.h
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

#pragma once

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <gst/gst.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_SPEAKER            (mks_speaker_get_type())

MKS_AVAILABLE_IN_ALL
MKS_DECLARE_INTERNAL_TYPE (MksSpeaker, mks_speaker, MKS, SPEAKER, MksDevice)

typedef void (*MksSpeakerPcmFunc) (MksSpeaker *speaker,
                                   guint64     stream_id,
                                   GBytes     *bytes,
                                   gpointer    user_data);

MKS_AVAILABLE_IN_ALL
gboolean        mks_speaker_get_muted           (MksSpeaker        *self);
MKS_AVAILABLE_IN_ALL
void            mks_speaker_set_muted           (MksSpeaker        *self,
                                                 gboolean           muted);
MKS_AVAILABLE_IN_ALL
MksAudioFormat *mks_speaker_dup_format          (MksSpeaker        *self,
                                                 guint64            stream_id);
MKS_AVAILABLE_IN_ALL
guint           mks_speaker_add_pcm_observer    (MksSpeaker        *self,
                                                 MksSpeakerPcmFunc  callback,
                                                 gpointer           user_data,
                                                 GDestroyNotify     user_data_destroy);
MKS_AVAILABLE_IN_ALL
void            mks_speaker_remove_pcm_observer (MksSpeaker        *self,
                                                 guint              observer_id);
MKS_AVAILABLE_IN_ALL
GstElement     *mks_speaker_create_gst_source   (MksSpeaker        *self,
                                                 guint64            stream_id);

G_END_DECLS
