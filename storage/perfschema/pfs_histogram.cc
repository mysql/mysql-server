/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_histogram.cc
  Miscellaneous global dependencies (implementation).
*/

#include "storage/perfschema/pfs_histogram.h"

/**
  Histogram base bucket timer, in picoseconds.
  Currently defined as 10 micro second.
*/
#define BUCKET_BASE_TIMER (10 * 1000 * 1000)

/**
  Bucket factor.
  histogram_timer[i+1] = BUCKET_BASE_FACTOR * histogram_timer[i]
  The value is chosen so that BUCKET_BASE_FACTOR ^ 50 = 10,
  which corresponds to a 4.7 percent increase for each bucket,
  or a power of 10 increase for 50 buckets.
*/
#define BUCKET_BASE_FACTOR 1.0471285480508996

/**
  Timer values used in histograms.
  Timer values are expressed in picoseconds.

  timer[0] = 0
  timer[1] = BUCKET_BASE_TIMER

  From then,
  timer[N+1] = BUCKET_BASE_FACTOR * timer[N]

  The last timer is set to infinity.
*/
PFS_histogram_timers g_histogram_pico_timers;

void PFS_histogram::reset() {
  ulong bucket_index;

  for (bucket_index = 0; bucket_index < NUMBER_OF_BUCKETS; bucket_index++) {
    m_bucket[bucket_index] = 0;
  }
}

void PFS_histogram_timers::init() {
  ulong bucket_index;
  double current_bucket_timer = BUCKET_BASE_TIMER;

  m_bucket_timer[0] = 0;

  for (bucket_index = 1; bucket_index < NUMBER_OF_BUCKETS; bucket_index++) {
    m_bucket_timer[bucket_index] = current_bucket_timer;
    current_bucket_timer *= BUCKET_BASE_FACTOR;
  }

  m_bucket_timer[NUMBER_OF_BUCKETS] = UINT64_MAX;
}
