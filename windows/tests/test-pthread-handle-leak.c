// test for a pthread handle leak

#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <toku_assert.h>
#include <unistd.h>
#include <toku_pthread.h>

static void *mythreadfunc(void *arg) {
    return arg;
}

int test_main(int argc, char *argv[]) {
#define N 1000000

    int i;
    for (i=0; i<N; i++) {
	int r;
	toku_pthread_t tid;
        r = toku_pthread_create(&tid, NULL, mythreadfunc, (void *)i);
	assert(r == 0);
        void *ret;
        r = toku_pthread_join(tid, &ret);
        assert(r == 0 && ret == (void*)i);
    }
    printf("ok\n"); fflush(stdout);
    return 0;
}

