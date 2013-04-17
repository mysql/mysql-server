#ifndef _TOKU_TRACE_MEM_H
#define _TOKU_TRACE_MEM_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

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

// print the trace
void toku_print_trace_mem(void) __attribute__((__visibility__("default")));

//  some trace functions added for the bulk loader
void bl_trace(const char *func __attribute__((unused)), 
              int         line __attribute__ ((unused)), 
              const char *str  __attribute__((unused)))
    __attribute__((unused));
void bl_trace_end(void) __attribute__((unused));

#define BL_DO_TRACE 0
#define BL_TRACE_PRINT 0

#if BL_DO_TRACE    
#define BL_TRACE(str) bl_trace(__FUNCTION__, __LINE__, str)
#define BL_TRACE_END bl_trace_end()
#else
#define BL_TRACE(str)
#define BL_TRACE_END
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif
                 
#endif
