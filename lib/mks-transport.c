/* mks-transport.c
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

#include "config.h"

#include "mks-transport-private.h"
#include "mks-clipboard.h"
#include "mks-device.h"

/**
 * MksTransport:
 *
 * A connection transport for a session.
 */

G_DEFINE_ABSTRACT_TYPE (MksTransport, mks_transport, G_TYPE_OBJECT)

static DexFuture *
mks_transport_real_start (MksTransport *self)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "%s does not implement start",
                                G_OBJECT_TYPE_NAME (self));
}

static void
mks_transport_real_add_observer (MksTransport               *self,
                                 const MksTransportObserver *observer)
{
  g_assert (MKS_IS_TRANSPORT (self));
  g_assert (observer != NULL);

  if (g_ptr_array_find (self->observers, observer, NULL))
    return;

  g_ptr_array_add (self->observers, (gpointer)observer);

  if (observer->name_changed != NULL)
    observer->name_changed (observer->user_data, self->name);

  if (observer->uuid_changed != NULL)
    observer->uuid_changed (observer->user_data, self->uuid);

  if (observer->clipboard_changed != NULL)
    observer->clipboard_changed (observer->user_data, self->clipboard);
}

static void
mks_transport_real_remove_observer (MksTransport               *self,
                                    const MksTransportObserver *observer)
{
  g_assert (MKS_IS_TRANSPORT (self));
  g_assert (observer != NULL);

  g_ptr_array_remove (self->observers, (gpointer)observer);
}

static void
mks_transport_finalize (GObject *object)
{
  MksTransport *self = (MksTransport *)object;

  g_clear_pointer (&self->observers, g_ptr_array_unref);
  g_clear_object (&self->clipboard);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (mks_transport_parent_class)->finalize (object);
}

static void
mks_transport_class_init (MksTransportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mks_transport_finalize;

  klass->start = mks_transport_real_start;
  klass->add_observer = mks_transport_real_add_observer;
  klass->remove_observer = mks_transport_real_remove_observer;
}

static void
mks_transport_init (MksTransport *self)
{
  self->observers = g_ptr_array_new ();
}

DexFuture *
_mks_transport_start (MksTransport *self)
{
  g_return_val_if_fail (MKS_IS_TRANSPORT (self), NULL);

  return MKS_TRANSPORT_GET_CLASS (self)->start (self);
}

void
_mks_transport_add_observer (MksTransport               *self,
                             const MksTransportObserver *observer)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));
  g_return_if_fail (observer != NULL);

  MKS_TRANSPORT_GET_CLASS (self)->add_observer (self, observer);
}

void
_mks_transport_remove_observer (MksTransport               *self,
                                const MksTransportObserver *observer)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));
  g_return_if_fail (observer != NULL);

  MKS_TRANSPORT_GET_CLASS (self)->remove_observer (self, observer);
}

void
_mks_transport_set_name (MksTransport *self,
                         const char   *name)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));

  if (!g_set_str (&self->name, name))
    return;

  for (guint i = 0; i < self->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (self->observers, i);

      if (observer->name_changed != NULL)
        observer->name_changed (observer->user_data, self->name);
    }
}

void
_mks_transport_set_uuid (MksTransport *self,
                         const char   *uuid)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));

  if (!g_set_str (&self->uuid, uuid))
    return;

  for (guint i = 0; i < self->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (self->observers, i);

      if (observer->uuid_changed != NULL)
        observer->uuid_changed (observer->user_data, self->uuid);
    }
}

void
_mks_transport_set_clipboard (MksTransport *self,
                              MksClipboard *clipboard)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));
  g_return_if_fail (!clipboard || MKS_IS_CLIPBOARD (clipboard));

  if (!g_set_object (&self->clipboard, clipboard))
    return;

  for (guint i = 0; i < self->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (self->observers, i);

      if (observer->clipboard_changed != NULL)
        observer->clipboard_changed (observer->user_data, self->clipboard);
    }
}

void
_mks_transport_emit_device_added (MksTransport *self,
                                  MksDevice    *device)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));
  g_return_if_fail (MKS_IS_DEVICE (device));

  for (guint i = 0; i < self->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (self->observers, i);

      if (observer->device_added != NULL)
        observer->device_added (observer->user_data, device);
    }
}

void
_mks_transport_emit_device_removed (MksTransport *self,
                                    MksDevice    *device)
{
  g_return_if_fail (MKS_IS_TRANSPORT (self));
  g_return_if_fail (MKS_IS_DEVICE (device));

  for (guint i = 0; i < self->observers->len; i++)
    {
      const MksTransportObserver *observer = g_ptr_array_index (self->observers, i);

      if (observer->device_removed != NULL)
        observer->device_removed (observer->user_data, device);
    }
}
