#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_portability.h"
#include "rdtsc.h"
#include "trace_mem.h"

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
