/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * jtie_tconv_string_impl.hpp
 */

#ifndef jtie_tconv_string_impl_hpp
#define jtie_tconv_string_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_string.hpp"
#include "jtie_tconv_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// Java String <-> const char * type conversion
// ---------------------------------------------------------------------------

// comments: to support UCS2 and also locale encodings...
// - class _jstring can be subclassed (analog to bytebuffer mappings)
// - see JNU_NewStringNative (§8.2.1) and JNU_GetStringNativeChars (§8.2.2)
//   in JNI Programming Guide & Tutorial...)
// - beware that GetStringChars() etc does not deliver not null-terminated
//   character strings; some OS (e.g., Windows) expect two trailing zero byte
//   values to terminate Unicode strings.

// Implements the mapping of Java String parameters.
// declared as template to support other specializations (e.g. char *)
template< typename J, typename C >
struct ParamStringT;

// Implements the mapping of Java String results.
// declared as template to support other specializations (e.g. char *)
template< typename J, typename C >
struct ResultStringT;

template<>
struct ParamStringT< jstring, const char * > {
    static const char *
    convert(cstatus & s, jstring j, JNIEnv * env) {
        TRACE("const char * ParamStringT.convert(cstatus &, jstring, JNIEnv *)");

        // init return value and status to error
        s = -1;
        const char * c = NULL;
        
        // return a C string from a Java String
        if (j == NULL) {
            // ok
            s = 0;
        } else {
            // get a UTF-8 string, to be released by ReleaseStringUTFChars()
            // ignore whether C string is pinned or a copy of Java string
            c = env->GetStringUTFChars(j, NULL); 
            if (c == NULL) {
                // exception pending
            } else {
                // ok
                s = 0;
            }
        }
        return c;
    }

    static void
    release(const char * c, jstring j, JNIEnv * env) {
        TRACE("void ParamStringT.release(const char *, jstring, JNIEnv *)"); 
        if (c == NULL) {
            assert(j == NULL);
        } else {
            assert(j);
            // release the UTF-8 string allocated by GetStringUTFChars()
            env->ReleaseStringUTFChars(j, c);
        }
    }
};

template<>
struct ResultStringT< jstring, const char * > {
    static jstring
    convert(const char * c, JNIEnv * env) {
        TRACE("jstring ResultStringT.convert(const char *, JNIEnv *)");
        if (c == NULL)
            return NULL;

        // construct a String object from a UTF-8 C string
        return env->NewStringUTF(c);
    }
};

// ---------------------------------------------------------------------------
// Specializations for Java String <-> [const] char * type conversion
// ---------------------------------------------------------------------------

// extend String specializations to const pointers
template< typename C >
struct Param< jstring, C * const > 
    : Param< jstring, C * > {};
template< typename C >
struct Result< jstring, C * const >
    : Result< jstring, C * > {};

// specialize Java Strings mapped to 'const char *'
template<>
struct Param< jstring, const char * >
    : ParamStringT< jstring, const char * > {};
template<>
struct Result< jstring, const char * >
    : ResultStringT< jstring, const char * > {};

// specialize Java Strings mapped to 'char *' (only result mapping!)
// no parameter mapping desirable
//   template<>
//   struct Param< jstring, char * >
//       : ParamStringT< jstring, const char * > {};
// result mapping of compatible with 'const char*'
template<>
struct Result< jstring, char * >
    : ResultStringT< jstring, const char * > {};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_string_impl_hpp
