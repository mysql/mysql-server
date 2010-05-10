#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "rdtsc.h"
#include "trace_mem.h"

#if BL_DO_TRACE && BL_SIMPLE_TRACE

static double saved_scale_factor = 1e-9; // approximate until we get the actual scale factor
static unsigned long long first_time = 0;
static __thread unsigned long long prev_time = 0;
static unsigned long long trace_hist[BLT_LIMIT];

unsigned long long bl_trace (const BL_TRACE_ENUM l, const int quiet) {
    assert(l<BLT_LIMIT);
    unsigned long long t = rdtsc();
    if (first_time==0) {
	first_time = t;
    }
    if (prev_time != 0) {
	unsigned long long diff = t-prev_time;
	if (l==blt_calibrate_done) {
	    saved_scale_factor = 1/(double)diff;
	}
	if (!quiet)
	    printf("-> %30s %21llu %21llu %13.6fs\n", blt_to_string(l), trace_hist[l], t, (t-first_time)*saved_scale_factor);
	trace_hist[l] += diff;
    }
    prev_time = t;
    return t;
}

bl_time_t bl_time_now(void) {
    return rdtsc();
}
double bl_time_diff(const bl_time_t a, const bl_time_t b) {
    return (a-b)*saved_scale_factor;
}


void bl_trace_end(void) {
    double scale_factor = trace_hist[blt_calibrate_done];
    unsigned long long total = 0;
    for (BL_TRACE_ENUM i=0; i<BLT_LIMIT; i++) {
	total+=trace_hist[i];
    }
    for (BL_TRACE_ENUM i=0; i<BLT_LIMIT; i++) {
	printf("%25s %20lld %8.3fs %5.1f%%\n", blt_to_string(i), trace_hist[i], trace_hist[i]/scale_factor, 100.0*trace_hist[i]/(double)total);
    }
}


#elif BL_DO_TRACE && !BL_SIMPLE_TRACE

// customize this as required
#define NTRACE 0
#if NTRACE
static struct toku_trace {
    const char *str;
    int n;
    unsigned long long ts;
} toku_trace[NTRACE];

static int toku_next_trace = 0;
#endif

void toku_add_trace_mem (const char *str __attribute__((unused)),
			 int n __attribute__((unused))) {
#if USE_RDTSC && NTRACE
    int i = toku_next_trace++;
    if (toku_next_trace >= NTRACE) toku_next_trace = 0;
    toku_trace[i].ts = rdtsc();
    toku_trace[i].str = str;
    toku_trace[i].n = n;
#endif
}

void toku_print_trace_mem(void) {
#if NTRACE
    int i = toku_next_trace;
    do {
        if (toku_trace[i].str)
            printf("%llu %s:%d\n", toku_trace[i].ts, toku_trace[i].str, toku_trace[i].n);
        i++;
        if (i >= NTRACE) i = 0;
    } while (i != toku_next_trace);
#endif
}

/*
 *   Some trace functions (similar) added for the bulk loader
 */


#define BL_TRACE_MAX_ENTRIES 1000000

#if BL_DO_TRACE
static struct bl_trace_s {
    const char *str;
    const char *func;
    int line;
    int tid;
    unsigned long long ts;
} bl_traces[BL_TRACE_MAX_ENTRIES];

static int bl_next_trace = 0;
static FILE* bltrace_fp = NULL;
#endif

void bl_trace(const char *func __attribute__((unused)), 
                     int  line __attribute__((unused)), 
              const char *str  __attribute__((unused))) 
{
#if BL_DO_TRACE
    if ( bl_next_trace < BL_TRACE_MAX_ENTRIES ) {
        bl_traces[bl_next_trace].ts = rdtsc();
        bl_traces[bl_next_trace].str = str;
        bl_traces[bl_next_trace].func = func;
        bl_traces[bl_next_trace].line = line;
        bl_traces[bl_next_trace].tid = toku_os_gettid();
        bl_next_trace++;
    }
    #if BL_TRACE_PRINT
    {
        int i=bl_next_trace - 1;
        printf("%10d %5d %20llu %s:%d %s\n", 
               i,
               bl_traces[i].tid,
               bl_traces[i].ts,
               bl_traces[i].func,
               bl_traces[i].line,
               bl_traces[i].str);
    }
    #endif
#endif
}

void bl_trace_end(void)
{
#error
#if BL_DO_TRACE
    char bltracefile[128];
    sprintf(bltracefile, "brtloader_%d.trace", toku_os_getpid());;
    printf("brtloader_%d.trace", toku_os_getpid());
    bltrace_fp = fopen(bltracefile, "w");
    assert(bltrace_fp != NULL);
    for (int i=0;i<bl_next_trace;i++) {
        fprintf(bltrace_fp, "%10d %5d %20llu %s:%d %s\n", 
                i,
                bl_traces[i].tid,
                bl_traces[i].ts,
                bl_traces[i].func,
                bl_traces[i].line,
                bl_traces[i].str);
    }
    fclose(bltrace_fp);
#endif
}

#endif
