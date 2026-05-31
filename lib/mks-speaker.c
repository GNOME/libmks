/*
 * mks-speaker.c
 *
 * Copyright 2023 Christian Hergert <christian@sourceandstack.com>
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

#include "config.h"

#include <glib/gstdio.h>

#include "mks-device-private.h"
#include "mks-qemu.h"
#include "mks-speaker.h"
#include "mks-util-private.h"

/**
 * MksSpeaker:
 * 
 * A virtualized QEMU speaker.
 */

struct _MksSpeaker
{
  MksDevice parent_instance;
  MksQemuAudio *audio;
  MksQemuAudioOutListener *listener;
  GDBusConnection *connection;
  guint muted : 1;
};

struct _MksSpeakerClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksSpeaker, mks_speaker, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MUTED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
mks_speaker_handle_init (MksSpeaker            *self,
                         GDBusMethodInvocation *invocation,
                         guint64                id,
                         guchar                 bits,
                         gboolean               is_signed,
                         gboolean               is_float,
                         guint                  freq,
                         guchar                 nchannels,
                         guint                  bytes_per_frame,
                         guint                  bytes_per_second,
                         gboolean               be)
{
  g_assert (MKS_IS_SPEAKER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  return FALSE;
}

static gboolean
mks_speaker_handle_fini (MksSpeaker            *self,
                         GDBusMethodInvocation *invocation,
                         guint64                id)
{
  g_assert (MKS_IS_SPEAKER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  return FALSE;
}

static gboolean
mks_speaker_handle_set_enabled (MksSpeaker            *self,
                                GDBusMethodInvocation *invocation,
                                guint64                id,
                                gboolean               enbled)
{
  g_assert (MKS_IS_SPEAKER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  return FALSE;
}

static gboolean
mks_speaker_handle_set_volume (MksSpeaker            *self,
                               GDBusMethodInvocation *invocation,
                               guint64                id,
                               gboolean               mute,
                               GVariant              *volume)
{
  g_assert (MKS_IS_SPEAKER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  return FALSE;
}

static gboolean
mks_speaker_handle_write (MksSpeaker            *self,
                          GDBusMethodInvocation *invocation,
                          guint64                id,
                          GVariant              *data)
{
  g_assert (MKS_IS_SPEAKER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  return FALSE;
}

static DexFuture *
mks_speaker_register_cb (DexFuture *future,
                         gpointer   user_data)
{
  MksSpeaker *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_SPEAKER (self));

  if (!dex_future_get_value (future, &error))
    g_warning ("Failed to register audio out listener: %s", error->message);

  return dex_future_new_true ();
}

static DexFuture *
mks_speaker_connection_cb (DexFuture *future,
                           gpointer   user_data)
{
  MksSpeaker *self = user_data;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  MksSocketpairConnection *socketpair;
  g_autofd int peer_fd = -1;
  gint64 begin_time;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_SPEAKER (self));

  if (!(value = dex_future_get_value (future, &error)))
    {
      g_warning ("Failed to create socketpair D-Bus connection: %s", error->message);
      return dex_future_new_true ();
    }

  socketpair = g_value_get_boxed (value);
  peer_fd = mks_socketpair_connection_steal_fd (socketpair);
  g_set_object (&self->connection, socketpair->connection);

  self->listener = mks_qemu_audio_out_listener_skeleton_new ();

  g_signal_connect_object (self->listener,
                           "handle-init",
                           G_CALLBACK (mks_speaker_handle_init),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-fini",
                           G_CALLBACK (mks_speaker_handle_fini),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-set-enabled",
                           G_CALLBACK (mks_speaker_handle_set_enabled),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-set-volume",
                           G_CALLBACK (mks_speaker_handle_set_volume),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-write",
                           G_CALLBACK (mks_speaker_handle_write),
                           self,
                           G_CONNECT_SWAPPED);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->listener),
                                         self->connection,
                                         "/org/qemu/Display1/AudioOutListener",
                                         &error))

    {
      g_warning ("Failed to export AudioOutListener on D-Bus connection: %s",
                 error->message);
      return dex_future_new_true ();
    }

  fd_list = g_unix_fd_list_new_from_array (&peer_fd, 1);
  peer_fd = -1;
  begin_time = MKS_TRACE_BEGIN_MARK ();

  return dex_future_finally (mks_marked_future (dex_dbus_connection_call_with_unix_fd_list (g_dbus_proxy_get_connection (G_DBUS_PROXY (self->audio)),
                                                                                            g_dbus_proxy_get_name (G_DBUS_PROXY (self->audio)),
                                                                                            g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->audio)),
                                                                                            "org.qemu.Display1.Audio",
                                                                                            "RegisterOutListener",
                                                                                            g_variant_new ("(h)", 0),
                                                                                            G_VARIANT_TYPE ("()"),
                                                                                            G_DBUS_CALL_FLAGS_NONE,
                                                                                            -1,
                                                                                            fd_list),
                                                begin_time,
                                                "speaker.register-out-listener"),
                             mks_speaker_register_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static gboolean
