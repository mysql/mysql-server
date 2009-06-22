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
 * jtie_tconv_cobject.hpp
 */

#ifndef jtie_tconv_cobject_hpp
#define jtie_tconv_cobject_hpp

#include <stdint.h>
#include <cstdio>
#include <cassert>
#include <jni.h>
//#include "helpers.hpp"
#include "jtie_tconv_def.hpp"

// ---------------------------------------------------------------------------
// Java object <-> C object type conversions
// ---------------------------------------------------------------------------

/*
// partial specialization not allowed for function templates...

// a function template for simple type adjustments by casts
template< typename T >
inline T &
cast< T, T * >(T * t) {
    TRACE("T & cast(T *)");
    return *t; // type conversions supported by C++
}

// this doesn't match A& <- A* either:

// a function template for simple type adjustments by casts
template< typename T, typename S >
inline T &
cast(S * s) {
    TRACE("T & cast(S *)");
    return *s; // type conversions supported by C++
}
*/

inline void
detachWrapper(jobject jo, JNIEnv * env) {
    // XXX OPTIMIZE: use field access instead of method call
    // XXX optimize, cache mid
    // get the class object
    jclass cls = env->FindClass("jtie/Wrapper");
    if (cls == NULL) {
        // exception pending
    } else {
        // get the method ID
        jmethodID mid = env->GetMethodID(cls, "detach", "()V");
        if (mid == NULL) {
            // exception pending
        } else {
            env->CallVoidMethod(jo, mid);
            if (env->ExceptionCheck() != JNI_OK) {
                // exception pending
            } else {
                // ok
            }
        }
        env->DeleteLocalRef(cls);
    }
}

template< typename C >
struct Param< jobject, C & > {
    static C &
    convert(cstatus & s, jobject j, JNIEnv * env) {
        TRACE("C & Param.convert(cstatus &, jobject, JNIEnv *)");
        s = -1; // init to error
        C * c = NULL;

        if (j == NULL) {
            // raise exception
            jclass iae = env->FindClass("java/lang/IllegalArgumentException");
            if (iae == NULL) {
                // exception pending
            } else {
                env->ThrowNew(iae,
                              "JNI wrapper: Java argument must not be null"
                              " when mapped to a C reference"
                              " (file: " __FILE__ ")");
                env->DeleteLocalRef(iae);
                // exception pending
            }
        } else {
            // sets status
            c = Param< jobject, C * >::convert(s, j, env);
        }

        return *c;
    };

    static void
    release(C & c, jobject j, JNIEnv * env) {
        TRACE("void Param.release(C &, jobject, JNIEnv *)");
        Param< jobject, C * >::release(&c, j, env);
    };
};

template< typename C >
struct Param< jobject, C * > {
    static C *
    convert(cstatus & s, jobject j, JNIEnv * env) {
        TRACE("C * Param.convert(cstatus &, jobject, JNIEnv *)");        
        s = -1; // init to error
        C * c = NULL;

        if (j == NULL) {
            // ok
            s = 0;
        } else {
            // XXX OPTIMIZE: use field access instead of method call
            // XXX optimize, cache fid
            // get the class object
            jclass cls = env->FindClass("jtie/Wrapper");
            if (cls == NULL) {
                // exception pending
            } else {
                // get the field ID
                jfieldID fid = env->GetFieldID(cls, "cdelegate", "J");
                if (fid == NULL) {
                    // exception pending
                } else {
                    // get the value
                    jlong p = env->GetLongField(j, fid);
                    // i/o problems with gcc 4.4.0 printing a number
                    //cout << "p = " << p << endl;
                    //printf("    c = %lx\n", (unsigned long)p);

                    // convert a jlong back into a pointer via intptr_t (C99)
                    //assert (sizeof(jlong) <= sizeof(intptr_t));
                    intptr_t ip = p;
                    c = reinterpret_cast< C * >(ip);
                    //printf("    c = %p\n", c);

                    // ok
                    s = 0;
                }
                env->DeleteLocalRef(cls);
            }
        }
        return c;
    };

    static void
    release(C * c, jobject j, JNIEnv * env) {
        TRACE("void Param.release(C *, jobject, JNIEnv *)");
        // i/o problems with gcc 4.4.0 printing a number
        //cout << "c = " << (int)c << endl;
        //printf("    c = %lx\n", (unsigned long)c);
    };
};

template< typename J , typename C >
struct Result< J *, C & > {
    static J *
    convert(C & c, JNIEnv * env) {
        TRACE("J * Result.convert(JNIEnv *, C &)");        
        J * j = NULL; // init to error
        C * p = &c;

        if (p == NULL) {
            // raise exception
            jclass ae = env->FindClass("java/lang/AssertionError");
            if (ae == NULL) {
                // exception pending
            } else {
                env->ThrowNew(ae,
                              "JNI wrapper: returned C reference must not be"
                              " null (for instance, did a memory allocation"
                              " fail without raising an exception, as can"
                              " happen with older C++ compilers?)"
                              " (file: " __FILE__ ")");
                env->DeleteLocalRef(ae);
                // exception pending
            }
        } else {
            // ok
            j = Result< J *, C * >::convert(p, env);
            assert(j != NULL);
        }
        return j;
    }
};

// cannot partially specialize on jobject, for we need the classname
template< typename J, typename C >
struct Result< J *, C * > { // XXX specialization v inheritance
    
    static J *
    convert(C * c, JNIEnv * env) {
        TRACE("J * Result.convert(JNIEnv *, C *)");
        J * j = NULL;

        if (c == NULL) {
            // ok
        } else {
            // get the result class object
            const char * jicn = J::java_internal_class_name;
            //cout << "jicn = '" << jicn << "'" << endl;
            jclass cls = env->FindClass(jicn);
            if (cls == NULL) {
                // exception pending
            } else {
                // get the method ID for the constructor
                //jmethodID cid = env->GetMethodID(cls, "<init>", "()V");
                jmethodID cid = env->GetMethodID(cls, "<init>", "(J)V");
                if (cid == NULL) {
                    // exception pending
                } else {
                    // convert a pointer into a jlong via intptr_t (C99)
                    //assert (sizeof(jlong) >= sizeof(intptr_t));
                    //printf("    p = %p\n", c);
                    intptr_t ip = reinterpret_cast< intptr_t >(c);
                    jlong p = ip;

                    // construct a Wrapper object
                    jobject jo = env->NewObject(cls, cid, p);
                    if (jo == NULL) {
                        // exception pending
                    } else {
                        // ok
                        j = cast< J *, jobject >(jo);
                    }
/*
                    // get the field ID
                    jfieldID fid = env->GetFieldID(cls, "cdelegate", "J");
                    if (fid == NULL) {
                        // exception pending
                        env->DeleteLocalRef(jo);
                    } else {
                        // ok

                        // convert a pointer into a jlong via intptr_t (C99)
                        //assert (sizeof(jlong) >= sizeof(intptr_t));
                        //printf("    p = %p\n", c);
                        intptr_t ip = reinterpret_cast< intptr_t >(c);
                        jlong p = ip;
                        // i/o problems with gcc 4.4.0 printing number
                        //cout << "p = " << p << endl;
                        printf("    p = %lx\n", (unsigned long)p);

                        // set the field's value
                        env->SetLongField(jo, fid, p);
                        if (env->ExceptionCheck() != JNI_OK) {
                            env->DeleteLocalRef(jo);
                        } else {
                            j = cast< J *, jobject >(jo);
                        }
                    }
*/
                }
                env->DeleteLocalRef(cls);
            }
        }
        return j;
    }
};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_cobject_hpp
