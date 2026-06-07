/* mks-session.c
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
#include "mks-chardev.h"
#include "mks-clipboard-private.h"
#include "mks-device-private.h"
#include "mks-microphone.h"
#include "mks-read-only-list-model-private.h"
#include "mks-screen-private.h"
#include "mks-session.h"
#include "mks-speaker.h"
#include "mks-util-private.h"


/**
 * MksSession:
 *
 * Session connected to a transport
 *
 * The `MksSession` represents a connection to a transport. It contains devices
 * such as the mouse, keyboard, and screen which can be used with GTK.
 *
 * You may monitor [property@Mks.Session:devices] using [signal@Gio.ListModel::items-changed] to be
 * notified of changes to available devices in the session.
 * [property@Mks.Session:screens] contains only screen devices.
 *
 * # Connecting
 *
 * Create a [class@Mks.Transport] and then a `MksSession` with
 * [ctor@Mks.Session.new]. The `MksSession` instance exposes devices via the
 * [property@Mks.Session:devices] [iface@Gio.ListModel] and screens via the
 * [property@Mks.Session:screens] [iface@Gio.ListModel].
 *
 * # Creating Widgets
 *
 * You can bind [property@Mks.Session:primary-screen] to
 * [property@Mks.Display:screen] for the common single-screen case.
 */

static void     mks_session_async_initable_init_async  (GAsyncInitable       *async_initable,
                                                        int                   io_priority,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
static gboolean mks_session_async_initable_init_finish (GAsyncInitable       *async_initable,
                                                        GAsyncResult         *result,
                                                        GError              **error);

struct _MksSession
{
  GObject parent_instance;

  /* @transport is used to communicate with the underlying session provider. */
  MksTransport         *transport;
  MksTransportObserver  transport_observer;

  /* As devices are discovered, MksDevice-based objects are stored in @devices. */
  GListStore *devices;

  /* @devices_read_only is a #MksReadOnlyListModel of @devices so that we may
   * allow observation of the #GListModel, but without access to perturb it.
   * Such opaqueness provides us a bit more assurance we don't leak
   * implementation details into the API.
   */
  GListModel *devices_read_only;

  /* Screens are also stored separately so applications can ignore unrelated
   * devices when choosing which displays to present.
   */
  GListStore *screens;
  GListModel *screens_read_only;

  MksScreen *primary_screen;
  MksClipboard *clipboard;

  char *name;
  char *uuid;
};

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = mks_session_async_initable_init_async;
  iface->init_finish = mks_session_async_initable_init_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (MksSession, mks_session, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_TRANSPORT,
  PROP_DEVICES,
  PROP_SCREENS,
  PROP_PRIMARY_SCREEN,
  PROP_CLIPBOARD,
  PROP_NAME,
  PROP_UUID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static int
mks_session_compare_screens (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  MksScreen *screen_a = (MksScreen *)a;
  MksScreen *screen_b = (MksScreen *)b;
  const char *device_address_a;
  const char *device_address_b;
  const char *name_a;
  const char *name_b;
  guint number_a;
  guint number_b;
  int ret;

  g_assert (MKS_IS_SCREEN (screen_a));
  g_assert (MKS_IS_SCREEN (screen_b));

  if (screen_a == screen_b)
    return 0;

  device_address_a = mks_screen_get_device_address (screen_a);
  device_address_b = mks_screen_get_device_address (screen_b);

  if ((ret = g_strcmp0 (device_address_a, device_address_b)))
    return ret;

  number_a = mks_screen_get_number (screen_a);
  number_b = mks_screen_get_number (screen_b);

  if (number_a < number_b)
    return -1;

  if (number_a > number_b)
    return 1;

  name_a = mks_device_get_name (MKS_DEVICE (screen_a));
  name_b = mks_device_get_name (MKS_DEVICE (screen_b));

  return g_strcmp0 (name_a, name_b);
}

static gboolean
mks_session_find_screen (MksSession *self,
                         MksScreen  *screen,
                         guint      *position)
{
  GListModel *model;
  guint n_items;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  model = G_LIST_MODEL (self->screens);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksScreen) item = g_list_model_get_item (model, i);

      if (item == screen)
        {
          if (position != NULL)
            *position = i;

          return TRUE;
        }
    }

  return FALSE;
}

