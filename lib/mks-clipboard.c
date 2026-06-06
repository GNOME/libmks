/* mks-clipboard.c
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

#include "mks-clipboard-private.h"
#include "mks-enums.h"
#include "mks-qemu.h"
#include "mks-util-private.h"

#define N_SELECTIONS 3

struct _MksClipboardContent
{
  int ref_count;
  char *mime_type;
  GBytes *bytes;
};

typedef struct
{
  MksClipboard *self;
  MksClipboardSelection selection;
  guint serial;
  char **mime_types;
} Claim;

struct _MksClipboard
{
  GObject parent_instance;

  MksSession *session;
  MksQemuObject *object;
  MksQemuClipboard *clipboard;
  MksQemuClipboard *skeleton;

  gboolean registered;
  guint serial[N_SELECTIONS];
  MksClipboardOwner owner[N_SELECTIONS];
  char **mime_types[N_SELECTIONS];

  MksClipboardReadFunc read_func;
  gpointer read_func_data;
  GDestroyNotify read_func_data_destroy;
};

G_DEFINE_FINAL_TYPE (MksClipboard, mks_clipboard, G_TYPE_OBJECT)
G_DEFINE_BOXED_TYPE (MksClipboardContent,
                     mks_clipboard_content,
                     mks_clipboard_content_ref,
                     mks_clipboard_content_unref)

enum {
  PROP_0,
  PROP_REGISTERED,
  N_PROPS
};

enum {
  OWNER_CHANGED,
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static gboolean
selection_is_valid (MksClipboardSelection selection)
{
  return selection >= MKS_CLIPBOARD_SELECTION_CLIPBOARD &&
         selection <= MKS_CLIPBOARD_SELECTION_SECONDARY;
}

static gboolean
mime_types_equal (const char * const *a,
                  const char * const *b)
{
  if (a == NULL || b == NULL)
    return a == b;

  return g_strv_equal (a, b);
}

static void
claim_free (Claim *claim)
{
  g_clear_object (&claim->self);
  g_clear_pointer (&claim->mime_types, g_strfreev);
  g_free (claim);
}

static void
mks_clipboard_set_registered (MksClipboard *self,
                              gboolean      registered)
{
  g_assert (MKS_IS_CLIPBOARD (self));

  registered = !!registered;

  if (self->registered != registered)
    {
      self->registered = registered;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REGISTERED]);
    }
}

static void
mks_clipboard_set_owner (MksClipboard          *self,
                         MksClipboardSelection  selection,
                         MksClipboardOwner      owner,
                         const char * const    *mime_types)
{
  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (selection_is_valid (selection));

  if (self->owner[selection] == owner &&
      mime_types_equal ((const char * const *) self->mime_types[selection], mime_types))
    return;

  self->owner[selection] = owner;
  g_strfreev (self->mime_types[selection]);
  self->mime_types[selection] = g_strdupv ((char **) mime_types);

  g_signal_emit (self, signals[OWNER_CHANGED], 0, selection);
  g_signal_emit (self, signals[CHANGED], 0, selection);
}

static gboolean
mks_clipboard_handle_register_cb (MksClipboard          *self,
                                  GDBusMethodInvocation *invocation,
                                  MksQemuClipboard      *skeleton)
{
  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_CLIPBOARD (skeleton));

  for (guint i = 0; i < N_SELECTIONS; i++)
    {
      self->serial[i] = 0;
      mks_clipboard_set_owner (self, i, MKS_CLIPBOARD_OWNER_NONE, NULL);
    }

  mks_clipboard_set_registered (self, TRUE);
  mks_qemu_clipboard_complete_register (skeleton, invocation);

  return TRUE;
}

static gboolean
mks_clipboard_handle_unregister_cb (MksClipboard          *self,
                                    GDBusMethodInvocation *invocation,
                                    MksQemuClipboard      *skeleton)
{
  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_CLIPBOARD (skeleton));

  for (guint i = 0; i < N_SELECTIONS; i++)
    mks_clipboard_set_owner (self, i, MKS_CLIPBOARD_OWNER_NONE, NULL);

  mks_clipboard_set_registered (self, FALSE);
  mks_qemu_clipboard_complete_unregister (skeleton, invocation);

  return TRUE;
}

static gboolean
mks_clipboard_handle_grab_cb (MksClipboard          *self,
                              GDBusMethodInvocation *invocation,
                              guint                  selection,
                              guint                  serial,
                              const char * const    *mime_types,
                              MksQemuClipboard      *skeleton)
{
  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_CLIPBOARD (skeleton));

  if (selection < N_SELECTIONS && serial > self->serial[selection])
    {
      self->serial[selection] = serial;
      mks_clipboard_set_owner (self, selection, MKS_CLIPBOARD_OWNER_REMOTE, mime_types);
    }

  mks_qemu_clipboard_complete_grab (skeleton, invocation);

  return TRUE;
}

static gboolean
mks_clipboard_handle_release_cb (MksClipboard          *self,
                                 GDBusMethodInvocation *invocation,
                                 guint                  selection,
                                 MksQemuClipboard      *skeleton)
{
  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_CLIPBOARD (skeleton));

  if (selection < N_SELECTIONS && self->owner[selection] == MKS_CLIPBOARD_OWNER_REMOTE)
    mks_clipboard_set_owner (self, selection, MKS_CLIPBOARD_OWNER_NONE, NULL);

  mks_qemu_clipboard_complete_release (skeleton, invocation);

  return TRUE;
}

static DexFuture *
mks_clipboard_handle_request_complete_cb (DexFuture *future,
                                          gpointer   user_data)
{
  GDBusMethodInvocation *invocation = user_data;
  g_autoptr(MksClipboardContent) content = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  GVariant *data;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (!(value = dex_future_get_value (future, &error)))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return dex_future_new_true ();
    }

  content = g_value_dup_boxed (value);
  bytes = mks_clipboard_content_ref_bytes (content);
  data = g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"), bytes, TRUE);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s@ay)",
                                                        mks_clipboard_content_get_mime_type (content),
                                                        data));

  return dex_future_new_true ();
}

static gboolean
mks_clipboard_handle_request_cb (MksClipboard          *self,
                                 GDBusMethodInvocation *invocation,
                                 guint                  selection,
                                 const char * const    *mime_types,
                                 MksQemuClipboard      *skeleton)
{
  DexFuture *future;

  g_assert (MKS_IS_CLIPBOARD (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (MKS_QEMU_IS_CLIPBOARD (skeleton));

  if (selection >= N_SELECTIONS || self->read_func == NULL)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_IO_ERROR,
                                             G_IO_ERROR_NOT_FOUND,
                                             "Clipboard content is not available");
      return TRUE;
    }

  future = self->read_func (self, selection, mime_types, self->read_func_data);
  dex_future_disown (dex_future_finally (future,
                                         mks_clipboard_handle_request_complete_cb,
                                         g_object_ref (invocation),
                                         g_object_unref));

  return TRUE;
}

static gboolean
mks_clipboard_export_skeleton (MksClipboard  *self,
                               GError       **error)
{
  g_assert (MKS_IS_CLIPBOARD (self));

  if (g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (self->skeleton)) != NULL)
    return TRUE;

  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                           mks_session_get_connection (self->session),
                                           g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->clipboard)),
                                           error);
}

static DexFuture *
mks_clipboard_complete_boolean (DexFuture *future,
                                gpointer   user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
mks_clipboard_register_complete_cb (DexFuture *future,
                                    gpointer   user_data)
{
  MksClipboard *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_CLIPBOARD (self));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < N_SELECTIONS; i++)
    {
      self->serial[i] = 0;
      mks_clipboard_set_owner (self, i, MKS_CLIPBOARD_OWNER_NONE, NULL);
    }

  mks_clipboard_set_registered (self, TRUE);

  return dex_future_new_true ();
}

static DexFuture *
mks_clipboard_unregister_complete_cb (DexFuture *future,
                                      gpointer   user_data)
{
  MksClipboard *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (MKS_IS_CLIPBOARD (self));

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < N_SELECTIONS; i++)
    mks_clipboard_set_owner (self, i, MKS_CLIPBOARD_OWNER_NONE, NULL);

  mks_clipboard_set_registered (self, FALSE);

  return dex_future_new_true ();
}

static DexFuture *
mks_clipboard_claim_complete_cb (DexFuture *future,
                                 gpointer   user_data)
{
  Claim *claim = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (claim != NULL);

  if (!dex_future_get_value (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  mks_clipboard_set_owner (claim->self,
                           claim->selection,
                           MKS_CLIPBOARD_OWNER_LOCAL,
                           (const char * const *) claim->mime_types);

  return dex_future_new_true ();
}

static DexFuture *
mks_clipboard_request_complete_cb (DexFuture *future,
                                   gpointer   user_data)
{
  g_autoptr(MksClipboardContent) content = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  MksQemuClipboardRequestResult *result;
  GValue boxed = G_VALUE_INIT;
  DexFuture *ret;

  g_assert (DEX_IS_FUTURE (future));

  if (!(value = dex_future_get_value (future, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  result = g_value_get_boxed (value);
  bytes = g_variant_get_data_as_bytes (result->data);
  content = mks_clipboard_content_new (result->reply_mime, bytes);

  g_value_init (&boxed, MKS_TYPE_CLIPBOARD_CONTENT);
  g_value_set_boxed (&boxed, content);
  ret = dex_future_new_for_value (&boxed);
  g_value_unset (&boxed);

  return ret;
}

static void
mks_clipboard_dispose (GObject *object)
{
  MksClipboard *self = (MksClipboard *)object;

  _mks_clipboard_set_read_func (self, NULL, NULL, NULL);

  if (self->skeleton != NULL)
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));

  g_clear_object (&self->skeleton);
  g_clear_object (&self->clipboard);
  g_clear_object (&self->object);
  g_clear_object (&self->session);

  for (guint i = 0; i < N_SELECTIONS; i++)
    g_clear_pointer (&self->mime_types[i], g_strfreev);

  G_OBJECT_CLASS (mks_clipboard_parent_class)->dispose (object);
}

static void
mks_clipboard_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MksClipboard *self = MKS_CLIPBOARD (object);

  switch (prop_id)
    {
    case PROP_REGISTERED:
      g_value_set_boolean (value, mks_clipboard_get_registered (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mks_clipboard_class_init (MksClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mks_clipboard_dispose;
  object_class->get_property = mks_clipboard_get_property;

  properties[PROP_REGISTERED] =
    g_param_spec_boolean ("registered", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[OWNER_CHANGED] =
    g_signal_new ("owner-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  MKS_TYPE_CLIPBOARD_SELECTION);

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  MKS_TYPE_CLIPBOARD_SELECTION);
}

static void
mks_clipboard_init (MksClipboard *self)
{
}

MksClipboard *
_mks_clipboard_new (MksSession    *session,
                    MksQemuObject *object)
{
  MksClipboard *self;

  g_return_val_if_fail (MKS_IS_SESSION (session), NULL);
  g_return_val_if_fail (MKS_QEMU_IS_OBJECT (object), NULL);

  self = g_object_new (MKS_TYPE_CLIPBOARD, NULL);
  self->session = g_object_ref (session);
  self->object = g_object_ref (object);
  self->clipboard = mks_qemu_object_get_clipboard (object);
  self->skeleton = mks_qemu_clipboard_skeleton_new ();

  g_signal_connect_object (self->skeleton,
                           "handle-register",
                           G_CALLBACK (mks_clipboard_handle_register_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skeleton,
                           "handle-unregister",
                           G_CALLBACK (mks_clipboard_handle_unregister_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skeleton,
                           "handle-grab",
                           G_CALLBACK (mks_clipboard_handle_grab_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skeleton,
                           "handle-release",
                           G_CALLBACK (mks_clipboard_handle_release_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->skeleton,
                           "handle-request",
                           G_CALLBACK (mks_clipboard_handle_request_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

void
_mks_clipboard_set_read_func (MksClipboard         *self,
                              MksClipboardReadFunc  read_func,
                              gpointer              user_data,
                              GDestroyNotify        destroy)
{
  g_return_if_fail (MKS_IS_CLIPBOARD (self));

  if (self->read_func_data_destroy != NULL)
    self->read_func_data_destroy (self->read_func_data);

  self->read_func = read_func;
  self->read_func_data = user_data;
  self->read_func_data_destroy = destroy;
}

/**
 * mks_clipboard_content_new:
 * @mime_type: a MIME type
 * @bytes: the content bytes
 *
 * Creates new clipboard content.
 *
 * Returns: (transfer full): a `MksClipboardContent`
 */
