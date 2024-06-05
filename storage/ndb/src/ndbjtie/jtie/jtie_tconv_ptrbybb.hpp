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
 * jtie_tconv_ptrbybb.hpp
 */

#ifndef jtie_tconv_ptrbybb_hpp
#define jtie_tconv_ptrbybb_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java ByteBuffer <-> C array/pointer type conversions
// ---------------------------------------------------------------------------

// root type for ByteBuffer argument/result mappings
struct _jtie_j_n_ByteBuffer : _jobject {
  // no need for a class name member as for user-defined classes
  // static const char * const java_internal_class_name;
};
typedef _jtie_j_n_ByteBuffer *jtie_j_n_ByteBuffer;

// subtype for ByteBuffer mappings with a required/allocated buffer size
template <jlong N>
struct _jtie_j_n_BoundedByteBuffer : _jtie_j_n_ByteBuffer {
  static const jlong capacity = N;
};

// wrapper type for BoundedByteBuffer mappings for template specialization
template <typename J>
struct _jtie_j_n_ByteBufferMapper : J {};

/**
 * Defines the trait type aliases for the mapping of a
 * Java NIO ByteBuffer to a C++ pointer.
 *
 * The macro takes these arguments:
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_0p_bb           BB <->       C *       (of unspecified length)
 *   ttrait_<T>_0cp_bb          BB <-> const C *       (of unspecified length)
 *   ttrait_<T>_0pc_bb          BB <->       C * const (of unspecified length)
 *   ttrait_<T>_0cpc_bb         BB <-> const C * const (of unspecified length)
 */
#define JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(C, T)                          \
  typedef ttrait<jobject, C *, jtie_j_n_ByteBuffer> ttrait_##T##_0p_bb;        \
  typedef ttrait<jobject, const C *, jtie_j_n_ByteBuffer> ttrait_##T##_0cp_bb; \
  typedef ttrait<jobject, C *const, jtie_j_n_ByteBuffer> ttrait_##T##_0pc_bb;  \
  typedef ttrait<jobject, const C *const, jtie_j_n_ByteBuffer>                 \
      ttrait_##T##_0cpc_bb;

JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(void, void)

/**
 * Defines the trait type aliases for the mapping of a
 * Java NIO ByteBuffer to a C++ array.
 *
 * The macro takes these arguments:
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_1p_bb           BB <->       C *       (of array length 1)
 *   ttrait_<T>_1cp_bb          BB <-> const C *       (of array length 1)
 *   ttrait_<T>_1pc_bb          BB <->       C * const (of array length 1)
 *   ttrait_<T>_1cpc_bb         BB <-> const C * const (of array length 1)
 */
#define JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(C, T)               \
  typedef ttrait<                                                           \
      jobject, C *,                                                         \
      _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<sizeof(C)>> *> \
      ttrait_##T##_1p_bb;                                                   \
  typedef ttrait<                                                           \
      jobject, const C *,                                                   \
      _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<sizeof(C)>> *> \
      ttrait_##T##_1cp_bb;                                                  \
  typedef ttrait<                                                           \
      jobject, C *const,                                                    \
      _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<sizeof(C)>> *> \
      ttrait_##T##_1pc_bb;                                                  \
  typedef ttrait<                                                           \
      jobject, const C *const,                                              \
      _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<sizeof(C)>> *> \
      ttrait_##T##_1cpc_bb;

// XXX cleanup (or remove corresponding unit test cases)
#if 1
// aliases for: <[const-]void>_<[const-]pointer>_<ByteBuffer<size=1>>
typedef ttrait<jobject, const void *,
               _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<1>> *>
    ttrait_void_1cp_bb;
typedef ttrait<jobject, void *,
               _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<1>> *>
    ttrait_void_1p_bb;
typedef ttrait<jobject, const void *const,
               _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<1>> *>
    ttrait_void_1cpc_bb;
typedef ttrait<jobject, void *const,
               _jtie_j_n_ByteBufferMapper<_jtie_j_n_BoundedByteBuffer<1>> *>
    ttrait_void_1pc_bb;
#endif

// ---------------------------------------------------------------------------

#endif  // jtie_tconv_ptrbybb_hpp
