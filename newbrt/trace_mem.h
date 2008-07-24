// a circular log of trace entries is maintained in memory. the trace
// entry consists of a string pointer, an integer, and the processor
// timestamp. there are functions to add an entry to the end of the
// trace log, and to print the trace log.
// example: one can use the __FUNCTION__ and __LINE__ macros as
// the arguments to the toku_add_trace function.
// performance: we trade speed for size by not compressing the trace
// entries.

void toku_add_trace_mem(const char *str, int n);
// add an entry to the end of the trace which consists of a string
// pointer, a number, and the processor timestamp

void toku_print_trace_mem();
// print the trace 
