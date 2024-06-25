/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// Support for derived tables.

#include "sql/sql_derived.h"
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/mem_root_array.h"
#include "sql/nested_join.h"
#include "sql/opt_trace.h"  // opt_trace_disable_etc
#include "sql/query_options.h"
#include "sql/sql_base.h"  // EXTRA_RECORD
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // parse_sql
#include "sql/sql_resolver.h"   // check_right_lateral_join
#include "sql/sql_tmp_table.h"  // Tmp tables
#include "sql/sql_union.h"      // Query_result_union
#include "sql/sql_view.h"       // check_duplicate_names
#include "sql/table.h"
#include "sql/table_function.h"
#include "sql/thd_raii.h"
#include "strfunc.h"
#include "thr_lock.h"

class Opt_trace_context;

/**
   Produces, from the first tmp TABLE object, a clone TABLE object for
   Table_ref 'tl', to have a single materialization of multiple references
   to a CTE.

   How sharing of a single tmp table works
   =======================================

   There are several scenarios.
   (1) Non-recursive CTE referenced only once: nothing special.
   (2) Non-recursive CTE referenced more than once:
   - multiple TABLEs, one TABLE_SHARE.
   - The first ref in setup_materialized_derived() calls
   create_tmp_table(); others call open_table_from_share().
   - The first ref in create_derived() calls instantiate_tmp_table()
   (which calls handler::create() then open_tmp_table()); others call
   open_tmp_table(). open_tmp_table() calls handler::open().
   - The first ref in materialize_derived() evaluates the subquery and does
   all writes to the tmp table.
   - Finally all refs set up a read access method (table scan, index scan,
   index lookup, etc) and do reads, possibly interlaced (example: a
   nested-loop join of two references to the CTE).
   - The storage engine (MEMORY or InnoDB) must be informed of the uses above;
   this is done by having TABLE_SHARE::ref_count>=2 for every handler::open()
   call.
   (3) Recursive CTE, referenced once or more than once:
   All of (2) applies, where the set of refs is the non-recursive
   ones (a recursive ref is a ref appearing in the definition of a recursive
   CTE). Additionally:
   - recursive refs do not call setup_materialized_derived(),
   create_derived(), materialize_derived().
   - right after a non-recursive ref has been in setup_materialized_derived(),
   its recursive refs are replaced with clones of that ref, made with
   open_table_from_share().
   - the first non-recursive ref in materialized_derived() initiates the
   with-recursive algorithm:
     * its recursive refs call open_tmp_table().
     * Then writes (to the non-recursive ref) and reads (from the recursive
     refs) happen interlaced.
   - a particular recursive ref is the UNION table, if UNION DISTINCT is
   present in the CTE's definition: there is a single TABLE for it,
   writes/reads to/from it happen interlaced (writes are done by
   Query_result_union::send_data(); reads are done by the fake_query_block's
   JOIN).
   - Finally all non-recursive refs set up a read access method and do reads,
   possibly interlaced.
   - The storage engine (MEMORY or InnoDB) must be informed of the uses above;
   this is done by having TABLE_SHARE::ref_count>=2 for every handler::open()
   call.
   - The Server code handling tmp table creation must also be informed:
   see how Query_result_union::create_result_table() disables PK promotion.

   How InnoDB manages the uses above
   =================================

   The storage engine needs to take measures so that inserts and reads
   don't corrupt each other's behaviour. In InnoDB that means two things
   (@see row_search_no_mvcc()):
   (a) A certain way to use its cursor when reading
   (b) Making the different handlers inform each other when one insertion
   modifies the structure of the index tree (e.g. splits a page; this
   triggers a refreshing of all read cursors).

   Requirements on tmp tables used to write/read CTEs
   ==================================================

   The internal tmp table must support a phase where table scans and
   insertions happen interlaced, either issued from a single TABLE or from
   multiple TABLE clones. If from a single TABLE, that object does repetitions
   of {"write rows" then "init scan / read rows / close scan"}. If from
   multiple TABLEs, one does "write rows", every other one does "init scan /
   read rows / close scan".
   During this, neither updates, nor deletes, nor any other type of read
   access than table scans, are allowed on this table (they are allowed after
   the phase's end).
   Any started table scan on this table:
   - must remember its position between two read operations, without influence
   from other scans/inserts;
   - must return rows inserted before and after it started (be catching up
   continuously) (however, when it reports EOF it is allowed to stop catching
   up and report EOF until closed).
   - must return rows in insertion order.
   - may be started from the first record (ha_rnd_init, ha_rnd_next) or from
   the record where the previous scan was ended (position(), ha_rnd_end,
   [...], ha_rnd_init, ha_rnd_pos(saved position), ha_rnd_next).
   - must return positions (handler::position()) which are stable if a write
   later occurs, so that a handler::rnd_pos() happening after the write finds
   the same record.

   Cursor re-positioning when MEMORY is converted to InnoDB
   ========================================================

   See create_ondisk_from_heap(). A requirement is that InnoDB is able to
   start a scan like this: rnd_init, rnd_pos(some PK value), rnd_next.

   @param thd   Thread handler
   @param tl    Table reference wanting the copy

   @returns New clone, or NULL if error
*/

TABLE *Common_table_expr::clone_tmp_table(THD *thd, Table_ref *tl) {
  // Should have been attached to CTE already.
  assert(tl->common_table_expr() == this);

#ifndef NDEBUG
  /*
    We're adding a clone; if another clone has been opened before, it was not
    aware of the new one, so perhaps the storage engine has not set up the
    necessary logic to share data among clones. Check that no clone is open:
  */
  Derived_refs_iterator it(tmp_tables[0]);
  while (TABLE *t = it.get_next()) assert(!t->is_created() && !t->materialized);
#endif
  TABLE *first = tmp_tables[0]->table;
  // Allocate clone on the memory root of the TABLE_SHARE.
  TABLE *t = static_cast<TABLE *>(first->s->mem_root.Alloc(sizeof(TABLE)));
  if (!t) return nullptr; /* purecov: inspected */
  if (open_table_from_share(thd, first->s, tl->alias,
                            /*
                              Pass db_stat == 0 to delay opening of table in SE,
                              as table is not instantiated in SE yet.
                            */
                            0,
                            /* We need record[1] for this TABLE instance. */
                            EXTRA_RECORD |
                                /*
                                  Use DELAYED_OPEN to have its own record[0]
                                  (necessary because db_stat is 0).
                                  Otherwise it would be shared with 'first'
                                  and thus a write to tmp table would modify
                                  the row just read by readers.
                                */
                                DELAYED_OPEN,
                            0, t, false, nullptr))
    return nullptr; /* purecov: inspected */
  assert(t->s == first->s && t != first && t->file != first->file);
  t->s->increment_ref_count();
  t->s->tmp_handler_count++;

  // In case this clone is used to fill the materialized table:
  bitmap_set_all(t->write_set);
  t->reginfo.lock_type = TL_WRITE;
  t->copy_blobs = true;

  tl->table = t;
  t->pos_in_table_list = tl;

  // If initial CTE table has a hash key, set up a hash key for
  // all clones too.
  if (first->hash_field) {
    t->hash_field = t->field[0];
  }
  t->hidden_field_count = first->hidden_field_count;

  t->set_not_started();

  if (tmp_tables.push_back(tl)) return nullptr; /* purecov: inspected */

  if (tl->derived_result != nullptr) {
    // Make clone's copy of tmp_table_param contain correct info, so copy
    tl->derived_result->tmp_table_param =
        tmp_tables[0]->derived_result->tmp_table_param;
  }

  return t;
}

/**
   Replaces the recursive reference in query block 'sl' with a clone of
   the first tmp table.

   @param thd   Thread handler
   @param sl    Query block

   @returns true if error
*/
bool Common_table_expr::substitute_recursive_reference(THD *thd,
                                                       Query_block *sl) {
  Table_ref *tl = sl->recursive_reference;
  assert(tl != nullptr && tl->table == nullptr);
  TABLE *t = clone_tmp_table(thd, tl);
  if (t == nullptr) return true; /* purecov: inspected */
  // Eliminate the dummy unit:
  tl->derived_query_expression()->exclude_tree();
  tl->set_derived_query_expression(nullptr);
  tl->set_privileges(SELECT_ACL);
  return false;
}

