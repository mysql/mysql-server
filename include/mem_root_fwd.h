/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
  @file mem_root_fwd.h

  C code before C11 cannot repeat a typedef multiple times, so in order to
  forward-declare MEM_ROOT in headers included by C code, we need to separate
  it into a separate header file that is \#include-guarded.
*/

#ifndef MEM_ROOT_FWD_INCLUDED
#define MEM_ROOT_FWD_INCLUDED

struct st_mem_root;
typedef struct st_mem_root MEM_ROOT;

#endif  // MEM_ROOT_FWD_INCLUDED
