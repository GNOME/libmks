/* mks-speaker.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "mks-device.h"

G_BEGIN_DECLS

#define MKS_TYPE_SPEAKER            (mks_speaker_get_type ())
#define MKS_SPEAKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_SPEAKER, MksSpeaker))
#define MKS_SPEAKER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MKS_TYPE_SPEAKER, MksSpeaker const))
#define MKS_SPEAKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MKS_TYPE_SPEAKER, MksSpeakerClass))
#define MKS_IS_SPEAKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKS_TYPE_SPEAKER))
#define MKS_IS_SPEAKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MKS_TYPE_SPEAKER))
#define MKS_SPEAKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MKS_TYPE_SPEAKER, MksSpeakerClass))

typedef struct _MksSpeakerClass MksSpeakerClass;

MKS_AVAILABLE_IN_ALL
GType    mks_speaker_get_type  (void) G_GNUC_CONST;
MKS_AVAILABLE_IN_ALL
gboolean mks_speaker_get_muted (MksSpeaker *self);
MKS_AVAILABLE_IN_ALL
void     mks_speaker_set_muted (MksSpeaker *self,
                                gboolean    muted);

G_END_DECLS