MksClipboardContent *
mks_clipboard_content_new (const char *mime_type,
                           GBytes     *bytes)
{
  MksClipboardContent *self;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_new0 (MksClipboardContent, 1);
  self->ref_count = 1;
  self->mime_type = g_strdup (mime_type);
  self->bytes = g_bytes_ref (bytes);

  return self;
}

/**
 * mks_clipboard_content_ref:
 * @self: a `MksClipboardContent`
 *
 * Acquires a reference to @self.
 *
 * Returns: (transfer full): @self
 */
MksClipboardContent *
mks_clipboard_content_ref (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  self->ref_count++;

  return self;
}

/**
 * mks_clipboard_content_unref:
 * @self: (nullable): a `MksClipboardContent`
 *
 * Releases a reference to @self.
 */
void
mks_clipboard_content_unref (MksClipboardContent *self)
{
  if (self == NULL)
    return;

  g_return_if_fail (self->ref_count > 0);

  if (--self->ref_count == 0)
    {
      g_clear_pointer (&self->mime_type, g_free);
      g_clear_pointer (&self->bytes, g_bytes_unref);
      g_free (self);
    }
}

/**
 * mks_clipboard_content_get_mime_type:
 * @self: a `MksClipboardContent`
 *
 * Gets the MIME type for the content.
 *
 * Returns: the MIME type
 */
