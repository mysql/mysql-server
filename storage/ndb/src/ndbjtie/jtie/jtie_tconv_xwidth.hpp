/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_tconv_xwidth.hpp
 */

#ifndef jtie_tconv_xwidth_hpp
#define jtie_tconv_xwidth_hpp

#include <jni.h>

#include "jtie_stdint.h"
#include "jtie_tconv_value.hpp"
#include "jtie_tconv_ptrbybb.hpp"
#include "jtie_tconv_refbybb.hpp"
#include "jtie_tconv_ptrbyval.hpp"
#include "jtie_tconv_refbyval.hpp"

// ---------------------------------------------------------------------------
// Java <-> C primitive & derived exact-width type conversions
// ---------------------------------------------------------------------------

/**
 * Defines the set of value, pointer, and reference trait type aliases
 * for the mapping of a basic Java type to a basic C++ type alias.
 *
 * The macro takes these arguments:
 *   J: A basic JNI type name (representing a basic Java type).
 *   C: A basic C++ type alias.
 *   T: A name tag for this mapping.
 *
 * Naming convention: see documentation of the JTIE_DEFINE_... macros
 */
#define JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( J, C, T )                   \
    JTIE_DEFINE_BASIC_TYPE_MAPPING(J, C, T)                             \
    JTIE_DEFINE_ARRAY_PTR_TYPE_MAPPING(_##J##Array, C, T)               \
    JTIE_DEFINE_ARRAY_PTR_LENGTH1_TYPE_MAPPING(_##J##Array, C, T)       \
    JTIE_DEFINE_VALUE_REF_TYPE_MAPPING(J, C, T)                         \
    JTIE_DEFINE_ARRAY_REF_TYPE_MAPPING(_##J##Array, C, T)               \
    JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(C, T)                       \
    JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(C, T)               \
    JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(C, T)

JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jboolean, bool, bool)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, char, char)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, signed char, schar)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, unsigned char, uchar)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, int8_t, int8)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, uint8_t, uint8)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jshort, int16_t, int16)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jshort, uint16_t, uint16)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jint, int32_t, int32)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jint, uint32_t, uint32)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jlong, int64_t, int64)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jlong, uint64_t, uint64)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jfloat, float, float)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jdouble, double, double)

// ---------------------------------------------------------------------------

#endif // jtie_tconv_xwidth_hpp
