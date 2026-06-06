/* mks-audio-format.h
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

#include <glib-object.h>
#include <gst/gst.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_AUDIO_FORMAT (mks_audio_format_get_type())

MKS_AVAILABLE_IN_ALL
GType           mks_audio_format_get_type             (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
MksAudioFormat *mks_audio_format_new                  (guchar          bits,
                                                       gboolean        is_signed,
                                                       gboolean        is_float,
                                                       guint           frequency,
                                                       guchar          channels,
                                                       guint           bytes_per_frame,
                                                       guint           bytes_per_second,
                                                       gboolean        big_endian);
MKS_AVAILABLE_IN_ALL
MksAudioFormat *mks_audio_format_copy                 (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
void            mks_audio_format_free                 (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
guchar          mks_audio_format_get_bits             (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
gboolean        mks_audio_format_get_signed           (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
gboolean        mks_audio_format_get_float            (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
guint           mks_audio_format_get_frequency        (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
guchar          mks_audio_format_get_channels         (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
guint           mks_audio_format_get_bytes_per_frame  (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
guint           mks_audio_format_get_bytes_per_second (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
gboolean        mks_audio_format_get_big_endian       (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
char           *mks_audio_format_dup_gstreamer_format (MksAudioFormat *self);
MKS_AVAILABLE_IN_ALL
GstCaps        *mks_audio_format_to_gst_caps          (MksAudioFormat *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksAudioFormat, mks_audio_format_free)

G_END_DECLS
