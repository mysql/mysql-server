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
 * jtie_tconv_refbybb_impl.hpp
 */

#ifndef jtie_tconv_refbybb_impl_hpp
#define jtie_tconv_refbybb_impl_hpp

#include <assert.h>  // not using namespaces yet
#include <jni.h>

#include "helpers.hpp"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_ptrbybb_impl.hpp"
#include "jtie_tconv_refbybb.hpp"
#include "jtie_tconv_utils_impl.hpp"

// ---------------------------------------------------------------------------
// ByteBufferRefParam, ByteBufferRefResult
// ---------------------------------------------------------------------------

// XXX document, cleanup

// implements the mapping of ByteBuffers to reference parameters
template <typename J, typename C>
struct ByteBufferRefParam;

// implements the mapping of ByteBuffers to reference results
template <typename J, typename C>
struct ByteBufferRefResult;

inline cstatus ensureNonNullBuffer(jtie_j_n_ByteBuffer jbb, JNIEnv *env) {
  // init return value to error
  cstatus s = -1;

  if (jbb == NULL) {
    const char *c = "java/lang/IllegalArgumentException";
    const char *m =
        ("JTie: java.nio.ByteBuffer cannot be null"
         " when mapped to an object reference type"
         " (file: " __FILE__ ")");
    registerException(env, c, m);
  } else {
    // ok
    s = 0;
  }
  return s;
}

template <typename J, typename C>
struct ByteBufferRefParam {
  static C &convert(cstatus &s, jtie_j_n_ByteBuffer j, JNIEnv *env) {
    TRACE(
        "C & ByteBufferRefParam.convert(cstatus &, jtie_j_n_ByteBuffer, JNIEnv "
        "*)");

    // init return value and status to error
    s = -1;
    C *c = NULL;

    if (ensureNonNullBuffer(j, env) != 0) {
      // exception pending
    } else {
      c = ByteBufferPtrParam<J, C>::convert(s, j, env);
      assert(s != 0 || c != NULL);
    }
    return *c;
  }

  static void release(C &c, jtie_j_n_ByteBuffer j, JNIEnv *env) {
    TRACE(
        "void ByteBufferRefParam.release(C &, jtie_j_n_ByteBuffer, JNIEnv *)");
    ByteBufferPtrParam<J, C>::release(&c, j, env);
  }
};

template <typename J, typename C>
struct ByteBufferRefResult {
  static J *convert(C &c, JNIEnv *env) {
    TRACE("J * ByteBufferRefResult.convert(C &, JNIEnv *)");
    // technically, C++ references can be null, hence, no asserts here
    // assert(&c != NULL);
    J *j = ByteBufferPtrResult<J, C>::convert(&c, env);
    // assert(j != NULL);
    return j;
  }
};

// ---------------------------------------------------------------------------
// Specializations for ByteBuffer type conversions
// ---------------------------------------------------------------------------

// specialize ByteBuffers mapped to references:
// - params: require a minimum buffer capacity of the size of the base type
// - results: allocate buffer with a capacity of the size of the base type
template <typename C>
struct Param<jtie_j_n_ByteBuffer, C &>
    : ByteBufferRefParam<_jtie_j_n_BoundedByteBuffer<sizeof(C)>, C> {};
template <typename C>
struct Result<jtie_j_n_ByteBuffer, C &>
    : ByteBufferRefResult<_jtie_j_n_BoundedByteBuffer<sizeof(C)>, C> {};

// ---------------------------------------------------------------------------

#endif  // jtie_tconv_refbybb_impl_hpp
