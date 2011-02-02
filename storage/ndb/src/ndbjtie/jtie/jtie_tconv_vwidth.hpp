/*
 Copyright 2010 Sun Microsystems, Inc.
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
 * jtie_tconv_vwidth.hpp
 */

#ifndef jtie_tconv_vwidth_hpp
#define jtie_tconv_vwidth_hpp

#include <jni.h>

#include "jtie_tconv.hpp"
#include "jtie_tconv_value.hpp"

// ---------------------------------------------------------------------------
// Java <-> C variable-width type mappings
// ---------------------------------------------------------------------------

// No definitions are given here for [[un]signed] char types; instead
// char is treated as an exact-width type of 8 bits.
#if 0 // use a static_assert() when made available in upcoming C++0x
#include <limits.h> // not using namespaces yet
static_assert(CHAR_BIT == 8);
#endif // use a static_assert() when made available in upcoming C++0x

/*
 * If defined, provides a default Java type mapping for the variable-width
 * C++ types: short, int, long, long long, long double.
 *
 * Examples (for the ILP32 data model):
 *   #define JTIE_JNI_SHORT_T jshort
 *   #define JTIE_JNI_INT_T jint
 *   #define JTIE_JNI_LONG_T jint
 *   #define JTIE_JNI_LONGLONG_T jlong
 *   #define JTIE_JNI_LONGDOUBLE_T jdouble
 */

#ifdef JTIE_JNI_SHORT_T
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_SHORT_T, signed short, short);
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_SHORT_T, unsigned short, ushort);
#endif
#ifdef JTIE_JNI_INT_T
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_INT_T, signed int, int);
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_INT_T, unsigned int, uint);
#endif
#ifdef JTIE_JNI_LONG_T
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_LONG_T, signed long, long);
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_LONG_T, unsigned long, ulong);
#endif
#ifdef JTIE_JNI_LONGLONG_T
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_LONGLONG_T, signed long long, longlong);
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_LONGLONG_T, unsigned long long, ulonglong);
#endif
#ifdef JTIE_JNI_LONGDOUBLE_T
JTIE_DEFINE_BASIC_TYPE_MAPPING(JTIE_JNI_LONGDOUBLE_T, long double, longdouble);
#endif

// ---------------------------------------------------------------------------

/*
// XXX variable-width type ByteBuffer conversions not supported yet
// - operational but no unit-tests yet

JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(signed short, short)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(unsigned short, ushort)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(signed int, int)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(unsigned int, uint)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(signed long, long)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(unsigned long, ulong)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(signed long long, longlong)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(unsigned long long, ulonglong)
JTIE_DEFINE_BYTEBUFFER_REF_TYPE_MAPPING(long double, longdouble)

JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(signed short, short)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(unsigned short, ushort)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(signed int, int)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(unsigned int, uint)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(signed long, long)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(unsigned long, ulong)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(signed long long, longlong)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(unsigned long long, ulonglong)
JTIE_DEFINE_BYTEBUFFER_PTR_TYPE_MAPPING(long double, longdouble)

JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(signed short, short)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(unsigned short, ushort)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(signed int, int)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(unsigned int, uint)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(signed long, long)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(unsigned long, ulong)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(signed long long, longlong)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(unsigned long long, ulonglong)
JTIE_DEFINE_BYTEBUFFER_PTR_LENGTH1_TYPE_MAPPING(long double, longdouble)
*/

// ---------------------------------------------------------------------------

/*
// XXX variable-width type array conversions not supported yet
// - default implementation would only support same-width types
// - no unit tests yet

#define JTIE_JNI_SHORT_ARRAY_T _jshortArray
#define JTIE_JNI_INT_ARRAY_T _jintArray
#define JTIE_JNI_LONG_ARRAY_T _jintArray
#define JTIE_JNI_LONGLONG_ARRAY_T _jlongArray
#define JTIE_JNI_LONGDOUBLE_ARRAY_T _jdoubleArray
*/

// ---------------------------------------------------------------------------

#endif // jtie_tconv_vwidth_hpp
