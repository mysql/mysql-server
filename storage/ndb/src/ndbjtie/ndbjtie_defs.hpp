/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * ndbjtie_defs.hpp
 */

#ifndef ndbjtie_defs_hpp
#define ndbjtie_defs_hpp

// API to implement against
#include "ndb_types.h"

// libraries
#include "helpers.hpp"
#include "jtie.hpp"

// the applied Java mapping of basic variable-width C++ types
#define JTIE_JNI_SHORT_T jshort
#define JTIE_JNI_INT_T jint
#define JTIE_JNI_LONG_T jint
#define JTIE_JNI_LONGLONG_T jlong
#define JTIE_JNI_LONGDOUBLE_T jdouble
#include "jtie_tconv_vwidth.hpp"

// ---------------------------------------------------------------------------
// NDB JTie Type Definitions
// ---------------------------------------------------------------------------

// compatibility layer typedefs: mapping trait type aliases for NDB types
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jbyte, Int8, Int8 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jbyte, Uint8, Uint8 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jshort, Int16, Int16 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jshort, Uint16, Uint16 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jint, Int32, Int32 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jint, Uint32, Uint32 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jlong, Int64, Int64 )
JTIE_DEFINE_BASIC_TYPE_MAPPING_SET( jlong, Uint64, Uint64 )

// XXXXX temporary, for testing
#if 1
#  define NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION 1
#  define NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION 1
#  define NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION 1
#else

// workaround for Sun Studio compilers (disambiguation of overloads)
// (Studio 12.1 = 5.10):
#if defined(__SUNPRO_CC)
//#  if (__SUNPRO_CC == 0x510)
#    define NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION 1
//#  endif
#endif
// workaround for MS Visual Studio compilers (disambiguation of overloads)
// (VC7 = VS2003 = 1310, VC8 = VS2005 = 1400, VC9 = VS2008 = 1500, ...?):
#if defined(_MSC_VER)
//#  if (1300 <= _MSC_VER) && (_MSC_VER <= 1600)
#    define NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION 1
#    define NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION 1
//#  endif
#endif

#endif

// XXX: document why clearing cdelegate in wrapper object upon delete
#define JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE 1

// ---------------------------------------------------------------------------

#endif // ndbjtie_defs_hpp
