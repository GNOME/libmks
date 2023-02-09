/*
 * mks-session.h
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

#pragma once

#include <gio/gio.h>

#include "mks-types.h"
#include "mks-version-macros.h"

G_BEGIN_DECLS

#define MKS_TYPE_SESSION (mks_session_get_type())

MKS_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (MksSession, mks_session, MKS, SESSION, GObject)

MKS_AVAILABLE_IN_ALL
void             mks_session_new_for_connection        (GDBusConnection      *connection,
                                                        int                   io_priority,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
MKS_AVAILABLE_IN_ALL
MksSession      *mks_session_new_for_connection_finish (GAsyncResult         *result,
                                                        GError              **error);
MKS_AVAILABLE_IN_ALL
MksSession      *mks_session_new_for_connection_sync   (GDBusConnection      *connection,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
MKS_AVAILABLE_IN_ALL
GDBusConnection *mks_session_get_connection            (MksSession           *self);
MKS_AVAILABLE_IN_ALL
GListModel      *mks_session_get_devices               (MksSession           *self);
MKS_AVAILABLE_IN_ALL
const char      *mks_session_get_name                  (MksSession           *self);
MKS_AVAILABLE_IN_ALL
const char      *mks_session_get_uuid                  (MksSession           *self);

G_END_DECLS
