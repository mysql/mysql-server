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
 * jtie_tconv_refbyval.hpp
 */

#ifndef jtie_tconv_refbyval_hpp
#define jtie_tconv_refbyval_hpp

#include <stdint.h>
#include <jni.h>
//#include "helpers.hpp"
#include "jtie_tconv_def.hpp"
#include "jtie_tconv_carray.hpp"

// ---------------------------------------------------------------------------
// infrastructure code: Java value copy <-> const C & type conversions
// ---------------------------------------------------------------------------

/*
// TOO NARROW, NOT TAKING INTO ACCOUNT SUPPORTED NON_IDENTITY CONVERSIONS:

// add a generic mapping between const references and value copies
template< typename C >
struct Result< C, const C & > {
    static C
    convert(const C & c, JNIEnv * env) {
        TRACE("C Result.convert(C, JNIEnv *)");
        return c;
    }
};

// add a generic mapping between const references and value copies
template< typename C >
struct Param< C, const C & > {
    static const C &
    convert(cstatus & s, C j, JNIEnv * env) {
        TRACE("const C & Param.convert(cstatus &, C, JNIEnv *)");
        s = 0;
        return j;
    }

    static void
    release(const C & c, C j, JNIEnv * env) {
        TRACE("void Param.release(const C &, C, JNIEnv *)");
    }
};
*/

// TOO BROAD, RESULTING IN AMBIGUITIES FOR NON-PROIMITIVE TYPES:

// for all defined Param< J, C > mappings, add a generic mapping between
// const references and value copies

template< typename J, typename C >
struct Param< J, const C & > : Param< J, C > {};

template< typename J, typename C >
struct Result< J, const C & > : Result< J, C > {};

// for all defined Param< J, C > mappings, add a generic mapping between
// const references and 1-element arrays serving as value holders

//template< typename J, typename C >
//struct Param< J, const C & > : Param< J, C > {};

template< typename J, typename C >
struct Result< J, C & > : Result< J, C > {};

// ---------------------------------------------------------------------------
// jarray
// ---------------------------------------------------------------------------

inline cstatus
ensureNonNullArray(jarray ja, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;
    
    if (ja == NULL) {
        // raise exception
        jclass iae = env->FindClass("java/lang/IllegalArgumentException");
        if (iae == NULL) {
            // exception pending
        } else {
            env->ThrowNew(iae,
                          "JNI wrapper: Java array cannot be null"
                          " when mapped to an object reference type"
                          " (file: " __FILE__ ")");
            env->DeleteLocalRef(iae);
        }
    } else {
        // ok
        s = 0;
    }
    return s;
}

inline cstatus
ensureSingleElementArray(jarray ja, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;

    jsize n = env->GetArrayLength(ja);
    if (n != 1) {
        // raise exception
        jclass iae = env->FindClass("java/lang/IllegalArgumentException");
        if (iae == NULL) {
            // exception pending
        } else {
            env->ThrowNew(iae,
                          "JNI wrapper: Java array must have a single element"
                          " when mapped to an object reference type"
                          " (file: " __FILE__ ")");
            env->DeleteLocalRef(iae);
        }
    } else {
        // ok
        s = 0;
    }
    return s;
}

template< typename J, typename C >
struct ParamValueHolder {

    inline static C &
    convert(cstatus & s, J j, JNIEnv * env) {
        TRACE("C & ParamValueHolder.convert(cstatus &, J, JNIEnv *)");

        // init return value and status to error
        s = -1;
        C * c = NULL;

        // return value of single element
        if (ensureNonNullArray(j, env) != 0) {
            // exception pending
        } else {
            if (ensureSingleElementArray(j, env) != 0) {
                // exception pending
            } else {
                // get a C array, to be released by ReleaseIntArrayElements()
                // ignore whether C array is pinned or a copy of Java array
                c = GetArrayElements< C, J >(env, j, NULL);
                if (c == NULL) {
                    // exception pending
                } else {
                    // ok
                    s = 0;
                }
            }
        }
        return *c;
    }

    inline static void
    release(C & c, J j, JNIEnv * env) {
        TRACE("void ParamValueHolder.release(C &, J, JNIEnv *)");

        // release the C array allocated by GetIntArrayElements()
        // if C array was a copy, copy back any changes to Java array
        ReleaseArrayElements< C, J >(env, j, &c, 0);
    }
};

// ---------------------------------------------------------------------------

template<>
struct Param< jbooleanArray, bool & > : ParamValueHolder< jbooleanArray, bool > {};

template<>
struct Param< jbyteArray, char & > : ParamValueHolder< jbyteArray, char > {};

