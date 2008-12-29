/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
// The helgrind1.tdbrun test should fail.  This is merely a check to verify that helgrind actually notices a race.
#include <assert.h>
#include <pthread.h>
#include "test.h"
int x;

void *starta(void* ignore __attribute__((__unused__))) {
    x++;
    return 0;
}
void *startb(void* ignore __attribute__((__unused__))) {
    x++;
    return 0;
}

int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    pthread_t a,b;
    { int x_l = pthread_create(&a, NULL, starta, NULL); assert(x_l==0); }
    { int x_l = pthread_create(&b, NULL, startb, NULL); assert(x_l==0); }
    { int x_l = pthread_join(a, NULL);           assert(x_l==0); }
    { int x_l = pthread_join(b, NULL);           assert(x_l==0); }
    return 0;
}
