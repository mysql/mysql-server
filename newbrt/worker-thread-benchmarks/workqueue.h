
typedef struct workqueue *WORKQUEUE;
struct workqueue {
    WORKITEM head, tail;        // head and tail of the linked list of work items
    pthread_cond_t wait_read;   // wait for read
    int want_read;              // number of threads waiting to read
    pthread_cond_t wait_write;  // wait for write
    int want_write;             // number of threads waiting to write
    int ninq;                   // number of work items in the queue
    char closed;                // kicks waiting threads off of the write queue
};

// initialize a workqueue
// expects: the workqueue is not initialized
// effects: the workqueue is set to empty and the condition variable is initialized

static void workqueue_init(WORKQUEUE wq) {
    wq->head = wq->tail = 0;
    int r;
    r = pthread_cond_init(&wq->wait_read, 0); assert(r == 0);
    wq->want_read = 0;
    r = pthread_cond_init(&wq->wait_write, 0); assert(r == 0);
    wq->want_write = 0;
    wq->ninq = 0;
    wq->closed = 0;
}

// destroy a workqueue
// expects: the workqueue must be initialized and empty

static void workqueue_destroy(WORKQUEUE wq) {
    assert(wq->head == 0 && wq->tail == 0);
    int r;
    r = pthread_cond_destroy(&wq->wait_read); assert(r == 0);
    r = pthread_cond_destroy(&wq->wait_write); assert(r == 0);
}

// close the workqueue
// effects: signal any threads blocked in the workqueue

static void workqueue_set_closed(WORKQUEUE wq) {
    wq->closed = 1;
    int r;
    r = pthread_cond_broadcast(&wq->wait_read); assert(r == 0);
    r = pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
}

// determine whether or not the write queue is empty
// return: 1 if the write queue is empty, otherwise 0

static int workqueue_empty(WORKQUEUE wq) {
    return wq->head == 0;
}

// put a work item at the tail of the write queue
// expects: the mutex is locked
// effects: append the workitem to the end of the write queue and signal
// any readers

static void workqueue_enq(WORKQUEUE wq, WORKITEM workitem) {
    workitem->next_wq = 0;
    if (wq->tail)
        wq->tail->next_wq = workitem;
    else
        wq->head = workitem;
    wq->tail = workitem;
    wq->ninq++;
    if (wq->want_read) {
        int r = pthread_cond_signal(&wq->wait_read); assert(r == 0);
    }
}

// get a workitem from the head of the write queue
// expects: the mutex is locked
// effects: wait until the workqueue is not empty, remove the first workitem from the
// write queue and return it
// returns: 0 if success, otherwise an error 

static int workqueue_deq(WORKQUEUE wq, pthread_mutex_t *mutex, WORKITEM *workitemptr) {
    while (workqueue_empty(wq)) {
        if (wq->closed)
            return EINVAL;
        wq->want_read++;
        int r = pthread_cond_wait(&wq->wait_read, mutex); assert(r == 0);
        wq->want_read--;
    }
    WORKITEM workitem = wq->head;
    wq->head = workitem->next_wq;
    if (wq->head == 0)
        wq->tail = 0;
    wq->ninq--;
    workitem->next_wq = 0;
    *workitemptr = workitem;
    return 0;
}

#if 0

// suspend the writer thread
// expects: the mutex is locked

static void workqueue_wait_write(WORKQUEUE wq, pthread_mutex_t *mutex) {
    wq->want_write++;
    int r = pthread_cond_wait(&wq->wait_write, mutex); assert(r == 0);
    wq->want_write--;
}

// wakeup the writer threads
// expects: the mutex is locked

static void workqueue_wakeup_write(WORKQUEUE wq) {
    if (wq->want_write) {
        int r = pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
    }
}
        
#endif
