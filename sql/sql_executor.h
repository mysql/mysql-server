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

/**
   Possible status of a "nested loop" operation (Next_select_func family of
   functions).
   All values except NESTED_LOOP_OK abort the nested loop.
*/
enum enum_nested_loop_state
{
  /**
     Thread shutdown was requested while processing the record
     @todo could it be merged with NESTED_LOOP_ERROR? Why two distinct states?
  */
  NESTED_LOOP_KILLED= -2,
  /// A fatal error (like table corruption) was detected
  NESTED_LOOP_ERROR= -1,
  /// Record has been successfully handled
  NESTED_LOOP_OK= 0,
  /**
     Record has been successfully handled; additionally, the nested loop
     produced the number of rows specified in the LIMIT clause for the query.
  */
  NESTED_LOOP_QUERY_LIMIT= 3,
  /**
     Record has been successfully handled; additionally, there is a cursor and
     the nested loop algorithm produced the number of rows that is specified
     for current cursor fetch operation.
  */
  NESTED_LOOP_CURSOR_LIMIT= 4
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


 /**
  Executor structure for the materialized semi-join info, which contains
   - Description of expressions selected from subquery
   - The sj-materialization temporary table
*/
class Semijoin_mat_exec : public Sql_alloc
{
public:
  Semijoin_mat_exec(TABLE_LIST *sj_nest, bool is_scan, uint table_count,
                    uint mat_table_index, uint inner_table_index)
    :sj_nest(sj_nest), is_scan(is_scan), table_count(table_count),
     mat_table_index(mat_table_index), inner_table_index(inner_table_index),
    table_param(), table(NULL)
  {}
  ~Semijoin_mat_exec()
  {}
  TABLE_LIST *const sj_nest;    ///< Semi-join nest for this materialization
  const bool is_scan;           ///< TRUE if executing a scan, FALSE if lookup
  const uint table_count;       ///< Number of tables in the sj-nest
  const uint mat_table_index;   ///< Index in join_tab for materialized table
  const uint inner_table_index; ///< Index in join_tab for first inner table
  TMP_TABLE_PARAM table_param;  ///< The temptable and its related info
  TABLE *table;                 ///< Reference to temporary table
};



/**
  QEP_operation is an interface class for operations in query execution plan.

  Currently following operations are implemented:
    JOIN_CACHE      - caches partial join result and joins with attached table
    QEP_tmp_table   - materializes join result in attached table

  An operation's life cycle is as follows:
  .) it is initialized on the init() call
  .) accumulates records one by one when put_record() is called.
  .) finalize record sending when end_send() is called.
  .) free all internal buffers on the free() call.

  Each operation is attached to a join_tab, to which exactly depends on the
  operation type: JOIN_CACHE is attached to the table following the table
  being cached, QEP_tmp_buffer is attached to a tmp table.
*/

class QEP_operation :public Sql_alloc
{
public:
  // Type of the operation
  enum enum_op_type { OT_CACHE, OT_TMP_TABLE };
  /**
    For JOIN_CACHE : Table to be joined with the partial join records from
                     the cache
    For JOIN_TMP_BUFFER : join_tab of tmp table
  */
  JOIN_TAB *join_tab;

  QEP_operation(): join_tab(NULL) {};
  QEP_operation(JOIN_TAB *tab): join_tab(tab) {};
  virtual ~QEP_operation() {};
  virtual enum_op_type type()= 0;
  /**
    Initialize operation's internal state.  Called once per query execution.
  */
  virtual int init() { return 0; };
  /**
    Put a new record into the operation's buffer
    @return
      return one of enum_nested_loop_state values.
  */
  virtual enum_nested_loop_state put_record()= 0;
  /**
    Finalize records sending.
  */
  virtual enum_nested_loop_state end_send()= 0;
  /**
    Internal state cleanup.
  */
  virtual void free() {};
};


/**
  @brief
    Class for accumulating join result in a tmp table, grouping them if
    necessary, and sending further.

  @details
    Join result records are accumulated on the put_record() call.
    The accumulation process is determined by the write_func, it could be:
      end_write          Simply store all records in tmp table.
      end_write_group    Perform grouping using join->group_fields,
                         records are expected to be sorted.
      end_update         Perform grouping using the key generated on tmp
                         table. Input records aren't expected to be sorted.
                         Tmp table uses the heap engine
      end_update_unique  Same as above, but the engine is myisam.

    Lazy table initialization is used - the table will be instantiated and
    rnd/index scan started on the first put_record() call.

*/

class QEP_tmp_table :public QEP_operation
{
public:
  QEP_tmp_table(JOIN_TAB *tab) : QEP_operation(tab),
    write_func(NULL)
  {};
  enum_op_type type() { return OT_TMP_TABLE; }
  enum_nested_loop_state put_record() { return put_record(false); };
  /*
    Send the result of operation further (to a next operation/client)
    This function is called after all records were put into the buffer
    (determined by the caller).

    @return return one of enum_nested_loop_state values.
  */
  enum_nested_loop_state end_send();
  /** write_func setter */
  void set_write_func(Next_select_func new_write_func)
  {
    write_func= new_write_func;
  }

private:
  /** Write function that would be used for saving records in tmp table. */
  Next_select_func write_func;
  enum_nested_loop_state put_record(bool end_of_records);
  bool prepare_tmp_table();
};

void setup_tmptable_write_func(JOIN_TAB *tab);
Next_select_func setup_end_select_func(JOIN *join, JOIN_TAB *tab);
enum_nested_loop_state sub_select_op(JOIN *join, JOIN_TAB *join_tab, bool
                                        end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, JOIN_TAB *join_tab,
                                      bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, JOIN_TAB *join_tab,
                                       bool end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JOIN_TAB *join_tab, bool
                                  end_of_records);
enum_nested_loop_state
evaluate_join_record(JOIN *join, JOIN_TAB *join_tab, int error);



void copy_fields(TMP_TABLE_PARAM *param);
bool copy_funcs(Item **func_ptr, const THD *thd);
bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref);

/** Help function when we get some an error from the table handler. */
int report_handler_error(TABLE *table, int error);

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
int join_materialize_derived(JOIN_TAB *tab);
int join_materialize_semijoin(JOIN_TAB *tab);
int join_read_prev_same(READ_RECORD *info);

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl);
int test_if_item_cache_changed(List<Cached_item> &list);

// Create list for using with tempory table
bool change_to_use_tmp_fields(THD *thd, Ref_ptr_array ref_pointer_array,
				     List<Item> &new_list1,
				     List<Item> &new_list2,
				     uint elements, List<Item> &items);
// Create list for using with tempory table
bool change_refs_to_tmp_fields(THD *thd, Ref_ptr_array ref_pointer_array,
				      List<Item> &new_list1,
				      List<Item> &new_list2,
				      uint elements, List<Item> &items);
bool alloc_group_fields(JOIN *join, ORDER *group);
bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct);
bool setup_sum_funcs(THD *thd, Item_sum **func_ptr);
bool make_group_fields(JOIN *main_join, JOIN *curr_join);
void disable_sorted_access(JOIN_TAB* join_tab);
bool setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param,
		  Ref_ptr_array ref_pointer_array,
		  List<Item> &res_selected_fields, List<Item> &res_all_fields,
		  uint elements, List<Item> &all_fields);
#endif /* SQL_EXECUTOR_INCLUDED */
