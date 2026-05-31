/*
 * mks-microphone.c
 *
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

#include "config.h"

#include <glib-unix.h>
#include <gst/app/gstappsink.h>

#include "mks-device-private.h"
#include "mks-microphone.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

struct _MksMicrophone
{
  MksDevice parent_instance;

  MksQemuAudio *audio;
  MksQemuAudioInListener *listener;
  GDBusConnection *connection;
  GHashTable *streams;
  guint muted : 1;
};

struct _MksMicrophoneClass
{
  MksDeviceClass parent_class;
};

G_DEFINE_FINAL_TYPE (MksMicrophone, mks_microphone, MKS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MUTED,
  N_PROPS
};

enum {
  STREAM_ADDED,
  STREAM_REMOVED,
  STREAM_ENABLED,
  VOLUME_CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

typedef struct _MksMicrophoneStream
{
  guint64         id;
  MksAudioFormat *format;
  GQueue          queue;
  gsize           queued;
  gsize           offset;
  guint           enabled : 1;
  guint           muted : 1;
} MksMicrophoneStream;

typedef struct _MksMicrophoneGstSink
{
  MksMicrophone *microphone;
  guint64        stream_id;
  gulong         stream_added_handler;
} MksMicrophoneGstSink;

static void
mks_microphone_stream_free (gpointer data)
{
  MksMicrophoneStream *stream = data;

  if (stream == NULL)
    return;

  g_clear_pointer (&stream->format, mks_audio_format_free);
  g_queue_clear_full (&stream->queue, (GDestroyNotify)g_bytes_unref);
  g_free (stream);
}

static void
mks_microphone_gst_sink_free (gpointer data)
{
  MksMicrophoneGstSink *sink = data;

  if (sink == NULL)
    return;

  if (sink->microphone != NULL)
    {
      g_clear_signal_handler (&sink->stream_added_handler, sink->microphone);
      g_clear_object (&sink->microphone);
    }

  g_free (sink);
}

static MksMicrophoneStream *
mks_microphone_lookup_stream (MksMicrophone *self,
                              guint64        id)
{
  g_assert (MKS_IS_MICROPHONE (self));

  return g_hash_table_lookup (self->streams, &id);
}

static MksMicrophoneStream *
mks_microphone_ensure_stream (MksMicrophone  *self,
                              guint64         id,
                              MksAudioFormat *format)
{
  MksMicrophoneStream *stream;
  guint64 *stream_id;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (format != NULL);

  if ((stream = mks_microphone_lookup_stream (self, id)))
    {
      g_clear_pointer (&stream->format, mks_audio_format_free);
      stream->format = mks_audio_format_copy (format);
      return stream;
    }

  stream = g_new0 (MksMicrophoneStream, 1);
  stream->id = id;
  stream->format = mks_audio_format_copy (format);
  stream_id = g_memdup2 (&id, sizeof id);
  g_hash_table_insert (self->streams, stream_id, stream);

  return stream;
}

static GBytes *
mks_microphone_stream_read (MksMicrophoneStream *stream,
                            guint64              size)
{
  g_autoptr(GByteArray) bytes = NULL;

  g_assert (stream != NULL);

  bytes = g_byte_array_sized_new (MIN (size, G_MAXUINT));

  while (size > 0 && !g_queue_is_empty (&stream->queue))
    {
      GBytes *head = g_queue_peek_head (&stream->queue);
      gconstpointer data;
      gsize available;
      gsize copy;

      data = g_bytes_get_data (head, &available);
      available -= stream->offset;
      copy = MIN (available, size);

      g_byte_array_append (bytes, ((const guint8 *)data) + stream->offset, copy);

      stream->offset += copy;
      stream->queued -= copy;
      size -= copy;

      if (stream->offset == g_bytes_get_size (head))
        {
          g_bytes_unref (g_queue_pop_head (&stream->queue));
          stream->offset = 0;
        }
    }

  return g_byte_array_free_to_bytes (g_steal_pointer (&bytes));
}

static gboolean
mks_microphone_handle_init (MksMicrophone         *self,
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
  g_autoptr(MksAudioFormat) format = NULL;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  format = mks_audio_format_new (bits,
                                 is_signed,
                                 is_float,
                                 freq,
                                 nchannels,
                                 bytes_per_frame,
                                 bytes_per_second,
                                 be);

  mks_microphone_ensure_stream (self, id, format);
  g_signal_emit (self, signals [STREAM_ADDED], 0, id, format);
  mks_qemu_audio_in_listener_complete_init (self->listener, invocation);

  return TRUE;
}

static gboolean
mks_microphone_handle_fini (MksMicrophone         *self,
                            GDBusMethodInvocation *invocation,
                            guint64                id)
{
  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  g_hash_table_remove (self->streams, &id);
  g_signal_emit (self, signals [STREAM_REMOVED], 0, id);
  mks_qemu_audio_in_listener_complete_fini (self->listener, invocation);

  return TRUE;
}

static gboolean
mks_microphone_handle_set_enabled (MksMicrophone         *self,
                                   GDBusMethodInvocation *invocation,
                                   guint64                id,
                                   gboolean               enabled)
{
  MksMicrophoneStream *stream;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if ((stream = mks_microphone_lookup_stream (self, id)))
    stream->enabled = !!enabled;

  g_signal_emit (self, signals [STREAM_ENABLED], 0, id, enabled);
  mks_qemu_audio_in_listener_complete_set_enabled (self->listener, invocation);

  return TRUE;
}

static gboolean
mks_microphone_handle_set_volume (MksMicrophone         *self,
                                  GDBusMethodInvocation *invocation,
                                  guint64                id,
                                  gboolean               mute,
                                  GVariant              *volume)
{
  MksMicrophoneStream *stream;
  g_autoptr(GBytes) bytes = NULL;
  gconstpointer element_data;
  gsize n_elements;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  element_data = g_variant_get_fixed_array (volume, &n_elements, sizeof (guchar));
  bytes = g_bytes_new (element_data, n_elements);

  if ((stream = mks_microphone_lookup_stream (self, id)))
    stream->muted = !!mute;

  g_signal_emit (self, signals [VOLUME_CHANGED], 0, id, mute, bytes);
  mks_qemu_audio_in_listener_complete_set_volume (self->listener, invocation);

  return TRUE;
}

static gboolean
mks_microphone_handle_read (MksMicrophone         *self,
                            GDBusMethodInvocation *invocation,
                            guint64                id,
                            guint64                size)
{
  MksMicrophoneStream *stream;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GVariant) data = NULL;
  gconstpointer bytes_data;
  gsize bytes_size;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (self->muted ||
      !(stream = mks_microphone_lookup_stream (self, id)) ||
      stream->muted ||
      !stream->enabled)
    bytes = g_bytes_new_static ("", 0);
  else
    bytes = mks_microphone_stream_read (stream, size);

  bytes_data = g_bytes_get_data (bytes, &bytes_size);
  data = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                    bytes_data,
                                    bytes_size,
                                    sizeof (guchar));
  mks_qemu_audio_in_listener_complete_read (self->listener,
                                            invocation,
                                            g_steal_pointer (&data));

  return TRUE;
}

static DexFuture *
mks_microphone_register_cb (DexFuture *future,
                            gpointer   user_data)
{
  MksMicrophone *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_MICROPHONE (self));

  if (!dex_future_get_value (future, &error))
    g_warning ("Failed to register audio in listener: %s", error->message);

  return dex_future_new_true ();
}

static DexFuture *
mks_microphone_connection_cb (DexFuture *future,
                              gpointer   user_data)
{
  MksMicrophone *self = user_data;
  MksSocketpairConnection *socketpair;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  g_autofd int peer_fd = -1;
  gint64 begin_time;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_MICROPHONE (self));

  if (!(value = dex_future_get_value (future, &error)))
    {
      g_warning ("Failed to create socketpair D-Bus connection: %s", error->message);
      return dex_future_new_true ();
    }

  socketpair = g_value_get_boxed (value);
  peer_fd = mks_socketpair_connection_steal_fd (socketpair);
  g_set_object (&self->connection, socketpair->connection);

  self->listener = mks_qemu_audio_in_listener_skeleton_new ();

  g_signal_connect_object (self->listener,
                           "handle-init",
                           G_CALLBACK (mks_microphone_handle_init),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-fini",
                           G_CALLBACK (mks_microphone_handle_fini),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-set-enabled",
                           G_CALLBACK (mks_microphone_handle_set_enabled),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-set-volume",
                           G_CALLBACK (mks_microphone_handle_set_volume),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->listener,
                           "handle-read",
                           G_CALLBACK (mks_microphone_handle_read),
                           self,
                           G_CONNECT_SWAPPED);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->listener),
                                         self->connection,
                                         "/org/qemu/Display1/AudioInListener",
                                         &error))
    {
      g_warning ("Failed to export AudioInListener on D-Bus connection: %s",
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
                                                                                            "RegisterInListener",
                                                                                            g_variant_new ("(h)", 0),
                                                                                            G_VARIANT_TYPE ("()"),
                                                                                            G_DBUS_CALL_FLAGS_NONE,
                                                                                            -1,
                                                                                            fd_list),
                                                begin_time,
                                                "microphone.register-in-listener"),
                             mks_microphone_register_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static gboolean
mks_microphone_setup (MksDevice     *device,
                      MksQemuObject *object)
{
  MksMicrophone *self = (MksMicrophone *)device;
  gint64 begin_time;

  g_assert (MKS_IS_MICROPHONE (self));
  g_assert (MKS_QEMU_IS_OBJECT (object));

  if (MKS_QEMU_IS_AUDIO (object))
    {
      g_set_object (&self->audio, MKS_QEMU_AUDIO (object));

      begin_time = MKS_TRACE_BEGIN_MARK ();
      dex_future_disown (dex_future_finally (mks_marked_future (mks_socketpair_connection_new (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                                                 begin_time,
                                                                 "microphone.socketpair-connection"),
                                             mks_microphone_connection_cb,
                                             g_object_ref (self),
                                             g_object_unref));

      return TRUE;
    }

  return FALSE;
}

static void
mks_microphone_dispose (GObject *object)
{
  MksMicrophone *self = (MksMicrophone *)object;

  if (self->listener != NULL)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->listener));
      g_clear_object (&self->listener);
    }

  g_clear_object (&self->connection);
  g_clear_object (&self->audio);
  g_clear_pointer (&self->streams, g_hash_table_unref);

  G_OBJECT_CLASS (mks_microphone_parent_class)->dispose (object);
}

static void
mks_microphone_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MksMicrophone *self = MKS_MICROPHONE (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      g_value_set_boolean (value, mks_microphone_get_muted (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_microphone_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  MksMicrophone *self = MKS_MICROPHONE (object);

  switch (prop_id)
    {
    case PROP_MUTED:
      mks_microphone_set_muted (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_microphone_class_init (MksMicrophoneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MksDeviceClass *device_class = MKS_DEVICE_CLASS (klass);

  object_class->dispose = mks_microphone_dispose;
  object_class->get_property = mks_microphone_get_property;
  object_class->set_property = mks_microphone_set_property;

  device_class->setup = mks_microphone_setup;

  properties [PROP_MUTED] =
    g_param_spec_boolean ("muted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [STREAM_ADDED] =
    g_signal_new ("stream-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_UINT64, MKS_TYPE_AUDIO_FORMAT);

  signals [STREAM_REMOVED] =
    g_signal_new ("stream-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_UINT64);

  signals [STREAM_ENABLED] =
    g_signal_new ("stream-enabled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_UINT64, G_TYPE_BOOLEAN);

  signals [VOLUME_CHANGED] =
    g_signal_new ("volume-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_UINT64, G_TYPE_BOOLEAN, G_TYPE_BYTES);
}

static void
mks_microphone_init (MksMicrophone *self)
{
  self->streams = g_hash_table_new_full (g_int64_hash,
                                         g_int64_equal,
                                         g_free,
                                         mks_microphone_stream_free);
}

/**
 * mks_microphone_get_muted:
 * @self: a `MksMicrophone`
 *
 * Gets if the microphone is muted.
 *
 * Returns: %TRUE if the microphone is muted
 */
