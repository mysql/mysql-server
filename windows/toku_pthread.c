#include <toku_portability.h>
#include <windows.h>
#include <toku_pthread.h>
#include "toku_assert.h"

int
toku_pthread_yield(void) {
    Sleep(0);
    return 0;
}

toku_pthread_win32_funcs pthread_win32 = {0};
HMODULE pthread_win32_dll = NULL;


//TODO: add a portability_init/destroy function (call in brt_init)
//TODO: Call in portability_init


int
toku_pthread_win32_init(void) {
    int r = 0;
    pthread_win32_dll = NULL;
    memset(&pthread_win32, 0, sizeof(pthread_win32));

    pthread_win32_dll = LoadLibrary(TEXT("pthreadVC2"));
    if (pthread_win32_dll == NULL)
        r = GetLastError();
    else {
#define LOAD_PTHREAD_FUNC(name) do { \
        pthread_win32.pthread_ ## name = (toku_pthread_win32_ ## name ## _func) GetProcAddress(pthread_win32_dll, "pthread_" #name); \
        assert(pthread_win32.pthread_ ## name != NULL); \
    } while (0)

        LOAD_PTHREAD_FUNC(attr_init);
        LOAD_PTHREAD_FUNC(attr_destroy);
        LOAD_PTHREAD_FUNC(attr_getstacksize);
        LOAD_PTHREAD_FUNC(attr_setstacksize);

        LOAD_PTHREAD_FUNC(mutex_init);
        LOAD_PTHREAD_FUNC(mutex_destroy);
        LOAD_PTHREAD_FUNC(mutex_lock);
        LOAD_PTHREAD_FUNC(mutex_trylock);
        LOAD_PTHREAD_FUNC(mutex_unlock);

        LOAD_PTHREAD_FUNC(cond_init);
        LOAD_PTHREAD_FUNC(cond_destroy);
        LOAD_PTHREAD_FUNC(cond_wait);
        LOAD_PTHREAD_FUNC(cond_timedwait);
        LOAD_PTHREAD_FUNC(cond_signal);
        LOAD_PTHREAD_FUNC(cond_broadcast);

        LOAD_PTHREAD_FUNC(rwlock_init);
        LOAD_PTHREAD_FUNC(rwlock_destroy);
        LOAD_PTHREAD_FUNC(rwlock_rdlock);
        LOAD_PTHREAD_FUNC(rwlock_wrlock);
        LOAD_PTHREAD_FUNC(rwlock_unlock);

        LOAD_PTHREAD_FUNC(create);
        LOAD_PTHREAD_FUNC(join);
        LOAD_PTHREAD_FUNC(self);
#undef LOAD_PTHREAD_FUNC
    }
    return r;
}

//TODO: Call in brt_destroy
int toku_pthread_win32_destroy(void) {
    assert(pthread_win32_dll != NULL);
    BOOL succ = FreeLibrary(pthread_win32_dll);
    assert(succ);
    return 0;
}


