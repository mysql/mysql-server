
/*
 * DO not delete this file. The hack here ensures that pthread_init() gets 
 * called before main does. This doesn't fix everything. It is still 
 * possible for a c++ module to reley on constructors that need pthreads.
 */
#include <pthread.h>

char __pthread_init_hack = 42;
