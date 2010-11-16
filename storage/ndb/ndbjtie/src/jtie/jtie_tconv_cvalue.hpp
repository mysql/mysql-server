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
 * jtie_tconv_cvalue.hpp
 */

#ifndef jtie_tconv_cvalue_hpp
#define jtie_tconv_cvalue_hpp

#include <stdint.h>
#include <jni.h>
//#include "helpers.hpp"
#include "jtie_tconv_def.hpp"
#include "jtie_ttrait.hpp"

// ---------------------------------------------------------------------------
// fixed-size number type mappings
// ---------------------------------------------------------------------------

// convenience type aliases for basic number type mappings

typedef ttrait< jbyte, char > ttrait_char;
typedef ttrait< jbyte, int8_t > ttrait_int8;
typedef ttrait< jbyte, uint8_t > ttrait_uint8;
typedef ttrait< jshort, int16_t > ttrait_int16;
typedef ttrait< jshort, uint16_t > ttrait_uint16;
typedef ttrait< jint, int32_t > ttrait_int32;
typedef ttrait< jint, uint32_t > ttrait_uint32;
typedef ttrait< jlong, int64_t > ttrait_int64;
typedef ttrait< jlong, uint64_t > ttrait_uint64;
typedef ttrait< jfloat, float > ttrait_float;
typedef ttrait< jdouble, double > ttrait_double;

typedef ttrait< jbyte, const char > ttrait_cchar;
typedef ttrait< jbyte, const int8_t > ttrait_cint8;
typedef ttrait< jbyte, const uint8_t > ttrait_cuint8;
typedef ttrait< jshort, const int16_t > ttrait_cint16;
typedef ttrait< jshort, const uint16_t > ttrait_cuint16;
typedef ttrait< jint, const int32_t > ttrait_cint32;
typedef ttrait< jint, const uint32_t > ttrait_cuint32;
typedef ttrait< jlong, const int64_t > ttrait_cint64;
typedef ttrait< jlong, const uint64_t > ttrait_cuint64;
typedef ttrait< jfloat, const float > ttrait_cfloat;
typedef ttrait< jdouble, const double > ttrait_cdouble;

// convenience type aliases for boolean number type mappings
typedef ttrait< jboolean, bool > ttrait_bool;
typedef ttrait< jboolean, const bool > ttrait_cbool;

// ---------------------------------------------------------------------------
// Java value <-> C value conversions
// ---------------------------------------------------------------------------

template< typename J, typename C >
struct ParamBasicT {
    static C
    convert(cstatus & s, J j, JNIEnv * env) {
        TRACE("C ParamBasicT.convert(cstatus &, J, JNIEnv *)");
        s = 0;
        return j;
    }

    static void
    release(C c, J j, JNIEnv * env) {
        TRACE("void ParamBasicT.release(C, J, JNIEnv *)");
    }
};

template< typename J, typename C >
struct ResultBasicT {
    static J
    convert(C c, JNIEnv * env) {
        TRACE("J ResultBasicT.convert(C, JNIEnv *)");
        return c;
    }
};

// ---------------------------------------------------------------------------
// fixed-size number type conversions
// ---------------------------------------------------------------------------

template<> struct Param< jbyte, char > : ParamBasicT< jbyte, char > {};
template<> struct Param< jbyte, int8_t > : ParamBasicT< jbyte, int8_t > {};
template<> struct Param< jbyte, uint8_t > : ParamBasicT< jbyte, uint8_t > {};
template<> struct Param< jshort, int16_t > : ParamBasicT< jshort, int16_t > {};
template<> struct Param< jshort, uint16_t > : ParamBasicT< jshort, uint16_t > {};
template<> struct Param< jint, int32_t > : ParamBasicT< jint, int32_t > {};
template<> struct Param< jint, uint32_t > : ParamBasicT< jint, uint32_t > {};
template<> struct Param< jlong, int64_t > : ParamBasicT< jlong, int64_t > {};
template<> struct Param< jlong, uint64_t > : ParamBasicT< jlong, uint64_t > {};
template<> struct Param< jfloat, float > : ParamBasicT< jfloat, float > {};
template<> struct Param< jdouble, double > : ParamBasicT< jdouble, double > {};

