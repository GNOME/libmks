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
test_mks_add_tests (void)
{
  g_test_add_func ("/Mks/init", test_mks_init);
  g_test_add_func ("/Mks/version/major", test_mks_get_major_version);
  g_test_add_func ("/Mks/version/minor", test_mks_get_minor_version);
  g_test_add_func ("/Mks/version/micro", test_mks_get_micro_version);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  test_mks_add_tests ();

  return g_test_run ();
}