void Common_table_expr::remove_table(Table_ref *tr) {
  (void)tmp_tables.erase_value(tr);
}

/**
  Resolve a derived table or view reference, including recursively resolving
  contained subqueries.

  @param thd thread handle
  @param apply_semijoin Apply possible semi-join transforms if this is true

  @returns false if success, true if error
*/

bool Table_ref::resolve_derived(THD *thd, bool apply_semijoin) {
  DBUG_TRACE;

  /*
    Helper class which takes care of restoration of members like
    THD::derived_tables_processing. These members are changed in this
    method scope for resolving derived tables.
  */
  class Context_handler {
   public:
    Context_handler(THD *thd)
        : m_thd(thd),
          m_deny_window_func_saved(thd->lex->m_deny_window_func),
          m_derived_tables_processing_saved(thd->derived_tables_processing) {
      /*
        Window functions are allowed; they're aggregated in the derived
        table's definition.
      */
      m_thd->lex->m_deny_window_func = 0;
      m_thd->derived_tables_processing = true;
    }

    ~Context_handler() {
      m_thd->lex->m_deny_window_func = m_deny_window_func_saved;
      m_thd->derived_tables_processing = m_derived_tables_processing_saved;
    }

   private:
    // Thread handle.
    THD *m_thd;

    // Saved state of THD::LEX::m_deny_window_func.
    nesting_map m_deny_window_func_saved;

    // Saved state of THD::derived_tables_processing.
    bool m_derived_tables_processing_saved;
  };

  if (!is_view_or_derived() || is_merged() || is_table_function()) return false;

  // Dummy derived tables for recursive references disappear before this stage
  assert(this != query_block->recursive_reference);

  if (is_derived() && derived->m_lateral_deps)
    query_block->end_lateral_table = this;

  const Context_handler ctx_handler(thd);

#ifndef NDEBUG    // CTEs, derived tables can have outer references
  if (is_view())  // but views cannot.
    for (Query_block *sl = derived->first_query_block(); sl;
         sl = sl->next_query_block()) {
      // Make sure there are no outer references
      assert(sl->context.outer_context == nullptr);
    }
#endif

  if (m_common_table_expr && m_common_table_expr->recursive &&
      !derived->is_recursive()) {
    // Ensure it's UNION.
    if (!derived->is_union()) {
      my_error(ER_CTE_RECURSIVE_REQUIRES_UNION, MYF(0), alias);
      return true;
    }
    if (derived->global_parameters()->is_ordered()) {
      /*
        ORDER BY applied to the UNION causes the use of the union tmp
        table. The fake_query_block would want to sort that table, which isn't
        going to work as the table is incomplete when fake_query_block first
        reads it. Workaround: put ORDER BY in the top query.
        Another reason: allowing
        ORDER BY <condition using fulltext> would make the UNION tmp table be
        of MyISAM engine which recursive CTEs don't support.
        LIMIT is allowed and will stop the row generation after N rows.
        However, without ORDER BY the CTE's content is ordered in an
        unpredictable way, so LIMIT theoretically returns an unpredictable
        subset of rows. Users are on their own.
        Instead of LIMIT, users can have a counter column and use a WHERE
        on it, to control depth level, which sounds more intelligent than a
        limit.
      */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0),
               "ORDER BY over UNION "
               "in recursive Common Table Expression");
      return true;
    }
    /*
      Should be:
      SELECT1 UNION [DISTINCT | ALL] ... SELECTN
      where SELECT1 is non-recursive, and all non-recursive SELECTs are before
      all recursive SELECTs.
      In SQL standard terms, the CTE must be "expandable" except that we allow
      it to have more than one recursive SELECT.
    */
    bool previous_is_recursive = false;
    Query_block *last_non_recursive = nullptr;
    for (Query_block *sl = derived->first_query_block(); sl;
         sl = sl->next_query_block()) {
      if (sl->is_recursive()) {
        if (sl->parent()->term_type() != QT_UNION) {
          my_error(ER_CTE_RECURSIVE_NOT_UNION, MYF(0));
          return true;
        } else if (sl->parent()->parent() != nullptr) {
          /*
            Right-nested UNIONs with recursive query blocks are not allowed. It
            is expected that all possible flattening of UNION blocks is done
            beforehand. Any nested UNION indicates a mixing of UNION DISTINCT
            and UNION ALL, which cannot be flattened further.
          */
          my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                   "right nested recursive query blocks, in "
                   "Common Table Expression");
          return true;
        }
        if (sl->is_ordered() || sl->has_limit() || sl->is_distinct()) {
          /*
            On top of posing implementation problems, it looks meaningless to
            want to order/limit every iterative sub-result.
            SELECT DISTINCT, if all expressions are constant, is implemented
            as LIMIT in QEP_TAB::remove_duplicates(); do_query_block() starts
            with send_records=0 so loses track of rows which have been sent in
            previous iterations.
          */
          my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                   "ORDER BY / LIMIT / SELECT DISTINCT"
                   " in recursive query block of Common Table Expression");
          return true;
        }
        if (sl == derived->last_distinct() && sl->next_query_block()) {
          /*
            Consider
              anchor UNION ALL rec1 UNION DISTINCT rec2 UNION ALL rec3:
            after execution of rec2 we must turn off the duplicate-checking
            index; it will thus not contain the keys of rows of rec3, so it
            becomes permanently unusable. The next iteration of rec1 or rec2
            may insert rows which are actually duplicates of those of rec3.
            So: if the last QB having DISTINCT to its left is recursive, and
            it is followed by another QB (necessarily connected with ALL),
            reject the query.
          */
          my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                   "recursive query blocks with"
                   " UNION DISTINCT then UNION ALL, in recursive "
                   "Common Table Expression");
          return true;
        }
      } else {
        if (previous_is_recursive) {
          my_error(ER_CTE_RECURSIVE_REQUIRES_NONRECURSIVE_FIRST, MYF(0), alias);
          return true;
        }
        last_non_recursive = sl;
      }
      previous_is_recursive = sl->is_recursive();
    }
    if (last_non_recursive == nullptr) {
      my_error(ER_CTE_RECURSIVE_REQUIRES_NONRECURSIVE_FIRST, MYF(0), alias);
      return true;
    }
    derived->first_recursive = last_non_recursive->next_query_block();
    assert(derived->is_recursive());
  }

  DEBUG_SYNC(thd, "derived_not_set");

  derived->derived_table = this;

  if (!(derived_result = new (thd->mem_root) Query_result_union()))
    return true; /* purecov: inspected */

  /// Give the unit to the result (the other fields are ignored).
  mem_root_deque<Item *> empty_list(thd->mem_root);
  if (derived_result->prepare(thd, empty_list, derived_query_expression()))
    return true;

  /*
    Prepare the underlying query expression of the derived table.
  */
  if (derived->prepare(thd, derived_result, nullptr,
                       !apply_semijoin ? SELECT_NO_SEMI_JOIN : 0, 0))
    return true;

  if (check_duplicate_names(m_derived_column_names,
                            *derived->get_unit_column_types(), false))
    return true;

  if (is_derived()) {
    // The underlying tables of a derived table are all readonly:
    for (Query_block *sl = derived->first_query_block(); sl;
         sl = sl->next_query_block())
      sl->set_tables_readonly();
    /*
      A derived table is transparent with respect to privilege checking.
      This setting means that privilege checks ignore the derived table
      and are done properly in underlying base tables and views.
      SELECT_ACL is used because derived tables cannot be used for update,
      delete or insert.
    */
    set_privileges(SELECT_ACL);

    if (derived->m_lateral_deps) {
      query_block->end_lateral_table = nullptr;
      derived->m_lateral_deps &= ~PSEUDO_TABLE_BITS;
      /*
        It is possible that derived->m_lateral_deps is now 0, if it was
        declared as LATERAL but actually contained no lateral references. Then
        it will be handled as if LATERAL hadn't been specified.
      */
    }
  }

  return false;
}

