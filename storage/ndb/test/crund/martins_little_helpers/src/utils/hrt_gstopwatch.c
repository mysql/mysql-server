/*
 * hrt_gstopwatch.c
 *
 */

#include "hrt_gstopwatch.h"

/*
 * High-Resolution Time Global Stopwatch Utility -- Implementation
 */

static hrt_stopwatch gsw;
//hrt_stopwatch gsw;

extern void
hrt_gsw_init(int cap)
{
    hrt_sw_init(&gsw, cap);
}

extern void
hrt_gsw_close(void)
{
    hrt_sw_close(&gsw);
}

extern int
hrt_gsw_top(void)
{
    return hrt_sw_top(&gsw);
}

extern int
hrt_gsw_capacity(void)
{
    return hrt_sw_capacity(&gsw);
}

extern int
hrt_gsw_pushmark(void)
{
    return hrt_sw_pushmark(&gsw);
}

extern double
hrt_gsw_rtmicros(int y, int x)
{
    return hrt_sw_rtmicros(&gsw, y, x);
}

extern double
hrt_gsw_ctmicros(int y, int x)
{
    return hrt_sw_ctmicros(&gsw, y, x);
}

extern void
hrt_gsw_clear(void)
{
    hrt_sw_clear(&gsw);
}
