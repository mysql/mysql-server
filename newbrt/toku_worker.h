/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef _TOKU_WORKER_H
#define _TOKU_WORKER_H

// initialize the work queue and worker threads

void toku_init_workers(WORKQUEUE wq, THREADPOOL *tpptr);

// destroy the work queue and worker threads

void toku_destroy_workers(WORKQUEUE wq, THREADPOOL *tpptr);

// this is the thread function for the worker threads in the worker thread
// pool. the arg is a pointer to the work queue that feeds work to the
// workers.

void *toku_worker(void *arg);

#endif