/// Helper function for Table_ref::setup_materialized_derived()
static void swap_column_names_of_unit_and_tmp_table(
    const mem_root_deque<Item *> &unit_items,
    const Create_col_name_list &tmp_table_col_names) {
  if (CountVisibleFields(unit_items) != tmp_table_col_names.size())
    // check_duplicate_names() will find and report error
    return;
  uint fieldnr = 0;
  for (Item *item : VisibleFields(unit_items)) {
    const char *s = item->item_name.ptr();
    size_t l = item->item_name.length();
    LEX_CSTRING &other_name =
        const_cast<LEX_CSTRING &>(tmp_table_col_names[fieldnr]);
    item->item_name.set(other_name.str, other_name.length);
    other_name.str = s;
    other_name.length = l;
    fieldnr++;
  }
}

/**
  Copy field information like table_ref, context etc of all the fields
  from the original expression to the cloned expression.
  @param thd          current thread
  @param orig_expr    original expression
  @param cloned_expr  cloned expression

  @returns true on error, false otherwise
*/
bool copy_field_info(THD *thd, Item *orig_expr, Item *cloned_expr) {
  class Field_info {
   public:
    Name_resolution_context *m_field_context{nullptr};
    Table_ref *m_table_ref{nullptr};
    Query_block *m_depended_from{nullptr};
    Field *m_field{nullptr};
    Field_info(Name_resolution_context *field_context, Table_ref *table_ref,
               Query_block *depended_from, Field *field)
        : m_field_context(field_context),
          m_table_ref(table_ref),
          m_depended_from(depended_from),
          m_field(field) {}
  };
  mem_root_deque<Field_info> field_info(thd->mem_root);
  class Collect_field_info : public Item_tree_walker {
   public:
    using Item_tree_walker::is_stopped;
    using Item_tree_walker::stop_at;
  };
  Collect_field_info info;
  Item_ref *ref_item = nullptr;
  // Collect information for fields from the original expression
  if (WalkItem(
          orig_expr, enum_walk::PREFIX | enum_walk::POSTFIX,
          [&info, &field_info, &ref_item](Item *inner_item) {
            if (info.is_stopped(inner_item)) return false;
            if (ref_item == inner_item) {
              // We have returned back to this root (POSTFIX) from where
              // we copied the "depended_from" information. Reset it now.
              ref_item = nullptr;
              return false;
            }
            if (inner_item->type() == Item::REF_ITEM &&
                inner_item->is_outer_reference()) {
              // If we have cloned a reference item that is an outer
              // reference, the underlying field might not be marked as
              // such. So we copy the "depended_from" information from the
              // reference.
              ref_item = down_cast<Item_ref *>(inner_item);
              return false;
            } else if (inner_item->type() == Item::FIELD_ITEM) {
              Item_field *field = down_cast<Item_field *>(inner_item);
              // If this field is being referenced, then it's "depended_from"
              // is part of reference. If it is part of the field as well,
              // check for consistency and then use the information.
              Query_block *depended_from =
                  (ref_item != nullptr) ? ref_item->depended_from : nullptr;
              Name_resolution_context *context =
                  (ref_item != nullptr) ? ref_item->context : nullptr;
              assert(depended_from == nullptr ||
                     depended_from == field->depended_from ||
                     depended_from == field->context->query_block);
              depended_from = field->depended_from != nullptr
                                  ? field->depended_from
                                  : depended_from;
              context = (context == nullptr)
                            ? field->context
                            : ((field->context->query_block->nest_level >=
                                context->query_block->nest_level)
                                   ? field->context
                                   : context);
              if (field_info.push_back(Field_info(context, field->m_table_ref,
                                                  depended_from, field->field)))
                return true;
              info.stop_at(inner_item);
            }
            return false;
          }))
    return true;
  // Copy the information to the fields in the cloned expression.
  WalkItem(cloned_expr, enum_walk::PREFIX, [&field_info](Item *inner_item) {
    if (inner_item->type() == Item::FIELD_ITEM) {
      assert(!field_info.empty());
      Item_field *field = down_cast<Item_field *>(inner_item);
      field->context = field_info[0].m_field_context;
      field->m_table_ref = field_info[0].m_table_ref;
      field->depended_from = field_info[0].m_depended_from;
      field->field = field_info[0].m_field;
      field_info.pop_front();
    }
    return false;
  });
  assert(field_info.empty());
  return false;
}

/**
  Given an item and a query block, this function creates a clone of the
  item (unresolved) by reparsing the item. Used during condition pushdown
  to derived tables.

  @param thd            Current thread.
  @param item           Item to be reparsed to get a clone.
  @param query_block    query block where expression is being parsed
  @param derived_table  derived table to which the item belongs to.
                        "nullptr" when cloning to make a copy of the
                        original condition to be pushed down
                        to a derived table that has SET operations.

  @returns A copy of the original item (unresolved) on success else nullptr.
*/
static Item *parse_expression(THD *thd, Item *item, Query_block *query_block,
                              Table_ref *derived_table) {
  // Set up for parsing item
  LEX *const old_lex = thd->lex;
  LEX new_lex;
  thd->lex = &new_lex;

  if (lex_start(thd)) {
    thd->lex = old_lex;
    return nullptr;  // OOM
  }
  // Take care not to print the variable index for stored procedure variables.
  // Also do not write a cloned stored procedure variable to query logs.
  thd->lex->reparse_derived_table_condition = true;
  // Get the printout of the expression
  StringBuffer<1024> str_buf(thd->charset());
  // For printing parameters we need to specify the flag QT_NO_DATA_EXPANSION
  // because for a case when statement gets reprepared during execution, we
  // still need Item_param::print() to print the '?' rather than the actual data
  // specified for the parameter.
  // The flag QT_TO_ARGUMENT_CHARSET is required for printing character string
  // literals with correct character set introducer.
  item->print(thd, &str_buf,
              enum_query_type(QT_NO_DATA_EXPANSION | QT_TO_ARGUMENT_CHARSET));
  str_buf.append('\0');

  String str;
  if (copy_string(thd->mem_root, &str, &str_buf)) return nullptr;
  Derived_expr_parser_state parser_state;
  parser_state.init(thd, str.ptr(), str.length());

  // Native functions introduced for INFORMATION_SCHEMA system views are
  // allowed to be invoked from *only* INFORMATION_SCHEMA system views.
  // THD::parsing_system_view is set if the view being parsed is
  // INFORMATION_SCHEMA system view and is allowed to invoke native function.
  // If not, error ER_NO_ACCESS_TO_NATIVE_FCT is reported.
  // Since we are cloning a condition here, we set it unconditionally
  // to avoid the errors.
  const bool parsing_system_view_saved = thd->parsing_system_view;
  thd->parsing_system_view = true;

  // Set the correct query block to parse the item. In some cases, like
  // fulltext functions, parser needs to add them to ftfunc_list of the
  // query block.
  thd->lex->unit = query_block->master_query_expression();
  thd->lex->set_current_query_block(query_block);
  // If this query block is part of a stored procedure, we might have to
  // parse a stored procedure variable (if present). Set the context
  // correctly.
  thd->lex->set_sp_current_parsing_ctx(old_lex->get_sp_current_parsing_ctx());
  thd->lex->sphead = old_lex->sphead;

  // If this is a prepare statement, we need to set prepare_mode correctly
  // so that parser does not raise errors for "params(?)".
  parser_state.m_lip.stmt_prepare_mode =
      (old_lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_PREPARE);
  if (parser_state.m_lip.stmt_prepare_mode) {
    // Collect positions of all parameters in the "item". Used to create
    // clones for the original parameters(Item_param::m_clones).
    WalkItem(item, enum_walk::POSTFIX, [&thd](Item *inner_item) {
      if (inner_item->type() == Item::PARAM_ITEM) {
        thd->lex->reparse_derived_table_params_at.push_back(
            down_cast<Item_param *>(inner_item)->pos_in_query);
      }
      return false;
    });
    thd->lex->param_list = old_lex->param_list;
  }

  // Get a newly created item from parser. Use the view creation
  // context if the item being parsed is part of a view.
  View_creation_ctx *view_creation_ctx =
      derived_table != nullptr ? derived_table->view_creation_ctx : nullptr;
  const bool result = parse_sql(thd, &parser_state, view_creation_ctx);

  // If a statement is being re-prepared, then all the parameters
  // that are cloned above need to be synced with the original
  // parameters that are specified in the query. In case of
  // re-prepare original parameters would have been assigned
  // a value and therefore the types too. When fix_fields() is
  // later called for the cloned expression, resolver would be
  // able to assign the type correctly for the cloned parameter
  // if it is synced with it's master.
  if (parser_state.result != nullptr) {
    List_iterator_fast<Item_param> it(thd->lex->param_list);
    WalkItem(parser_state.result, enum_walk::POSTFIX, [&it](Item *inner_item) {
      if (inner_item->type() == Item::PARAM_ITEM) {
        Item_param *master;
        while ((master = it++)) {
          if (master->pos_in_query ==
              down_cast<Item_param *>(inner_item)->pos_in_query)
            master->sync_clones();
        }
      }
      return false;
    });
  }
  thd->lex->reparse_derived_table_condition = false;
  // lex_end() would try to destroy sphead if set. So we reset it.
  thd->lex->set_sp_current_parsing_ctx(nullptr);
  thd->lex->sphead = nullptr;
  // End of parsing.
  lex_end(thd->lex);
  thd->lex = old_lex;
  thd->parsing_system_view = parsing_system_view_saved;
  if (result) return nullptr;

  return parser_state.result;
}

