/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <sys/types.h>

#include "my_base.h"                            /* ha_rows */
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql_alloc.h"                          /* Sql_alloc */

class Addon_fields;
class Field;
class QEP_TAB;
class THD;
struct TABLE;
struct st_order;
struct st_sort_field;

enum class Addon_fields_status;

/**
  Sorting related info.
*/
class Filesort: public Sql_alloc
{
public:
  /// The QEP entry for the table to be sorted
  QEP_TAB *const tab;
  /// List of expressions to order the table by
  st_order *order;
  /// Maximum number of rows to return
  ha_rows limit;
  /// ORDER BY list with some precalculated info for filesort
  st_sort_field *sortorder;
  /// true means we are using Priority Queue for order by with limit.
  bool using_pq;
  /// Addon fields descriptor
  Addon_fields *addon_fields;

  Filesort(QEP_TAB *tab_arg, st_order *order_arg, ha_rows limit_arg):
    tab(tab_arg),
    order(order_arg),
    limit(limit_arg),
    sortorder(NULL),
    using_pq(false),
    addon_fields(NULL)
  {
    DBUG_ASSERT(order);
  };

  /* Prepare ORDER BY list for sorting. */
  uint make_sortorder();

  Addon_fields *get_addon_fields(ulong max_length_for_sort_data,
                                 Field **ptabfield,
                                 uint sortlength,
                                 Addon_fields_status *addon_fields_status,
                                 uint *plength,
                                 uint *ppackable_length);
};

bool filesort(THD *thd, Filesort *fsort, bool sort_positions,
              ha_rows *examined_rows, ha_rows *found_rows,
              ha_rows *returned_rows);
void filesort_free_buffers(TABLE *table, bool full);
void change_double_for_sort(double nr,uchar *to);

/// Declared here so we can unit test it.
uint sortlength(THD *thd, st_sort_field *sortorder, uint s_length,
                bool *multi_byte_charset);

#endif /* FILESORT_INCLUDED */
