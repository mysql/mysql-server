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
 * jtie_tconv_refbyval_impl.hpp
 */

#ifndef jtie_tconv_refbyval_impl_hpp
#define jtie_tconv_refbyval_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_refbyval.hpp"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_ptrbyval_impl.hpp"
#include "jtie_tconv_utils_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// ArrayRefParam, ArrayRefResult
// ---------------------------------------------------------------------------

// XXX document, cleanup

// implements the mapping of (1-elem) arrays to reference parameters
template< typename J, typename C > struct ArrayRefParam;

// implements the mapping of (1-elem) arrays to reference results
template< typename J, typename C > struct ArrayRefResult;

inline cstatus
ensureNonNullArray(jarray ja, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;

    if (ja == NULL) {
        const char * c = "java/lang/IllegalArgumentException";
        const char * m = ("JNI wrapper: Java array cannot be null"
                          " when mapped to an object reference type"
                          " (file: " __FILE__ ")");
        registerException(env, c, m);
    } else {
        // ok
        s = 0;
    }
    return s;
}

template< typename J, typename C >
struct ArrayRefParam {

    static C &
    convert(cstatus & s, typename J::JA_t * j, JNIEnv * env) {
        TRACE("C & ArrayRefParam.convert(cstatus &, typename J::JA_t *, JNIEnv *)");

        // init return value and status to error
        s = -1;
        C * c = NULL;

        if (ensureNonNullArray(j, env) != 0) {
            // exception pending
        } else {
            c = ArrayPtrParam< J, C >::convert(s, j, env);
            assert(s != 0 || c != NULL);
        }
        return *c;
    }

    static void
    release(C & c, typename J::JA_t * j, JNIEnv * env) {
        TRACE("void ArrayRefParam.release(C &, typename J::JA_t *, JNIEnv *)");
        ArrayPtrParam< J, C >::release(&c, j, env);
    }
};

// actually, there's not much of a point to map a result reference to an
// 1-element array as a value-copy holder, for we can return the value
// directly, instead; hence, this class is defined for completeness only
template< typename J, typename C >
struct ArrayRefResult {
    static J *
    convert(C & c, JNIEnv * env) {
        TRACE("J * ArrayRefResult.convert(C &, JNIEnv *)");
        // technically, C++ references can be null, hence, no asserts here
        //assert(&c != NULL);
        J * j = ArrayPtrResult< J, C >::convert(&c, env);
        //assert(j != NULL);
        return j;
    }
};

// ---------------------------------------------------------------------------
// Specializations for reference type conversions
// ---------------------------------------------------------------------------

// Avoid mapping types by broad, generic rules, which easily results in
// template instantiation ambiguities for non-primitive types.  Therefore,
// we enumerate all specicializations for primitive type references.

// specialize values/value-holders mapped to references:
// - const params: map as value copy
// - const results: map as value copy
// - non-const params: map as value holder (array of length 1)
// - non-const results:  map as value copy (no use as value holders)
#define JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING( JA, J, C )              \
    template<>                                                          \
    struct Param< J, const C & >                                        \
        : Param< J, C > {};                                             \
    template<>                                                          \
    struct Result< J, const C & >                                       \
        : Result< J, C > {};                                            \
    template<>                                                          \
    struct Param< JA *, C & >                                           \
        : ArrayRefParam< _jtie_j_BoundedArray< JA, 1 >, C > {};         \
    template<>                                                          \
    struct Result< J, C & >                                             \
        : Result< J, C > {};                                            \

// ---------------------------------------------------------------------------
// Specializations for reference to exact-width primitive type conversions
// ---------------------------------------------------------------------------

JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jbooleanArray, jboolean, bool)

JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jbyteArray, jbyte, char)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jbyteArray, jbyte, signed char)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jbyteArray, jbyte, unsigned char)

JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jfloatArray, jfloat, float)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jdoubleArray, jdouble, double)

// ---------------------------------------------------------------------------
// Specializations for reference to variable-width primitive type conversions
// ---------------------------------------------------------------------------

// jshort in LP32, ILP32, LP64, ILP64, LLP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jshortArray, jshort, signed short)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jshortArray, jshort, unsigned short)

// jshort in LP32
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jshortArray, jshort, signed int)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jshortArray, jshort, unsigned int)

// jint in ILP32, LP64, LLP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jintArray, jint, signed int)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jintArray, jint, unsigned int)

// jint in LP32, ILP32, LLP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jintArray, jint, signed long)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jintArray, jint, unsigned long)

// jlong in ILP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, signed int)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, unsigned int)

// jlong in LP64, ILP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, signed long)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, unsigned long)

// jlong in LLP64
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, signed long long)
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jlongArray, jlong, unsigned long long)

// jdouble
JTIE_SPECIALIZE_REFERENCE_TYPE_MAPPING(_jdoubleArray, jdouble, long double)

// ---------------------------------------------------------------------------

#endif // jtie_tconv_refbyval_impl_hpp

