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
 * jtie_tconv.hpp
 */

#ifndef jtie_tconv_hpp
#define jtie_tconv_hpp

#include <jni.h>
#include "helpers.hpp"
#include "jtie_tconv_def.hpp"

// XXX misc type mappings -- need to be adapted to new scheme

// ---------------------------------------------------------------------------
// BigInteger
// ---------------------------------------------------------------------------

/*
// returns a value in big endian format
template<typename C>
C
big_endian(C c)
{
    // test if big or little endian
    C r = 1;
    if(*(char *)&r == 0) {
        // big endian, ok
        return c;
    }

    // little-endian, reverse byte order (better: use optimized swap macros)
    const size_t n = sizeof(C);
    char *s = (char *)&c;
    char *t = (char *)&r;
    for (int i = n-1, j = 0; i >= 0; i--, j++)
        t[j] = s[i];
    return r;
}

class _j_m_BigInteger {};
typedef _j_m_BigInteger * j_m_BigInteger;

template< typename I >
struct Result< j_m_BigInteger, I > {
    static j_m_BigInteger
    convert(JNIEnv * env, I const & c) {
        TRACE("j_m_BigInteger Result.convert(JNIEnv *, I const &)");

        // init target, even in case of errors (better: use exceptions)
        j_m_BigInteger j = NULL;

        // construct a BigInteger object from a two-complements byte array
        const jsize n = sizeof(I);
        jbyteArray ja = env->NewByteArray(n);
        if (ja != NULL) {

            // ensure the byte array is in the expected big-endian format
            I cbe = big_endian(c);

            // copy the two's-compliment into the byte array
            const jbyte * b = reinterpret_cast<const jbyte *>(&cbe);
            env->SetByteArrayRegion(ja, 0, n, b);
            if (env->ExceptionCheck() == JNI_OK) {

                // get the BigInteger class object
                jclass sbClass = env->FindClass("java/math/BigInteger");
                if (sbClass != NULL) {

                    // get the method ID for the BigInteger(byte[]) constructor
                    jmethodID cid = env->GetMethodID(sbClass, "<init>",
                                                     "([B)V");
                    if (cid != NULL) {

                        // construct a BigInteger object from a byte array
                        jobject jo = env->NewObject(sbClass, cid, ja);
                        j = reinterpret_cast< j_m_BigInteger >(jo);
                    }
                    env->DeleteLocalRef(sbClass);
                }
            }
            env->DeleteLocalRef(ja);
        }

        return j;
    }
};
*/

// ---------------------------------------------------------------------------
// jstring
// ---------------------------------------------------------------------------

/*
template<>
struct Param< jstring, const char * > {
    static int
    convert(JNIEnv * env, const char * & c, jstring const & j) {
        TRACE("int Param.convert(JNIEnv *, const char * &, jstring const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get a const UTF-8 string, to be released by ReleaseStringUTFChars()
        c = env->GetStringUTFChars(j, NULL);
        return (c == NULL);
    }

    static void
    release(JNIEnv * env, const char * & c, jstring const & j) {
        TRACE("void Param.release(JNIEnv *, const char * &, jstring const &)");
        if (c == NULL) {
            assert(j == NULL);
            return;
        }
        assert(j);

        // release the UTF-8 string allocated by GetStringUTFChars()
        env->ReleaseStringUTFChars(j, c);
    }
};

template<>
struct Result< jstring, const char * > {
    static jstring
    convert(JNIEnv * env, const char * const & c) {
        TRACE("jstring Result.convert(JNIEnv *, const char * const &)");
        if (c == NULL)
            return NULL;

        // construct a String object from a UTF-8 C string
        return env->NewStringUTF(c);
    }
};
*/

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

#endif // jtie_tconv_hpp
