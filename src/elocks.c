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

#if defined(__linux__) && __linux__
//Enabled for linux
#define YDB_LOCK_MISS_TIME 1
#else
//Enabled for windows
#define YDB_LOCK_MISS_TIME 1
#endif

struct ydb_big_lock {
    toku_pthread_mutex_t lock;
#if YDB_LOCK_MISS_TIME
    int32_t waiters;
    toku_pthread_key_t time_key;
    uint64_t start_miss_count, start_miss_time;
    uint64_t start_fsync_count, start_fsync_time;
    struct toku_list all_ydbtimes;
#endif
};
static struct ydb_big_lock ydb_big_lock;

// status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.
static SCHEDULE_STATUS_S status;

static inline u_int64_t u64max(u_int64_t a, u_int64_t b) {return a > b ? a : b; }

#define MAX_SLEEP 1000000  // 1 second covers the case of a 5 level tree with 30 millisecond read delays and a few waiting threads

#if YDB_LOCK_MISS_TIME

#include "toku_atomic.h"

#define MAXTHELD 250000   // if lock was apparently held longer than 250 msec, then theld is probably invalid (do we still need this?)

struct ydbtime {          // one per thread, 
    struct toku_list all_ydbtimes; // must be first
    uint64_t tacquire;    // valid only when lock is not held, this is the next time the thread may take the lock (0 if no latency required)
    uint64_t theld_prev;  // how long was lock held the previous time this thread held the lock
};

static void
ydbtime_destr(void *a) {
    struct ydbtime *ydbtime = (struct ydbtime *) a;
    int r;
    // printf("%s %p\n", __FUNCTION__, a);
    r = toku_pthread_mutex_lock(&ydb_big_lock.lock); resource_assert_zero(r);
    toku_list_remove(&ydbtime->all_ydbtimes);
    r = toku_pthread_mutex_unlock(&ydb_big_lock.lock); resource_assert_zero(r);
    toku_free(ydbtime);
}

// get a timestamp in units of microseconds
static uint64_t 
get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL);
    resource_assert_zero(r);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}
#endif

static void 
init_status(void) {
    uint64_t cpuhz = 0;
#if YDB_LOCK_MISS_TIME
    int r = toku_os_get_processor_frequency(&cpuhz); resource_assert_zero(r);
#endif
    status.ydb_lock_ctr = 0;
    status.max_possible_sleep = MAX_SLEEP;
    status.processor_freq_mhz = cpuhz / 1000000ULL;
    status.max_requested_sleep = 0;
    status.times_max_sleep_used = 0;
    status.total_sleepers = 0;
    status.total_sleep_time = 0;
    status.max_waiters = 0;
    status.total_waiters = 0;
    status.total_clients = 0;
    status.time_ydb_lock_held_unavailable = 0;
    status.max_time_ydb_lock_held = 0;
    status.total_time_ydb_lock_held = 0;
}

void 
toku_ydb_lock_get_status(SCHEDULE_STATUS statp) {
    *statp = status;
}

int 
toku_ydb_lock_init(void) {
    int r;
    r = toku_pthread_mutex_init(&ydb_big_lock.lock, NULL); resource_assert_zero(r);
#if YDB_LOCK_MISS_TIME
    ydb_big_lock.waiters = 0;
    r = toku_pthread_key_create(&ydb_big_lock.time_key, ydbtime_destr); resource_assert_zero(r);
    toku_list_init(&ydb_big_lock.all_ydbtimes);
#endif
    init_status();
    return r;
}

int 
toku_ydb_lock_destroy(void) {
    int r;
#if YDB_LOCK_MISS_TIME
    r = toku_pthread_key_delete(ydb_big_lock.time_key); resource_assert_zero(r);
    while (!toku_list_empty(&ydb_big_lock.all_ydbtimes)) {
        struct toku_list *list = toku_list_pop(&ydb_big_lock.all_ydbtimes);
        struct ydbtime *ydbtime = toku_list_struct(list, struct ydbtime, all_ydbtimes);
        ydbtime_destr(ydbtime);
    }
#endif
    r = toku_pthread_mutex_destroy(&ydb_big_lock.lock); resource_assert_zero(r);
    return r;
}

