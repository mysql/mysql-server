/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

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
    toku_mutex_t lock;
    tokutime_t starttime; // what time was the lock initialized?
    tokutime_t acquired_time; // what time was the lock acquired
};
static struct ydb_big_lock ydb_big_lock;

static inline u_int64_t u64max(u_int64_t a, u_int64_t b) {return a > b ? a : b; }

/* Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */
static volatile YDB_LOCK_STATUS_S ydb_lock_status;

#define STATUS_INIT(k,t,l) { \
	ydb_lock_status.status[k].keyname = #k; \
	ydb_lock_status.status[k].type    = t;  \
	ydb_lock_status.status[k].legend  = "ydb lock: " l; \
    }
static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(YDB_LOCK_TAKEN,               UINT64,   "taken");
    STATUS_INIT(YDB_LOCK_RELEASED,            UINT64,   "released");
    STATUS_INIT(YDB_NUM_WAITERS_NOW,          UINT64,   "num waiters now");
    STATUS_INIT(YDB_MAX_WAITERS,              UINT64,   "max waiters");
    STATUS_INIT(YDB_TOTAL_SLEEP_TIME,         UINT64,   "total sleep time (usec)");
    STATUS_INIT(YDB_MAX_TIME_YDB_LOCK_HELD,   TOKUTIME, "max time held (sec)");
    STATUS_INIT(YDB_TOTAL_TIME_YDB_LOCK_HELD, TOKUTIME, "total time held (sec)");
    STATUS_INIT(YDB_TOTAL_TIME_SINCE_START,   TOKUTIME, "total time since start (sec)");

    ydb_lock_status.initialized = true;
}
#undef STATUS_INIT

void
toku_ydb_lock_get_status(YDB_LOCK_STATUS statp) {
    if (!ydb_lock_status.initialized)
	status_init();
    *statp = ydb_lock_status;
}

#define STATUS_VALUE(x) ydb_lock_status.status[x].value.num

/* End of status section.
 */

void
toku_ydb_lock_init(void) {
    toku_mutex_init(&ydb_big_lock.lock, NULL);
    ydb_big_lock.starttime   = get_tokutime();
    ydb_big_lock.acquired_time = 0;
}

void
toku_ydb_lock_destroy(void) {
    toku_mutex_destroy(&ydb_big_lock.lock);
}

void 
toku_ydb_lock(void) {
    u_int32_t new_num_waiters = __sync_add_and_fetch(&STATUS_VALUE(YDB_NUM_WAITERS_NOW), 1);

    toku_mutex_lock(&ydb_big_lock.lock);

    u_int64_t now = get_tokutime();

    // Update the lock
    ydb_big_lock.acquired_time = now;

    // Update status
    STATUS_VALUE(YDB_LOCK_TAKEN)++;
    if (new_num_waiters > STATUS_VALUE(YDB_MAX_WAITERS)) 
        STATUS_VALUE(YDB_MAX_WAITERS) = new_num_waiters;
    STATUS_VALUE(YDB_TOTAL_TIME_SINCE_START) = now - ydb_big_lock.starttime;
}

static void 
ydb_unlock_internal(unsigned long useconds) {
    STATUS_VALUE(YDB_LOCK_RELEASED)++;

    tokutime_t now = get_tokutime();
    tokutime_t time_held = now - ydb_big_lock.acquired_time;
    STATUS_VALUE(YDB_TOTAL_TIME_YDB_LOCK_HELD) += time_held;
    if (time_held > STATUS_VALUE(YDB_MAX_TIME_YDB_LOCK_HELD))
        STATUS_VALUE(YDB_MAX_TIME_YDB_LOCK_HELD) = time_held;
    STATUS_VALUE(YDB_TOTAL_TIME_SINCE_START) = now - ydb_big_lock.starttime;

    toku_mutex_unlock(&ydb_big_lock.lock);

    int new_num_waiters = __sync_add_and_fetch(&STATUS_VALUE(YDB_NUM_WAITERS_NOW), -1);

    if (new_num_waiters > 0 && useconds > 0) {
        __sync_add_and_fetch(&STATUS_VALUE(YDB_TOTAL_SLEEP_TIME), useconds);
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

#undef STATUS_VALUE
