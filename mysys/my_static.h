#ifndef MYSYS_MY_STATIC_INCLUDED
#define MYSYS_MY_STATIC_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_static.h
  Static variables for mysys library. All defined here for easy making of
  a shared library.
*/

#include <sys/types.h>

#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"

extern char curr_dir[FN_REFLEN], home_dir_buff[FN_REFLEN];

extern USED_MEM* my_once_root_block;
extern uint	 my_once_extra;

extern struct st_my_file_info my_file_info_default[MY_NFILE];

extern ulonglong query_performance_frequency, query_performance_offset,
                 query_performance_offset_micros;

#endif /* MYSYS_MY_STATIC_INCLUDED */