/**
  Resolves the expression given. Used with parse_expression()
  to clone an item during condition pushdown. For all the
  column references in the expression, information like table
  reference, field, context etc is expected to be correctly set.
  This will just do a short cut fix_fields() for Item_field.

  @param thd         Current thread.
  @param item        Item to resolve.
  @param query_block query block where this item needs to be
                     resolved.

  @returns
  resolved item if resolving was successful else nullptr.
*/
Item *resolve_expression(THD *thd, Item *item, Query_block *query_block) {
  const Access_bitmask save_old_privilege = thd->want_privilege;
  thd->want_privilege = 0;
  Query_block *saved_current_query_block = thd->lex->current_query_block();
  thd->lex->set_current_query_block(query_block);
  const nesting_map save_allow_sum_func = thd->lex->allow_sum_func;
  thd->lex->allow_sum_func |= static_cast<nesting_map>(1)
                              << thd->lex->current_query_block()->nest_level;

  if (item->fix_fields(thd, &item)) {
    return nullptr;
  }
  // For items with params, propagate the default data type.
  if (item->data_type() == MYSQL_TYPE_INVALID &&
      item->propagate_type(thd, item->default_data_type())) {
    return nullptr;
  }
  // Restore original state back
  thd->want_privilege = save_old_privilege;
  thd->lex->set_current_query_block(saved_current_query_block);
  thd->lex->allow_sum_func = save_allow_sum_func;
  return item;
}

/**
  Clone an expression. This clone will be used for pushing conditions
  down to a materialized derived table.
  Cloning of an expression is done for two purposes:
  1. When the derived table has a query expression with multiple query
  blocks, each query block involved will be getting a clone of the
  condition that is being pushed down.
  2. When pushing a condition down to a derived table (with or without
  unions), columns in the condition are replaced with the derived
  table's expressions. If there are nested derived tables, these columns
  will be replaced again with another derived table's expression when
  the condition is pushed further down. If the derived table expressions
  are simple columns, we would just keep replacing the original columns
  with derived table columns. However if the derived table expressions
  are not simple column references E.g. functions, then columns will be
  replaced with functions, and arguments to these functions would get
  replaced when the condition is pushed further down. However, arguments
  to a function are part of both the SELECT clause of one derived table
  and the WHERE clause of another derived table where the condition is
  pushed down (Example below). To keep the sanity of the derived table's
  expression, a clone is created and used before pushing a condition down.

  Ex: Where cloned objects become necessary even when the derived
  table does not have a UNION.

  Consider a query like this one:
  SELECT * FROM (SELECT i+10 AS n FROM
  (SELECT a+7 AS i FROM t1) AS dt1 ) AS dt2 WHERE n > 100;

  The first call to Query_block::push_conditions_to_derived_tables would
  result in the following query. "n" in the where clause is
  replaced with (i+10).
  SELECT * FROM (SELECT i+10 AS n FROM
  (SELECT a+7 AS i FROM t1) AS dt1 WHERE (dt1.i+10) > 100) as dt2;

  The next call to Query_block::push_conditions_to_derived_tables should
  result in the following query. "i" is replaced with "a+7".
  SELECT * FROM (SELECT i+10 AS n FROM
  (SELECT a+7 AS i FROM t1 WHERE ((t1.a+7)+10) > 100) AS dt1) as dt2;

  However without cloned expressions, it would be

  SELECT * FROM (SELECT ((t1.a+7)+10) AS n FROM
  (SELECT a+7 AS i FROM t1 WHERE ((t1.a+7)+10) > 100) AS dt1) as dt2;

  Notice that the column "i" in derived table dt2 is getting replaced
  with (a+7) because the argument of the function in Item_func_plus
  in (i+10) is replaced with (a+7). The arguments to the function
  (i+10) need to be different so as to be able to replace them with
  some other expressions later.

  To clone an expression, we re-parse the expression to get another copy
  and resolve it against the tables of the query block where it will be
  placed.

  @param thd            Current thread
  @param item           Item for which clone is requested
  @param derived_table  derived table to which the item belongs to.

  @returns
  Cloned object for the item.
*/

Item *Query_block::clone_expression(THD *thd, Item *item,
                                    Table_ref *derived_table) {
  Item *cloned_item = parse_expression(thd, item, this, derived_table);
  if (cloned_item == nullptr) return nullptr;
  if (item->item_name.is_set())
    cloned_item->item_name.set(item->item_name.ptr(), item->item_name.length());

  // Collect details like table reference, field etc from the fields in the
  // original expression. Assign it to the corresponding field in the cloned
  // expression.
  if (copy_field_info(thd, item, cloned_item)) return nullptr;
  // A boolean expression to be cloned comes from a WHERE condition,
  // which treats UNKNOWN the same as FALSE, thus the cloned expression
  // should have the same property. apply_is_true() is ignored for
  // non-boolean expressions
  cloned_item->apply_is_true();
  return resolve_expression(thd, cloned_item, this);
}

/**
  Prepare a derived table or view for materialization.
  The derived table must have been
  - resolved by resolve_derived(),
  - or resolved as a subquery (by Item_*_subselect_::fix_fields()) then
  converted to a derived table.

  @param  thd   THD pointer

  @return false if successful, true if error
*/
bool Table_ref::setup_materialized_derived(THD *thd)

{
  return setup_materialized_derived_tmp_table(thd) ||
         derived->check_materialized_derived_query_blocks(thd);
}

/**
  Sets up the tmp table to contain the derived table's rows.
  @param  thd   THD pointer
  @return false if successful, true if error
*/
bool Table_ref::setup_materialized_derived_tmp_table(THD *thd)

