/* mks-clipboard-redirector.h
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

#if !defined(MKS_INSIDE) && !defined(MKS_COMPILATION)
# error "Only <libmks.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include "mks-clipboard.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_CLIPBOARD_REDIRECTOR (mks_clipboard_redirector_get_type())

typedef enum _MksClipboardRedirectorSelections
{
  MKS_CLIPBOARD_REDIRECTOR_SELECTION_CLIPBOARD = 1 << 0,
  MKS_CLIPBOARD_REDIRECTOR_SELECTION_PRIMARY   = 1 << 1,
} MksClipboardRedirectorSelections;

MKS_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (MksClipboardRedirector, mks_clipboard_redirector, MKS, CLIPBOARD_REDIRECTOR, GObject)

MKS_AVAILABLE_IN_ALL
MksClipboardRedirector           *mks_clipboard_redirector_new            (MksClipboard                     *clipboard,
                                                                           GdkDisplay                       *display);
MKS_AVAILABLE_IN_ALL
MksClipboard                     *mks_clipboard_redirector_get_clipboard  (MksClipboardRedirector           *self);
MKS_AVAILABLE_IN_ALL
GdkDisplay                       *mks_clipboard_redirector_get_display    (MksClipboardRedirector           *self);
MKS_AVAILABLE_IN_ALL
gboolean                          mks_clipboard_redirector_get_enabled    (MksClipboardRedirector           *self);
MKS_AVAILABLE_IN_ALL
void                              mks_clipboard_redirector_set_enabled    (MksClipboardRedirector           *self,
                                                                           gboolean                          enabled);
MKS_AVAILABLE_IN_ALL
MksClipboardRedirectorSelections  mks_clipboard_redirector_get_selections (MksClipboardRedirector           *self);
MKS_AVAILABLE_IN_ALL
void                              mks_clipboard_redirector_set_selections (MksClipboardRedirector           *self,
                                                                           MksClipboardRedirectorSelections  selections);

G_END_DECLS
