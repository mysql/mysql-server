/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
 * jtie_tconv_ptrbyval_impl.hpp
 */

#ifndef jtie_tconv_ptrbyval_impl_hpp
#define jtie_tconv_ptrbyval_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_ptrbyval.hpp"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_array_impl.hpp"
#include "jtie_tconv_utils_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// ArrayPtrParam, ArrayPtrResult
// ---------------------------------------------------------------------------

// Returns zero if an array has a min size; otherwise, an exception is pending.
template< jlong N >
inline cstatus
ensureMinArraySize(jarray ja, JNIEnv * env) {
    assert(ja != NULL);
    
    // init return value to error
    cstatus s = -1;

    // check the array's length
    jsize n = env->GetArrayLength(ja);
    if (env->ExceptionCheck() != JNI_OK) {
        // exception pending
        assert(false); // coding error: argument not valid
    } else {
        if (n < N) {
            const char * c = "java/lang/IllegalArgumentException";
            const char * m = ("JTie: the Java array's length is too small for"
                              "  the mapped parameter (file: " __FILE__ ")");
            registerException(env, c, m);
        } else {
            // ok
            s = 0;
        }
    }
    return s;
}

// Implements the mapping of arrays to pointer parameters.
template< typename J, typename C >
struct ArrayPtrParam {
    static C *
    convert(cstatus & s, typename J::JA_t * j, JNIEnv * env) {
        TRACE("C * ArrayPtrParam.convert(cstatus &, typename J::JA_t *, JNIEnv *)");
        // init return value and status to error
        s = -1;
        C * c = NULL;

        if (j == NULL) {
            // ok
            s = 0;
        } else {
            if (ensureMinArraySize< J::length >(j, env) != 0) {
                // exception pending
            } else {
                assert(env->GetArrayLength(j) >= J::length);

                // get a C array, to be released by releaseArrayElements()
                // ignore whether C array is pinned or a copy of Java array
                c = (ArrayConv< typename J::JA_t *, C * >
                     ::getArrayElements(env, j, NULL));
                if (c == NULL) {
                    // exception pending
                } else {
                    // ok
                    s = 0;
                }
            }
        }
        return c;
    }

    static void
    release(C * c, typename J::JA_t * j, JNIEnv * env) {
        TRACE("void ArrayPtrParam.release(C *, typename J::JA_t *, JNIEnv *)");

        // compile-time flag whether to copy back any possible changes to the
        // Java array; tradeoff between
        // - minor performance gains for non-mutable types (const C *)
        // - observable data differences for C functions that modify an array
        //   argument by casting away its constness
        //
        // the settings below reflect this semantics: for
        // - mutable types (C *):
        //   ==> all changes to the C array are reflected in the Java array
        // - non-mutable types (const C *) and
        //   - C functions that modify the array despite its constness and
        //     - a JVM that returns a pointer to the pinned, original array
        //       ==> all changes are reflected in the Java array
        //     - a JVM that choses to return a copy of the original array
        //       ==> any change to the C array is lost
        const jint copyBackMode
            = (TypeInfo< C >::isMutable()
               ? 0		// copy back content if needed and free buffer
               : JNI_ABORT);	// free the buffer without copying back

        if (c == NULL) {
            assert(j == NULL); // corresponding convert() succeeded (!)
            // ok
        } else {
            assert(j != NULL);
            // release the C array allocated by getArrayElements()
            (ArrayConv< typename J::JA_t *, C * >
             ::releaseArrayElements(env, j, c, copyBackMode));
        }
    }
};

// Implements the mapping of arrays to pointer results.
template< typename J, typename C >
struct ArrayPtrResult {
    static J *
    convert(C * c, JNIEnv * env) {
        TRACE("J * ArrayPtrResult.convert(C *, JNIEnv *)");

        // init return value to error
        J * j = NULL;

        if (c == NULL) {
            // ok
        } else {
            jarray jja = (ArrayConv< typename J::JA_t *, C * >
                          ::newArray(env, J::length, c));
            J * ja = static_cast< J * >(jja);
            if (ja == NULL) {
                // exception pending
            } else {
                assert(env->GetArrayLength(ja) == J::length);
                // ok
                j = ja;
            }
        }
        return j;
    }
};

// ---------------------------------------------------------------------------
// Specializations for pointer type conversions
// ---------------------------------------------------------------------------

// Avoid mapping types by broad, generic rules, which easily results in
// template instantiation ambiguities for non-primitive types.  Therefore,
// we enumerate all specicializations for primitive type pointers.

// extend array param specializations to const pointers
template< typename C >
struct Param< _jbooleanArray *, C * const >
    : Param< _jbooleanArray *, C * > {};