{
  DBUG_TRACE;

  assert(is_view_or_derived() && !is_merged() && table == nullptr);

  DBUG_PRINT("info", ("algorithm: TEMPORARY TABLE"));

  Opt_trace_context *const trace = &thd->opt_trace;
  const Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_derived(trace, is_view() ? "view" : "derived");
  trace_derived.add_utf8_table(this)
      .add("select#", derived->first_query_block()->select_number)
      .add("materialized", true);

  set_uses_materialization();

  // From resolver POV, columns of this table are readonly
  set_readonly();

  if (m_common_table_expr && m_common_table_expr->tmp_tables.size() > 0) {
    trace_derived.add("reusing_tmp_table", true);
    table = m_common_table_expr->clone_tmp_table(thd, this);
    if (table == nullptr) return true; /* purecov: inspected */
    derived_result->table = table;
  }

  if (table == nullptr) {
    // Create the result table for the materialization
    const ulonglong create_options =
        derived->first_query_block()->active_options() | TMP_TABLE_ALL_COLUMNS;

    if (m_derived_column_names) {
      /*
        Tmp table's columns will be created from derived->types (the SELECT
        list), names included.
        But the user asked that the tmp table's columns use other specified
        names. So, we replace the names of SELECT list items with specified
        column names, just for the duration of tmp table creation.
      */
      swap_column_names_of_unit_and_tmp_table(*derived->get_unit_column_types(),
                                              *m_derived_column_names);
    }

    // If we're materializing directly into the result and we have a UNION
    // DISTINCT query, we're going to need a unique index for deduplication.
    // (If we're materializing into a temporary table instead, the deduplication
    // will happen on that table, and is not set here.) create_result_table()
    // will figure out whether it wants to create it as the primary key or just
    // a regular index.
    const bool is_distinct = derived->can_materialize_directly_into_result() &&
                             derived->has_top_level_distinct();

    const bool rc = derived_result->create_result_table(
        thd, *derived->get_unit_column_types(), is_distinct, create_options,
        alias, false, false);

    if (m_derived_column_names)  // Restore names
      swap_column_names_of_unit_and_tmp_table(*derived->get_unit_column_types(),
                                              *m_derived_column_names);

    if (rc) return true; /* purecov: inspected */

    table = derived_result->table;
    table->pos_in_table_list = this;
    if (m_common_table_expr && m_common_table_expr->tmp_tables.push_back(this))
      return true; /* purecov: inspected */
  }

  table->s->tmp_table = NON_TRANSACTIONAL_TMP_TABLE;

  // Table is "nullable" if inner table of an outer_join
  if (is_inner_table_of_outer_join()) table->set_nullable();

  dep_tables |= derived->m_lateral_deps;

  return false;
}

/**
  Sets up query blocks belonging to the query expression of a materialized
  derived table.
  @param  thd_arg   THD pointer
  @return false if successful, true if error
*/

bool Query_expression::check_materialized_derived_query_blocks(THD *thd_arg) {
  for (Query_block *sl = first_query_block(); sl; sl = sl->next_query_block()) {
    // All underlying tables are read-only
    sl->set_tables_readonly();
    /*
      Derived tables/view are materialized prior to UPDATE, thus we can skip
      them from table uniqueness check
    */
    sl->propagate_unique_test_exclusion();

    /*
      SELECT privilege is needed for all materialized derived tables and views,
      and columns must be marked for read.
    */
    if (sl->check_view_privileges(thd_arg, SELECT_ACL, SELECT_ACL)) return true;

    // Set all selected fields to be read:
    // @todo Do not set fields that are not referenced from outer query
    const Column_privilege_tracker tracker(thd_arg, SELECT_ACL);
    Mark_field mf(MARK_COLUMNS_READ);
    for (Item *item : sl->fields) {
      if (item->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                     (uchar *)thd_arg))
        return true;
      item->walk(&Item::mark_field_in_map, enum_walk::POSTFIX, (uchar *)&mf);
    }
  }
  return false;
}

/**
  Prepare a table function for materialization.

  @param  thd   THD pointer

  @return false if successful, true if error
*/
bool Table_ref::setup_table_function(THD *thd) {
  DBUG_TRACE;

  assert(is_table_function());

  DBUG_PRINT("info", ("algorithm: TEMPORARY TABLE"));

  Opt_trace_context *const trace = &thd->opt_trace;
  const Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_derived(trace, "table_function");
  const char *func_name;
  uint func_name_len;
  func_name = table_function->func_name();
  func_name_len = strlen(func_name);

  set_uses_materialization();

  /*
    A table function has name resolution context of query which owns FROM
    clause. So it automatically is LATERAL. This end_lateral_table is to
    make sure a table function won't access tables located after it in FROM
    clause.
  */
  query_block->end_lateral_table = this;

  if (table_function->init()) return true;

  // Create the result table for the materialization
  if (table_function->create_result_table(thd, 0LL, alias))
    return true; /* purecov: inspected */
  table = table_function->table;
  table->pos_in_table_list = this;

  table->s->tmp_table = NON_TRANSACTIONAL_TMP_TABLE;

  // Table is "nullable" if inner table of an outer_join
  if (is_inner_table_of_outer_join()) table->set_nullable();

  const char *saved_where = thd->where;
  thd->where = "a table function argument";
  const enum_mark_columns saved_mark = thd->mark_used_columns;
  thd->mark_used_columns = MARK_COLUMNS_READ;
  if (table_function->init_args()) return true;

  thd->mark_used_columns = saved_mark;
  set_privileges(SELECT_ACL);
  /*
    Trace needs to be here as it'ss print the table, and columns have to be
    set up at the moment of printing.
  */
  trace_derived.add_utf8_table(this)
      .add_utf8("function_name", func_name, func_name_len)
      .add("materialized", true);

  query_block->end_lateral_table = nullptr;

  thd->where = saved_where;

  return false;
}

/**
  Returns true if a condition can be pushed down to derived
  table based on some constraints.

  A condition cannot be pushed down to derived table if any of
  the following holds true:
  1. Hint and/or optimizer switch DERIVED_CONDITION_PUSHDOWN is off.
  2. If it has LIMIT - If the query expression underlying the derived
  table has LIMIT, then the pushed condition would affect the number
  of rows that would be fetched.
  3. It cannot be an inner table of an outer join - that would lead to
  more NULL-complemented rows.
  4. This cannot be a CTE having derived tables being referenced
  multiple times - there is only one temporary table for both references,
  if materialized ("shared materialization"). Also, we cannot push
  conditions down to CTEs that are recursive.
  5. If the derived query block has any user variable assignments -
  would affect the result of evaluating assignments to user variables
  in SELECT list of the derived table.
  6. The derived table stems from a scalar to derived table transformation
  which relies on cardinality check.
*/

bool Table_ref::can_push_condition_to_derived(THD *thd) {
  Query_expression const *unit = derived_query_expression();
  return hint_table_state(thd, this, DERIVED_CONDITION_PUSHDOWN_HINT_ENUM,
                          OPTIMIZER_SWITCH_DERIVED_CONDITION_PUSHDOWN) &&  // 1
         !unit->has_any_limit() &&                                         // 2
         !is_inner_table_of_outer_join() &&                                // 3
         !(common_table_expr() &&
           (common_table_expr()->references.size() >= 2 ||
            common_table_expr()->recursive)) &&     // 4
         (thd->lex->set_var_list.elements == 0) &&  // 5
         !unit->m_reject_multiple_rows;             // 6
}

