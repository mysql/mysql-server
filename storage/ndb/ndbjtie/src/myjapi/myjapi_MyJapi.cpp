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
 * myjapi_MyJapi.cpp
 */

#include <stdint.h>
#include <jni.h>
//#include <cstring>
#include "helpers.hpp"
#include "myapi.hpp"
#include "myjapi_MyJapi.h"
#include "jtie_ttrait.hpp"
#include "jtie_tconv.hpp"
#include "jtie_tconv_cvalue.hpp"
#include "jtie_tconv_cstring.hpp"
#include "jtie_tconv_carray.hpp"
#include "jtie_tconv_refbybb.hpp"
#include "jtie_tconv_refbyval.hpp"
#include "jtie_tconv_cstring.hpp"
#include "jtie_gcalls.hpp"

// ---------------------------------------------------------------------------
// generatable, application-dependent code: API JNI function stubs
// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f0(JNIEnv * env, jclass cls)
{
    TRACE("void Java_myjapi_MyJapi_f010(JNIEnv *, jclass)");
    gcall< f0 >(env);
}

// ---------------------------------------------------------------------------

JNIEXPORT jstring JNICALL Java_myjapi_MyJapi_s012(JNIEnv * env, jclass cls)
{
    TRACE("jstring Java_jtie_MyJAPI_s012(JNIEnv *, jclass)");
    return gcall< ttrait_cstring, s012 >(env);
}

JNIEXPORT void JNICALL Java_myjapi_MyJapi_s112(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("void Java_jtie_MyJAPI_s112(JNIEnv *, jclass, jstring)");
    gcall< ttrait_cstring, s112 >(env, p0);
}

