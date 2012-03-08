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
 * jtie_tconv_string.hpp
 */

#ifndef jtie_tconv_string_hpp
#define jtie_tconv_string_hpp

#include <jni.h>

#include "jtie_tconv.hpp"

// ---------------------------------------------------------------------------
// Java String <-> [const] char * type conversions
// ---------------------------------------------------------------------------

// comments:
// - The offered mappings of '[const] char *' to Java Strings
//   - require a) null-terminated and b) (modified) UTF8-encoded strings
//     (as descibed by the JNI specification);
//   - always copy content (by the JNI API as well as encoding conversion);
//     for pass-by-reference semantics, a ByteBuffer mapping is the only way.
// - For 'char *' mappings:
//   - only result and no parameter mappings are supported for Java Strings,
//     since a) Strings are immutable and b) no implicit conversions from 
//     'const char*' to 'char *' should be allowed through Java;
//   - there's no point in offering a 'Java StringBuilder' mapping, for it's 
//     complex to implement and as inefficient as when done from Java.

// aliases for: <char>_<[const]pointer[const]>_<encoding>_<String>
typedef ttrait< jstring, char * > ttrait_char_p_jutf8null;
typedef ttrait< jstring, char * const > ttrait_char_pc_jutf8null;
typedef ttrait< jstring, char const * > ttrait_char_cp_jutf8null;
typedef ttrait< jstring, char const * const > ttrait_char_cpc_jutf8null;

// ---------------------------------------------------------------------------
// Java String[] <-> const char * * type conversions
// ---------------------------------------------------------------------------

#if 0 // XXX implement and test mapping String[] <-> char * *
// XXX aliases for: <const-char>_<[const-]pointer>_<[const-]pointer>_<String[]>
typedef ttrait< jobjectArray, const char * * > ttrait_utf8cstring_a;
typedef ttrait< jobjectArray, const char * const * > ttrait_utf8cstring_ca;
typedef ttrait< jobjectArray, const char * const * const > ttrait_utf8cstring_cac;
#endif // XXX implement and test mapping String[] <-> char * *

// ---------------------------------------------------------------------------

#endif // jtie_tconv_string_hpp