/**
 Make a condition that can be pushed down to the derived table, and push it.

 @returns
  true if error
  false otherwise
*/
bool Condition_pushdown::make_cond_for_derived() {
  const Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_cond(trace, "condition_pushdown_to_derived");
  trace_cond.add_utf8_table(m_derived_table);
  trace_cond.add("original_condition", m_cond_to_check);

  Query_expression *derived_query_expression =
      m_derived_table->derived_query_expression();

  // Check if a part or full condition can be pushed down to the derived table.
  m_checking_purpose = CHECK_FOR_DERIVED;

  m_cond_to_push = extract_cond_for_table(m_cond_to_check);

  // Condition could not be pushed down to derived table (even partially)
  if (m_cond_to_push == nullptr) {
    m_remainder_cond = m_cond_to_check;
  } else {
    // Make the remainder condition that could not be pushed down. This is
    // left in the outer query block.
    if (make_remainder_cond(m_cond_to_check, &m_remainder_cond)) return true;
  }
  trace_cond.add("condition_to_push", m_cond_to_push);
  trace_cond.add("remaining_condition", m_remainder_cond);
  if (m_cond_to_push == nullptr) return false;

  const Opt_trace_array trace_steps(trace, "pushdown_to_query_blocks");
  Item *orig_cond_to_push = m_cond_to_push;
  for (Query_block *qb = derived_query_expression->first_query_block();
       qb != nullptr; qb = qb->next_query_block()) {
    // Make a copy that can be pushed to this query block
    if (derived_query_expression->is_set_operation()) {
      m_cond_to_push =
          derived_query_expression->outer_query_block()->clone_expression(
              thd, orig_cond_to_push, /*derived_table=*/nullptr);
      if (m_cond_to_push == nullptr) return true;
      m_cond_to_push->apply_is_true();
    }
    m_query_block = qb;

    // Analyze the condition that needs to be pushed, to push past window
    // functions and GROUP BY. The condition to be pushed, could be split
    // into HAVING condition, WHERE condition and remainder condition based
    // on the presence of window functions and GROUP BY.
    Opt_trace_object qb_wrapper(trace);

    qb_wrapper.add("query_block", m_query_block->select_number);
    if (push_past_window_functions()) return true;
    if (m_having_cond == nullptr) continue;
    if (push_past_group_by()) return true;
    qb_wrapper.add("pushed_having_condition", m_having_cond);
    qb_wrapper.add("pushed_where_condition", m_where_cond);
    qb_wrapper.add("remaining_condition", m_remainder_cond);

    // If this condition has a semi-join condition, remove expressions from
    // semi-join expression lists. Replace columns in the condition with
    // derived table expressions.
    if (m_having_cond != nullptr) {
      check_and_remove_sj_exprs(m_having_cond);
      if (replace_columns_in_cond(&m_having_cond, true)) return true;
    }
    if (m_where_cond != nullptr) {
      check_and_remove_sj_exprs(m_where_cond);
      if (replace_columns_in_cond(&m_where_cond, false)) return true;
    }

    // Attach the conditions to the derived table query block.
    if (m_having_cond &&
        attach_cond_to_derived(qb->having_cond(), m_having_cond, true))
      return true;
    if (m_where_cond &&
        attach_cond_to_derived(qb->where_cond(), m_where_cond, false))
      return true;
    m_where_cond = nullptr;
    m_having_cond = nullptr;
  }
  if (m_remainder_cond != nullptr && !m_remainder_cond->fixed &&
      m_remainder_cond->fix_fields(thd, &m_remainder_cond))
    return true;

  assert(!thd->is_error());
  return false;
}

/**
  This function is called multiple times to extract parts of a
  condition. To extract the condition, it performs certain checks
  and marks the condition accordingly.
  When the checking purpose is CHECK_FOR_DERIVED - it checks if
  all columns in a condition (fully or partially) are from the
  derived table.
  When the checking purpose is CHECK_FOR_HAVING - it checks if
  all columns in a condition (fully or partially) are part of
  PARTITION clause of window functions.
  When the checking purpose is CHECK_FOR_WHERE - it checks if
  all columns in a condition (fully or partially) are part of
  GROUP BY.

  If it is an "AND", a new AND condition is created and all the
  arguments to original AND condition which pass the above checks
  will be added as arguments to the new condition.
  If it is an OR, we can extract iff all the arguments pass the
  above checks.

  @param[in]  cond Condition that needs to be examined for extraction.

  @retval
  Condition that passes the checks.
  @retval
  nullptr if the condition does not pass checks.
*/