static guint
mks_session_get_screen_insert_position (MksSession *self,
                                        MksScreen  *screen)
{
  GListModel *model;
  guint n_items;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  model = G_LIST_MODEL (self->screens);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksScreen) item = g_list_model_get_item (model, i);

      if (mks_session_compare_screens (screen, item, NULL) < 0)
        return i;
    }

  return n_items;
}

static int
mks_session_score_primary_screen (MksSession *self,
                                  MksScreen  *screen)
{
  int score = 0;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  if (mks_screen_get_kind (screen) != MKS_SCREEN_KIND_GRAPHIC)
    return -1;

  if (mks_screen_get_width (screen) > 0 && mks_screen_get_height (screen) > 0)
    score += 100;

  if (mks_screen_get_number (screen) == 0)
    score += 10;

  return score;
}

static gboolean
mks_session_screen_is_primary_candidate (MksSession *self,
                                         MksScreen  *screen)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  return mks_session_find_screen (self, screen, NULL) &&
         mks_session_score_primary_screen (self, screen) >= 0;
}

static void
mks_session_set_primary_screen (MksSession *self,
                                MksScreen  *primary_screen)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (!primary_screen || MKS_IS_SCREEN (primary_screen));

  if (g_set_object (&self->primary_screen, primary_screen))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIMARY_SCREEN]);
}

static void
mks_session_update_primary_screen (MksSession *self)
{
  g_autoptr(MksScreen) best = NULL;
  GListModel *model;
  guint n_items;
  gint64 current_active_time = 0;
  gboolean current_is_candidate;
  int best_score = -1;
  gint64 best_active_time = 0;

  g_assert (MKS_IS_SESSION (self));

  current_is_candidate = self->primary_screen != NULL &&
                         mks_session_screen_is_primary_candidate (self, self->primary_screen);

  if (current_is_candidate)
    current_active_time = mks_screen_get_last_active_time (self->primary_screen);

  model = G_LIST_MODEL (self->screens);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksScreen) screen = g_list_model_get_item (model, i);
      gint64 active_time;
      int score;

      if ((score = mks_session_score_primary_screen (self, screen)) < 0)
        continue;

      active_time = mks_screen_get_last_active_time (screen);

      /* Once a primary screen exists, metadata alone should not steal focus
       * from it. A different screen needs newer display activity to replace it.
       */
      if (current_is_candidate &&
          screen != self->primary_screen &&
          active_time <= current_active_time)
        continue;

      if (active_time > best_active_time ||
          (active_time == best_active_time && score > best_score))
        {
          best_active_time = active_time;
          best_score = score;
          g_set_object (&best, screen);
        }
    }

  if (best == NULL && current_is_candidate)
    g_set_object (&best, self->primary_screen);

  mks_session_set_primary_screen (self, best);
}

static void
mks_session_resort_screen (MksSession *self,
                           MksScreen  *screen)
{
  g_autoptr(MksScreen) hold = NULL;
  guint old_position;
  guint new_position;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  if (!mks_session_find_screen (self, screen, &old_position))
    return;

  hold = g_object_ref (screen);
  g_list_store_remove (self->screens, old_position);

  new_position = mks_session_get_screen_insert_position (self, hold);
  g_list_store_insert (self->screens, new_position, hold);
}

static void
mks_session_screen_notify_cb (MksSession *self,
                              GParamSpec *pspec,
                              MksScreen  *screen)
{
  const char *name;

  g_assert (MKS_IS_SESSION (self));
  g_assert (pspec != NULL);
  g_assert (MKS_IS_SCREEN (screen));

  name = pspec->name;

  if (g_str_equal (name, "device-address") ||
      g_str_equal (name, "number"))
    mks_session_resort_screen (self, screen);

  if (g_str_equal (name, "device-address") ||
      g_str_equal (name, "height") ||
      g_str_equal (name, "kind") ||
      g_str_equal (name, "last-active-time") ||
      g_str_equal (name, "number") ||
      g_str_equal (name, "width"))
    mks_session_update_primary_screen (self);
}

