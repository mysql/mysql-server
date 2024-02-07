/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 Use is subject to license terms.

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
 * jtie_tconv_value_impl.hpp
 */

#ifndef jtie_tconv_value_impl_hpp
#define jtie_tconv_value_impl_hpp

#include <assert.h>  // not using namespaces yet
#include <jni.h>

#include "helpers.hpp"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_value.hpp"

// ---------------------------------------------------------------------------
// Java <-> C basic type conversions
// ---------------------------------------------------------------------------

// Implements primitive type parameter conversions.
template <typename J, typename C>
struct ParamBasicT {
  static C convert(cstatus &s, J j, JNIEnv *env) {
    TRACE("C ParamBasicT.convert(cstatus &, J, JNIEnv *)");
    (void)env;
    s = 0;
    // XXX assert(static_cast< J >(static_cast< C >(j)) == j);
    return static_cast<C>(j);  // may convert to unsigned type
  }

  static void release(C c, J j, JNIEnv *env) {
    TRACE("void ParamBasicT.release(C, J, JNIEnv *)");
    (void)c;
    (void)j;
    (void)env;
  }

 private:
  // prohibit instantiation
  ParamBasicT() {
    // prohibit unsupported template specializations
    is_valid_primitive_type_mapping<J, C>();
  }
};

// Implements primitive type result conversions.
template <typename J, typename C>
struct ResultBasicT {
  static J convert(C c, JNIEnv *env) {
    TRACE("J ResultBasicT.convert(C, JNIEnv *)");
    (void)env;
    // XXX assert(static_cast< C >(static_cast< J >(c)) == c);
    return static_cast<J>(c);  // may convert to signed type
  }

 private:
  // prohibit instantiation
  ResultBasicT() {
    // prohibit unsupported template specializations
    is_valid_primitive_type_mapping<J, C>();
  }
};

// ---------------------------------------------------------------------------
// Specializations for basic type conversions
// ---------------------------------------------------------------------------

// Avoid mapping types by broad, generic rules, which easily results in
// template instantiation ambiguities for non-primitive types.  Therefore,
// we enumerate all specicializations for primitive types.

// Lessons learned:
//
// Cannot extend Param/Result specializations for const types by a generic
// rule on the base class (no template match with this indirection)
//   template<> struct ParamBasicT< J, C const > : ParamBasicT< J, C > {};
//   template<> struct ResultBasicT< J, C const > : ResultBasicT< J, C > {};
// but have to specialize Param/Result directly
//   template<> struct Param< J, C const > : ParamBasicT< J, C > {};
//   template<> struct Result< J, C const > : ResultBasicT< J, C > {};
//
// Specializations must be defined over intrinsic types, not aliases
//
// Datatype     LP64    ILP64   LLP64   ILP32   LP32
// char         8       8       8       8       8
// short        16      16      16      16      16
// int          32      64      32      32      16
// long         64      64      32      32      32
// long long                    64
// pointer      64      64      64      32      32

// extend set of valid primitive type mappings for const value specializations
template <typename J, typename C>
struct is_valid_primitive_type_mapping<const J, C> {};
template <typename J, typename C>
struct is_valid_primitive_type_mapping<J, const C> {};
template <typename J, typename C>
struct is_valid_primitive_type_mapping<const J, const C> {};

// also provides specializations for 'const'
// template clutter can be reduced a bit: const value types do not need extra
// specializations of their implementation
//   ... : ParamBasicT< J, C const > {};
//   ... : ResultBasicT< J, C const > {};
// but can be derived from their non-const specializations
#define JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(J, C)   \
  template <>                                      \
  struct is_valid_primitive_type_mapping<J, C> {}; \
  template <>                                      \
  struct Param<J, C> : ParamBasicT<J, C> {};       \
  template <>                                      \
  struct Result<J, C> : ResultBasicT<J, C> {};     \
  template <>                                      \
  struct Param<J, C const> : ParamBasicT<J, C> {}; \
  template <>                                      \
  struct Result<J, C const> : ResultBasicT<J, C> {};

// ---------------------------------------------------------------------------
// Specializations for boolean conversions
// ---------------------------------------------------------------------------

// Implements boolean type parameter conversions.
template <>
struct ParamBasicT<jboolean, bool> {
  static bool convert(cstatus &s, jboolean j, JNIEnv *env) {
    TRACE("bool ParamBasicT.convert(cstatus &, jboolean, JNIEnv *)");
    (void)env;
    s = 0;
    // Java v C: jboolean is unsigned 8-bit, so, beware of truncation
    return (j == JNI_TRUE);
  }

  static void release(bool c, jboolean j, JNIEnv *env) {
    TRACE("void ParamBasicT.release(bool, jboolean, JNIEnv *)");
    (void)c;
    (void)j;
    (void)env;
  }
};

// Implements boolean type result conversions.
template <>
struct ResultBasicT<jboolean, bool> {
  static jboolean convert(bool c, JNIEnv *env) {
    TRACE("jboolean ResultBasicT.convert(bool, JNIEnv *)");
    (void)env;
    // Java v C: jboolean is unsigned 8-bit, so, beware of truncation
    // on some platforms, JNI_TRUE/FALSE seems top be defined as int
    return static_cast<jboolean>(c ? JNI_TRUE : JNI_FALSE);
  }
};

JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jboolean, bool)

// ---------------------------------------------------------------------------
// Specializations for exact-width number type conversions
// ---------------------------------------------------------------------------

JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jbyte, char)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jbyte, signed char)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jbyte, unsigned char)

JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jfloat, float)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jdouble, double)

// ---------------------------------------------------------------------------
// Specializations for variable-width number type conversions
// ---------------------------------------------------------------------------

// Datatype      LP32   ILP32   LP64    ILP64   LLP64
// char          8      8       8       8       8
// short         16     16      16      16      16
// int           16     32      32      64      32
// long          32     32      64      64      32
// long long                                    64
// pointer       32     32      64      64      64

// jshort in LP32, ILP32, LP64, ILP64, LLP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jshort, signed short)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jshort, unsigned short)

// jshort in LP32
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jshort, signed int)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jshort, unsigned int)

// jint in ILP32, LP64, LLP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jint, signed int)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jint, unsigned int)

// jint in LP32, ILP32, LLP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jint, signed long)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jint, unsigned long)

// jlong in ILP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, signed int)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, unsigned int)

// jlong in LP64, ILP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, signed long)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, unsigned long)

// jlong in LLP64
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, signed long long)
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jlong, unsigned long long)

// jdouble
JTIE_SPECIALIZE_BASIC_TYPE_MAPPING(jdouble, long double)

// ---------------------------------------------------------------------------

#endif  // jtie_tconv_value_impl_hpp
