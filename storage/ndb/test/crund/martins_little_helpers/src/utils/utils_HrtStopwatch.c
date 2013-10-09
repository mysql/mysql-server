/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include <jni.h>

#include "utils_HrtStopwatch.h"
#include "hrt_gstopwatch.h"

JNIEXPORT void JNICALL
Java_utils_HrtStopwatch_init(JNIEnv *env, jclass cls,
                                        jint cap)
{
    hrt_gsw_init(cap);
}

JNIEXPORT void JNICALL
Java_utils_HrtStopwatch_close(JNIEnv *env, jclass cls)
{
    hrt_gsw_close();
}

JNIEXPORT jint JNICALL
Java_utils_HrtStopwatch_top(JNIEnv *env, jclass cls)
{
    return hrt_gsw_top();
}

JNIEXPORT jint JNICALL
Java_utils_HrtStopwatch_capacity(JNIEnv *env, jclass cls)
{
    return hrt_gsw_capacity();
}

JNIEXPORT jint JNICALL
Java_utils_HrtStopwatch_pushmark(JNIEnv *env, jclass cls)
{
    return hrt_gsw_pushmark();
}

JNIEXPORT void JNICALL
Java_utils_HrtStopwatch_popmark(JNIEnv * env, jclass cls)
{
    hrt_gsw_popmark();
}

JNIEXPORT jdouble JNICALL
Java_utils_HrtStopwatch_rtmicros(JNIEnv *env, jclass cls,
                                            jint y, jint x)
{
    return hrt_gsw_rtmicros(y, x);
}

JNIEXPORT jdouble JNICALL
Java_utils_HrtStopwatch_ctmicros(JNIEnv *env, jclass cls,
                                            jint y, jint x)
{
    return hrt_gsw_ctmicros(y, x);
}

JNIEXPORT void JNICALL
Java_utils_HrtStopwatch_clear(JNIEnv *env, jclass cls)
{
    hrt_gsw_clear();
}
