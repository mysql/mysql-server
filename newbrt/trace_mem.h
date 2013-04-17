#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// a circular log of trace entries is maintained in memory. the trace
// entry consists of a string pointer, an integer, and the processor
// timestamp. there are functions to add an entry to the end of the
// trace log, and to print the trace log.
// example: one can use the __FUNCTION__ and __LINE__ macros as
// the arguments to the toku_add_trace function.
// performance: we trade speed for size by not compressing the trace
// entries.

void toku_add_trace_mem(const char *str, int n) __attribute__((__visibility__("default")));
// add an entry to the end of the trace which consists of a string
// pointer, a number, and the processor timestamp

void toku_print_trace_mem(void) __attribute__((__visibility__("default")));
// print the trace
