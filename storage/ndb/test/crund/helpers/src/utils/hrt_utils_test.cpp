/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_utils_test.c
 *
 */

#include <assert.h>
#include <stdio.h>

#include "hrt_utils.h"

/*
 * High-Resolution Time Utilities -- Test
 */

static void
hrt_rtprint(const hrt_rtstamp* x)
{
#if (HRT_REALTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    printf("time.tv_sec  = %ld\n", (long)x->time.tv_sec);
    printf("time.tv_nsec = %ld\n", (long)x->time.tv_nsec);
#elif (HRT_REALTIME_METHOD==HRT_USE_GETTIMEOFDAY)
    printf("time.tv_sec  = %ld\n", (long)x->time.tv_sec);
    printf("time.tv_usec = %ld\n", (long)x->time.tv_usec);
#elif (HRT_REALTIME_METHOD==HRT_USE_TIMES)
    printf("time = %ld\n", (long)x->time);
#elif (HRT_REALTIME_METHOD==HRT_USE_ANSI_TIME)
    printf("time = %ld\n", (long)x->time);
#endif
}

static void
hrt_ctprint(const hrt_ctstamp* x)
{
#if (HRT_CPUTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    printf("time.tv_sec  = %ld\n", (long)x->time.tv_sec);
    printf("time.tv_nsec = %ld\n", (long)x->time.tv_nsec);
#elif (HRT_CPUTIME_METHOD==HRT_USE_GETRUSAGE)
    printf("time.ru_utime.tv_sec  = %ld\n", (long)x->time.ru_utime.tv_sec);
    printf("time.ru_utime.tv_usec = %ld\n", (long)x->time.ru_utime.tv_usec);
    printf("time.ru_stime.tv_sec  = %ld\n", (long)x->time.ru_stime.tv_sec);
    printf("time.ru_stime.tv_usec = %ld\n", (long)x->time.ru_stime.tv_usec);
#elif (HRT_CPUTIME_METHOD==HRT_USE_TIMES)
    printf("time.tms_utime = %ld\n", (long)x->time.tms_utime);
    printf("time.tms_stime = %ld\n", (long)x->time.tms_stime);
#elif (HRT_CPUTIME_METHOD==HRT_USE_ANSI_CLOCK)
    printf("time = %ld\n", (long)x->time);
#endif
}

static void
hrt_tprint(const hrt_tstamp* x)
{
    hrt_rtprint(&x->rtstamp);
    hrt_ctprint(&x->ctstamp);
}

static void
do_something(void)
{
    const unsigned long loop = 1000000000L;
    unsigned long i;
    static volatile unsigned long dummy;
    for (i = 0; i < loop; i++)
	dummy = i;
}

int
main(int argc, const char* argv[])
{
    printf("--> main()\n");
    do_something();

    hrt_tstamp t0, t1;
    hrt_tnull(&t0);
    hrt_tnull(&t1);

    printf("\nmarking time...\n");
    int r;
    if ((r = hrt_tnow(&t0)) != 0) {
        fprintf(stderr, "error: hrt_now(&t0) returned %d\n", r);
    }
    hrt_tprint(&t0);

    do_something();

    printf("\nmarking time...\n");
    if ((r = hrt_tnow(&t1)) != 0) {
        fprintf(stderr, "error: hrt_now(&t1) returned %d\n", r);
    }
    hrt_tprint(&t1);

    printf("\namount of times:\n");
    double rtmicros = hrt_rtmicros(&t1.rtstamp, &t0.rtstamp);
    double ctmicros = hrt_ctmicros(&t1.ctstamp, &t0.ctstamp);
    printf("real   = %.3f us\n", rtmicros);
    printf("cpu    = %.3f us\n", ctmicros);
    
    printf("\n<-- main()\n");
    return 0;
}
