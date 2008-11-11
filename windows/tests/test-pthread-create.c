#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <windows.h>
#include "toku_pthread.h"

static void *myfunc1(void *arg) {
    printf("%s %p %lu\n", __FUNCTION__, arg, GetCurrentThreadId());
    fflush(stdout);
    Sleep(10*1000);
    return arg;
}

static void *myfunc2(void *arg) {
    printf("%s %p %lu\n", __FUNCTION__, arg, GetCurrentThreadId());
    fflush(stdout);
    Sleep(10*1000);
    return arg;
}

int main(void) {
#define N 10
    toku_pthread_t t[N];
    int i;

    for (i=0; i<N; i++) {
        toku_pthread_create(&t[i], NULL, i & 1 ? myfunc1 : myfunc2, (void *)i);
    }
    for (i=0; i<N; i++) {
        void *ret;
        toku_pthread_join(t[i], &ret);
        assert(ret == (void*)i);
    }
    return 0;
}