template<> struct Result< jbyte, char > : ResultBasicT< jbyte, char > {};
template<> struct Result< jbyte, int8_t > : ResultBasicT< jbyte, int8_t > {};
template<> struct Result< jbyte, uint8_t > : ResultBasicT< jbyte, uint8_t > {};
template<> struct Result< jshort, int16_t > : ResultBasicT< jshort, int16_t > {};
template<> struct Result< jshort, uint16_t > : ResultBasicT< jshort, uint16_t > {};
template<> struct Result< jint, int32_t > : ResultBasicT< jint, int32_t > {};
template<> struct Result< jint, uint32_t > : ResultBasicT< jint, uint32_t > {};
template<> struct Result< jlong, int64_t > : ResultBasicT< jlong, int64_t > {};
template<> struct Result< jlong, uint64_t > : ResultBasicT< jlong, uint64_t > {};
template<> struct Result< jfloat, float > : ResultBasicT< jfloat, float > {};
template<> struct Result< jdouble, double > : ResultBasicT< jdouble, double > {};

template<> struct Param< jbyte, const char > : ParamBasicT< jbyte, const char > {};
template<> struct Param< jbyte, const int8_t > : ParamBasicT< jbyte, const int8_t > {};
template<> struct Param< jbyte, const uint8_t > : ParamBasicT< jbyte, const uint8_t > {};
template<> struct Param< jshort, const int16_t > : ParamBasicT< jshort, const int16_t > {};
template<> struct Param< jshort, const uint16_t > : ParamBasicT< jshort, const uint16_t > {};
template<> struct Param< jint, const int32_t > : ParamBasicT< jint, const int32_t > {};
template<> struct Param< jint, const uint32_t > : ParamBasicT< jint, const uint32_t > {};
template<> struct Param< jlong, const int64_t > : ParamBasicT< jlong, const int64_t > {};
template<> struct Param< jlong, const uint64_t > : ParamBasicT< jlong, const uint64_t > {};
template<> struct Param< jfloat, const float > : ParamBasicT< jfloat, const float > {};
template<> struct Param< jdouble, const double > : ParamBasicT< jdouble, const double > {};

template<> struct Result< jbyte, const char > : ResultBasicT< jbyte, const char > {};
template<> struct Result< jbyte, const int8_t > : ResultBasicT< jbyte, const int8_t > {};
template<> struct Result< jbyte, const uint8_t > : ResultBasicT< jbyte, const uint8_t > {};
template<> struct Result< jshort, const int16_t > : ResultBasicT< jshort, const int16_t > {};
template<> struct Result< jshort, const uint16_t > : ResultBasicT< jshort, const uint16_t > {};
template<> struct Result< jint, const int32_t > : ResultBasicT< jint, const int32_t > {};
template<> struct Result< jint, const uint32_t > : ResultBasicT< jint, const uint32_t > {};
template<> struct Result< jlong, const int64_t > : ResultBasicT< jlong, const int64_t > {};
template<> struct Result< jlong, const uint64_t > : ResultBasicT< jlong, const uint64_t > {};
template<> struct Result< jfloat, const float > : ResultBasicT< jfloat, const float > {};
template<> struct Result< jdouble, const double > : ResultBasicT< jdouble, const double > {};

// ---------------------------------------------------------------------------
// jboolean
// ---------------------------------------------------------------------------

template<>
struct Param< jboolean, bool > {
    static bool
    convert(cstatus & s, jboolean j, JNIEnv * env) {
        TRACE("bool Param.convert(cstatus &, jboolean, JNIEnv *)");
        s = 0;
        return (j == JNI_TRUE); // J/C++ may have differ
    }

    static void
    release(bool c, jboolean j, JNIEnv * env) {
        TRACE("void Param.release(bool, jboolean, JNIEnv *)");
    }
};

template<>
struct Result< jboolean, bool > {
    static jboolean
    convert(bool c, JNIEnv * env) {
        TRACE("jboolean Result.convert(bool, JNIEnv *)");
        return (c ? JNI_TRUE : JNI_FALSE); // J/C++ may have differ
    }
};

template<>
struct Param< jboolean, const bool > {
    static const bool
    convert(cstatus & s, jboolean j, JNIEnv * env) {
        TRACE("const bool Param.convert(cstatus &, jboolean, JNIEnv *)");
        s = 0;
        return (j == JNI_TRUE); // J/C++ may have differ
    }

    static void
    release(const bool c, jboolean j, JNIEnv * env) {
        TRACE("void Param.release(const bool, jboolean, JNIEnv *)");
    }
};

template<>
struct Result< jboolean, const bool > {
    static jboolean
    convert(const bool c, JNIEnv * env) {
        TRACE("jboolean Result.convert(const bool, JNIEnv *)");
        return (c ? JNI_TRUE : JNI_FALSE); // J/C++ may have differ
    }
};

#endif // jtie_tconv_cvalue_hpp
