/*
 * mks-audio-format.c
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

#include "config.h"

#include "mks-audio-format.h"

struct _MksAudioFormat
{
  guchar   bits;
  gboolean is_signed;
  gboolean is_float;
  guint    frequency;
  guchar   channels;
  guint    bytes_per_frame;
  guint    bytes_per_second;
  gboolean big_endian;
};

G_DEFINE_BOXED_TYPE (MksAudioFormat, mks_audio_format,
                     mks_audio_format_copy,
                     mks_audio_format_free)

/**
 * mks_audio_format_new:
 * @bits: bits per sample
 * @is_signed: if integer samples are signed
 * @is_float: if samples are IEEE floating point
 * @frequency: sample rate in Hz
 * @channels: number of interleaved channels
 * @bytes_per_frame: bytes per interleaved frame
 * @bytes_per_second: bytes per second
 * @big_endian: if samples are big endian
 *
 * Creates a new PCM audio format.
 *
 * Returns: (transfer full): a new `MksAudioFormat`
 */
MksAudioFormat *
mks_audio_format_new (guchar   bits,
                      gboolean is_signed,
                      gboolean is_float,
                      guint    frequency,
                      guchar   channels,
                      guint    bytes_per_frame,
                      guint    bytes_per_second,
                      gboolean big_endian)
{
  MksAudioFormat *self;

  self = g_new0 (MksAudioFormat, 1);
  self->bits = bits;
  self->is_signed = !!is_signed;
  self->is_float = !!is_float;
  self->frequency = frequency;
  self->channels = channels;
  self->bytes_per_frame = bytes_per_frame;
  self->bytes_per_second = bytes_per_second;
  self->big_endian = !!big_endian;

  return self;
}

/**
 * mks_audio_format_copy:
 * @self: a `MksAudioFormat`
 *
 * Copies an audio format.
 *
 * Returns: (transfer full): a copy of @self
 */
MksAudioFormat *
mks_audio_format_copy (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return mks_audio_format_new (self->bits,
                               self->is_signed,
                               self->is_float,
                               self->frequency,
                               self->channels,
                               self->bytes_per_frame,
                               self->bytes_per_second,
                               self->big_endian);
}

/**
 * mks_audio_format_free:
 * @self: (transfer full) (nullable): a `MksAudioFormat`
 *
 * Frees an audio format.
 */
void
mks_audio_format_free (MksAudioFormat *self)
{
  g_free (self);
}

guchar
mks_audio_format_get_bits (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->bits;
}

gboolean
mks_audio_format_get_signed (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->is_signed;
}

gboolean
mks_audio_format_get_float (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->is_float;
}

guint
mks_audio_format_get_frequency (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->frequency;
}

guchar
mks_audio_format_get_channels (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->channels;
}

guint
mks_audio_format_get_bytes_per_frame (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->bytes_per_frame;
}

guint
mks_audio_format_get_bytes_per_second (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->bytes_per_second;
}

gboolean
mks_audio_format_get_big_endian (MksAudioFormat *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->big_endian;
}

/**
 * mks_audio_format_dup_gstreamer_format:
 * @self: a `MksAudioFormat`
 *
 * Creates a GStreamer `audio/x-raw` format string for @self.
 *
 * Returns: (transfer full) (nullable): a format string such as `S16LE`
 */
char *
mks_audio_format_dup_gstreamer_format (MksAudioFormat *self)
{
  const char *endian;

  g_return_val_if_fail (self != NULL, NULL);

  endian = self->big_endian ? "BE" : "LE";

  if (self->is_float)
    {
      if (self->bits == 32 || self->bits == 64)
        return g_strdup_printf ("F%u%s", self->bits, endian);
    }
  else if (self->bits == 8)
    {
      return g_strdup (self->is_signed ? "S8" : "U8");
    }
  else if (self->bits == 16 || self->bits == 24 || self->bits == 32)
    {
      return g_strdup_printf ("%c%u%s", self->is_signed ? 'S' : 'U', self->bits, endian);
    }

  return NULL;
}

/**
 * mks_audio_format_to_gst_caps:
 * @self: a `MksAudioFormat`
 *
 * Creates GStreamer caps for the PCM format.
 *
 * Returns: (transfer full) (nullable): `audio/x-raw` caps
 */
GstCaps *
mks_audio_format_to_gst_caps (MksAudioFormat *self)
{
  g_autofree char *format = NULL;

  g_return_val_if_fail (self != NULL, NULL);

  if (!(format = mks_audio_format_dup_gstreamer_format (self)))
    return NULL;

  return gst_caps_new_simple ("audio/x-raw",
                              "format", G_TYPE_STRING, format,
                              "rate", G_TYPE_INT, (int)self->frequency,
                              "channels", G_TYPE_INT, (int)self->channels,
                              "layout", G_TYPE_STRING, "interleaved",
                              NULL);
}
