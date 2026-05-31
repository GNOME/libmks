/*
 * mks-microphone.h
 *
 * Copyright 2026 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <gst/gst.h>

#include "mks-audio-format.h"
#include "mks-device.h"

G_BEGIN_DECLS

#define MKS_TYPE_MICROPHONE            (mks_microphone_get_type())
#define MKS_MICROPHONE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_MICROPHONE, MksMicrophone))
#define MKS_MICROPHONE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_MICROPHONE, MksMicrophone const))
#define MKS_MICROPHONE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MKS_TYPE_MICROPHONE, MksMicrophoneClass))
#define MKS_IS_MICROPHONE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKS_TYPE_MICROPHONE))
#define MKS_IS_MICROPHONE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MKS_TYPE_MICROPHONE))
#define MKS_MICROPHONE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MKS_TYPE_MICROPHONE, MksMicrophoneClass))

typedef struct _MksMicrophoneClass MksMicrophoneClass;

MKS_AVAILABLE_IN_ALL
GType           mks_microphone_get_type        (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
gboolean        mks_microphone_get_muted       (MksMicrophone *self);
MKS_AVAILABLE_IN_ALL
void            mks_microphone_set_muted       (MksMicrophone *self,
                                                gboolean       muted);
MKS_AVAILABLE_IN_ALL
MksAudioFormat *mks_microphone_dup_format      (MksMicrophone *self,
                                                guint64        stream_id);
MKS_AVAILABLE_IN_ALL
void            mks_microphone_queue_pcm       (MksMicrophone *self,
                                                guint64        stream_id,
                                                GBytes        *bytes);
MKS_AVAILABLE_IN_ALL
GstElement     *mks_microphone_create_gst_sink (MksMicrophone *self,
                                                guint64        stream_id);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksMicrophone, g_object_unref)

G_END_DECLS
