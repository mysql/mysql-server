/*
 Copyright 2010 Sun Microsystems, Inc.
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
 * jtie_tconv_refbyval.hpp
 */

#ifndef jtie_tconv_refbyval_hpp
#define jtie_tconv_refbyval_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java value/array <-> C reference type conversions
// ---------------------------------------------------------------------------

/**
 * Defines the trait type aliases for the mapping of a
 * basic Java value type to a C++ reference.
 *
 * The macro takes these arguments:
 *   J: A JNI type name (representing a basic Java type).
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_r_v             J <->       C &
 *   ttrait_<T>_cr_v            J <-> const C &
 */
#define JTIE_DEFINE_VALUE_REF_TYPE_MAPPING( J, C, T )                   \
    typedef ttrait< J, C &                                              \
                    > ttrait_##T##_r_v;                                 \
    typedef ttrait< J, const C &                                        \
                    > ttrait_##T##_cr_v;

/**
 * Defines the trait type aliases for the mapping of a
 * basic Java value type to a C++ reference.
 *
 * The macro takes these arguments:
 *   J: A JNI array type name (representing a basic Java array type).
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_r_a             J <->       C &       (of array length 1)
 *
 * Note that there's no point in defining a mapping:
 *   ttrait_<T>_cr_a            J <-> const C &       (of array length 1)
 */
#define JTIE_DEFINE_ARRAY_REF_TYPE_MAPPING( J, C, T )                   \
    typedef ttrait< J *, C &                                            \
                    > ttrait_##T##_r_a;

// ---------------------------------------------------------------------------

#endif // jtie_tconv_refbyval_hpp
