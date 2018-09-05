#ifndef SQL_EXECUTOR_INCLUDED
#define SQL_EXECUTOR_INCLUDED

/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file sql/sql_executor.h
  Classes for query execution.
*/

#include <string.h>
#include <sys/types.h>
#include <memory>

#include "my_alloc.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "sql/item.h"
#include "sql/records.h"  // READ_RECORD
#include "sql/row_iterator.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"  // QEP_shared_owner
#include "sql/table.h"
#include "sql/temp_table_param.h"  // Temp_table_param

class Field;
class Field_longlong;
class Filesort;
class Item_sum;
class JOIN;
class JOIN_TAB;
class Opt_trace_object;
class QEP_TAB;
class QUICK_SELECT_I;
struct CACHE_FIELD;
struct MI_COLUMNDEF;
struct POSITION;
struct st_join_table;
template <class T>
class List;

/**
   Possible status of a "nested loop" operation (Next_select_func family of
   functions).
   All values except NESTED_LOOP_OK abort the nested loop.
*/
enum enum_nested_loop_state {
  /**
     Thread shutdown was requested while processing the record
     @todo could it be merged with NESTED_LOOP_ERROR? Why two distinct states?
  */
  NESTED_LOOP_KILLED = -2,
  /// A fatal error (like table corruption) was detected
  NESTED_LOOP_ERROR = -1,
  /// Record has been successfully handled
  NESTED_LOOP_OK = 0,
  /**
     Record has been successfully handled; additionally, the nested loop
     produced the number of rows specified in the LIMIT clause for the query.
  */
  NESTED_LOOP_QUERY_LIMIT = 3,
  /**
     Record has been successfully handled; additionally, there is a cursor and
     the nested loop algorithm produced the number of rows that is specified
     for current cursor fetch operation.
  */
  NESTED_LOOP_CURSOR_LIMIT = 4
};

typedef enum_nested_loop_state (*Next_select_func)(JOIN *, class QEP_TAB *,
                                                   bool);

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

class SJ_TMP_TABLE {
 public:
  SJ_TMP_TABLE() : hash_field(NULL) {}
  /*
    Array of pointers to tables whose rowids compose the temporary table
    record.
  */
  class TAB {
   public:
    QEP_TAB *qep_tab;
    uint rowid_offset;
    ushort null_byte;
    uchar null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;

  /*
    is_confluent==true means this is a special case where the temptable record
    has zero length (and presence of a unique key means that the temptable can
    have either 0 or 1 records).
    In this case we don't create the physical temptable but instead record
    its state in SJ_TMP_TABLE::have_confluent_record.
  */
  bool is_confluent;

  /*
    When is_confluent==true: the contents of the table (whether it has the
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
  /* Calc hash instead of too long key */
  Field_longlong *hash_field;
};

/**
 Executor structure for the materialized semi-join info, which contains
  - Description of expressions selected from subquery
  - The sj-materialization temporary table
*/
class Semijoin_mat_exec {
 public:
  Semijoin_mat_exec(TABLE_LIST *sj_nest, bool is_scan, uint table_count,
                    uint mat_table_index, uint inner_table_index)
      : sj_nest(sj_nest),
        is_scan(is_scan),
        table_count(table_count),
        mat_table_index(mat_table_index),
        inner_table_index(inner_table_index),
        table_param(),
        table(NULL) {}
  ~Semijoin_mat_exec() {}
  TABLE_LIST *const sj_nest;     ///< Semi-join nest for this materialization
  const bool is_scan;            ///< true if executing a scan, false if lookup
  const uint table_count;        ///< Number of tables in the sj-nest
  const uint mat_table_index;    ///< Index in join_tab for materialized table
  const uint inner_table_index;  ///< Index in join_tab for first inner table
  Temp_table_param table_param;  ///< The temptable and its related info
  TABLE *table;                  ///< Reference to temporary table
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

class QEP_operation {
 public:
  // Type of the operation
  enum enum_op_type { OT_CACHE, OT_TMP_TABLE };
  /**
    For JOIN_CACHE : Table to be joined with the partial join records from
                     the cache
    For JOIN_TMP_BUFFER : join_tab of tmp table
  */
  QEP_TAB *qep_tab;

