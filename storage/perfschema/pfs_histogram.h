/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_HISTOGRAM_H
#define PFS_HISTOGRAM_H

#include <atomic>
#include "my_compiler.h"
#include "my_inttypes.h"

/**
  @file storage/perfschema/pfs_histogram.h
*/

/** Number of buckets used in histograms. */
#define NUMBER_OF_BUCKETS 450

struct PFS_histogram
{
public:
  void reset();

  void
  increment_bucket(uint bucket_index)
  {
    m_bucket[bucket_index]++;
  }

  ulonglong
  read_bucket(uint bucket_index)
  {
    return m_bucket[bucket_index];
  }

private:
  std::atomic<ulonglong> m_bucket[NUMBER_OF_BUCKETS];
};

struct PFS_histogram_timers
{
  ulonglong m_bucket_timer[NUMBER_OF_BUCKETS + 1];

  void init();
};

extern struct PFS_histogram_timers g_histogram_pico_timers;

#endif
