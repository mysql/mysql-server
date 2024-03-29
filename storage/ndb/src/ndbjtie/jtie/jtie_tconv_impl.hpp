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
 * jtie_tconv_impl.hpp
 */

#ifndef jtie_tconv_impl_hpp
#define jtie_tconv_impl_hpp

#include <jni.h>

#include "helpers.hpp"

// ---------------------------------------------------------------------------
// Java <-> C type conversions
// ---------------------------------------------------------------------------

// Design rationale:
//
// The classes here only document prototypes, no functions are declared
// or defined here, in order to have undefined type mappings result in a
// compilation error.  A better option might be to use static_assert()
// when made available in upcoming C++0x.  Alternatively, other patterns
// could be looked at for specifying and mandating specialization
// (a possible candidate: the Curiosly Recurring Template Pattern).

/*
 * A type conversion status type.
 */
typedef int cstatus;

/**
 * An incomplete class template predicating supported type conversions
 * by presence of template specialization.
 *
 * By default, no type conversions are supported at all to prevent any
 * inadvertent or unsafe type mappings.
 */
template < typename J, typename C > struct is_supported_type_mapping;

/**
 * A class template with static functions for conversion of parameter data
 * (Java <-> C++).
 *
 * This class only documents prototypes, no functions are declared or
 * defined here, in order to have undefined type mappings result in a
 * compilation error.
 */
template< typename J, typename C >
struct Param
#if 0 // only document class template, to be defined by specialization
{
    /**
     * Returns the C argument for a Java argument.
     *
     * Pre condition:
     * - no JNI exception is pending: assert(!env->ExceptionCheck())
     *
     * Post condition:
     * - conversion status in output parameter 's':
     *   0:
     *     - this thread has no pending JNI exception (!env->ExceptionCheck())
     *     - the return value is valid and may be used
     *     - other convert() and the C delegate function may be called
     *     - the corresponding release() function must be called
     *   otherwise:
     *     - this thread has a pending JNI exception (env->ExceptionCheck())
     *     - the return value is not valid and cannot be used
     *     - no other convert() or the C delegate function must be called
     *     - the corresponding release() function must not be called
     */
    static C
    convert(cstatus & s, J j, JNIEnv * env) {
        TRACE("C Param.convert(cstatus &, J, JNIEnv *)");
        (void)j; (void)env;
        s = 1;
        static_assert(false, "missing specialization of parameter conversion");
        return 0;
    }

    /**
     * Releases any resources allocated by the corresponding convert() call.
     *
     * Pre condition:
     * - function convert() has been called for parameter 'j' and returned
     *   conversion status 0
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
    static void
    release(C c, J j, JNIEnv * env) {
        TRACE("void Param.release(C, J, JNIEnv *)");
        (void)c; (void)j; (void)env;
        static_assert(false, "missing specialization of parameter conversion");
    }
}
#endif // only document class template, to be defined by specialization
;

/**
 * A class template with static functions for conversion of target objects
 * of method invocations (Java -> C++).
 *
 * Note that while this class may appear a specialization of Param< J, C >,
 * there are subtle differences in the usage and semantics of template C
 * (instantiated with a direct class type, not pointer or reference), which
 * make Target incompatible to derive from Param.
 *
 * This class only documents prototypes, no functions are declared or
 * defined here, in order to have undefined type mappings result in a
 * compilation error.
 */
template< typename J, typename C >
struct Target
#if 0 // only document class template, to be defined by specialization
{
    /**
     * Returns the C delegate of a Java wrapper object as target of a
     * method invocation.
     *
     * Pre + Post conditions: same as Param< J, C >::convert.
     */
    static C &
    convert(cstatus & s, J j, JNIEnv * env) {
        TRACE("C & Target.convert(cstatus &, J, JNIEnv *)");
        (void)j; (void)env;
        s = 1;
        static_assert(false, "missing specialization of target conversion");
        return 0;
    }

    /**
     * Releases any resources allocated by the corresponding convert() call.
     *
     * Pre + Post conditions: same as Param< J, C >::release.
     */
    static void
    release(C & c, J j, JNIEnv * env) {
        TRACE("void Target.release(C &, J, JNIEnv *)");
        (void)c; (void)j; (void)env;
        static_assert(false, "missing specialization of target conversion");
    }
}
#endif // only document class template, to be defined by specialization
;

/**
 * A class template with static functions for conversion of function call
 * or data access result data (Java <- C++).
 *
 * This class only documents prototypes, no functions are declared or
 * defined here, in order to have undefined type mappings result in a
 * compilation error.
 */
template< typename J, typename C >
struct Result
#if 0 // only document class template, to be defined by specialization
{
    /**
     * Returns the Java result value for a C result.
     *
     * Pre condition:
     * - no JNI exception is pending: assert(!env->ExceptionCheck())
     *
     * Post condition:
     * - the return value is valid and may be used by Java caller;
     *   otherwise, a JNI exception is pending (env->ExceptionCheck())
     *
     *   In other words, any errors during the result conversion must be
     *   signaled by registering a Java exception with the VM.
     */
    static J
    convert(C c, JNIEnv * env) {
        TRACE("J Result.convert(C, JNIEnv *)");
        (void)c; (void)env;
        static_assert(false, "missing specialization of result conversion");
        return 0;
    }
}
#endif // only document class template, to be defined by specialization
;

// Lessons learned:
// 
// Basing the type conversion code on class templates rather than loose
// function templates allows for:
// - defining a uniform type converter interface (no overloading ambiguities)
// - writing generic conversion rules using partial template specialization
// - inheriting implementations from type-specific conversion classes
// - separating J->C from C->J conversion expressing convert/release asymmetry

// ---------------------------------------------------------------------------
// Generic specializations for Java <-> C type conversions
// ---------------------------------------------------------------------------

// The set of explicit template specializations in the type conversion
// definitions can be reduced by generifying some type mapping rules.
// We need to be careful, though, to avoid template instantiation clutter
// and resolution ambiguities by too (many) broad rules.

// For values (primitive types and pointers), which are always copied,
// it is safe to specialize const values/pointers to non-const ones.
//
// Examples: 'int const', 'A * const' (does not apply to 'const A *')

// XXX ambiguous with enums
//template< typename J, typename C >
//struct Param< J, C const > : Param< J, C > {};
//
//template< typename J, typename C >
//struct Result< J, C const > : Result< J, C > {};

// XXX untested
template< typename J, typename C >
struct Param< J const, C > : Param< J, C > {};

template< typename J, typename C >
struct Result< J const, C > : Result< J, C > {};

// ---------------------------------------------------------------------------
// formal <-> actual parameter/result type casts
// ---------------------------------------------------------------------------

/**
 * A function template for formal/actual parameter/result type adjustments.
 */
template< typename T, typename S >
inline T
cast(S s) {
    TRACE("T cast(S)");
    return static_cast< T >(s); // support base->derived class pointer casts
}

// Design note:
//
// XXX Rationale for cast<>...
//
// No class template needed (in contrast to Param/Result type conversion),
// a function template is sufficient for actual/formal casts.

// ---------------------------------------------------------------------------

#endif // jtie_tconv_impl_hpp
