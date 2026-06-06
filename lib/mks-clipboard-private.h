/* mks-clipboard-private.h
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

#include "mks-clipboard.h"
#include "mks-qemu.h"
#include "mks-session.h"

G_BEGIN_DECLS

typedef DexFuture *(*MksClipboardReadFunc) (MksClipboard          *self,
                                            MksClipboardSelection  selection,
                                            const char * const    *mime_types,
                                            gpointer               user_data);

MksClipboard *_mks_clipboard_new           (MksSession            *session,
                                            MksQemuObject         *object);
void          _mks_clipboard_set_read_func (MksClipboard          *self,
                                            MksClipboardReadFunc   read_func,
                                            gpointer               user_data,
                                            GDestroyNotify         destroy);

G_END_DECLS