Item *Condition_pushdown::extract_cond_for_table(Item *cond) {
  cond->marker = Item::MARKER_NONE;
  if ((m_checking_purpose == CHECK_FOR_DERIVED) && cond->const_item()) {
    // There is no benefit in pushing a constant condition, we can as well
    // evaluate it at the top query's level.
    return nullptr;
  }
  // Make a new condition
  if (cond->type() == Item::COND_ITEM) {
    Item_cond *and_or_cond = down_cast<Item_cond *>(cond);
    if (and_or_cond->functype() == Item_func::COND_AND_FUNC) {
      Item_cond_and *new_cond = new (thd->mem_root) Item_cond_and;
      List_iterator<Item> li(*(and_or_cond)->argument_list());
      Item *item;
      uint n_marked = 0;
      while ((item = li++)) {
        Item *extracted_cond = extract_cond_for_table(item);
        if (extracted_cond)
          new_cond->argument_list()->push_back(extracted_cond);
        n_marked += (item->marker == Item::MARKER_COND_DERIVED_TABLE);
      }
      if (n_marked == and_or_cond->argument_list()->elements)
        and_or_cond->marker = Item::MARKER_COND_DERIVED_TABLE;
      switch (new_cond->argument_list()->elements) {
        case 0:
          return nullptr;
        case 1:
          return new_cond->argument_list()->head();
        default: {
          return new_cond;
        }
      }
    } else {
      Item_cond_or *new_cond = new (thd->mem_root) Item_cond_or;
      List_iterator<Item> li(*(and_or_cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *extracted_cond = extract_cond_for_table(item);
        if (item->marker != Item::MARKER_COND_DERIVED_TABLE) return nullptr;
        new_cond->argument_list()->push_back(extracted_cond);
      }
      and_or_cond->marker = Item::MARKER_COND_DERIVED_TABLE;
      return new_cond;
    }
  }

  // Perform checks
  if (m_checking_purpose == CHECK_FOR_DERIVED) {
    Derived_table_info dti(m_derived_table, m_query_block);
    // Check if the condition's used_tables() match that of the
    // derived table's. A constant expression is an exception.
    if ((cond->used_tables() & ~PSEUDO_TABLE_BITS) != m_derived_table->map() &&
        !cond->const_for_execution())
      return nullptr;
    // Examine the condition closely to see if it could be
    // pushed down to the derived table.
    if (cond->walk(&Item::is_valid_for_pushdown, enum_walk::POSTFIX,
                   pointer_cast<uchar *>(&dti)))
      return nullptr;
  } else if (m_checking_purpose == CHECK_FOR_HAVING) {
    if (cond->walk(&Item::check_column_in_window_functions, enum_walk::POSTFIX,
                   pointer_cast<uchar *>(m_query_block)))
      return nullptr;
  } else {
    if (cond->walk(&Item::check_column_in_group_by, enum_walk::POSTFIX,
                   pointer_cast<uchar *>(m_query_block)))
      return nullptr;
  }

  // Pushing in2exists conditions down into other query blocks
  // could cause them to get lost, as Item_subselect would not know
  // where to remove them from. They're a very rare case to have pushable,
  // so simply refuse pushing them.
  if (cond->created_by_in2exists()) {
    return nullptr;
  }

  // Mark the condition as it passed the checks
  cond->marker = Item::MARKER_COND_DERIVED_TABLE;
  return cond;
}

/**
 Get the expression from this query block using its position in
 the fields list of the derived table this query block is part of.
 Note that the field's position in a derived table does not always
 reflect the position in the visible field list of the query block.
 Creation of temporary table for a materialized derived table alters
 the field position whenever the temporary table adds a hidden field.

 @param[in] field_index  position in the fields list of the derived table.

 @returns expression from the derived table's query block.
*/

Item *Query_block::get_derived_expr(uint field_index) {
  // In some cases (noticed when derived table has multiple query blocks),
  // "field_index" does not always represent the index in the visible
  // field list. So, we adjust the index accordingly.
  Table_ref *derived_table = master_query_expression()->derived_table;
  uint adjusted_field_index =
      field_index - derived_table->get_hidden_field_count_for_derived();
  for (auto item : visible_fields())
    if (adjusted_field_index-- == 0) return item;

  assert(false);
  return nullptr;
}

/**
  Try to push past window functions into the HAVING clause of the
  derived table. Check that all columns in the condition are present
  as window partition columns in all the window functions of the
  current query block. If not, the condition cannot be pushed down
  to derived table.
  @todo
  Introduce another condition (like WHERE and HAVING) which can be
  used to filter after window function execution.
*/
bool Condition_pushdown::push_past_window_functions() {
  if (m_query_block->m_windows.elements == 0) {
    m_having_cond = m_cond_to_push;
    return false;
  }
  m_checking_purpose = CHECK_FOR_HAVING;
  Opt_trace_object step_wrapper(trace, "pushing_past_window_functions");
  m_having_cond = extract_cond_for_table(m_cond_to_push);
  Item *r_cond = nullptr;
  if (m_having_cond != nullptr) {
    if (make_remainder_cond(m_cond_to_push, &r_cond)) return true;
  } else
    r_cond = m_cond_to_push;

  if (r_cond != nullptr) m_remainder_cond = and_items(m_remainder_cond, r_cond);
  step_wrapper.add("condition_to_push_to_having", m_having_cond);
  step_wrapper.add("remaining_condition", m_remainder_cond);
  return false;
}

/**
  Try to push the condition or parts of the condition past GROUP BY into
  the WHERE clause of the derived table.
  1. For a non-grouped query, the condition is moved to the WHERE clause.
  2. For an implicitly grouped query, condition remains in the HAVING
     clause in order to preserve semantics.
  3. For a query with ROLLUP, the condition will remain in the HAVING
     clause because ROLLUP might add NULL values to the grouping columns.
  4. For other grouped queries, predicates involving grouping columns
     can be moved to the WHERE clause. Predicates that reference aggregate
     functions remain in HAVING clause.
  We perform the same checks for a non-standard compliant GROUP BY too.
  If a window function's PARTITION BY clause is on non-grouping columns
  (possible if GROUP BY is non-standard compliant or when these columns
   are functionally dependednt on the grouping columns) then the condition
  will stay in HAVING clause.
*/
bool Condition_pushdown::push_past_group_by() {
  if (!m_query_block->is_grouped()) {
    m_where_cond = m_having_cond;
    m_having_cond = nullptr;
    return false;
  }
  if (m_query_block->is_implicitly_grouped() ||
      m_query_block->is_non_primitive_grouped())
    return false;
  m_checking_purpose = CHECK_FOR_WHERE;
  Opt_trace_object step_wrapper(trace, "pushing_past_group_by");

  m_where_cond = extract_cond_for_table(m_having_cond);
  Item *remainder_cond = nullptr;
  if (m_where_cond != nullptr) {
    if (make_remainder_cond(m_having_cond, &remainder_cond)) return true;
    m_having_cond = remainder_cond;
  }

  step_wrapper.add("condition_to_push_to_having", m_having_cond);
  step_wrapper.add("condition_to_push_to_where", m_where_cond);
  step_wrapper.add("remaining_condition", m_remainder_cond);
  return false;
}

/**
  Make the remainder condition. Any part of the condition that is not
  marked will be made into a independent condition.

  @param[in]      cond           condition to look into for the marker
  @param[in,out]  remainder_cond condition that is not marked

  @returns
   true on error, false otherwise
*/

bool Condition_pushdown::make_remainder_cond(Item *cond,
                                             Item **remainder_cond) {
  if (cond->marker ==
      Item::MARKER_COND_DERIVED_TABLE)  // This condition is marked
    return false;

  if (cond->type() == Item::COND_ITEM &&
      ((down_cast<Item_cond *>(cond))->functype() ==
       Item_func::COND_AND_FUNC)) {
    /// Create new top level AND item
    Item_cond_and *new_cond = new (thd->mem_root) Item_cond_and;
    if (new_cond == nullptr) return true;
    List_iterator<Item> li(*(down_cast<Item_cond *>(cond))->argument_list());
    Item *item;
    while ((item = li++)) {
      Item *r_cond = nullptr;
      if (make_remainder_cond(item, &r_cond)) return true;
      if (r_cond != nullptr) new_cond->argument_list()->push_back(r_cond);
    }
    switch (new_cond->argument_list()->elements) {
      case 0:
        return false;
      case 1:
        if (new_cond->fix_fields(thd, reinterpret_cast<Item **>(&new_cond)))
          return true;
        *remainder_cond = new_cond->argument_list()->head();
        return false;
      default:
        if (new_cond->fix_fields(thd, reinterpret_cast<Item **>(&new_cond)))
          return true;
        *remainder_cond = new_cond;
        return false;
    }
  }
  *remainder_cond = cond;
  return false;
}

/**
 Replace columns in a condition that will be pushed to this derived table
 with the derived table expressions.

 If there is a HAVING condition that needs to be pushed down, we replace
 columns in the condition with references to the corresponding derived table
 expressions and for WHERE condition, we replace columns with derived table
 expressions.
*/

bool Condition_pushdown::replace_columns_in_cond(Item **cond, bool is_having) {
  // For a view reference, the underlying expression could be shared if the
  // expression is referenced elsewhere in the query. So we clone the expression
  // before replacing it with derived table expression.
  bool view_ref = false;
  WalkItem((*cond), enum_walk::PREFIX, [&view_ref](Item *inner_item) {
    if (inner_item->type() == Item::REF_ITEM &&
        down_cast<Item_ref *>(inner_item)->ref_type() == Item_ref::VIEW_REF) {
      view_ref = true;
      return true;
    }
    return false;
  });
  Derived_table_info dti(m_derived_table, m_query_block);

  if (view_ref) {
    (*cond) = (*cond)->transform(&Item::replace_view_refs_with_clone,
                                 pointer_cast<uchar *>(&dti));
    if (*cond == nullptr) return true;
  }
  Item *new_cond =
      is_having ? (*cond)->transform(&Item::replace_with_derived_expr_ref,
                                     pointer_cast<uchar *>(&dti))
                : (*cond)->transform(&Item::replace_with_derived_expr,
                                     pointer_cast<uchar *>(&dti));
  if (new_cond == nullptr) return true;
  new_cond->update_used_tables();
  (*cond) = new_cond;
  return false;
}

/**
  Check if this derived table is part of a semi-join. If so, we might
  be pushing down a semi-join condition attached to the outer where condition.
  We need to remove the expressions that are part of such a condition from
  semi-join inner/outer expression lists. Otherwise, once the columns
  of the semi-join condition get replaced with derived table expressions,
  these lists will also be pointing to the derived table expressions which is
  not correct. Updating the lists is also natural: the condition is pushed down,
  so it's not to be tested on the outer level anymore; leaving it in the
  list would make it be tested on the outer level.
  Once this function determines that this table is part of a semi-join, it
  calls remove_sj_exprs() to remove expressions found in the condition
  from semi-join expressions lists.
  Note that sj_inner_tables, sj_depends_on, sj_corr_tables are not updated,
  which may make us miss some semi-join strategies, but is not critical.
*/

void Condition_pushdown::check_and_remove_sj_exprs(Item *cond) {
  // To check for all the semi-join outer expressions that could be part of
  // the condition.
  if (m_derived_table->join_list) {
    for (Table_ref *tl : *m_derived_table->join_list) {
      if (tl->is_sj_or_aj_nest()) remove_sj_exprs(cond, tl->nested_join);
    }
  }
  // To check for all the semi-join inner expressions that could be part of
  // the condition.
  if (m_derived_table->embedding &&
      m_derived_table->embedding->is_sj_or_aj_nest()) {
    remove_sj_exprs(cond, m_derived_table->embedding->nested_join);
  }
}

/**
  This function examines the condition that is being pushed down to see
  if the expressions from the condition are a match for inner/outer expressions
  of the semi-join. If its a match, it removes such expressions from these
  expression lists.

  @param[in]     cond    condition that needs to be looked into
  @param[in,out] sj_nest semi-join nest from where the inner/outer expressions
  are being matched to the expressions from "cond"

*/
void Condition_pushdown::remove_sj_exprs(Item *cond, NESTED_JOIN *sj_nest) {
  if (cond->type() == Item::COND_ITEM) {
    Item_cond *cond_item = down_cast<Item_cond *>(cond);
    List_iterator<Item> li(*cond_item->argument_list());
    Item *item;
    while ((item = li++)) remove_sj_exprs(item, sj_nest);
  } else if ((cond->type() == Item::FUNC_ITEM &&
              down_cast<Item_func *>(cond)->functype() == Item_func::EQ_FUNC)) {
    // We found a possible semi-join condition which is of the form
    // "outer_expr = inner_expr" (as created by build_sj_cond())
    auto it_o = sj_nest->sj_outer_exprs.begin();
    auto it_i = sj_nest->sj_inner_exprs.begin();
    while (it_i != sj_nest->sj_inner_exprs.end() &&
           it_o != sj_nest->sj_outer_exprs.end()) {
      Item *outer = *it_o, *inner = *it_i;
      // Check if the arguments of the equality match with expressions in the
      // lists. If so, remove them from the lists.
      if (outer == down_cast<Item_func_eq *>(cond)->get_arg(0) &&
          inner == down_cast<Item_func_eq *>(cond)->get_arg(1)) {
        it_i = sj_nest->sj_inner_exprs.erase(it_i);
        it_o = sj_nest->sj_outer_exprs.erase(it_o);
        if (sj_nest->sj_inner_exprs.empty()) {
          assert(sj_nest->sj_outer_exprs.empty());
          // Materialization needs non-empty lists (same as in
          // Query_block::build_sj_cond())
          Item *const_item = new Item_int(1);
          sj_nest->sj_inner_exprs.push_back(const_item);
          sj_nest->sj_outer_exprs.push_back(const_item);
        }
        break;
      }
      ++it_i;
      ++it_o;
    }
  }
}

/**
  Increment cond_count and between_count in the derived table query block
  based on the number of BETWEEN predicates and number of other predicates
  pushed down.
*/
void Condition_pushdown::update_cond_count(Item *cond) {
  if (cond->type() == Item::COND_ITEM) {
    Item_cond *cond_item = down_cast<Item_cond *>(cond);
    List_iterator<Item> li(*cond_item->argument_list());
    Item *item;
    while ((item = li++)) update_cond_count(item);
  } else if ((cond->type() == Item::FUNC_ITEM &&
              down_cast<Item_func *>(cond)->functype() == Item_func::BETWEEN))
    m_query_block->between_count++;
  else {
    m_query_block->cond_count++;
  }
}

/**
  Attach condition to derived table query block.

  @param[in] derived_cond   condition in derived table to which
                            another condition needs to be attached.
  @param[in] cond_to_attach condition that needs to be attached to
                            the derived table query block.
  @param[in] having         true if this is having condition, false
                            if it is the where condition.

  @retval
  true if error
  @retval
  false on success
*/
bool Condition_pushdown::attach_cond_to_derived(Item *derived_cond,
                                                Item *cond_to_attach,
                                                bool having) {
  Query_block *saved_query_block = thd->lex->current_query_block();
  thd->lex->set_current_query_block(m_query_block);
  const bool fix_having = m_query_block->having_fix_field;

  derived_cond = and_items(derived_cond, cond_to_attach);
  // Need to call setup_ftfuncs() if we are going to push
  // down a condition having full text function.
  if (m_query_block->has_ft_funcs() &&
      contains_function_of_type(cond_to_attach, Item_func::FT_FUNC)) {
    if (setup_ftfuncs(thd, m_query_block)) {
      return true;
    }
  }
  if (having) m_query_block->having_fix_field = true;
  if (!derived_cond->fixed && derived_cond->fix_fields(thd, &derived_cond)) {
    m_query_block->having_fix_field = fix_having;
    thd->lex->set_current_query_block(saved_query_block);
    return true;
  }
  m_query_block->having_fix_field = fix_having;
  update_cond_count(cond_to_attach);
  having ? m_query_block->set_having_cond(derived_cond)
         : m_query_block->set_where_cond(derived_cond);
  thd->lex->set_current_query_block(saved_query_block);
  return false;
}

/**
  Optimize the query expression representing a derived table/view.

  @note
  If optimizer finds out that the derived table/view is of the type
  "SELECT a_constant" this functions also materializes it.

  @param thd thread handle

  @returns false if success, true if error.
*/

bool Table_ref::optimize_derived(THD *thd) {
  DBUG_TRACE;

  Query_expression *const unit = derived_query_expression();

  assert(unit && !unit->is_optimized());

  if (!table->has_storage_handler()) {
    Derived_refs_iterator ref_it(this);
    TABLE *t;
    while ((t = ref_it.get_next())) {
      if (setup_tmp_table_handler(thd, t,
                                  unit->first_query_block()->active_options() |
                                      TMP_TABLE_ALL_COLUMNS))
        return true; /* purecov: inspected */
      t->set_not_started();
    }
  }

  if (unit->optimize(thd, table,
                     /*finalize_access_paths=*/true) ||
      thd->is_error())
    return true;

  // If the table is const, materialize it now. The hypergraph optimizer
  // doesn't care about const tables, though, so it prefers to do this
  // at execution time (in fact, it will get confused and crash if it has
  // already been materialized).
  if (!thd->lex->using_hypergraph_optimizer()) {
    if (materializable_is_const() &&
        (create_materialized_table(thd) || materialize_derived(thd)))
      return true;
  }

  return false;
}

/**
  Create result table for a materialized derived table/view/table function.

  @param thd     thread handle

  This function actually creates the result table for given 'derived'
  table/view, but it doesn't fill it.

  @returns false if success, true if error.
*/

bool Table_ref::create_materialized_table(THD *thd) {
  DBUG_TRACE;

  // @todo: Be able to assert !table->is_created() as well
  assert((is_table_function() || derived_query_expression()) &&
         uses_materialization() && table);

  if (!table->is_created()) {
    Derived_refs_iterator it(this);
    while (TABLE *t = it.get_next())
      if (t->is_created()) {
        assert(table->in_use == nullptr || table->in_use == thd);
        table->in_use = thd;
        if (open_tmp_table(table)) return true; /* purecov: inspected */
        break;
      }
  }

  /*
    Don't create result table if:
    1) Table is already created, or
    2) Table is a constant one with all NULL values.
  */
  if (table->is_created() ||                           // 1
      (query_block->join != nullptr &&                 // 2
       (query_block->join->const_table_map & map())))  // 2
  {
    /*
      At this point, a const table should have null rows.
      Exception being a shared CTE.
    */
#ifndef NDEBUG
    QEP_TAB *tab = table->reginfo.qep_tab;
    assert((common_table_expr() != nullptr &&
            common_table_expr()->references.size() > 1) ||
           tab == nullptr || tab->type() != JT_CONST || table->has_null_row());
#endif
    return false;
  }
  /* create tmp table */
  if (instantiate_tmp_table(thd, table)) return true; /* purecov: inspected */

  table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);

  return false;
}

