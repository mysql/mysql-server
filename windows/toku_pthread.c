#include <toku_portability.h>
#include <windows.h>
#include <toku_pthread.h>

int
toku_pthread_yield(void) {
    Sleep(0);
    return 0;
}

