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
 * myjapi_classes.cpp
 */

#include <stdint.h>
#include <jni.h>
#include "helpers.hpp"
#include "myapi.hpp"
#include "myjapi_A.h"
#include "myjapi_B0.h"
#include "myjapi_B1.h"
#include "jtie_ttrait.hpp"
#include "jtie_tconv.hpp"
#include "jtie_tconv_cvalue.hpp"
#include "jtie_tconv_carray.hpp"
#include "jtie_tconv_cstring.hpp"
#include "jtie_tconv_refbybb.hpp"
#include "jtie_tconv_refbyval.hpp"
#include "jtie_tconv_cobject.hpp"
#include "jtie_gcalls.hpp"

// ---------------------------------------------------------------------------
// generatable, application-dependent code: API JNI function stubs
// ---------------------------------------------------------------------------

//
// Type conversion definitions for class myapi.A
//

struct _m_A {
    static const char * const java_internal_class_name;
};
const char * const _m_A::java_internal_class_name = "myjapi/A";
typedef _m_A * m_A;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, m_A >(m_A s) {
    TRACE("jobject cast(m_A)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline m_A
cast< m_A, jobject >(jobject s) {
    TRACE("m_A cast(jobject)");
    return reinterpret_cast< m_A >(s);
}

// type mapping aliases
typedef ttrait< jobject, A, jobject, A & > ttrait_myjapi_A_target;
typedef ttrait< jobject, A *, m_A, A * > ttrait_myjapi_A_ptr_return;
typedef ttrait< jobject, A &, m_A, A & > ttrait_myjapi_A_ref_return;
typedef ttrait< jobject, A *, jobject, A * > ttrait_myjapi_A_ptr_arg;
typedef ttrait< jobject, A &, jobject, A & > ttrait_myjapi_A_ref_arg;

// constructor wrapper (return a reference for automatic Java exceptions)
A &
myjapi_A_create() {
    TRACE("A & myjapi_A_create()");
    A * r = new A();
    printf("    r = %p\n", r);
    return *r;
};

// destructor wrapper (take a reference for automatic Java exceptions)
void
myjapi_A_delete(A & p0) {
    TRACE("void myjapi_A_delete(A &)");
    printf("    p0 = %p\n", &p0);
    delete &p0;

};

// this trait definition has a declared but undefined Param match
// (default Param decl); hence, compilation passes but we get
// undefined symbols during linking:
// ttrait< jobject, A *, m_A, A * > ttrait_myjapi_A_ptr_arg;

//
// Type conversion definitions for class myapi.B0
//

struct _m_B0 {
    static const char * const java_internal_class_name;
};
const char * const _m_B0::java_internal_class_name = "myjapi/B0";
typedef _m_B0 * m_B0;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, m_B0 >(m_B0 s) {
    TRACE("jobject cast(m_B0)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline m_B0
cast< m_B0, jobject >(jobject s) {
    TRACE("m_B0 cast(jobject)");
    return reinterpret_cast< m_B0 >(s);
}

// type mapping aliases
typedef ttrait< jobject, B0, jobject, B0 & > ttrait_myjapi_B0_target;
typedef ttrait< jobject, B0 *, m_B0, B0 * > ttrait_myjapi_B0_ptr_return;
typedef ttrait< jobject, B0 &, m_B0, B0 & > ttrait_myjapi_B0_ref_return;
typedef ttrait< jobject, B0 *, jobject, B0 * > ttrait_myjapi_B0_ptr_arg;
typedef ttrait< jobject, B0 &, jobject, B0 & > ttrait_myjapi_B0_ref_arg;

//
// Type conversion definitions for class myapi.B1
//

struct _m_B1 {
    static const char * const java_internal_class_name;
};
const char * const _m_B1::java_internal_class_name = "myjapi/B1";
typedef _m_B1 * m_B1;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, m_B1 >(m_B1 s) {
    TRACE("jobject cast(m_B1)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline m_B1
cast< m_B1, jobject >(jobject s) {
    TRACE("m_B1 cast(jobject)");
    return reinterpret_cast< m_B1 >(s);
}

// type mapping aliases
typedef ttrait< jobject, B1, jobject, B1 & > ttrait_myjapi_B1_target;
typedef ttrait< jobject, B1 *, m_B1, B1 * > ttrait_myjapi_B1_ptr_return;
typedef ttrait< jobject, B1 &, m_B1, B1 & > ttrait_myjapi_B1_ref_return;
typedef ttrait< jobject, B1 *, jobject, B1 * > ttrait_myjapi_B1_ptr_arg;
typedef ttrait< jobject, B1 &, jobject, B1 & > ttrait_myjapi_B1_ref_arg;

// ---------------------------------------------------------------------------

JNIEXPORT jobject JNICALL
Java_myjapi_A_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_A_create(JNIEnv *, jclass)");
    return gcreate< ttrait_myjapi_A_ref_return, myjapi_A_create >(env);
}

JNIEXPORT void JNICALL
Java_myjapi_A_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_myjapi_A_ref_arg, myjapi_A_delete >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_f0s(JNIEnv * env, jclass cls) 
{
    TRACE("jint Java_myjapi_A_f0s(JNIEnv *, jclass)");
    return gcall< ttrait_int32, A::f0s >(env);
    //return 10;
}

JNIEXPORT jint JNICALL
Java_myjapi_A_f0n(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jint Java_myjapi_A_f0n(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, &A::f0n >(env, obj);
    //return 11;
}

JNIEXPORT jint JNICALL
Java_myjapi_A_f0v(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_myjapi_A_f0v(JNIEnv *, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, &A::f0v >(env, obj);
    //return 12;
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_getB0(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_myjapi_A_getB0(JNIEnv *, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_myjapi_B0_ptr_return, &A::getB0 >(env, obj);
    //return 0;
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_getB1(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_myjapi_A_getB1(JNIEnv *, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_myjapi_B1_ptr_return, &A::getB1 >(env, obj);
    //return 0;
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_return_1ptr(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_A_return_1ptr(JNIEnv *, jclass)");
    return gcall< ttrait_myjapi_A_ptr_return, A::return_ptr >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_return_1null_1ptr(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_A_return_1null_1ptr(JNIEnv *, jclass)");
    return gcall< ttrait_myjapi_A_ptr_return, A::return_null_ptr >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_return_1ref(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_A_return_1ref(JNIEnv *, jclass)");
    return gcall< ttrait_myjapi_A_ref_return, A::return_ref >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_A_return_1null_1ref(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_A_return_1null_1ref(JNIEnv *, jclass)");
    return gcall< ttrait_myjapi_A_ref_return, A::return_null_ref >(env);
}

JNIEXPORT void JNICALL
Java_myjapi_A_take_1ptr(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_take_1ptr(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_ptr_arg, A::take_ptr >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_take_1null_1ptr(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_take_1null_1ptr(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_ptr_arg, A::take_null_ptr >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_take_1ref(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_take_1ref(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_ref_arg, A::take_ref >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_take_1null_1ref(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_take_1null_1ref(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_ref_arg, A::take_null_ref >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_print(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_A_print(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_ptr_arg, A::print >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_B0_f0s(JNIEnv * env, jclass cls) 
{
    TRACE("jint Java_myjapi_B0_f0s(JNIEnv *, jclass)");
    return gcall< ttrait_int32, B0::f0s >(env);
    //return 20;
}

JNIEXPORT jint JNICALL
Java_myjapi_B0_f0n(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jint Java_myjapi_B0_f0n(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_myjapi_B0_target, ttrait_int32, &B0::f0n >(env, obj);
    //return 21;
}

JNIEXPORT jint JNICALL
Java_myjapi_B0_f0v(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_myjapi_B0_f0v(JNIEnv *, jobject)");
    return gcall< ttrait_myjapi_B0_target, ttrait_int32, &B0::f0v >(env, obj);
    //return 22;
}

JNIEXPORT jint JNICALL
Java_myjapi_B1_f0s(JNIEnv * env, jclass cls) 
{
    TRACE("jint Java_myjapi_B1_f0s(JNIEnv *, jclass)");
    return gcall< ttrait_int32, B1::f0s >(env);
    //return 30;
}

JNIEXPORT jint JNICALL
Java_myjapi_B1_f0n(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jint Java_myjapi_B1_f0n(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_myjapi_B1_target, ttrait_int32, &B1::f0n >(env, obj);
    //return 31;
}

JNIEXPORT jint JNICALL
Java_myjapi_B1_f0v(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_myjapi_B1_f0v(JNIEnv *, jobject)");
    return gcall< ttrait_myjapi_B1_target, ttrait_int32, &B1::f0v >(env, obj);
    //return 32;
}

// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_A_h0(JNIEnv * env, jclass cls)
{
    TRACE("void Java_myjapi_A_h0(JNIEnv *, jclass)");
    gcall< &h0 >(env);
}

JNIEXPORT void JNICALL
Java_myjapi_A_h1(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_A_h1(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_int8, &h1 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_h2(JNIEnv * env, jclass cls, jbyte p0, jshort p1)
{
    TRACE("void Java_myjapi_A_h2(JNIEnv *, jclass, jbyte, jshort)");
    gcall< ttrait_int8, ttrait_int16, &h2 >(env, p0, p1);
}

JNIEXPORT void JNICALL
Java_myjapi_A_h3(JNIEnv * env, jclass cls, jbyte p0, jshort p1, jint p2)
{
    TRACE("void Java_myjapi_A_h3(JNIEnv *, jclass, jbyte, jshort, jint)");
    gcall< ttrait_int8, ttrait_int16, ttrait_int32, &h3 >(env, p0, p1, p2);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_h0r(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_A_h0r(JNIEnv *, jclass)");
    return gcall< ttrait_int32, &h0r >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_h1r(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jint Java_myjapi_A_h1r(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_int32, ttrait_int8, &h1r >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_h2r(JNIEnv * env, jclass cls, jbyte p0, jshort p1)
{
    TRACE("jint Java_myjapi_A_h2r(JNIEnv *, jclass, jbyte, jshort)");
    return gcall< ttrait_int32, ttrait_int8, ttrait_int16, &h2r >(env, p0, p1);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_h3r(JNIEnv * env, jclass cls, jbyte p0, jshort p1, jint p2)
{
    TRACE("jint Java_myjapi_A_h3r(JNIEnv *, jclass, jbyte, jshort, jint)");
    return gcall< ttrait_int32, ttrait_int8, ttrait_int16, ttrait_int32, &h3r >(env, p0, p1, p2);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g0c(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("void Java_myjapi_A_g0c(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_target, &A::g0c >(env, obj);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g1c(JNIEnv * env, jclass cls, jobject obj, jbyte p0)
{
    TRACE("void Java_myjapi_A_g1c(JNIEnv *, jclass, jobject, jbyte)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, &A::g1c >(env, obj, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g2c(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1)
{
    TRACE("void Java_myjapi_A_g2c(JNIEnv *, jclass, jobject, jbyte, jshort)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, ttrait_int16, &A::g2c >(env, obj, p0, p1);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g3c(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1, jint p2)
{
    TRACE("void Java_myjapi_A_g3c(JNIEnv *, jclass, jobject, jbyte, jshort, jint)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, ttrait_int16, ttrait_int32, &A::g3c >(env, obj, p0, p1, p2);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g0(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("void Java_myjapi_A_g0(JNIEnv *, jclass, jobject)");
    gcall< ttrait_myjapi_A_target, &A::g0 >(env, obj);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g1(JNIEnv * env, jclass cls, jobject obj, jbyte p0)
{
    TRACE("void Java_myjapi_A_g1(JNIEnv *, jclass, jobject, jbyte)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, &A::g1 >(env, obj, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g2(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1)
{
    TRACE("void Java_myjapi_A_g2(JNIEnv *, jclass, jobject, jbyte, jshort)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, ttrait_int16, &A::g2 >(env, obj, p0, p1);
}

JNIEXPORT void JNICALL
Java_myjapi_A_g3(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1, jint p2)
{
    TRACE("void Java_myjapi_A_g3(JNIEnv *, jclass, jobject, jbyte, jshort, jint)");
    gcall< ttrait_myjapi_A_target, ttrait_int8, ttrait_int16, ttrait_int32, &A::g3 >(env, obj, p0, p1, p2);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g0rc(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jint Java_myjapi_A_g0rc(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, &A::g0rc >(env, obj);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g1rc(JNIEnv * env, jclass cls, jobject obj, jbyte p0)
{
    TRACE("jint Java_myjapi_A_g1rc(JNIEnv *, jclass, jobject, jbyte)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, &A::g1rc >(env, obj, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g2rc(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1)
{
    TRACE("jint Java_myjapi_A_g2rc(JNIEnv *, jclass, jobject, jbyte, jshort)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, ttrait_int16, &A::g2rc >(env, obj, p0, p1);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g3rc(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1, jint p2)
{
    TRACE("jint Java_myjapi_A_g3rc(JNIEnv *, jclass, jobject, jbyte, jshort, jint)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, ttrait_int16, ttrait_int32, &A::g3rc >(env, obj, p0, p1, p2);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g0r(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jint Java_myjapi_A_g0r(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, &A::g0r >(env, obj);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g1r(JNIEnv * env, jclass cls, jobject obj, jbyte p0)
{
    TRACE("jint Java_myjapi_A_g1r(JNIEnv *, jclass, jobject, jbyte)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, &A::g1r >(env, obj, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g2r(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1)
{
    TRACE("jint Java_myjapi_A_g2r(JNIEnv *, jclass, jobject, jbyte, jshort)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, ttrait_int16, &A::g2r >(env, obj, p0, p1);
}

JNIEXPORT jint JNICALL
Java_myjapi_A_g3r(JNIEnv * env, jclass cls, jobject obj, jbyte p0, jshort p1, jint p2)
{
    TRACE("jint Java_myjapi_A_g3r(JNIEnv *, jclass, jobject, jbyte, jshort, jint)");
    return gcall< ttrait_myjapi_A_target, ttrait_int32, ttrait_int8, ttrait_int16, ttrait_int32, &A::g3r >(env, obj, p0, p1, p2);
}

// ---------------------------------------------------------------------------
