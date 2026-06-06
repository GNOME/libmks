/* mks-speaker-private.h
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

#include "mks-device-private.h"
#include "mks-speaker.h"

G_BEGIN_DECLS

struct _MksSpeaker
{
  MksDevice parent_instance;
};

struct _MksSpeakerClass
{
  MksDeviceClass parent_class;

  gboolean        (*get_muted)           (MksSpeaker        *self);
  void            (*set_muted)           (MksSpeaker        *self,
                                          gboolean           muted);
  MksAudioFormat *(*dup_format)          (MksSpeaker        *self,
                                          guint64            stream_id);
  guint           (*add_pcm_observer)    (MksSpeaker        *self,
                                          MksSpeakerPcmFunc  callback,
                                          gpointer           user_data,
                                          GDestroyNotify     user_data_destroy);
  void            (*remove_pcm_observer) (MksSpeaker        *self,
                                          guint              observer_id);
  GstElement     *(*create_gst_source)   (MksSpeaker        *self,
                                          guint64            stream_id);
};

G_END_DECLS
