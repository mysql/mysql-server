/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.
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
 * jtie_tconv_ptrbyval.hpp
 */

#ifndef jtie_tconv_ptrbyval_hpp
#define jtie_tconv_ptrbyval_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java array <-> C array/pointer type conversions
// ---------------------------------------------------------------------------

// type deriving from <jtype>Array for mappings with a required/allocated size
template< typename J, jsize N >
struct _jtie_j_BoundedArray : J {
    typedef J JA_t;
    static const jsize length = N;
};

// wrapper type for BoundedArray mappings for template specialization
template< typename J >
struct _jtie_j_ArrayMapper : J {
};

/**
 * Defines the trait type aliases for the mapping of a
 * Java array to a C++ pointer.
 *
 * The macro takes these arguments:
 *   J: A JNI array type name (representing a basic Java array type).
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_0p_a            J <->       C *       (of unspecified length)
 *   ttrait_<T>_0cp_a           J <-> const C *       (of unspecified length)
 *   ttrait_<T>_0pc_a           J <->       C * const (of unspecified length)
 *   ttrait_<T>_0cpc_a          J <-> const C * const (of unspecified length)
 */
#define JTIE_DEFINE_ARRAY_PTR_TYPE_MAPPING( J, C, T )                   \
    typedef ttrait< J *, C *                                            \
                    > ttrait_##T##_0p_a;                                \
    typedef ttrait< J *, const C *                                      \
                    > ttrait_##T##_0cp_a;                               \
    typedef ttrait< J *, C * const                                      \
                    > ttrait_##T##_0pc_a;                               \
    typedef ttrait< J *, const C * const                                \
                    > ttrait_##T##_0cpc_a;

/**
 * Defines the trait type aliases for the mapping of a
 * Java array to a C++ array.
 *
 * The macro takes these arguments:
 *   J: A JNI array type name (representing a basic Java array type).
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_1p_a            J <->       C *       (of array length 1)
 *   ttrait_<T>_1cp_a           J <-> const C *       (of array length 1)
 *   ttrait_<T>_1pc_a           J <->       C * const (of array length 1)
 *   ttrait_<T>_1cpc_a          J <-> const C * const (of array length 1)
 */
#define JTIE_DEFINE_ARRAY_PTR_LENGTH1_TYPE_MAPPING( J, C, T )           \
    typedef ttrait< J *, C *, _jtie_j_ArrayMapper< _jtie_j_BoundedArray< J, 1 > > * \
                    > ttrait_##T##_1p_a;                                \
    typedef ttrait< J *, const C *, _jtie_j_ArrayMapper< _jtie_j_BoundedArray< J, 1 > > * \
                    > ttrait_##T##_1cp_a;                               \
    typedef ttrait< J *, C * const, _jtie_j_ArrayMapper< _jtie_j_BoundedArray< J, 1 > > * \
                    > ttrait_##T##_1pc_a;                               \
    typedef ttrait< J *, const C * const, _jtie_j_ArrayMapper< _jtie_j_BoundedArray< J, 1 > > * \
                    > ttrait_##T##_1cpc_a;

// ---------------------------------------------------------------------------

#endif // jtie_tconv_ptrbyval_hpp
