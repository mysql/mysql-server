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
 * jtie_tconv_def.hpp
 */

#ifndef jtie_tconv_def_hpp
#define jtie_tconv_def_hpp

//#include <jni.h>
//#include <cstring>
//#include "helpers.hpp"

// ---------------------------------------------------------------------------
// formal <-> actual parameter/result type casts
// ---------------------------------------------------------------------------

// a function template for simple type adjustments by casts
template< typename T, typename S >
inline T
cast(S s) {
    TRACE("T cast(S)");
    return s; // type conversions supported by C++
}

// ---------------------------------------------------------------------------
// Java <-> C type conversions
// ---------------------------------------------------------------------------

// benefits of using a class template over loose function templates:
// - separates J->C from C->J conversion
// - explicitely expresses convert/release asymmetry for J->C and C->J
// - may allow defining/enforcing uniform converter interface
// - may allow combining with inheritance
// - function templates don't seem to allow for partial specialization

// default conversion semantics:
// - may not want (unsafe) default conversions
// - cannot specify initializer (= 0) for static member function
// - can declare but not define methods => undefined symbols

// passing the classname to Result.convert:
// - unfortunately, string literals cannot be used as template arguments
//   template< typename J, typename C, char * JCN = "" >
//   must be integral constant with external linkage

typedef int cstatus;

/**
 * A class template with functions for parameter type conversion.
 */
template< typename J, typename C >
struct Param {
    /**
     * Returns the C data type value for a Java data value.
     *
     * Writes a converion status to the output parameter 's':
     * 0:
     *     - no JNI exception is pending (!env->ExceptionCheck())
     *     - other convert() and the C delegate function may be called
     *     - the corresponding release() function must be called
     *   otherwise:
     *     - a JNI exception is pending (env->ExceptionCheck())
     *     - no other convert() or the C delegate function must be called
     *     - the corresponding release() function must not be called
     */
    static C
    convert(cstatus & s, J j, JNIEnv * env);
/*
    {
        TRACE("C Param.convert(cstatus &, J, JNIEnv *)");
        s = 0;
        return j; // all conversions by C++
    }
*/

    /**
     * Releases any resources allocated by a previous convert() call.
     *
     * May only call JNI functions that are safe for pending exception:
     * - ExceptionOccurred
     * - ExceptionDescribe
     * - ExceptionClear
     * - ExceptionCheck
     * - ReleaseStringChars
     * - ReleaseStringUTFchars
     * - ReleaseStringCritical
     * - Release<Type>ArrayElements
     * - ReleasePrimitiveArrayCritical
     * - DeleteLocalRef
     * - DeleteGlobalRef
     * - DeleteWeakGlobalRef
     * - MonitorExit
     */
    static void
    release(C c, J j, JNIEnv * env);
/*
    {
        TRACE("void Param.release(C, J, JNIEnv *)");
    }
*/
};

/**
 * A class template with functions for result type conversion.
 */
template< typename J, typename C >
struct Result {
    /**
     * Returns the Java data type value for a C data value.
     *
     * Any errors must be signaled by creating a Java exception with the VM
     * (env->ExceptionCheck()).
     */
    static J
    convert(C c, JNIEnv * env);
/*
    {
        TRACE("J Result.convert(C, JNIEnv *)");
        return c; // all conversions by C++
    }
*/
};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_def_hpp