static void
mks_session_add_screen (MksSession *self,
                        MksScreen  *screen)
{
  guint position;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  if (mks_session_find_screen (self, screen, NULL))
    return;

  g_signal_connect_object (screen,
                           "notify",
                           G_CALLBACK (mks_session_screen_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  position = mks_session_get_screen_insert_position (self, screen);
  g_list_store_insert (self->screens, position, screen);
  mks_session_update_primary_screen (self);
}

static void
mks_session_remove_screen (MksSession *self,
                           MksScreen  *screen)
{
  guint position;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_SCREEN (screen));

  if (!mks_session_find_screen (self, screen, &position))
    return;

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (mks_session_screen_notify_cb),
                                        self);

  g_list_store_remove (self->screens, position);
  mks_session_update_primary_screen (self);
}

static void
mks_session_add_device (MksSession *self,
                        MksDevice  *device)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (!device || MKS_IS_DEVICE (device));

  if (device == NULL)
    return;

  g_list_store_append (self->devices, device);

  if (MKS_IS_SCREEN (device))
    mks_session_add_screen (self, MKS_SCREEN (device));
}

static void
mks_session_remove_device (MksSession *self,
                           MksDevice  *device)
{
  guint n_items;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_DEVICE (device));

  if (MKS_IS_SCREEN (device))
    mks_session_remove_screen (self, MKS_SCREEN (device));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MksDevice) item = g_list_model_get_item (G_LIST_MODEL (self->devices), i);

      if (item == device)
        {
          g_list_store_remove (self->devices, i);
          return;
        }
    }
}

static void
mks_session_set_name (MksSession *self,
                      const char *name)
{
  g_assert (MKS_IS_SESSION (self));

  if (g_set_str (&self->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
}

static void
mks_session_set_uuid (MksSession *self,
                      const char *uuid)
{
  g_assert (MKS_IS_SESSION (self));

  if (g_set_str (&self->uuid, uuid))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_UUID]);
}

static void
mks_session_transport_name_changed_cb (gpointer    user_data,
                                       const char *name)
{
  MksSession *self = user_data;

  g_assert (MKS_IS_SESSION (self));

  mks_session_set_name (self, name);
}

static void
mks_session_transport_uuid_changed_cb (gpointer    user_data,
                                       const char *uuid)
{
  MksSession *self = user_data;

  g_assert (MKS_IS_SESSION (self));

  mks_session_set_uuid (self, uuid);
}

static void
mks_session_transport_clipboard_changed_cb (gpointer      user_data,
                                            MksClipboard *clipboard)
{
  MksSession *self = user_data;

  g_assert (MKS_IS_SESSION (self));
  g_assert (!clipboard || MKS_IS_CLIPBOARD (clipboard));

  if (g_set_object (&self->clipboard, clipboard))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIPBOARD]);
}

static void
mks_session_transport_device_added_cb (gpointer   user_data,
                                       MksDevice *device)
{
  MksSession *self = user_data;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_DEVICE (device));

  mks_session_add_device (self, device);
}

static void
mks_session_transport_device_removed_cb (gpointer   user_data,
                                         MksDevice *device)
{
  MksSession *self = user_data;

  g_assert (MKS_IS_SESSION (self));
  g_assert (MKS_IS_DEVICE (device));

  mks_session_remove_device (self, device);
}

static void
mks_session_set_transport (MksSession   *self,
                           MksTransport *transport)
{
  g_assert (MKS_IS_SESSION (self));
  g_assert (!transport || MKS_IS_TRANSPORT (transport));
  g_assert (self->transport == NULL);

  if (transport == NULL)
    g_critical ("%s created without a MksTransport, this cannot work.",
                G_OBJECT_TYPE_NAME (self));
  else
    {
      self->transport_observer.user_data = self;
      self->transport_observer.name_changed = mks_session_transport_name_changed_cb;
      self->transport_observer.uuid_changed = mks_session_transport_uuid_changed_cb;
      self->transport_observer.clipboard_changed = mks_session_transport_clipboard_changed_cb;
      self->transport_observer.device_added = mks_session_transport_device_added_cb;
      self->transport_observer.device_removed = mks_session_transport_device_removed_cb;

      self->transport = g_object_ref (transport);
      _mks_transport_add_observer (self->transport, &self->transport_observer);
    }
}

