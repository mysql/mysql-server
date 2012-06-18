/* Copyright (C) 2009-2010 Facebook, Inc.  All Rights Reserved.

   Dual licensed under BSD license and GPLv2.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
  
   THIS SOFTWARE IS PROVIDED BY FACEBOOK, INC. ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
   EVENT SHALL FACEBOOK, INC. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   this program; if not, write to the Free Software Foundation, Inc., 59 Temple
   Place, Suite 330, Boston, MA  02111-1307  USA */

/* Various performance statistics utilities.
 *
 *
 * Fast timers derive from the CPU time stamp counter provide a very low
 * overhead timer for performance metrics.  The values returned may be very
 * large (greater than the maximum integral value that can be stored accurately
 * in a double) so you should only convert elapsed times to doubles.
 *
 *
 * Table stats are used to track per-table compression statistics.  The
 * data is stored in a global hash table of TABLE_STATS objects.  This hash
 * table is protected by a mutex.
 */

#ifndef _my_perf_h
#define _my_perf_h

#include "my_global.h"

struct TABLE;

/** Compression statistics for a fil_space */
struct comp_stat_struct {
  /** Size of the compressed data on the page */
  int page_size;
  /** Number of page compressions */
  ulonglong compressed;
  /** Number of successful page compressions */
  ulonglong compressed_ok;
  /** Number of compressions in primary index */
  ulonglong compressed_primary;
  /** Number of successful compressions in primary index */
  ulonglong compressed_primary_ok;
  /** Number of page decompressions */
  ulonglong decompressed;
  /** Duration of page compressions in microseconds */
  ulonglong compressed_usec;
	/** Duration of succesful page compressions in microseconds */
	ulonglong compressed_ok_usec;
  /** Duration of page decompressions in microseconds */
  ulonglong decompressed_usec;
  /** Duration of primary index page compressions in microseconds */
  ulonglong compressed_primary_usec;
  /** Duration of successful primary index page compressions in microseconds */
  ulonglong compressed_primary_ok_usec;
};

/** Compression statistics */
typedef struct comp_stat_struct comp_stat_t;

/* Type used for low-overhead timers */
typedef ulonglong my_fast_timer_t;

/* Per-table operation and IO statistics */
struct st_table_stats;
struct st_table;
struct handlerton;

/* Fetches table stats for a given table */
struct st_table_stats* get_table_stats(TABLE *table);

/* The inverse of the CPU frequency used to convert the time stamp counter
   to seconds. */
extern double my_tsc_scale;

/* Reads the time stamp counter on an Intel processor */
static __inline__ ulonglong rdtsc(void)
{
  ulonglong tsc;

#if defined(__GNUC__) && defined(__i386__)
  __asm__ __volatile__ ("rdtsc" : "=A" (tsc));
#elif defined(__GNUC__) && defined(__x86_64__)
  uint high, low;
  __asm__ __volatile__ ("rdtsc" : "=a"(low), "=d"(high));
  tsc = (((ulonglong)high)<<32) | low;
#else
  tsc = 0;
  assert(! "Aborted: rdtsc unimplemented for this configuration.");
#endif

  return tsc;
}

/* Initialize the fast timer at startup Counts timer ticks for given seconds.
 */
static __inline__ void my_init_fast_timer(int seconds)
{
  longlong delta;
  int retry;
	retry = 3;

  do {
    ulonglong before = rdtsc();
    sleep(seconds);
    delta = rdtsc() - before;
  } while (retry-- && delta < 0);

  my_tsc_scale = (delta > 0) ? (double)seconds / delta : 0;
}

/* Returns a fast timer suitable for performance measurements. */
static __inline__ void my_get_fast_timer(my_fast_timer_t* timer)
{
  *timer = rdtsc();
}

/* Returns the difference between stop and start in seconds. Returns 0
   when stop < start. */
static __inline__ double my_fast_timer_diff(my_fast_timer_t const *start,
                                            my_fast_timer_t const *stop)
{
  ulonglong delta;
  if (*stop <= *start)
    return 0;

  delta = *stop - *start;

  return my_tsc_scale * delta;
}

/* Returns the difference between now and the time from 'in' in seconds.  Also
   optionally returns current fast timer in 'out'.  It is safe to pass the same
   struct for 'in' and 'out'. */
static __inline__ double my_fast_timer_diff_now(my_fast_timer_t const *in,
                                                my_fast_timer_t *out)
{
  my_fast_timer_t now = rdtsc();

  if (out) {
    *out = now;
  }

  return my_fast_timer_diff(in, &now);
}

/* Returns -1, 1, or 0 if *x is less than, greater than, or equal to *y */
static __inline__ int my_fast_timer_cmp(my_fast_timer_t const *x,
                                        my_fast_timer_t const *y)
{
  if (*x < *y)
    return -1;
  if (*x > *y)
    return 1;
  else
    return 0;
}

/* Sets a fast timer to an invalid value */
static __inline__ void my_fast_timer_invalidate(my_fast_timer_t *timer)
{
  *timer = 0;
}

/* Sets a fast timer to the value of another */
static __inline__ void my_fast_timer_set(my_fast_timer_t *dest,
                                         my_fast_timer_t const *src)
{
  *dest = *src;
}

/* Returns true if the timer is valid */
static __inline__ int my_fast_timer_is_valid(my_fast_timer_t const *timer)
{
  return (int)(*timer);
}

/* Return the fast timer scale factor.  If this value is 0 fast timers are
   not initialized. */
static __inline__ double my_fast_timer_get_scale()
{
  return my_tsc_scale;
}

#endif /* _my_perf_h */
