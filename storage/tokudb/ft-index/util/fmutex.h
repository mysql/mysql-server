#pragma once

// fair mutex
struct fmutex {
    pthread_mutex_t mutex;
    int mutex_held;
    int num_want_mutex;
    struct queue_item *wait_head;
    struct queue_item *wait_tail;
};

// item on the queue
struct queue_item {
    pthread_cond_t *cond;
    struct queue_item *next;
};

static void enq_item(struct fmutex *fm, struct queue_item *const item) {
    assert(item->next == NULL);
    if (fm->wait_tail != NULL) {
        fm->wait_tail->next = item;
    } else {
        assert(fm->wait_head == NULL);
        fm->wait_head = item;
    }
    fm->wait_tail = item;
}

static pthread_cond_t *deq_item(struct fmutex *fm) {
    assert(fm->wait_head != NULL);
    assert(fm->wait_tail != NULL);
    struct queue_item *item = fm->wait_head;
    fm->wait_head = fm->wait_head->next;
    if (fm->wait_tail == item) {
        fm->wait_tail = NULL;
    }
    return item->cond;
}

void fmutex_create(struct fmutex *fm) {
    pthread_mutex_init(&fm->mutex, NULL);
    fm->mutex_held = 0;
    fm->num_want_mutex = 0;
    fm->wait_head = NULL;
    fm->wait_tail = NULL;
}

void fmutex_destroy(struct fmutex *fm) {
    pthread_mutex_destroy(&fm->mutex);
}

// Prerequisite: Holds m_mutex.
void fmutex_lock(struct fmutex *fm) {
    pthread_mutex_lock(&fm->mutex);

    if (fm->mutex_held == 0 || fm->num_want_mutex == 0) {
        // No one holds the lock.  Grant the write lock.
        fm->mutex_held = 1;
        return;
    }

    pthread_cond_t cond;
    pthread_cond_init(&cond, NULL);
    struct queue_item item = { .cond = &cond, .next = NULL };
    enq_item(fm, &item);

    // Wait for our turn.
    ++fm->num_want_mutex;
    pthread_cond_wait(&cond, &fm->mutex);
    pthread_cond_destroy(&cond);

    // Now it's our turn.
    assert(fm->num_want_mutex > 0);
    assert(fm->mutex_held == 0);

    // Not waiting anymore; grab the lock.
    --fm->num_want_mutex;
    fm->mutex_held = 1;

    pthread_mutex_unlock();
}

void fmutex_mutex_unlock(struct fmutex *fm) {
    pthread_mutex_lock();

    fm->mutex_held = 0;
    if (fm->wait_head == NULL) {
        assert(fm->num_want_mutex == 0);
        return;
    }
    assert(fm->num_want_mutex > 0);

    // Grant lock to the next waiter
    pthread_cond_t *cond = deq_item(fm);
    pthread_cond_signal(cond);

    pthread_mutex_unlock();
}

int fmutex_users(struct fmutex *fm) const {
    return fm->mutex_held + fm->num_want_mutex;
}

int fmutex_blocked_users(struct fmutex *fm) const {
    return fm->num_want_mutex;
}
