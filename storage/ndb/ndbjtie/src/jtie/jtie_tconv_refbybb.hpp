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
 * jtie_tconv_refbybb.hpp
 */

#ifndef jtie_tconv_refbybb_hpp
#define jtie_tconv_refbybb_hpp

#include <jni.h>
#include "helpers.hpp"
#include "jtie_tconv_def.hpp"

// ---------------------------------------------------------------------------
// infrastructure code: Java ByteBuffer <-> C & type conversions
// ---------------------------------------------------------------------------

/*
template< int n = 0 >
struct _j_n_ByteBuffer {
    static inline const int capacity = n;
};
typedef _j_n_ByteBuffer<> * j_n_ByteBuffer;
*/

class _j_n_ByteBuffer {};
typedef _j_n_ByteBuffer * j_n_ByteBuffer;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, j_n_ByteBuffer >(j_n_ByteBuffer s) {
    TRACE("jobject cast(j_n_ByteBuffer)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline j_n_ByteBuffer
cast< j_n_ByteBuffer, jobject >(jobject s) {
    TRACE("j_n_ByteBuffer cast(jobject)");
    return reinterpret_cast< j_n_ByteBuffer >(s);
}

// ---------------------------------------------------------------------------

inline cstatus
ensureNonNullBuffer(jobject jo, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;
    
    if (jo == NULL) {
        // raise exception
        jclass iae = env->FindClass("java/lang/IllegalArgumentException");
        if (iae == NULL) {
            // exception pending
        } else {
            env->ThrowNew(iae,
                          "JNI wrapper: java.nio.ByteBuffer cannot be null"
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
ensureMutableBuffer(jobject jo, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;
    
    // get the ByteBuffer class object
    jclass sbClass = env->FindClass("java/nio/ByteBuffer");
    if (sbClass == NULL) {
        // exception pending
    } else {
        // get the method ID for the ByteBuffer.isReadOnlyBuffer() method
        jmethodID mid = env->GetMethodID(sbClass, "isReadOnly", "()Z");
        if (mid == NULL) {
            // exception pending
        } else {
            // error if the ByteBuffer is read-only
            jboolean ro = env->CallBooleanMethod(jo, mid);
            if (env->ExceptionCheck() != JNI_OK) {
                // exception pending
            } else {
                if (ro) {
                    // raise exception
                    jclass robe
                        = env->FindClass("java/nio/ReadOnlyBufferException");
                    if (robe == NULL) {
                        // exception pending
                    } else {
                        env->ThrowNew(
                            robe,
                            "JNI wrapper: java.nio.ByteBuffer cannot be"
                            " read-only when mapped to a non-const object"
                            " reference type (file: " __FILE__ ")");
                        env->DeleteLocalRef(robe);
                    }
                } else {
                    // ok
                    s = 0;
                }
            }
        }
        env->DeleteLocalRef(sbClass);
    }
    return s;
}

template< typename C >
struct Param< j_n_ByteBuffer, C & > {

    static C &
    convert(cstatus & s, j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("C & Param.convert(cstatus &, j_n_ByteBuffer, JNIEnv *)");

        // init return value and status to error
        C * c = NULL;

        jobject jo = cast< jobject, j_n_ByteBuffer >(j);
        if (ensureNonNullBuffer(jo, env) != 0) {
            // exception pending
        } else {
            if (ensureMutableBuffer(jo, env) != 0) {
                // exception pending
            } else {
                // get the internal buffer address of direct ByteBuffer
                void * cb = env->GetDirectBufferAddress(jo);
                c = static_cast<C *>(cb);
            }
        }
        s = (c == NULL);
        return *c;
    }

    static void
    release(C & c, j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("void Param.release(C &, j_n_ByteBuffer, JNIEnv *)");
    }
};

template< typename C >
struct Param< j_n_ByteBuffer, const C & > {

    static const C &
    convert(cstatus & s, j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("const C & Param.convert(cstatus &, j_n_ByteBuffer, JNIEnv *)");

        // init return value and status to error
        const C * c = NULL;

        jobject jo = cast< jobject, j_n_ByteBuffer >(j);
        if (ensureNonNullBuffer(jo, env) != 0) {
            // exception pending
        } else {
            // get the internal buffer address of direct ByteBuffer
            void * cb = env->GetDirectBufferAddress(jo);
            c = static_cast<C *>(cb);
        }
        s = (c == NULL);
        return *c;
    }

    static void
    release(const C & c, j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("void Param.release(const C &, j_n_ByteBuffer, JNIEnv *)");
    }
};

template< typename C >
inline jobject 
wrapReferenceAsByteBuffer(C & c, JNIEnv * env) {
    // construct a direct java.nio.ByteBuffer wrapping the source
    void * mc = static_cast< void * >(&c);
    jlong cap = sizeof(C);
    jobject jo = env->NewDirectByteBuffer(mc, cap);
    return jo;
}

template< typename C >
struct Result< j_n_ByteBuffer, C & > {
    static j_n_ByteBuffer
    convert(C & c, JNIEnv * env) {
        TRACE("j_n_ByteBuffer Result.convert(C &, JNIEnv *)");

        // init return value to error
        j_n_ByteBuffer j = NULL;

        jobject jo = wrapReferenceAsByteBuffer(c, env);
        if (jo == NULL) {
            // exception pending
        } else {
            j = cast< j_n_ByteBuffer, jobject >(jo);
        }
        return j;
    }
};

inline jobject
wrapByteBufferAsReadOnly(jobject jo, JNIEnv * env) {
    // init return value to error
    jobject j = NULL;
    
    // get the ByteBuffer class object
    jclass sbClass = env->FindClass("java/nio/ByteBuffer");
    if (sbClass == NULL) {
        // exception pending
    } else {
        // get the method ID of ByteBuffer.asReadOnlyBuffer()
        jmethodID mid = env->GetMethodID(sbClass, "asReadOnlyBuffer",
                                         "()Ljava/nio/ByteBuffer;");
        if (mid == NULL) {
            // exception pending
        } else {
            // get a read-only copy from the ByteBuffer object
            j = env->CallObjectMethod(jo, mid);
        }
        env->DeleteLocalRef(sbClass);
    }
    return j;
}

template< typename C >
struct Result< j_n_ByteBuffer, const C & > {
    static j_n_ByteBuffer
    convert(const C & c, JNIEnv * env) {
        TRACE("j_n_ByteBuffer Result.convert(const C &, JNIEnv *)");

        // init return value to error
        j_n_ByteBuffer j = NULL;

        // ok to temporarily strip const, wrapping as read-only buffer
        C & mc = const_cast< C & >(c);
        jobject jo = wrapReferenceAsByteBuffer(mc, env);
        if (jo == NULL) {
            // exception pending
        } else {
            jobject jro = wrapByteBufferAsReadOnly(jo, env);
            if (jro == NULL) {
                // exception pending
            } else {
                j = cast< j_n_ByteBuffer, jobject >(jro);
            }
            env->DeleteLocalRef(jo);
        }
        return j;
    }
};

// ---------------------------------------------------------------------------

/*
template<>
struct Param< j_n_ByteBuffer, char * > {
    static int
    convert(JNIEnv * env, char * & c, j_n_ByteBuffer const & j) {
        TRACE("int Param.convert(JNIEnv *, char * &, j_n_ByteBuffer const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get the ByteBuffer class object
        jclass sbClass = env->FindClass("java/nio/ByteBuffer");
        if (sbClass != NULL) {

            // get the method ID for the ByteBuffer.asReadOnlyBuffer() method
            jmethodID mid = env->GetMethodID(sbClass, "isReadOnly", "()Z");
            if (mid != NULL) {

                // error if the ByteBuffer is read-only
                jobject jo = reinterpret_cast< jobject >(j);
                jboolean ro = env->CallBooleanMethod(jo, mid);
                if (!ro) {
                    // get the internal buffer address of direct ByteBuffer
                    void * cb = env->GetDirectBufferAddress(jo);
                    c = static_cast<char *>(cb);
                } else {
                    jclass robe
                        = env->FindClass("java/nio/ReadOnlyBufferException");
                    if (robe != NULL) {
                        env->ThrowNew(
                            robe,
                            "JNI wrapper: cannot retrieve a non-const buffer "
                            "address from a read-only java.nio.ByteBuffer, "
                            " file: " __FILE__);
                        env->DeleteLocalRef(robe);
                    }
                }
            }
            env->DeleteLocalRef(sbClass);
        }

        return (c == NULL);
    }

    static void
    release(JNIEnv * env, char * & c, j_n_ByteBuffer const & j) {
        TRACE("void Param.release(JNIEnv *, char * &, j_n_ByteBuffer const &)");
    }
};

template<>
struct Result< j_n_ByteBuffer, char * > {
    static j_n_ByteBuffer
    convert(JNIEnv * env, char * const & c) {
        TRACE("j_n_ByteBuffer Result.convert(JNIEnv *, char * const &)");

        // init target, even in case of errors (better: use exceptions)
        j_n_ByteBuffer j = NULL;
        if (c == NULL)
            return j;

        // construct a direct java.nio.ByteBuffer wrapping the source
        // XXX how large to choose capacity?
        //jlong cap = (1<<31) - 1;
        // XXX SECURITY HOLE!!!
        jlong cap = 0;
        //jlong cap = j->capacity;
        jobject jo = env->NewDirectByteBuffer(c, cap);
        j = reinterpret_cast< j_n_ByteBuffer >(jo);
        return j;
    }
};
*/

// ---------------------------------------------------------------------------

/*
template<>
struct Param< j_n_ByteBuffer, void * > {
    static int
    convert(JNIEnv * env, void * & c, j_n_ByteBuffer const & j) {
        TRACE("int Param.convert(JNIEnv *, void * &, j_n_ByteBuffer const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get the ByteBuffer class object
        jclass sbClass = env->FindClass("java/nio/ByteBuffer");
        if (sbClass != NULL) {

            // get the method ID for the ByteBuffer.isReadOnlyBuffer() method
            jmethodID mid = env->GetMethodID(sbClass, "isReadOnly", "()Z");
            if (mid != NULL) {

                // error if the ByteBuffer is read-only
                jobject jo = reinterpret_cast< jobject >(j);
                jboolean ro = env->CallBooleanMethod(jo, mid);
                if (!ro) {
                    // get the internal buffer address of direct ByteBuffer
                    void * cb = env->GetDirectBufferAddress(jo);
                    c = static_cast<void *>(cb);
                } else {
                    jclass robe
                        = env->FindClass("java/nio/ReadOnlyBufferException");
                    if (robe != NULL) {
                        env->ThrowNew(
                            robe,
                            "JNI wrapper: cannot retrieve a non-const buffer "
                            "address from a read-only java.nio.ByteBuffer, "
                            " file: " __FILE__);
                        env->DeleteLocalRef(robe);
                    }
                }
            }
            env->DeleteLocalRef(sbClass);
        }

        return (c == NULL);
    }

    static void
    release(JNIEnv * env, void * & c, j_n_ByteBuffer const & j) {
        TRACE("void Param.release(JNIEnv *, void * &, j_n_ByteBuffer const &)");
    }
};

template<>
struct Result< j_n_ByteBuffer, void * > {
    static j_n_ByteBuffer
    convert(JNIEnv * env, void * const & c) {
        TRACE("j_n_ByteBuffer Result.convert(JNIEnv *, void * const &)");

        // init target, even in case of errors (better: use exceptions)
        j_n_ByteBuffer j = NULL;
        if (c == NULL)
            return j;

        // construct a direct java.nio.ByteBuffer wrapping the source
        // XXX how large to choose capacity?
        //jlong cap = (1<<31) - 1;
        // XXX SECURITY HOLE!!!
        jlong cap = 0;
        jobject jo = env->NewDirectByteBuffer(c, cap);
        j = reinterpret_cast< j_n_ByteBuffer >(jo);
        return j;
    }
};
*/

// ---------------------------------------------------------------------------

/*
template<>
struct Param< j_n_ByteBuffer, const void * > {
    static int
    convert(JNIEnv * env, const void * & c, j_n_ByteBuffer const & j) {
        TRACE("int Param.convert(JNIEnv *, const void * &, j_n_ByteBuffer const &)");

        // init target, even in case of errors (better: use exceptions)
        c = NULL;
        if (j == NULL)
            return 0;

        // get the internal buffer address of the direct java.nio.ByteBuffer
        jobject jo = reinterpret_cast< jobject >(j);
        void * cb = env->GetDirectBufferAddress(jo);
        c = static_cast<const void *>(cb);

        return (c == NULL);
    }

    static void
    release(JNIEnv * env, const void * & c, j_n_ByteBuffer const & j) {
        TRACE("void Param.release(JNIEnv *, const void * &, j_n_ByteBuffer const &)");
    }
};

template<>
struct Result< j_n_ByteBuffer, const void * > {
    static j_n_ByteBuffer
    convert(JNIEnv * env, const void * const & c) {
        TRACE("j_n_ByteBuffer Result.convert(JNIEnv *, const void * const &)");

        // init target, even in case of errors (better: use exceptions)
        j_n_ByteBuffer j = NULL;
        if (c == NULL)
            return j;

        // ok to remove constness here since wrapping with a read-only buffer
        void * mc = const_cast<void *>(c);

        // construct a direct java.nio.ByteBuffer wrapping the source
        // XXX how large to choose capacity?
        //jlong cap = (1<<31) - 1;
        // XXX SECURITY HOLE!!!
        jlong cap = 0;
        jobject jo = env->NewDirectByteBuffer(mc, cap);
        if (jo != NULL) {

            // get the ByteBuffer class object
            jclass sbClass = env->FindClass("java/nio/ByteBuffer");
            if (sbClass != NULL) {

                // get the method ID of ByteBuffer.asReadOnlyBuffer()
                jmethodID mid = env->GetMethodID(sbClass, "asReadOnlyBuffer",
                                                 "()Ljava/nio/ByteBuffer;");
                if (mid != NULL) {
                    // get a read-only copy from the ByteBuffer object
                    jobject jro = env->CallObjectMethod(jo, mid);
                    j = reinterpret_cast< j_n_ByteBuffer >(jro);
                }
                env->DeleteLocalRef(sbClass);
            }
            env->DeleteLocalRef(jo);
        }

        return j;
    }
};
*/

// ---------------------------------------------------------------------------

#endif // jtie_tconv_refbybb_hpp