const char *
mks_clipboard_content_get_mime_type (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->mime_type;
}

/**
 * mks_clipboard_content_ref_bytes:
 * @self: a `MksClipboardContent`
 *
 * Gets the bytes for the content.
 *
 * Returns: (transfer full): the content bytes
 */
GBytes *
mks_clipboard_content_ref_bytes (MksClipboardContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_bytes_ref (self->bytes);
}

/**
 * mks_clipboard_get_registered:
 * @self: a `MksClipboard`
 *
 * Gets if @self has registered with QEMU for clipboard redirection.
 */
gboolean
mks_clipboard_get_registered (MksClipboard *self)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);

  return self->registered;
}

/**
 * mks_clipboard_get_owner:
 * @self: a `MksClipboard`
 * @selection: the clipboard selection
 *
 * Gets the current owner for @selection.
 */
MksClipboardOwner
mks_clipboard_get_owner (MksClipboard          *self,
                         MksClipboardSelection  selection)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), MKS_CLIPBOARD_OWNER_NONE);
  g_return_val_if_fail (selection_is_valid (selection), MKS_CLIPBOARD_OWNER_NONE);

  return self->owner[selection];
}

/**
 * mks_clipboard_dup_mime_types:
 * @self: a `MksClipboard`
 * @selection: the clipboard selection
 *
 * Gets the MIME types available for @selection.
 *
 * Returns: (transfer full) (nullable): the MIME types
 */
