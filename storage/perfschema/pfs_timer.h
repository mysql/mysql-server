/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_TIMER_H
#define PFS_TIMER_H

/**
  @file storage/perfschema/pfs_timer.h
  Performance schema timers (declarations).
*/
#include <my_rdtsc.h>
#include "pfs_column_types.h"

struct time_normalizer
{
  static time_normalizer* get(enum_timer_name timer_name);

  ulonglong m_v0;
  ulonglong m_factor;

  inline ulonglong wait_to_pico(ulonglong wait)
  {
    return wait * m_factor;
  }

  inline ulonglong time_to_pico(ulonglong t)
  {
    return (t == 0 ? 0 : (t - m_v0) * m_factor);
  }

  void to_pico(ulonglong start, ulonglong end,
               ulonglong *pico_start, ulonglong *pico_end, ulonglong *pico_wait);
};

extern enum_timer_name wait_timer;
extern MY_TIMER_INFO pfs_timer_info;

void init_timers();

extern "C"
{
  typedef ulonglong (*timer_fct_t)(void);
}

ulonglong get_timer_pico_value(enum_timer_name timer_name);
ulonglong get_timer_raw_value_and_function(enum_timer_name timer_name, timer_fct_t *fct);


#endif

