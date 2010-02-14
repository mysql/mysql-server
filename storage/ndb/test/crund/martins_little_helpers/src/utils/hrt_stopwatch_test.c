/*
 * hrt_stopwatch_test.c
 *
 */

#include <assert.h>
#include <stdio.h>

#include "hrt_stopwatch.h"
#include "hrt_gstopwatch.h"

/*
 * High-Resolution Time Stopwatch Utility -- Test
 */


static void
do_something(void)
{
    const unsigned long loop = 1000000000L;
    unsigned long i;
    static volatile unsigned long dummy;
    for (i = 0; i < loop; i++)
	dummy = i;
}

int
main(int argc, const char* argv[])
{
    printf("--> main()\n");

    printf("init stopwatches...\n");
    hrt_gsw_init(10);
    hrt_stopwatch sw;
    hrt_sw_init(&sw, 10);
    
    printf("marking global time...\n");
    int g0 = hrt_gsw_pushmark();
    do_something();

    printf("marking time...\n");
    int t0 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking time...\n");
    int t1 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking time...\n");
    int t2 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking global time...\n");
    int g2 = hrt_gsw_pushmark();

    printf("\namount of times:\n");
    double rt0 = hrt_sw_rtmicros(&sw, t1, t0);
    double rt1 = hrt_sw_rtmicros(&sw, t2, t1);
    double rt2 = hrt_sw_rtmicros(&sw, t2, t0);
    double grt2 = hrt_gsw_rtmicros(g2, g0);
    printf("[t0..t1] real   = %.3f us\n", rt0);
    printf("[t1..t2] real   = %.3f us\n", rt1);
    printf("[t0..t2] real   = %.3f us\n", rt2);
    printf("[g0..g2] real   = %.3f us\n", grt2);
    double ct0 = hrt_sw_ctmicros(&sw, t1, t0);
    double ct1 = hrt_sw_ctmicros(&sw, t2, t1);
    double ct2 = hrt_sw_ctmicros(&sw, t2, t0);
    double gct2 = hrt_gsw_ctmicros(g2, g0);
    printf("[t0..t1] cpu    = %.3f us\n", ct0);
    printf("[t1..t2] cpu    = %.3f us\n", ct1);
    printf("[t0..t2] cpu    = %.3f us\n", ct2);
    printf("[g0..g2] cpu    = %.3f us\n", gct2);
    
    printf("closing stopwatches...\n");
    hrt_sw_close(&sw);
    hrt_gsw_close();

    printf("\n<-- main()\n");
    return 0;
}
