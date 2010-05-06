#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#define _GNU_SOURCE 1
#include <toku_pthread.h>

int toku_pthread_yield(void) {
    return pthread_yield();
}
