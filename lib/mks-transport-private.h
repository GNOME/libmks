/* mks-transport-private.h
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

#include "mks-transport.h"

G_BEGIN_DECLS

typedef struct _MksTransportObserver MksTransportObserver;

struct _MksTransportObserver
{
  gpointer user_data;

  void (*name_changed)      (gpointer      user_data,
                             const char   *name);
  void (*uuid_changed)      (gpointer      user_data,
                             const char   *uuid);
  void (*clipboard_changed) (gpointer      user_data,
                             MksClipboard *clipboard);
  void (*device_added)      (gpointer      user_data,
                             MksDevice    *device);
  void (*device_removed)    (gpointer      user_data,
                             MksDevice    *device);
};

struct _MksTransport
{
  GObject parent_instance;

  GPtrArray    *observers;
  MksClipboard *clipboard;
  char         *name;
  char         *uuid;
};

struct _MksTransportClass
{
  GObjectClass parent_class;

  DexFuture *(*start)           (MksTransport               *self);
  void       (*add_observer)    (MksTransport               *self,
                                 const MksTransportObserver *observer);
  void       (*remove_observer) (MksTransport               *self,
                                 const MksTransportObserver *observer);
};

DexFuture *_mks_transport_start               (MksTransport               *self);
void       _mks_transport_add_observer        (MksTransport               *self,
                                               const MksTransportObserver *observer);
void       _mks_transport_remove_observer     (MksTransport               *self,
                                               const MksTransportObserver *observer);
void       _mks_transport_set_name            (MksTransport               *self,
                                               const char                 *name);
void       _mks_transport_set_uuid            (MksTransport               *self,
                                               const char                 *uuid);
void       _mks_transport_set_clipboard       (MksTransport               *self,
                                               MksClipboard               *clipboard);
void       _mks_transport_emit_device_added   (MksTransport               *self,
                                               MksDevice                  *device);
void       _mks_transport_emit_device_removed (MksTransport               *self,
                                               MksDevice                  *device);

G_END_DECLS
