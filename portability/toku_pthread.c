#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#define _GNU_SOURCE 1
#include <config.h>
#include <toku_pthread.h>

int toku_pthread_yield(void) {
#if defined(HAVE_PTHREAD_YIELD)
    return pthread_yield();
#elif defined(HAVE_PTHREAD_YIELD_NP)
    pthread_yield_np();
    return 0;
#else
# error "cannot find pthread_yield or pthread_yield_np"
#endif
}