  QEP_operation() : qep_tab(NULL) {}
  QEP_operation(QEP_TAB *qep_tab_arg) : qep_tab(qep_tab_arg) {}
  virtual ~QEP_operation() {}
  virtual enum_op_type type() = 0;
  /**
    Initialize operation's internal state.  Called once per query execution.
  */
  virtual int init() { return 0; }
  /**
    Put a new record into the operation's buffer
    @return
      return one of enum_nested_loop_state values.
  */
  virtual enum_nested_loop_state put_record() = 0;
  /**
    Finalize records sending.
  */
  virtual enum_nested_loop_state end_send() = 0;
  /**
    Internal state cleanup.
  */
  virtual void mem_free() {}
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

class QEP_tmp_table : public QEP_operation {
 public:
  QEP_tmp_table(QEP_TAB *qep_tab_arg)
      : QEP_operation(qep_tab_arg), write_func(NULL) {}
  enum_op_type type() { return OT_TMP_TABLE; }
  enum_nested_loop_state put_record() { return put_record(false); }
  /*
    Send the result of operation further (to a next operation/client)
    This function is called after all records were put into the buffer
    (determined by the caller).

    @return return one of enum_nested_loop_state values.
  */
  enum_nested_loop_state end_send();
  /** write_func setter */
  void set_write_func(Next_select_func new_write_func) {
    write_func = new_write_func;
  }

 private:
  /** Write function that would be used for saving records in tmp table. */
  Next_select_func write_func;
  enum_nested_loop_state put_record(bool end_of_records);
  MY_ATTRIBUTE((warn_unused_result))
  bool prepare_tmp_table();
};

void setup_tmptable_write_func(QEP_TAB *tab, Opt_trace_object *trace);
enum_nested_loop_state sub_select_op(JOIN *join, QEP_TAB *qep_tab,
                                     bool end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, QEP_TAB *qep_tab,
                                      bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, QEP_TAB *qep_tab,
                                       bool end_of_records);
enum_nested_loop_state sub_select(JOIN *join, QEP_TAB *qep_tab,
                                  bool end_of_records);
enum_nested_loop_state evaluate_join_record(JOIN *join, QEP_TAB *qep_tab,
                                            int error);
enum_nested_loop_state end_send_count(JOIN *join, QEP_TAB *qep_tab);

MY_ATTRIBUTE((warn_unused_result))
bool copy_fields(Temp_table_param *param, const THD *thd);

enum Copy_func_type {
  /**
    In non-windowing step, copies functions
  */
  CFT_ALL,
  /**
    In windowing step, copies framing window function, including
    all grouping aggregates, e.g. SUM, AVG and FIRST_VALUE, LAST_VALUE.
  */
  CFT_WF_FRAMING,
  /**
    In windowing step, copies non framing window function, e.g.
    ROW_NUMBER, RANK, DENSE_RANK, except those that are two_pass cf.
    copy_two_pass_window_functions which are treated separately.
   */
  CFT_WF_NON_FRAMING,
  /**
    In windowing step, copies window functions that need frame cardinality,
    that is we need to read all rows of a partition before we can compute the
    wf's value for the the first row in the partition.
  */
  CFT_WF_NEEDS_CARD,
  /**
    In windowing step, copies framing window functions that read only one row
    per frame.
  */
  CFT_WF_USES_ONLY_ONE_ROW,
  /**
    In final windowing step, copies all non-wf functions. Must be called after
    all wfs have been evaluated, as non-wf functions may reference wf,
    e.g. 1+RANK.
  */
  CFT_NON_WF,
  /**
    Copies all window functions.
  */
  CFT_WF
};

bool copy_funcs(Temp_table_param *, const THD *thd,
                Copy_func_type type = CFT_ALL);

/**
  Copy the lookup key into the table ref's key buffer.

  @param thd   pointer to the THD object
  @param table the table to read
  @param ref   information about the index lookup key

  @retval false ref key copied successfully
  @retval true  error dectected during copying of key
*/
bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref);

/** Help function when we get some an error from the table handler. */
int report_handler_error(TABLE *table, int error);

int safe_index_read(QEP_TAB *tab);

int join_read_const_table(JOIN_TAB *tab, POSITION *pos);
void join_setup_read_record(QEP_TAB *tab);
int join_materialize_derived(QEP_TAB *tab);
int join_materialize_table_function(QEP_TAB *tab);
int join_materialize_semijoin(QEP_TAB *tab);

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl);
int update_item_cache_if_changed(List<Cached_item> &list);

// Create list for using with tempory table
bool change_to_use_tmp_fields(THD *thd, Ref_item_array ref_item_array,
                              List<Item> &new_list1, List<Item> &new_list2,
                              uint elements, List<Item> &items);
// Create list for using with tempory table
bool change_refs_to_tmp_fields(THD *thd, Ref_item_array ref_item_array,
                               List<Item> &new_list1, List<Item> &new_list2,
                               uint elements, List<Item> &items);
bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct);
bool setup_sum_funcs(THD *thd, Item_sum **func_ptr);
bool make_group_fields(JOIN *main_join, JOIN *curr_join);
bool setup_copy_fields(THD *thd, Temp_table_param *param,
                       Ref_item_array ref_item_array,
                       List<Item> &res_selected_fields,
                       List<Item> &res_all_fields, uint elements,
                       List<Item> &all_fields);
