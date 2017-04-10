#ifndef SQL_TMP_TABLE_INCLUDED
#define SQL_TMP_TABLE_INCLUDED

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
  @file

  @brief
  Temporary table handling functions.
*/

#include <sys/types.h>

#include "item.h"           // Item
#include "mem_root_array.h"
#include "my_base.h"        // ha_rows
#include "my_inttypes.h"
#include "table.h"

class Create_field;
class Field;
class Opt_trace_context;
class SJ_TMP_TABLE;
class THD;
class Temp_table_param;
template <class T> class List;

typedef struct st_columndef MI_COLUMNDEF;
class KEY;

typedef struct st_order ORDER;


/*
   For global system variable internal_tmp_disk_storage_engine
 */
enum enum_internal_tmp_disk_storage_engine { TMP_TABLE_MYISAM, TMP_TABLE_INNODB };

TABLE *
create_tmp_table(THD *thd, Temp_table_param *param, List<Item> &fields,
                 ORDER *group, bool distinct, bool save_sum_fields,
                 ulonglong select_options, ha_rows rows_limit,
                 const char *table_alias);
bool open_tmp_table(TABLE *table);
TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list);
bool create_ondisk_from_heap(THD *thd, TABLE *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
                             int error, bool ignore_last_dup,
                             bool *is_duplicate);
void free_tmp_table(THD *thd, TABLE *entry);
TABLE *create_duplicate_weedout_tmp_table(THD *thd, 
                                          uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl);
bool instantiate_tmp_table(THD *thd, TABLE *table, KEY *keyinfo,
                           MI_COLUMNDEF *start_recinfo,
                           MI_COLUMNDEF **recinfo,
                           ulonglong options, bool big_tables);
Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
                        Mem_root_array<Item *> *copy_func, Field **from_field,
                        Field **default_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields,
                        bool make_copy_field);
Field* create_tmp_field_from_field(THD *thd, Field* org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item);

void get_max_key_and_part_length(uint *max_key_length,
                                 uint *max_key_part_length,
                                 uint *max_key_parts);
void init_cache_tmp_engine_properties();
bool reposition_innodb_cursor(TABLE *table, ha_rows row_num);
#endif /* SQL_TMP_TABLE_INCLUDED */

