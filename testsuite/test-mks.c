/*
 * test-mks.c
 *
 * Copyright 2023 Sandro Bonazzola
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

#include <libmks.h>

static void
test_mks_init (void)
{
  mks_init ();
}

static void
test_mks_get_major_version (void)
{
  int version = mks_get_major_version ();

  g_assert_cmpint (version, ==, MKS_MAJOR_VERSION);
}

static void
test_mks_get_minor_version (void)
{
  int version = mks_get_minor_version ();

  g_assert_cmpint (version, ==, MKS_MINOR_VERSION);
}

static void
test_mks_get_micro_version (void)
{
  int version = mks_get_micro_version ();

  g_assert_cmpint (version, ==, MKS_MICRO_VERSION);
}

static void
test_mks_clipboard_content (void)
{
  static const char data[] = "hello";
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GBytes) copy = NULL;
  g_autoptr(MksClipboardContent) content = NULL;
  gconstpointer copy_data;
  gsize copy_size;

  bytes = g_bytes_new_static (data, sizeof data - 1);
  content = mks_clipboard_content_new ("text/plain", bytes);

  g_assert_cmpstr (mks_clipboard_content_get_mime_type (content), ==, "text/plain");

  copy = mks_clipboard_content_ref_bytes (content);
  copy_data = g_bytes_get_data (copy, &copy_size);

  g_assert_cmpmem (copy_data, copy_size, data, sizeof data - 1);
}

static void
test_mks_add_tests (void)
{
  g_test_add_func ("/Mks/init", test_mks_init);
  g_test_add_func ("/Mks/version/major", test_mks_get_major_version);
  g_test_add_func ("/Mks/version/minor", test_mks_get_minor_version);
  g_test_add_func ("/Mks/version/micro", test_mks_get_micro_version);
  g_test_add_func ("/Mks/clipboard/content", test_mks_clipboard_content);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  test_mks_add_tests ();

  return g_test_run ();
}
