#ifndef SQL_TMP_TABLE_INCLUDED
#define SQL_TMP_TABLE_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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
  @file

  @brief
  Temporary table handling functions.
*/

#include "sql_list.h"
#include "sql_class.h"
#include "my_base.h"
#include "field.h"
#include "item.h"

class SJ_TMP_TABLE;
struct TABLE;
class THD;
class TMP_TABLE_PARAM;
typedef struct st_order ORDER;
typedef struct st_columndef MI_COLUMNDEF;

TABLE *
create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 ulonglong select_options, ha_rows rows_limit,
		 const char *table_alias);
/**
  General routine to change field->ptr of a NULL-terminated array of Field
  objects. Useful when needed to call val_int, val_str or similar and the
  field data is not in table->record[0] but in some other structure.
  set_key_field_ptr changes all fields of an index using a key_info object.
  All methods presume that there is at least one field to change.
*/

TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list);
bool create_myisam_from_heap(THD *thd, TABLE *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
			     int error, bool ignore_last_dup,
                             bool *is_duplicate);
void free_tmp_table(THD *thd, TABLE *entry);
TABLE *create_duplicate_weedout_tmp_table(THD *thd, 
                                          uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl);
bool instantiate_tmp_table(TABLE *table, KEY *keyinfo,
                           MI_COLUMNDEF *start_recinfo,
                           MI_COLUMNDEF **recinfo,
                           ulonglong options, my_bool big_tables,
                           Opt_trace_context *trace);
Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
                        Item ***copy_func, Field **from_field,
                        Field **default_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields,
                        bool make_copy_field);
Field* create_tmp_field_from_field(THD *thd, Field* org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item);


#endif /* SQL_TMP_TABLE_INCLUDED */