static void
mks_session_dispose (GObject *object)
{
  MksSession *self = (MksSession *)object;

  if (self->transport != NULL)
    _mks_transport_remove_observer (self->transport, &self->transport_observer);

  if (self->devices != NULL)
    g_list_store_remove_all (self->devices);

  if (self->screens != NULL)
    {
      GListModel *model = G_LIST_MODEL (self->screens);
      guint n_items = g_list_model_get_n_items (model);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(MksScreen) screen = g_list_model_get_item (model, i);

          g_signal_handlers_disconnect_by_func (screen,
                                                G_CALLBACK (mks_session_screen_notify_cb),
                                                self);
        }

      g_list_store_remove_all (self->screens);
    }

  g_clear_object (&self->primary_screen);
  g_clear_object (&self->clipboard);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (mks_session_parent_class)->dispose (object);
}

static void
mks_session_finalize (GObject *object)
{
  MksSession *self = (MksSession *)object;

  g_clear_object (&self->devices);
  g_clear_object (&self->devices_read_only);
  g_clear_object (&self->screens);
  g_clear_object (&self->screens_read_only);
  g_clear_object (&self->transport);

  G_OBJECT_CLASS (mks_session_parent_class)->finalize (object);
}

static void
mks_session_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MksSession *self = MKS_SESSION (object);

  switch (prop_id)
    {
    case PROP_TRANSPORT:
      g_value_set_object (value, self->transport);
      break;


    case PROP_DEVICES:
      g_value_take_object (value, mks_session_list_devices (self));
      break;

    case PROP_SCREENS:
      g_value_take_object (value, mks_session_list_screens (self));
      break;

    case PROP_PRIMARY_SCREEN:
      g_value_set_object (value, self->primary_screen);
      break;

    case PROP_CLIPBOARD:
      g_value_set_object (value, self->clipboard);
      break;

    case PROP_NAME:
      g_value_set_string (value, mks_session_get_name (self));
      break;

    case PROP_UUID:
      g_value_set_string (value, mks_session_get_uuid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_session_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MksSession *self = MKS_SESSION (object);

  switch (prop_id)
    {
    case PROP_TRANSPORT:
      mks_session_set_transport (self, g_value_get_object (value));
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_session_class_init (MksSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_session_dispose;
  object_class->finalize = mks_session_finalize;
  object_class->get_property = mks_session_get_property;
  object_class->set_property = mks_session_set_property;

  /**
   * MksSession:transport:
   *
   * The [class@Mks.Transport] that provides the session devices.
   */
  properties [PROP_TRANSPORT] =
    g_param_spec_object ("transport", NULL, NULL,
                         MKS_TYPE_TRANSPORT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:devices:
   *
   * A [iface@Gio.ListModel] of devices discovered by the transport.
   */
  properties [PROP_DEVICES] =
    g_param_spec_object ("devices", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:screens:
   *
   * A [iface@Gio.ListModel] of screens discovered by the transport.
   */
  properties [PROP_SCREENS] =
    g_param_spec_object ("screens", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:primary-screen:
   *
   * The best screen to present by default.
   */
  properties [PROP_PRIMARY_SCREEN] =
    g_param_spec_object ("primary-screen", NULL, NULL,
                         MKS_TYPE_SCREEN,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:clipboard:
   *
   * The [class@Mks.Clipboard] for the session, if clipboard redirection is
   * available.
   */
  properties [PROP_CLIPBOARD] =
    g_param_spec_object ("clipboard", NULL, NULL,
                         MKS_TYPE_CLIPBOARD,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:name:
   *
   * The session name as specified by the transport.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * MksSession:uuid:
   *
   * The session unique identifier specified by the transport.
   */
  properties [PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mks_session_init (MksSession *self)
{
  self->devices = g_list_store_new (MKS_TYPE_DEVICE);
  self->devices_read_only = mks_read_only_list_model_new (G_LIST_MODEL (self->devices));
  self->screens = g_list_store_new (MKS_TYPE_SCREEN);
  self->screens_read_only = mks_read_only_list_model_new (G_LIST_MODEL (self->screens));
}

static DexFuture *
mks_session_start (MksSession *self)
{
  g_assert (MKS_IS_SESSION (self));

  if (self->transport == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_CONNECTED,
                                  "Not connected");

  return _mks_transport_start (self->transport);
}

static void
mks_session_async_initable_init_async (GAsyncInitable      *async_initable,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  MksSession *self = (MksSession *)async_initable;
  g_autoptr(DexAsyncResult) result = NULL;

  g_assert (MKS_IS_SESSION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_set_priority (result, io_priority);
  dex_async_result_set_static_name (result, G_STRFUNC);
  dex_async_result_await (result, mks_session_start (self));
}

static gboolean
mks_session_async_initable_init_finish (GAsyncInitable  *async_initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  MksSession *self = (MksSession *)async_initable;

  g_assert (MKS_IS_SESSION (self));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_session_list_devices:
 * @self: a #MksSession
 *
 * Lists devices connected to the session.
 *
 * Returns: (transfer full): a #GListModel of #MksDevice
 */
GListModel *
mks_session_list_devices (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return g_object_ref (self->devices_read_only);
}

/**
 * mks_session_list_screens:
 * @self: a #MksSession
 *
 * Lists screens connected to the session.
 *
 * Returns: (transfer full): a #GListModel of #MksScreen
 */
GListModel *
mks_session_list_screens (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return g_object_ref (self->screens_read_only);
}

typedef struct _MksSessionNew
{
  MksSession *self;
} MksSessionNew;

static void
mks_session_new_free (MksSessionNew *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
mks_session_new_complete (DexFuture *future,
                          gpointer   user_data)
{
  MksSessionNew *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (state != NULL);
  g_assert (MKS_IS_SESSION (state->self));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_object (state->self);
}

/**
 * mks_session_new:
 * @transport: a #MksTransport
 *
 * Creates a #MksSession which communicates using @transport.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   #MksSession.
 */
DexFuture *
mks_session_new (MksTransport *transport)
{
  g_autoptr(MksSession) self = NULL;
  MksSessionNew *state;
  gint64 begin_time;

  dex_return_error_if_fail (MKS_IS_TRANSPORT (transport));

  self = g_object_new (MKS_TYPE_SESSION,
                       "transport", transport,
                       NULL);
  state = g_new0 (MksSessionNew, 1);
  state->self = g_object_ref (self);
  begin_time = MKS_TRACE_BEGIN_MARK ();

  return mks_marked_future (dex_future_then (mks_session_start (self),
                                             mks_session_new_complete,
                                             state,
                                             (GDestroyNotify) mks_session_new_free),
                            begin_time,
                            "session.new");
}





/**
 * mks_session_dup_transport:
 * @self: a #MksSession
 *
 * Gets the transport used for this session.
 *
 * Returns: (transfer full): a #MksTransport
 */
MksTransport *
mks_session_dup_transport (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return g_object_ref (self->transport);
}



/**
 * mks_session_get_uuid:
 * @self: a #MksSession
 *
 * Gets the unique identifier of the VM.
 */
const char *
mks_session_get_uuid (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->uuid;
}

/**
 * mks_session_get_name:
 * @self: a #MksSession
 *
 * Gets the name of the VM.
 */
const char *
mks_session_get_name (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->name;
}

/**
 * mks_session_dup_primary_screen:
 * @self: a #MksSession
 *
 * Gets the primary screen for the session.
 *
 * Returns: (transfer full) (nullable): a #MksScreen or %NULL
 */
MksScreen *
mks_session_dup_primary_screen (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->primary_screen ? g_object_ref (self->primary_screen) : NULL;
}

/**
 * mks_session_dup_clipboard:
 * @self: a `MksSession`
 *
 * Gets the clipboard for the session, if available.
 *
 * Returns: (transfer full) (nullable): an `MksClipboard`
 */
MksClipboard *
mks_session_dup_clipboard (MksSession *self)
{
  g_return_val_if_fail (MKS_IS_SESSION (self), NULL);

  return self->clipboard ? g_object_ref (self->clipboard) : NULL;
}