gboolean
mks_microphone_get_muted (MksMicrophone *self)
{
  g_return_val_if_fail (MKS_IS_MICROPHONE (self), FALSE);

  return self->muted;
}

/**
 * mks_microphone_set_muted:
 * @self: a `MksMicrophone`
 * @muted: if the microphone should be muted
 *
 * Mute or un-mute the microphone.
 */
void
mks_microphone_set_muted (MksMicrophone *self,
                          gboolean       muted)
{
  g_return_if_fail (MKS_IS_MICROPHONE (self));

  muted = !!muted;

  if (self->muted != muted)
    {
      self->muted = muted;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MUTED]);
    }
}

/**
 * mks_microphone_dup_format:
 * @self: a `MksMicrophone`
 * @stream_id: a QEMU audio stream id
 *
 * Gets the PCM format for @stream_id.
 *
 * Returns: (transfer full) (nullable): a copy of the stream format
 */
MksAudioFormat *
mks_microphone_dup_format (MksMicrophone *self,
                           guint64        stream_id)
{
  MksMicrophoneStream *stream;

  g_return_val_if_fail (MKS_IS_MICROPHONE (self), NULL);

  if (!(stream = mks_microphone_lookup_stream (self, stream_id)))
    return NULL;

  return mks_audio_format_copy (stream->format);
}