template<>
struct Param< jbyteArray, int8_t & > : ParamValueHolder< jbyteArray, int8_t > {};

template<>
struct Param< jbyteArray, uint8_t & > : ParamValueHolder< jbyteArray, uint8_t > {};

template<>
struct Param< jshortArray, int16_t & > : ParamValueHolder< jshortArray, int16_t > {};

template<>
struct Param< jshortArray, uint16_t & > : ParamValueHolder< jshortArray, uint16_t > {};

template<>
struct Param< jintArray, int32_t & > : ParamValueHolder< jintArray, int32_t > {};

template<>
struct Param< jintArray, uint32_t & > : ParamValueHolder< jintArray, uint32_t > {};

template<>
struct Param< jlongArray, int64_t & > : ParamValueHolder< jlongArray, int64_t > {};

template<>
struct Param< jlongArray, uint64_t & > : ParamValueHolder< jlongArray, uint64_t > {};

template<>
struct Param< jfloatArray, float & > : ParamValueHolder< jfloatArray, float > {};

template<>
struct Param< jdoubleArray, double & > : ParamValueHolder< jdoubleArray, double > {};

// ---------------------------------------------------------------------------
// jarray
// ---------------------------------------------------------------------------

/*
template< typename I >
struct Param< jintArray, I > {
    static int
    convert(JNIEnv * env, I & c, jintArray const & j) {
        TRACE("int Param.convert(JNIEnv *, I &, jintArray const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get a C array, to be released by ReleaseIntArrayElements()
        // ignore whether C array is pinned or a copy of Java array
        jint * cj = env->GetIntArrayElements(j, NULL);
        c = reinterpret_cast< I >(cj);

        return (c == NULL);
    }

    static void
    release(JNIEnv * env, I & c, jintArray const & j) {
        TRACE("void Param.release(JNIEnv *, I &, jintArray const &)");
    }
};

template<>
struct Param< jintArray, const signed int * > {
    static int
    convert(JNIEnv * env, const signed int * & c, jintArray const & j) {
        TRACE("int Param.convert(JNIEnv *, const signed int * &, jintArray const &)");
        return convert(env, c, j);
    }

    static void
    release(JNIEnv * env, const signed int * & c, jintArray const & j) {
        TRACE("void Param.release(JNIEnv *, const signed int * &, jintArray const &)");
        if (c == NULL) {
            assert(j == NULL);
            return;
        }
        assert(j);

        // release the C array allocated by GetIntArrayElements()
        // if C array was a copy, discard any changes since contracted as const
        signed int * mc = const_cast<signed int *>(c);
        jint * cj = reinterpret_cast<jint *>(mc);
        env->ReleaseIntArrayElements(j, cj, JNI_ABORT); // safe to call
    }
};

template<>
struct Param< jintArray, signed int * > {
    static int
    convert(JNIEnv * env, signed int * & c, jintArray const & j) {
        TRACE("int Param.convert(JNIEnv *, signed int * &, jintArray const &)");
        return convert(env, c, j);
    }

    static void
    release(JNIEnv * env, signed int * & c, jintArray const & j) {
        TRACE("void Param.release(JNIEnv *, signed int * &, jintArray const &)");
        if (c == NULL) {
            assert(j == NULL);
            return;
        }
        assert(j);

        // release the C array allocated by GetIntArrayElements()
        // if C array was a copy, copy back any changes to Java array
        jint * cj = reinterpret_cast<jint *>(c);
        env->ReleaseIntArrayElements(j, cj, 0); // safe to call
    }
};

template< typename I >
struct Result< jintArray, I > {
    static jintArray
    convert(JNIEnv * env, I const & c) {
        TRACE("jintArray Result.convert(JNIEnv *, I const &)");

        // init target, even in case of errors (better: use exceptions)
        jintArray j = NULL;
        if (c == NULL)
            return j;

        // construct a Java array object from a C array
        // XXX how large to choose size?
        const jsize n = 0;
        j = env->NewIntArray(n);
        if (j != NULL) {

            // fill Java array from C array, release target in case of errors
            const jint * cj = reinterpret_cast<const jint *>(c);
            env->SetIntArrayRegion(j, 0, n, cj);
            if (env->ExceptionCheck()) {
                env->DeleteLocalRef(j);
                j = 0;
            }
        }
        return j;
    }
};
*/

/*
template<>
struct Result< jintArray, const signed int * > {
    static jintArray
    convert(JNIEnv * env, const signed int * const & c) {
        TRACE("jintArray Result.convert(JNIEnv *, const signed int * const &)");
        return convert(env, c);
    }
};
*/

// ---------------------------------------------------------------------------

#endif // jtie_tconv_refbyval_hpp
