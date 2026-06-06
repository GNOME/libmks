/* mks-types.h
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

#include <libdex.h>

#include "mks-version-macros.h"

G_BEGIN_DECLS

typedef struct _MksAudioFormat         MksAudioFormat;
typedef struct _MksTransport           MksTransport;
typedef struct _MksChardev             MksChardev;
typedef struct _MksClipboard           MksClipboard;
typedef struct _MksClipboardContent    MksClipboardContent;
typedef struct _MksClipboardRedirector MksClipboardRedirector;
typedef struct _MksDBusTransport       MksDBusTransport;
typedef struct _MksDevice              MksDevice;
typedef struct _MksKeyboard            MksKeyboard;
typedef struct _MksMicrophone          MksMicrophone;
typedef struct _MksMouse               MksMouse;
typedef struct _MksScreen              MksScreen;
typedef struct _MksScreenAttributes    MksScreenAttributes;
typedef struct _MksSession             MksSession;
typedef struct _MksSpeaker             MksSpeaker;
typedef struct _MksTouchable           MksTouchable;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MksDevice, g_object_unref)

/*<private>
 * MKS_DECLARE_INTERNAL_TYPE:
 * @ModuleObjName: The name of the new type, in camel case (like GtkWidget)
 * @module_obj_name: The name of the new type in lowercase, with words
 *  separated by '_' (like 'gtk_widget')
 * @MODULE: The name of the module, in all caps (like 'GTK')
 * @OBJ_NAME: The bare name of the type, in all caps (like 'WIDGET')
 * @ParentName: the name of the parent type, in camel case (like GtkWidget)
 *
 * A convenience macro for emitting the usual declarations in the header file
 * for a type which is intended to be subclassed only by internal consumers.
 *
 * This macro differs from %G_DECLARE_DERIVABLE_TYPE and %G_DECLARE_FINAL_TYPE
 * by declaring a type that is only derivable internally. Internal users can
 * derive this type, assuming they have access to the instance and class
 * structures; external users will not be able to subclass this type.
 */
#define MKS_DECLARE_INTERNAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName)      \
  GType module_obj_name##_get_type (void);                                                           \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                   \
  typedef struct _##ModuleObjName ModuleObjName;                                                     \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                                       \
                                                                                                     \
  _GLIB_DEFINE_AUTOPTR_CHAINUP (ModuleObjName, ParentName)                                           \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (ModuleObjName##Class, g_type_class_unref)                           \
                                                                                                     \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                   \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }         \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) {    \
    return G_TYPE_CHECK_CLASS_CAST (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }     \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                       \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                        \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) {               \
    return G_TYPE_CHECK_CLASS_TYPE (ptr, module_obj_name##_get_type ()); }                           \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) {\
    return G_TYPE_INSTANCE_GET_CLASS (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }   \
  G_GNUC_END_IGNORE_DEPRECATIONS

G_END_DECLS
