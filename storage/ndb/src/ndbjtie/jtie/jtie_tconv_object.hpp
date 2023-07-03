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
 * jtie_tconv_object.hpp
 */

#ifndef jtie_tconv_object_hpp
#define jtie_tconv_object_hpp

#include <jni.h>

#include "jtie_tconv.hpp"
#include "jtie_tconv_idcache_impl.hpp"

// ---------------------------------------------------------------------------
// Java object <-> C class object type conversions
// ---------------------------------------------------------------------------

/**
 * Internal root class for representing Java classes in peer type mappings.
 *
 * Rationale: A dedicated type, distinct from JNI's _jobject, allows for
 * better control of template resolution (avoiding ambiguities) and
 * instantiation (reducing clutter).  To allow for pointer compatibility
 * by static type conversion, this type derives from _jobject.
 */
struct _jtie_Object : _jobject {
};

/**
 * Internal, generic trait type mapping a Java class.
 *
 * Rationale: This generic class has outlived its purpose, but can be used
 * as a container for additional, class-specific mapping information.
 */
template< typename J >
struct _jtie_ObjectMapper : _jtie_Object {
    /**
     * Name and descriptor of this class's no-argument constructor.
     */
    // XXX cleanup: use a template decl instead of a macro
    JTIE_DEFINE_METHOD_MEMBER_INFO( ctor )
};

/**
 * Defines the trait type aliases for the mapping of a
 * user-defined Java class to a C++ class.
 *
 * The macro takes these arguments:
 *   C: The name of the C++ class to which the Java class is mapped.
 *   T: A name tag, identifying the Java peer class in this mapping.
 *
 * The type aliases with suffix _t or _ct serve the use of the class as
 * target for [const] member access (field access or method calls); the
 * aliases ending on _p (pointer) or _r (reference), and their const
 * variations, allow for use as parameter or result types.
 *
 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_t               J <->       C
 *   ttrait_<T>_ct              J <-> const C
 *   ttrait_<T>_r               J <->       C &
 *   ttrait_<T>_cr              J <-> const C &
 *   ttrait_<T>_p               J <->       C *
 *   ttrait_<T>_cp              J <-> const C *
 *   ttrait_<T>_pc              J <->       C * const
 *   ttrait_<T>_cpc             J <-> const C * const
 *
 * Implementation note: Using a macro instead of a class template with
 * type members (via typedefs) has the benefit of allowing for direct
 * use of the defined type names, while each use of a type member needs
 * to be prepended with the C++ keyword "typename".
 */
#define JTIE_DEFINE_PEER_CLASS_MAPPING( C, T )                          \
    struct T {};                                                        \
    typedef ttrait< jobject, C, _jtie_ObjectMapper< T > *               \
                    > ttrait_##T##_t;                                   \
    typedef ttrait< jobject, const C, _jtie_ObjectMapper< T > *         \
                    > ttrait_##T##_ct;                                  \
    typedef ttrait< jobject, C &, _jtie_ObjectMapper< T > *             \
                    > ttrait_##T##_r;                                   \
    typedef ttrait< jobject, const C &, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_cr;                                  \
    typedef ttrait< jobject, C *, _jtie_ObjectMapper< T > *             \
                    > ttrait_##T##_p;                                   \
    typedef ttrait< jobject, const C *, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_cp;                                  \
    typedef ttrait< jobject, C * const, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_pc;                                  \
    typedef ttrait< jobject, const C * const, _jtie_ObjectMapper< T > * \
                    > ttrait_##T##_cpc;

#if 0 // XXX cleanup this unsupported mapping

 * Naming convention:
 *   type alias:                specifies a mapping:
 *   ttrait_<T>_a               J <->       C *
 *   ttrait_<T>_ca              J <-> const C *
 *   ttrait_<T>_ac              J <->       C * const
 *   ttrait_<T>_cac             J <-> const C * const

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

// XXX and this is how it's done
    typedef ttrait< jobjectArray, C *,                                  \
                    _jtie_j_ArrayMapper<                                \
                      _jtie_j_BoundedArray<                             \
                        _jtie_ObjectMapper< J >,                        \
                        1 >                                             \
                      > *                                               \
                    > ttrait_##J##_1a;                                  \

#endif // XXX cleanup this unsupported mapping

// XXX to document
// XXX cleanup: symmetry with JTIE_DEFINE_METHOD_MEMBER_INFO( ctor ) above
#define JTIE_INSTANTIATE_PEER_CLASS_MAPPING( T, JCN )                   \
    JTIE_INSTANTIATE_CLASS_MEMBER_INFO_1(_jtie_ObjectMapper< T >::ctor, \
                                         JCN, "<init>", "()V")          \
    template struct _jtie_ObjectMapper< T >;                            

// ---------------------------------------------------------------------------

#endif // jtie_tconv_object_hpp