char **
mks_clipboard_dup_mime_types (MksClipboard          *self,
                              MksClipboardSelection  selection)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), NULL);
  g_return_val_if_fail (selection_is_valid (selection), NULL);

  return g_strdupv (self->mime_types[selection]);
}

/**
 * mks_clipboard_register:
 * @self: a `MksClipboard`
 *
 * Registers for clipboard redirection with QEMU.
 *
 * Returns: (transfer full): a future that resolves to %TRUE
 */
DexFuture *
mks_clipboard_register (MksClipboard *self)
{
  g_autoptr(GError) error = NULL;

  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  if (!mks_clipboard_export_skeleton (self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_then (mks_qemu_clipboard_call_register_future (self->clipboard),
                          mks_clipboard_register_complete_cb,
                          g_object_ref (self),
                          g_object_unref);
}

void
mks_clipboard_register_async (MksClipboard        *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_clipboard_register (self));
}

gboolean
mks_clipboard_register_finish (MksClipboard  *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_clipboard_unregister:
 * @self: a `MksClipboard`
 *
 * Unregisters clipboard redirection with QEMU.
 *
 * Returns: (transfer full): a future that resolves to %TRUE
 */
DexFuture *
mks_clipboard_unregister (MksClipboard *self)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));

  return dex_future_then (mks_qemu_clipboard_call_unregister_future (self->clipboard),
                          mks_clipboard_unregister_complete_cb,
                          g_object_ref (self),
                          g_object_unref);
}

