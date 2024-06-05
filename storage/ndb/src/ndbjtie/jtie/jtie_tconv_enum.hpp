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
 * jtie_tconv_enum.hpp
 */

#ifndef jtie_tconv_enum_hpp
#define jtie_tconv_enum_hpp

#include <jni.h>

// ---------------------------------------------------------------------------
// Java value <-> C enum conversions
// ---------------------------------------------------------------------------

// currently, only Java int <-> C enum mappings are supported

/**
 * A root type for enum value argument/result mappings.
 * Unlike the root class definitions for object (array etc) mappings,
 * this class does not derive from a JNI type.  Therefore, a conversion
 * constructor/operator is used to facilitate the static type conversion
 * between the Java formal and actual type in this mapping.
 * While this approach (conceptually) involves creating instances of this
 * root class at runtime, which hold the the enum value, this should be
 * detectable to the compiler as purely type-changing copy operations.
 */
struct _jtie_jint_Enum {
  // the enum value (cannot be const to enable assignment)
  jint value;

  // conversion constructor
  _jtie_jint_Enum(jint v) : value(v) {}

  // conversion operator
  operator jint() { return value; }
};

/**
 * Defines the trait type aliases for the mapping of
 * an integral Java type to a C++ enum type.
 *
 * The macro takes these arguments:
 *   C: A C++ enum type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_i               jint <->       C
 *   ttrait_<T>_c_iv            jint <-> const C
 */
#define JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(C, T)            \
  typedef ttrait<jint, C, _jtie_jint_Enum> ttrait_##T##_iv; \
  typedef ttrait<jint, C const, _jtie_jint_Enum> ttrait_##T##_c_iv;

/**
 * A helper class template that predicates the supported type conversions
 * by presence of specialization.
 */
/*
template < typename J, typename C >
struct is_valid_enum_type_mapping;
*/

// XXX to document
#define JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(C)
//    template struct is_valid_enum_type_mapping< jint, C >;

// ---------------------------------------------------------------------------

#endif  // jtie_tconv_enum_hpp
