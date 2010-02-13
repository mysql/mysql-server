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
 * jtie_tconv_array.hpp
 */

#ifndef jtie_tconv_array_impl_hpp
#define jtie_tconv_array_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_value.hpp"

// ---------------------------------------------------------------------------
// Utilities for Java array <-> C array type conversions
// ---------------------------------------------------------------------------

/**
 * A class with helper methods to convert native arrays into Java arrays
 * and vice versa.
 * ArrayConv's function signatures support both, const and non-const base type
 * specializations.
 *
 * This class only documents prototypes, no functions are declared or
 * defined here, in order to have undefined type mappings result in a
 * compilation error.
 */
template< typename JA,
          typename CA >
struct ArrayConv {
    /**
     * Returns the body of the primitive array.
     * The result is valid until ReleaseArrayElements() is called.
     * Returns a non-const C array to allow for use in non-const context.
     **/
#if 0 // disabled on purpose, only document function
    static CA
    GetArrayElements(JNIEnv * env, JA j, jboolean * isCopy) {
        TRACE("CA GetArrayElements(JNIEnv *, JA, jboolean *)");
        (void)env; (void)j; (void)isCopy;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function

    /**
     * Informs the VM that the native code no longer needs access to elems.
     * Accepts a const C array to allow for use in const context.
     */
#if 0 // disabled on purpose, only document function
    static void
    ReleaseArrayElements(JNIEnv * env, JA j, const CA c, jint mode) {
        TRACE("void ReleaseArrayElements(JNIEnv *, JA, const CA, jint)");
        (void)env; (void)j; (void)c; (void)mode;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function

    /**
     * Constructs a new primitive array object with elements from a buffer.
     * Accepts a const C array to allow for use in const context.
     */
#if 0 // disabled on purpose, only document function
    static JA
    NewArray(JNIEnv * env, jsize len, const CA c) {
        TRACE("JA NewArray(JNIEnv *, jsize, const CA)");
        (void)env; (void)len; (void)c;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function
};

/**
 * Implements ArrayConv on primitive types.
 *
 * The JNI headers, but not the spec, of JDKs >= 1.4 declare the last
 * parameter of Set<Jtype>ArrayRegion(..., const J *) correctly as const
 */
template< typename JA,
          typename J,
          typename C,
          J * (JNIEnv::*GET)(JA, jboolean *),
          void (JNIEnv::*REL)(JA, J *, jint),
          JA (JNIEnv::*NEW)(jsize),
          void (JNIEnv::*SET)(JA, jsize, jsize, const J *) >
struct ArrayConvImpl {
    static C *
    GetArrayElements(JNIEnv * env, JA j, jboolean * isCopy) {
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        // convert pointer types
        return reinterpret_cast< C * >((env->*GET)(j, isCopy));
    }

    static void
    ReleaseArrayElements(JNIEnv * env, JA j, const C * c, jint mode) {
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        // ok to strip const, pinned arrays are not changed by release
        // and copies cannot be used after release
        C * ca = const_cast< C * >(c);
        // convert pointer types
        (env->*REL)(j, reinterpret_cast< J * >(ca), mode);
    }

    static JA
    NewArray(JNIEnv * env, jsize len, const C * c) {
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        JA ja = (env->*NEW)(len);
        // convert pointer types
        const J * cjc = reinterpret_cast< const J * >(c);
        (env->*SET)(ja, 0, len, cjc);
        return ja;
    }

private:
    ArrayConvImpl() {
        // prohibit unsupported array type casts
        is_valid_primitive_type_mapping< J, C >();
    }
};

// ---------------------------------------------------------------------------
// Specializations for array conversions
// ---------------------------------------------------------------------------

// extend specializations to array of const
// Not sure why this generalized const specialization
//   template< typename JA,
//             typename CA >
//   struct ArrayConv< JA, const CA > : ArrayConv< JA, CA > {};
// doesn't resolve (no effect); specializing individually for each type then.

// specialize arrays conversion helper (for non-const and const)
#define JTIE_SPECIALIZE_ARRAY_TYPE_HELPER( JA, J, JN, C )               \
    template<>                                                          \
    struct ArrayConv< JA, C * >                                         \
        : ArrayConvImpl< JA, J, C,                                      \
                         &JNIEnv::Get##JN##ArrayElements,               \
                         &JNIEnv::Release##JN##ArrayElements,           \
                         &JNIEnv::New##JN##Array,                       \
                         &JNIEnv::Set##JN##ArrayRegion > {};            \
    template<>                                                          \
    struct ArrayConv< JA, const C * >                                   \
        : ArrayConv< JA, C * > {};

// ---------------------------------------------------------------------------
// Specializations for exact-width type array conversions
// ---------------------------------------------------------------------------

JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jbooleanArray, jboolean, Boolean, bool)

JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jbyteArray, jbyte, Byte, char)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jbyteArray, jbyte, Byte, signed char)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jbyteArray, jbyte, Byte, unsigned char)

JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jfloatArray, jfloat, Float, float)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jdoubleArray, jdouble, Double, double)


// ---------------------------------------------------------------------------
// Specializations for variable-width type array conversions
// ---------------------------------------------------------------------------

// jshort in LP32, ILP32, LP64, ILP64, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jshortArray, jshort, Short, signed short)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jshortArray, jshort, Short, unsigned short)

// jshort in LP32
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jshortArray, jshort, Short, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jshortArray, jshort, Short, unsigned int)

// jint in ILP32, LP64, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jintArray, jint, Int, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jintArray, jint, Int, unsigned int)

// jint in LP32, ILP32, LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jintArray, jint, Int, signed long)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jintArray, jint, Int, unsigned long)

// jlong in ILP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, signed int)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, unsigned int)

// jlong in LP64, ILP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, signed long)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, unsigned long)

// jlong in LLP64
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, signed long long)
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jlongArray, jlong, Long, unsigned long long)

// jdouble
JTIE_SPECIALIZE_ARRAY_TYPE_HELPER(jdoubleArray, jdouble, Double, long double)

// ---------------------------------------------------------------------------

#endif // jtie_tconv_array_impl_hpp
