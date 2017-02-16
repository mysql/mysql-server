/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef FILESORT_INCLUDED
#define FILESORT_INCLUDED

class SQL_SELECT;

#include "my_global.h"                          /* uint, uchar */
#include "my_base.h"                            /* ha_rows */

class SQL_SELECT;
class THD;
struct TABLE;
typedef struct st_sort_field SORT_FIELD;

ha_rows filesort(THD *thd, TABLE *table, st_sort_field *sortorder,
                 uint s_length, SQL_SELECT *select,
                 ha_rows max_rows, bool sort_positions,
                 ha_rows *examined_rows);
void filesort_free_buffers(TABLE *table, bool full);
double get_merge_many_buffs_cost(uint *buffer, uint last_n_elems,
                                 int elem_size);
void change_double_for_sort(double nr,uchar *to);

#endif /* FILESORT_INCLUDED */
