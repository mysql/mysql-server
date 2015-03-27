/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TEMP_TABLE_PARAM_INCLUDED
#define TEMP_TABLE_PARAM_INCLUDED

#include "my_global.h"
#include "my_base.h"
#include "mem_root_array.h"
#include "sql_alloc.h"
#include "sql_list.h"


struct st_columndef;
struct st_key;
class Copy_field;
class Item;


/**
  Object containing parameters used when creating and using temporary
  tables. Temporary tables created with the help of this object are
  used only internally by the query execution engine.
*/

typedef Mem_root_array<Item*, true> Func_ptr_array;

class Temp_table_param :public Sql_alloc
{
public:
  List<Item> copy_funcs;
  Copy_field *copy_field, *copy_field_end;
  uchar	    *group_buff;
  Func_ptr_array *items_to_copy;             /* Fields in tmp table */
  st_columndef *recinfo,*start_recinfo;

  /**
    After temporary table creation, points to an index on the table
    created depending on the purpose of the table - grouping,
    duplicate elimination, etc. There is at most one such index.
  */
  st_key *keyinfo;
  ha_rows end_write_records;
  /**
    Number of normal fields in the query, including those referred to
    from aggregate functions. Hence, "SELECT `field1`,
    SUM(`field2`) from t1" sets this counter to 2.

    @see count_field_types
  */
  uint	field_count; 
  /**
    Number of fields in the query that have functions. Includes both
    aggregate functions (e.g., SUM) and non-aggregates (e.g., RAND).
    Also counts functions referred to from aggregate functions, i.e.,
    "SELECT SUM(RAND())" sets this counter to 2.

    @see count_field_types
  */
  uint  func_count;  
  /**
    Number of fields in the query that have aggregate functions. Note
    that the optimizer may choose to optimize away these fields by
    replacing them with constants, in which case sum_func_count will
    need to be updated.

    @see opt_sum_query, count_field_types
  */
  uint  sum_func_count;   
  uint  hidden_field_count;
  uint	group_parts,group_length,group_null_parts;
  uint	quick_group;
  /**
    Number of outer_sum_funcs i.e the number of set functions that are
    aggregated in a query block outer to this subquery.

    @see count_field_types
  */
  uint  outer_sum_func_count;
  /**
    Enabled when we have atleast one outer_sum_func. Needed when used
    along with distinct.

    @see create_tmp_table
  */
  bool  using_outer_summary_function;
  CHARSET_INFO *table_charset; 
  bool schema_table;
  /*
    True if GROUP BY and its aggregate functions are already computed
    by a table access method (e.g. by loose index scan). In this case
    query execution should not perform aggregation and should treat
    aggregate functions as normal functions.
  */
  bool precomputed_group_by;
  bool force_copy_fields;
  /**
    TRUE <=> don't actually create table handler when creating the result
    table. This allows range optimizer to add indexes later.
    Used for materialized derived tables/views.
    @see TABLE_LIST::update_derived_keys.
  */
  bool skip_create_table;
  /*
    If TRUE, create_tmp_field called from create_tmp_table will convert
    all BIT fields to 64-bit longs. This is a workaround the limitation
    that MEMORY tables cannot index BIT columns.
  */
  bool bit_fields_as_long;
  /*
    Generally, pk of internal temp table can be used as unique key to eliminate
    the duplication of records. But because Innodb doesn't support disable PK
    (cluster key)when doing operations mixed UNION ALL and UNION, the PK can't
    be treated as the unique key in such a case.
  */
  bool can_use_pk_for_unique;

  Temp_table_param()
    :copy_field(NULL), copy_field_end(NULL),
     recinfo(NULL), start_recinfo(NULL),
     keyinfo(NULL),
     field_count(0), func_count(0), sum_func_count(0), hidden_field_count(0),
     group_parts(0), group_length(0), group_null_parts(0),
     quick_group(1),
     outer_sum_func_count(0),
     using_outer_summary_function(false),
     table_charset(NULL),
     schema_table(false), precomputed_group_by(false), force_copy_fields(false),
     skip_create_table(false), bit_fields_as_long(false),
     can_use_pk_for_unique(true)
  {}
  ~Temp_table_param()
  {
    cleanup();
  }

  void cleanup(void);
};

#endif  // TEMP_TABLE_PARAM_INCLUDED