bool check_unique_constraint(TABLE *table);
ulonglong unique_hash(Field *field, ulonglong *hash);

class QEP_TAB : public QEP_shared_owner {
 public:
  QEP_TAB()
      : QEP_shared_owner(),
        table_ref(NULL),
        flush_weedout_table(NULL),
        check_weed_out_table(NULL),
        firstmatch_return(NO_PLAN_IDX),
        loosescan_key_len(0),
        loosescan_buf(NULL),
        match_tab(NO_PLAN_IDX),
        found_match(false),
        found(false),
        not_null_compl(false),
        first_unmatched(NO_PLAN_IDX),
        rematerialize(false),
        materialize_table(NULL),
        next_select(NULL),
        read_record(),
        used_null_fields(false),
        used_uneven_bit_fields(false),
        keep_current_rowid(false),
        copy_current_rowid(NULL),
        not_used_in_distinct(false),
        cache_idx_cond(NULL),
        having(NULL),
        op(NULL),
        tmp_table_param(NULL),
        filesort(NULL),
        ref_item_slice(REF_SLICE_SAVED_BASE),
        send_records(0),
        m_condition_optim(NULL),
        m_quick_optim(NULL),
        m_keyread_optim(false),
        m_reversed_access(false),
        m_fetched_rows(0) {}

  /// Initializes the object from a JOIN_TAB
  void init(JOIN_TAB *jt);
  // Cleans up.
  void cleanup();

  // Getters and setters

  Item *condition_optim() const { return m_condition_optim; }
  QUICK_SELECT_I *quick_optim() const { return m_quick_optim; }
  void set_quick_optim() { m_quick_optim = quick(); }
  void set_condition_optim() { m_condition_optim = condition(); }
  bool keyread_optim() const { return m_keyread_optim; }
  void set_keyread_optim() {
    if (table()) m_keyread_optim = table()->key_read;
  }
  bool reversed_access() const { return m_reversed_access; }
  void set_reversed_access(bool arg) { m_reversed_access = arg; }

  void set_table(TABLE *t) {
    m_qs->set_table(t);
    if (t) t->reginfo.qep_tab = this;
  }