template< typename C >
struct Param< _jbyteArray *, C * const >
    : Param< _jbyteArray *, C * > {};
template< typename C >
struct Param< _jshortArray *, C * const >
    : Param< _jshortArray *, C * > {};
template< typename C >
struct Param< _jintArray *, C * const >
    : Param< _jintArray *, C * > {};
template< typename C >
struct Param< _jlongArray *, C * const >
    : Param< _jlongArray *, C * > {};
template< typename C >
struct Param< _jfloatArray *, C * const >
    : Param< _jfloatArray *, C * > {};
template< typename C >
struct Param< _jdoubleArray *, C * const >
    : Param< _jdoubleArray *, C * > {};

// extend result array specializations to const pointers
template< typename C >
struct Result< _jbooleanArray *, C * const >
    : Result< _jbooleanArray *, C * > {};
template< typename C >
struct Result< _jbyteArray *, C * const >
    : Result< _jbyteArray *, C * > {};
template< typename C >
struct Result< _jshortArray *, C * const >
    : Result< _jshortArray *, C * > {};
template< typename C >
struct Result< _jintArray *, C * const >
    : Result< _jintArray *, C * > {};
template< typename C >
struct Result< _jlongArray *, C * const >
    : Result< _jlongArray *, C * > {};
template< typename C >
struct Result< _jfloatArray *, C * const >
    : Result< _jfloatArray *, C * > {};
template< typename C >
struct Result< _jdoubleArray *, C * const >
    : Result< _jdoubleArray *, C * > {};

// extend BoundedArrays specializations to const pointers
template< typename J, typename C >
struct Param< _jtie_j_ArrayMapper< J > *, C * const >
    :  Param< _jtie_j_ArrayMapper< J > *, C * > {};
template< typename J, typename C >
struct Result< _jtie_j_ArrayMapper< J > *, C * const >
    : Result< _jtie_j_ArrayMapper< J > *, C * > {};

// specialize BoundedArrays mapped to pointers/arrays:
// - params: require a minimum array length given by the BoundedArray's
//   static data member
// - results: allocate array with a length given by the BoundedArray's
//   static data member
template< typename J, typename C >
struct Param< _jtie_j_ArrayMapper< J > *, C * >
    : ArrayPtrParam< _jtie_j_ArrayMapper< J >, C > {};
template< typename J, typename C >
struct Result< _jtie_j_ArrayMapper< J > *, C * >
    : ArrayPtrResult< _jtie_j_ArrayMapper< J >, C > {};

// specialize arrays mapped to pointers/arrays:
// - params: do not require a minimum buffer capacity, for size may be zero
//   when just passing an address
// - results: allocate buffer with a capacity of zero, since the size is
//    unknown (i.e., just returning an address)
#define JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING( J, C )                      \
    template<>                                                          \
    struct Param< J *, C * >                                            \
        : ArrayPtrParam< _jtie_j_BoundedArray< J, 0 >, C > {};          \
    template<>                                                          \
    struct Param< J *, const C * >                                      \
        : ArrayPtrParam< _jtie_j_BoundedArray< J, 0 >, const C > {};    \
    template<>                                                          \
    struct Result< J *, C * >                                           \
        : ArrayPtrResult< _jtie_j_BoundedArray< J, 0 >, C > {};         \
    template<>                                                          \
    struct Result< J *, const C * >                                     \
        : ArrayPtrResult< _jtie_j_BoundedArray< J, 0 >, const C > {};

// ---------------------------------------------------------------------------
// Specializations for pointer to exact-width primitive type conversions
// ---------------------------------------------------------------------------

JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jbooleanArray, bool)

JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jbyteArray, char)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jbyteArray, signed char)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jbyteArray, unsigned char)

JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jfloatArray, float)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jdoubleArray, double)

// ---------------------------------------------------------------------------
// Specializations for pointer to variable-width primitive type conversions
// ---------------------------------------------------------------------------

// jshort in LP32, ILP32, LP64, ILP64, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jshortArray, signed short)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jshortArray, unsigned short)

// jshort in LP32
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jshortArray, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jshortArray, unsigned int)

// jint in ILP32, LP64, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jintArray, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jintArray, unsigned int)

// jint in LP32, ILP32, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jintArray, signed long)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jintArray, unsigned long)

// jlong in ILP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, unsigned int)

// jlong in LP64, ILP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, signed long)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, unsigned long)

// jlong in LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, signed long long)
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jlongArray, unsigned long long)

// jdouble
JTIE_SPECIALIZE_ARRAY_TYPE_MAPPING(_jdoubleArray, long double)

// ---------------------------------------------------------------------------

#endif // jtie_tconv_ptrbyval_impl_hpp