/**
 * mks_microphone_queue_pcm:
 * @self: a `MksMicrophone`
 * @stream_id: a QEMU audio stream id
 * @bytes: (transfer full): PCM audio data matching the stream format
 *
 * Queues PCM audio data for QEMU to read from @stream_id.
 */
void
mks_microphone_queue_pcm (MksMicrophone *self,
                          guint64        stream_id,
                          GBytes        *bytes)
{
  MksMicrophoneStream *stream;
  g_autoptr(GBytes) hold = bytes;
  gsize size;

  g_return_if_fail (MKS_IS_MICROPHONE (self));
  g_return_if_fail (bytes != NULL);

  if (self->muted || !(stream = mks_microphone_lookup_stream (self, stream_id)))
    return;

  g_bytes_get_data (bytes, &size);
  if (size == 0)
    return;

  g_queue_push_tail (&stream->queue, g_steal_pointer (&hold));
  stream->queued += size;

  while (stream->queued > (4 * 1024 * 1024) &&
         !g_queue_is_empty (&stream->queue))
    {
      GBytes *dropped = g_queue_pop_head (&stream->queue);

      if (stream->offset > 0)
        {
          gsize dropped_size = g_bytes_get_size (dropped);

          stream->queued -= dropped_size - stream->offset;
          stream->offset = 0;
        }
      else
        stream->queued -= g_bytes_get_size (dropped);

      g_bytes_unref (dropped);
    }
}

