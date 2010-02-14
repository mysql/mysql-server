
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
