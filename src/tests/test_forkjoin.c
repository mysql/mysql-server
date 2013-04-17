#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static void *
f (void *arg) {
    //pthread_exit(arg);  // pthread_exit has a memoryh leak.
    return arg;
}

int main() {
    pthread_t t;
    int r = pthread_create(&t, 0, f, 0); assert(r == 0);
    void *ret;
    r = pthread_join(t, &ret); assert(r == 0);
    return 0;
}
