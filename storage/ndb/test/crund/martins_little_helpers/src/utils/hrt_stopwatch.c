/*
 * hrt_stopwatch.c
 *
 */

#include <stdlib.h>
#include <assert.h>

#include "hrt_stopwatch.h"

/*
 * High-Resolution Time Stopwatch Utility -- Implementation
 */

extern void
hrt_sw_init(hrt_stopwatch* sw, int cap)
{
    sw->cap = cap;
    sw->top = 0;
    sw->tstamps = malloc(sizeof(hrt_tstamp) * cap);
}

extern void
hrt_sw_close(hrt_stopwatch* sw)
{
    free(sw->tstamps);
    sw->top = sw->cap = 0;
}

extern int
hrt_sw_top(const hrt_stopwatch* sw)
{
    return sw->top;
}

extern int
hrt_sw_capacity(const hrt_stopwatch* sw)
{
    return sw->cap;
}

extern int
hrt_sw_pushmark(hrt_stopwatch* sw)
{
    assert (sw->top+1 < sw->cap);
    int r = hrt_tnow(sw->tstamps + sw->top);
    assert (r == 0);
    (void)r;
    return sw->top++;
}

extern double
hrt_sw_rtmicros(const hrt_stopwatch* sw, int y, int x)
{
    assert (0 <= y && y < sw->top);
    assert (0 <= x && x < sw->top);
    return hrt_rtmicros(&sw->tstamps[y].rtstamp, &sw->tstamps[x].rtstamp);
}

extern double
hrt_sw_ctmicros(const hrt_stopwatch* sw, int y, int x)
{
    assert (0 <= y && y < sw->top);
    assert (0 <= x && x < sw->top);
    return hrt_ctmicros(&sw->tstamps[y].ctstamp, &sw->tstamps[x].ctstamp);
}

extern void
hrt_sw_clear(hrt_stopwatch* sw)
{
    sw->top = 0;
}

