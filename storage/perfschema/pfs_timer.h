/* Copyright (c) 2008 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

extern enum_timer_name wait_timer;
extern MY_TIMER_INFO pfs_timer_info;

void init_timers();

ulonglong get_timer_value(enum_timer_name timer_name);

#endif

