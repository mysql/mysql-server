#ifndef _TOKU_TRACE_MEM_H
#define _TOKU_TRACE_MEM_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// Define this even when traces are compiled out so we don't have to recompile things like scanscan.c
// print the trace
void toku_print_trace_mem(void) __attribute__((__visibility__("default")));

#define BL_DO_TRACE 1
// BL_SIMPLE_TRACE 1 is Bradley's in-memory trace analysis.
// BL_SIMPLE_TRACE 0 is Dave's post-processing analysis.
#define BL_SIMPLE_TRACE 1
#define BL_TRACE_PRINT 0

#if BL_DO_TRACE && !BL_SIMPLE_TRACE
// a circular log of trace entries is maintained in memory. the trace
// entry consists of a string pointer, an integer, and the processor
// timestamp. there are functions to add an entry to the end of the
// trace log, and to print the trace log.
// example: one can use the __FUNCTION__ and __LINE__ macros as
// the arguments to the toku_add_trace function.
// performance: we trade speed for size by not compressing the trace
// entries.

// add an entry to the end of the trace which consists of a string
// pointer, a number, and the processor timestamp
void toku_add_trace_mem(const char *str, int n) __attribute__((__visibility__("default")));

//  some trace functions added for the bulk loader
void bl_trace(const char *func __attribute__((unused)), 
              int         line __attribute__ ((unused)), 
              const char *str  __attribute__((unused)))
    __attribute__((unused));
void bl_trace_end(void) __attribute__((unused));

#define BL_TRACE(sym) bl_trace(__FUNCTION__, __LINE__, #sym)
#define BL_TRACE_END bl_trace_end()

#elif BL_DO_TRACE && BL_SIMPLE_TRACE


typedef enum bl_trace_enum {BLT_START,

			    blt_calibrate_begin,
			    blt_calibrate_done,

			    blt_open,

			    // Time spent in the extractor
			    blt_extractor_init,
			    blt_extractor,
			    blt_extract_deq,
			    blt_sort_and_write_rows,

			    // Time spent by the main thread in parallel to the extractor
			    blt_do_put,
			    blt_extract_enq,
			    blt_join_on_extractor,

			    // Time spent in the fractal thread
			    blt_fractal_thread,
			    blt_fractal_deq,

			    // Time spent by the main thread in parallel to the fractal thread
			    blt_start_fractal_thread,
			    blt_do_i,
			    blt_read_row,
			    blt_fractal_enq,
			    blt_join_on_fractal,
			    blt_close,

			    //
			    BLT_LIMIT}
    BL_TRACE_ENUM;
static const char * blt_to_string (BL_TRACE_ENUM) __attribute__((__unused__));
#define BLSCASE(s) case s: return #s
static const char * blt_to_string (BL_TRACE_ENUM i) {
    switch(i) {
	BLSCASE(BLT_START);
	BLSCASE(blt_calibrate_begin);
	BLSCASE(blt_calibrate_done);
	BLSCASE(blt_close);
	BLSCASE(blt_do_i);
	BLSCASE(blt_do_put);
	BLSCASE(blt_extract_deq);
	BLSCASE(blt_extract_enq);
	BLSCASE(blt_extractor);
	BLSCASE(blt_extractor_init);
	BLSCASE(blt_fractal_deq);
	BLSCASE(blt_fractal_enq);
	BLSCASE(blt_fractal_thread);
	BLSCASE(blt_join_on_extractor);
	BLSCASE(blt_join_on_fractal);
	BLSCASE(blt_open);
	BLSCASE(blt_read_row);
	BLSCASE(blt_sort_and_write_rows);
	BLSCASE(blt_start_fractal_thread);
	BLSCASE(BLT_LIMIT);
    }
    return NULL;
}
typedef unsigned long long bl_time_t;
bl_time_t bl_trace(const BL_TRACE_ENUM, const int quiet);
bl_time_t bl_time_now(void);
double bl_time_diff(const bl_time_t a, const bl_time_t b);
void bl_trace_end(void);

 #define BL_TRACE(sym) bl_trace(sym, 0)
 #if 0
  #define BL_TRACE_QUIET(sym) bl_trace(sym, 1)
 #else
  #define BL_TRACE_QUIET(sym)
 #endif
 #define BL_TRACE_END bl_trace_end()

#else

#define BL_TRACE(sym)
#define BL_TRACE_FROM(sym,q,t)
#define BL_TRACE_QUIET(sym)
#define BL_TRACE_FROM_QUIET(sym,q,t)
#define BL_TRACE_END

#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif
                 
#endif