  /// @returns semijoin strategy for this table.
  uint get_sj_strategy() const;

  /// Return true if join_tab should perform a FirstMatch action
  bool do_firstmatch() const { return firstmatch_return != NO_PLAN_IDX; }

  /// Return true if join_tab should perform a LooseScan action
  bool do_loosescan() const { return loosescan_key_len; }

  /// Return true if join_tab starts a Duplicate Weedout action
  bool starts_weedout() const { return flush_weedout_table; }

  /// Return true if join_tab finishes a Duplicate Weedout action
  bool finishes_weedout() const { return check_weed_out_table; }

  bool prepare_scan();

  /**
    A helper function that allocates appropriate join cache object and
    sets next_select function of previous tab.
  */
  void init_join_cache(JOIN_TAB *join_tab);

  /**
     @returns query block id for an inner table of materialized semi-join, and
              0 for all other tables.
     @note implementation is not efficient (loops over all tables) - use this
     function only in EXPLAIN.
  */
  uint sjm_query_block_id() const;

  /// @returns whether this is doing QS_DYNAMIC_RANGE
  bool dynamic_range() const {
    if (!position()) return false;  // tmp table
    return using_dynamic_range;
  }

  bool use_order() const;  ///< Use ordering provided by chosen index?
  bool sort_table();
  bool remove_duplicates();

  inline bool skip_record(THD *thd, bool *skip_record_arg) {
    *skip_record_arg = condition() ? condition()->val_int() == false : false;
    return thd->is_error();
  }

  /**
     Used to begin a new execution of a subquery. Necessary if this subquery
     has done a filesort which which has cleared condition/quick.
  */
  void restore_quick_optim_and_condition() {
    if (m_condition_optim) set_condition(m_condition_optim);
    if (m_quick_optim) set_quick(m_quick_optim);
  }

  void pick_table_access_method(const JOIN_TAB *join_tab);
  void set_pushed_table_access_method(void);
  void push_index_cond(const JOIN_TAB *join_tab, uint keyno,
                       Opt_trace_object *trace_obj);

  /// @return the index used for a table in a QEP
  uint effective_index() const;

  bool pfs_batch_update(JOIN *join);

 public:
  /// Pointer to table reference
  TABLE_LIST *table_ref;

  /* Variables for semi-join duplicate elimination */
  SJ_TMP_TABLE *flush_weedout_table;
  SJ_TMP_TABLE *check_weed_out_table;

  /*
    If set, means we should stop join enumeration after we've got the first
    match and return to the specified join tab. May be PRE_FIRST_PLAN_IDX
    which means stopping join execution after the first match.
  */
  plan_idx firstmatch_return;

  /*
    Length of key tuple (depends on #keyparts used) to store in loosescan_buf.
    If zero, means that loosescan is not used.
  */
  uint loosescan_key_len;

  /* Buffer to save index tuple to be able to skip duplicates */
  uchar *loosescan_buf;

  /*
    If doing a LooseScan, this QEP is the first (i.e.  "driving")
    QEP_TAB, and match_tab points to the last QEP_TAB handled by the strategy.
    match_tab->found_match should be checked to see if the current value group
    had a match.
    If doing a FirstMatch, check this QEP_TAB to see if there is a match.
    Unless the FirstMatch performs a "split jump", this is equal to the
    current QEP_TAB.
  */
  plan_idx match_tab;

  /*
    Used by FirstMatch and LooseScan. true <=> there is a matching
    record combination
  */
  bool found_match;

  /**
    Used to decide whether an inner table of an outer join should produce NULL
    values. If it is true after a call to evaluate_join_record(), the join
    condition has been satisfied for at least one row from the inner
    table. This member is not really manipulated by this class, see sub_select
    for details on its use.
  */
  bool found;

