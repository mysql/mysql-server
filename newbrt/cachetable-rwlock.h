// A read lock is acquired by threads that get and pin an entry in the
// cachetable. A write lock is acquired by the writer thread when an entry
// is evicted from the cachetable and is being written storage.

// Properties:
// 1. multiple readers, no writers
// 2. one writer at a time
// 3. pending writers have priority over pending readers

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the rwlock itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct ctpair_rwlock *CTPAIR_RWLOCK;
struct ctpair_rwlock {
    int pinned;                  // the number of readers
    int want_pin;                // the number of blocked readers
    toku_pthread_cond_t wait_pin;
    int writer;                  // the number of writers
    int want_write;              // the number of blocked writers
    toku_pthread_cond_t wait_write;
};

// initialize a read write lock

static __attribute__((__unused__))
void
ctpair_rwlock_init(CTPAIR_RWLOCK rwlock) {
    int r;
    rwlock->pinned = rwlock->want_pin = 0;
    r = toku_pthread_cond_init(&rwlock->wait_pin, 0); assert(r == 0);
    rwlock->writer = rwlock->want_write = 0;
    r = toku_pthread_cond_init(&rwlock->wait_write, 0); assert(r == 0);
}

// destroy a read write lock

static __attribute__((__unused__))
void
ctpair_rwlock_destroy(CTPAIR_RWLOCK rwlock) {
    int r;
    assert(rwlock->pinned == 0 && rwlock->want_pin == 0);
    assert(rwlock->writer == 0 && rwlock->want_write == 0);
    r = toku_pthread_cond_destroy(&rwlock->wait_pin); assert(r == 0);
    r = toku_pthread_cond_destroy(&rwlock->wait_write); assert(r == 0);
}

// obtain a read lock
// expects: mutex is locked

static inline void ctpair_read_lock(CTPAIR_RWLOCK rwlock, toku_pthread_mutex_t *mutex) {
    if (rwlock->writer || rwlock->want_write) {
        rwlock->want_pin++;
        while (rwlock->writer || rwlock->want_write) {
            int r = toku_pthread_cond_wait(&rwlock->wait_pin, mutex); assert(r == 0);
        }
        rwlock->want_pin--;
    }
    rwlock->pinned++;
}

// release a read lock
// expects: mutex is locked

static inline void ctpair_read_unlock(CTPAIR_RWLOCK rwlock) {
    rwlock->pinned--;
    if (rwlock->pinned == 0 && rwlock->want_write) {
        int r = toku_pthread_cond_signal(&rwlock->wait_write); assert(r == 0);
    }
}

// obtain a write lock
// expects: mutex is locked

static inline void ctpair_write_lock(CTPAIR_RWLOCK rwlock, toku_pthread_mutex_t *mutex) {
    if (rwlock->pinned || rwlock->writer) {
        rwlock->want_write++;
        while (rwlock->pinned || rwlock->writer) {
            int r = toku_pthread_cond_wait(&rwlock->wait_write, mutex); assert(r == 0);
        }
        rwlock->want_write--;
    }
    rwlock->writer++;
}

// release a write lock
// expects: mutex is locked

static inline void ctpair_write_unlock(CTPAIR_RWLOCK rwlock) {
    rwlock->writer--;
    if (rwlock->writer == 0) {
        if (rwlock->want_write) {
            int r = toku_pthread_cond_signal(&rwlock->wait_write); assert(r == 0);
        } else if (rwlock->want_pin) {
            int r = toku_pthread_cond_broadcast(&rwlock->wait_pin); assert(r == 0);
        }
    }
}

// returns: the number of readers

static inline int ctpair_pinned(CTPAIR_RWLOCK rwlock) {
    return rwlock->pinned;
}

// returns: the number of writers

static inline int ctpair_writers(CTPAIR_RWLOCK rwlock) {
    return rwlock->writer;
}

// returns: the sum of the number of readers, pending readers, writers, and
// pending writers

static inline int ctpair_users(CTPAIR_RWLOCK rwlock) {
    return rwlock->pinned + rwlock->want_pin + rwlock->writer + rwlock->want_write;
}
