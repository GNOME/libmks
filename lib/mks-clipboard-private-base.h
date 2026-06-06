/* mks-clipboard-private-base.h
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

#include "mks-clipboard-private.h"

G_BEGIN_DECLS

struct _MksClipboard
{
  GObject parent_instance;
};

struct _MksClipboardClass
{
  GObjectClass parent_class;

  gboolean            (*get_registered) (MksClipboard          *self);
  MksClipboardOwner   (*get_owner)      (MksClipboard          *self,
                                         MksClipboardSelection  selection);
  char              **(*dup_mime_types) (MksClipboard          *self,
                                         MksClipboardSelection  selection);
  DexFuture          *(*register_)      (MksClipboard          *self);
  DexFuture          *(*unregister)     (MksClipboard          *self);
  DexFuture          *(*claim)          (MksClipboard          *self,
                                         MksClipboardSelection  selection,
                                         const char * const    *mime_types);
  DexFuture          *(*release)        (MksClipboard          *self,
                                         MksClipboardSelection  selection);
  DexFuture          *(*request)        (MksClipboard          *self,
                                         MksClipboardSelection  selection,
                                         const char * const    *mime_types);
  void                (*set_read_func)  (MksClipboard          *self,
                                         MksClipboardReadFunc   read_func,
                                         gpointer               user_data,
                                         GDestroyNotify         destroy);
};

G_END_DECLS
