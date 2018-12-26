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
 * jtie_tconv_refbybb.hpp
 */

#ifndef jtie_tconv_refbybb_hpp
#define jtie_tconv_refbybb_hpp

#include <jni.h>

#include "jtie_tconv.hpp"
#include "jtie_tconv_ptrbybb.hpp"

// ---------------------------------------------------------------------------
// Java ByteBuffer <-> C reference type conversions
// ---------------------------------------------------------------------------

/**
 * Defines the trait type aliases for the mapping of a
 * Java NIO ByteBuffer to a C++ reference.
 *
 * The macro takes these arguments:
 *   C: A basic C++ type name.
 *   T: A name tag for this mapping.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_r_bb            BB <->       C &
 *   ttrait_<T>_cr_bb           BB <-> const C &
 */
#define JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING( C, T )                 \
    typedef ttrait< jobject, C &, jtie_j_n_ByteBuffer                   \
                    > ttrait_##T##_r_bb;                                \
    typedef ttrait< jobject, const C &, jtie_j_n_ByteBuffer             \
                    > ttrait_##T##_cr_bb;

// ---------------------------------------------------------------------------

#endif // jtie_tconv_refbybb_hpp
