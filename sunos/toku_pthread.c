#include <toku_pthread.h>

int toku_pthread_yield(void) {
    return sched_yield();
}
