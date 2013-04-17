#include "includes.h"

int verbose = 0;

// test create and destroy

static void
test_create_destroy (void) {
    struct ctpair_rwlock the_rwlock, *rwlock = &the_rwlock;

    ctpair_rwlock_init(rwlock);
    ctpair_rwlock_destroy(rwlock);
}

// test read lock and unlock with no writers

static void
test_simple_read_lock (int n) {
    struct ctpair_rwlock the_rwlock, *rwlock = &the_rwlock;

    ctpair_rwlock_init(rwlock);
    assert(ctpair_pinned(rwlock) == 0);
    int i;
    for (i=1; i<=n; i++) {
        ctpair_read_lock(rwlock, 0);
        assert(ctpair_pinned(rwlock) == i);
        assert(ctpair_users(rwlock) == i);
    }
    for (i=n-1; i>=0; i--) {
        ctpair_read_unlock(rwlock);
        assert(ctpair_pinned(rwlock) == i);
        assert(ctpair_users(rwlock) == i);
    }
    ctpair_rwlock_destroy(rwlock);
}

// test write lock and unlock with no readers

static void
test_simple_write_lock (void) {
    struct ctpair_rwlock the_rwlock, *rwlock = &the_rwlock;

    ctpair_rwlock_init(rwlock);
    assert(ctpair_users(rwlock) == 0);
    ctpair_write_lock(rwlock, 0);
    assert(ctpair_writers(rwlock) == 1);
    assert(ctpair_users(rwlock) == 1);
    ctpair_write_unlock(rwlock);
    assert(ctpair_users(rwlock) == 0);
    ctpair_rwlock_destroy(rwlock);
}

struct rw_event {
    int e;
    struct ctpair_rwlock the_rwlock;
    pthread_mutex_t mutex;
};

static void
rw_event_init (struct rw_event *rwe) {
    rwe->e = 0;
    ctpair_rwlock_init(&rwe->the_rwlock);
    int r = pthread_mutex_init(&rwe->mutex, 0); assert(r == 0);
}

static void
rw_event_destroy (struct rw_event *rwe) {
    ctpair_rwlock_destroy(&rwe->the_rwlock);
    int r = pthread_mutex_destroy(&rwe->mutex); assert(r == 0);
}

static void *
test_writer_priority_thread (void *arg) {
    struct rw_event *rwe = arg;
    int r;

    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_write_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 3);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);
    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    rwe->e++; assert(rwe->e == 4);
    ctpair_write_unlock(&rwe->the_rwlock);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);
    
    return arg;
}

// test writer priority over new readers

static void
test_writer_priority (void) {
    struct rw_event rw_event, *rwe = &rw_event;
    int r;

    rw_event_init(rwe);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_read_lock(&rwe->the_rwlock, &rwe->mutex);
    sleep(1);
    rwe->e++; assert(rwe->e == 1);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);

    pthread_t tid;
    r = pthread_create(&tid, 0, test_writer_priority_thread, rwe);
    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    rwe->e++; assert(rwe->e == 2);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);

    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_read_unlock(&rwe->the_rwlock);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);
    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_read_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 5);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);
    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_read_unlock(&rwe->the_rwlock);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);

    void *ret;
    r = pthread_join(tid, &ret); assert(r == 0);

    rw_event_destroy(rwe);
}

// test single writer

static void *
test_single_writer_thread (void *arg) {
    struct rw_event *rwe = arg;
    int r;

    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_write_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 3);
    assert(ctpair_writers(&rwe->the_rwlock) == 1);
    ctpair_write_unlock(&rwe->the_rwlock);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);
    
    return arg;
}

static void
test_single_writer (void) {
    struct rw_event rw_event, *rwe = &rw_event;
    int r;

    rw_event_init(rwe);
    assert(ctpair_writers(&rwe->the_rwlock) == 0);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    ctpair_write_lock(&rwe->the_rwlock, &rwe->mutex);
    assert(ctpair_writers(&rwe->the_rwlock) == 1);
    sleep(1);
    rwe->e++; assert(rwe->e == 1);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);

    pthread_t tid;
    r = pthread_create(&tid, 0, test_single_writer_thread, rwe);
    sleep(1);
    r = pthread_mutex_lock(&rwe->mutex); assert(r == 0);
    rwe->e++; assert(rwe->e == 2);
    assert(ctpair_writers(&rwe->the_rwlock) == 1);
    assert(ctpair_users(&rwe->the_rwlock) == 2);
    ctpair_write_unlock(&rwe->the_rwlock);
    r = pthread_mutex_unlock(&rwe->mutex); assert(r == 0);

    void *ret;
    r = pthread_join(tid, &ret); assert(r == 0);

    assert(ctpair_writers(&rwe->the_rwlock) == 0);
    rw_event_destroy(rwe);
}

int main(int argc, char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
    }
    test_create_destroy();
    test_simple_read_lock(0);
    test_simple_read_lock(42);
    test_simple_write_lock();
    test_writer_priority();
    test_single_writer();
    
    return 0;
}