void
mks_clipboard_unregister_async (MksClipboard        *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_clipboard_unregister (self));
}

gboolean
mks_clipboard_unregister_finish (MksClipboard  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

/**
 * mks_clipboard_claim:
 * @self: a `MksClipboard`
 * @selection: the clipboard selection
 * @mime_types: (array zero-terminated=1): the available MIME types
 *
 * Claims @selection for the local peer.
 *
 * Returns: (transfer full): a future that resolves to %TRUE
 */
DexFuture *
mks_clipboard_claim (MksClipboard          *self,
                     MksClipboardSelection  selection,
                     const char * const    *mime_types)
{
  Claim *claim;

  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));
  dex_return_error_if_fail (selection_is_valid (selection));
  dex_return_error_if_fail (mime_types != NULL);

  claim = g_new0 (Claim, 1);
  claim->self = g_object_ref (self);
  claim->selection = selection;
  claim->serial = ++self->serial[selection];
  claim->mime_types = g_strdupv ((char **) mime_types);

  return dex_future_then (mks_qemu_clipboard_call_grab_future (self->clipboard,
                                                               selection,
                                                               claim->serial,
                                                               mime_types),
                          mks_clipboard_claim_complete_cb,
                          claim,
                          (GDestroyNotify) claim_free);
}

/**
 * mks_clipboard_release:
 * @self: a `MksClipboard`
 * @selection: the clipboard selection
 *
 * Releases @selection.
 *
 * Returns: (transfer full): a future that resolves to %TRUE
 */
DexFuture *
mks_clipboard_release (MksClipboard          *self,
                       MksClipboardSelection  selection)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));
  dex_return_error_if_fail (selection_is_valid (selection));

  if (self->owner[selection] == MKS_CLIPBOARD_OWNER_LOCAL)
    mks_clipboard_set_owner (self, selection, MKS_CLIPBOARD_OWNER_NONE, NULL);

  return dex_future_then (mks_qemu_clipboard_call_release_future (self->clipboard, selection),
                          mks_clipboard_complete_boolean,
                          NULL,
                          NULL);
}

/**
 * mks_clipboard_request:
 * @self: a `MksClipboard`
 * @selection: the clipboard selection
 * @mime_types: (array zero-terminated=1): MIME types in preference order
 *
 * Requests clipboard content from QEMU.
 *
 * Returns: (transfer full): a future that resolves to `MksClipboardContent`
 */
DexFuture *
mks_clipboard_request (MksClipboard          *self,
                       MksClipboardSelection  selection,
                       const char * const    *mime_types)
{
  dex_return_error_if_fail (MKS_IS_CLIPBOARD (self));
  dex_return_error_if_fail (selection_is_valid (selection));
  dex_return_error_if_fail (mime_types != NULL);

  return dex_future_then (mks_qemu_clipboard_call_request_future (self->clipboard,
                                                                  selection,
                                                                  mime_types),
                          mks_clipboard_request_complete_cb,
                          NULL,
                          NULL);
}

void
mks_clipboard_request_async (MksClipboard          *self,
                             MksClipboardSelection  selection,
                             const char * const    *mime_types,
                             GCancellable          *cancellable,
                             GAsyncReadyCallback    callback,
                             gpointer               user_data)
{
  mks_future_to_async_result (self,
                              cancellable,
                              callback,
                              user_data,
                              G_STRFUNC,
                              mks_clipboard_request (self, selection, mime_types));
}

MksClipboardContent *
mks_clipboard_request_finish (MksClipboard  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_autoptr(DexFuture) future = NULL;
  const GValue *value;

  g_return_val_if_fail (MKS_IS_CLIPBOARD (self), NULL);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), NULL);

  future = dex_async_result_dup_future (DEX_ASYNC_RESULT (result));

  if (!(value = dex_future_get_value (future, error)))
    return NULL;

  return g_value_dup_boxed (value);
}
