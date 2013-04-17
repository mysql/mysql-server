/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "$Id:$"

#include <toku_pthread.h>
#include <toku_time.h>
#include "brttypes.h"


// Specification:
// A minicron is a miniature cron job for executing a job periodically inside a pthread.
// To create a minicron,
//   1) allocate a "struct minicron" somewhere.
//      Rationale:  This struct can be stored inside another struct (such as the cachetable), avoiding a malloc/free pair.
//   2) call toku_minicron_setup, specifying a period (in seconds), a function, and some arguments.
//      If the period is positive then the function is called periodically (with the period specified)
//      Note: The period is measured from when the previous call to f finishes to when the new call starts.
//            Thus, if the period is 5 minutes, and it takes 8 minutes to run f, then the actual periodicity is 13 minutes.
//      Rationale:  If f always takes longer than f to run, then it will get "behind".  This module makes getting behind explicit.
//   3) When finished, call toku_minicron_shutdown.
//   4) If you want to change the period, then call toku_minicron_change_period.    The time since f finished is applied to the new period
//      and the call is rescheduled.  (If the time since f finished is more than the new period, then f is called immediately).

struct minicron {
    toku_pthread_t thread;
    toku_timespec_t time_of_last_call_to_f;
    toku_pthread_mutex_t mutex;
    toku_pthread_cond_t  condvar;
    int (*f)(void*);
    void *arg;
    u_int32_t period_in_seconds;
    BOOL      do_shutdown;
};

int toku_minicron_setup (struct minicron *s, u_int32_t period_in_seconds, int(*f)(void *), void *arg);
int toku_minicron_change_period(struct minicron *p, u_int32_t new_period);
u_int32_t toku_minicron_get_period(struct minicron *p);
int toku_minicron_shutdown(struct minicron *p);
BOOL toku_minicron_has_been_shutdown(struct minicron *p);
