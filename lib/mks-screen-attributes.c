/*
 * mks-screen-attributes.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "mks-screen-attributes-private.h"

G_DEFINE_BOXED_TYPE (MksScreenAttributes,
                     mks_screen_attributes,
                     mks_screen_attributes_copy,
                     mks_screen_attributes_free)

/**
 * mks_screen_attributes_new:
 *
 * Creates a new #MksScreenAttributes.
 *
 * Returns: (transfer full): A newly created #MksScreenAttributes
 */
MksScreenAttributes *
mks_screen_attributes_new (void)
{
  return g_new0 (MksScreenAttributes, 1);
}

/**
 * mks_screen_attributes_copy:
 * @self: (nullable): a #MksScreenAttributes
 *
 * Makes a deep copy of a #MksScreenAttributes.
 *
 * Returns: (transfer full): A newly created #MksScreenAttributes with the same
 *   contents as @self. If @self is %NULL, %NULL is returned.
 */
MksScreenAttributes *
mks_screen_attributes_copy (MksScreenAttributes *self)
{
  if (self == NULL)
    return NULL;

  return g_memdup2 (self, sizeof *self);
}

/**
 * mks_screen_attributes_free:
 * @self: a #MksScreenAttributes
 *
 * Frees a #MksScreenAttributes allocated using mks_screen_attributes_new()
 * or mks_screen_attributes_copy().
 */
void
mks_screen_attributes_free (MksScreenAttributes *self)
{
  g_free (self);
}

/**
 * mks_screen_attributes_equal:
 * @self: a #MksScreenAttributes
 * @other: a #MksScreenAttributes
 *
 * Returns `true` if the two attributes are equal, `false` otherwise.
 */
gboolean
mks_screen_attributes_equal (MksScreenAttributes *self,
                             MksScreenAttributes *other)
{
  if (self == NULL || other == NULL)
    return FALSE;

  return (self->width == other->width &&
          self->height == other->height &&
          self->x_offset == other->x_offset &&
          self->y_offset == other->y_offset &&
          self->width_mm == other->width_mm &&
          self->height_mm == other->height_mm);
}

void
mks_screen_attributes_set_width_mm (MksScreenAttributes *self,
                                    guint16              width_mm)
{
  self->width_mm = width_mm;
}

void
mks_screen_attributes_set_height_mm (MksScreenAttributes *self,
                                     guint16              height_mm)
{
  self->height_mm = height_mm;
}

void
mks_screen_attributes_set_x_offset (MksScreenAttributes *self,
                                    int                  x_offset)
{
  self->x_offset = x_offset;
}

void
mks_screen_attributes_set_y_offset (MksScreenAttributes *self,
                                    int                  y_offset)
{
  self->y_offset = y_offset;
}

void
mks_screen_attributes_set_width (MksScreenAttributes *self,
                                 guint                width)
{
  self->width = width;
}

void
mks_screen_attributes_set_height (MksScreenAttributes *self,
                                  guint                height)
{
  self->height = height;
}