void 
toku_ydb_lock(void) {
#if !YDB_LOCK_MISS_TIME
    int r = toku_pthread_mutex_lock(&ydb_big_lock.lock);   resource_assert_zero(r);
#endif

#if YDB_LOCK_MISS_TIME
    int r;
    u_int64_t requested_sleep = 0;
    BOOL new_ydbtime = FALSE;
    struct ydbtime *ydbtime = toku_pthread_getspecific(ydb_big_lock.time_key);
    if (!ydbtime) {          // allocate the per thread timestamp if not yet allocated
        new_ydbtime = TRUE;
        ydbtime = toku_malloc(sizeof (struct ydbtime));
        resource_assert(ydbtime);
        memset(ydbtime, 0, sizeof (struct ydbtime));
        r = toku_pthread_setspecific(ydb_big_lock.time_key, ydbtime);
        resource_assert_zero(r);
	(void) toku_sync_fetch_and_increment_uint64(&status.total_clients);
    }
    if (ydbtime->tacquire) { // delay the thread if the lock acquire time is set and is less than the current time
	if (0) printf("%"PRIu64"\n", ydbtime->tacquire);
        uint64_t t = get_tnow();
        if (t < ydbtime->tacquire) {
            t = ydbtime->tacquire - t;
	    requested_sleep = t;
            // put an upper bound on the sleep time since the timestamps may be crazy due to thread movement between cpu's or cpu frequency changes
            if (t > MAX_SLEEP) {
                t = MAX_SLEEP;
		(void) toku_sync_fetch_and_increment_uint64(&status.times_max_sleep_used);
	    }
#if !TOKU_WINDOWS || TOKU_WINDOWS_HAS_ATOMIC_64 
	    (void) toku_sync_fetch_and_add_uint64(&status.total_sleep_time, t);
#endif
	    (void) toku_sync_fetch_and_increment_uint64(&status.total_sleepers);
            usleep(t);	    
        }
    }
    r = toku_pthread_mutex_trylock(&ydb_big_lock.lock);
    if (r != 0) {           // if we can not get the lock, bump the count of the lock waits, and block on the lock
        assert(r == EBUSY);
        (void) toku_sync_fetch_and_increment_int32(&ydb_big_lock.waiters);
        (void) toku_sync_fetch_and_increment_uint64(&status.total_waiters);
        r = toku_pthread_mutex_lock(&ydb_big_lock.lock);
        resource_assert_zero(r);
        (void) toku_sync_fetch_and_decrement_int32(&ydb_big_lock.waiters);
    }
    status.max_requested_sleep = u64max(status.max_requested_sleep, requested_sleep);
    toku_cachetable_get_miss_times(NULL, &ydb_big_lock.start_miss_count, &ydb_big_lock.start_miss_time);
    toku_get_fsync_sched(&ydb_big_lock.start_fsync_count, &ydb_big_lock.start_fsync_time);
    if (new_ydbtime)
        toku_list_push(&ydb_big_lock.all_ydbtimes, &ydbtime->all_ydbtimes);
#endif

    status.ydb_lock_ctr++;
    invariant((status.ydb_lock_ctr & 0x01) == 1);
}

// low_priority causes the routine to sleep if there are others waiting for the ydb_lock
static void 
ydb_unlock_internal(unsigned long useconds) {
    status.ydb_lock_ctr++;
    invariant((status.ydb_lock_ctr & 0x01) == 0);

#if !YDB_LOCK_MISS_TIME
    int r = toku_pthread_mutex_unlock(&ydb_big_lock.lock); resource_assert_zero(r);
#endif

#if YDB_LOCK_MISS_TIME
    struct ydbtime *ydbtime = toku_pthread_getspecific(ydb_big_lock.time_key);
    resource_assert(ydbtime);

    int r;
    uint64_t theld;
    int waiters = ydb_big_lock.waiters;             // get the number of lock waiters (used to compute the lock acquisition time)
    int do_sleep = 0;
    if (waiters == 0) {
	theld = 0;
    } else {
	uint64_t disk_count, disk_time;             // number of disk reads + fsyncs and the time waiting on disk reads + fsyncs
        toku_cachetable_get_miss_times(NULL, &disk_count, &disk_time);
	disk_count = disk_count - ydb_big_lock.start_miss_count;  // number of cache misses
	disk_time = disk_time - ydb_big_lock.start_miss_time;     // usec spent waiting for disk reads
        uint64_t fsync_count, fsync_time;
        toku_get_fsync_sched(&fsync_count, &fsync_time);          // number of fsync operations 
        disk_count += fsync_count - ydb_big_lock.start_fsync_count; // time waiting for fsyncs to complete
        disk_time += fsync_time - ydb_big_lock.start_fsync_time;
	if (disk_count == 0) {
	    theld = 0; do_sleep = 1;
	} else {
	    theld = disk_time ? disk_time : disk_count * 20000ULL; // if we decide not to compile in miss_time, then backoff to 20 milliseconds per cache miss

	    if (theld < MAXTHELD) {
		status.max_time_ydb_lock_held = u64max(status.max_time_ydb_lock_held, theld);
		ydbtime->theld_prev = theld;
	    } else {                                      // thread appears to have migrated (theld out of range)
		theld = ydbtime->theld_prev;              // if time measurement unavailable, assume same as previous use of ydb lock by this thread
		status.time_ydb_lock_held_unavailable++;
	    }
	    status.max_waiters = u64max(status.max_waiters, waiters);
	    status.total_time_ydb_lock_held += theld;
	}
    }

    r = toku_pthread_mutex_unlock(&ydb_big_lock.lock); resource_assert_zero(r);

    if (do_sleep && useconds > 0)
        usleep(useconds);

    // we use a lower bound of 100 microseconds on the sleep time to avoid system call overhead for short sleeps
    if (waiters == 0 || theld <= 100ULL)
	ydbtime->tacquire = 0;                            // there is no delay on acquiring the lock the next time since there was no lock contention or the lock was not held very long
    else
	ydbtime->tacquire = get_tnow() + theld * waiters; // set the min time from now that the lock can not be reacquired
#endif

}

void 
toku_ydb_unlock(void) {
    ydb_unlock_internal(0);
}

void 
toku_ydb_unlock_and_yield(unsigned long useconds) {
    ydb_unlock_internal(useconds);
}
