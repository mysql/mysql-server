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

// XXX subclass _jstring to distinguish UTF8 encoding from UCS2 and
// also locale encodings!!!
//(see JNU_NewStringNative (§8.2.1) and JNU_GetStringNativeChars (§8.2.2)
// in JNI Programming Guide & Tutorial...)
//
// when attempting UCS2 decoding, beware that GetStringChars() (or
// GetStringCritical()) are not NULL-terminated!  Some OS (e.g., Windows)
// expect two trailing zero byte values to terminate Unicode strings.

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
        s = -1; // init to error
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
// Specializations for Java String <-> const char * type conversion
// ---------------------------------------------------------------------------

// extend String specializations to const pointers
template< typename C >
struct Param< jstring, C * const > 
    : Param< jstring, C * > {};
template< typename C >
struct Result< jstring, C * const >
    : Result< jstring, C * > {};

// specialize Java Strings mapped to C strings:
template<>
struct Param< jstring, const char * >
    : ParamStringT< jstring, const char * > {};
template<>
struct Result< jstring, const char * >
    : ResultStringT< jstring, const char * > {};

// ---------------------------------------------------------------------------
// StringBuilder
// ---------------------------------------------------------------------------

/*
// define char* conversions to/from StringBuilder
// this type mapping is inefficient due to multiple copying operations
// but should serve as a more complex example

// defining a new type j_l_StringBuilder as an alias and leads to conflicting
// declaration errors when instantiating the function templates
//typedef jobject j_l_StringBuilder;

// defining *j_l_StringBuilder as a subclass of *jobject would rely on
// type name conventions in the JNI header jni.h
//class _j_l_StringBuilder : public _jobject {};
//typedef _j_l_StringBuilder *j_l_StringBuilder;

// so, we define j_l_StringBuilder as unrelated type and apply approriate
// reinterpret_casts where dealing with jobject
class _j_l_StringBuilder {};
typedef _j_l_StringBuilder *j_l_StringBuilder;

template<>
struct Param< j_l_StringBuilder, char * > {
    static int
    convert(JNIEnv * env, char * & c, j_l_StringBuilder const & j) {
        TRACE("int Param.convert(JNIEnv *, char * &, j_l_StringBuilder const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get the StringBuilder class object
        jclass sbClass = env->FindClass("java/lang/StringBuilder");
        if (sbClass != NULL) {

            // get the method ID for the StringBuilder.toString() method
            jmethodID mid = env->GetMethodID(sbClass, "toString",
                                             "()Ljava/lang/String;");
            if (mid != NULL) {

                // get a String from the StringBuilder object
                jobject jo = reinterpret_cast<jobject>(j);
                jobject jso = env->CallObjectMethod(jo, mid);
                if (env->ExceptionCheck() == JNI_OK) {

                    // get length in bytes of String, does not throw exceptions
                    jstring js = static_cast<jstring>(jso);
                    const jsize n = env->GetStringUTFLength(js);

                    // allocate C string to hold UTF-8 copy of String
                    c = new char[n];
                    if (c != NULL) {
                        // copy String as UTF-8 into the C target string
                        env->GetStringUTFRegion(js, 0, n, c);
                        if (env->ExceptionCheck()) {
                            // release the C string
                            delete[] c;
                            c = NULL;
                        }
                        assert(c[n - 1] == '\0');
                    } else {
                        // old C++ compilers may not raise an exception
                        jclass oomec
                            = env->FindClass("java/lang/OutOfMemoryError");
                        if (oomec != NULL) {
                            env->ThrowNew(
                                oomec,
                                "JNI wrapper: failed to allocate memory, "
                                " file: " __FILE__);
                            env->DeleteLocalRef(oomec);
                        }
                    }
                    env->DeleteLocalRef(jso);
                }
            }
            env->DeleteLocalRef(sbClass);
        }

        return (c == NULL);
    }

    static void
    release(JNIEnv * env, char * & c, j_l_StringBuilder const & j) {
        TRACE("void Param.release(JNIEnv *, char * &, j_l_StringBuilder const &)");
        if (c == NULL) {
            assert(j == NULL);
            return;
        }
        assert(j);

        // release the C string
        delete[] c;
    }
};

template<>
struct Result< j_l_StringBuilder, char * > {
    static j_l_StringBuilder
    convert(JNIEnv * env, char * const & c) {
        TRACE("j_l_StringBuilder Result.convert(JNIEnv *, char * const &)");

        // init target, even in case of errors (better: use exceptions)
        j_l_StringBuilder j = NULL;
        if (c == NULL)
            return j;

        // construct a String object from a UTF-8 C string
        jstring js = env->NewStringUTF(c);
        if (js != NULL) {

            // get the StringBuilder class object
            jclass sbClass = env->FindClass("java/lang/StringBuilder");
            if (sbClass != NULL) {

                // get the method ID for the StringBuilder(String) constructor
                jmethodID cid = env->GetMethodID(sbClass, "<init>",
                                                 "(Ljava/lang/String;)V");
                if (cid != NULL) {

                    // construct a StringBuilder object from a String
                    jobject jo = env->NewObject(sbClass, cid, js);
                    j = reinterpret_cast<j_l_StringBuilder>(jo);
                }
                env->DeleteLocalRef(sbClass);
            }
            env->DeleteLocalRef(js);
        }
        return j;
    }
};
*/

// ---------------------------------------------------------------------------

#endif // jtie_tconv_string_impl_hpp
