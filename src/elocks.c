/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
  \file elocks.c
  \brief Ephemeral locks

   The ydb big lock serializes access to the tokudb
   every call (including methods) into the tokudb library gets the lock 
   no internal function should invoke a method through an object */

#include <toku_portability.h>
#include "ydb-internal.h"
#include <string.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <sys/types.h>

struct ydb_big_lock {
    toku_pthread_mutex_t lock;
    u_int64_t starttime; // what time (in microseconds according to gettimeofday()) was the lock initialized?
    u_int64_t acquired_time;
};
static struct ydb_big_lock ydb_big_lock;

// status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.
static SCHEDULE_STATUS_S status;

static inline u_int64_t u64max(u_int64_t a, u_int64_t b) {return a > b ? a : b; }

static void 
init_status(void) {
    status.ydb_lock_ctr = 0;
    status.num_waiters_now = 0;
    status.max_waiters = 0;
    status.total_sleep_time = 0;
    status.max_time_ydb_lock_held = 0;
    status.total_time_ydb_lock_held = 0;
    status.total_time_since_start = 0;
}

void 
toku_ydb_lock_get_status(SCHEDULE_STATUS statp) {
    *statp = status;
}

static u_int64_t get_current_time_in_microseconds (void) {
    // On recent linux, gettimeofday() runs extremely fast (in userspace only), taking only a few nanoseconds.
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_usec + 1000000 * tv.tv_sec;
}

int 
toku_ydb_lock_init(void) {
    int r;
    r = toku_pthread_mutex_init(&ydb_big_lock.lock, NULL); resource_assert_zero(r);
    ydb_big_lock.starttime   = get_current_time_in_microseconds();
    ydb_big_lock.acquired_time = 0;
    init_status();
    return r;
}

int 
toku_ydb_lock_destroy(void) {
    int r;
    r = toku_pthread_mutex_destroy(&ydb_big_lock.lock); resource_assert_zero(r);
    return r;
}

void 
toku_ydb_lock(void) {
    u_int32_t new_num_waiters = __sync_add_and_fetch(&status.num_waiters_now, 1);

    int r = toku_pthread_mutex_lock(&ydb_big_lock.lock);   resource_assert_zero(r);

    u_int64_t now = get_current_time_in_microseconds();

    // Update the lock
    ydb_big_lock.acquired_time = now;

    // Update status
    status.ydb_lock_ctr++;
    if (new_num_waiters > status.max_waiters) status.max_waiters = new_num_waiters;
    status.total_time_since_start = now - ydb_big_lock.starttime;

    invariant((status.ydb_lock_ctr & 0x01) == 1);
}

static void 
ydb_unlock_internal(unsigned long useconds) {
    status.ydb_lock_ctr++;
    invariant((status.ydb_lock_ctr & 0x01) == 0);

    u_int64_t now = get_current_time_in_microseconds();
    u_int64_t time_held = now - ydb_big_lock.acquired_time;
    status.total_time_ydb_lock_held += time_held;
    if (time_held > status.max_time_ydb_lock_held) status.max_time_ydb_lock_held = time_held;
    status.total_time_since_start = now - ydb_big_lock.starttime;

    int r = toku_pthread_mutex_unlock(&ydb_big_lock.lock); resource_assert_zero(r);

    int new_num_waiters = __sync_add_and_fetch(&status.num_waiters_now, -1);

    if (new_num_waiters > 0 && useconds > 0) {
	__sync_add_and_fetch(&status.total_sleep_time, useconds);
        usleep(useconds);
    }
}

void 
toku_ydb_unlock(void) {
    ydb_unlock_internal(0);
}

void 
toku_ydb_unlock_and_yield(unsigned long useconds) {
    ydb_unlock_internal(useconds);
}
