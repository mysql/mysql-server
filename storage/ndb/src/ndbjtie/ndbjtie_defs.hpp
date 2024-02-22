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
 * ndbjtie_defs.hpp
 */

#ifndef ndbjtie_defs_hpp
#define ndbjtie_defs_hpp

// API to implement against
#include "ndb_types.h"

// libraries
#include "helpers.hpp"
#include "jtie.hpp"

// the applied Java mapping of basic variable-width C++ types
#define JTIE_JNI_SHORT_T jshort
#define JTIE_JNI_INT_T jint
#define JTIE_JNI_LONG_T jint
#define JTIE_JNI_LONGLONG_T jlong
#define JTIE_JNI_LONGDOUBLE_T jdouble
#include "jtie_tconv_vwidth.hpp"

// ---------------------------------------------------------------------------
// NDB JTie Type Definitions
// ---------------------------------------------------------------------------

// compatibility layer typedefs: mapping trait type aliases for NDB types
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, Int8, Int8)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jbyte, Uint8, Uint8)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jshort, Int16, Int16)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jshort, Uint16, Uint16)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jint, Int32, Int32)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jint, Uint32, Uint32)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jlong, Int64, Int64)
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET(jlong, Uint64, Uint64)

// Don't make compilers choose between the const and non-const versions
// of otherwise identical methods.
#define NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION 1

// XXX: document why clearing cdelegate in wrapper object upon delete
#define JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE 1

// ---------------------------------------------------------------------------

#endif  // ndbjtie_defs_hpp
