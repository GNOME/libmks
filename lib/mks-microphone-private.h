/* mks-microphone-private.h
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
#include "mks-microphone.h"

G_BEGIN_DECLS

struct _MksMicrophone
{
  MksDevice parent_instance;
};

struct _MksMicrophoneClass
{
  MksDeviceClass parent_class;

  gboolean        (*get_muted)       (MksMicrophone *self);
  void            (*set_muted)       (MksMicrophone *self,
                                      gboolean       muted);
  MksAudioFormat *(*dup_format)      (MksMicrophone *self,
                                      guint64        stream_id);
  void            (*queue_pcm)       (MksMicrophone *self,
                                      guint64        stream_id,
                                      GBytes        *bytes);
  GstElement     *(*create_gst_sink) (MksMicrophone *self,
                                      guint64        stream_id);
};

G_END_DECLS
