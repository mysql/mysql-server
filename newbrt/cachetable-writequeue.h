// When objects are evicted from the cachetable, they are written to storage by a 
// thread in a thread pool.  The objects are placed onto a write queue that feeds 
// the thread pool.  The write queue expects that an external mutex is used to 
// protect it.

typedef struct writequeue *WRITEQUEUE;
struct writequeue {
    PAIR head, tail;            // head and tail of the linked list of pair's
    toku_pthread_cond_t wait_read;   // wait for read
    int want_read;              // number of threads waiting to read
    toku_pthread_cond_t wait_write;  // wait for write
    int want_write;             // number of threads waiting to write
    int ninq;                   // number of pairs in the queue
    char closed;                // kicks waiting threads off of the write queue
};

// initialize a writequeue
// expects: the writequeue is not initialized
// effects: the writequeue is set to empty and the condition variable is initialized

static void writequeue_init(WRITEQUEUE wq) {
    wq->head = wq->tail = 0;
    int r;
    r = toku_pthread_cond_init(&wq->wait_read, 0); assert(r == 0);
    wq->want_read = 0;
    r = toku_pthread_cond_init(&wq->wait_write, 0); assert(r == 0);
    wq->want_write = 0;
    wq->ninq = 0;
    wq->closed = 0;
}

// destroy a writequeue
// expects: the writequeue must be initialized and empty

static void writequeue_destroy(WRITEQUEUE wq) {
    assert(wq->head == 0 && wq->tail == 0);
    int r;
    r = toku_pthread_cond_destroy(&wq->wait_read); assert(r == 0);
    r = toku_pthread_cond_destroy(&wq->wait_write); assert(r == 0);
}

// close the writequeue
// effects: signal any threads blocked in the writequeue

static void writequeue_set_closed(WRITEQUEUE wq) {
    wq->closed = 1;
    int r;
    r = toku_pthread_cond_broadcast(&wq->wait_read); assert(r == 0);
    r = toku_pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
}

// determine whether or not the write queue is empty
// return: 1 if the write queue is empty, otherwise 0

static int writequeue_empty(WRITEQUEUE wq) {
    return wq->head == 0;
}

// put a pair at the tail of the write queue
// expects: the mutex is locked
// effects: append the pair to the end of the write queue and signal
// any readers.

static void writequeue_enq(WRITEQUEUE wq, PAIR pair) {
    pair->next_wq = 0;
    if (wq->tail)
        wq->tail->next_wq = pair;
    else
        wq->head = pair;
    wq->tail = pair;
    wq->ninq++;
    if (wq->want_read) {
        int r = toku_pthread_cond_signal(&wq->wait_read); assert(r == 0);
    }
}

// get a pair from the head of the write queue
// expects: the mutex is locked
// effects: wait until the writequeue is not empty, remove the first pair from the
// write queue and return it
// returns: 0 if success, otherwise an error 

static int writequeue_deq(WRITEQUEUE wq, toku_pthread_mutex_t *mutex, PAIR *pairptr) {
    while (writequeue_empty(wq)) {
        if (wq->closed)
            return EINVAL;
        wq->want_read++;
        int r = toku_pthread_cond_wait(&wq->wait_read, mutex); assert(r == 0);
        wq->want_read--;
    }
    PAIR pair = wq->head;
    wq->head = pair->next_wq;
    if (wq->head == 0)
        wq->tail = 0;
    wq->ninq--;
    pair->next_wq = 0;
    *pairptr = pair;
    return 0;
}

// suspend the writer thread
// expects: the mutex is locked

static void writequeue_wait_write(WRITEQUEUE wq, toku_pthread_mutex_t *mutex) {
    wq->want_write++;
    int r = toku_pthread_cond_wait(&wq->wait_write, mutex); assert(r == 0);
    wq->want_write--;
}

// wakeup the writer threads
// expects: the mutex is locked

static void writequeue_wakeup_write(WRITEQUEUE wq) {
    if (wq->want_write) {
        int r = toku_pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
    }
}
   