// ---------------------------------------------------------------------------

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapi_f011(JNIEnv * env, jclass cls)
{
    TRACE("jboolean Java_myjapi_MyJapi_f011(JNIEnv *, jclass)");
    return gcall< ttrait_cbool, f011 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f012(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f012(JNIEnv *, jclass)");
    return gcall< ttrait_cchar, f012 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f013(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f013(JNIEnv *, jclass)");
    return gcall< ttrait_cint8, f013 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f014(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f014(JNIEnv *, jclass)");
    return gcall< ttrait_cuint8, f014 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f015(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f015(JNIEnv *, jclass)");
    return gcall< ttrait_cint16, f015 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f016(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f016(JNIEnv *, jclass)");
    return gcall< ttrait_cuint16, f016 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f017(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f017(JNIEnv *, jclass)");
    return gcall< ttrait_cint32, f017 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f018(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f018(JNIEnv *, jclass)");
    return gcall< ttrait_cuint32, f018 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f021(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f021(JNIEnv *, jclass)");
    return gcall< ttrait_cint64, f021 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f022(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f022(JNIEnv *, jclass)");
    return gcall< ttrait_cuint64, f022 >(env);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapi_f023(JNIEnv * env, jclass cls)
{
    TRACE("jfloat Java_myjapi_MyJapi_f023(JNIEnv *, jclass)");
    return gcall< ttrait_cfloat, f023 >(env);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapi_f024(JNIEnv * env, jclass cls)
{
    TRACE("jdouble Java_myjapi_MyJapi_f024(JNIEnv *, jclass)");
    return gcall< ttrait_cdouble, f024 >(env);
}

// ---------------------------------------------------------------------------

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapi_f031(JNIEnv * env, jclass cls)
{
    TRACE("jboolean Java_myjapi_MyJapi_f031(JNIEnv *, jclass)");
    return gcall< ttrait_bool, f031 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f032(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f032(JNIEnv *, jclass)");
    return gcall< ttrait_char, f032 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f033(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f033(JNIEnv *, jclass)");
    return gcall< ttrait_int8, f033 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f034(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f034(JNIEnv *, jclass)");
    return gcall< ttrait_uint8, f034 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f035(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f035(JNIEnv *, jclass)");
    return gcall< ttrait_int16, f035 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f036(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f036(JNIEnv *, jclass)");
    return gcall< ttrait_uint16, f036 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f037(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f037(JNIEnv *, jclass)");
    return gcall< ttrait_int32, f037 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f038(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f038(JNIEnv *, jclass)");
    return gcall< ttrait_uint32, f038 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f041(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f041(JNIEnv *, jclass)");
    return gcall< ttrait_int64, f041 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f042(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f042(JNIEnv *, jclass)");
    return gcall< ttrait_uint64, f042 >(env);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapi_f043(JNIEnv * env, jclass cls)
{
    TRACE("jfloat Java_myjapi_MyJapi_f043(JNIEnv *, jclass)");
    return gcall< ttrait_float, f043 >(env);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapi_f044(JNIEnv * env, jclass cls)
{
    TRACE("jdouble Java_myjapi_MyJapi_f044(JNIEnv *, jclass)");
    return gcall< ttrait_double, f044 >(env);
}

// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f111(JNIEnv * env, jclass cls, jboolean p0)
{
    TRACE("void Java_myjapi_MyJapi_f111(JNIEnv *, jclass, jboolean)");
    gcall< ttrait_cbool, f111 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f112(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f112(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_cchar, f112 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f113(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f113(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_cint8, f113 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f114(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f114(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_cuint8, f114 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f115(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f115(JNIEnv *, jclass, jshort)");
    gcall< ttrait_cint16, f115 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f116(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f116(JNIEnv *, jclass, jshort)");
    gcall< ttrait_cuint16, f116 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f117(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f117(JNIEnv *, jclass, jint)");
    gcall< ttrait_cint32, f117 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f118(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f118(JNIEnv *, jclass, jint)");
    gcall< ttrait_cuint32, f118 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f121(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f121(JNIEnv *, jclass, jlong)");
    gcall< ttrait_cint64, f121 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f122(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f122(JNIEnv *, jclass, jlong)");
    gcall< ttrait_cuint64, f122 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f123(JNIEnv * env, jclass cls, jfloat p0)
{
    TRACE("void Java_myjapi_MyJapi_f123(JNIEnv *, jclass, jfloat)");
    gcall< ttrait_cfloat, f123 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f124(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("void Java_myjapi_MyJapi_f124(JNIEnv *, jclass, jdouble)");
    gcall< ttrait_cdouble, f124 >(env, p0);
}

// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f131(JNIEnv * env, jclass cls, jboolean p0)
{
    TRACE("void Java_myjapi_MyJapi_f131(JNIEnv *, jclass, jboolean)");
    gcall< ttrait_bool, f131 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f132(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f132(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_char, f132 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f133(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f133(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_int8, f133 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f134(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f134(JNIEnv *, jclass, jbyte)");
    gcall< ttrait_uint8, f134 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f135(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f135(JNIEnv *, jclass, jshort)");
    gcall< ttrait_int16, f135 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f136(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f136(JNIEnv *, jclass, jshort)");
    gcall< ttrait_uint16, f136 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f137(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f137(JNIEnv *, jclass, jint)");
    gcall< ttrait_int32, f137 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f138(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f138(JNIEnv *, jclass, jint)");
    gcall< ttrait_uint32, f138 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f141(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f141(JNIEnv *, jclass, jlong)");
    gcall< ttrait_int64, f141 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f142(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f142(JNIEnv *, jclass, jlong)");
    gcall< ttrait_uint64, f142 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f143(JNIEnv * env, jclass cls, jfloat p0)
{
    TRACE("void Java_myjapi_MyJapi_f143(JNIEnv *, jclass, jfloat)");
    gcall< ttrait_float, f143 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f144(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("void Java_myjapi_MyJapi_f144(JNIEnv *, jclass, jdouble)");
    gcall< ttrait_double, f144 >(env, p0);
}

// ---------------------------------------------------------------------------

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f211bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f211bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const bool &, j_n_ByteBuffer >, f211 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f212bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f212bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const char &, j_n_ByteBuffer >, f212 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f213bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f213bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const int8_t &, j_n_ByteBuffer >, f213 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f214bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f214bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const uint8_t &, j_n_ByteBuffer >, f214 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f215bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f215bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const int16_t &, j_n_ByteBuffer >, f215 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f216bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f216bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const uint16_t &, j_n_ByteBuffer >, f216 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f217bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f217bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const int32_t &, j_n_ByteBuffer >, f217 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f218bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f218bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const uint32_t &, j_n_ByteBuffer >, f218 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f221bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f221bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const int64_t &, j_n_ByteBuffer >, f221 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f222bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f222bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const uint64_t &, j_n_ByteBuffer >, f222 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f223bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f223bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const float &, j_n_ByteBuffer >, f223 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f224bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f224bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, const double &, j_n_ByteBuffer >, f224 >(env);
}

// ---------------------------------------------------------------------------

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f231bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f231bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, bool &, j_n_ByteBuffer >, f231 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f232bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f232bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, char &, j_n_ByteBuffer >, f232 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f233bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f233bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, int8_t &, j_n_ByteBuffer >, f233 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f234bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f234bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, uint8_t &, j_n_ByteBuffer >, f234 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f235bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f235bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, int16_t &, j_n_ByteBuffer >, f235 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f236bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f236bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, uint16_t &, j_n_ByteBuffer >, f236 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f237bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f237bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, int32_t &, j_n_ByteBuffer >, f237 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f238bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f238bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, uint32_t &, j_n_ByteBuffer >, f238 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f241bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f241bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, int64_t &, j_n_ByteBuffer >, f241 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f242bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f242bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, uint64_t &, j_n_ByteBuffer >, f242 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f243bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f243bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, float &, j_n_ByteBuffer >, f243 >(env);
}

JNIEXPORT jobject JNICALL
Java_myjapi_MyJapi_f244bb(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_myjapi_MyJapi_f244bb(JNIEnv *, jclass)");
    return gcall< ttrait< jobject, double &, j_n_ByteBuffer >, f244 >(env);
}

// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f311bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f311bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const bool &, j_n_ByteBuffer >, f311 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f312bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f312bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const char &, j_n_ByteBuffer >, f312 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f313bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f313bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const int8_t &, j_n_ByteBuffer >, f313 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f314bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f314bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const uint8_t &, j_n_ByteBuffer >, f314 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f315bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f315bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const int16_t &, j_n_ByteBuffer >, f315 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f316bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f316bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const uint16_t &, j_n_ByteBuffer >, f316 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f317bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f317bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const int32_t &, j_n_ByteBuffer >, f317 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f318bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f318bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const uint32_t &, j_n_ByteBuffer >, f318 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f321bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f321bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const int64_t &, j_n_ByteBuffer >, f321 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f322bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f322bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const uint64_t &, j_n_ByteBuffer >, f322 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f323bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f323bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const float &, j_n_ByteBuffer >, f323 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f324bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f324bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, const double &, j_n_ByteBuffer >, f324 >(env, p0);
}

// ---------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f331bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f331bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, bool &, j_n_ByteBuffer >, f331 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f332bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f332bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, char &, j_n_ByteBuffer >, f332 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f333bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f333bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, int8_t &, j_n_ByteBuffer >, f333 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f334bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f334bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, uint8_t &, j_n_ByteBuffer >, f334 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f335bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f335bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, int16_t &, j_n_ByteBuffer >, f335 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f336bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f336bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, uint16_t &, j_n_ByteBuffer >, f336 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f337bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f337bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, int32_t &, j_n_ByteBuffer >, f337 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f338bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f338bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, uint32_t &, j_n_ByteBuffer >, f338 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f341bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f341bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, int64_t &, j_n_ByteBuffer >, f341 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f342bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f342bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, uint64_t &, j_n_ByteBuffer >, f342 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f343bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f343bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, float &, j_n_ByteBuffer >, f343 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f344bb(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_myjapi_MyJapi_f344bb(JNIEnv *, jclass, jobject)");
    gcall< ttrait< jobject, double &, j_n_ByteBuffer >, f344 >(env, p0);
}

// ---------------------------------------------------------------------------

/*
JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f217v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f217v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, const int32_t & >, f217 >(env);
}
*/

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapi_f211v(JNIEnv * env, jclass cls)
{
    TRACE("jboolean Java_myjapi_MyJapi_f211v(JNIEnv *, jclass)");
    return gcall< ttrait< jboolean, const bool & >, f211 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f212v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f212v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, const char & >, f212 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f213v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f213v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, const int8_t & >, f213 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f214v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f214v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, const uint8_t & >, f214 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f215v(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f215v(JNIEnv *, jclass)");
    return gcall< ttrait< jshort, const int16_t & >, f215 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f216v(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f216v(JNIEnv *, jclass)");
    return gcall< ttrait< jshort, const uint16_t & >, f216 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f217v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f217v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, const int32_t & >, f217 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f218v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f218v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, const uint32_t & >, f218 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f221v(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f221v(JNIEnv *, jclass)");
    return gcall< ttrait< jlong, const int64_t & >, f221 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f222v(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f222v(JNIEnv *, jclass)");
    return gcall< ttrait< jlong, const uint64_t & >, f222 >(env);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapi_f223v(JNIEnv * env, jclass cls)
{
    TRACE("jfloat Java_myjapi_MyJapi_f223v(JNIEnv *, jclass)");
    return gcall< ttrait< jfloat, const float & >, f223 >(env);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapi_f224v(JNIEnv * env, jclass cls)
{
    TRACE("jdouble Java_myjapi_MyJapi_f224v(JNIEnv *, jclass)");
    return gcall< ttrait< jdouble, const double & >, f224 >(env);
}

// ---------------------------------------------------------------------------

/*
JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f237v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f237v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, int32_t & >, f237 >(env);
}
*/

JNIEXPORT jboolean JNICALL
Java_myjapi_MyJapi_f231v(JNIEnv * env, jclass cls)
{
    TRACE("jboolean Java_myjapi_MyJapi_f231v(JNIEnv *, jclass)");
    return gcall< ttrait< jboolean, bool & >, f231 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f232v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f232v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, char & >, f232 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f233v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f233v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, int8_t & >, f233 >(env);
}

JNIEXPORT jbyte JNICALL
Java_myjapi_MyJapi_f234v(JNIEnv * env, jclass cls)
{
    TRACE("jbyte Java_myjapi_MyJapi_f234v(JNIEnv *, jclass)");
    return gcall< ttrait< jbyte, uint8_t & >, f234 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f235v(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f235v(JNIEnv *, jclass)");
    return gcall< ttrait< jshort, int16_t & >, f235 >(env);
}

JNIEXPORT jshort JNICALL
Java_myjapi_MyJapi_f236v(JNIEnv * env, jclass cls)
{
    TRACE("jshort Java_myjapi_MyJapi_f236v(JNIEnv *, jclass)");
    return gcall< ttrait< jshort, uint16_t & >, f236 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f237v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f237v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, int32_t & >, f237 >(env);
}

JNIEXPORT jint JNICALL
Java_myjapi_MyJapi_f238v(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_myjapi_MyJapi_f238v(JNIEnv *, jclass)");
    return gcall< ttrait< jint, uint32_t & >, f238 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f241v(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f241v(JNIEnv *, jclass)");
    return gcall< ttrait< jlong, int64_t & >, f241 >(env);
}

JNIEXPORT jlong JNICALL
Java_myjapi_MyJapi_f242v(JNIEnv * env, jclass cls)
{
    TRACE("jlong Java_myjapi_MyJapi_f242v(JNIEnv *, jclass)");
    return gcall< ttrait< jlong, uint64_t & >, f242 >(env);
}

JNIEXPORT jfloat JNICALL
Java_myjapi_MyJapi_f243v(JNIEnv * env, jclass cls)
{
    TRACE("jfloat Java_myjapi_MyJapi_f243v(JNIEnv *, jclass)");
    return gcall< ttrait< jfloat, float & >, f243 >(env);
}

JNIEXPORT jdouble JNICALL
Java_myjapi_MyJapi_f244v(JNIEnv * env, jclass cls)
{
    TRACE("jdouble Java_myjapi_MyJapi_f244v(JNIEnv *, jclass)");
    return gcall< ttrait< jdouble, double & >, f244 >(env);
}

// ---------------------------------------------------------------------------

/*
JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f317v(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f317v(JNIEnv *, jclass, jint)");
    gcall< ttrait< jint, const int32_t & >, f317 >(env, p0);
}
*/

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f311v(JNIEnv * env, jclass cls, jboolean p0)
{
    TRACE("void Java_myjapi_MyJapi_f311v(JNIEnv *, jclass, jboolean)");
    gcall< ttrait< jboolean, const bool & >, f311 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f312v(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f312v(JNIEnv *, jclass, jbyte)");
    gcall< ttrait< jbyte, const char & >, f312 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f313v(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f313v(JNIEnv *, jclass, jbyte)");
    gcall< ttrait< jbyte, const int8_t & >, f313 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f314v(JNIEnv * env, jclass cls, jbyte p0)
{
    TRACE("void Java_myjapi_MyJapi_f314v(JNIEnv *, jclass, jbyte)");
    gcall< ttrait< jbyte, const uint8_t & >, f314 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f315v(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f315v(JNIEnv *, jclass, jshort)");
    gcall< ttrait< jshort, const int16_t & >, f315 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f316v(JNIEnv * env, jclass cls, jshort p0)
{
    TRACE("void Java_myjapi_MyJapi_f316v(JNIEnv *, jclass, jshort)");
    gcall< ttrait< jshort, const uint16_t & >, f316 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f317v(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f317v(JNIEnv *, jclass, jint)");
    gcall< ttrait< jint, const int32_t & >, f317 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f318v(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("void Java_myjapi_MyJapi_f318v(JNIEnv *, jclass, jint)");
    gcall< ttrait< jint, const uint32_t & >, f318 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f321v(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f321v(JNIEnv *, jclass, jlong)");
    gcall< ttrait< jlong, const int64_t & >, f321 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f322v(JNIEnv * env, jclass cls, jlong p0)
{
    TRACE("void Java_myjapi_MyJapi_f322v(JNIEnv *, jclass, jlong)");
    gcall< ttrait< jlong, const uint64_t & >, f322 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f323v(JNIEnv * env, jclass cls, jfloat p0)
{
    TRACE("void Java_myjapi_MyJapi_f323v(JNIEnv *, jclass, jfloat)");
    gcall< ttrait< jfloat, const float & >, f323 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f324v(JNIEnv * env, jclass cls, jdouble p0)
{
    TRACE("void Java_myjapi_MyJapi_f324v(JNIEnv *, jclass, jdouble)");
    gcall< ttrait< jdouble, const double & >, f324 >(env, p0);
}

// ---------------------------------------------------------------------------

/*
JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f337v(JNIEnv * env, jclass cls, jintArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f337v(JNIEnv *, jclass, jintArray)");
    gcall< ttrait< jintArray, int32_t & >, f337 >(env, p0);
}
*/

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f331v(JNIEnv * env, jclass cls, jbooleanArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f331v(JNIEnv *, jclass, jbooleanArray)");
    gcall< ttrait< jbooleanArray, bool & >, f331 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f332v(JNIEnv * env, jclass cls, jbyteArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f332v(JNIEnv *, jclass, jbyteArray)");
    gcall< ttrait< jbyteArray, char & >, f332 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f333v(JNIEnv * env, jclass cls, jbyteArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f333v(JNIEnv *, jclass, jbyteArray)");
    gcall< ttrait< jbyteArray, int8_t & >, f333 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f334v(JNIEnv * env, jclass cls, jbyteArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f334v(JNIEnv *, jclass, jbyteArray)");
    gcall< ttrait< jbyteArray, uint8_t & >, f334 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f335v(JNIEnv * env, jclass cls, jshortArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f335v(JNIEnv *, jclass, jshortArray)");
    gcall< ttrait< jshortArray, int16_t & >, f335 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f336v(JNIEnv * env, jclass cls, jshortArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f336v(JNIEnv *, jclass, jshortArray)");
    gcall< ttrait< jshortArray, uint16_t & >, f336 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f337v(JNIEnv * env, jclass cls, jintArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f337v(JNIEnv *, jclass, jintArray)");
    gcall< ttrait< jintArray, int32_t & >, f337 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f338v(JNIEnv * env, jclass cls, jintArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f338v(JNIEnv *, jclass, jintArray)");
    gcall< ttrait< jintArray, uint32_t & >, f338 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f341v(JNIEnv * env, jclass cls, jlongArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f341v(JNIEnv *, jclass, jlongArray)");
    gcall< ttrait< jlongArray, int64_t & >, f341 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f342v(JNIEnv * env, jclass cls, jlongArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f342v(JNIEnv *, jclass, jlongArray)");
    gcall< ttrait< jlongArray, uint64_t & >, f342 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f343v(JNIEnv * env, jclass cls, jfloatArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f343v(JNIEnv *, jclass, jfloatArray)");
    gcall< ttrait< jfloatArray, float & >, f343 >(env, p0);
}

JNIEXPORT void JNICALL
Java_myjapi_MyJapi_f344v(JNIEnv * env, jclass cls, jdoubleArray p0)
{
    TRACE("void Java_myjapi_MyJapi_f344v(JNIEnv *, jclass, jdoubleArray)");
    gcall< ttrait< jdoubleArray, double & >, f344 >(env, p0);
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