mks_speaker_setup (MksDevice     *device,
                   MksQemuObject *object)
{
  MksSpeaker *self = (MksSpeaker *)device;
  gint64 begin_time;

  g_assert (MKS_IS_SPEAKER (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  if (MKS_QEMU_IS_AUDIO (object))
    {
      g_set_object (&self->audio, MKS_QEMU_AUDIO (object));
      begin_time = MKS_TRACE_BEGIN_MARK ();
      dex_future_disown (dex_future_finally (mks_marked_future (mks_socketpair_connection_new (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                                                 begin_time,
                                                                 "speaker.socketpair-connection"),
                                             mks_speaker_connection_cb,
                                             g_object_ref (self),
                                             g_object_unref));
      return TRUE;
    }

  return FALSE;
}

static void
mks_speaker_dispose (GObject *object)
{
  MksSpeaker *self = (MksSpeaker *)object;

  if (self->listener != NULL)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->listener));
      g_clear_object (&self->listener);
    }

  g_clear_object (&self->connection);
  g_clear_object (&self->audio);

  G_OBJECT_CLASS (mks_speaker_parent_class)->dispose (object);
}

static void
mks_speaker_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksSpeaker *self = MKS_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      g_value_set_boolean (value, mks_speaker_get_muted (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_speaker_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MksSpeaker *self = MKS_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      mks_speaker_set_muted (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_speaker_class_init (MksSpeakerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_speaker_dispose;
  object_class->get_property = mks_speaker_get_property;
  object_class->set_property = mks_speaker_set_property;

  device_class->setup = mks_speaker_setup;

  /**
   * MksSpeaker:muted:
   *
   * If audio received from the instance is dropped and 
   * the remote sound device should attempt to be set as muted.
   */
  properties [PROP_MUTED] =
    g_param_spec_boolean ("muted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_speaker_init (MksSpeaker *self)
{
}

/**
 * mks_speaker_get_muted:
 * @self: a #MksSpeaker
 *
 * Gets if the speaker is muted.
 *
 * Returns: %TRUE if the #MksSpeaker is muted.
 */
gboolean
mks_speaker_get_muted (MksSpeaker *self)
{
  g_return_val_if_fail (MKS_IS_SPEAKER (self), FALSE);

  return self->muted;
}

/**
 * mks_speaker_set_muted:
 * @self: a #MksSpeaker
 * @muted: if the speaker should be muted
 *
 * Mute or un-mute the speaker.
 */
void
mks_speaker_set_muted (MksSpeaker *self,
                       gboolean    muted)
{
  g_return_if_fail (MKS_IS_SPEAKER (self));

  muted = !!muted;

  if (self->muted != muted)
    {
      self->muted = muted;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MUTED]);
    }
}
