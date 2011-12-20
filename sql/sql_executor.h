#ifndef SQL_EXECUTOR_INCLUDED
#define SQL_EXECUTOR_INCLUDED

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

/** @file Classes for query execution */

#include "records.h"                          /* READ_RECORD */

class JOIN;
typedef struct st_join_table JOIN_TAB;
typedef struct st_table_ref TABLE_REF;
typedef struct st_position POSITION;

enum enum_nested_loop_state
{
  NESTED_LOOP_KILLED= -2, NESTED_LOOP_ERROR= -1,
  NESTED_LOOP_OK= 0, NESTED_LOOP_NO_MORE_ROWS= 1,
  NESTED_LOOP_QUERY_LIMIT= 3, NESTED_LOOP_CURSOR_LIMIT= 4
};


typedef enum_nested_loop_state
(*Next_select_func)(JOIN *, struct st_join_table *, bool);

/*
  Temporary table used by semi-join DuplicateElimination strategy

  This consists of the temptable itself and data needed to put records
  into it. The table's DDL is as follows:

    CREATE TABLE tmptable (col VARCHAR(n) BINARY, PRIMARY KEY(col));

  where the primary key can be replaced with unique constraint if n exceeds
  the limit (as it is always done for query execution-time temptables).

  The record value is a concatenation of rowids of tables from the join we're
  executing. If a join table is on the inner side of the outer join, we
  assume that its rowid can be NULL and provide means to store this rowid in
  the tuple.
*/

class SJ_TMP_TABLE : public Sql_alloc
{
public:
  /*
    Array of pointers to tables whose rowids compose the temporary table
    record.
  */
  class TAB
  {
  public:
    JOIN_TAB *join_tab;
    uint rowid_offset;
    ushort null_byte;
    uchar null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;
  
  /* 
    is_confluent==TRUE means this is a special case where the temptable record
    has zero length (and presence of a unique key means that the temptable can
    have either 0 or 1 records). 
    In this case we don't create the physical temptable but instead record
    its state in SJ_TMP_TABLE::have_confluent_record.
  */
  bool is_confluent;

  /* 
    When is_confluent==TRUE: the contents of the table (whether it has the
    record or not).
  */
  bool have_confluent_row;
  
  /* table record parameters */
  uint null_bits;
  uint null_bytes;
  uint rowid_len;

  /* The temporary table itself (NULL means not created yet) */
  TABLE *tmp_table;

  /*
    These are the members we got from temptable creation code. We'll need
    them if we'll need to convert table from HEAP to MyISAM/Maria.
  */
  MI_COLUMNDEF *start_recinfo;
  MI_COLUMNDEF *recinfo;

  /* Pointer to next table (next->start_idx > this->end_idx) */
  SJ_TMP_TABLE *next; 
};

enum_nested_loop_state sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool
                                        end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, JOIN_TAB *join_tab,
                                      bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, JOIN_TAB *join_tab,
                                       bool end_of_records);
enum_nested_loop_state sub_select_sjm(JOIN *join, JOIN_TAB *join_tab, 
                                      bool end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JOIN_TAB *join_tab, bool
                                  end_of_records);



void copy_fields(TMP_TABLE_PARAM *param);
bool copy_funcs(Item **func_ptr, const THD *thd);
bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref);
int report_error(TABLE *table, int error);
int safe_index_read(JOIN_TAB *tab);
SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length,
                                  SORT_FIELD *sortorder);
void pick_table_access_method(JOIN_TAB *tab);

int join_read_const_table(JOIN_TAB *tab, POSITION *pos);
void join_read_key_unlock_row(st_join_table *tab);
int join_init_quick_read_record(JOIN_TAB *tab);
int join_init_read_record(JOIN_TAB *tab);
int join_read_first(JOIN_TAB *tab);
int join_read_last(JOIN_TAB *tab);
int join_read_last_key(JOIN_TAB *tab);
int join_materialize_table(JOIN_TAB *tab);
int join_read_prev_same(READ_RECORD *info);

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl);
int test_if_item_cache_changed(List<Cached_item> &list);

#endif /* SQL_EXECUTOR_INCLUDED */
