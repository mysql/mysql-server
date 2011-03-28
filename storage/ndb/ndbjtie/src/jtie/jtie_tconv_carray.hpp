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
 * jtie_tconv_carray.hpp
 */

#ifndef jtie_tconv_carray_hpp
#define jtie_tconv_carray_hpp

#include <stdint.h>
#include <jni.h>
//#include "helpers.hpp"
//#include "jtie_tconv_def.hpp"

// ---------------------------------------------------------------------------
// Java array <-> C array fixed-size type conversions
// ---------------------------------------------------------------------------

template< typename C, typename J >
C *
GetArrayElements(JNIEnv * env, J j, jboolean * isCopy);

template< typename C, typename J >
void
ReleaseArrayElements(JNIEnv * env, J j, C * c, jint mode);

// ---------------------------------------------------------------------------

template<>
inline bool *
GetArrayElements(JNIEnv * env, jbooleanArray j, jboolean * isCopy) {
    return reinterpret_cast< bool * >(env->GetBooleanArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jbooleanArray j, bool * c, jint mode) {
    env->ReleaseBooleanArrayElements(j, reinterpret_cast< jboolean * >(c), mode);
}

template<>
inline char *
GetArrayElements(JNIEnv * env, jbyteArray j, jboolean * isCopy) {
    return reinterpret_cast< char * >(env->GetByteArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jbyteArray j, char * c, jint mode) {
    env->ReleaseByteArrayElements(j, reinterpret_cast< jbyte * >(c), mode);
}

template<>
inline int8_t *
GetArrayElements(JNIEnv * env, jbyteArray j, jboolean * isCopy) {
    return reinterpret_cast< int8_t * >(env->GetByteArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jbyteArray j, int8_t * c, jint mode) {
    env->ReleaseByteArrayElements(j, reinterpret_cast< jbyte * >(c), mode);
}

template<>
inline uint8_t *
GetArrayElements(JNIEnv * env, jbyteArray j, jboolean * isCopy) {
    return reinterpret_cast< uint8_t * >(env->GetByteArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jbyteArray j, uint8_t * c, jint mode) {
    env->ReleaseByteArrayElements(j, reinterpret_cast< jbyte * >(c), mode);
}

template<>
inline int16_t *
GetArrayElements(JNIEnv * env, jshortArray j, jboolean * isCopy) {
    return reinterpret_cast< int16_t * >(env->GetShortArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jshortArray j, int16_t * c, jint mode) {
    env->ReleaseShortArrayElements(j, reinterpret_cast< jshort * >(c), mode);
}

template<>
inline uint16_t *
GetArrayElements(JNIEnv * env, jshortArray j, jboolean * isCopy) {
    return reinterpret_cast< uint16_t * >(env->GetShortArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jshortArray j, uint16_t * c, jint mode) {
    env->ReleaseShortArrayElements(j, reinterpret_cast< jshort * >(c), mode);
}

template<>
inline int32_t *
GetArrayElements(JNIEnv * env, jintArray j, jboolean * isCopy) {
    return reinterpret_cast< int32_t * >(env->GetIntArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jintArray j, int32_t * c, jint mode) {
    env->ReleaseIntArrayElements(j, reinterpret_cast< jint * >(c), mode);
}

template<>
inline uint32_t *
GetArrayElements(JNIEnv * env, jintArray j, jboolean * isCopy) {
    return reinterpret_cast< uint32_t * >(env->GetIntArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jintArray j, uint32_t * c, jint mode) {
    env->ReleaseIntArrayElements(j, reinterpret_cast< jint * >(c), mode);
}

template<>
inline int64_t *
GetArrayElements(JNIEnv * env, jlongArray j, jboolean * isCopy) {
    return reinterpret_cast< int64_t * >(env->GetLongArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jlongArray j, int64_t * c, jint mode) {
    env->ReleaseLongArrayElements(j, reinterpret_cast< jlong * >(c), mode);
}

template<>
inline uint64_t *
GetArrayElements(JNIEnv * env, jlongArray j, jboolean * isCopy) {
    return reinterpret_cast< uint64_t * >(env->GetLongArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jlongArray j, uint64_t * c, jint mode) {
    env->ReleaseLongArrayElements(j, reinterpret_cast< jlong * >(c), mode);
}

template<>
inline float *
GetArrayElements(JNIEnv * env, jfloatArray j, jboolean * isCopy) {
    return reinterpret_cast< float * >(env->GetFloatArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jfloatArray j, float * c, jint mode) {
    env->ReleaseFloatArrayElements(j, reinterpret_cast< jfloat * >(c), mode);
}

template<>
inline double *
GetArrayElements(JNIEnv * env, jdoubleArray j, jboolean * isCopy) {
    return reinterpret_cast< double * >(env->GetDoubleArrayElements(j, isCopy));
}

template<>
inline void
ReleaseArrayElements(JNIEnv * env, jdoubleArray j, double * c, jint mode) {
    env->ReleaseDoubleArrayElements(j, reinterpret_cast< jdouble * >(c), mode);
}

#endif // jtie_tconv_carray_hpp
