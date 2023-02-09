/*
 * mks-read-only-list-model-private.h
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

G_BEGIN_DECLS

#define MKS_TYPE_READ_ONLY_LIST_MODEL (mks_read_only_list_model_get_type())

G_DECLARE_FINAL_TYPE (MksReadOnlyListModel, mks_read_only_list_model, MKS, READ_ONLY_LIST_MODEL, GObject)

GListModel *mks_read_only_list_model_new (GListModel *model);

G_END_DECLS
