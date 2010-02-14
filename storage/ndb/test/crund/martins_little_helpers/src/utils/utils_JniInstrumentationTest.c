
#include <jni.h>

#include "utils_JniInstrumentationTest.h"
#include "hrt_gstopwatch.h"

JNIEXPORT void JNICALL
Java_utils_JniInstrumentationTest_aNativeMethod(JNIEnv * env, jclass cls)
{
    printf("--> Java_utils_JniInstrumentationTest_aNativeMethod()\n");

    //printf("init libjnitest stopwatch...\n\n");
    //hrt_gsw_init(10);

    printf("marking time C ...\n");
    int g1 = hrt_gsw_pushmark();
    printf("DOING THIS & THAT\n");
    int g2 = hrt_gsw_pushmark();
    printf("... marked time C\n");

    (void)g1;
    (void)g2;
    //printf("\namount of times C:\n");
    //double grt2 = hrt_gsw_rtmicros(g2, g1);
    //printf("[g%d..g%d] real   = %.3f us\n", g1, g2, grt2);
    //double gct2 = hrt_gsw_ctmicros(g2, g1);
    //printf("[g%d..g%d] cpu    = %.3f us\n", g1, g2, gct2);

    //printf("\nclosing libjnitest stopwatch...\n");
    //hrt_gsw_close();

    printf("<-- Java_utils_JniInstrumentationTest_aNativeMethod()\n");
}