  /**
    This member is true as long as we are evaluating rows from the inner
    tables of an outer join. If none of these rows satisfy the join condition,
    we generated NULL-complemented rows and set this member to false. In the
    meantime, the value may be read by triggered conditions, see
    Item_func_trig_cond::val_int().
  */
  bool not_null_compl;

  plan_idx first_unmatched; /**< used for optimization purposes only   */

  /// Dependent table functions have to be materialized on each new scan
  bool rematerialize;

  typedef int (*Setup_func)(QEP_TAB *);
  Setup_func materialize_table;
  bool using_dynamic_range = false;
  Next_select_func next_select;
  READ_RECORD read_record;

  // join-cache-related members
  bool used_null_fields;
  bool used_uneven_bit_fields;

  /*
    Used by DuplicateElimination. tab->table->ref must have the rowid
    whenever we have a current record. copy_current_rowid needed because
    we cannot bind to the rowid buffer before the table has been opened.
  */
  bool keep_current_rowid;
  CACHE_FIELD *copy_current_rowid;

  /** true <=> remove duplicates on this table. */
  bool needs_duplicate_removal = false;

  bool not_used_in_distinct;

  /// Index condition for BKA access join
  Item *cache_idx_cond;

  /** HAVING condition for checking prior saving a record into tmp table*/
  Item *having;

  QEP_operation *op;

  /* Tmp table info */
  Temp_table_param *tmp_table_param;

  /* Sorting related info */
  Filesort *filesort;

  /**
    Slice number of the ref items array to switch to before reading rows from
    this table.
  */
  uint ref_item_slice;

  /** Number of records saved in tmp table */
  ha_rows send_records;

  /// @see m_quick_optim
  Item *m_condition_optim;

  /**
     m_quick is the quick "to be used at this stage of execution".
     It can happen that filesort uses the quick (produced by the optimizer) to
     produce a sorted result, then the read of this result has to be done
     without "quick", so we must reset m_quick to NULL, but we want to delay
     freeing of m_quick or it would close the filesort's result and the table
     prematurely.
     In that case, we move m_quick to m_quick_optim (=> delay deletion), reset
     m_quick to NULL (read of filesort's result will be without quick); if
     this is a subquery which is later executed a second time,
     QEP_TAB::reset() will restore the quick from m_quick_optim into m_quick.
     quick_optim stands for "the quick decided by the optimizer".
     EXPLAIN reads this member and m_condition_optim; so if you change them
     after exposing the plan (setting plan_state), do it with the
     LOCK_query_plan mutex.
  */
  QUICK_SELECT_I *m_quick_optim;

  /**
     True if only index is going to be read for this table. This is the
     optimizer's decision.
  */
  bool m_keyread_optim;

  /**
    True if reversed scan is used. This is the optimizer's decision.
  */
  bool m_reversed_access;

  /**
    Count of rows fetched from this table; maintained by sub_select() and
    reset to 0 by JOIN::reset().
  */
  ha_rows m_fetched_rows;

  QEP_TAB(const QEP_TAB &);             // not defined
  QEP_TAB &operator=(const QEP_TAB &);  // not defined
};

/**
   @returns a pointer to the QEP_TAB whose index is qtab->member. For
   example, QEP_AT(x,first_inner) is the first_inner table of x.
*/
#define QEP_AT(qtab, member) (qtab->join()->qep_tab[qtab->member])

/**
   Use this class when you need a QEP_TAB not connected to any JOIN_TAB.
*/
class QEP_TAB_standalone {
 public:
  QEP_TAB_standalone() { m_qt.set_qs(&m_qs); }
  ~QEP_TAB_standalone() { m_qt.cleanup(); }
  /// @returns access to the QEP_TAB
  QEP_TAB &as_QEP_TAB() { return m_qt; }

 private:
  QEP_shared m_qs;
  QEP_TAB m_qt;
};

bool set_record_buffer(const QEP_TAB *tab);

#endif /* SQL_EXECUTOR_INCLUDED */
