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
 * demoj_A.cpp
 */

#include <jni.h>
#include <cassert>
#include <cstdio>
#include "helpers.hpp"
#include "demo.hpp"
#include "demoj_A.h"

// ---------------------------------------------------------------------------
// generatable, application-dependent code: API JNI function stubs
// ---------------------------------------------------------------------------

// implements demoj.A method: static double simple(double p0)
// demonstrates the simple case where no type conversions are necessary
JNIEXPORT jdouble JNICALL
Java_demoj_A_simple(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("jdouble Java_demoj_A_simple(JNIEnv *, jclass, jdouble)");
    return simple(p0);
}

// implements demoj.A method: static void print(String p0)
// demonstrates a parameter type conversion
JNIEXPORT void JNICALL
Java_demoj_A_print__Ljava_lang_String_2(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("void Java_demoj_A_print__Ljava_lang_String_2(JNIEnv *, jclass, jstring)");

    // a status flag indicating any error (!= 0)
    int s = -1;

    // convert parameter p0 from Java to C
    jstring j = p0;
    const char * c = NULL;
    if (j == NULL) {
        // ok
        s = 0;
    } else {        
        // get a const UTF-8 string, to be released by ReleaseStringUTFChars()
        // ignore whether C string is pinned or a copy of Java string
        c = env->GetStringUTFChars(j, NULL); 
        if (c == NULL) {
            // exception pending with VM
            // only a very limited set of JNI functions may be called
            // and no other parameter conversions may be attempted
        } else {
            // ok
            s = 0;
        }
    }
    if (s == 0) {

        // convert other parameters (if there were any) from Java to C
        // ...
        // if (s == 0) {

        // call the delegate function with converted arguments
        A::print(c);

        // ...
        // release resources for other parameters (if there were any)

        // release resources for parameter p0
        if (c == NULL) {
            assert(j == NULL);
        } else {
            assert(j);
            // release the UTF-8 string allocated by GetStringUTFChars()
            env->ReleaseStringUTFChars(j, c);
        }
    }    
}

// implements demoj.A method: static A getA();
// demonstrates a result type conversion
JNIEXPORT jobject JNICALL
Java_demoj_A_getA(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_demoj_A_getA(JNIEnv *, jclass)");

    // call the delegate function
    A * c = A::getA();

    // convert result from C to Java
    jobject j = NULL;
    if (c == NULL) {
        // ok
    } else {
        // get the result class object
        const char * jicn = "demoj/A";
        jclass cls = env->FindClass(jicn);
        if (cls == NULL) {
            // exception pending
        } else {
            // get the method ID for the constructor
            // XXX optimize: use field access, cache fid/cid
            jmethodID cid = env->GetMethodID(cls, "<init>", "(J)V");
            if (cid == NULL) {
                // exception pending with VM
                // only a very limited set of JNI functions may be called
            } else {
                // convert a pointer into a jlong via intptr_t (C99)
                //assert (sizeof(jlong) >= sizeof(intptr_t));
                intptr_t ip = reinterpret_cast< intptr_t >(c);
                jlong p = ip;

                // construct a Wrapper object
                jobject jo = env->NewObject(cls, cid, p);
                if (jo == NULL) {
                    // exception pending with VM
                    // only a very limited set of JNI functions may be called
                } else {
                    // ok
                    j = jo;
                }
            }
            env->DeleteLocalRef(cls);
        }
    }
    return j;
}

// implements demoj.A method: void print()
// demonstrates a target object type conversion for a member function call
JNIEXPORT void JNICALL
Java_demoj_A_print__(JNIEnv * env, jobject obj)
{
    TRACE("void Java_demoj_A_print__(JNIEnv *, jobject)");

    // a status flag indicating any error (!= 0)
    int s = -1;

    // retrieve the C target object for member function call from Java wrapper
    jobject j = obj;
    A * c = NULL;
    if (j == NULL) {
        // raise exception
        jclass iae = env->FindClass("java/lang/IllegalArgumentException");
        if (iae == NULL) {
            // exception pending with VM
            // only a very limited set of JNI functions may be called
            // and no other parameter conversions may be attempted
        } else {
            env->ThrowNew(iae,
                          "JNI wrapper: Java object reference must not be null"
                          " when target of a member function call"
                          " (file: " __FILE__ ")");
            env->DeleteLocalRef(iae);
            // exception pending with VM
            // only a very limited set of JNI functions may be called
            // and no other parameter conversions may be attempted
        }
    } else {
        // get the class object
        jclass cls = env->FindClass("demoj/A");
        if (cls == NULL) {
            // exception pending with VM
            // only a very limited set of JNI functions may be called
            // and no other parameter conversions may be attempted
        } else {
            // get the field ID
            // XXX optimize, cache fid
            jfieldID fid = env->GetFieldID(cls, "cdelegate", "J");
            if (fid == NULL) {
                // exception pending with VM
                // only a very limited set of JNI functions may be called
                // and no other parameter conversions may be attempted
            } else {
                // get the value
                jlong p = env->GetLongField(j, fid);

                // convert a jlong back into a pointer via intptr_t (C99)
                //assert (sizeof(jlong) <= sizeof(intptr_t));
                intptr_t ip = p;
                c = reinterpret_cast< A * >(ip);

                // ok
                s = 0;
            }
            env->DeleteLocalRef(cls);
        }
    }
    
    // call the delegate member function on target object
    if (s == 0) {
        assert (c);
        c->print();
    }
}

// ---------------------------------------------------------------------------
