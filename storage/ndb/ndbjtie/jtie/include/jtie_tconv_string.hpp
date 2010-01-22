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
 * jtie_tconv_string.hpp
 */

#ifndef jtie_tconv_string_hpp
#define jtie_tconv_string_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java String <-> const char * type conversions
// ---------------------------------------------------------------------------

// aliases for: <const-char>_<[const-]pointer>_<String>
typedef ttrait< jstring, const char * > ttrait_utf8cstring;
typedef ttrait< jstring, const char * const > ttrait_utf8cstring_c;

// ---------------------------------------------------------------------------
// Java String <-> char * type conversions
// ---------------------------------------------------------------------------

#if 0 // XXX implement and test mapping String <-> char * 
// aliases for: <char>_<[const-]pointer>_<String>
typedef ttrait< jstring, char * > ttrait_utf8string;
typedef ttrait< jstring, char * const > ttrait_utf8string_c;
#endif // XXX implement and test mapping String <-> char * 

// ---------------------------------------------------------------------------

#endif // jtie_tconv_string_hpp
