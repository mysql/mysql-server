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
 * jtie_tconv_carray_ext.hpp
 */

#ifndef jtie_tconv_carray_ext_hpp
#define jtie_tconv_carray_ext_hpp

#include <stdint.h>
#include <jni.h>
//#include "helpers.hpp"
//#include "jtie_tconv_def.hpp"
#include "jtie_tconv_carray.hpp"

// ---------------------------------------------------------------------------
// platform-dependent Java array <-> C array conversions
// ---------------------------------------------------------------------------

template<>
inline signed long *
GetArrayElements(JNIEnv * env, jintArray j, jboolean * isCopy) {
    return reinterpret_cast< signed long * >(env->GetIntArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jintArray j, signed long * c, jint mode) {
    env->ReleaseIntArrayElements(j, reinterpret_cast< jint * >(c), mode);
}

template<>
inline unsigned long *
GetArrayElements(JNIEnv * env, jintArray j, jboolean * isCopy) {
    return reinterpret_cast< unsigned long * >(env->GetIntArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jintArray j, unsigned long * c, jint mode) {
    env->ReleaseIntArrayElements(j, reinterpret_cast< jint * >(c), mode);
}

template<>
inline long double *
GetArrayElements(JNIEnv * env, jintArray j, jboolean * isCopy) {
    return reinterpret_cast< long double * >(env->GetIntArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jintArray j, long double * c, jint mode) {
    env->ReleaseIntArrayElements(j, reinterpret_cast< jint * >(c), mode);
}

#endif // jtie_tconv_carray_ext_hpp
