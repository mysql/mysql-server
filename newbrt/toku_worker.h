/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef _TOKU_WORKER_H
#define _TOKU_WORKER_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// initialize the work queue and worker 
void toku_init_workers(WORKQUEUE wq, THREADPOOL *tpptr);

// destroy the work queue and worker 
void toku_destroy_workers(WORKQUEUE wq, THREADPOOL *tpptr);

// this is the thread function for the worker threads in the worker thread
// pool. the arg is a pointer to the work queue that feeds work to the
// workers.
void *toku_worker(void *arg);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

