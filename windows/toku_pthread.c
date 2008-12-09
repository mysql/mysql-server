#include <windows.h>
#include <assert.h>
#include <errno.h>
#include <toku_pthread.h>

int
toku_pthread_mutex_init(toku_pthread_mutex_t *mutex, const toku_pthread_mutexattr_t *attr) {
    assert(attr == NULL);
    // assert(!mutex->initialized);
    InitializeCriticalSection(&mutex->section);
    mutex->initialized = TRUE;
    return 0;
}

int
toku_pthread_mutex_destroy(toku_pthread_mutex_t *mutex) {
    assert(mutex->initialized);
    DeleteCriticalSection(&mutex->section);
    mutex->initialized = FALSE;
    return 0;
}

int
toku_pthread_mutex_lock(toku_pthread_mutex_t *mutex) {
    assert(mutex->initialized);
    EnterCriticalSection(&mutex->section);
    return 0;
}

#if 0
int
toku_pthread_mutex_trylock(toku_pthread_mutex_t *mutex) {
    assert(mutex->initialized);
    int r = 0;
    if (!TryEnterCriticalSection(&mutex->section))
        r = EBUSY;
    return r;
}
#endif

int
toku_pthread_mutex_unlock(toku_pthread_mutex_t *mutex) {
    assert(mutex->initialized);
    LeaveCriticalSection(&mutex->section);
    return 0;
}

int
toku_pthread_attr_init(toku_pthread_attr_t *attr) {
    attr->stacksize = 0;
    return 0;
}

int 
toku_pthread_attr_destroy(toku_pthread_attr_t *attr) {
    attr = attr;
    return 0;
}

int 
toku_pthread_attr_setstacksize(toku_pthread_attr_t *attr, size_t stacksize) {
    attr->stacksize = stacksize;
    return 0;
}

int 
toku_pthread_attr_getstacksize(toku_pthread_attr_t *attr, size_t *stacksize) {
    *stacksize = attr->stacksize;
    return 0;
}


static DWORD WINAPI 
toku_pthread_start(LPVOID a) {
    toku_pthread_t thread = (toku_pthread_t) a;
    thread->ret = thread->f(thread->arg);
    return 0;
}

int 
toku_pthread_create(toku_pthread_t *threadptr, const toku_pthread_attr_t *attr, void *(*f)(void *), void *arg) {
    size_t stacksize = 0;
    toku_pthread_t thread = malloc(sizeof (struct toku_pthread));
    if (thread == 0) 
        return ENOMEM;
    if (attr)
        stacksize = attr->stacksize;
    thread->f = f;
    thread->arg = arg;
    // _beginthread or CreateThread
    thread->handle = CreateThread(NULL, stacksize, toku_pthread_start, thread, 0, &thread->id);
    *threadptr = thread;
    return 0;
}

int 
toku_pthread_join(toku_pthread_t thread, void **ret) {
    WaitForSingleObject(thread->handle, INFINITE);
    if (ret)
        *ret = thread->ret;
    free(thread);
    return 0;
}

toku_pthread_t
toku_pthread_self(void) {
    // lookup a pthread by thread id and return it
    return 0;
}

#if 0
void
toku_pthread_exit(void *ret) {
    toku_pthread_t self = toku_pthread_self();
    thread->ret = ret;
    // _endthread or ExitThread
    ExitThread(0);
}
#endif

int 
toku_pthread_cond_init(toku_pthread_cond_t *cond, const toku_pthread_condattr_t *attr) {
    int r;
    assert(attr == 0);
    cond->events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cond->events[0] == NULL)
        return GetLastError();
    cond->events[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (cond->events[1] == NULL) {
        r = GetLastError();
        CloseHandle(cond->events[0]);
        return r;
    }
    cond->waiters = 0;
    return 0;
}

int 
toku_pthread_cond_destroy(toku_pthread_cond_t *cond) {
    int i;
    for (i=0; i<TOKU_PTHREAD_COND_NEVENTS; i++)
        CloseHandle(cond->events[i]);
    return 0;
}

int 
toku_pthread_cond_wait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex) {
    DWORD r;
    cond->waiters++;
    toku_pthread_mutex_unlock(mutex);
    r = WaitForMultipleObjects(TOKU_PTHREAD_COND_NEVENTS, cond->events, FALSE, INFINITE);
    toku_pthread_mutex_lock(mutex);
    cond->waiters--;
    if (cond->waiters == 0 && r == WAIT_OBJECT_0 + 1)
        ResetEvent(cond->events[1]);
    return 0;
}

int 
toku_pthread_cond_broadcast(toku_pthread_cond_t *cond) {
    if (cond->waiters > 0)
        SetEvent(cond->events[1]);
    return 0;
}

int 
toku_pthread_cond_signal(toku_pthread_cond_t *cond) { 
    if (cond->waiters > 0)
        SetEvent(cond->events[0]);
    return 0;
}

int
toku_pthread_yield(void) {
    Sleep(0);
    return 0;
}

