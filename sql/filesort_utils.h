/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FILESORT_UTILS_INCLUDED

#include "my_base.h"

/*
  Calculate cost of merge sort

    @param num_rows            Total number of rows.
    @param num_keys_per_buffer Number of keys per buffer.
    @param elem_size           Size of each element.

    Calculates cost of merge sort by simulating call to merge_many_buff().

  @retval
    Computed cost of merge sort in disk seeks.

  @note
    Declared here in order to be able to unit test it,
    since library dependencies have not been sorted out yet.

    See also comments get_merge_many_buffs_cost().
*/

#define FILESORT_UTILS_INCLUDED
double get_merge_many_buffs_cost_fast(ha_rows num_rows,
                                      ha_rows num_keys_per_buffer,
                                      uint    elem_size);

#endif  // FILESORT_UTILS_INCLUDED
