/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/pfs_timer.cc
  Performance schema timers (implementation).
*/

#include "storage/perfschema/pfs_timer.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <sys/types.h>

#include "my_rdtsc.h"
#include "mysqld_error.h"
#include "sql/log.h" /* log_errlog */

MY_TIMER_INFO pfs_timer_info;

static ulonglong cycle_v0;
static ulonglong nanosec_v0;
static ulonglong microsec_v0;
static ulonglong millisec_v0;

static ulong cycle_to_pico;    /* 1000 at 1 GHz, 333 at 3GHz, 250 at 4GHz */
static ulong nanosec_to_pico;  /* In theory, 1 000 */
static ulong microsec_to_pico; /* In theory, 1 000 000 */
static ulong millisec_to_pico; /* In theory, 1 000 000 000, fits in uint32 */

/* Indexed by enum enum_timer_name */
static struct time_normalizer
    to_pico_data[FIRST_TIMER_NAME + COUNT_TIMER_NAME] = {
        {0, 0, {0}}, /* pico (identity) */
        {0, 0, {0}}, /* cycle */
        {0, 0, {0}}, /* nanosec */
        {0, 0, {0}}, /* microsec */
        {0, 0, {0}}, /* millisec */
};

void init_timers(void) {
  double pico_frequency = 1.0e12;

  my_timer_init(&pfs_timer_info);

  cycle_v0 = my_timer_cycles();
  nanosec_v0 = my_timer_nanoseconds();
  microsec_v0 = my_timer_microseconds();
  millisec_v0 = my_timer_milliseconds();

  if (pfs_timer_info.cycles.frequency > 0) {
    cycle_to_pico =
        lrint(pico_frequency / (double)pfs_timer_info.cycles.frequency);
  } else {
    cycle_to_pico = 0;
  }

  if (pfs_timer_info.nanoseconds.frequency > 0) {
    nanosec_to_pico =
        lrint(pico_frequency / (double)pfs_timer_info.nanoseconds.frequency);
  } else {
    nanosec_to_pico = 0;
  }

  if (pfs_timer_info.microseconds.frequency > 0) {
    microsec_to_pico =
        lrint(pico_frequency / (double)pfs_timer_info.microseconds.frequency);
  } else {
    microsec_to_pico = 0;
  }

  if (pfs_timer_info.milliseconds.frequency > 0) {
    millisec_to_pico =
        lrint(pico_frequency / (double)pfs_timer_info.milliseconds.frequency);
  } else {
    millisec_to_pico = 0;
  }

  to_pico_data[TIMER_NAME_CYCLE].m_v0 = cycle_v0;
  to_pico_data[TIMER_NAME_CYCLE].m_factor = cycle_to_pico;

  to_pico_data[TIMER_NAME_NANOSEC].m_v0 = nanosec_v0;
  to_pico_data[TIMER_NAME_NANOSEC].m_factor = nanosec_to_pico;

  to_pico_data[TIMER_NAME_MICROSEC].m_v0 = microsec_v0;
  to_pico_data[TIMER_NAME_MICROSEC].m_factor = microsec_to_pico;

  to_pico_data[TIMER_NAME_MILLISEC].m_v0 = millisec_v0;
  to_pico_data[TIMER_NAME_MILLISEC].m_factor = millisec_to_pico;

  if (cycle_to_pico == 0) {
    log_errlog(WARNING_LEVEL, ER_CYCLE_TIMER_IS_NOT_AVAILABLE);
  }

#ifdef HAVE_NANOSEC_TIMER
  if (nanosec_to_pico == 0) {
    log_errlog(WARNING_LEVEL, ER_NANOSECOND_TIMER_IS_NOT_AVAILABLE);
  }
#else
  if (microsec_to_pico == 0) {
    log_errlog(WARNING_LEVEL, ER_MICROSECOND_TIMER_IS_NOT_AVAILABLE);
  }
#endif

  /* Initialize histograms bucket timers. */

  uint timer_index;

  for (timer_index = FIRST_TIMER_NAME; timer_index <= LAST_TIMER_NAME;
       timer_index++) {
    time_normalizer *normalizer = &to_pico_data[timer_index];
    ulonglong to_pico = normalizer->m_factor;
    ulonglong bucket_index;

    if (to_pico != 0) {
      for (bucket_index = 0; bucket_index < NUMBER_OF_BUCKETS; bucket_index++) {
        normalizer->m_bucket_timer[bucket_index] =
            g_histogram_pico_timers.m_bucket_timer[bucket_index] / to_pico;
      }
    } else {
      for (bucket_index = 0; bucket_index < NUMBER_OF_BUCKETS; bucket_index++) {
        normalizer->m_bucket_timer[bucket_index] = 0;
      }
    }

    normalizer->m_bucket_timer[NUMBER_OF_BUCKETS] = UINT64_MAX;
  }
}

time_normalizer *time_normalizer::get_idle() {
  return &to_pico_data[USED_TIMER_NAME];
}

time_normalizer *time_normalizer::get_wait() {
  return &to_pico_data[TIMER_NAME_CYCLE];
}

time_normalizer *time_normalizer::get_stage() {
  return &to_pico_data[USED_TIMER_NAME];
}

time_normalizer *time_normalizer::get_statement() {
  return &to_pico_data[USED_TIMER_NAME];
}

time_normalizer *time_normalizer::get_transaction() {
  return &to_pico_data[USED_TIMER_NAME];
}

void time_normalizer::to_pico(ulonglong start, ulonglong end,
                              ulonglong *pico_start, ulonglong *pico_end,
                              ulonglong *pico_wait) {
  if (start == 0) {
    *pico_start = 0;
    *pico_end = 0;
    *pico_wait = 0;
  } else {
    *pico_start = (start - m_v0) * m_factor;
    if (end == 0) {
      *pico_end = 0;
      *pico_wait = 0;
    } else {
      *pico_end = (end - m_v0) * m_factor;
      *pico_wait = (end - start) * m_factor;
    }
  }
}

ulong time_normalizer::bucket_index(ulonglong t) {
  ulong low = 0;
  ulong mid;
  ulong high = NUMBER_OF_BUCKETS;

  assert(m_bucket_timer[low] <= t);
  assert(t <= m_bucket_timer[high]);

  do {
    mid = (low + high) / 2;
    assert(low < mid);
    assert(mid < high);
    if (t < m_bucket_timer[mid]) {
      high = mid;
    } else {
      low = mid;
    }
  } while (low + 1 < high);

  assert(m_bucket_timer[low] <= t);
  assert((t < m_bucket_timer[high]) || (high == NUMBER_OF_BUCKETS));

  return low;
}