static void
mks_microphone_gst_sink_update_caps (GstElement     *element,
                                     MksAudioFormat *format)
{
  g_autoptr(GstCaps) caps = NULL;

  g_assert (GST_IS_APP_SINK (element));
  g_assert (format != NULL);

  if ((caps = mks_audio_format_to_gst_caps (format)))
    gst_app_sink_set_caps (GST_APP_SINK (element), caps);
}

static void
mks_microphone_gst_sink_stream_added_cb (GstElement     *element,
                                         guint64         stream_id,
                                         MksAudioFormat *format,
                                         MksMicrophone  *microphone)
{
  MksMicrophoneGstSink *sink;

  g_assert (GST_IS_APP_SINK (element));
  g_assert (format != NULL);
  g_assert (MKS_IS_MICROPHONE (microphone));

  sink = g_object_get_data (G_OBJECT (element), "MksMicrophoneGstSink");

  if (sink != NULL && sink->stream_id == stream_id)
    mks_microphone_gst_sink_update_caps (element, format);
}

typedef struct _BufferBytes
{
  GstBuffer  *buffer;
  GstMapInfo  map;
} BufferBytes;

static void
buffer_bytes_free (gpointer data)
{
  BufferBytes *bb = data;

  gst_buffer_unmap (bb->buffer, &bb->map);
  gst_buffer_unref (bb->buffer);
  g_free (bb);
}

