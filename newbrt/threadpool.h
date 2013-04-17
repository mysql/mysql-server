/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_THREADPOOL_H
#define TOKU_THREADPOOL_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "c_dialects.h"

C_BEGIN

// A toku_thread is toku_pthread that can be cached.
struct toku_thread;

// Run a function f on a thread
// This function setups up the thread to run function f with argument arg and then wakes up
// the thread to run it.
void toku_thread_run(struct toku_thread *thread, void *(*f)(void *arg), void *arg);

// A toku_thread_pool is a pool of toku_threads.  These threads can be allocated from the pool
// and can run an arbitrary function.
struct toku_thread_pool;

typedef struct toku_thread_pool *THREADPOOL;

// Create a new threadpool
// Effects: a new threadpool is allocated and initialized. the number of threads in the threadpool is limited to max_threads.  
// If max_threads == 0 then there is no limit on the number of threads in the pool.
// Initially, there are no threads in the pool. Threads are allocated by the _get or _run functions.
// Returns: if there are no errors, the threadpool is set and zero is returned.  Otherwise, an error number is returned.
int toku_thread_pool_create(struct toku_thread_pool **threadpoolptr, int max_threads);

// Destroy a threadpool
// Effects: the calling thread joins with all of the threads in the threadpool.
// Effects: the threadpool memory is freed.
// Returns: the threadpool is set to null.
void toku_thread_pool_destroy(struct toku_thread_pool **threadpoolptr);

// Get the current number of threads in the thread pool
int toku_thread_pool_get_current_threads(struct toku_thread_pool *pool);

// Get one or more threads from the thread pool
// dowait indicates whether or not the caller blocks waiting for threads to free up
// nthreads on input determines the number of threads that are wanted
// nthreads on output indicates the number of threads that were allocated
// toku_thread_return on input supplies an array of thread pointers (all NULL).  This function returns the threads
// that were allocated in the array.
int toku_thread_pool_get(struct toku_thread_pool *pool, int dowait, int *nthreads, struct toku_thread **toku_thread_return);

// Run a function f on one or more threads allocated from the thread pool
int toku_thread_pool_run(struct toku_thread_pool *pool, int dowait, int *nthreads, void *(*f)(void *arg), void *arg);

// Print the state of the thread pool
void toku_thread_pool_print(struct toku_thread_pool *pool, FILE *out);

C_END


#endif
