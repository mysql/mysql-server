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
 * jtie_tconv_cvalue_ext.hpp
 */

#ifndef jtie_tconv_cvalue_ext_hpp
#define jtie_tconv_cvalue_ext_hpp

//#include <stdint.h>
#include <jni.h>
//#include "helpers.hpp"
//#include "jtie_tconv_def.hpp"
#include "jtie_tconv_cvalue.hpp"
#include "jtie_ttrait.hpp"

// ---------------------------------------------------------------------------
// platform-dependent number type mappings
// ---------------------------------------------------------------------------

// convenience type aliases for basic number type mappings

typedef ttrait< jint, signed int > ttrait_int;
typedef ttrait< jint, unsigned int > ttrait_uint;
typedef ttrait< jint, signed long > ttrait_long;
typedef ttrait< jint, unsigned long > ttrait_ulong;
typedef ttrait< jdouble, long double > ttrait_ldouble;

typedef ttrait< jint, const signed int > ttrait_cint;
typedef ttrait< jint, const unsigned int > ttrait_cuint;
typedef ttrait< jint, const signed long > ttrait_clong;
typedef ttrait< jint, const unsigned long > ttrait_culong;
typedef ttrait< jdouble, const long double > ttrait_cldouble;

// ---------------------------------------------------------------------------
// platform-dependent Java value <-> C value conversions
// ---------------------------------------------------------------------------

template<> struct Param< jint, signed long > : ParamBasicT< jint, signed long > {};
template<> struct Param< jint, unsigned long > : ParamBasicT< jint, unsigned long > {};
template<> struct Param< jdouble, long double > : ParamBasicT< jdouble, long double > {};

template<> struct Result< jint, signed long > : ResultBasicT< jint, signed long > {};
template<> struct Result< jint, unsigned long > : ResultBasicT< jint, unsigned long > {};
template<> struct Result< jdouble, long double > : ResultBasicT< jdouble, long double > {};

template<> struct Param< jint, const signed long > : ParamBasicT< jint, const signed long > {};
template<> struct Param< jint, const unsigned long > : ParamBasicT< jint, const unsigned long > {};
template<> struct Param< jdouble, const long double > : ParamBasicT< jdouble, const long double > {};

template<> struct Result< jint, const signed long > : ResultBasicT< jint, const signed long > {};
template<> struct Result< jint, const unsigned long > : ResultBasicT< jint, const unsigned long > {};
template<> struct Result< jdouble, const long double > : ResultBasicT< jdouble, const long double > {};

#endif // jtie_tconv_cvalue_ext_hpp
