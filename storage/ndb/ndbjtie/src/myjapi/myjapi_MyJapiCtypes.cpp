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
 * myjapi_MyJapiCtypes.cpp
 */

#include <stdint.h>
#include <jni.h>
#include "helpers.hpp"
#include "myapi.hpp"
#include "myjapi_MyJapiCtypes.h"
#include "jtie_ttrait.hpp"
#include "jtie_tconv.hpp"
#include "jtie_tconv_cvalue.hpp"
#include "jtie_tconv_cvalue_ext.hpp"
#include "jtie_tconv_carray.hpp"
#include "jtie_tconv_carray_ext.hpp"
#include "jtie_tconv_refbybb.hpp"
#include "jtie_tconv_refbyval.hpp"
#include "jtie_tconv_refbyval_ext.hpp"
#include "jtie_gcalls.hpp"


// ---------------------------------------------------------------------------
// generatable, application & platform-dependent code: API JNI function stubs
// ---------------------------------------------------------------------------

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapiCtypes_f11(JNIEnv * env, jclass cls, jboolean p0)
{
    TRACE("jboolean Java_myjapi_MyJapiCtypes_f11(JNIEnv *, jclass, jboolean)");
    return gcall< ttrait_cbool, ttrait_cbool, f11 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f12(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f12(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_cchar, ttrait_cchar, f12 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f13(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f13(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_cint8, ttrait_cint8, f13 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f14(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f14(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_cuint8, ttrait_cuint8, f14 >(env, p0);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapiCtypes_f15(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("jshort Java_myjapi_MyJapiCtypes_f15(JNIEnv *, jclass, jshort)");
    return gcall< ttrait_cint16, ttrait_cint16, f15 >(env, p0);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapiCtypes_f16(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("jshort Java_myjapi_MyJapiCtypes_f16(JNIEnv *, jclass, jshort)");
    return gcall< ttrait_cuint16, ttrait_cuint16, f16 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f17(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f17(JNIEnv *, jclass, jint)");
    return gcall< ttrait_cint32, ttrait_cint32, f17 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f18(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f18(JNIEnv *, jclass, jint)");
    return gcall< ttrait_cuint32, ttrait_cuint32, f18 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f19(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f19(JNIEnv *, jclass, jint)");
    return gcall< ttrait_clong, ttrait_clong, f19 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f20(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f20(JNIEnv *, jclass, jint)");
    return gcall< ttrait_culong, ttrait_culong, f20 >(env, p0);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapiCtypes_f21(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("jlong Java_myjapi_MyJapiCtypes_f21(JNIEnv *, jclass, jlong)");
    return gcall< ttrait_cint64, ttrait_cint64, f21 >(env, p0);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapiCtypes_f22(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("jlong Java_myjapi_MyJapiCtypes_f22(JNIEnv *, jclass, jlong)");
    return gcall< ttrait_cuint64, ttrait_cuint64, f22 >(env, p0);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapiCtypes_f23(JNIEnv * env, jclass cls, jfloat p0)
{
    TRACE("jfloat Java_myjapi_MyJapiCtypes_f23(JNIEnv *, jclass, jfloat)");
    return gcall< ttrait_cfloat, ttrait_cfloat, f23 >(env, p0);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapiCtypes_f24(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("jdouble Java_myjapi_MyJapiCtypes_f24(JNIEnv *, jclass, jdouble)");
    return gcall< ttrait_cdouble, ttrait_cdouble, f24 >(env, p0);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapiCtypes_f25(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("jdouble Java_myjapi_MyJapiCtypes_f25(JNIEnv *, jclass, jdouble)");
    return gcall< ttrait_cldouble, ttrait_cldouble, f25 >(env, p0);
}

// ---------------------------------------------------------------------------

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapiCtypes_f31(JNIEnv * env, jclass cls, jboolean p0)
{
    TRACE("jboolean Java_myjapi_MyJapiCtypes_f31(JNIEnv *, jclass, jboolean)");
    return gcall< ttrait_bool, ttrait_bool, f31 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f32(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f32(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_char, ttrait_char, f32 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f33(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f33(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_int8, ttrait_int8, f33 >(env, p0);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapiCtypes_f34(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("jbyte Java_myjapi_MyJapiCtypes_f34(JNIEnv *, jclass, jbyte)");
    return gcall< ttrait_uint8, ttrait_uint8, f34 >(env, p0);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapiCtypes_f35(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("jshort Java_myjapi_MyJapiCtypes_f35(JNIEnv *, jclass, jshort)");
    return gcall< ttrait_int16, ttrait_int16, f35 >(env, p0);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapiCtypes_f36(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("jshort Java_myjapi_MyJapiCtypes_f36(JNIEnv *, jclass, jshort)");
    return gcall< ttrait_uint16, ttrait_uint16, f36 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f37(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f37(JNIEnv *, jclass, jint)");
    return gcall< ttrait_int32, ttrait_int32, f37 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f38(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f38(JNIEnv *, jclass, jint)");
    return gcall< ttrait_uint32, ttrait_uint32, f38 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f39(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f39(JNIEnv *, jclass, jint)");
    return gcall< ttrait_long, ttrait_long, f39 >(env, p0);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapiCtypes_f40(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("jint Java_myjapi_MyJapiCtypes_f40(JNIEnv *, jclass, jint)");
    return gcall< ttrait_ulong, ttrait_ulong, f40 >(env, p0);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapiCtypes_f41(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("jlong Java_myjapi_MyJapiCtypes_f41(JNIEnv *, jclass, jlong)");
    return gcall< ttrait_int64, ttrait_int64, f41 >(env, p0);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapiCtypes_f42(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("jlong Java_myjapi_MyJapiCtypes_f42(JNIEnv *, jclass, jlong)");
    return gcall< ttrait_uint64, ttrait_uint64, f42 >(env, p0);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapiCtypes_f43(JNIEnv * env, jclass cls, jfloat p0)
{
    TRACE("jfloat Java_myjapi_MyJapiCtypes_f43(JNIEnv *, jclass, jfloat)");
    return gcall< ttrait_float, ttrait_float, f43 >(env, p0);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapiCtypes_f44(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("jdouble Java_myjapi_MyJapiCtypes_f44(JNIEnv *, jclass, jdouble)");
    return gcall< ttrait_double, ttrait_double, f44 >(env, p0);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapiCtypes_f45(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("jdouble Java_myjapi_MyJapiCtypes_f45(JNIEnv * env, jclass cls, jdouble)");
    return gcall< ttrait_ldouble, ttrait_ldouble, f45 >(env, p0);
}

// ---------------------------------------------------------------------------
