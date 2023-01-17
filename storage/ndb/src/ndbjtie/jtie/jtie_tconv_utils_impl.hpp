/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_tconv_utils_impl.hpp
 */

#ifndef jtie_tconv_utils_impl_hpp
#define jtie_tconv_utils_impl_hpp

#include <jni.h>

//#include "helpers.hpp"

// ---------------------------------------------------------------------------
// Java <-> C type conversion utilities
// ---------------------------------------------------------------------------

// provides some (meta) predicates on types
template< typename C >
struct TypeInfo {
    // indicates whether a type is const
    static bool isMutable() {
        return true;
    }
};

// partial specialization for const types
template< typename C >
struct TypeInfo< const C > {
    static bool isMutable() {
        return false;
    }
};

// registers an exception with JNI for this thread
inline void
registerException(JNIEnv * env, const char * jvmClassName, const char * msg) 
{
    jclass ec = env->FindClass(jvmClassName);
    if (ec == NULL) {
        // exception pending
    } else {
        env->ThrowNew(ec, msg);
        env->DeleteLocalRef(ec);
        // exception pending
    }
}

// returns a value in big endian format
template<typename C>
C
big_endian(C c)
{
    // test if big or little endian
    C r = 1;
    if(*(char *)&r == 0) {
        // big endian, ok
        return c;
    }

    // little-endian, reverse byte order (better: use optimized swap macros)
    const size_t n = sizeof(C);
    char * s = (char *)&c;
    char * t = (char *)&r;
    for (int i = n-1, j = 0; i >= 0; i--, j++)
        t[j] = s[i];
    return r;
}

// ---------------------------------------------------------------------------

#endif // jtie_tconv_utils_impl_hpp