/**
  Materialize derived table

  @param  thd	    Thread handle

  Derived table is resolved with temporary table. It is created based on the
  queries defined. After temporary table is materialized, if this is not
  EXPLAIN, then the entire unit / node is deleted. unit is deleted if UNION is
  used for derived table and node is deleted is it is a  simple SELECT.
  If you use this function, make sure it's not called at prepare.
  Due to evaluation of LIMIT clause it can not be used at prepared stage.

  @returns false if success, true if error.
*/

bool Table_ref::materialize_derived(THD *thd) {
  DBUG_TRACE;
  assert(is_view_or_derived() && uses_materialization());
  assert(table && table->is_created() && !table->materialized);

  Derived_refs_iterator it(this);
  while (TABLE *t = it.get_next())
    if (t->materialized) {
      table->materialized = true;
      table->set_not_started();
      return false;
    }

  /*
    The with-recursive algorithm needs the table scan to return rows in
    insertion order.
    For MEMORY and Temptable it is true.
    For InnoDB: InnoDB's table scan returns rows in PK order. If the PK
    is (not) the autogenerated autoincrement InnoDB ROWID, PK order will (not)
    be the same as insertion order.
    So let's verify that the table has no MySQL-created PK.
  */
  Query_expression *const unit = derived_query_expression();
  if (unit->is_recursive()) {
    assert(table->s->primary_key == MAX_KEY);
  }

  if (table->hash_field) {
    table->file->ha_index_init(0, false);
  }

  // execute unit without cleaning up
  if (unit->force_create_iterators(thd)) {
    return true;
  }
  bool res = unit->execute(thd);

  if (table->hash_field) {
    table->file->ha_index_or_rnd_end();
  }

  if (!res) {
    /*
      Here we entirely fix both Table_ref and list of SELECT's as
      there were no derived tables
    */
    if (derived_result->flush()) res = true; /* purecov: inspected */
  }

  table->materialized = true;

  // Mark the table as not started (default is just zero status),
  // or read_system() and read_const() will forget to read the row.
  table->set_not_started();

  return res;
}
