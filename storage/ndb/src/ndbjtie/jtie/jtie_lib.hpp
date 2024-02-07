/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_lib.hpp
 */

#ifndef jtie_lib_hpp
#define jtie_lib_hpp

#include <jni.h>

#include "helpers.hpp"
#include "jtie.hpp"

// ---------------------------------------------------------------------------
// JTie Template Library: Include these global symbol definitions in
//                        exactly one compilation unit
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// JTie Library: Global Variable Definitions & Template Instantiations
// ---------------------------------------------------------------------------

JTIE_INSTANTIATE_CLASS_MEMBER_INFO_0(_ByteBuffer_isReadOnly,
                                     "java/nio/ByteBuffer", "isReadOnly", "()Z")

JTIE_INSTANTIATE_CLASS_MEMBER_INFO_0(_ByteBuffer_asReadOnlyBuffer,
                                     "java/nio/ByteBuffer", "asReadOnlyBuffer",
                                     "()Ljava/nio/ByteBuffer;")

JTIE_INSTANTIATE_CLASS_MEMBER_INFO_0(_ByteBuffer_remaining,
                                     "java/nio/ByteBuffer", "remaining", "()I")

JTIE_INSTANTIATE_CLASS_MEMBER_INFO_0(_ByteBuffer_position,
                                     "java/nio/ByteBuffer", "position", "()I")

JTIE_INSTANTIATE_CLASS_MEMBER_INFO_0(_Wrapper_cdelegate,
                                     "com/mysql/jtie/Wrapper", "cdelegate", "J")

// ---------------------------------------------------------------------------
// JTie Library: Load and Unload Handlers
// ---------------------------------------------------------------------------

// root object, which allows Threads to obtain their local JNIEnv
static JavaVM *jtie_cached_jvm;

/**
 * Handler function to be called from a user-defined function JNI_OnLoad
 * with the same signature.
 *
 * Initializes JTie's resources (e.g., cached JNI method and field ids)
 * when the native, JTie-based wrapper library is loaded into a Java VM.
 * As of JDK 1.2, the same JNI native library cannot be loaded into more
 * than one class loader at a time (UnsatisfiedLinkError).
 *
 * Returns the JNI version needed by JTie or JNI_ERR with a pending error.
 * If the VM does not recognize the version number returned by JNI_OnLoad,
 * the native library cannot be loaded.
 */
jint JTie_OnLoad(JavaVM *jvm, void *reserved) {
  TRACE("jint JTie_OnLoad(JavaVM *, void *)");
  (void)reserved;
  VERBOSE("initializing the JTie resources ...");

  // beware of circular loading dependencies: do not load classes here
  // whose static initializers have a dependency upon this native library...

  // cache the JavaVM pointer
  jtie_cached_jvm = jvm;

  // get the JNI environment
  JNIEnv *env;
  if (jvm->GetEnv((void **)&env, JNI_VERSION_1_2) != JNI_OK) {
    return JNI_ERR;  // unsupported version or thread not attached to VM
  }

  // JTie requires JDK 1.4 JNI functions (e.g., direct ByteBuffer access)
  VERBOSE("... initialized the JTie resources");
  return JNI_VERSION_1_4;
}

/**
 * Handler function to be called from a user-defined function JNI_OnUnload
 * with the same signature.
 *
 * Frees JTie's resources (e.g., cached JNI ids) when the class loader
 * containing the native, JTie-based wrapper library is garbage-collected.
 *
 * This function is called in an unknown context (such as from a finalizer),
 * which requires to be conservative and refrain from arbitrary Java
 * call-backs (classes have been unloaded when JNI_OnUnload is invoked).
 */
void JTie_OnUnload(JavaVM *jvm, void *reserved) {
  TRACE("void JTie_OnUnload(JavaVM *, void *)");
  (void)reserved;
  VERBOSE("releasing the JTie resources ...");

  // get the JNI environment
  JNIEnv *env;
  if (jvm->GetEnv((void **)&env, JNI_VERSION_1_2) != JNI_OK) {
    return;  // unsupported version or thread not attached to VM
  }

  jtie_cached_jvm = NULL;
  VERBOSE("... released the JTie resources");
}

// ---------------------------------------------------------------------------
// JTie Library: Wrapper Class Load Handler
// ---------------------------------------------------------------------------

#if 0  // XXX not implemented, see comments in Wrapper.java

// XXX review & ceanup...

// XXX document: cannot cache/store:
// JNIEnv -- thread-specific
// local references -- call-frame- (and thread-) specific
//  Local references are valid only inside a single invocation of a native method
//  Local references are valid only within the thread in which they are created; do not pass between threads
// Create a global reference when it is necessary to pass a reference across threads.

#include "jtie_Wrapper.h"
// XXX how to avoid?
//#include "jtie_tconv_idcache_impl.hpp"

extern "C" {
JNIEXPORT void JNICALL
Java_com_mysql_jtie_Wrapper_initIds(JNIEnv *, jclass);

/**
 * Handler function called when the class Wrapper is loaded.
 */
JNIEXPORT void JNICALL
Java_com_mysql_jtie_Wrapper_initIds(JNIEnv * env, jclass cls)
{
    TRACE("void Java_com_mysql_jtie_Wrapper_initIds(JNIEnv *, jclass)");
    // store class in a weak global ref to allow for class to be unloaded
    jtie_cls_com_mysql_jtie_Wrapper = env->NewWeakGlobalRef(cls);
    if (jtie_cls_com_mysql_jtie_Wrapper == NULL) {
        return; // OutOfMemoryError pending
    }
}

}

#endif

// ---------------------------------------------------------------------------

#endif  // jtie_lib_hpp
