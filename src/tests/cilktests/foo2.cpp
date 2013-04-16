/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include "foo2.h"
#include "cilk.h"

pthread_t pt[2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" void* start (void *extra __attribute__((__unused__))) {
    { int r = pthread_mutex_lock(&mutex);    assert(r==0); }
    printf("T%lx got lock\n", pthread_self());
    sleep(1);
    printf("T%lx releasing lock\n", pthread_self());
    { int r = pthread_mutex_unlock(&mutex); assert(r==0); }
    return 0;
}

void create_pthread(void) {
    for (int i=0; i<2; i++) {
	int r = pthread_create(&pt[i], 0, start, NULL);
	assert(r==0);
    }
}

void join_pthread (void) {
    for (int i=0; i<2; i++) {
	int r = pthread_join(pt[i], NULL);
	assert(r==0);
    }
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    __cilkscreen_disable_instrumentation();
    create_pthread();
    __cilkscreen_enable_instrumentation();
    do_foo();
    join_pthread();
    return 0;
}
