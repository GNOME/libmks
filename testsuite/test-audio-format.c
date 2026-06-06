/* test-audio-format.c
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

#include <libmks.h>

static void
test_mks_audio_format_gst_caps (void)
{
  g_autoptr(MksAudioFormat) format = NULL;
  g_autoptr(GstCaps) caps = NULL;
  g_autofree char *gst_format = NULL;
  GstStructure *structure;
  int rate = 0;
  int channels = 0;

  format = mks_audio_format_new (16, TRUE, FALSE, 48000, 2, 4, 192000, FALSE);
  gst_format = mks_audio_format_dup_gstreamer_format (format);
  caps = mks_audio_format_to_gst_caps (format);

  g_assert_cmpstr (gst_format, ==, "S16LE");
  g_assert_nonnull (caps);
  g_assert_cmpuint (gst_caps_get_size (caps), ==, 1);

  structure = gst_caps_get_structure (caps, 0);
  g_assert_cmpstr (gst_structure_get_name (structure), ==, "audio/x-raw");
  g_assert_cmpstr (gst_structure_get_string (structure, "format"), ==, "S16LE");
  g_assert_true (gst_structure_get_int (structure, "rate", &rate));
  g_assert_true (gst_structure_get_int (structure, "channels", &channels));
  g_assert_cmpint (rate, ==, 48000);
  g_assert_cmpint (channels, ==, 2);
}

static void
init_tests (void)
{
  g_test_add_func ("/Mks/AudioFormat/gst-caps", test_mks_audio_format_gst_caps);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  init_tests ();

  return g_test_run ();
}
