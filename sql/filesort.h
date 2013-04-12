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
#include "sql_list.h"                           /* Sql_alloc */
class SQL_SELECT;
class THD;
struct TABLE;
typedef struct st_sort_field SORT_FIELD;
typedef struct st_order ORDER;

/**
  Sorting related info.
  To be extended by another WL to include complete filesort implementation.
*/
class Filesort: public Sql_alloc
{
public:
  /** List of expressions to order the table by */
  ORDER *order;
  /** Number of records to return */
  ha_rows limit;
  /** ORDER BY list with some precalculated info for filesort */
  SORT_FIELD *sortorder;
  /** select to use for getting records */
  SQL_SELECT *select;
  /** TRUE <=> free select on destruction */
  bool own_select;
  /** true means we are using Priority Queue for order by with limit. */
  bool using_pq;

  Filesort(ORDER *order_arg, ha_rows limit_arg, SQL_SELECT *select_arg):
    order(order_arg),
    limit(limit_arg),
    sortorder(NULL),
    select(select_arg),
    own_select(false), 
    using_pq(false)
  {
    DBUG_ASSERT(order);
  };

  ~Filesort() { cleanup(); }
  /* Prepare ORDER BY list for sorting. */
  uint make_sortorder();

private:
  void cleanup();
};

ha_rows filesort(THD *thd, TABLE *table, Filesort *fsort, bool sort_positions,
                 ha_rows *examined_rows, ha_rows *found_rows);
void filesort_free_buffers(TABLE *table, bool full);
void change_double_for_sort(double nr,uchar *to);

class Sort_param;
/// Declared here so we can unit test it.
void make_sortkey(Sort_param *param, uchar *to, uchar *ref_pos);
/// Declared here so we can unit test it.
uint sortlength(THD *thd, SORT_FIELD *sortorder, uint s_length,
                bool *multi_byte_charset);

#endif /* FILESORT_INCLUDED */
