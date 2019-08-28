/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * jtie_tconv_array.hpp
 */

#ifndef jtie_tconv_array_impl_hpp
#define jtie_tconv_array_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_value.hpp"
#include "jtie_tconv_value_impl.hpp"
#include "jtie_tconv_object_impl.hpp"

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
     * Returns the body of the primitive array, or NULL if the operation
     * fails. 
     * The result is valid until releaseArrayElements() is called.
     * Returns a non-const C array to allow for use in non-const context.
     *
     * Pre condition:
     * - no JNI exception is pending: assert(!env->ExceptionCheck())
     * - argument: assert(j != NULL)
     * - argument: assert(env != NULL)
     *
     * Post condition:
     * - return value:
     *   non-NULL:
     *     - this thread has no pending JNI exception (!env->ExceptionCheck())
     *     - the return value is valid and initialized array
     *     - corresponding releaseArrayElements() must be called
     *   NULL:
     *     - this thread has a pending JNI exception (env->ExceptionCheck())
     *     - corresponding releaseArrayElements() must not be called
     */
#if 0 // disabled on purpose, only document function
    static CA
    getArrayElements(JNIEnv * env, JA j, jboolean * isCopy) {
        TRACE("CA ArrayConv.getArrayElements(JNIEnv *, JA, jboolean *)");
        (void)env; (void)j; (void)isCopy;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function

    /**
     * Informs the VM that the native code no longer needs access to elems.
     * Accepts a const C array to allow for use in const context.
     *
     * Pre condition:
     * - getArrayElements() has been called with 'j' and returned 'c'
     * - argument: assert(j != NULL)
     * - argument: assert(c != NULL)
     * - argument: assert(env != NULL)
     *
     * Post condition:
     * - In case of a pending exception (env->ExceptionCheck()), only these
     *   safe JNI functions have been called:
     *     - ExceptionOccurred
     *     - ExceptionDescribe
     *     - ExceptionClear
     *     - ExceptionCheck
     *     - ReleaseStringChars
     *     - ReleaseStringUTFchars
     *     - ReleaseStringCritical
     *     - Release<Type>ArrayElements
     *     - ReleasePrimitiveArrayCritical
     *     - DeleteLocalRef
     *     - DeleteGlobalRef
     *     - DeleteWeakGlobalRef
     *     - MonitorExit
     */
#if 0 // disabled on purpose, only document function
    static void
    releaseArrayElements(JNIEnv * env, JA j, const CA c, jint mode) {
        TRACE("void ArrayConv.releaseArrayElements(JNIEnv *, JA, const CA, jint)");
        (void)env; (void)j; (void)c; (void)mode;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function

    /**
     * Constructs a new primitive array object with elements from a buffer.
     * Accepts a const C array to allow for use in const context.
     *
     * Pre condition:
     * - no JNI exception is pending: assert(!env->ExceptionCheck())
     * - argument: assert(c != NULL)
     * - argument: assert(env != NULL)
     *
     * Post condition:
     * - return value:
     *   non-NULL:
     *     - this thread has no pending JNI exception (!env->ExceptionCheck())
     *     - the return value is a valid and initialized Java array 
     *   NULL:
     *     - a JNI exception is pending (env->ExceptionCheck())
     *
     *   In other words, any errors during the result conversion must be
     *   signaled by registering a Java exception with the VM.
     */
#if 0 // disabled on purpose, only document function
    static JA
    newArray(JNIEnv * env, jsize len, const CA c) {
        TRACE("JA ArrayConv.newArray(JNIEnv *, jsize, const CA)");
        (void)env; (void)len; (void)c;
        static_assert(false, "missing specialization of array conversion");
        return 0;
    }
#endif // disabled on purpose, only document function
};

/**
 * Implements ArrayConv on primitive array types.
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
struct PrimArrayConvImpl {
    static C *
    getArrayElements(JNIEnv * env, JA j, jboolean * isCopy) {
        TRACE("C * PrimArrayConvImpl.getArrayElements(JNIEnv *, JA, jboolean *)");
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        assert(j != NULL);

        // init return value to error
        C * c = NULL;

        J * ja = (env->*GET)(j, isCopy);
        if (env->ExceptionCheck() != JNI_OK) {
            // exception pending
        } else {
            // the JNI Spec (1.4..6) on Get<PrimitiveType>ArrayElements is
            // not explicit on whether an exception has been registered 
            // when the operation returns NULL; so, better code defensively:
            if (ja == NULL) {
                const char * cl = "java/lang/AssertionError";
                const char * m = ("JTie: a JNI Get<PrimitiveType>ArrayElements"
                                  " function call returned NULL but has not"
                                  " registered an exception with the VM"
                                  " (file: " __FILE__ ")");
                registerException(env, cl, m);
            } else {
                // ok, convert pointer types
                c = reinterpret_cast< C * >(ja);
            }
        }
        return c;
    }

    static void
    releaseArrayElements(JNIEnv * env, JA j, const C * c, jint mode) {
        TRACE("void PrimArrayConvImpl.releaseArrayElements(JNIEnv *, JA, const C *, jint)");
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        assert(j != NULL);
        assert(c != NULL);

        if (c != NULL) {
            // ok to strip const, pinned arrays are not changed by release
            // and copies cannot be used after release
            C * ca = const_cast< C * >(c);
            // convert pointer types
            (env->*REL)(j, reinterpret_cast< J * >(ca), mode);
        }
    }

    static JA
    newArray(JNIEnv * env, jsize len, const C * c) {
        TRACE("JA PrimArrayConvImpl.newArray(JNIEnv *, jsize, const C *)");
        // XXX currently, only exact-width base type conversions supported
        assert(sizeof(J) == sizeof(C));
        assert(c != NULL);
        
        // init return value to error
        JA j = NULL;

        JA ja = (env->*NEW)(len);
        if (env->ExceptionCheck() != JNI_OK) {
            // exception pending            
        } else {
            // the JNI Spec (1.4..6) on New<PrimitiveType>Array is not
            // explicit on whether an exception has been registered when the 
            // operation returns NULL; so, better code defensively:
            if (ja == NULL) {
                const char * cl = "java/lang/AssertionError";
                const char * m = ("JTie: a JNI New<PrimitiveType>Array"
                                  " function call returned NULL but has not"
                                  " registered an exception with the VM"
                                  " (file: " __FILE__ ")");
                registerException(env, cl, m);
            } else {
                // convert pointer types
                const J * cjc = reinterpret_cast< const J * >(c);

                // copy values to Java array
                (env->*SET)(ja, 0, len, cjc);
                if (env->ExceptionCheck() != JNI_OK) {
                    // exception pending
                    assert(false); // coding error: invalid index
                } else {
                    // ok
                    j = ja;
                }
            }
        }
        return j;
    }

private:
    PrimArrayConvImpl() {
        // prohibit unsupported array type casts
        is_valid_primitive_type_mapping< J, C >();
    }
};

/**
 * Implements ArrayConv for Java Object array types.
 *
 * Please, note that on balance this type of object array mapping, while
 * having a few merits, has turned out inferior to other array mappings:  It
 * - comes with an unavoidable performance overhead (value-copy semantics)
 * - displays different argument conversion semantics (pass-by-value for
 *   Java-to-C while pass-by-reference for C-to-Java) and
 * - hence, complicates the usage and caller's object management (making it
 *   more error-prone to memory leaks or integrity errors), as well as
 * - hampers the maintenance of this class due to fine points in the code.
 *
 * The reason for the difficulties of this mapping stems from an asymmetry:
 * The natural 1-1 mapping of Java 'MyClass[]' is C 'MyClass**' while C
 * 'MyClass*', strictly speaking, has no Java Object array pendant, for
 * lack of notion of (contigous arrays of) embedded Java Objects.
 *
 * While assumed to work, this mapping has not been used and tested yet;
 * we're keeping it, for it still meets best Java programmers' expectations
 * for C object arrays to be mapped to Java Object[].
 */
template< typename J, typename C >
struct ObjectArrayConvImpl {
    static C *
    getArrayElements(JNIEnv * env, jobjectArray j, jboolean * isCopy) {
        TRACE("C * ObjectArrayConvImpl.getArrayElements(JNIEnv *, jobjectArray, jboolean *)");
        assert(j != NULL);

        // init return value to error
        C * c = NULL;

        const jsize n = env->GetArrayLength(j);
        if (env->ExceptionCheck() != JNI_OK) {
            // exception pending
            assert(false); // coding error: invalid argument
        } else {
            // ISO C++: 'new' throws std::bad_alloc if unsuccessful
            C * ca = new C[n];

            cstatus s = copyToCObjectArray(ca, j, n, env);
            if (s != 0) {
                // exception pending
                assert(env->ExceptionCheck() != JNI_OK);
                delete[] ca;
            } else {
                // assign isCopy out parameter
                if (isCopy != NULL) {
                    *isCopy
                        = ResultBasicT< jboolean, bool >::convert(true, env);
                }
                
                // ok
                c = ca;
            }
        }
        return c;
    }

    static void
    releaseArrayElements(JNIEnv * env, jobjectArray j, const C * c, jint mode) {
        TRACE("void ObjectArrayConvImpl.releaseArrayElements(JNIEnv *, jobjectArray, const C *, jint)");
        assert(j != NULL);
        assert(c != NULL);
        delete[] c;
    }

    static jobjectArray
    newArray(JNIEnv * env, jsize len, const C * c) {
        TRACE("jobjectArray ObjectArrayConvImpl.newArray(JNIEnv *, jsize, const C *)");
        assert(c != NULL);
        
        // init return value to error
        jobjectArray j = NULL;

        // get a (local or global) class object reference
        jclass cls = ObjectResult< J *, C * >::J_ctor::getClass(env);
        if (cls == NULL) {
            // exception pending
        } else {
            J * ja = newJavaObjectArray(cls, len, env);            
            if (ja == NULL) {
                // exception pending
            } else {
                cstatus s = copyToJavaObjectArray(ja, c, len, env);
                if (s != 0) {
                    // exception pending
                    assert(env->ExceptionCheck() != JNI_OK);
                } else {
                    // ok
                    j = ja;
                }
            }

            // release reference (if needed)
            ObjectResult< J *, C * >::J_ctor::releaseRef(env, cls);
        }
        return j;
    }

private:
    // Returns a new Java Object array with all elements initialized to null;
    // in case of a NULL return, an exception is pending.
    static J *
    newJavaObjectArray(jclass cls, jsize n, JNIEnv * env);
    
    // Copies objects referred by a Java Object array over a C object array;
    // a non-zero return value indicates failure with an exception pending.
    static cstatus
    copyToCObjectArray(C * c, jobjectArray j, jsize n, JNIEnv * env);

    // Initializes a Java Object array with references from a C object array;
    // a non-zero return value indicates failure with an exception pending.
    static cstatus
    copyToJavaObjectArray(jobjectArray j, C * c, jsize n, JNIEnv * env);
};

template< typename J, typename C >
inline J *
ObjectArrayConvImpl< J, C >::
newJavaObjectArray(jclass cls, jsize n, JNIEnv * env) {
    assert(cls);

    // init return value to error
    J * j = NULL;

    jobjectArray ja = env->NewObjectArray(n, cls, NULL);
    if (env->ExceptionCheck() != JNI_OK) {
        // exception pending
    } else {
        // the JNI Spec (1.4..6) on NewObjectArray is not explicit on
        // whether an exception has been registered when the operation
        // returns NULL; so, better code defensively:
        if (ja == NULL) {
            const char * cl = "java/lang/AssertionError";
            const char * m = ("JTie: a JNI NewObjectArray function call"
                              " returned NULL but has not registered an"
                              " exception with the VM"
                              " (file: " __FILE__ ")");
            registerException(env, cl, m);
        } else {
            // ok
            j = ja;
        }
    }
    return j;
}

template< typename J, typename C >
inline cstatus
ObjectArrayConvImpl< J, C >::
copyToCObjectArray(C * c, jobjectArray j, jsize n, JNIEnv * env) {
    assert(j != NULL);
    assert(c != NULL);
    
    // init return value to error
    cstatus s = -1;

    // copy objects referenced from Java array to new array
    jsize i;
    for (i = 0; i < n; i++) {
        // get the Java array element
        _jobject * jfo = env->GetObjectArrayElement(j, i);
        if (env->ExceptionCheck() != JNI_OK) {
            // exception pending
            assert(false); // coding error: invalid index
            break;
        } 
        assert(env->ExceptionCheck() == JNI_OK);

        // get the instance referenced by Java array element
        _jtie_Object * jao = cast< _jtie_Object *, _jobject * >(jfo);
        if (jao == NULL) {
            const char * cl = "java/lang/IllegalArgumentException";
            const char * m = ("JTie: the Java Object array must not have"
                              " null as elements when mapped to a"
                              " C object array (file: " __FILE__ ")");
            registerException(env, cl, m);
            break;
        }
        assert(jao != NULL);
            
        // get the C/C++ object referenced by Java array element
        C * co = ObjectParam< _jtie_Object *, C * >::convert(s, jao, env);
        assert(s != 0 || co != NULL);
        if (s != 0) {
            // exception pending
            break;
        }
        assert(co != NULL);

        // copy referenced object to array element
        // - copy-by-value semantics for Java only knows object references;
        // - therefore, this mapping requires an accessible copy c'tor;
        // - asymmetry to copyToCObjectArray()'s reference semantics
        c[i] = *co;
    }
    if (i < n) {
        // exception pending
    } else {
        // ok
        s = 0;
    }
    
    return c;
}

template< typename J, typename C >
inline cstatus
ObjectArrayConvImpl< J, C >::
copyToJavaObjectArray(jobjectArray j, C * c, jsize n, JNIEnv * env) {
    assert(c != NULL);
    assert(j != NULL);
    
    // init return value to error
    cstatus s = -1;

    // copy C object references to Java array
    jsize i;
    for (i = 0; i < n; i++) {
        // obtain reference to array element
        // - no need for value-copy, would burden app's object management
        // - asymmetry to copyToJavaObjectArray()'s value-copy semantics
        C * co = c + i;

        // get a Java object reference
        J * jao = ObjectResult< J *, C * >::convert(co, env);
        if (jao == NULL) {
            // exception pending
            assert(env->ExceptionCheck() != JNI_OK);
            break;
        }
        assert(jao != NULL);

        // set the Java array element
        _jobject * jfo = cast< _jobject *, J * >(jao);
        env->SetObjectArrayElement(j, i, jfo);
        if (env->ExceptionCheck() != JNI_OK) {
            // exception pending
            assert(false); // coding error: invalid index or jao not subclass
            break;
        } 
        assert(env->ExceptionCheck() == JNI_OK);
    }
    if (i < n) {
        // exception pending
    } else {
        // ok
        s = 0;
    }
    
    return c;
}

// ---------------------------------------------------------------------------
// Specializations for array conversions
// ---------------------------------------------------------------------------

// Avoid mapping types by broad, generic rules, which easily results in
// template instantiation ambiguities for non-primitive types.  Therefore,
// we enumerate all specicializations for primitive types.

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
        : PrimArrayConvImpl< JA, J, C,                                  \
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
