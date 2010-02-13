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
 * jtie_tconv_enum_impl.hpp
 */

#ifndef jtie_tconv_enum_impl_hpp
#define jtie_tconv_enum_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_enum.hpp"
#include "jtie_tconv_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// Java value <-> C enum conversions
// ---------------------------------------------------------------------------

// currently, only Java int <-> C enum mappings are supported

// Implements enum parameter conversions.
template< typename J, typename C >
struct ParamEnumT {
    // ok to pass J by value
    static C
    convert(cstatus & s, J j, JNIEnv * env) {
        TRACE("C ParamEnumT.convert(cstatus &, J, JNIEnv *)");
        (void)env;
        s = 0;
        return static_cast< C >(j.value);
    }

    static void
    release(C c, J j, JNIEnv * env) {
        TRACE("void ParamEnumT.release(C, J, JNIEnv *)");
        (void)c; (void)j; (void)env;
    }

private:
    // prohibit instantiation
    ParamEnumT() {
        // prohibit unsupported template specializations
        /* is_valid_enum_type_mapping< J, C >(); */
    }
};

// Implements enum type result conversions.
template< typename J, typename C >
struct ResultEnumT {
    // ok to return J by value
    static J
    convert(C c, JNIEnv * env) {
        TRACE("J ResultEnumT.convert(C, JNIEnv *)");
        (void)env;
        return static_cast< J >(c);
    }

private:
    // prohibit instantiation
    ResultEnumT() {
        // prohibit unsupported template specializations
        /* is_valid_enum_type_mapping< J, C >(); */
    }
};

// ---------------------------------------------------------------------------
// Specializations for integral <-> enum type conversions
// ---------------------------------------------------------------------------

// Avoid mapping types by broad, generic rules, which easily results in
// template instantiation ambiguities for non-enum types.  Therefore,
// we enumerate all specicializations for enum types.

/*
// define set of valid enum type mappings
template < typename C >
struct is_valid_enum_type_mapping< _jtie_jint_Enum, C > {};

// extend for const enum specializations
template < typename J, typename C >
struct is_valid_enum_type_mapping< const J, C > {};
template < typename J, typename C >
struct is_valid_enum_type_mapping< J, const C > {};
template < typename J, typename C >
struct is_valid_enum_type_mapping< const J, const C > {};
*/

// non-const enum value parameter types
template< typename C >
struct Param< _jtie_jint_Enum, C >
    : ParamEnumT< _jtie_jint_Enum, C > {};

// non-const enum value result types
template< typename C >
struct Result< _jtie_jint_Enum, C >
    : ResultEnumT< _jtie_jint_Enum, C > {};

#if 0 // XXX HACK -- tmp support for enums
// non-const enum parameter types
//template< typename C > struct Param< jint, C > : ParamEnumT< jint, C > {};
template< typename J, typename C >
C
Param< J, C >::convert(cstatus & s, J j, JNIEnv * env)
{
    TRACE("C Param.convert(cstatus &, J, JNIEnv *)");
    (void)env;
    s = 0;
    // XXX assert(static_cast< J >(static_cast< C >(j)) == j);
    return static_cast< C >(j); // XXX check why necessary when fixed enums
}

template< typename J, typename C >
void
Param< J, C >::release(C c, J j, JNIEnv * env)
{
    TRACE("void Param.release(C, J, JNIEnv *)");
    (void)c; (void)j; (void)env;
}
    
// non-const enum result types
//template< typename C > struct Result< jint, C > : ResultEnumT< jint, C > {};
template< typename J, typename C >
J
Result< J, C >::convert(C c, JNIEnv * env)
{
    TRACE("J Result.convert(C, JNIEnv *)");
    (void)env;
    return static_cast< J >(c);
    // XXX assert(static_cast< C >(static_cast< J >(c)) == c);
    return static_cast< J >(c); // XXX check why necessary when fixed enums
}
#endif // XXX HACK -- tmp support for enums

// ---------------------------------------------------------------------------

#endif // jtie_tconv_enum_impl_hpp
