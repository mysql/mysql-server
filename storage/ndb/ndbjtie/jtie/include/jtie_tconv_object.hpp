/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

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
 * A root class representing Java peer classes in type mappings.
 *
 * Rationale: A dedicated type, distinct from JNI's _jobject, allows for
 * better control of template resolution (avoiding ambiguities) and
 * instantiation (reducing clutter).  To allow for pointer compatibility
 * by static type conversion, this type derives from _jobject.
 */
struct _jtie_Object : _jobject {
};

// XXX, document: type specifying an Object mapping with a class name

// trait type wrapping named-parametrized Object mappings for specialization
template< typename J >
struct _jtie_ObjectMapper : _jtie_Object {
    // the name of the Java peer class in the JVM format (i.e., '/'-separated)
    static const char * const class_name;

    // the name, descriptor, and JNI type of the class's no-arg c'tor
    static const char * const member_name;
    static const char * const member_descriptor;
    typedef _jmethodID * memberID_t;
};

// XXX can/should these definitions be moves to _lib ?

template< typename J >
const char * const _jtie_ObjectMapper< J >::class_name
    = J::class_name;

template< typename J >
const char * const _jtie_ObjectMapper< J >::member_name
    = "<init>";

template< typename J >
const char * const _jtie_ObjectMapper< J >::member_descriptor
    = "()V";

// Design note:
//
// As of pre-C++0x, string literals cannot be used as template arguments
// which must be integral constants with external linkage.
//
// So, we cannot declare:
//
//    template< const char * >
//    struct _jtie_ClassNamedObject : _jtie_Object {
//        static const char * const java_internal_class_name;
//    };
//
// As a feasible workaround, we require the application to provide a
// trait type for each class, e.g.
//
//    struct _m_A : _jobject {
//        static const char * const java_internal_class_name;
//    };
//    const char * const _m_A::java_internal_class_name = "myjapi/A";
//
// and we retrieve the class name from there.

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
 *   ttrait_<T>_p               J <->       C *
 *   ttrait_<T>_cp              J <-> const C *
 *   ttrait_<T>_pc              J <->       C * const
 *   ttrait_<T>_cpc             J <-> const C * const
 *   ttrait_<T>_r               J <->       C &
 *   ttrait_<T>_cr              J <-> const C &
 *
 * Implementation note: Using a macro instead of a class template with
 * type members (via typedefs) has the benefit of allowing for direct
 * use of the defined type names, while each use of a type member needs
 * to be prepended with the C++ keyword "typename".
 */
#define JTIE_DEFINE_PEER_CLASS_MAPPING( C, T )                          \
    struct T {                                                          \
        static const char * const class_name;                           \
    };                                                                  \
    typedef ttrait< jobject, C, _jtie_ObjectMapper< T > *               \
                    > ttrait_##T##_t;                                   \
    typedef ttrait< jobject, const C, _jtie_ObjectMapper< T > *         \
                    > ttrait_##T##_ct;                                  \
    typedef ttrait< jobject, C *, _jtie_ObjectMapper< T > *             \
                    > ttrait_##T##_p;                                   \
    typedef ttrait< jobject, const C *, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_cp;                                  \
    typedef ttrait< jobject, C * const, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_pc;                                  \
    typedef ttrait< jobject, const C * const, _jtie_ObjectMapper< T > * \
                    > ttrait_##T##_cpc;                                 \
    typedef ttrait< jobject, C &, _jtie_ObjectMapper< T > *             \
                    > ttrait_##T##_r;                                   \
    typedef ttrait< jobject, const C &, _jtie_ObjectMapper< T > *       \
                    > ttrait_##T##_cr;

// XXX to document
#define JTIE_INSTANTIATE_PEER_CLASS_MAPPING( T, JCN )           \
    const char * const T::class_name = JCN;                     \
    template struct _jtie_ObjectMapper< T >;                    \
    template struct MemberId< _jtie_ObjectMapper< T > >;        \
    template struct MemberIdCache< _jtie_ObjectMapper< T > >;

// ---------------------------------------------------------------------------

#endif // jtie_tconv_object_hpp