static GBytes *
buffer_to_bytes (GstBuffer *buffer)
{
  BufferBytes *bb = g_new0 (BufferBytes, 1);

  bb->buffer = gst_buffer_ref (buffer);

  if (!gst_buffer_map (bb->buffer, &bb->map, GST_MAP_READ))
    {
      gst_buffer_unref (bb->buffer);
      g_free (bb);
      return NULL;
    }

  return g_bytes_new_with_free_func (bb->map.data,
                                     bb->map.size,
                                     buffer_bytes_free,
                                     bb);
}

static GstFlowReturn
mks_microphone_gst_sink_new_sample_cb (GstAppSink *app_sink,
                                       gpointer    user_data)
{
  MksMicrophoneGstSink *sink = user_data;
  g_autoptr(GstSample) sample = NULL;
  g_autoptr(GBytes) bytes = NULL;
  GstBuffer *buffer;

  g_assert (GST_IS_APP_SINK (app_sink));
  g_assert (sink != NULL);
  g_assert (MKS_IS_MICROPHONE (sink->microphone));

  if (!(sample = gst_app_sink_pull_sample (app_sink)))
    return GST_FLOW_EOS;

  if ((buffer = gst_sample_get_buffer (sample)) &&
      (bytes = buffer_to_bytes (buffer)))
    mks_microphone_queue_pcm (sink->microphone,
                              sink->stream_id,
                              g_steal_pointer (&bytes));

  return GST_FLOW_OK;
}

/**
 * mks_microphone_create_gst_sink:
 * @self: a `MksMicrophone`
 * @stream_id: a QEMU audio stream id
 *
 * Creates a GStreamer `appsink` that queues PCM frames into @stream_id.
 *
 * Returns: (transfer floating): a new `GstAppSink`
 */
GstElement *
mks_microphone_create_gst_sink (MksMicrophone *self,
                                guint64        stream_id)
{
  g_autoptr(MksAudioFormat) format = NULL;
  MksMicrophoneGstSink *sink;
  GstElement *element;

  g_return_val_if_fail (MKS_IS_MICROPHONE (self), NULL);

  element = gst_element_factory_make ("appsink", NULL);
  g_return_val_if_fail (element != NULL, NULL);

  sink = g_new0 (MksMicrophoneGstSink, 1);
  sink->microphone = g_object_ref (self);
  sink->stream_id = stream_id;

  if ((format = mks_microphone_dup_format (self, stream_id)))
    mks_microphone_gst_sink_update_caps (element, format);

  sink->stream_added_handler =
    g_signal_connect_object (self,
                             "stream-added",
                             G_CALLBACK (mks_microphone_gst_sink_stream_added_cb),
                             element,
                             G_CONNECT_SWAPPED);

  g_object_set_data_full (G_OBJECT (element),
                          "MksMicrophoneGstSink",
                          sink,
                          mks_microphone_gst_sink_free);

  gst_app_sink_set_emit_signals (GST_APP_SINK (element), TRUE);
  g_signal_connect (element,
                    "new-sample",
                    G_CALLBACK (mks_microphone_gst_sink_new_sample_cb),
                    sink);

  return element;
}
