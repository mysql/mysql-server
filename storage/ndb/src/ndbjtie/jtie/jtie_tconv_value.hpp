/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.
 Use is subject to license terms.

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
 * jtie_tconv_value.hpp
 */

#ifndef jtie_tconv_value_hpp
#define jtie_tconv_value_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java <-> C primitive type conversions
// ---------------------------------------------------------------------------

/**
 * Defines the trait type aliases for the mapping of
 * a basic Java type to a basic C++ type.
 *
 * The macro takes these arguments:
 *   J: A JNI type name (representing a basic Java type).
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>                 J <->       C 
 *   ttrait_<T>_c               J <-> const C
 */
#define JTIE_DEFINE_BASIC_TYPE_MAPPING( J, C, T )       \
    typedef ttrait< J, C                                \
                    > ttrait_##T;                       \
    typedef ttrait< J, const C                          \
                    > ttrait_##T##_c;

/**
 * A helper class template that predicates the supported type conversions
 * by presence of specialization.
 */
// XXX move? used where?
template < typename J, typename C >
struct is_valid_primitive_type_mapping;

// ---------------------------------------------------------------------------

#endif // jtie_tconv_value_hpp
