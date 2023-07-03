/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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
  @file

  @brief
  Sum functions (COUNT, MIN...)
*/

#include "sql/item_sum.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <utility>  // std::forward

#include "decimal.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_compare.h"
#include "my_dbug.h"
#include "my_double2ulonglong.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql-common/json_dom.h"
#include "sql/aggregate_check.h"  // Distinct_check
#include "sql/create_field.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/dd/cache/dictionary_client.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometry_extraction.h"
#include "sql/gis/relops.h"
#include "sql/handler.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_json_func.h"
#include "sql/item_subselect.h"
#include "sql/key_spec.h"
#include "sql/mysqld.h"
#include "sql/parse_tree_helpers.h"    // PT_item_list
#include "sql/parse_tree_node_base.h"  // Parse_context
#include "sql/parse_tree_nodes.h"      // PT_order_list
#include "sql/parser_yystype.h"
#include "sql/sql_array.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_resolver.h"  // setup_order
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/srs_fetcher.h"    // Srs_fetcher
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"  // Temp_table_param
#include "sql/uniques.h"           // Unique
#include "sql/window.h"

using std::max;
using std::min;

bool Item_sum::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (Item_result_field::itemize(pc, res)) return true;

  if (m_window) {
    if (m_window->contextualize(pc)) return true; /* purecov: inspected */
    if (!m_window->is_reference()) {
      pc->select->m_windows.push_back(m_window);
      m_window->set_def_pos(pc->select->m_windows.elements);
    }
    m_is_window_function = true;
    pc->select->n_sum_items++;
    set_wf();
  } else {
    mark_as_sum_func(pc->select);
    pc->select->in_sum_expr++;
  }

  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->itemize(pc, &args[i])) return true;
  }

  if (!m_window) pc->select->in_sum_expr--;

  return false;
}

/**
  Calculate the affordable RAM limit for structures like TREE or Unique
  used in Item_sum_*
*/

ulonglong Item_sum::ram_limitation(THD *thd) {
  ulonglong limitation =
      min(thd->variables.tmp_table_size, thd->variables.max_heap_table_size);

  DBUG_EXECUTE_IF("simulate_low_itemsum_ram_limitation", limitation = 32;);

  return limitation;
}

/**
  Prepare an aggregate function for checking of context.

    The function initializes the members of the Item_sum object.
    It also checks the general validity of the set function:
    If none of the currently active query blocks allow evaluation of
    set functions, an error is reported.

  @note
    This function must be called for all set functions when expressions are
    resolved. It must be invoked in prefix order, ie at the descent of this
    traversal. @see corresponding Item_sum::check_sum_func(), which should
    be called on ascent.

  @param thd      reference to the thread context info

  @returns false if success, true if error
*/

bool Item_sum::init_sum_func_check(THD *thd) {
  if (m_is_window_function) {
    /*
      Are either no aggregates of any kind allowed at this level, or
      specifically not window functions?
    */
    LEX *const lex = thd->lex;
    if (((~lex->allow_sum_func | lex->m_deny_window_func) >>
         lex->current_query_block()->nest_level) &
        0x1) {
      my_error(ER_WINDOW_INVALID_WINDOW_FUNC_USE, MYF(0), func_name());
      return true;
    }
    in_sum_func = nullptr;
  } else {
    if (!thd->lex->allow_sum_func) {
      my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
      return true;
    }
    // Set a reference to the containing set function if there is one
    in_sum_func = thd->lex->in_sum_func;
    /*
      Set this object as the current containing set function, used when
      checking arguments of this set function.
    */
    thd->lex->in_sum_func = this;
  }
  save_deny_window_func = thd->lex->m_deny_window_func;
  thd->lex->m_deny_window_func |=
      (nesting_map)1 << thd->lex->current_query_block()->nest_level;
  // @todo: When resolving once, move following code to constructor
  base_query_block = thd->lex->current_query_block();
  aggr_query_block = nullptr;  // Aggregation query block is undetermined yet
  referenced_by[0] = nullptr;
  /*
    Leave referenced_by[1] unchanged as in execution of PS, in-to-exists is not
    re-done, so referenced_by[1] isn't set again. So keep it as it was in
    preparation.
  */
  if (thd->lex->current_query_block()->first_execution)
    referenced_by[1] = nullptr;
  max_aggr_level = -1;
  max_sum_func_level = -1;
  used_tables_cache = 0;
  return false;
}

/**
  Validate the semantic requirements of a set function.

    Check whether the context of the set function allows it to be aggregated
    and, when it is an argument of another set function, directly or indirectly,
    the function makes sure that these two set functions are aggregated in
    different query blocks.
    If the context conditions are not met, an error is reported.
    If the set function is aggregated in some outer query block, it is
    added to the chain of items inner_sum_func_list attached to the
    aggregating query block.

    A number of designated members of the object are used to check the
    conditions. They are specified in the comment before the Item_sum
    class declaration.
    Additionally a bitmap variable called allow_sum_func is employed.
    It is included into the LEX structure.
    The bitmap contains 1 at n-th position if the query block at level "n"
    allows a set function reference (i.e the current resolver context for
    the query block is either in the SELECT list or in the HAVING or
    ORDER BY clause).

    Consider the query:
    @code
       SELECT SUM(t1.b) FROM t1 GROUP BY t1.a
         HAVING t1.a IN (SELECT t2.c FROM t2 WHERE AVG(t1.b) > 20) AND
                t1.a > (SELECT MIN(t2.d) FROM t2);
    @endcode
    when the set functions are resolved, allow_sum_func will contain:
    - for SUM(t1.b) - 1 at position 0 (SUM is in SELECT list)
    - for AVG(t1.b) - 1 at position 0 (subquery is in HAVING clause)
                      0 at position 1 (AVG is in WHERE clause)
    - for MIN(t2.d) - 1 at position 0 (subquery is in HAVING clause)
                      1 at position 1 (MIN is in SELECT list)

  @note
    This function must be called for all set functions when expressions are
    resolved. It must be invoked in postfix order, ie at the ascent of this
    traversal.

  @param thd  reference to the thread context info
  @param ref  location of the pointer to this item in the containing expression

  @returns false if success, true if error
*/

bool Item_sum::check_sum_func(THD *thd, Item **ref) {
  DBUG_TRACE;

  if (m_is_window_function) {
    update_used_tables();
    thd->lex->m_deny_window_func = save_deny_window_func;
    return false;
  }

  const nesting_map allow_sum_func = thd->lex->allow_sum_func;
  const nesting_map nest_level_map = (nesting_map)1
                                     << base_query_block->nest_level;

  assert(thd->lex->current_query_block() == base_query_block);
  assert(aggr_query_block == nullptr);

  /*
    max_aggr_level is the level of the innermost qualifying query block of
    the column references of this set function. If the set function contains
    no column references, max_aggr_level is -1.
    max_aggr_level cannot be greater than nest level of the current query block.
  */
  assert(max_aggr_level <= base_query_block->nest_level);

  if (base_query_block->nest_level == max_aggr_level) {
    /*
      The function must be aggregated in the current query block,
      and it must be referred within a clause where it is valid
      (ie. HAVING clause, ORDER BY clause or SELECT list)
    */
    if ((allow_sum_func & nest_level_map) != 0)
      aggr_query_block = base_query_block;
  } else if (max_aggr_level >= 0 || !(allow_sum_func & nest_level_map)) {
    /*
      Look for an outer query block where the set function should be
      aggregated. If it finds such a query block, then aggr_query_block is set
      to this query block
    */
    for (Query_block *sl = base_query_block->outer_query_block();
         sl && sl->nest_level >= max_aggr_level; sl = sl->outer_query_block()) {
      if (allow_sum_func & ((nesting_map)1 << sl->nest_level))
        aggr_query_block = sl;
    }
  } else  // max_aggr_level < 0
  {
    /*
      Set function without column reference is aggregated in innermost query,
      without any validation.
    */
    aggr_query_block = base_query_block;
  }

  if (aggr_query_block == nullptr && (allow_sum_func & nest_level_map) != 0 &&
      !(thd->variables.sql_mode & MODE_ANSI))
    aggr_query_block = base_query_block;

  /*
    At this place a query block where the set function is to be aggregated
    has been found and is assigned to aggr_query_block, or aggr_query_block is
    NULL to indicate an invalid set function.

    Additionally, check whether possible nested set functions are acceptable
    here: their aggregation level must be greater than this set function's
    aggregation level.
  */
  if (aggr_query_block == nullptr ||
      aggr_query_block->nest_level <= max_sum_func_level) {
    my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
    return true;
  }

  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->has_aggregation() &&
        WalkItem(args[i], enum_walk::SUBQUERY_POSTFIX, [this](Item *subitem) {
          if (subitem->type() != Item::SUM_FUNC_ITEM) return false;
          Item_sum *si = down_cast<Item_sum *>(subitem);
          return si->aggr_query_block == this->aggr_query_block;
        })) {
      my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
      return true;
    }
  }

  if (aggr_query_block != base_query_block) {
    referenced_by[0] = ref;
    /*
      Add the set function to the list inner_sum_func_list for the
      aggregating query block.

      @note
        Now we 'register' only set functions that are aggregated in outer
        query blocks. Actually it makes sense to link all set functions for
        a query block in one chain. It would simplify the process of 'splitting'
        for set functions.
    */
    if (!aggr_query_block->inner_sum_func_list)
      next_sum = this;
    else {
      next_sum = aggr_query_block->inner_sum_func_list->next_sum;
      aggr_query_block->inner_sum_func_list->next_sum = this;
    }
    aggr_query_block->inner_sum_func_list = this;
    aggr_query_block->with_sum_func = true;

    /*
      Mark subqueries as containing set function all the way up to the
      set function's aggregation query block.
      Note that we must not mark the Item of calculation context itself
      because with_sum_func on the aggregation query block is already set above.

      has_aggregation() being set for an Item means that this Item refers
      (somewhere in it, e.g. one of its arguments if it's a function) directly
      or indirectly to a set function that is calculated in a
      context "outside" of the Item (e.g. in the current or outer query block).

      with_sum_func being set for a query block means that this query block
      has set functions directly referenced (i.e. not through a subquery).

      If, going up, we meet a derived table, we do nothing special for it:
      it doesn't need this information.
    */
    for (Query_block *sl = base_query_block; sl && sl != aggr_query_block;
         sl = sl->outer_query_block()) {
      if (sl->master_query_expression()->item)
        sl->master_query_expression()->item->set_aggregation();
    }

    base_query_block->mark_as_dependent(aggr_query_block, true);
  }

  if (in_sum_func) {
    /*
      If the set function is nested adjust the value of
      max_sum_func_level for the containing set function.
      We take into account only set functions that are to be aggregated on
      the same level or outer compared to the nest level of the containing
      set function.
      But we must always pass up the max_sum_func_level because it is
      the maximum nest level of all directly and indirectly contained
      set functions. We must do that even for set functions that are
      aggregated inside of their containing set function's nest level
      because the containing function may contain another containing
      function that is to be aggregated outside or on the same level
      as its parent's nest level.
    */
    if (in_sum_func->base_query_block->nest_level >=
        aggr_query_block->nest_level)
      in_sum_func->max_sum_func_level = max(in_sum_func->max_sum_func_level,
                                            int8(aggr_query_block->nest_level));
    in_sum_func->max_sum_func_level =
        max(in_sum_func->max_sum_func_level, max_sum_func_level);
  }

  aggr_query_block->set_agg_func_used(true);
  if (sum_func() == JSON_AGG_FUNC)
    aggr_query_block->set_json_agg_func_used(true);
  update_used_tables();
  thd->lex->in_sum_func = in_sum_func;
  thd->lex->m_deny_window_func = save_deny_window_func;

  return false;
}

bool Item_sum::check_wf_semantics1(THD *, Query_block *,
                                   Window_evaluation_requirements *r) {
  const PT_frame *frame = m_window->frame();

  /*
    If we have ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW, we can just
    accumulate as we see rows, never need to invert old rows or to look at
    future rows, so don't need a frame buffer.
  */
  r->needs_buffer = !(frame->m_query_expression == WFU_ROWS &&
                      frame->m_from->m_border_type == WBT_UNBOUNDED_PRECEDING &&
                      frame->m_to->m_border_type == WBT_CURRENT_ROW);

  if (with_distinct) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "<window function>(DISTINCT ..)");
    return true;
  }
  return false;
}

Item_sum::Item_sum(const POS &pos, PT_item_list *opt_list, PT_window *w)
    : Item_func(pos, opt_list), m_window(w) {}

/**
  Constructor used in processing select with temporary tebles.
*/

Item_sum::Item_sum(THD *thd, const Item_sum *item)
    : Item_func(thd, item),
      m_window(item->m_window),
      base_query_block(item->base_query_block),
      aggr_query_block(item->aggr_query_block),
      allow_group_via_temp_table(item->allow_group_via_temp_table),
      forced_const(item->forced_const) {
  assert(arg_count == item->arg_count);
  with_distinct = item->with_distinct;
  if (item->aggr) {
    Item_sum::set_aggregator(item->aggr->Aggrtype());
  }
  assert(!m_is_window_function);  // WF items are never copied
}

void Item_sum::mark_as_sum_func() {
  mark_as_sum_func(current_thd->lex->current_query_block());
}

void Item_sum::mark_as_sum_func(Query_block *cur_query_block) {
  cur_query_block->n_sum_items++;
  cur_query_block->with_sum_func = true;
  set_aggregation();
}

void Item_sum::print(const THD *thd, String *str,
                     enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');
  if (has_with_distinct()) str->append("distinct ");

  for (uint i = 0; i < arg_count; i++) {
    if (i) str->append(',');
    args[i]->print(thd, str, query_type);
  }
  str->append(')');

  if (m_window) {
    str->append(" OVER ");
    m_window->print(thd, str, query_type, false);
  }
}

bool Item_sum::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, -1)) return true;

  set_nullable(true);
  null_value = true;

  const Sumfunctype t = sum_func();

  // None except these 4 types are allowed for geometry arguments.
  if (!(t == COUNT_FUNC || t == COUNT_DISTINCT_FUNC || t == SUM_BIT_FUNC ||
        t == GEOMETRY_AGGREGATE_FUNC))
    return reject_geometry_args(arg_count, args, this);
  return false;
}

/**
  Remove the item from the list of inner aggregation functions in the
  Query_block it was moved to by Item_sum::check_sum_func().

  This is done to undo some of the effects of Item_sum::check_sum_func() so
  that the item may be removed from the query.

  @note This doesn't completely undo Item_sum::check_sum_func(), as
  aggregation information is left untouched. This means that if this
  item is removed, aggr_query_block and all subquery items between
  aggr_query_block and this item may be left with has_aggregation() set to true,
  even if there are no aggregation functions. To our knowledge, this has no
  impact on the query result.

  @see Item_sum::check_sum_func()
  @see remove_redundant_subquery_clauses()

  If this is a window function, remove the reference from the window.
  This is needed when constant predicates are being removed.

  @see Item_cond::fix_fields()
  @see Item_cond::remove_const_cond()
 */
bool Item_sum::clean_up_after_removal(uchar *arg) {
  Cleanup_after_removal_context *const ctx =
      pointer_cast<Cleanup_after_removal_context *>(arg);

  if (ctx->is_stopped(this)) return false;

  // Remove item on upward traversal, not downward:
  if (marker == MARKER_NONE) {
    marker = MARKER_TRAVERSAL;
    return false;
  }
  assert(marker == MARKER_TRAVERSAL);
  marker = MARKER_NONE;

  /*
    Don't do anything if
    1) this is an unresolved item (This may happen if an
       expression occurs twice in the same query. In that case, the
       whole item tree for the second occurrence is replaced by the
       item tree for the first occurrence, without calling fix_fields()
       on the second tree. Therefore there's nothing to clean up.), or
    If it is a grouped aggregate,
    2) there is no inner_sum_func_list, or
    3) the item is not an element in the inner_sum_func_list.
  */
  if (!fixed ||  // 1
      (m_window == nullptr &&
       (aggr_query_block == nullptr ||
        aggr_query_block->inner_sum_func_list == nullptr  // 2
        || next_sum == nullptr)))                         // 3
    return false;

  if (m_window) {
    // Cleanup the reference for this window function from m_functions
    List_iterator<Item_sum> li(m_window->functions());
    Item *item = nullptr;
    while ((item = li++)) {
      if (item == this) {
        li.remove();
        break;
      }
    }
  } else {
    if (next_sum == this)
      aggr_query_block->inner_sum_func_list = nullptr;
    else {
      Item_sum *prev;
      for (prev = this; prev->next_sum != this; prev = prev->next_sum)
        ;
      prev->next_sum = next_sum;
      next_sum = nullptr;

      if (aggr_query_block->inner_sum_func_list == this)
        aggr_query_block->inner_sum_func_list = prev;
    }
    // Replace the removed item with a NULL value. Perform a replace rather
    // than a removal so that the size of the array stays the same. A hidden
    // NULL value will not affect processing of the query block.
    for (size_t i = 0; i < aggr_query_block->fields.size(); i++) {
      if (aggr_query_block->fields[i] == this) {
        Item_null *null_item = new Item_null();
        null_item->hidden = true;
        aggr_query_block->fields[i] = null_item;
        break;
      }
    }
  }

  return false;
}

/// @note Please keep in sync with Item_func::eq().
bool Item_sum::eq(const Item *item, bool binary_cmp) const {
  /* Assume we don't have rtti */
  if (this == item) return true;
  if (item->type() != type() ||
      item->m_is_window_function != m_is_window_function)
    return false;
  const Item_sum *item_sum = down_cast<const Item_sum *>(item);
  const enum Sumfunctype my_sum_func = sum_func();
  if (item_sum->sum_func() != my_sum_func || item_sum->m_window != m_window)
    return false;

  if (is_rollup_sum_wrapper() || item_sum->is_rollup_sum_wrapper()) {
    // we want to compare underlying Item_sums
    const Item_sum *this_real_sum = unwrap_sum();
    const Item_sum *item_real_sum = item_sum->unwrap_sum();
    return this_real_sum->eq(item_real_sum, binary_cmp);
  }

  if (arg_count != item_sum->arg_count ||
      (my_sum_func != Item_sum::UDF_SUM_FUNC &&
       strcmp(func_name(), item_sum->func_name()) != 0) ||
      (my_sum_func == Item_sum::UDF_SUM_FUNC &&
       my_strcasecmp(system_charset_info, func_name(), item_sum->func_name())))
    return false;
  return AllItemsAreEqual(args, item_sum->args, arg_count, binary_cmp);
}

bool Item_sum::aggregate_check_distinct(uchar *arg) {
  assert(fixed);
  Distinct_check *dc = reinterpret_cast<Distinct_check *>(arg);

  if (dc->is_stopped(this)) return false;

  /*
    In the Standard, ORDER BY cannot contain an aggregate function;
    we are less strict, we allow it.
    However, if the aggregate in ORDER BY is not in the SELECT list, it
    might not be functionally dependent on all selected expressions, and thus
    might produce random order in combination with DISTINCT; then we reject
    it.

    One case where the aggregate is surely functionally dependent on the
    selected expressions, is if all GROUP BY expressions are in the SELECT
    list. But in that case DISTINCT is redundant and we have removed it in
    Query_block::prepare().
  */
  if (aggr_query_block == dc->select) return true;

  return false;
}

bool Item_sum::aggregate_check_group(uchar *arg) {
  assert(fixed);

  Group_check *gc = reinterpret_cast<Group_check *>(arg);

  if (gc->is_stopped(this)) return false;

  if (aggr_query_block != gc->select) {
    /*
      If aggr_query_block is inner to gc's query_block, this aggregate function
      might reference some columns of gc, so we need to analyze its arguments.
      If it is outer, analyzing its arguments should not cause a problem, we
      will meet outer references which we will ignore.
    */
    return false;
  }

  if (gc->is_fd_on_source(this)) {
    gc->stop_at(this);
    return false;
  }

  return true;
}

bool Item_sum::has_aggregate_ref_in_group_by(uchar *) {
  /*
    We reject references to aggregates in the GROUP BY clause of the
    query block where the aggregation happens.
  */
  return aggr_query_block != nullptr && aggr_query_block->group_fix_field;
}

Field *Item_sum::create_tmp_field(bool, TABLE *table) {
  DBUG_TRACE;
  Field *field;
  switch (result_type()) {
    case REAL_RESULT:
      field = new (*THR_MALLOC) Field_double(
          max_length, is_nullable(), item_name.ptr(), decimals, false, true);
      break;
    case INT_RESULT:
      field = new (*THR_MALLOC) Field_longlong(max_length, is_nullable(),
                                               item_name.ptr(), unsigned_flag);
      break;
    case STRING_RESULT:
      return make_string_field(table);
    case DECIMAL_RESULT:
      field = Field_new_decimal::create_from_item(this);
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      return nullptr;
  }
  if (field) field->init(table);
  return field;
}

bool Item_sum::collect_grouped_aggregates(uchar *arg) {
  auto *info = pointer_cast<Collect_grouped_aggregate_info *>(arg);

  if (m_is_window_function || info->m_break_off) return false;

  if (info->m_query_block == aggr_query_block && is_outer_reference()) {
    // This aggregate function aggregates in the transformed query block, but is
    // located inside a subquery. Currently, transform cannot get to this since
    // it doesn't descend into subqueries. This means we cannot substitute a
    // field for this aggregates, so break off. TODO.
    info->m_break_off = true;
    return false;
  }

  if (info->m_query_block != aggr_query_block) {
    // Aggregated either inside a subquery of the transformed query block or
    // outside of it. In either case, ignore it.
    info->m_outside = true;
    return false;
  }

  for (auto e : info->list) {  // eliminate duplicates
    if (e == this) {
      return false;
    }
  }

  info->list.emplace_back(this);
  return false;
}

Item *Item_sum::replace_aggregate(uchar *arg) {
  auto *info = pointer_cast<Item::Aggregate_replacement *>(arg);
  if (info->m_target == this)
    return info->m_replacement;
  else
    return this;
}

bool Item_sum::collect_scalar_subqueries(uchar *arg) {
  if (!m_is_window_function) {
    auto *info = pointer_cast<Collect_scalar_subquery_info *>(arg);
    /// Don't walk below grouped aggregate functions
    if (info->is_stopped(this)) return false;
    info->stop_at(this);
  }
  return false;
}

bool Item_sum::collect_item_field_or_view_ref_processor(uchar *arg) {
  if (!m_is_window_function) {
    auto *info = pointer_cast<Collect_item_fields_or_view_refs *>(arg);
    /// Don't walk below grouped aggregate functions
    if (info->is_stopped(this)) return false;
    info->stop_at(this);
  }
  return false;
}

void Item_sum::update_used_tables() {
  /*
    When evaluated as a constant value during optimization, there is no reason
    to update used tables information, as used_tables() will always report
    this item as const.
  */
  if (forced_const) return;

  used_tables_cache = 0;
  // Re-accumulate all properties except three
  m_accum_properties &=
      (PROP_AGGREGATION | PROP_WINDOW_FUNCTION | PROP_ROLLUP_EXPR);

  for (uint i = 0; i < arg_count; i++) {
    args[i]->update_used_tables();
    used_tables_cache |= args[i]->used_tables();
    add_accum_properties(args[i]);
  }
  add_used_tables_for_aggr_func();
}

void Item_sum::fix_after_pullout(Query_block *parent_query_block,
                                 Query_block *removed_query_block) {
  // Cannot aggregate into a context that is merged up.
  assert(aggr_query_block != removed_query_block);

  // We may merge up a query block, if it is not the aggregating query context
  if (base_query_block == removed_query_block)
    base_query_block = parent_query_block;

  // Perform pullout of arguments to aggregate function
  used_tables_cache = 0;

  Item **arg, **arg_end;
  for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
    Item *const item = *arg;
    item->fix_after_pullout(parent_query_block, removed_query_block);
    used_tables_cache |= item->used_tables();
  }
  // Complete used_tables information by looking at aggregate function
  add_used_tables_for_aggr_func();
}

/**
  Add used_tables information for aggregate function, based on its aggregated
  query block.

  If the function is aggregated into its local context, it can
  be calculated only after evaluating the full join, thus it
  depends on all tables of this join. Otherwise, it depends on
  outer tables, even if its arguments args[] do not explicitly
  reference an outer table, like COUNT (*) or COUNT(123).

  Window functions are always evaluated in the local scope
  and depend on all tables involved in the join since they cannot
  be evaluated until after the join is completed.
*/

void Item_sum::add_used_tables_for_aggr_func() {
  used_tables_cache |=
      aggr_query_block == base_query_block || m_is_window_function
          ? base_query_block->all_tables_map()
          : OUTER_REF_TABLE_BIT;
  /*
    Aggregate functions are not allowed to be const, so if there are no tables
    to depend them on, ensure they are executed anyway:
  */
  if (const_for_execution()) used_tables_cache |= RAND_TABLE_BIT;
}

Item *Item_sum::set_arg(THD *thd, uint i, Item *new_val) {
  thd->change_item_tree(args + i, new_val);
  return new_val;
}

int Item_sum::set_aggregator(Aggregator::Aggregator_type aggregator) {
  /*
    Dependent subselects may be executed multiple times, making
    set_aggregator to be called multiple times. The aggregator type
    will be the same, but it needs to be reset so that it is
    reevaluated with the new dependent data.
    This function may also be called multiple times during query optimization.
    In this case, the type may change, so we delete the old aggregator,
    and create a new one.
  */
  if (aggr && aggregator == aggr->Aggrtype()) {
    aggr->clear();
    return false;
  }

  destroy(aggr);
  switch (aggregator) {
    case Aggregator::DISTINCT_AGGREGATOR:
      aggr = new (*THR_MALLOC) Aggregator_distinct(this);
      break;
    case Aggregator::SIMPLE_AGGREGATOR:
      aggr = new (*THR_MALLOC) Aggregator_simple(this);
      break;
  };
  return aggr ? false : true;
}

void Item_sum::cleanup() {
  if (aggr != nullptr) {
    destroy(aggr);
    aggr = nullptr;
  }
  Item_result_field::cleanup();
  // forced_const may have been set during optimization, reset it:
  forced_const = false;
}

bool Item_sum::fix_fields(THD *thd, Item **ref [[maybe_unused]]) {
  assert(fixed == 0);
  if (m_window != nullptr) {
    if (m_window_resolved) return false;

    if (Window::resolve_reference(thd, this, &m_window)) return true;

    m_window_resolved = true;
  }
  return false;
}

void Item_sum::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                              mem_root_deque<Item *> *fields) {
  if (m_is_window_function) {
    for (auto &it : Bounds_checked_array<Item *>(args, arg_count))
      it->split_sum_func2(thd, ref_item_array, fields, &it, true);
  }
}

bool Item_sum::reset_wf_state(uchar *arg) {
  if (!m_is_window_function) return false;
  DBUG_TRACE;
  bool *do_framing = (bool *)arg;

  if (*do_framing) {
    if (framing()) clear();
  } else {
    if (!framing()) clear();
  }
  return false;
}

bool Item_sum::wf_common_init() {
  if (m_window->do_copy_null()) {
    assert(m_window->needs_buffering());
    null_value = is_nullable();
    return true;
  }
  if (m_window->at_partition_border() && !m_window->needs_buffering()) {
    clear();
  }
  return false;
}

/**
  Compare keys consisting of single field that cannot be compared as binary.

  Used by the Unique class to compare keys. Will do correct comparisons
  for all field types.

  @param    arg     Pointer to the relevant Field class instance
  @param    a       left key image
  @param    b       right key image
  @return   comparison result
    @retval < 0       if key1 < key2
    @retval = 0       if key1 = key2
    @retval > 0       if key1 > key2
*/

static int simple_str_key_cmp(const void *arg, const void *a, const void *b) {
  const Field *f = pointer_cast<const Field *>(arg);
  const uchar *key1 = pointer_cast<const uchar *>(a);
  const uchar *key2 = pointer_cast<const uchar *>(b);
  return f->cmp(key1, key2);
}

/**
  Correctly compare composite keys.

  Used by the Unique class to compare keys. Will do correct comparisons
  for composite keys with various field types.

  @param arg     Pointer to the relevant Aggregator_distinct instance
  @param a       left key image
  @param b       right key image
  @return        comparison result
    @retval <0       if key1 < key2
    @retval =0       if key1 = key2
    @retval >0       if key1 > key2
*/

int Aggregator_distinct::composite_key_cmp(const void *arg, const void *a,
                                           const void *b) {
  const Aggregator_distinct *aggr =
      static_cast<const Aggregator_distinct *>(arg);
  const uchar *key1 = pointer_cast<const uchar *>(a);
  const uchar *key2 = pointer_cast<const uchar *>(b);
  Field **field = aggr->table->field;
  Field **field_end = field + aggr->table->s->fields;
  uint32 *lengths = aggr->field_lengths;
  for (; field < field_end; ++field) {
    Field *f = *field;
    int len = *lengths++;
    int res = f->cmp(key1, key2);
    if (res) return res;
    key1 += len;
    key2 += len;
  }
  return 0;
}

static enum enum_field_types calc_tmp_field_type(
    enum enum_field_types table_field_type, Item_result result_type) {
  /* Adjust tmp table type according to the chosen aggregation type */
  switch (result_type) {
    case STRING_RESULT:
    case REAL_RESULT:
      if (table_field_type != MYSQL_TYPE_FLOAT)
        table_field_type = MYSQL_TYPE_DOUBLE;
      break;
    case INT_RESULT:
      table_field_type = MYSQL_TYPE_LONGLONG;
      [[fallthrough]];
    case DECIMAL_RESULT:
      if (table_field_type != MYSQL_TYPE_LONGLONG)
        table_field_type = MYSQL_TYPE_NEWDECIMAL;
      break;
    case ROW_RESULT:
    default:
      assert(0);
  }
  return table_field_type;
}

/***************************************************************************/

/* Declarations for auxiliary C-callbacks */

static int simple_raw_key_cmp(const void *arg, const void *key1,
                              const void *key2) {
  return memcmp(key1, key2, *(const uint *)arg);
}

static int item_sum_distinct_walk(void *element, element_count, void *item) {
  return ((Aggregator_distinct *)(item))->unique_walk_function(element);
}

/***************************************************************************/
/**
  Called before feeding the first row. Used to allocate/setup
  the internal structures used for aggregation.

  @param thd Thread descriptor
  @return status
    @retval false success
    @retval true  failure

    Prepares Aggregator_distinct to process the incoming stream.
    Creates the temporary table and the Unique class if needed.
    Called by Item_sum::aggregator_setup()
*/

bool Aggregator_distinct::setup(THD *thd) {
  endup_done = false;
  /*
    Setup can be called twice for ROLLUP items. This is a bug.
    Please add assert(tree == 0) here when it's fixed.
  */
  if (tree || table || tmp_table_param) return false;

  assert(thd->lex->current_query_block() == item_sum->aggr_query_block);

  if (item_sum->setup(thd)) return true;
  if (item_sum->sum_func() == Item_sum::COUNT_FUNC ||
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC) {
    mem_root_deque<Item *> list(thd->mem_root);
    Query_block *query_block = item_sum->aggr_query_block;

    if (!(tmp_table_param = new (thd->mem_root) Temp_table_param)) return true;

    /**
      Create a table with an unique key over all parameters.
      If the list contains only const values, const_distinct
      is set to CONST_NOT_NULL to avoid creation of temp table
      and thereby counting as count(distinct of const values)
      will always be 1. If any of these const values is null,
      const_distinct is set to CONST_NULL to ensure aggregation
      does not happen.
     */
    uint const_items = 0;
    uint num_args = item_sum->argument_count();
    assert(num_args);
    for (uint i = 0; i < num_args; i++) {
      Item *item = item_sum->get_arg(i);
      list.push_back(item);
      if (item->const_item()) {
        const bool is_null = item->is_null();
        if (thd->is_error()) return true;  // is_null can fail
        if (is_null) {
          const_distinct = CONST_NULL;
          return false;
        } else
          const_items++;
      }
    }
    if (num_args == const_items) {
      const_distinct = CONST_NOT_NULL;
      return false;
    }
    count_field_types(query_block, tmp_table_param, list, false, false);
    tmp_table_param->force_copy_fields = item_sum->has_force_copy_fields();
    assert(table == nullptr);
    /*
      Make create_tmp_table() convert BIT columns to BIGINT.
      This is needed because BIT fields store parts of their data in table's
      null bits, and we don't have methods to compare two table records, which
      is needed by Unique which is used when HEAP table is used.
    */
    for (Item *item : list) {
      if (item->type() == Item::FIELD_ITEM &&
          ((Item_field *)item)->field->type() == FIELD_TYPE_BIT)
        item->marker = Item::MARKER_BIT;
      assert(!item->hidden);
    }
    if (!(table = create_tmp_table(thd, tmp_table_param, list, nullptr, true,
                                   false, query_block->active_options(),
                                   HA_POS_ERROR, "")))
      return true;
    table->file->ha_extra(HA_EXTRA_NO_ROWS);  // Don't update rows
    table->no_rows = true;
    if (table->hash_field) table->file->ha_index_init(0, false);

    if ((table->s->db_type() == temptable_hton ||
         table->s->db_type() == heap_hton) &&
        (table->s->blob_fields == 0)) {
      /*
        No blobs:
        set up a compare function and its arguments to use with Unique.
      */
      qsort2_cmp compare_key;
      void *cmp_arg;
      Field **field = table->field;
      Field **field_end = field + table->s->fields;
      bool all_binary = true;

      for (tree_key_length = 0; field < field_end; ++field) {
        Field *f = *field;
        enum enum_field_types type = f->type();
        tree_key_length += f->pack_length();
        if ((type == MYSQL_TYPE_VARCHAR) ||
            (!f->binary() &&
             (type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VAR_STRING))) {
          all_binary = false;
          break;
        }
      }
      if (all_binary) {
        cmp_arg = (void *)&tree_key_length;
        compare_key = simple_raw_key_cmp;
      } else {
        if (table->s->fields == 1) {
          /*
            If we have only one field, which is the most common use of
            count(distinct), it is much faster to use a simpler key
            compare method that can take advantage of not having to worry
            about other fields.
          */
          compare_key = simple_str_key_cmp;
          cmp_arg = (void *)table->field[0];
          /* tree_key_length has been set already */
        } else {
          uint32 *length;
          compare_key = composite_key_cmp;
          cmp_arg = (void *)this;
          field_lengths =
              (uint32 *)thd->alloc(table->s->fields * sizeof(uint32));
          for (tree_key_length = 0, length = field_lengths,
              field = table->field;
               field < field_end; ++field, ++length) {
            *length = (*field)->pack_length();
            tree_key_length += *length;
          }
        }
      }
      assert(tree == nullptr);
      tree = new (thd->mem_root) Unique(compare_key, cmp_arg, tree_key_length,
                                        item_sum->ram_limitation(thd));
      /*
        The only time tree_key_length could be 0 is if someone does
        count(distinct) on a char(0) field - stupid thing to do,
        but this has to be handled - otherwise someone can crash
        the server with a DoS attack
      */
      if (!tree) return true;
    }
    return false;
  } else {
    List<Create_field> field_list;
    Create_field field_def; /* field definition */
    Item *arg;
    DBUG_TRACE;
    /* It's legal to call setup() more than once when in a subquery */
    if (tree) return false;

    /*
      Virtual table and the tree are created anew on each re-execution of
      PS/SP. Hence all further allocations are performed in the runtime
      mem_root.
    */
    if (field_list.push_back(&field_def)) return true;

    item_sum->set_nullable(true);
    item_sum->null_value = true;
    item_sum->allow_group_via_temp_table = false;

    assert(item_sum->get_arg(0)->fixed);

    arg = item_sum->get_arg(0);
    if (arg->const_item()) {
      if (arg->update_null_value()) return true;
      if (arg->null_value) {
        const_distinct = CONST_NULL;
        return false;
      }
    }

    enum enum_field_types field_type =
        calc_tmp_field_type(arg->data_type(), arg->result_type());

    field_def.init_for_tmp_table(
        field_type, arg->max_length,
        field_type == MYSQL_TYPE_NEWDECIMAL
            ? min<unsigned int>(arg->decimals, DECIMAL_MAX_SCALE)
            : arg->decimals,
        arg->is_nullable(), arg->unsigned_flag, 0);

    if (!(table = create_tmp_table_from_fields(thd, field_list))) return true;

    /* XXX: check that the case of CHAR(0) works OK */
    tree_key_length = table->s->reclength - table->s->null_bytes;

    /*
      Unique handles all unique elements in a tree until they can't fit
      in.  Then the tree is dumped to the temporary file. We can use
      simple_raw_key_cmp because the table contains numbers only; decimals
      are converted to binary representation as well.
    */
    tree = new (thd->mem_root)
        Unique(simple_raw_key_cmp, &tree_key_length, tree_key_length,
               item_sum->ram_limitation(thd));

    return tree == nullptr;
  }
}

/**
  Invalidate calculated value and clear the distinct rows.

  Frees space used by the internal data structures.
  Removes the accumulated distinct rows. Invalidates the calculated result.
*/

void Aggregator_distinct::clear() {
  endup_done = false;
  item_sum->clear();
  if (tree) tree->reset();
  /* tree and table can be both null only if const_distinct is enabled*/
  if (item_sum->sum_func() == Item_sum::COUNT_FUNC ||
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC) {
    if (!tree && table) {
      (void)table->empty_result_table();
      if (table->hash_field) table->file->ha_index_init(0, false);
    }
  } else {
    item_sum->null_value = true;
  }
}

/**
  Process incoming row.

  Add it to Unique/temp hash table if it's unique. Skip the row if
  not unique.
  Prepare Aggregator_distinct to process the incoming stream.
  Create the temporary table and the Unique class if needed.
  Called by Item_sum::aggregator_add().
  To actually get the result value in item_sum's buffers
  Aggregator_distinct::endup() must be called.

  @return status
    @retval false     success
    @retval true      failure
*/

bool Aggregator_distinct::add() {
  THD *thd = current_thd;

  if (const_distinct == CONST_NULL) return false;

  if (item_sum->sum_func() == Item_sum::COUNT_FUNC ||
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC) {
    int error;

    if (const_distinct == CONST_NOT_NULL) {
      assert(item_sum->fixed == 1);
      Item_sum_count *sum = (Item_sum_count *)item_sum;
      sum->count = 1;
      return false;
    }
    if (copy_funcs(tmp_table_param, thd)) return true;

    for (Field **field = table->field; *field; field++)
      if ((*field)->is_real_null()) return false;  // Don't count NULL

    if (tree) {
      /*
        The first few bytes of record (at least one) are just markers
        for deleted and NULLs. We want to skip them since they will
        bloat the tree without providing any valuable info. Besides,
        key_length used to initialize the tree didn't include space for them.
      */
      return tree->unique_add(table->record[0] + table->s->null_bytes);
    }

    if (!check_unique_constraint(table)) return false;
    error = table->file->ha_write_row(table->record[0]);
    if (error && !table->file->is_ignorable_error(error)) {
      if (create_ondisk_from_heap(current_thd, table, error,
                                  /*insert_last_record=*/true,
                                  /*ignore_last_dup=*/true,
                                  /*is_duplicate=*/nullptr) ||
          table->file->ha_index_init(0, false)) {
        return true;
      }
    }
    return false;
  } else {
    item_sum->get_arg(0)->save_in_field(table->field[0], false);
    if (current_thd->is_error()) {
      return true;
    }
    if (table->field[0]->is_null()) return false;
    assert(tree);
    item_sum->null_value = false;
    /*
      '0' values are also stored in the tree. This doesn't matter
      for SUM(DISTINCT), but is important for AVG(DISTINCT)
    */
    return tree->unique_add(table->field[0]->field_ptr());
  }
}

/**
  Calculate the aggregate function value.

  Since Distinct_aggregator::add() just collects the distinct rows,
  we must go over the distinct rows and feed them to the aggregation
  function before returning its value.
  This is what endup () does. It also sets the result validity flag
  endup_done to true so it will not recalculate the aggregate value
  again if the Item_sum hasn't been reset.
*/

void Aggregator_distinct::endup() {
  DBUG_TRACE;
  /* prevent consecutive recalculations */
  if (endup_done) return;

  if (const_distinct == CONST_NOT_NULL) {
    endup_done = true;
    return;
  }

  /* we are going to calculate the aggregate value afresh */
  item_sum->clear();

  /* The result will definitely be null : no more calculations needed */
  if (const_distinct == CONST_NULL) return;

  if (item_sum->sum_func() == Item_sum::COUNT_FUNC ||
      item_sum->sum_func() == Item_sum::COUNT_DISTINCT_FUNC) {
    assert(item_sum->fixed == 1);
    Item_sum_count *sum = (Item_sum_count *)item_sum;

    if (tree && tree->is_in_memory()) {
      /* everything fits in memory */
      sum->count = (longlong)tree->elements_in_tree();
      endup_done = true;
    }
    if (!tree) {
      /* there were blobs */
      table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
      if (table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT)
        sum->count = table->file->stats.records;
      else {
        // index must be closed before ha_records() is called
        if (table->file->inited) table->file->ha_index_or_rnd_end();
        ha_rows num_rows = 0;
        table->file->ha_records(&num_rows);
        // We have to initialize hash_index because update_sum_func needs it
        if (table->hash_field) table->file->ha_index_init(0, false);
        sum->count = static_cast<longlong>(num_rows);
      }
      endup_done = true;
    }
  }

  /*
    We don't have a tree only if 'setup()' hasn't been called;
    this is the case of sql_executor.cc:return_zero_rows.
  */
  if (tree && !endup_done) {
    /*
      All tree's values are not NULL.
      Note that value of field is changed as we walk the tree, in
      Aggregator_distinct::unique_walk_function, but it's always not NULL.
    */
    table->field[0]->set_notnull();
    /* go over the tree of distinct keys and calculate the aggregate value */
    use_distinct_values = true;
    tree->walk(item_sum_distinct_walk, (void *)this);
    use_distinct_values = false;
  }
  /* prevent consecutive recalculations */
  endup_done = true;
}

String *Item_sum_num::val_str(String *str) { return val_string_from_real(str); }

my_decimal *Item_sum_num::val_decimal(my_decimal *decimal_value) {
  return val_decimal_from_real(decimal_value);
}

String *Item_sum_int::val_str(String *str) { return val_string_from_int(str); }

my_decimal *Item_sum_int::val_decimal(my_decimal *decimal_value) {
  return val_decimal_from_int(decimal_value);
}

bool Item_sum_num::fix_fields(THD *thd, Item **ref) {
  if (super::fix_fields(thd, ref)) return true; /* purecov: inspected */

  if (init_sum_func_check(thd)) return true;

  Condition_context CCT(thd->lex->current_query_block());

  set_nullable(false);

  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
    set_nullable(is_nullable() || args[i]->is_nullable());
  }

  // Set this value before calling resolve_type()
  null_value = true;

  if (resolve_type(thd)) return true;

  if (check_sum_func(thd, ref)) return true;

  fixed = true;
  return false;
}

bool Item_sum_bit::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);

  if (super::fix_fields(thd, ref)) return true; /* purecov: inspected */

  if (init_sum_func_check(thd)) return true;

  Condition_context CCT(thd->lex->current_query_block());

  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (resolve_type(thd)) return true;

  assert(!thd->is_error());

  if (check_sum_func(thd, ref)) return true;

  fixed = true;
  return false;
}

bool Item_sum_bit::resolve_type(THD *thd) {
  // Assume varbinary; if integer is provided then re-prepare.
  if (args[0]->data_type() == MYSQL_TYPE_INVALID) {
    if (args[0]->propagate_type(
            thd, Type_properties(MYSQL_TYPE_VARCHAR, &my_charset_bin)))
      return true;
    // avoid length-too-big error further down
    args[0]->max_length = (CONVERT_IF_BIGGER_TO_BLOB - 1);
  }

  max_length = 0;
  if (bit_func_returns_binary(args[0], nullptr)) {
    hybrid_type = STRING_RESULT;
    for (uint i = 0; i < arg_count; i++)
      max_length = max(max_length, args[i]->max_length);
    if (max_length > (CONVERT_IF_BIGGER_TO_BLOB - 1)) {
      /*
        Implementation of Item_sum_bit_field expects that "result_field" is
        Field_varstring, not Field_blob, so that the buffer's content is easily
        modifiable.
        The above check guarantees that the tmp table code will choose a
        Field_varstring over a Field_blob, and an assertion is present in the
        constructor of Item_sum_bit_field to verify the Field.
      */
      my_error(ER_INVALID_BITWISE_AGGREGATE_OPERANDS_SIZE, MYF(0), func_name());
      return true;
    }
    m_digit_cnt_card = max_length * 8;
    /*
      One extra byte needed to store a per-group boolean flag
      if Item_sum_bit_field is used.
    */
    max_length++;
    set_data_type(MYSQL_TYPE_VARCHAR);
  } else {
    m_digit_cnt_card = DIGIT_CNT_CARD;
    hybrid_type = INT_RESULT;
    max_length = MAX_BIGINT_WIDTH + 1;
    set_data_type(MYSQL_TYPE_LONGLONG);
  }

  if (m_window != nullptr && !m_is_xor) {
    m_digit_cnt = new (thd->mem_root) ulonglong[m_digit_cnt_card];
    if (m_digit_cnt == nullptr) return true;
    std::memset(m_digit_cnt, 0, m_digit_cnt_card * sizeof(ulonglong));
  }

  set_nullable(false);
  null_value = false;
  result_field = nullptr;
  decimals = 0;
  unsigned_flag = true;

  return reject_geometry_args(arg_count, args, this);
}

void Item_sum_bit::remove_bits(const String *s1, ulonglong b1) {
  if (m_is_xor) {
    // XOR satisfies ((A OP B) OP B) == A, so inverting is easy:
    (void)add_bits(s1, b1);  // add_bits() cannot fail here.
    return;
  }

  const uchar *s1_c_p;
  uchar *value_bits;
  size_t buff_length;

  if (hybrid_type == STRING_RESULT) {
    s1_c_p = pointer_cast<const uchar *>(s1->ptr());
    value_bits = pointer_cast<uchar *>(value_buff.ptr());
    buff_length = value_buff.length() - 1;
  } else {
    s1_c_p = pointer_cast<const uchar *>(&b1);
    value_bits = pointer_cast<uchar *>(&bits);
    buff_length = sizeof(b1);
  }

  /*
    Execute the bitwise inverse operation. We could have executed this
    with a combination of std::bitset<sizeeof(ulonglong) * 8> and
    std::bitset<8>, as does add_bits(), but longer bits shifting
    to get bits in place might not be beneficial, so use just bytes.
    Microbenchmarking showed little difference.
  */
  for (size_t i = 0; i < buff_length; i++) {
    std::bitset<8> s1_bits(s1_c_p[i]);
    if (is_and()) {
      for (uint bit = 0; bit < 8; bit++) {
        m_digit_cnt[(i * 8) + bit] -= !s1_bits[bit];  // one less 0 in frame
        // Temporarily save updated bit in s1_bits:
        s1_bits.set(bit, m_digit_cnt[(i * 8) + bit] == 0);
      }
    } else  // OR
    {
      for (uint bit = 0; bit < 8; bit++) {
        m_digit_cnt[(i * 8) + bit] -= s1_bits[bit];  // one less 1 in frame
        s1_bits.set(bit, m_digit_cnt[(i * 8) + bit] > 0);
      }
    }

    value_bits[i] = s1_bits.to_ulong();
  }
}

/**
  Helper for Item_sum_bit::add_bits().

  Does value_bits = s1_c_p bit_op value_bits.

  @tparam Char_op  class offering a bit operation for a uchar: AND, OR
                   or XOR
  @tparam Int_op   class offering a bit operation for a ulonglong: ditto
  @param  buff_length  length of s1_c_p
  @param  s1_c_p             first argument of bit op
  @param[in,out] value_bits  second argument of bit op, and result
*/
template <class Char_op, class Int_op>
static inline void apply_bit_op(size_t buff_length, const uchar *s1_c_p,
                                uchar *value_bits) {
  auto int_op = Int_op();
  auto char_op = Char_op();
  size_t i = 0;
  // Execute the bitwise operation.
  while (i + sizeof(longlong) <= buff_length) {
    int8store(&value_bits[i],
              int_op(uint8korr(&s1_c_p[i]), uint8korr(&value_bits[i])));
    i += sizeof(longlong);
  }
  while (i < buff_length) {
    value_bits[i] = char_op(s1_c_p[i], value_bits[i]);
    i++;
  }
}

bool Item_sum_bit::add_bits(const String *s1, ulonglong b1) {
  assert(!args[0]->null_value);

  const uchar *s1_c_p;
  size_t buff_length;

  if (hybrid_type == STRING_RESULT) {
    assert(s1 != nullptr);
    s1_c_p = pointer_cast<const uchar *>(s1->ptr());
    buff_length = s1->length();
    assert(value_buff.length() > 0);
    // See if there has been a non-NULL value in this group/frame:
    const bool non_nulls = value_buff[value_buff.length() - 1];
    if (!non_nulls) {
      // Allocate length of argument + one extra byte for non_nulls
      if (value_buff.alloc(buff_length + 1)) {
        null_value = true;
        return true;
      }
      value_buff.length(buff_length + 1);
      // This is the first non-NULL value of the group, accumulate it.
      std::memcpy(&value_buff[0], s1->ptr(), buff_length);
      // Store that a non-NULL value has been seen.
      value_buff[buff_length] = 1;
    } else {
      /*
        If current value's length is different from the length of the
        accumulated value for this group, return error.
      */
      if ((value_buff.length() - 1) != buff_length) {
        my_error(ER_INVALID_BITWISE_OPERANDS_SIZE, MYF(0), func_name());
        return true;
      }

      // At this point the values should be not-null and have the same size.
      uchar *value_bits = pointer_cast<uchar *>(value_buff.ptr());
      if (m_is_xor)
        apply_bit_op<std::bit_xor<char>, std::bit_xor<ulonglong>>(
            buff_length, s1_c_p, value_bits);
      else if (is_and())
        apply_bit_op<std::bit_and<char>, std::bit_and<ulonglong>>(
            buff_length, s1_c_p, value_bits);
      else
        apply_bit_op<std::bit_or<char>, std::bit_or<ulonglong>>(
            buff_length, s1_c_p, value_bits);
    }
  } else {
    bits = m_is_xor ? (bits ^ b1) : (is_and() ? (bits & b1) : (bits | b1));
    // Consider the integer's bytes as a string for the rest of this function
    s1_c_p = pointer_cast<const uchar *>(&b1);
    buff_length = sizeof(b1);
  }

  /*
    For each bit in s1's bytes, update the bit's counter (m_digit_cnt) for
    that bit as follows: for BIT_AND, increment the counter if we see a zero in
    that bit; for BIT_OR increment the counter if we see a 1 in that bit.
    BIT_XOR doesn't need special treatment. And set functions don't use
    inversion so don't need the counter.
  */

  if (!m_is_window_function || m_is_xor) return false;

  for (size_t i = 0; i < buff_length; i++) {
    std::bitset<8> s1_bits(s1_c_p[i]);
    for (uint bit = 0; bit < 8; bit++) {
      assert((i * 8) + bit < m_digit_cnt_card);
      m_digit_cnt[(i * 8) + bit] += s1_bits[bit] ^ is_and();
    }
  }

  return false;
}

/**
  Executes the requested bitwise operation, using args[0] as first argument.
  If the result type is 'binary string':
   - takes value_buff as second argument and stores the result in value_buff.
   - sets the last character of value_buff to be a 'char' equal to
     1 if at least one non-NULL value has been seen for this group, to 0
     otherwise.
  If the result type is integer:
   - takes 'bits' as second argument and stores the result in 'bits'.
*/
bool Item_sum_bit::add() {
  char buff[CONVERT_IF_BIGGER_TO_BLOB - 1];

  const String *argval_s = nullptr;
  ulonglong argval_i = 0;

  String tmp_str(buff, sizeof(buff), &my_charset_bin);
  if (hybrid_type == STRING_RESULT) {
    argval_s = args[0]->val_str(&tmp_str);
  } else
    argval_i = (ulonglong)args[0]->val_int();
  if (current_thd->is_error()) {
    return true;
  }

  /*
    Handle grouped aggregates first
  */
  if (!m_is_window_function) {
    if (args[0]->null_value)
      return false;  // NULLs are ignorable for the set function
    return add_bits(argval_s, argval_i);
  }

  /*
    The next section follows the normal pattern for optimized window function
    aggregates.
  */
  if (!args[0]->null_value) {
    if (m_window->do_inverse()) {
      assert(m_count > 0 && m_count > m_frame_null_count);
      remove_bits(argval_s, argval_i);
      m_count--;
    } else {
      if (add_bits(argval_s, argval_i))
        return true;  // error, typically different length
      m_count++;
    }
  } else {
    if (m_window->do_inverse()) {
      assert(m_count >= m_frame_null_count && m_frame_null_count > 0);
      m_count--;
      m_frame_null_count--;
    } else {
      m_count++;
      m_frame_null_count++;
    }
  }

  if (m_count == m_frame_null_count) {
    if (hybrid_type == STRING_RESULT) {
      // Mark that there are only NULLs; val_str() will set default value
      const size_t buff_length = value_buff.length() - 1;
      value_buff[buff_length] = 0;
    } else
      bits = reset_bits;
  }

  return false;
}

bool Item_sum_hybrid::fix_fields(THD *thd, Item **ref) {
  if (super::fix_fields(thd, ref)) return true; /* purecov: inspected */

  Item *item = args[0];

  if (init_sum_func_check(thd)) return true;

  Condition_context CCT(thd->lex->current_query_block());

  // 'item' can be changed during fix_fields
  if ((!item->fixed && item->fix_fields(thd, args)) ||
      (item = args[0])->check_cols(1))
    return true;

  hybrid_type = item->result_type();

  if (setup_hybrid(args[0], nullptr)) return true;
  /* MIN/MAX can return NULL for empty set independent of the used column */
  set_nullable(true);
  result_field = nullptr;
  null_value = true;
  if (resolve_type(thd)) return true;

  set_data_type_from_item(item->real_item());

  if (check_sum_func(thd, ref)) return true;

  fixed = true;
  return false;
}

bool Item_sum_hybrid::setup_hybrid(Item *item, Item *value_arg) {
  value = Item_cache::get_cache(item);
  value->setup(item);
  value->store(value_arg);
  arg_cache = Item_cache::get_cache(item);
  if (arg_cache == nullptr) return true;
  arg_cache->setup(item);
  cmp = new (*THR_MALLOC) Arg_comparator();
  if (cmp == nullptr) return true;
  if (cmp->set_cmp_func(this, pointer_cast<Item **>(&arg_cache),
                        pointer_cast<Item **>(&value), false))
    return true;
  collation.set(item->collation);

  return false;
}

Field *Item_sum_hybrid::create_tmp_field(bool group, TABLE *table) {
  DBUG_TRACE;
  Field *field;
  if (args[0]->type() == Item::FIELD_ITEM) {
    field = down_cast<Item_field *>(args[0])->field;

    field = create_tmp_field_from_field(current_thd, field, item_name.ptr(),
                                        table, nullptr);
    if (field == nullptr) return nullptr;
    field->clear_flag(NOT_NULL_FLAG);
    field->orig_table_name = nullptr;
    field->orig_db_name = nullptr;
    return field;
  }
  /*
    DATE/TIME fields have STRING_RESULT result types.
    In order to preserve field type, it's needed to handle DATE/TIME
    fields creations separately.
  */
  switch (args[0]->data_type()) {
    case MYSQL_TYPE_DATE:
      field = new (*THR_MALLOC) Field_newdate(is_nullable(), item_name.ptr());
      break;
    case MYSQL_TYPE_TIME:
      field = new (*THR_MALLOC)
          Field_timef(is_nullable(), item_name.ptr(), decimals);
      break;
    case MYSQL_TYPE_TIMESTAMP:
      field = new (*THR_MALLOC)
          Field_timestampf(is_nullable(), item_name.ptr(), decimals);
      break;
    case MYSQL_TYPE_DATETIME:
      field = new (*THR_MALLOC)
          Field_datetimef(is_nullable(), item_name.ptr(), decimals);
      break;
    default:
      return Item_sum::create_tmp_field(group, table);
  }
  if (field) field->init(table);
  return field;
}

/***********************************************************************
** reset and add of sum_func
***********************************************************************/

/**
  @todo
  check if the following assignments are really needed
*/
Item_sum_sum::Item_sum_sum(THD *thd, Item_sum_sum *item)
    : Item_sum_num(thd, item),
      hybrid_type(item->hybrid_type),
      curr_dec_buff(item->curr_dec_buff),
      m_count(item->m_count),
      m_frame_null_count(item->m_frame_null_count) {
  /* TODO: check if the following assignments are really needed */
  if (hybrid_type == DECIMAL_RESULT) {
    my_decimal2decimal(item->dec_buffs, dec_buffs);
    my_decimal2decimal(item->dec_buffs + 1, dec_buffs + 1);
  } else
    sum = item->sum;
}

Item *Item_sum_sum::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_sum(thd, this);
  return result;
}

void Item_sum_sum::clear() {
  null_value = true;
  if (hybrid_type == DECIMAL_RESULT) {
    curr_dec_buff = 0;
    my_decimal_set_zero(&dec_buffs[0]);
    my_decimal_set_zero(&dec_buffs[1]);
  } else
    sum = 0.0;
  m_count = 0;
  m_frame_null_count = 0;
}

void Item_sum_sum::no_rows_in_result() { clear(); }

bool Item_sum_sum::resolve_type(THD *thd) {
  DBUG_TRACE;
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_DOUBLE)) return true;
  if (reject_geometry_args(arg_count, args, this)) return true;

  set_nullable(true);
  null_value = true;

  switch (args[0]->numeric_context_result_type()) {
    case REAL_RESULT:
      set_data_type_double();
      // If argument has specified precision and scale, copy those values:
      if (args[0]->decimals != DECIMAL_NOT_SPECIFIED) {
        decimals = args[0]->decimals;
        max_length = float_length(decimals);
      }
      sum = 0.0;
      break;
    case INT_RESULT:
    case DECIMAL_RESULT: {
      // SUM result cannot be longer than length(arg) + length(MAX_ROWS)
      int precision = args[0]->decimal_precision() + DECIMAL_LONGLONG_DIGITS;
      set_data_type_decimal(precision, args[0]->decimals);
      curr_dec_buff = 0;
      my_decimal_set_zero(dec_buffs);
      break;
    }
    case STRING_RESULT:
    case ROW_RESULT:
    default:
      assert(0);
  }

  hybrid_type = Item::type_to_result(data_type());

  DBUG_PRINT("info",
             ("Type: %s (%d, %d)",
              (hybrid_type == REAL_RESULT
                   ? "REAL_RESULT"
                   : hybrid_type == DECIMAL_RESULT
                         ? "DECIMAL_RESULT"
                         : hybrid_type == INT_RESULT ? "INT_RESULT"
                                                     : "--ILLEGAL!!!--"),
              max_length, (int)decimals));
  return false;
}

bool Item_sum_sum::check_wf_semantics1(THD *thd, Query_block *select,
                                       Window_evaluation_requirements *r) {
  bool result = Item_sum::check_wf_semantics1(thd, select, r);
  if (hybrid_type == REAL_RESULT) {
    /*
      If the frame's start moves we will consider inversion, to remove the
      start rows. But, as we're using REAL_RESULT, and floating point
      arithmetic isn't mathematically exact, inversion may give different
      results from that of the non-optimized path. So, we use it only if the
      user allowed it:
    */
    const PT_frame *f = m_window->frame();
    if (f->m_from->m_border_type == WBT_VALUE_PRECEDING ||
        f->m_from->m_border_type == WBT_VALUE_FOLLOWING ||
        f->m_from->m_border_type == WBT_CURRENT_ROW) {
      r->row_optimizable &= !thd->variables.windowing_use_high_precision;
      r->range_optimizable &= !thd->variables.windowing_use_high_precision;
    }
  }
  return result;
}

bool Item_sum_sum::add() {
  DBUG_TRACE;
  assert(!m_is_window_function);
  if (hybrid_type == DECIMAL_RESULT) {
    my_decimal value;
    const my_decimal *val = aggr->arg_val_decimal(&value);
    if (current_thd->is_error()) return true;
    if (!aggr->arg_is_null(true)) {
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff ^ 1), val,
                     dec_buffs + curr_dec_buff);
      curr_dec_buff ^= 1;
      null_value = false;
    }
  } else {
    sum += aggr->arg_val_real();
    if (current_thd->is_error()) return true;
    if (!aggr->arg_is_null(true)) null_value = false;
  }
  return false;
}

longlong Item_sum_sum::val_int() {
  assert(fixed == 1);
  if (m_window != nullptr) {
    if (hybrid_type == REAL_RESULT) {
      return llrint_with_overflow_check(val_real());
    }
    longlong result = 0;
    my_decimal tmp;
    my_decimal *r = Item_sum_sum::val_decimal(&tmp);
    if (r != nullptr && !null_value)
      my_decimal2int(E_DEC_FATAL_ERROR, r, unsigned_flag, &result);
    return result;
  }

  if (aggr) aggr->endup();
  if (hybrid_type == DECIMAL_RESULT) {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, unsigned_flag,
                   &result);
    return result;
  }
  return llrint_with_overflow_check(val_real());
}

double Item_sum_sum::val_real() {
  DBUG_TRACE;
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0.0;

    if (hybrid_type == DECIMAL_RESULT) {
      my_decimal tmp;
      my_decimal *r = Item_sum_sum::val_decimal(&tmp);
      if (r != nullptr && !null_value)
        my_decimal2double(E_DEC_FATAL_ERROR, r, &sum);
    } else {
      double d = args[0]->val_real();

      if (!args[0]->null_value) {
        if (m_window->do_inverse()) {
          assert(m_count > 0 && m_count > m_frame_null_count);
          sum -= d;
          m_count--;
        } else {
          sum += d;
          m_count++;
        }
      } else {
        if (m_window->do_inverse()) {
          assert(m_count >= m_frame_null_count && m_frame_null_count > 0);
          m_count--;
          m_frame_null_count--;
        } else {
          m_count++;
          m_frame_null_count++;
        }
      }
      null_value = (m_count == m_frame_null_count);
    }
    return sum;
  } else {
    if (aggr) aggr->endup();
    if (hybrid_type == DECIMAL_RESULT)
      my_decimal2double(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, &sum);
    return sum;
  }
}

String *Item_sum_sum::val_str(String *str) {
  if (aggr) aggr->endup();
  if (hybrid_type == DECIMAL_RESULT) return val_string_from_decimal(str);
  return val_string_from_real(str);
}

my_decimal *Item_sum_sum::val_decimal(my_decimal *val) {
  if (m_is_window_function) {
    if (hybrid_type != DECIMAL_RESULT) return val_decimal_from_real(val);

    if (wf_common_init()) {
      return error_decimal(val);
    }

    my_decimal *const argd = args[0]->val_decimal(&dec_buffs[0]);

    if (!args[0]->null_value) {
      my_decimal tmp;
      if (m_window->do_inverse()) {
        assert(m_count > 0 && m_count > m_frame_null_count);
        my_decimal_sub(E_DEC_FATAL_ERROR, &tmp, &dec_buffs[1], argd);
        tmp.swap(dec_buffs[1]);
        m_count--;
      } else {
        my_decimal_add(E_DEC_FATAL_ERROR, &tmp, &dec_buffs[1], argd);
        tmp.swap(dec_buffs[1]);
        m_count++;
      }
    } else {
      if (m_window->do_inverse()) {
        assert(m_count >= m_frame_null_count && m_frame_null_count > 0);
        m_count--;
        m_frame_null_count--;
      } else {
        m_count++;
        m_frame_null_count++;
      }
    }

    null_value = (m_count == m_frame_null_count);

    return &dec_buffs[1];
  }

  if (aggr) aggr->endup();
  if (hybrid_type == DECIMAL_RESULT) return (dec_buffs + curr_dec_buff);
  return val_decimal_from_real(val);
}

/**
  Aggregate a distinct row from the distinct hash table.

  Called for each row into the hash table 'Aggregator_distinct::table'.
  Includes the current distinct row into the calculation of the
  aggregate value. Uses the Field classes to get the value from the row.
  This function is used for AVG/SUM(DISTINCT). For COUNT(DISTINCT)
  it's called only when there are no blob arguments and the data don't
  fit into memory (so Unique makes persisted trees on disk).

  @param element     pointer to the row data.

  @return status
    @retval false     success
    @retval true      failure
*/

bool Aggregator_distinct::unique_walk_function(void *element) {
  DBUG_TRACE;
  memcpy(table->field[0]->field_ptr(), element, tree_key_length);
  item_sum->add();
  return false;
}

Aggregator_distinct::~Aggregator_distinct() {
  if (tree) {
    destroy(tree);
    tree = nullptr;
  }
  if (table) {
    if (table->file) table->file->ha_index_or_rnd_end();
    close_tmp_table(table);
    free_tmp_table(table);
    table = nullptr;
  }
  if (tmp_table_param) {
    destroy(tmp_table_param);
    tmp_table_param = nullptr;
  }
}

my_decimal *Aggregator_simple::arg_val_decimal(my_decimal *value) {
  return item_sum->args[0]->val_decimal(value);
}

double Aggregator_simple::arg_val_real() {
  return item_sum->args[0]->val_real();
}

bool Aggregator_simple::arg_is_null(bool use_null_value) {
  Item **item = item_sum->args;
  const uint item_count = item_sum->arg_count;
  if (use_null_value) {
    for (uint i = 0; i < item_count; i++) {
      if (item[i]->null_value) return true;
    }
  } else {
    for (uint i = 0; i < item_count; i++) {
      if (item[i]->is_nullable() && item[i]->is_null()) return true;
    }
  }
  return false;
}

my_decimal *Aggregator_distinct::arg_val_decimal(my_decimal *value) {
  return use_distinct_values ? table->field[0]->val_decimal(value)
                             : item_sum->args[0]->val_decimal(value);
}

double Aggregator_distinct::arg_val_real() {
  return use_distinct_values ? table->field[0]->val_real()
                             : item_sum->args[0]->val_real();
}

bool Aggregator_distinct::arg_is_null(bool use_null_value) {
  if (use_distinct_values) {
    const bool rc = table->field[0]->is_null();
    assert(!rc);  // NULLs are never stored in 'tree'
    return rc;
  }
  return use_null_value ? item_sum->args[0]->null_value
                        : (item_sum->args[0]->is_nullable() &&
                           item_sum->args[0]->is_null());
}

Item *Item_sum_count::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result = m_is_window_function ? this
                                      : new (thd->mem_root)
                                            Item_sum_count(thd, this);
  return result;
}

void Item_sum_count::clear() { count = 0; }

bool Item_sum_count::add() {
  assert(!m_is_window_function);
  if (aggr->arg_is_null(false)) {
    return current_thd->is_error();
  }
  count++;
  return current_thd->is_error();
}

longlong Item_sum_count::val_int() {
  DBUG_TRACE;
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0;

    DBUG_EXECUTE_IF(("enter"), {
      DBUG_PRINT("enter", ("Item_sum_count::val_int arg0 %p", args[0]));
      if (dynamic_cast<Item_field *>(args[0])) {
        Item_field *f = down_cast<Item_field *>(args[0]);
        DBUG_PRINT(("enter"), ("Item_sum_count::val_int field: %p ptr: %p",
                               f->field, f->field->field_ptr()));
      }
    });

    if (args[0]->is_null()) {
      return count;
    }
    if (m_window->do_inverse()) {
      if (count > 0) count--;
    } else {
      count++;
    }
    null_value = false;

    return count;
  } else {
    if (aggr) aggr->endup();
    return count;
  }
}

void Item_sum_count::cleanup() {
  DBUG_TRACE;
  count = 0;
  Item_sum_int::cleanup();
}

bool Item_sum_avg::resolve_type(THD *thd) {
  if (Item_sum_sum::resolve_type(thd)) return true;

  set_nullable(true);
  null_value = true;
  prec_increment = thd->variables.div_precincrement;
  if (hybrid_type == DECIMAL_RESULT) {
    int precision = args[0]->decimal_precision() + prec_increment;
    int scale =
        min<uint>(args[0]->decimals + prec_increment, DECIMAL_MAX_SCALE);
    set_data_type_decimal(precision, scale);
    f_precision =
        min(precision + DECIMAL_LONGLONG_DIGITS, DECIMAL_MAX_PRECISION);
    f_scale = args[0]->decimals;
    dec_bin_size = my_decimal_get_binary_size(f_precision, f_scale);
  } else {
    assert(hybrid_type == REAL_RESULT);
    // If type has specified precision and scale, adjust according to increment:
    if (decimals != DECIMAL_NOT_SPECIFIED) {
      decimals = min<uint>(decimals + prec_increment, DECIMAL_NOT_SPECIFIED);
      max_length = float_length(decimals);
    }
  }
  return false;
}

Item *Item_sum_avg::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_avg(thd, this);
  return result;
}

Field *Item_sum_avg::create_tmp_field(bool group, TABLE *table) {
  DBUG_TRACE;
  Field *field;
  if (group) {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    field = new (*THR_MALLOC) Field_string(
        ((hybrid_type == DECIMAL_RESULT) ? dec_bin_size : sizeof(double)) +
            sizeof(longlong),
        false, item_name.ptr(), &my_charset_bin);
  } else if (hybrid_type == DECIMAL_RESULT)
    field = Field_new_decimal::create_from_item(this);
  else
    field = new (*THR_MALLOC) Field_double(
        max_length, is_nullable(), item_name.ptr(), decimals, false, true);
  if (field) field->init(table);
  return field;
}

void Item_sum_avg::clear() { Item_sum_sum::clear(); }

bool Item_sum_avg::add() {
  assert(!m_is_window_function);
  if (Item_sum_sum::add()) return true;
  if (!aggr->arg_is_null(true)) m_count++;
  return false;
}

double Item_sum_avg::val_real() {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0.0;

    double sum = Item_sum_sum::val_real();

    if (m_window->is_last_row_in_frame()) {
      const int64 divisor = m_count - m_frame_null_count;
      if (divisor > 0) sum = sum / ulonglong2double(divisor);
    }
    m_avg = sum;  // save
    return sum;
  } else {
    if (aggr) aggr->endup();
    if (!m_count) {
      null_value = true;
      return 0.0;
    }
    return Item_sum_sum::val_real() / ulonglong2double(m_count);
  }
}

my_decimal *Item_sum_avg::val_decimal(my_decimal *val) {
  DBUG_TRACE;
  my_decimal sum_buff, cnt;
  const my_decimal *sum_dec;
  assert(fixed == 1);

  if (m_is_window_function) {
    if (hybrid_type != DECIMAL_RESULT) {
      my_decimal *result = val_decimal_from_real(val);
      return result;
    }

    if (wf_common_init()) {
      return error_decimal(val);
    }

    /*
      dec_buff[0]:   the current value
      dec_buff[1]:   holds sum so far
    */
    my_decimal *argd = args[0]->val_decimal(&dec_buffs[0]);

    if (!args[0]->null_value) {
      my_decimal tmp;
      if (m_window->do_inverse()) {
        assert(m_count > 0 && m_count > m_frame_null_count);
        my_decimal_sub(E_DEC_FATAL_ERROR, &tmp, &dec_buffs[1], argd);
        tmp.swap(dec_buffs[1]);
        m_count--;
      } else {
        my_decimal_add(E_DEC_FATAL_ERROR, &tmp, &dec_buffs[1], argd);
        tmp.swap(dec_buffs[1]);
        m_count++;
      }
    } else {
      if (m_window->do_inverse()) {
        assert(m_count >= m_frame_null_count && m_frame_null_count > 0);
        m_frame_null_count--;
        m_count--;
        // else no need to inverse if we only saw nulls
      } else {
        m_frame_null_count++;
        m_count++;
      }
    }

    const int64 divisor = m_count - m_frame_null_count;

    if (m_window->is_last_row_in_frame() && divisor > 0) {
      int2my_decimal(E_DEC_FATAL_ERROR, divisor, false, &cnt);
      my_decimal_div(E_DEC_FATAL_ERROR, &dec_buffs[0], &dec_buffs[1], &cnt,
                     prec_increment);
      val->swap(dec_buffs[0]);
    } else
      my_decimal2decimal(&dec_buffs[1], val);

    null_value = (m_count == m_frame_null_count);
    my_decimal tmp(*val);
    m_avg_dec.swap(tmp);  // save result
    return val;
  } else {
    if (aggr) aggr->endup();
    if (!m_count) {
      null_value = true;
      return nullptr;
    }

    /*
     For non-DECIMAL hybrid_type the division will be done in
     Item_sum_avg::val_real().
     */
    if (hybrid_type != DECIMAL_RESULT) {
      my_decimal *result = val_decimal_from_real(val);
      return result;
    }

    sum_dec = dec_buffs + curr_dec_buff;
    int2my_decimal(E_DEC_FATAL_ERROR, m_count, false, &cnt);
    my_decimal_div(E_DEC_FATAL_ERROR, val, sum_dec, &cnt, prec_increment);
    return val;
  }
}

String *Item_sum_avg::val_str(String *str) {
  if (aggr) aggr->endup();
  if (hybrid_type == DECIMAL_RESULT) return val_string_from_decimal(str);
  return val_string_from_real(str);
}

/*
  Standard deviation
*/

double Item_sum_std::val_real() {
  assert(fixed == 1);
  double nr = Item_sum_variance::val_real();

  assert(nr >= 0.0);

  return sqrt(nr);
}

Item *Item_sum_std::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_std(thd, this);
  return result;
}

/*
  Variance function has two implementations:
  The first implementation (Algorithm I - see Item_sum_variance) is based
  on Knuth's _TAoCP_, 3rd ed, volume 2, pg232. This alters the value at
  m, s, and increments count.
  The second implementation (Algorithm II - See Item_sum_variance)
  initializes 'm' to the first sample and uses a different formula to
  get s, s^2. This implementation allows incremental computation which
  is used in optimizing windowing functions with frames.
  By default, group aggregates and windowing functions use algorithm I.
  Algorithm II is used when user explicitly requests optimized way of
  calculating variance if frames are present.

  variance_fp_recurrence_next calculates the recurrence values m,s used in
  algorithm I.
  add_sample/remove_sample calculates the recurrence values m,s,s2 used in
  algorithm II.
*/

/**
  Calculates the next recurrence value s,s2 using the current sample
  as input. m is initialized to the first sample. Its not changed for the
  later calls.

  @param[in,out] m     recurrence value
  @param[in,out] s     recurrence value
  @param[in,out] s2    Square of the recurrence value s
  @param[in,out] count Number of rows for which m,s,s2 is calculated
  @param[in]     nr    Current sample
*/
static void add_sample(double *m, double *s, double *s2, ulonglong *count,
                       double nr) {
  *count += 1;
  if (*count == 1) {
    *m = nr;
    *s = 0;
    *s2 = 0;
  } else {
    *s += nr - *m;
    *s2 += (nr - *m) * (nr - *m);
  }
}

/**
  Removes the earlier calculated recurrence value s,s2 for current
  sample from the current s,s2 values. Called when do_inverse()
  is true.

  @param[in]     m     recurrence value
  @param[in,out] s     recurrence value
  @param[in,out] s2    Square of the recurrence value s
  @param[in,out] count Number of rows for which s,s2 is calculated
  @param[in]     nr    Current sample
*/
static void remove_sample(double *m, double *s, double *s2, ulonglong *count,
                          double nr) {
  *count -= 1;
  *s -= (nr - *m);
  *s2 -= (nr - *m) * (nr - *m);
}

/**
  Calculates the next recurrence value for current sample.

  @param[in]     self  The object on which behalf we are computing
  @param[in,out] m     recurrence value
  @param[in,out] s     recurrence value
  @param[in,out] s2    Square of the recurrence value s
  @param[in,out] count Number of rows for which m,s,s2 is calculated
  @param[in] nr        Current sample
  @param[in] optimize  If set to true is Algorithm II is used to calculate
                       m,s and s2. Else Algorithm I is used to calculate
                       m,s.
  @param[in] inverse   If set to true, we use formulas from Algorithm II
                       to remove value calculated for s,s2 for sample "nr"
                       from the the current value of (s,s2).

  @returns false if success, true if error

  Note:
  variance_fp_recurrence_next and variance_fp_recurrence_result are used by
  Item_sum_variance and Item_variance_field classes, which are unrelated,
  and each need to calculate variance. The difference between the two
  classes is that the first is used for a mundane SELECT and when used with
  windowing functions, while the latter is used in a GROUPing SELECT.
*/
static bool variance_fp_recurrence_next(Item_sum_variance *self, double *m,
                                        double *s, double *s2, ulonglong *count,
                                        double nr, bool optimize,
                                        bool inverse) {
  assert(!std::isnan(*m));
  assert(!std::isnan(*s));
  assert(s2 == nullptr || !std::isnan(*s2));
  assert(!std::isnan(nr));

  assert(!std::isinf(*m));
  assert(!std::isinf(*s));
  assert(s2 == nullptr || !std::isinf(*s2));
  assert(!std::isinf(nr));

  if (optimize) {
    if (inverse)
      remove_sample(m, s, s2, count, nr);
    else
      add_sample(m, s, s2, count, nr);
  } else {
    *count += 1;

    if (*count == 1) {
      *m = nr;
      *s = 0;
    } else {
      double m_kminusone = *m;
      *m = m_kminusone + (nr - m_kminusone) / (double)*count;
      *s = *s + (nr - m_kminusone) * (nr - *m);
    }
  }
  *m = self->check_float_overflow(*m);
  *s = self->check_float_overflow(*s);
  if (s2 != nullptr) *s2 = self->check_float_overflow(*s2);
  return current_thd->is_error();
}

/**
  Calculates variance using one of the two algorithms
  (See Item_sum_variance) as specified.

  @param[in] s                  Recurrence value
  @param[in] s2                 Square of the recurrence value. Used
                                only by Algorithm II
  @param[in] count              Number of rows for which variance needs
                                to be calculated.
  @param[in] is_sample_variance True if calculating sample variance and
                                false if population variance.
  @param[in] optimize           True if algorithm II is used to calculate
                                variance.

  @retval                       Returns calculated variance value

*/
static double variance_fp_recurrence_result(double s, double s2,
                                            ulonglong count,
                                            bool is_sample_variance,
                                            bool optimize) {
  if (count == 1) return 0.0;

  if (optimize) {
    double variance = is_sample_variance
                          ? ((s2 - (s * s) / count) / (count - 1))
                          : ((s2 - (s * s) / count) / count);

    /*
      In optimized code path, we might see a rounding error while
      calculating recurrence_s2 in remove_sample leading to negative
      variance (happens rarely). Fix this.
    */
    if (variance < 0.0) return 0.0;

    return variance;
  }

  return is_sample_variance ? (s / (count - 1)) : (s / count);
}

Item_sum_variance::Item_sum_variance(THD *thd, Item_sum_variance *item)
    : Item_sum_num(thd, item),
      hybrid_type(item->hybrid_type),
      count(item->count),
      sample(item->sample),
      prec_increment(item->prec_increment),
      optimize(item->optimize) {
  recurrence_m = item->recurrence_m;
  recurrence_s = item->recurrence_s;
  recurrence_s2 = item->recurrence_s2;
}

bool Item_sum_variance::check_wf_semantics1(THD *thd, Query_block *select,
                                            Window_evaluation_requirements *r) {
  bool result = Item_sum::check_wf_semantics1(thd, select, r);
  const PT_frame *f = m_window->frame();
  if (f->m_from->m_border_type == WBT_VALUE_PRECEDING ||
      f->m_from->m_border_type == WBT_VALUE_FOLLOWING ||
      f->m_from->m_border_type == WBT_CURRENT_ROW) {
    optimize = !thd->variables.windowing_use_high_precision;
    r->row_optimizable &= optimize;
    r->range_optimizable &= optimize;
  } else
    r->row_optimizable = r->range_optimizable = optimize = false;

  return result;
}

bool Item_sum_variance::resolve_type(THD *thd) {
  DBUG_TRACE;
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_NEWDECIMAL)) return true;
  set_nullable(true);
  null_value = true;

  /*
    According to the SQL2003 standard (Part 2, Foundations; sec 10.9,
    aggregate function; paragraph 7h of Syntax Rules), "the declared
    type of the result is an implementation-defined approximate numeric
    type.
  */
  set_data_type_double();
  hybrid_type = REAL_RESULT;

  if (reject_geometry_args(arg_count, args, this)) return true;
  DBUG_PRINT("info", ("Type: REAL_RESULT (%d, %d)", max_length, (int)decimals));
  return false;
}

Item *Item_sum_variance::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result = m_is_window_function ? this
                                      : new (thd->mem_root)
                                            Item_sum_variance(thd, this);
  return result;
}

/**
  Create a new field to match the type of value we're expected to yield.
  If we're grouping, then we need some space to serialize variables into, to
  pass around.
*/
Field *Item_sum_variance::create_tmp_field(bool group, TABLE *table) {
  DBUG_TRACE;
  Field *field;
  if (group) {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    field =
        new (*THR_MALLOC) Field_string(sizeof(double) * 2 + sizeof(longlong),
                                       false, item_name.ptr(), &my_charset_bin);
  } else
    field = new (*THR_MALLOC) Field_double(
        max_length, is_nullable(), item_name.ptr(), decimals, false, true);

  if (field != nullptr) field->init(table);

  return field;
}

void Item_sum_variance::clear() { count = 0; }

bool Item_sum_variance::add() {
  /*
    Why use a temporary variable?  We don't know if it is null until we
    evaluate it, which has the side-effect of setting null_value .
  */
  double nr = args[0]->val_real();
  if (current_thd->is_error()) {
    return true;
  }

  if (!args[0]->null_value) {
    if (variance_fp_recurrence_next(
            this, &recurrence_m, &recurrence_s, &recurrence_s2, &count, nr,
            optimize, m_is_window_function ? m_window->do_inverse() : false))
      return true;
  }

  null_value = (count <= sample);
  return false;
}

double Item_sum_variance::val_real() {
  assert(fixed == 1);

  /*
    'sample' is a 1/0 boolean value.  If it is 1/true, id est this is a sample
    variance call, then we should set nullness when the count of the items
    is one or zero.  If it's zero, i.e. a population variance, then we only
    set nullness when the count is zero.

    Another way to read it is that 'sample' is the numerical threshold, at and
    below which a 'count' number of items is called NULL.
  */
  assert((sample == 0) || (sample == 1));
  if (m_is_window_function) {
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for a window function, which does not use Aggregator, it has to be called
      here.
    */
    if (wf_common_init()) return 0.0;
    if (add()) return error_real();
    if (null_value) return 0.0;
  } else if ((null_value = (count <= sample)))
    return 0.0;

  assert(!null_value);
  return variance_fp_recurrence_result(recurrence_s, recurrence_s2, count,
                                       sample, optimize);
}

my_decimal *Item_sum_variance::val_decimal(my_decimal *dec_buf) {
  assert(fixed == 1);
  return val_decimal_from_real(dec_buf);
}

void Item_sum_variance::reset_field() {
  double nr;
  uchar *res = result_field->field_ptr();

  nr = args[0]->val_real(); /* sets null_value as side-effect */

  if (args[0]->null_value)
    memset(res, 0, sizeof(double) * 2 + sizeof(longlong));
  else {
    /* Serialize format is (double)m, (double)s, (longlong)count */
    ulonglong tmp_count;
    double tmp_s;
    float8store(res, nr); /* recurrence variable m */
    tmp_s = 0.0;
    float8store(res + sizeof(double), tmp_s);
    tmp_count = 1;
    int8store(res + sizeof(double) * 2, tmp_count);
  }
}

void Item_sum_variance::update_field() {
  ulonglong field_count;
  uchar *res = result_field->field_ptr();

  double nr = args[0]->val_real(); /* sets null_value as side-effect */

  if (args[0]->null_value) return;

  /* Serialize format is (double)m, (double)s, (longlong)count */
  double field_recurrence_m = float8get(res);
  double field_recurrence_s = float8get(res + sizeof(double));
  field_count = sint8korr(res + sizeof(double) * 2);

  if (variance_fp_recurrence_next(this, &field_recurrence_m,
                                  &field_recurrence_s, nullptr, &field_count,
                                  nr, false, false))
    return;

  float8store(res, field_recurrence_m);
  float8store(res + sizeof(double), field_recurrence_s);
  res += sizeof(double) * 2;
  int8store(res, field_count);
}

/* min & max */

void Item_sum_hybrid::clear() {
  value->clear();
  value->store(args[0]);
  arg_cache->clear();
  arg_cache->store(args[0]);
  null_value = true;
  m_cnt = 0;
  m_saved_last_value_at = 0;
}

void Item_sum_hybrid::update_after_wf_arguments_changed(THD *) {
  value->setup(args[0]);
  arg_cache->setup(args[0]);
}

bool Item_sum_hybrid::check_wf_semantics1(THD *thd, Query_block *select,
                                          Window_evaluation_requirements *r) {
  bool result = Item_sum::check_wf_semantics1(thd, select, r);

  const PT_order_list *order = m_window->effective_order_by();
  if (order != nullptr) {
    ORDER *o = order->value.first;
    // The logic below (see class's doc) makes sense only for MIN and MAX
    assert(sum_func() == MIN_FUNC || sum_func() == MAX_FUNC);
    if ((*o->item)->real_item()->eq(args[0]->real_item(), false)) {
      if (r->row_optimizable || r->range_optimizable) {
        m_optimize = true;
        value->setup(args[0]);  // no comparisons needed
        if (o->direction == ORDER_ASC) {
          r->opt_first_row = m_is_min ? true : r->opt_first_row;
          r->opt_last_row = !m_is_min ? true : r->opt_last_row;
          m_want_first = m_is_min;
          m_nulls_first = true;
        } else {
          r->opt_last_row = m_is_min ? true : r->opt_last_row;
          r->opt_first_row = !m_is_min ? true : r->opt_first_row;
          m_want_first = !m_is_min;
          m_nulls_first = false;
        }
      }
    }
  }
  if (!m_optimize) {
    r->row_optimizable = false;
    r->range_optimizable = false;
  }
  return result;
}

bool Item_sum_hybrid::compute() {
  m_cnt++;

  if (m_window->do_inverse()) {
    null_value = true;
    return true;
  }

  /*
    We have four cases:
               m_want_first  m_nulls_first
          (1)       F            F
          (2)       F            T
          (3)       T            F
          (4)       T            T

    Since we want non-null values if present, special handling is needed for
    (1) and (4), i.e. those cases where we have to potentially[1] ignore nulls
    before (4) or after (1) a non-null value in a frame.

    [1] If we have a frame stretching back or forward to a non-null.
  */
  if (m_want_first != m_nulls_first) {
    // Cases (2) and (3): same structure as Item_first_last_value::compute

    const bool visiting_first_in_frame =
        (m_window->optimizable_row_aggregates() &&
         m_window->rowno_being_visited() ==
             m_window->first_rowno_in_rows_frame()) ||
        !m_window->optimizable_row_aggregates();

    if ((m_window->needs_buffering() &&
         (((m_window->rowno_in_frame() == 1 && m_want_first &&
            visiting_first_in_frame) ||
           (m_window->is_last_row_in_frame() && !m_want_first)) ||
          m_window->rowno_being_visited() == 0 /* No FROM; one const row */)) ||
        (!m_window->needs_buffering() &&
         ((m_want_first && m_cnt == 1) || !m_want_first))) {
      value->cache_value();
      null_value = value->null_value;
    }
  } else if (m_want_first) {
    /*
      Case (4) Handle potential nulls before non-null. If we don't find a
      non-NULL value on the first row of the frame, try on succeeding rows.
      If the first row in the frame never is a non-NULL, the value is still set
      when evaluating the last row (which will cover all rows in the frame at
      one time or another); in the priming (non-optimized) loop or in the
      optimized loop; see more below.
    */
    if ((m_window->needs_buffering() &&
         ((m_window->rowno_in_frame() == 1) ||
          (null_value && m_window->rowno_in_frame() > 1) ||
          m_window->rowno_being_visited() == 0 /* No FROM; one const row */)) ||
        (!m_window->needs_buffering() && m_cnt == 1)) {
      assert(m_nulls_first);
      value->store_and_cache(args[0]);
      null_value = value->null_value;

      if (!null_value) {
        /*
          In optimized mode with a moving frame, the visit pattern[1] is:
             invert N-1, read N (new first).. read M (new last).

          [1] in process_buffered_windowing_record

          The first time we find a non-null value can actually be[2] when we,
          in optimized mode, have discovered that we have a now last row,
          cf. the branch in [1]:

             if (new_last_row) ..

          Since this will be first non-null row in this case, it will be
          the MIN (or MAX is descending sort) until it goes out of frame.

          When we next read the new first in a moving frame (N+1), if the value
          if NULL, we already have the value cached, and use it, see "else if".

          [2] if the frame for the first row in the partition didn't see a non-
          NULL row under priming (non-optimized loop in [1]).
        */
        arg_cache->store_and_cache(value);
      } else if (!arg_cache->null_value) {
        value->store_and_cache(arg_cache);
        null_value = value->null_value;
      }
    }
  } else {
    /*
      Case (1) Handle potential nulls after non-null. If we see a NULL, reuse
      any earlier seen non-NULL value as long as that value is still in
      frame.
    */
    if ((m_window->needs_buffering() &&
         ((m_window->is_last_row_in_frame()) ||
          m_window->rowno_being_visited() == 0 /* No FROM; one const row */)) ||
        (!m_window->needs_buffering())) {
      value->store_and_cache(args[0]);
      null_value = value->null_value;

      const int64 frame_start = m_window->optimizable_row_aggregates()
                                    ? m_window->first_rowno_in_rows_frame()
                                    : (m_window->rowno_being_visited() -
                                       m_window->rowno_in_frame() + 1);

      if (!value->null_value &&
          m_window->rowno_being_visited() > m_saved_last_value_at) {
        arg_cache->store_and_cache(value);
        m_saved_last_value_at = m_window->rowno_being_visited();
      } else if (m_saved_last_value_at >= frame_start) {
        assert(!m_nulls_first);
        value->store_and_cache(arg_cache);
        null_value = value->null_value;
      }
    }
  }
  return null_value || current_thd->is_error();
}

double Item_sum_hybrid::val_real() {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0.0;
    bool ret = false;
    m_optimize ? ret = compute() : add();
    if (ret) return error_real();
  }
  if (null_value) return 0.0;
  double retval = value->val_real();
  if ((null_value = value->null_value)) assert(retval == 0.0);
  return retval;
}

longlong Item_sum_hybrid::val_int() {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0;
    bool ret = false;
    m_optimize ? ret = compute() : add();
    if (ret) return error_int();
  }
  if (null_value) return 0;
  longlong retval = value->val_int();
  if ((null_value = value->null_value)) assert(retval == 0);
  return retval;
}

longlong Item_sum_hybrid::val_time_temporal() {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0;
    if (m_optimize ? compute() : add()) return 0;
  }
  if (null_value) return 0;
  longlong retval = value->val_time_temporal();
  if ((null_value = value->null_value)) assert(retval == 0);
  return retval;
}

longlong Item_sum_hybrid::val_date_temporal() {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return 0;
    if (m_optimize ? compute() : add()) return 0;
  }
  if (null_value) return 0;
  longlong retval = value->val_date_temporal();
  if ((null_value = value->null_value)) assert(retval == 0);
  return retval;
}

my_decimal *Item_sum_hybrid::val_decimal(my_decimal *val) {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) {
      return error_decimal(val);
    }
    bool ret = false;
    m_optimize ? ret = compute() : add();
    if (ret) return nullptr;
  }
  if (null_value) return nullptr;
  my_decimal *retval = value->val_decimal(val);
  if ((null_value = value->null_value))
    assert(retval == nullptr || my_decimal_is_zero(retval));
  return retval;
}

bool Item_sum_hybrid::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return true;
    if (m_optimize ? compute() : add()) return true;
  }
  if (null_value) return true;
  return (null_value = value->get_date(ltime, fuzzydate));
}

bool Item_sum_hybrid::get_time(MYSQL_TIME *ltime) {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return true;
    if (m_optimize ? compute() : add()) return true;
  }
  if (null_value) return true;
  return (null_value = value->get_time(ltime));
}

String *Item_sum_hybrid::val_str(String *str) {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return error_str();
    if (m_optimize ? compute() : add()) return error_str();
  }
  if (null_value) return nullptr;

  String *retval = value->val_str(str);
  if ((null_value = value->null_value)) assert(retval == nullptr);
  return retval;
}

bool Item_sum_hybrid::val_json(Json_wrapper *wr) {
  assert(fixed);
  if (m_is_window_function) {
    if (wf_common_init()) return false;  // NULL
    // compute() returns true both on error and NULL, so we need to check
    // THD::is_error() to see which it is.
    if (m_optimize ? compute() : add()) return current_thd->is_error();
  }
  if (null_value) return false;
  bool ok = value->val_json(wr);
  null_value = value->null_value;
  return ok;
}

void Item_sum_hybrid::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                                     mem_root_deque<Item *> *fields) {
  super::split_sum_func(thd, ref_item_array, fields);
  /*
    Grouped aggregate functions used as arguments to windowing functions get
    replaced with aggregate ref's in split_sum_func. So need to redo the cache
    setup.
  */
  update_after_wf_arguments_changed(thd);
}

void Item_sum_hybrid::cleanup() {
  DBUG_TRACE;
  Item_sum::cleanup();
  if (cmp != nullptr) cmp->cleanup();
  /*
    by default it is true to avoid true reporting by
    Item_func_not_all/Item_func_nop_all if this item was never called.

    no_rows_in_result() set it to false if was not results found.
    If some results found it will be left unchanged.
  */
  was_values = true;
}

void Item_sum_hybrid::no_rows_in_result() {
  was_values = false;
  clear();
}

Item *Item_sum_hybrid::copy_or_same(THD *thd) {
  if (m_is_window_function) return this;
  Item_sum_hybrid *item = clone_hybrid(thd);
  if (item == nullptr || item->setup_hybrid(args[0], value)) return nullptr;
  return item;
}

Item_sum_min *Item_sum_min::clone_hybrid(THD *thd) const {
  return new (thd->mem_root) Item_sum_min(thd, this);
}

Item_sum_max *Item_sum_max::clone_hybrid(THD *thd) const {
  return new (thd->mem_root) Item_sum_max(thd, this);
}

/**
  Checks if a value should replace the minimum or maximum value seen so far in
  the MIN and MAX aggregate functions.

  @param comparison_result  the result of comparing the current value with the
                            min/max value seen so far (negative if it's
                            smaller, 0 if it's equal, positive if it's greater)
  @param is_min  true if called by MIN, false if called by MAX

  @return true if the current value should replace the min/max value seen so far
*/
static bool min_max_best_so_far(int comparison_result, bool is_min) {
  return is_min ? comparison_result < 0 : comparison_result > 0;
}

bool Item_sum_hybrid::add() {
  arg_cache->cache_value();
  if (current_thd->is_error()) {
    return true;
  }
  if (!arg_cache->null_value &&
      (null_value || min_max_best_so_far(cmp->compare(), m_is_min))) {
    value->store(arg_cache);
    value->cache_value();
    if (current_thd->is_error()) {
      return true;
    }
    null_value = false;
  }
  return false;
}

String *Item_sum_bit::val_str(String *str) {
  if (m_is_window_function) {
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for a window function, which does not use Aggregator, it has to be called
      here.
    */
    if (!wf_common_init()) {
      if (add()) return error_str();
    }
  }

  if (hybrid_type == INT_RESULT) return val_string_from_int(str);

  assert(value_buff.length() > 0);
  const bool non_nulls = value_buff[value_buff.length() - 1];
  // If the group has no non-NULLs repeat the default value max_length times.
  if (!non_nulls) {
    str->length(0);
    if (str->fill(max_length - 1, static_cast<char>(reset_bits)))
      return error_str();
    str->set_charset(&my_charset_bin);
  } else {
    // Prepare the result (skip the flag at the end)
    if (str->copy(value_buff.ptr(), value_buff.length() - 1, &my_charset_bin))
      return error_str();
  }

  return str;
}

bool Item_sum_bit::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  if (hybrid_type == INT_RESULT)
    return get_date_from_int(ltime, fuzzydate);
  else
    return get_date_from_string(ltime, fuzzydate);
}

bool Item_sum_bit::get_time(MYSQL_TIME *ltime) {
  if (hybrid_type == INT_RESULT)
    return get_time_from_int(ltime);
  else
    return get_time_from_string(ltime);
}

my_decimal *Item_sum_bit::val_decimal(my_decimal *dec_buf) {
  if (m_is_window_function) {
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for a window function, which does not use Aggregator, it has be called
      here.
    */
    if (!wf_common_init()) {
      if (add()) return error_decimal(dec_buf);
    }
  }

  if (hybrid_type == INT_RESULT)
    return val_decimal_from_int(dec_buf);
  else
    return val_decimal_from_string(dec_buf);
}

double Item_sum_bit::val_real() {
  assert(fixed);

  if (m_is_window_function) {
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for a window function, which does not use Aggregator, it has be called
      here.
    */
    if (!wf_common_init()) {
      if (add()) return error_real();
    }
  }

  if (hybrid_type == INT_RESULT) return bits;
  String *res;
  if (!(res = val_str(&str_value))) return 0.0;

  int ovf_error;
  const char *from = res->ptr();
  size_t len = res->length();
  const char *end = from + len;
  return my_strtod(from, &end, &ovf_error);
}
/* bit_or and bit_and */

longlong Item_sum_bit::val_int() {
  assert(fixed);
  if (m_is_window_function) {
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for a window function, which does not use Aggregator, it has be called
      here.
    */
    if (!wf_common_init()) {
      if (add()) return error_int();
    }
  }

  if (hybrid_type == INT_RESULT) return (longlong)bits;

  String *res;
  if (!(res = val_str(&str_value))) return 0;

  int ovf_error;
  const char *from = res->ptr();
  size_t len = res->length();
  const char *end = from + len;
  return my_strtoll10(from, &end, &ovf_error);
}

void Item_sum_bit::clear() {
  if (hybrid_type == INT_RESULT)
    bits = reset_bits;
  else {
    // Prepare value_buff for a new group: no non-NULLs seen.
    value_buff[value_buff.length() - 1] = 0;
  }
  m_count = 0;
  m_frame_null_count = 0;
  if (m_digit_cnt != nullptr) {
    std::memset(m_digit_cnt, 0, m_digit_cnt_card * sizeof(ulonglong));
  }
}

Item *Item_sum_or::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_or(thd, this);
  return result;
}

Item *Item_sum_xor::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_xor(thd, this);
  return result;
}

Item *Item_sum_and::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result =
      m_is_window_function ? this : new (thd->mem_root) Item_sum_and(thd, this);
  return result;
}

/************************************************************************
** reset result of a Item_sum with is saved in a tmp_table
*************************************************************************/

void Item_sum_num::reset_field() {
  double nr = args[0]->val_real();

  if (is_nullable()) {
    if (args[0]->null_value) {
      nr = 0.0;
      result_field->set_null();
    } else
      result_field->set_notnull();
  }
  float8store(result_field->field_ptr(), nr);
}

void Item_sum_hybrid::reset_field() {
  switch (hybrid_type) {
    case STRING_RESULT: {
      if (args[0]->is_temporal()) {
        longlong nr = args[0]->val_temporal_by_field_type();
        if (is_nullable()) {
          if (args[0]->null_value) {
            nr = 0;
            result_field->set_null();
          } else
            result_field->set_notnull();
        }
        result_field->store_packed(nr);
        break;
      }

      char buff[MAX_FIELD_WIDTH];
      String tmp(buff, sizeof(buff), result_field->charset()), *res;

      res = args[0]->val_str(&tmp);
      if (args[0]->null_value) {
        result_field->set_null();
        result_field->reset();
      } else {
        result_field->set_notnull();
        result_field->store(res->ptr(), res->length(), tmp.charset());
      }
      break;
    }
    case INT_RESULT: {
      longlong nr = args[0]->val_int();

      if (is_nullable()) {
        if (args[0]->null_value) {
          nr = 0;
          result_field->set_null();
        } else
          result_field->set_notnull();
      }
      result_field->store(nr, unsigned_flag);
      break;
    }
    case REAL_RESULT: {
      double nr = args[0]->val_real();

      if (is_nullable()) {
        if (args[0]->null_value) {
          nr = 0.0;
          result_field->set_null();
        } else
          result_field->set_notnull();
      }
      result_field->store(nr);
      break;
    }
    case DECIMAL_RESULT: {
      my_decimal value_buff, *arg_dec = args[0]->val_decimal(&value_buff);

      if (is_nullable()) {
        if (args[0]->null_value)
          result_field->set_null();
        else
          result_field->set_notnull();
      }
      /*
        We must store zero in the field as we will use the field value in
        add()
      */
      if (!arg_dec)  // Null
        arg_dec = &decimal_zero;
      result_field->store_decimal(arg_dec);
      break;
    }
    case ROW_RESULT:
    default:
      assert(0);
  }
}

void Item_sum_sum::reset_field() {
  assert(aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT) {
    my_decimal value, *arg_val = args[0]->val_decimal(&value);
    if (!arg_val)  // Null
      arg_val = &decimal_zero;
    result_field->store_decimal(arg_val);
  } else {
    assert(hybrid_type == REAL_RESULT);
    double nr = args[0]->val_real();  // Nulls also return 0
    float8store(result_field->field_ptr(), nr);
  }
  if (args[0]->null_value)
    result_field->set_null();
  else
    result_field->set_notnull();
}

void Item_sum_count::reset_field() {
  longlong nr = 0;
  assert(aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);

  if (!args[0]->is_nullable() || !args[0]->is_null()) nr = 1;
  int8store(result_field->field_ptr(), nr);
}

void Item_sum_avg::reset_field() {
  uchar *res = result_field->field_ptr();
  assert(aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT) {
    longlong tmp;
    my_decimal value, *arg_dec = args[0]->val_decimal(&value);
    if (args[0]->null_value) {
      arg_dec = &decimal_zero;
      tmp = 0;
    } else
      tmp = 1;
    my_decimal2binary(E_DEC_FATAL_ERROR, arg_dec, res, f_precision, f_scale);
    res += dec_bin_size;
    int8store(res, tmp);
  } else {
    double nr = args[0]->val_real();

    if (args[0]->null_value)
      memset(res, 0, sizeof(double) + sizeof(longlong));
    else {
      longlong tmp = 1;
      float8store(res, nr);
      res += sizeof(double);
      int8store(res, tmp);
    }
  }
}

void Item_sum_bit::reset_field() {
  reset_and_add();
  if (hybrid_type == INT_RESULT)
    // Store the result in result_field
    result_field->store(bits, unsigned_flag);
  else
    result_field->store(value_buff.ptr(), value_buff.length(),
                        value_buff.charset());
}

void Item_sum_bit::update_field() {
  if (hybrid_type == INT_RESULT) {
    // Restore previous value to bits
    bits = result_field->val_int();
    // Add the current value to the group determined value.
    add();
    // Store the value in the result_field
    result_field->store(bits, unsigned_flag);
  } else  // hybrid_type == STRING_RESULT
  {
    // Restore previous value to result_field
    result_field->val_str(&value_buff);
    // Add the current value to the previously determined one
    add();
    // Store the value in the result_field
    result_field->store(value_buff.ptr(), value_buff.length(),
                        default_charset());
  }
}

/**
  calc next value and merge it with field_value.
*/

void Item_sum_sum::update_field() {
  DBUG_TRACE;
  assert(aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);
  if (hybrid_type == DECIMAL_RESULT) {
    my_decimal value, *arg_val = args[0]->val_decimal(&value);
    if (!args[0]->null_value) {
      if (!result_field->is_null()) {
        my_decimal field_value,
            *field_val = result_field->val_decimal(&field_value);
        my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, field_val);
        result_field->store_decimal(dec_buffs);
      } else {
        result_field->store_decimal(arg_val);
        result_field->set_notnull();
      }
    }
  } else {
    uchar *res = result_field->field_ptr();

    double old_nr = float8get(res);
    double nr = args[0]->val_real();
    if (!args[0]->null_value) {
      old_nr += nr;
      result_field->set_notnull();
    }
    float8store(res, old_nr);
  }
}

void Item_sum_count::update_field() {
  longlong nr;
  uchar *res = result_field->field_ptr();

  nr = sint8korr(res);
  if (!args[0]->is_nullable() || !args[0]->is_null()) nr++;
  int8store(res, nr);
}

void Item_sum_avg::update_field() {
  DBUG_TRACE;
  longlong field_count;
  uchar *res = result_field->field_ptr();

  assert(aggr->Aggrtype() != Aggregator::DISTINCT_AGGREGATOR);

  if (hybrid_type == DECIMAL_RESULT) {
    my_decimal value, *arg_val = args[0]->val_decimal(&value);
    if (!args[0]->null_value) {
      binary2my_decimal(E_DEC_FATAL_ERROR, res, dec_buffs + 1, f_precision,
                        f_scale);
      field_count = sint8korr(res + dec_bin_size);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, dec_buffs + 1);
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_buffs, res, f_precision,
                        f_scale);
      res += dec_bin_size;
      field_count++;
      int8store(res, field_count);
    }
  } else {
    double nr;

    nr = args[0]->val_real();
    if (!args[0]->null_value) {
      double old_nr = float8get(res);
      field_count = sint8korr(res + sizeof(double));
      old_nr += nr;
      float8store(res, old_nr);
      res += sizeof(double);
      field_count++;
      int8store(res, field_count);
    }
  }
}

void Item_sum_hybrid::update_field() {
  switch (hybrid_type) {
    case STRING_RESULT:
      if (args[0]->is_temporal())
        min_max_update_temporal_field();
      else if (data_type() == MYSQL_TYPE_JSON)
        min_max_update_json_field();
      else
        min_max_update_str_field();
      break;
    case INT_RESULT:
      min_max_update_int_field();
      break;
    case DECIMAL_RESULT:
      min_max_update_decimal_field();
      break;
    default:
      min_max_update_real_field();
  }
}

void Item_sum_hybrid::min_max_update_temporal_field() {
  const longlong nr = args[0]->val_temporal_by_field_type();
  if (args[0]->null_value) return;

  if (result_field->is_null()) {
    result_field->set_notnull();
  } else {
    const longlong old_nr = result_field->val_temporal_by_field_type();
    if (!min_max_best_so_far(
            unsigned_flag ? compare_numbers(ulonglong(nr), ulonglong(old_nr))
                          : compare_numbers(nr, old_nr),
            m_is_min))
      return;
  }

  result_field->store_packed(nr);
}

void Item_sum_hybrid::min_max_update_json_field() {
  Json_wrapper json1;
  if (args[0]->val_json(&json1)) return;
  if (args[0]->null_value) return;

  Field_json *const json_field = down_cast<Field_json *>(result_field);
  if (json_field->is_null()) {
    json_field->set_notnull();
  } else {
    Json_wrapper json2;
    if (json_field->val_json(&json2) ||
        !min_max_best_so_far(json1.compare(json2), m_is_min))
      return;
  }

  json_field->store_json(&json1);
}

void Item_sum_hybrid::min_max_update_str_field() {
  assert(cmp);
  const String *const res_str = args[0]->val_str(&cmp->value1);
  if (args[0]->null_value) return;

  if (result_field->is_null())
    result_field->set_notnull();
  else if (!min_max_best_so_far(
               sortcmp(res_str, result_field->val_str(&cmp->value2),
                       collation.collation),
               m_is_min))
    return;

  result_field->store(res_str->ptr(), res_str->length(), res_str->charset());
}

void Item_sum_hybrid::min_max_update_real_field() {
  const double nr = args[0]->val_real();
  if (args[0]->null_value) return;

  if (result_field->is_null())
    result_field->set_notnull();
  else if (!min_max_best_so_far(compare_numbers(nr, result_field->val_real()),
                                m_is_min))
    return;

  result_field->store(nr);
}

void Item_sum_hybrid::min_max_update_int_field() {
  const longlong nr = args[0]->val_int();
  if (args[0]->null_value) return;

  if (result_field->is_null()) {
    result_field->set_notnull();
  } else {
    const longlong old_nr = result_field->val_int();
    if (!min_max_best_so_far(
            unsigned_flag ? compare_numbers(ulonglong(nr), ulonglong(old_nr))
                          : compare_numbers(nr, old_nr),
            m_is_min))
      return;
  }

  result_field->store(nr, unsigned_flag);
}

void Item_sum_hybrid::min_max_update_decimal_field() {
  my_decimal nr_val;
  const my_decimal *const nr = args[0]->val_decimal(&nr_val);
  if (args[0]->null_value) return;

  if (result_field->is_null()) {
    result_field->set_notnull();
  } else {
    my_decimal old_val;
    const my_decimal *const old_nr = result_field->val_decimal(&old_val);
    if (!min_max_best_so_far(my_decimal_cmp(nr, old_nr), m_is_min)) return;
  }

  result_field->store_decimal(nr);
}

Item_avg_field::Item_avg_field(Item_result res_type, Item_sum_avg *item) {
  assert(!item->m_is_window_function);
  item_name = item->item_name;
  decimals = item->decimals;
  max_length = item->max_length;
  unsigned_flag = item->unsigned_flag;
  field = item->get_result_field();
  set_nullable(true);
  hybrid_type = res_type;
  set_data_type(hybrid_type == DECIMAL_RESULT ? MYSQL_TYPE_NEWDECIMAL
                                              : MYSQL_TYPE_DOUBLE);
  prec_increment = item->prec_increment;
  if (hybrid_type == DECIMAL_RESULT) {
    f_scale = item->f_scale;
    f_precision = item->f_precision;
    dec_bin_size = item->dec_bin_size;
  }
}

double Item_avg_field::val_real() {
  // fix_fields() never calls for this Item
  longlong count;
  uchar *res;

  if (hybrid_type == DECIMAL_RESULT) return val_real_from_decimal();

  double nr = float8get(field->field_ptr());
  res = (field->field_ptr() + sizeof(double));
  count = sint8korr(res);

  if ((null_value = !count)) return 0.0;
  return nr / (double)count;
}

my_decimal *Item_avg_field::val_decimal(my_decimal *dec_buf) {
  // fix_fields() never calls for this Item
  if (hybrid_type == REAL_RESULT) return val_decimal_from_real(dec_buf);
  longlong count = sint8korr(field->field_ptr() + dec_bin_size);
  if ((null_value = !count)) return nullptr;

  my_decimal dec_count, dec_field;
  binary2my_decimal(E_DEC_FATAL_ERROR, field->field_ptr(), &dec_field,
                    f_precision, f_scale);
  int2my_decimal(E_DEC_FATAL_ERROR, count, false, &dec_count);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &dec_field, &dec_count,
                 prec_increment);
  return dec_buf;
}

String *Item_avg_field::val_str(String *str) {
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT) return val_string_from_decimal(str);
  return val_string_from_real(str);
}

Item_sum_bit_field::Item_sum_bit_field(Item_result res_type, Item_sum_bit *item,
                                       ulonglong neutral_element) {
  assert(!item->m_is_window_function);
  reset_bits = neutral_element;
  item_name = item->item_name;
  decimals = item->decimals;
  max_length = item->max_length;
  unsigned_flag = item->unsigned_flag;
  field = item->get_result_field();
  set_nullable(false);
  hybrid_type = res_type;
  assert(hybrid_type == INT_RESULT || hybrid_type == STRING_RESULT);
  if (hybrid_type == INT_RESULT)
    set_data_type(MYSQL_TYPE_LONGLONG);
  else if (hybrid_type == STRING_RESULT)
    set_data_type(MYSQL_TYPE_VARCHAR);
  // Implementation requires a non-Blob for string results.
  assert(hybrid_type != STRING_RESULT || field->type() == MYSQL_TYPE_VARCHAR);
}

longlong Item_sum_bit_field::val_int() {
  if (hybrid_type == INT_RESULT)
    return uint8korr(field->field_ptr());
  else {
    String *res;
    if (!(res = val_str(&str_value))) return 0;

    int ovf_error;
    const char *from = res->ptr();
    size_t len = res->length();
    const char *end = from + len;
    return my_strtoll10(from, &end, &ovf_error);
  }
}

double Item_sum_bit_field::val_real() {
  if (hybrid_type == INT_RESULT) {
    ulonglong result = uint8korr(field->field_ptr());
    return result;
  } else {
    String *res;
    if (!(res = val_str(&str_value))) return 0.0;

    int ovf_error;
    const char *from = res->ptr();
    size_t len = res->length();
    const char *end = from + len;

    return my_strtod(from, &end, &ovf_error);
  }
}

my_decimal *Item_sum_bit_field::val_decimal(my_decimal *dec_buf) {
  if (hybrid_type == INT_RESULT)
    return val_decimal_from_int(dec_buf);
  else
    return val_decimal_from_string(dec_buf);
}

/// @see Item_sum_bit::val_str()
String *Item_sum_bit_field::val_str(String *str) {
  if (hybrid_type == INT_RESULT)
    return val_string_from_int(str);
  else {
    String *res_str = field->val_str(str);
    const bool non_nulls = res_str->ptr()[res_str->length() - 1];
    if (!non_nulls) {
      DBUG_EXECUTE_IF("simulate_sum_out_of_memory", { return nullptr; });
      if (res_str->alloc(max_length - 1)) return nullptr;
      std::memset(res_str->ptr(), static_cast<int>(reset_bits), max_length - 1);
      res_str->length(max_length - 1);
      res_str->set_charset(&my_charset_bin);
    } else
      res_str->length(res_str->length() - 1);
    return res_str;
  }
}

bool Item_sum_bit_field::get_date(MYSQL_TIME *ltime,
                                  my_time_flags_t fuzzydate) {
  if (hybrid_type == INT_RESULT)
    return get_date_from_decimal(ltime, fuzzydate);
  else
    return get_date_from_string(ltime, fuzzydate);
}
bool Item_sum_bit_field::get_time(MYSQL_TIME *ltime) {
  if (hybrid_type == INT_RESULT)
    return get_time_from_numeric(ltime);
  else
    return get_time_from_string(ltime);
}

Item_std_field::Item_std_field(Item_sum_std *item)
    : Item_variance_field(item) {}

double Item_std_field::val_real() {
  double nr;
  // fix_fields() never calls for this Item
  nr = Item_variance_field::val_real();
  assert(nr >= 0.0);
  return sqrt(nr);
}

my_decimal *Item_std_field::val_decimal(my_decimal *dec_buf) {
  /*
    We can't call val_decimal_from_real() for DECIMAL_RESULT as
    Item_variance_field::val_real() would cause an infinite loop
  */
  my_decimal tmp_dec, *dec;
  double nr;
  if (hybrid_type == REAL_RESULT) return val_decimal_from_real(dec_buf);

  dec = Item_variance_field::val_decimal(dec_buf);
  if (!dec) return nullptr;
  my_decimal2double(E_DEC_FATAL_ERROR, dec, &nr);
  assert(nr >= 0.0);
  nr = sqrt(nr);
  double2my_decimal(E_DEC_FATAL_ERROR, nr, &tmp_dec);
  my_decimal_round(E_DEC_FATAL_ERROR, &tmp_dec, decimals, false, dec_buf);
  return dec_buf;
}

Item_variance_field::Item_variance_field(Item_sum_variance *item) {
  assert(!item->m_is_window_function);
  item_name = item->item_name;
  decimals = item->decimals;
  max_length = item->max_length;
  unsigned_flag = item->unsigned_flag;
  field = item->get_result_field();
  set_nullable(true);
  sample = item->sample;
  hybrid_type = item->hybrid_type;
  assert(hybrid_type == REAL_RESULT);
  set_data_type(MYSQL_TYPE_DOUBLE);
}

double Item_variance_field::val_real() {
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT) return val_real_from_decimal();

  double recurrence_s = float8get(field->field_ptr() + sizeof(double));
  ulonglong count = uint8korr(field->field_ptr() + sizeof(double) * 2);

  if ((null_value = (count <= sample))) return 0.0;
  return variance_fp_recurrence_result(recurrence_s, 0.0, count, sample, false);
}

/****************************************************************************
** Functions to handle dynamic loadable aggregates
****************************************************************************/

bool Item_udf_sum::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
  pc->thd->lex->set_has_udf();
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UDF);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

void Item_udf_sum::clear() {
  DBUG_TRACE;
  assert(udf.is_initialized());
  udf.clear();
}

bool Item_udf_sum::add() {
  DBUG_TRACE;
  assert(udf.is_initialized());
  udf.add(&null_value);
  return false;
}

void Item_udf_sum::cleanup() {
  /*
    udf_handler::cleanup() nicely handles case when we have not
    original item but one created by copy_or_same() method.
  */
  udf.cleanup();
  Item_sum::cleanup();
}

void Item_udf_sum::print(const THD *thd, String *str,
                         enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');
  for (uint i = 0; i < arg_count; i++) {
    if (i) str->append(',');
    args[i]->print(thd, str, query_type);
  }
  str->append(')');
}

Item *Item_sum_udf_float::copy_or_same(THD *thd) {
  assert(udf.is_initialized());
  return new (thd->mem_root) Item_sum_udf_float(thd, this);
}

double Item_sum_udf_float::val_real() {
  DBUG_TRACE;
  assert(fixed);
  DBUG_PRINT("info", ("result_type: %d  arg_count: %d", args[0]->result_type(),
                      arg_count));
  return udf.val_real(&null_value);
}

String *Item_sum_udf_float::val_str(String *str) {
  return val_string_from_real(str);
}

my_decimal *Item_sum_udf_float::val_decimal(my_decimal *dec) {
  return val_decimal_from_real(dec);
}

String *Item_sum_udf_decimal::val_str(String *str) {
  return val_string_from_decimal(str);
}

double Item_sum_udf_decimal::val_real() { return val_real_from_decimal(); }

longlong Item_sum_udf_decimal::val_int() { return val_int_from_decimal(); }

my_decimal *Item_sum_udf_decimal::val_decimal(my_decimal *dec_buf) {
  assert(fixed == 1);
  DBUG_TRACE;
  DBUG_PRINT("info", ("result_type: %d  arg_count: %d", args[0]->result_type(),
                      arg_count));

  return udf.val_decimal(&null_value, dec_buf);
}

Item *Item_sum_udf_decimal::copy_or_same(THD *thd) {
  return new (thd->mem_root) Item_sum_udf_decimal(thd, this);
}

Item *Item_sum_udf_int::copy_or_same(THD *thd) {
  return new (thd->mem_root) Item_sum_udf_int(thd, this);
}

longlong Item_sum_udf_int::val_int() {
  assert(fixed == 1);
  DBUG_TRACE;
  DBUG_PRINT("info", ("result_type: %d  arg_count: %d", args[0]->result_type(),
                      arg_count));
  return udf.val_int(&null_value);
}

String *Item_sum_udf_int::val_str(String *str) {
  return val_string_from_int(str);
}

my_decimal *Item_sum_udf_int::val_decimal(my_decimal *dec) {
  return val_decimal_from_int(dec);
}

/** Default max_length is max argument length. */

bool Item_sum_udf_str::resolve_type(THD *) {
  set_data_type(MYSQL_TYPE_VARCHAR);
  max_length = 0;
  for (uint i = 0; i < arg_count; i++)
    max_length = max(max_length, args[i]->max_length);
  return false;
}

Item *Item_sum_udf_str::copy_or_same(THD *thd) {
  return new (thd->mem_root) Item_sum_udf_str(thd, this);
}

my_decimal *Item_sum_udf_str::val_decimal(my_decimal *dec) {
  return val_decimal_from_string(dec);
}

String *Item_sum_udf_str::val_str(String *str) {
  assert(fixed == 1);
  DBUG_TRACE;
  String *res = udf.val_str(str, &str_value);
  null_value = !res;
  return res;
}

/*****************************************************************************
 GROUP_CONCAT function

 SQL SYNTAX:
  GROUP_CONCAT([DISTINCT] expr,... [ORDER BY col [ASC|DESC],...]
    [SEPARATOR str_const])

 concat of values from "group by" operation

 BUGS
   Blobs doesn't work with DISTINCT or ORDER BY
*****************************************************************************/

/**
  Compares the values for fields in expr list of GROUP_CONCAT.

  @code
     GROUP_CONCAT([DISTINCT] expr [,expr ...]
              [ORDER BY {unsigned_integer | col_name | expr}
                  [ASC | DESC] [,col_name ...]]
              [SEPARATOR str_val])
  @endcode

  @retval -1 : key1 < key2
  @retval  0 : key1 = key2
  @retval  1 : key1 > key2
*/

int group_concat_key_cmp_with_distinct(const void *arg, const void *key1,
                                       const void *key2) {
  DBUG_TRACE;
  const Item_func_group_concat *item_func =
      static_cast<const Item_func_group_concat *>(arg);
  TABLE *table = item_func->table;

  for (uint i = 0; i < item_func->m_field_arg_count; i++) {
    Item *item = item_func->args[i];
    /*
      If item is a const item then either get_tmp_table_field returns 0
      or it is an item over a const table.
    */
    if (item->const_item()) continue;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field = item->get_tmp_table_field();

    if (!field) continue;

    uint offset = field->offset(field->table->record[0]) - table->s->null_bytes;
    int res = field->cmp(pointer_cast<const uchar *>(key1) + offset,
                         pointer_cast<const uchar *>(key2) + offset);
    if (res) return res;
  }
  return 0;
}

/**
  function of sort for syntax: GROUP_CONCAT(expr,... ORDER BY col,... )
*/

int group_concat_key_cmp_with_order(const void *arg, const void *key1,
                                    const void *key2) {
  DBUG_TRACE;
  const Item_func_group_concat *grp_item =
      static_cast<const Item_func_group_concat *>(arg);
  const ORDER *order_item, *end;
  TABLE *table = grp_item->table;

  for (order_item = grp_item->order_array.begin(),
      end = grp_item->order_array.end();
       order_item < end; order_item++) {
    Item *item = *(order_item)->item;
    /*
      If item is a const item then either get_tmp_table_field returns 0
      or it is an item over a const table.
    */
    if (item->const_item()) continue;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
     */
    Field *field = item->get_tmp_table_field();
    if (!field) continue;

    uint offset =
        (field->offset(field->table->record[0]) - table->s->null_bytes);
    int res = field->cmp(pointer_cast<const uchar *>(key1) + offset,
                         pointer_cast<const uchar *>(key2) + offset);
    if (res) return ((order_item)->direction == ORDER_ASC) ? res : -res;
  }
  /*
    We can't return 0 because in that case the tree class would remove this
    item as double value. This would cause problems for case-changes and
    if the returned values are not the same we do the sort on.
  */
  return 1;
}

/**
  Append data from current leaf to item->result.
*/

int dump_leaf_key(void *key_arg, element_count count [[maybe_unused]],
                  void *item_arg) {
  DBUG_TRACE;
  Item_func_group_concat *item = (Item_func_group_concat *)item_arg;
  TABLE *table = item->table;
  String tmp((char *)table->record[1], table->s->reclength,
             default_charset_info);
  String tmp2;
  uchar *key = (uchar *)key_arg;
  String *result = &item->result;
  Item **arg = item->args, **arg_end = item->args + item->m_field_arg_count;
  size_t old_length = result->length();

  if (!item->m_result_finalized)
    item->m_result_finalized = true;
  else
    result->append(*item->separator);

  tmp.length(0);

  for (; arg < arg_end; arg++) {
    String *res;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
      We also can't use table->field array to access the fields
      because it contains both order and arg list fields.
     */
    if ((*arg)->const_item())
      res = (*arg)->val_str(&tmp);
    else {
      Field *field = (*arg)->get_tmp_table_field();
      if (field) {
        uint offset =
            (field->offset(field->table->record[0]) - table->s->null_bytes);
        assert(offset < table->s->reclength);
        res = field->val_str(&tmp, key + offset);
      } else
        res = (*arg)->val_str(&tmp);
    }
    if (res) result->append(*res);
  }

  item->row_count++;

  /*
     Stop if the size of group_concat value, in bytes, is longer than
     the maximum size.
  */
  if (result->length() > item->group_concat_max_len) {
    int well_formed_error;
    const CHARSET_INFO *cs = item->collation.collation;
    const char *ptr = result->ptr();
    size_t add_length;
    /*
      It's ok to use item->result.length() as the fourth argument
      as this is never used to limit the length of the data.
      Cut is done with the third argument.
    */
    add_length = cs->cset->well_formed_len(
        cs, ptr + old_length, ptr + item->group_concat_max_len,
        result->length(), &well_formed_error);
    result->length(old_length + add_length);
    item->warning_for_row = true;
    push_warning_printf(
        current_thd, Sql_condition::SL_WARNING, ER_CUT_VALUE_GROUP_CONCAT,
        ER_THD(current_thd, ER_CUT_VALUE_GROUP_CONCAT), item->row_count);

    /**
       To avoid duplicated warnings in Item_func_group_concat::val_str()
    */
    if (table && table->blob_storage)
      table->blob_storage->set_truncated_value(false);
    return 1;
  }
  return 0;
}

/**
  Constructor of Item_func_group_concat.

  @param pos The token's position.
  @param distinct_arg   distinct
  @param select_list    list of expression for show values
  @param opt_order_list list of sort columns
  @param separator_arg  string value of separator.
  @param w              window, iff we have a windowing use of GROUP_CONCAT
*/

Item_func_group_concat::Item_func_group_concat(
    const POS &pos, bool distinct_arg, PT_item_list *select_list,
    PT_order_list *opt_order_list, String *separator_arg, PT_window *w)
    : super(pos, w),
      distinct(distinct_arg),
      m_order_arg_count(opt_order_list ? opt_order_list->value.elements : 0),
      m_field_arg_count(select_list->elements()),
      separator(separator_arg),
      order_array(*THR_MALLOC) {
  Item **arg_ptr;

  allow_group_via_temp_table = false;
  arg_count = m_field_arg_count + m_order_arg_count;

  if (!(args = (Item **)(*THR_MALLOC)->Alloc(sizeof(Item *) * arg_count)))
    return;

  if (order_array.reserve(m_order_arg_count)) return;

  /* fill args items of show and sort */
  auto it = select_list->value.begin();

  for (arg_ptr = args; it != select_list->value.end(); ++arg_ptr, ++it) {
    *arg_ptr = *it;
  }

  if (m_order_arg_count > 0) {
    for (ORDER *order_item = opt_order_list->value.first; order_item != nullptr;
         order_item = order_item->next) {
      order_array.push_back(*order_item);
      *arg_ptr = *order_item->item;
      order_array.back().item = arg_ptr++;
    }
    for (ORDER *ord = order_array.begin(); ord < order_array.end(); ++ord)
      ord->next = ord != &order_array.back() ? ord + 1 : nullptr;
  }
}

bool Item_func_group_concat::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
  context = pc->thd->lex->current_context();
  return false;
}

Item_func_group_concat::Item_func_group_concat(THD *thd,
                                               Item_func_group_concat *item)
    : Item_sum(thd, item),
      distinct(item->distinct),
      always_null(item->always_null),
      m_order_arg_count(item->m_order_arg_count),
      m_field_arg_count(item->m_field_arg_count),
      context(item->context),
      separator(item->separator),
      tmp_table_param(item->tmp_table_param),
      tree(item->tree),
      unique_filter(item->unique_filter),
      table(item->table),
      order_array(thd->mem_root),
      row_count(item->row_count),
      group_concat_max_len(item->group_concat_max_len),
      warning_for_row(item->warning_for_row),
      force_copy_fields(item->force_copy_fields),
      original(item) {
  allow_group_via_temp_table = item->allow_group_via_temp_table;
  result.set_charset(collation.collation);

  /*
    Since the ORDER structures pointed to by the elements of the 'order' array
    may be modified in find_order_in_list() called from
    Item_func_group_concat::setup(), create a copy of those structures so that
    such modifications done in this object would not have any effect on the
    object being copied.
  */
  if (order_array.reserve(m_order_arg_count)) return;

  for (uint i = 0; i < m_order_arg_count; i++) {
    /*
      Compiler generated copy constructor is used to
      to copy all the members of ORDER struct.
      It's also necessary to update ORDER::next pointer
      so that it points to new ORDER element.
    */
    order_array.push_back(item->order_array[i]);
  }
  if (m_order_arg_count > 0) {
    for (ORDER *ord = order_array.begin(); ord < order_array.end(); ++ord)
      ord->next = ord != &order_array.back() ? ord + 1 : nullptr;
  }
}

void Item_func_group_concat::cleanup() {
  DBUG_TRACE;
  Item_sum::cleanup();

  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (original == nullptr) {
    destroy(tmp_table_param);
    tmp_table_param = nullptr;
    if (table != nullptr) {
      if (table->blob_storage) destroy(table->blob_storage);
      close_tmp_table(table);
      free_tmp_table(table);
      table = nullptr;
      if (tree != nullptr) {
        delete_tree(tree);
        tree = nullptr;
      }
      if (unique_filter) {
        destroy(unique_filter);
        unique_filter = nullptr;
      }
    }
    assert(tree == nullptr);
  }
  row_count = 0;
}

Field *Item_func_group_concat::make_string_field(TABLE *table_arg) const {
  Field *field;
  assert(collation.collation);
  /*
    Use mbminlen to determine maximum number of characters.
    Compared to using mbmaxlen, this provides ability to
    accommodate more characters in case of charsets that
    support variable length characters.
    If the actual data has characters with length less than
    mbmaxlen, with this approach more characters can be stored.
  */

  const uint32 max_characters =
      group_concat_max_len / collation.collation->mbminlen;

  // Avoid arithmetic overflow
  const uint32 field_length = min<uint64>(
      static_cast<uint64>(max_characters) * collation.collation->mbmaxlen,
      UINT_MAX32);

  if (max_characters > CONVERT_IF_BIGGER_TO_BLOB)
    field = new (*THR_MALLOC)
        Field_blob(field_length, is_nullable(), item_name.ptr(),
                   collation.collation, true);
  else
    field = new (*THR_MALLOC)
        Field_varstring(field_length, is_nullable(), item_name.ptr(),
                        table_arg->s, collation.collation);

  if (field) field->init(table_arg);
  return field;
}

Item *Item_func_group_concat::copy_or_same(THD *thd) {
  DBUG_TRACE;
  Item *result = m_is_window_function ? this
                                      : new (thd->mem_root)
                                            Item_func_group_concat(thd, this);
  return result;
}

void Item_func_group_concat::no_rows_in_result() { clear(); }

void Item_func_group_concat::clear() {
  result.length(0);
  result.copy();
  null_value = true;
  warning_for_row = false;
  m_result_finalized = false;
  if (tree) reset_tree(tree);
  if (unique_filter) unique_filter->reset();
  if (table && table->blob_storage) table->blob_storage->reset();
  /* No need to reset the table as we never call write_row */
}

bool Item_func_group_concat::add() {
  if (always_null) return false;
  THD *thd = current_thd;
  if (copy_funcs(tmp_table_param, thd)) return true;

  for (uint i = 0; i < m_field_arg_count; i++) {
    Item *show_item = args[i];
    if (show_item->const_item()) continue;

    Field *field = show_item->get_tmp_table_field();
    if (field && field->is_null_in_record((const uchar *)table->record[0]))
      return false;  // Skip row if it contains null
  }

  null_value = false;
  bool row_eligible = true;

  if (distinct) {
    /* Filter out duplicate rows. */
    uint count = unique_filter->elements_in_tree();
    unique_filter->unique_add(table->record[0] + table->s->null_bytes);
    if (count == unique_filter->elements_in_tree()) row_eligible = false;
  }

  TREE_ELEMENT *el = nullptr;  // Only for safety
  if (row_eligible && tree) {
    DBUG_EXECUTE_IF("trigger_OOM_in_gconcat_add",
                    DBUG_SET("+d,simulate_persistent_out_of_memory"););
    el = tree_insert(tree, table->record[0] + table->s->null_bytes, 0,
                     tree->custom_arg);
    DBUG_EXECUTE_IF("trigger_OOM_in_gconcat_add",
                    DBUG_SET("-d,simulate_persistent_out_of_memory"););
    /* check if there was enough memory to insert the row */
    if (!el) return true;
  }
  /*
    In case of GROUP_CONCAT with DISTINCT or ORDER BY (or both) don't dump the
    row to the output buffer here. That will be done in val_str.
  */
  if (row_eligible && !warning_for_row && tree == nullptr && !distinct) {
    dump_leaf_key(table->record[0] + table->s->null_bytes, 1, this);
    if (current_thd->is_error()) {
      return true;
    }
  }

  return false;
}

bool Item_func_group_concat::fix_fields(THD *thd, Item **ref) {
  if (super::fix_fields(thd, ref)) return true;

  if (init_sum_func_check(thd)) return true;

  set_nullable(true);

  Condition_context CCT(thd->lex->current_query_block());

  // Fix fields for select list and ORDER clause

  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (param_type_is_default(thd, 0, -1)) return true;

  // Aggregate character set for expression columns (not order columns)
  if (agg_item_charsets_for_string_result(collation, func_name(), args,
                                          m_field_arg_count))
    return true;

  result.set_charset(collation.collation);
  group_concat_max_len = thd->variables.group_concat_max_len;
  if (thd->variables.group_concat_max_len > UINT_MAX32)
    group_concat_max_len = UINT_MAX32;
  else
    group_concat_max_len =
        static_cast<uint>(thd->variables.group_concat_max_len);
  uint32 max_chars = group_concat_max_len / collation.collation->mbminlen;
  // Avoid arithmetic overflow
  uint32 max_byte_length = min<uint64>(
      static_cast<uint64>(max_chars) * collation.collation->mbmaxlen,
      UINT_MAX32);
  max_chars > CONVERT_IF_BIGGER_TO_BLOB ? set_data_type_blob(max_byte_length)
                                        : set_data_type_string(max_chars);

  size_t offset;
  if (separator->needs_conversion(separator->length(), separator->charset(),
                                  collation.collation, &offset)) {
    size_t buflen = collation.collation->mbmaxlen * separator->length();

    char *buf = pointer_cast<char *>(thd->alloc(buflen));
    if (buf == nullptr) return true;

    String *new_separator =
        new (thd->mem_root) String(buf, buflen, collation.collation);
    if (new_separator == nullptr) return true;

    uint errors;
    size_t conv_length =
        copy_and_convert(buf, buflen, collation.collation, separator->ptr(),
                         separator->length(), separator->charset(), &errors);
    new_separator->length(conv_length);
    separator = new_separator;
  }

  if (check_sum_func(thd, ref)) return true;

  // Create a list with all the non-NULL fields:
  mem_root_deque<Item *> fields(thd->mem_root);
  for (uint i = 0; i < m_field_arg_count; i++) {
    Item *item = args[i];
    fields.push_back(item);
    if (item->const_item() && !thd->lex->is_view_context_analysis() &&
        item->is_null()) {
      // "is_null()" may cause error:
      if (thd->is_error()) return true;
      always_null = true;
    }
  }

  /*
    Find and resolve every ORDER BY expression in the list of GROUP_CONCAT
    arguments.
    The "fields" list is not used after the call to setup_order(), however it
    must be recreated during optimization to create tmp table columns.
  */
  if (m_order_arg_count > 0 && !always_null &&
      setup_order(thd, Ref_item_array(args, arg_count), context->table_list,
                  &fields, order_array.begin()))
    return true;

  null_value = true;

  fixed = true;

  return false;
}

bool Item_func_group_concat::setup(THD *thd) {
  DBUG_TRACE;
  /*
    Currently setup() can be called twice. Please add
    assertion here when this is fixed.
  */
  if (table != nullptr || tree != nullptr) return false;

  // Nothing to set up if always NULL:
  if (always_null) return false;

  assert(thd->lex->current_query_block() == aggr_query_block);

  uint new_max_len;
  if (thd->variables.group_concat_max_len > UINT_MAX32)
    new_max_len = UINT_MAX32;
  else
    new_max_len = static_cast<uint>(thd->variables.group_concat_max_len);
  if (group_concat_max_len < new_max_len) {
    /*
      Probably the user increased @@group_concat_max_len between preparation
      and execution. The Field we have set up may be too short for the
      new requested length.
    */
    if (ask_to_reprepare(thd)) return true;
    assert(false);
    // Continue; we'll truncate more than wanted. Should not happen.
  }

  const bool order_or_distinct = m_order_arg_count > 0 || distinct;

  assert(tmp_table_param == nullptr);
  tmp_table_param = new (thd->mem_root) Temp_table_param;
  if (tmp_table_param == nullptr) return true;

  // Create a temporary list with the required fields
  mem_root_deque<Item *> fields(thd->mem_root);

  // First add the fields from the concat field list
  for (uint i = 0; i < m_field_arg_count; i++) {
    fields.push_back(args[i]);
  }
  // Then prepend the ordered fields not already in the "fields" list
  for (uint i = 0; i < m_order_arg_count; i++) {
    bool skip = false;
    for (Item *item : fields) {
      if (item == order_array[i].item[0]) skip = true;
    }
    if (skip) continue;
    fields.push_front(order_array[i].item[0]);
  }

  count_field_types(aggr_query_block, tmp_table_param, fields, false, true);
  tmp_table_param->force_copy_fields = force_copy_fields;
  if (order_or_distinct) {
    /*
      Force the create_tmp_table() to convert BIT columns to INT
      as we cannot compare two table records containing BIT fields
      stored in the the tree used for distinct/order by.
      Moreover we don't even save in the tree record null bits
      where BIT fields store parts of their data.
    */
    for (Item *item : fields) {
      if (item->type() == Item::FIELD_ITEM &&
          down_cast<Item_field *>(item)->field->type() == FIELD_TYPE_BIT)
        item->marker = Item::MARKER_BIT;
    }
  }

  /*
    Create a temporary table to get descriptions of fields (types, sizes, etc).
    The table contains the ORDER BY fields followed by the field list.
  */
  assert(table == nullptr);
  table =
      create_tmp_table(thd, tmp_table_param, fields, nullptr, false, true,
                       aggr_query_block->active_options(), HA_POS_ERROR, "");
  if (table == nullptr) return true;

  table->file->ha_extra(HA_EXTRA_NO_ROWS);
  table->no_rows = true;

  /*
    Initialize blob_storage if GROUP_CONCAT is used
    with ORDER BY | DISTINCT and BLOB field count > 0.
  */
  if (order_or_distinct && table->s->blob_fields) {
    table->blob_storage = new (thd->mem_root) Blob_mem_storage();
    if (table->blob_storage == nullptr) return true;
  }
  /*
     Need sorting or uniqueness: init tree and choose a function to sort.
     Don't reserve space for NULLs: if any of gconcat arguments is NULL,
     the row is not added to the result.
  */
  uint tree_key_length = table->s->reclength - table->s->null_bytes;

  if (m_order_arg_count > 0) {
    tree = &tree_base;
    /*
      Create a tree for sorting. The tree is used to sort (according to the
      syntax of this function). If there is no ORDER BY clause, we don't
      create this tree.
    */
    init_tree(tree, 0, tree_key_length, group_concat_key_cmp_with_order, false,
              nullptr, this);
  }

  if (distinct) {
    unique_filter = new (thd->mem_root)
        Unique(group_concat_key_cmp_with_distinct, (void *)this,
               tree_key_length, ram_limitation(thd));
    if (unique_filter == nullptr) return true;
  }

  null_value = true;

  return false;
}

/* This is used by rollup to create a separate usable copy of the function */

void Item_func_group_concat::make_unique() {
  tmp_table_param = nullptr;
  table = nullptr;
  original = nullptr;
  force_copy_fields = true;
  tree = nullptr;
}

double Item_func_group_concat::val_real() {
  String *res = val_str(&str_value);
  if (res == nullptr) return 0.0;
  return double_from_string_with_check(collation.collation, res->ptr(),
                                       res->ptr() + res->length());
}

String *Item_func_group_concat::val_str(String *) {
  assert(fixed == 1);
  if (null_value) return nullptr;

  if (!m_result_finalized)  // Result yet to be written.
  {
    if (tree != nullptr)  // order by
      tree_walk(tree, &dump_leaf_key, this, left_root_right);
    else if (distinct)  // distinct (and no order by).
      unique_filter->walk(&dump_leaf_key, this);
    else
      assert(false);  // Can't happen
  }

  if (table && table->blob_storage &&
      table->blob_storage->is_truncated_value()) {
    warning_for_row = true;
    push_warning_printf(
        current_thd, Sql_condition::SL_WARNING, ER_CUT_VALUE_GROUP_CONCAT,
        ER_THD(current_thd, ER_CUT_VALUE_GROUP_CONCAT), row_count);
  }

  return &result;
}

void Item_func_group_concat::print(const THD *thd, String *str,
                                   enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("group_concat("));
  if (distinct) str->append(STRING_WITH_LEN("distinct "));
  for (uint i = 0; i < m_field_arg_count; i++) {
    if (i) str->append(',');
    args[i]->print(thd, str, query_type);
  }
  if (m_order_arg_count > 0) {
    str->append(STRING_WITH_LEN(" order by "));
    for (uint i = 0; i < m_order_arg_count; i++) {
      if (i) str->append(',');
      args[i + m_field_arg_count]->print(thd, str, query_type);
      if (order_array[i].direction == ORDER_ASC)
        str->append(STRING_WITH_LEN(" ASC"));
      else
        str->append(STRING_WITH_LEN(" DESC"));
    }
  }
  str->append(STRING_WITH_LEN(" separator \'"));

  if (query_type & QT_TO_SYSTEM_CHARSET) {
    // Convert to system charset.
    convert_and_print(separator, str, system_charset_info);
  } else if (query_type & QT_TO_ARGUMENT_CHARSET) {
    /*
      Convert the string literals to str->charset(),
      which is typically equal to charset_set_client.
    */
    convert_and_print(separator, str, str->charset());
  } else {
    separator->print(str);
  }
  str->append(STRING_WITH_LEN("\')"));
}

bool Item_non_framing_wf::fix_fields(THD *thd, Item **items) {
  if (super::fix_fields(thd, items)) return true;

  if (init_sum_func_check(thd)) return true;

  /*
    Although group aggregate functions must use Disable_semijoin_flattening
    here, WFs need not. Indeed, WFs can never be used in a WHERE or JOIN ON
    condition, so semijoin is never attempted on any subquery argument of
    theirs.
  */
  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (resolve_type(thd)) return true;

  if (check_sum_func(thd, items)) return true;

  fixed = true;
  return false;
}

longlong Item_row_number::val_int() {
  DBUG_TRACE;

  if (m_window->at_partition_border() && !m_window->needs_buffering()) {
    clear();
  }

  m_ctr++;

  DBUG_PRINT("enter", ("Item_row_number::val_int  at border: %d ctr: %llu",
                       m_window->at_partition_border(), m_ctr));
  return m_ctr;
}

double Item_row_number::val_real() {
  assert(unsigned_flag);
  return (ulonglong)val_int();
}

String *Item_row_number::val_str(String *buff) {
  return val_string_from_int(buff);
}

my_decimal *Item_row_number::val_decimal(my_decimal *buffer) {
  (void)int2my_decimal(E_DEC_FATAL_ERROR, val_int(), false, buffer);
  return buffer;
}

void Item_row_number::clear() { m_ctr = 0; }

void Item_rank::update_after_wf_arguments_changed(THD *thd) {
  const PT_order_list *order = m_window->effective_order_by();
  if (!order) return;
  ORDER *o = order->value.first;
  for (unsigned i = 0; i < m_previous.size(); ++i, o = o->next) {
    // If using the old optimizer, the references created for ORDER BY
    // expressions should not be disturbed. The ref array slices depend
    // on them. This is called only during resolving with ROLLUP in case
    // of old optimizer.
    Item **item_to_be_changed;
    if (!thd->lex->using_hypergraph_optimizer) {
      Item_ref *item_ref = down_cast<Item_ref *>(m_previous[i]->get_item());
      item_to_be_changed = item_ref->ref_pointer();
    } else {
      item_to_be_changed = m_previous[i]->get_item_ptr();
    }
    if (thd->lex->is_exec_started()) {
      thd->change_item_tree(item_to_be_changed, (*o->item)->real_item());
    } else {
      *item_to_be_changed = (*o->item)->real_item();
    }
  }
}

bool Item_rank::check_wf_semantics1(THD *thd, Query_block *select,
                                    Window_evaluation_requirements *) {
  const PT_order_list *order = m_window->effective_order_by();
  // SQL2015 6.10 <window function> SR 6.a: require ORDER BY; we don't.
  if (!order) return false;  // all rows in partition are peers
  for (ORDER *o = order->value.first; o != nullptr; o = o->next) {
    /*
      We need to access the value of the ORDER expression when evaluating
      RANK to determine equality or not, so we need a handle.
    */
    Item_ref *ir = new Item_ref(&select->context, o->item, "<partition order>");
    if (ir == nullptr) return true;

    m_previous.push_back(new_Cached_item(thd, ir));
  }
  return false;
}

longlong Item_rank::val_int() {
  DBUG_TRACE;
  if (m_window->at_partition_border() && !m_window->needs_buffering()) {
    clear();
  }

  bool change = false;
  if (m_window->has_windowing_steps()) {
    /*
      Check if any of the ORDER BY expressions have changed. If so, we
      need to update the rank, considering any duplicates.
    */
    for (Cached_item *item : m_previous) {
      change |= item->cmp();
    }
  }
  // if no windowing steps, no comparison needed.

  if (change) {
    m_rank_ctr += 1 + (m_dense ? 0 : m_duplicates);
    m_duplicates = 0;
  } else {
    m_duplicates++;
  }

  return m_rank_ctr;
}

double Item_rank::val_real() {
  assert(unsigned_flag);
  return (ulonglong)val_int();
}

String *Item_rank::val_str(String *buff) { return val_string_from_int(buff); }

my_decimal *Item_rank::val_decimal(my_decimal *buffer) {
  (void)int2my_decimal(E_DEC_FATAL_ERROR, val_int(), false, buffer);
  return buffer;
}

void Item_rank::clear() {
  /*
    Cf. also ::reset_cmp which can't be called until we have the partition's
    first row ready (after copy_fields).
  */
  m_rank_ctr = 1;
  m_duplicates = -1;

  // Reset comparator
  if (m_window->has_windowing_steps()) {
    for (Cached_item *item : m_previous) {
      item->cmp();  // set baseline
    }
  }  // if no windowing steps, no comparison needed.
}

Item_rank::~Item_rank() {
  for (Cached_item *ci : m_previous) {
    destroy(ci);
  }
  m_previous.clear();
}

bool Item_cume_dist::check_wf_semantics1(THD *, Query_block *,
                                         Window_evaluation_requirements *r) {
  // we need to know partition cardinality, so two passes
  r->needs_buffer = true;
  // Before we can compute for the current row we need the count of its peers
  r->needs_peerset = true;
  // SQL2015 6.10 <window function> SR 6.h: don't require ORDER BY.
  return false;
}

double Item_cume_dist::val_real() {
  DBUG_TRACE;

  if (!m_window->has_windowing_steps())
    return 1.0;  // degenerate case, no real windowing

  double cume_dist = (double)m_window->last_rowno_in_peerset() /
                     m_window->last_rowno_in_cache();

  return cume_dist;
}

longlong Item_cume_dist::val_int() {
  DBUG_TRACE;

  longlong result = (longlong)rint(val_real());

  return result;
}

String *Item_cume_dist::val_str(String *buff) {
  return val_string_from_real(buff);
}

my_decimal *Item_cume_dist::val_decimal(my_decimal *buffer) {
  (void)double2my_decimal(E_DEC_FATAL_ERROR, val_real(), buffer);
  return buffer;
}

bool Item_percent_rank::check_wf_semantics1(THD *, Query_block *,
                                            Window_evaluation_requirements *r) {
  // we need to know partition cardinality, so two passes
  r->needs_buffer = true;
  /*
    The family of RANK functions doesn't need the peer set: even though they
    give the same value to peers, that value can be computed for the first row
    of the peer set without knowing how many peers it has. However, this family
    needs detection of when the current row leaves the current peer set (to
    increase the rank counter):
    - RANK and DENSE_RANK do so internally with row comparison;
    - but PERCENT_RANK, as it needs partition cardinality, requires buffering,
    so it can simply pretend it needs_peerset() and then the buffering code will
    detect the peer set's end and provide it in last_rowno_in_peerset().
  */
  r->needs_peerset = true;

  const PT_order_list *order = m_window->effective_order_by();
  // SQL2015 6.10 <window function> SR 6.g+6.a: require ORDER BY; we don't.
  if (!order) return false;  // all rows in partition are peers

  return false;
}

double Item_percent_rank::val_real() {
  DBUG_TRACE;

  if (!m_window->has_windowing_steps())
    return 0.0;  // degenerate case, no real windowing

  if (m_window->rowno_being_visited() == m_window->rowno_in_partition()) {
    if (m_last_peer_visited) {
      m_rank_ctr += m_peers;
      m_peers = 0;
      m_last_peer_visited = false;
    }

    m_peers++;

    if (m_window->rowno_being_visited() == m_window->last_rowno_in_peerset())
      m_last_peer_visited = true;

    if (m_rank_ctr == 1) return 0;
  }

  double percent_rank =
      (double)(m_rank_ctr - 1) / (m_window->last_rowno_in_cache() - 1);
  return percent_rank;
}

longlong Item_percent_rank::val_int() {
  DBUG_TRACE;

  longlong result = (longlong)rint(val_real());

  return result;
}

String *Item_percent_rank::val_str(String *buff) {
  return val_string_from_real(buff);
}

my_decimal *Item_percent_rank::val_decimal(my_decimal *buffer) {
  (void)double2my_decimal(E_DEC_FATAL_ERROR, val_real(), buffer);
  return buffer;
}

void Item_percent_rank::clear() {
  m_rank_ctr = 1;
  m_peers = 0;
  m_last_peer_visited = false;
}

Item_percent_rank::~Item_percent_rank() = default;

bool Item_nth_value::check_wf_semantics2(Window_evaluation_requirements *r) {
  /*
    Semantic check of the row argument. Should be a positive constant
    integer larger than zero, cf. SQL 2011 section 6.10 GR 1,d,ii,1-2)
    NULL literal is not allowed. Dynamic parameter is allowed and may be
    NULL.
  */
  Item *arg = args[1];
  if (!arg->const_for_execution() || arg->result_type() != INT_RESULT ||
      ((m_n = arg->val_int()) <= 0 && !arg->is_null())) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return true;
  }
  r->opt_nth_row.m_rowno = m_n;
  r->opt_nth_row.m_from_last = m_from_last;
  return false;
}

bool Item_ntile::fix_fields(THD *thd, Item **items) {
  if (super::fix_fields(thd, items)) return true;

  return false;
}

longlong Item_ntile::val_int() {
  if (m_window->rowno_being_visited() == m_window->rowno_in_partition()) {
    if (args[0]->is_null()) {
      null_value = true;
      return 0;
    }

    longlong buckets = args[0]->val_int();
    if (buckets == 0) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return error_int();
    }

    /*
      Should not be evaluated until we have read all rows in partition
      notwithstanding any frames, so last_rowno_in_cache should be cardinality
      of partition.
    */

    int64 full_rounds = m_window->last_rowno_in_cache() / buckets;
    int64 modulus = m_window->last_rowno_in_cache() % buckets;
    int64 r;

    /*
      Rows might not distribute evenly, if modulus!=0. In that case, add
      extras at the beginning as per SQL 2011 section 6.10 <window function>
      GR 1a, ii, 3): the first 'modulus' buckets contain 'full_rounds + 1'
      rows, the other buckets contain 'full_rounds' rows.
     */
    if (modulus == 0 && full_rounds == 0) {
      r = 1;  // degenerate case; no real windowing
    } else {
      // Using convention "row 0 is first row" for those two variables:
      int64 rowno = m_window->rowno_in_partition() - 1,
            // the first rowno of smaller buckets
          first_of_small = modulus * (full_rounds + 1);
      if (rowno >= first_of_small)  // row goes into small buckets
      {
        r = (rowno - first_of_small) / full_rounds + 1 + modulus;
      } else  // row goes into big buckets
      {
        r = rowno / (full_rounds + 1) + 1;
      }
    }
    m_value = r;
  }

  return m_value;
}

double Item_ntile::val_real() {
  assert(unsigned_flag);
  return (ulonglong)val_int();
}

String *Item_ntile::val_str(String *buff) { return val_string_from_int(buff); }

my_decimal *Item_ntile::val_decimal(my_decimal *buffer) {
  (void)int2my_decimal(E_DEC_FATAL_ERROR, val_int(), false, buffer);
  return buffer;
}

bool Item_ntile::check_wf_semantics1(THD *, Query_block *,
                                     Window_evaluation_requirements *r) {
  r->needs_buffer =
      true;  // we need to know partition cardinality, so two passes
  // SQL2015 6.10 <window function> SR 6.a: require ORDER BY; we don't.
  return false;
}

bool Item_ntile::check_wf_semantics2(Window_evaluation_requirements *) {
  Item *arg = args[0];
  /*
    Semantic check of the argument. Should be a positive constant
    integer larger than zero, cf. SQL 2011 section 6.10 GR 1,a,ii,1-2)
    NULL literal is not allowed. Dynamic parameter is allowed, and may not be
    NULL.
  */
  if (!arg->const_for_execution() || arg->result_type() != INT_RESULT ||
      arg->val_int() <= 0 || arg->is_null()) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return true;
  }
  return false;
}

bool Item_first_last_value::check_wf_semantics1(
    THD *thd, Query_block *select, Window_evaluation_requirements *r) {
  if (super::check_wf_semantics1(thd, select, r)) return true;

  r->opt_first_row = m_is_first;
  r->opt_last_row = !m_is_first;

  if (m_null_treatment == NT_IGNORE_NULLS) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "IGNORE NULLS");
    return true;
  }
  return false;
}

bool Item_first_last_value::resolve_type(THD *thd) {
  set_nullable(true);  // if empty frame, notwithstanding nullability of arg
  null_value = true;
  if (param_type_is_default(thd, 0, 1)) return true;
  set_data_type_from_item(args[0]);
  m_hybrid_type = args[0]->result_type();

  return false;
}

bool Item_first_last_value::fix_fields(THD *thd, Item **items) {
  if (super::fix_fields(thd, items)) return true;

  if (init_sum_func_check(thd)) return true;

  if ((!args[0]->fixed && args[0]->fix_fields(thd, args)) ||
      args[0]->check_cols(1))
    return true;

  if (resolve_type(thd)) return true;

  if (setup_first_last()) return true;

  if (check_sum_func(thd, items)) return true;

  fixed = true;
  return false;
}

void Item_first_last_value::split_sum_func(THD *thd,
                                           Ref_item_array ref_item_array,
                                           mem_root_deque<Item *> *fields) {
  super::split_sum_func(thd, ref_item_array, fields);
  // Need to redo this now:
  update_after_wf_arguments_changed(thd);
}

bool Item_first_last_value::setup_first_last() {
  m_value = Item_cache::get_cache(args[0]);
  if (m_value == nullptr) return true;
  /*
    After any split_sum_func, we will need to update the m_value::example,
    cf. Item_first_last_value::split_sum_func
  */
  m_value->setup(args[0]);
  return false;
}

void Item_first_last_value::clear() {
  m_value->clear();
  null_value = true;
  cnt = 0;
}

void Item_first_last_value::update_after_wf_arguments_changed(THD *) {
  m_value->setup(args[0]);
}

bool Item_first_last_value::compute() {
  cnt++;

  if (m_window->do_inverse()) {
    null_value = true;
  } else if ((m_window->needs_buffering() &&
              (((m_window->rowno_in_frame() == 1 && m_is_first) ||
                (m_window->is_last_row_in_frame() && !m_is_first)) ||
               m_window->rowno_being_visited() ==
                   0 /* No FROM; one const row */)) ||
             (!m_window->needs_buffering() &&
              ((m_is_first && cnt == 1) || !m_is_first))) {
    // if() above says we are positioned at the proper first/last row of frame
    m_value->cache_value();
    null_value = m_value->null_value;
  }
  return null_value || current_thd->is_error();
}
longlong Item_first_last_value::val_int() {
  if (wf_common_init()) return 0;

  if (compute()) return error_int();

  longlong retval = m_value->val_int();
  null_value = m_value->null_value;
  return retval;
}

double Item_first_last_value::val_real() {
  if (wf_common_init()) return 0.0;

  if (compute()) return error_real();

  double retval = m_value->val_real();
  null_value = m_value->null_value;
  return retval;
}

bool Item_first_last_value::get_date(MYSQL_TIME *ltime,
                                     my_time_flags_t fuzzydate) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  bool retval = m_value->get_date(ltime, fuzzydate);
  null_value = m_value->null_value;
  return retval;
}

bool Item_first_last_value::get_time(MYSQL_TIME *ltime) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  bool retval = m_value->get_time(ltime);
  null_value = m_value->null_value;
  return retval;
}

bool Item_first_last_value::val_json(Json_wrapper *jw) {
  if (wf_common_init()) return false;

  if (compute()) return current_thd->is_error();

  bool retval = m_value->val_json(jw);
  null_value = m_value->null_value;
  return retval;
}

my_decimal *Item_first_last_value::val_decimal(my_decimal *decimal_buffer) {
  if (wf_common_init()) {
    return error_decimal(decimal_buffer);
  }

  if (compute()) {
    return error_decimal(decimal_buffer);
  }

  my_decimal *retval = m_value->val_decimal(decimal_buffer);
  null_value = m_value->null_value;
  return retval;
}

String *Item_first_last_value::val_str(String *str) {
  if (wf_common_init()) return str;

  if (compute()) return error_str();

  String *retval = m_value->val_str(str);
  null_value = m_value->null_value;
  return retval;
}

bool Item_nth_value::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1)) return true;
  if (args[1]->propagate_type(thd, MYSQL_TYPE_LONGLONG, true)) return true;

  set_nullable(true);

  set_data_type_from_item(args[0]);

  m_hybrid_type = args[0]->result_type();

  return false;
}

bool Item_nth_value::fix_fields(THD *thd, Item **items) {
  if (super::fix_fields(thd, items)) return true;

  if (init_sum_func_check(thd)) return true;

  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  /*
    Semantic check of the row argument. Should be a positive constant
    integer larger than zero, cf. SQL 2011 section 6.10 GR 1,d,ii,1-2)
    NULL is allowed. Dynamic parameter is allowed.
  */
  if (args[1]->const_for_execution()) {
    // we are in a PREPARE phase, so can't check yet
  } else {
    if (!args[1]->const_item() ||
        (!args[1]->is_null() &&
         (args[1]->result_type() != INT_RESULT || args[1]->val_int() <= 0))) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
    m_n = args[1]->val_int();
  }

  result_field = nullptr;

  if (resolve_type(thd)) return true;

  if (setup_nth()) return true;

  if (check_sum_func(thd, items)) return true;

  fixed = true;
  return false;
}

void Item_nth_value::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                                    mem_root_deque<Item *> *fields) {
  super::split_sum_func(thd, ref_item_array, fields);
  // If function was set up, need to redo this now:
  update_after_wf_arguments_changed(thd);
}

bool Item_nth_value::setup_nth() {
  /*
    After any split_sum_func, we will need to update the m_value::example,
    cf. Item_nth_value::split_sum_func
  */
  m_value = Item_cache::get_cache(args[0]);
  if (m_value == nullptr) return true;
  m_value->setup(args[0]);
  return false;
}

void Item_nth_value::clear() {
  m_value->clear();
  null_value = true;
  m_cnt = 0;
}

void Item_nth_value::update_after_wf_arguments_changed(THD *) {
  m_value->setup(args[0]);
}

bool Item_nth_value::check_wf_semantics1(THD *thd, Query_block *select,
                                         Window_evaluation_requirements *r) {
  if (super::check_wf_semantics1(thd, select, r)) return true;

  r->opt_nth_row.m_rowno = m_n;
  r->opt_nth_row.m_from_last = m_from_last;

  if (m_null_treatment == NT_IGNORE_NULLS) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "IGNORE NULLS");
    return true;
  }

  if (m_from_last) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "FROM LAST");
    return true;
  }

  return false;
}

bool Item_nth_value::compute() {
  m_cnt++;

  if (m_window->do_inverse())
    null_value = true;
  else if (!m_window->needs_buffering()) {
    if (m_cnt == m_n) {
      m_value->cache_value();
      null_value = m_value->null_value;
    }
  } else if (m_window->rowno_being_visited() == 0) {
    // empty FROM, single constant row
    if (m_n == 1) {
      m_value->cache_value();
      null_value = m_value->null_value;
    }
  } else if (!m_from_last) {
    if (m_window->rowno_in_frame() == m_n) {
      m_value->cache_value();
      null_value = m_value->null_value;
    }
  } else if (m_from_last) {
    assert(false);  // Not yet supported
    //    if (m_window->frame_cardinality() - m_window->rowno_in_frame() + 1
    //        == m_n)
    //    {
    //      m_value->cache_value();
    //      null_value= m_value->null_value;
    //    }
  }
  return null_value || current_thd->is_error();
}

longlong Item_nth_value::val_int() {
  if (wf_common_init()) return 0;

  if (compute()) return error_int();

  longlong retval = m_value->val_int();
  null_value = m_value->null_value;
  return retval;
}

double Item_nth_value::val_real() {
  if (wf_common_init()) return 0;

  if (compute()) return error_real();

  double retval = m_value->val_real();
  null_value = m_value->null_value;
  return retval;
}

my_decimal *Item_nth_value::val_decimal(my_decimal *decimal_buffer) {
  if (wf_common_init()) {
    return error_decimal(decimal_buffer);
  }

  if (compute()) {
    return error_decimal(decimal_buffer);
  }

  my_decimal *retval = m_value->val_decimal(decimal_buffer);
  null_value = m_value->null_value;
  return retval;
}

String *Item_nth_value::val_str(String *str) {
  if (wf_common_init()) return str;

  if (compute()) return error_str();

  String *retval = m_value->val_str(str);
  null_value = m_value->null_value;
  return retval;
}

bool Item_nth_value::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  bool retval = m_value->get_date(ltime, fuzzydate);
  null_value = m_value->null_value;
  return retval;
}

bool Item_nth_value::get_time(MYSQL_TIME *ltime) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  bool retval = m_value->get_time(ltime);
  null_value = m_value->null_value;
  return retval;
}

bool Item_nth_value::val_json(Json_wrapper *jw) {
  if (wf_common_init()) return false;

  if (compute()) return current_thd->is_error();

  bool retval = m_value->val_json(jw);
  null_value = m_value->null_value;
  return retval;
}

bool Item_lead_lag::resolve_type(THD *thd) {
  /*
    If we have default, check type compatibility of default_value to the main
    expression. Modeled on IFNULL, i.e. what's done for
    Item_func_ifnull::resolve_type.
  */

  /*
    LEAD(expr, offset [, default]).
    As we have to aggregate types of args[0] and args[2], and for that we use
    functions which take arrays, let's temporarily copy args[2] to args[1].
  */
  Item *save_arg1 = nullptr;
  uint orig_arg_count = arg_count;
  if (arg_count == 3) {
    save_arg1 = args[1];
    args[1] = args[2];
    arg_count--;
  } else if (arg_count == 2) {
    arg_count--;
  }

  if (param_type_uses_non_param(thd)) return true;
  aggregate_type(make_array(args, arg_count));
  m_hybrid_type = Field::result_merge_type(data_type());

  if (arg_count == 2)
    set_nullable(args[1]->is_nullable() || args[0]->is_nullable());
  else
    set_nullable(true);  // No default value provided, so we get NULLs

  if (m_hybrid_type == STRING_RESULT) {
    if (aggregate_string_properties(func_name(), args, arg_count)) return true;
  } else {
    aggregate_num_type(m_hybrid_type, args, arg_count);
  }

  if (orig_arg_count == 3)  // restore args array
  {
    // agg_item_charsets can have changed args[1]:
    args[2] = args[1];
    args[1] = save_arg1;
  }
  arg_count = orig_arg_count;
  /*
    In SQL2015, offset has to be a numeric literal.
    We allow a dynamic parameter too.
  */
  if (arg_count > 1 && args[1]->propagate_type(thd, MYSQL_TYPE_LONGLONG, true))
    return true;
  return false;
}

bool Item_lead_lag::fix_fields(THD *thd, Item **items) {
  if (super::fix_fields(thd, items)) return true;

  if (setup_lead_lag()) return true;

  fixed = true;
  return false;
}

bool Item_lead_lag::check_wf_semantics2(Window_evaluation_requirements *r) {
  /*
    Semantic check of the offset argument. Should be a integral constant,
    non-negative.
  */
  if (arg_count >= 2) {
    Item *arg = args[1];
    if (!arg->const_for_execution() || arg->result_type() != INT_RESULT ||
        ((m_n = arg->val_int()) < 0 || arg->is_null())) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
  } else {
    m_n = 1;
  }

  /*
    Canonicalize LEAD to negative LAG so we can order all sequentially around
    current row: positive value are LAG, i.e. addresses a row earlier than
    the current row in the result set.
  */
  if (m_is_lead) {
    m_n = -m_n;
  }
  r->opt_ll_row.m_rowno = m_n;
  return false;
}

void Item_lead_lag::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                                   mem_root_deque<Item *> *fields) {
  super::split_sum_func(thd, ref_item_array, fields);
  // If function was set up, need to redo these now:
  update_after_wf_arguments_changed(thd);
}

bool Item_lead_lag::setup_lead_lag() {
  /*
    After any split_sum_func, we will need to update the m_value::example
    and any m_default::example cf. Item_lead_lag_value::split_sum_func
  */
  m_value = Item_cache::get_cache(args[0]);
  if (m_value == nullptr) return true;
  m_value->setup(args[0]);
  if (arg_count == 3) {
    m_default = Item_cache::get_cache(args[2]);
    if (m_default == nullptr) return true;
    m_default->setup(args[2]);
  }
  return false;
}

bool Item_lead_lag::check_wf_semantics1(THD *thd [[maybe_unused]],
                                        Query_block *select [[maybe_unused]],
                                        Window_evaluation_requirements *r) {
  if (m_null_treatment == NT_IGNORE_NULLS) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "IGNORE NULLS");
    return true;
  }
  r->needs_buffer = true;
  // SQL2015 6.10 <window function> SR 6.a: require ORDER BY; we don't.
  return false;
}

void Item_lead_lag::clear() {
  m_value->clear();
  null_value = true;
  m_has_value = false;
  m_use_default = false;
}

void Item_lead_lag::update_after_wf_arguments_changed(THD *) {
  m_value->setup(args[0]);
  if (m_default != nullptr) m_default->setup(args[2]);
}

longlong Item_lead_lag::val_int() {
  if (wf_common_init()) return 0;

  if (compute()) return error_int();

  return m_use_default ? m_default->val_int() : m_value->val_int();
}

double Item_lead_lag::val_real() {
  if (wf_common_init()) return 0;

  if (compute()) return error_real();

  return m_use_default ? m_default->val_real() : m_value->val_real();
}

my_decimal *Item_lead_lag::val_decimal(my_decimal *decimal_buffer) {
  if (wf_common_init()) {
    return error_decimal(decimal_buffer);
  }

  if (compute()) {
    return error_decimal(decimal_buffer);
  }

  return m_use_default ? m_default->val_decimal(decimal_buffer)
                       : m_value->val_decimal(decimal_buffer);
}

String *Item_lead_lag::val_str(String *str) {
  if (wf_common_init()) return str;

  if (compute()) return error_str();

  return m_use_default ? m_default->val_str(str) : m_value->val_str(str);
}

bool Item_lead_lag::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  return m_use_default ? m_default->get_date(ltime, fuzzydate)
                       : m_value->get_date(ltime, fuzzydate);
}

bool Item_lead_lag::get_time(MYSQL_TIME *ltime) {
  if (wf_common_init()) return true;

  if (compute()) return true;

  return m_use_default ? m_default->get_time(ltime) : m_value->get_time(ltime);
}

bool Item_lead_lag::val_json(Json_wrapper *jw) {
  if (wf_common_init()) return false;

  if (compute()) return current_thd->is_error();

  return (m_has_value ? (m_use_default ? m_default->val_json(jw)
                                       : m_value->val_json(jw))
                      : false);
}

bool Item_lead_lag::compute() {
  if (m_window->do_inverse()) {
    // nothing, not relevant for LEAD/LAG
  } else {
    if (m_window->rowno_being_visited() == m_window->rowno_in_partition()) {
      /*
        Setup default value if present: it needs to be evaluated on the
        current row, not at the lead/lag row, cf. GR 1.b.i, SQL 2011
      */
      if (arg_count == 3) m_default->cache_value();
      null_value = true;  // a priori for current row
    }

    if (!m_window->has_windowing_steps()) {
      // empty FROM: we have exactly one constant row
      if (m_n == 0) {
        m_value->cache_value();
        null_value = m_value->null_value;
        m_has_value = true;
      } else if (arg_count == 3) {
        null_value = m_default->null_value;
        m_use_default = true;
        m_has_value = true;
      } else {
        null_value = true;
      }

      return null_value || current_thd->is_error();
    }

    bool our_offset = (m_window->rowno_being_visited() ==
                       m_window->rowno_in_partition() - m_n);

    if (our_offset) {
      if ((m_window->rowno_being_visited()) < 1 ||
          (m_window->rowno_being_visited() > m_window->last_rowno_in_cache())) {
        /*
          The row is outside the partition set; use default value if any
          provided else use NULL
        */
        if (arg_count == 3) {
          null_value = m_default->null_value;
          m_use_default = true;
        }
      } else {
        m_value->cache_value();
        null_value = m_value->null_value;
      }
      m_has_value = true;
    } else {
      // Visiting another function; return NULL or result we have.
      if (!m_has_value) null_value = true;
    }
  }
  return null_value || current_thd->is_error();
}

template <typename... Args>
Item_sum_json::Item_sum_json(unique_ptr_destroy_only<Json_wrapper> wrapper,
                             Args &&... parent_args)
    : Item_sum(std::forward<Args>(parent_args)...),
      m_wrapper(std::move(wrapper)) {
  set_data_type_json();
}

Item_sum_json::~Item_sum_json() = default;

bool Item_sum_json::check_wf_semantics1(THD *thd, Query_block *select,
                                        Window_evaluation_requirements *reqs) {
  return Item_sum::check_wf_semantics1(thd, select, reqs);
}

bool Item_sum_json::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);
  result_field = nullptr;

  if (super::fix_fields(thd, ref)) return true; /* purecov: inspected */

  if (init_sum_func_check(thd)) return true;

  Condition_context CCT(thd->lex->current_query_block());

  for (uint i = 0; i < arg_count; i++) {
    if ((!args[i]->fixed && args[i]->fix_fields(thd, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (resolve_type(thd)) return true;

  if (check_sum_func(thd, ref)) return true;

  set_nullable(true);
  null_value = true;
  fixed = true;
  return false;
}

String *Item_sum_json::val_str(String *str) {
  assert(fixed == 1);
  if (m_is_window_function) {
    if (wf_common_init()) return str;
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for window functions, which does not use Aggregator, it has to be called
      here.
    */
    if (add()) return error_str();
  }
  if (null_value || m_wrapper->empty()) return nullptr;
  str->length(0);
  if (m_wrapper->to_string(str, true, func_name(),
                           JsonDocumentDefaultDepthHandler))
    return error_str();

  return str;
}

bool Item_sum_json::val_json(Json_wrapper *wr) {
  if (m_is_window_function) {
    if (wf_common_init()) return false;
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for window functions, which does not use Aggregator, it has to be called
      here.
    */
    if (add()) return error_json();
  }

  assert(!m_wrapper->empty());

  if (null_value) return false;

  /*
    val_* functions are called more than once in aggregates and
    by passing the dom some function will destroy it so a clone is needed.
  */
  *wr = Json_wrapper(m_wrapper->clone_dom());
  return false;
}

double Item_sum_json::val_real() {
  if (m_is_window_function) {
    if (wf_common_init()) return 0.0;
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for window functions, which does not use Aggregator, it has to be called
      here.
    */
    if (add()) return error_real();
  }
  if (null_value || m_wrapper->empty()) return 0.0;

  return m_wrapper->coerce_real(func_name());
}

longlong Item_sum_json::val_int() {
  if (m_is_window_function) {
    if (wf_common_init()) return 0;
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for window functions, which does not use Aggregator, it has to be called
      here.
    */
    if (add()) return error_int();
  }
  if (null_value || m_wrapper->empty()) return 0;

  return m_wrapper->coerce_int(func_name());
}

my_decimal *Item_sum_json::val_decimal(my_decimal *decimal_value) {
  if (m_is_window_function) {
    if (wf_common_init()) return nullptr;
    /*
      For a group aggregate function, add() is called by Aggregator* classes;
      for window functions, which does not use Aggregator, it has to be called
      here.
    */
    if (add()) return error_decimal(decimal_value);
  }
  if (null_value || m_wrapper->empty()) {
    return error_decimal(decimal_value);
  }

  return m_wrapper->coerce_decimal(decimal_value, func_name());
}

bool Item_sum_json::get_date(MYSQL_TIME *ltime, my_time_flags_t) {
  if (null_value || m_wrapper->empty()) return true;

  return m_wrapper->coerce_date(ltime, func_name());
}

bool Item_sum_json::get_time(MYSQL_TIME *ltime) {
  if (null_value || m_wrapper->empty()) return true;

  return m_wrapper->coerce_time(ltime, func_name());
}

void Item_sum_json::reset_field() {
  /* purecov: begin inspected */
  assert(0);  // Check JOIN::with_json_agg for more details.
  // Create the container
  clear();
  // Append element to the container.
  add();

  /*
    field_type is MYSQL_TYPE_JSON so Item::make_string_field will always
    create a Field_json(in Item_sum::create_tmp_field).
    The cast is need since Field does not expose store_json function.
  */
  Field_json *json_result_field = down_cast<Field_json *>(result_field);
  json_result_field->set_notnull();
  // Store the container inside the field.
  json_result_field->store_json(m_wrapper.get());
  /* purecov: end */
}

void Item_sum_json::update_field() {
  /* purecov: begin inspected */
  assert(0);  // Check JOIN::with_json_agg for more details.
  /*
    field_type is MYSQL_TYPE_JSON so Item::make_string_field will always
    create a Field_json(in Item_sum::create_tmp_field).
    The cast is need since Field does not expose store_json function.
  */
  Field_json *json_result_field = down_cast<Field_json *>(result_field);
  // Restore the container(m_wrapper) from the field
  json_result_field->val_json(m_wrapper.get());

  // Append elements to the container.
  add();
  // Store the container inside the field.
  json_result_field->store_json(m_wrapper.get());
  json_result_field->set_notnull();
  /* purecov: end */
}

Item_sum_json_array::Item_sum_json_array(
    THD *thd, Item_sum *item, unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_array> array)
    : Item_sum_json(std::move(wrapper), thd, item),
      m_json_array(std::move(array)) {}

Item_sum_json_array::Item_sum_json_array(
    const POS &pos, Item *a, PT_window *w,
    unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_array> array)
    : Item_sum_json(std::move(wrapper), pos, a, w),
      m_json_array(std::move(array)) {}

Item_sum_json_array::~Item_sum_json_array() = default;

void Item_sum_json_array::clear() {
  null_value = true;
  m_json_array->clear();

  // Set the array to the m_wrapper, but let Item_sum_json_array keep the
  // ownership.
  *m_wrapper = Json_wrapper(m_json_array.get(), true);
}

Item_sum_json_object::Item_sum_json_object(
    THD *thd, Item_sum *item, unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_object> object)
    : Item_sum_json(std::move(wrapper), thd, item),
      m_json_object(std::move(object)) {}

Item_sum_json_object::Item_sum_json_object(
    const POS &pos, Item *a, Item *b, PT_window *w,
    unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_object> object)
    : Item_sum_json(std::move(wrapper), pos, a, b, w),
      m_json_object(std::move(object)) {}

Item_sum_json_object::~Item_sum_json_object() = default;

void Item_sum_json_object::clear() {
  null_value = true;
  m_json_object->clear();

  // Set the object to the m_wrapper, but let Item_sum_json_object keep the
  // ownership.
  *m_wrapper = Json_wrapper(m_json_object.get(), true);
  m_key_map.clear();
}

bool Item_sum_json_object::check_wf_semantics1(
    THD *thd, Query_block *select, Window_evaluation_requirements *r) {
  Item_sum_json::check_wf_semantics1(thd, select, r);
  /*
    As Json_object always stores only the last value for a key,
    optimization/inversion for windowing function is not possible
    unless row of the stored key/value pair is known. In case of
    an ordered result, if its known that a row is the last peer
    in a window frame for a key, then that key/value pair can be
    removed from the Json_object. So we let
    process_buffered_windowing_record() know by setting
    needs_last_peer_in_frame to true.
  */
  const PT_order_list *order = m_window->effective_order_by();
  if (order != nullptr) {
    ORDER *o = order->value.first;
    if (o->item[0]->real_item()->eq(args[0]->real_item(), false)) {
      r->needs_last_peer_in_frame = true;
      m_optimize = true;
    }
  }
  return false;
}

bool Item_sum_json_array::add() {
  assert(fixed == 1);
  assert(arg_count == 1);

  const THD *thd = base_query_block->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error()) return error_json();

  try {
    if (m_is_window_function) {
      if (m_window->do_inverse()) {
        auto arr = down_cast<Json_array *>(m_wrapper->to_dom());
        arr->remove(0);  // Remove the first element from the array
        arr->size() == 0 ? null_value = true : null_value = false;
        return false;
      }
    }
    Json_wrapper value_wrapper;
    // Get the value.
    if (get_atom_null_as_null(args, 0, func_name(), &m_value,
                              &m_conversion_buffer, &value_wrapper))
      return error_json();

    Json_dom_ptr value_dom(value_wrapper.to_dom());
    value_wrapper.set_alias();  // release the DOM

    /*
      The m_wrapper always points to m_json_array or the result of
      deserializing the result_field in reset/update_field.
    */
    const auto arr = down_cast<Json_array *>(m_wrapper->to_dom());
    if (arr->append_alias(std::move(value_dom)))
      return error_json(); /* purecov: inspected */

    null_value = false;
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  return false;
}

Item *Item_sum_json_array::copy_or_same(THD *thd) {
  if (m_is_window_function) return this;

  auto wrapper = make_unique_destroy_only<Json_wrapper>(thd->mem_root);
  if (wrapper == nullptr) return nullptr;

  unique_ptr_destroy_only<Json_array> array{::new (thd->mem_root) Json_array};
  if (array == nullptr) return nullptr;

  return new (thd->mem_root)
      Item_sum_json_array(thd, this, std::move(wrapper), std::move(array));
}

bool Item_sum_json_object::add() {
  assert(fixed == 1);
  assert(arg_count == 2);

  const THD *thd = base_query_block->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error()) return error_json();

  try {
    // key
    Item *key_item = args[0];
    const char *safep;   // contents of key_item, possibly converted
    size_t safe_length;  // length of safep

    if (get_json_object_member_name(thd, key_item, &m_tmp_key_value,
                                    &m_conversion_buffer, &safep, &safe_length))
      return error_json();

    std::string key(safep, safe_length);
    if (m_is_window_function) {
      /*
        When a row is leaving a frame, we have two options:
        1. If rows are ordered according to the "key", then remove
        the key/value pair from Json_object if this row is the
        last row in peerset for that key.
        2. If unordered, reduce the count in the key map for this key.
        If the count is 0, remove the key/value pair from the Json_object.
      */
      if (m_window->do_inverse()) {
        auto object = down_cast<Json_object *>(m_wrapper->to_dom());
        if (m_optimize)  // Option 1
        {
          if (m_window->is_last_row_in_peerset_within_frame())
            object->remove(key);
        } else  // Option 2
        {
          auto it = m_key_map.find(key);
          if (it != m_key_map.end()) {
            int count = it->second - 1;
            if (count > 0) {
              it->second = count;
            } else {
              m_key_map.erase(it);
              object->remove(key);
            }
          }
        }
        object->cardinality() == 0 ? null_value = true : null_value = false;
        return false;
      }
    }
    // value
    Json_wrapper value_wrapper;
    if (get_atom_null_as_null(args, 1, func_name(), &m_value,
                              &m_conversion_buffer, &value_wrapper))
      return error_json();

    /*
      The m_wrapper always points to m_json_object or the result of
      deserializing the result_field in reset/update_field.
    */
    Json_object *object = down_cast<Json_object *>(m_wrapper->to_dom());
    if (object->add_alias(key, value_wrapper.to_dom()))
      return error_json(); /* purecov: inspected */
    /*
      If rows in the window are not ordered based on "key", add this key
      to the key map.
    */
    if (m_is_window_function && !m_optimize) {
      int count = 1;
      auto it = m_key_map.find(key);
      if (it != m_key_map.end()) {
        count = count + it->second;
        it->second = count;
      } else
        m_key_map.emplace(std::make_pair(key, count));
    }

    null_value = false;
    // object will take ownership of the value
    value_wrapper.set_alias();
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  return false;
}

Item *Item_sum_json_object::copy_or_same(THD *thd) {
  if (m_is_window_function) return this;

  auto wrapper = make_unique_destroy_only<Json_wrapper>(thd->mem_root);
  if (wrapper == nullptr) return nullptr;

  unique_ptr_destroy_only<Json_object> object{::new (thd->mem_root)
                                                  Json_object};
  if (object == nullptr) return nullptr;

  return new (thd->mem_root)
      Item_sum_json_object(thd, this, std::move(wrapper), std::move(object));
}

/**
  Resolve the fields in the GROUPING function.
  The GROUPING function can only appear in SELECT list or
  in HAVING clause and requires WITH ROLLUP. Check that this holds.
  We also need to check if all the arguments of the function
  are present in GROUP BY clause. As GROUP BY columns are not
  resolved at this time, we do it in Query_block::resolve_rollup().
  However, if the GROUPING function is found in HAVING clause,
  we can check here. Also, resolve_rollup() does not
  check for items present in HAVING clause.

  @param[in]     thd        current thread
  @param[in,out] ref        reference to place where item is
                            stored
  @retval
    true  if error
  @retval
    false on success

*/
bool Item_func_grouping::fix_fields(THD *thd, Item **ref) {
  /*
    We do not allow GROUPING by position. However GROUP BY allows
    it for now.
  */
  Item **arg, **arg_end;
  for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
    if ((*arg)->type() == Item::INT_ITEM && (*arg)->basic_const_item()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "GROUPING function");
      return true;
    }
  }

  if (Item_func::fix_fields(thd, ref)) return true;

  // Make GROUPING function dependent upon all tables (prevents const-ness)
  used_tables_cache |= thd->lex->current_query_block()->all_tables_map();

  /*
    More than 64 args cannot be supported as the bitmask which is
    used to represent the result cannot accommodate.
  */
  if (arg_count > 64) {
    my_error(ER_INVALID_NO_OF_ARGS, MYF(0), "GROUPING", arg_count, "64");
    return true;
  }

  /*
    GROUPING() is not allowed in a WHERE condition or a JOIN condition and
    cannot be used without rollup.
  */
  Query_block *select = thd->lex->current_query_block();

  if (select->olap == UNSPECIFIED_OLAP_TYPE ||
      select->resolve_place == Query_block::RESOLVE_JOIN_NEST ||
      select->resolve_place == Query_block::RESOLVE_CONDITION) {
    my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
    return true;
  }

  return false;
}

/**
  Evaluation of the GROUPING function.
  We check the type of the item for all the arguments of
  GROUPING function. If it's a NULL_RESULT_ITEM, set the bit for
  the field in the result. The result of the GROUPING function
  would be the integer bit mask having 1's for the arguments
  of type NULL_RESULT_ITEM.

  @return
  integer bit mask having 1's for the arguments which have a
  NULL in their result because of ROLLUP operation.
*/
longlong Item_func_grouping::val_int() {
  longlong result = 0;
  for (uint i = 0; i < arg_count; i++) {
    Item *real_item = args[i]->real_item();
    if (has_rollup_result(real_item)) {
      result += 1ULL << (arg_count - (i + 1));
    }
  }
  return result;
}

/**
  Used by Distinct_check::check_query to determine whether an
  error should be returned if the GROUPING item from the ORDER
  is not present in the select list.

  @retval
    true  if error
  @retval
    false on success
*/
bool Item_func_grouping::aggregate_check_distinct(uchar *arg) {
  assert(fixed);
  Distinct_check *dc = reinterpret_cast<Distinct_check *>(arg);

  /**
    If the GROUPING function in ORDER BY is not in the SELECT list, it
    might not be functionally dependent on all selected expressions, and thus
    might produce random order in combination with DISTINCT; so we reject
    it.
  */
  if (dc->is_stopped(this)) return false;
  return true;
}

/**
  This function is expected to check if GROUPING function with
  its arguments is "group-invariant".
  However, GROUPING function produces only one value per
  group similar to the other set functions and the arguments
  to the GROUPING function are always present in GROUP BY (this
  is checked in resolve_rollup() which is called much earlier to
  aggregate_check_group). As a result, aggregate_check_group does
  not have to determine if the result of this function is
  "group-invariant".

  @retval
    true  if error
  @retval
    false on success
*/
bool Item_func_grouping::aggregate_check_group(uchar *arg) {
  Group_check *gc = reinterpret_cast<Group_check *>(arg);

  if (gc->is_stopped(this)) return false;

  if (gc->is_fd_on_source(this)) {
    gc->stop_at(this);
    return false;
  }
  return true;
}

void Item_func_grouping::update_used_tables() {
  Item_int_func::update_used_tables();
  set_grouping_func();
  set_rollup_expr();
  /*
    GROUPING function can never be a constant item. It's
    result always depends on ROLLUP result.
  */
  used_tables_cache |=
      current_thd->lex->current_query_block()->all_tables_map();
}

inline Item *Item_rollup_sum_switcher::current_arg() const {
  assert(m_current_rollup_level >= 0 && m_current_rollup_level < m_num_levels);
  return args[m_current_rollup_level];
}

bool Item_rollup_sum_switcher::get_date(MYSQL_TIME *ltime,
                                        my_time_flags_t fuzzydate) {
  assert(fixed);
  return (null_value = current_arg()->get_date(ltime, fuzzydate));
}

bool Item_rollup_sum_switcher::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  return (null_value = current_arg()->get_time(ltime));
}

double Item_rollup_sum_switcher::val_real() {
  assert(fixed);
  double res = current_arg()->val_real();
  if ((null_value = current_arg()->null_value)) return 0.0;
  return res;
}

longlong Item_rollup_sum_switcher::val_int() {
  assert(fixed);
  longlong res = current_arg()->val_int();
  if ((null_value = current_arg()->null_value)) return 0;
  return res;
}

String *Item_rollup_sum_switcher::val_str(String *str) {
  assert(fixed);
  String *res = current_arg()->val_str(str);
  if ((null_value = current_arg()->null_value)) return nullptr;
  return res;
}

my_decimal *Item_rollup_sum_switcher::val_decimal(my_decimal *dec) {
  assert(fixed);
  my_decimal *res = current_arg()->val_decimal(dec);
  if ((null_value = current_arg()->null_value)) return nullptr;
  return res;
}

bool Item_rollup_sum_switcher::val_json(Json_wrapper *result) {
  assert(fixed);
  bool res = current_arg()->val_json(result);
  null_value = current_arg()->null_value;
  return res;
}

bool Item_rollup_sum_switcher::is_null() {
  assert(fixed);
  return current_arg()->is_null();
}

void Item_rollup_sum_switcher::print(const THD *thd, String *str,
                                     enum_query_type query_type) const {
  if (query_type & QT_HIDE_ROLLUP_FUNCTIONS) {
    master()->print(thd, str, query_type);
  } else {
    Item_sum::print(thd, str, query_type);
  }
}

Field *Item_rollup_sum_switcher::create_tmp_field(bool group, TABLE *table) {
  return master()->create_tmp_field(group, table);
}

void Item_rollup_sum_switcher::clear() {
  for (int i = 0; i < m_num_levels; ++i) {
    child(i)->clear();
  }
}

bool Item_rollup_sum_switcher::reset_and_add_for_rollup(
    int last_unchanged_group_item_idx) {
  for (int i = 0; i < m_num_levels; ++i) {
    if (i >= last_unchanged_group_item_idx) {
      if (child(i)->reset_and_add()) return true;
    } else {
      if (child(i)->aggregator_add()) return true;
    }
  }
  return false;
}

int Item_rollup_sum_switcher::set_aggregator(
    Aggregator::Aggregator_type aggregator) {
  for (int i = 0; i < m_num_levels; ++i) {
    int err = child(i)->set_aggregator(aggregator);
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

bool Item_rollup_sum_switcher::aggregator_setup(THD *thd) {
  for (int i = 0; i < m_num_levels; ++i) {
    if (child(i)->aggregator_setup(thd)) {
      return true;
    }
  }
  return false;
}

namespace {
std::unique_ptr<gis::Geometrycollection> filtergeometries(
    std::unique_ptr<gis::Geometrycollection> geometrycollection,
    const dd::Spatial_reference_system *srs) {
  assert(geometrycollection.get() != nullptr);
  auto filtered_geometries = std::unique_ptr<gis::Geometrycollection>(
      gis::Geometrycollection::create_geometrycollection(
          geometrycollection->coordinate_system()));
  for (size_t i = 0; i < geometrycollection->size(); i++) {
    auto comparator = [&srs](gis::Geometry *geometrya,
                             gis::Geometry *geometryb) {
      bool equals = false;
      bool isnull = false;
      gis::equals(srs, geometrya, geometryb, "ST_Collect", &equals, &isnull);
      return equals;
    };
    bool equals = false;
    for (size_t j = 0; j < filtered_geometries->size(); ++j) {
      equals |= comparator(&filtered_geometries->operator[](j),
                           &geometrycollection->operator[](i));
    }
    if (!equals) {
      filtered_geometries->push_back(geometrycollection->operator[](i));
    }
  }
  return filtered_geometries;
}
}  // namespace

bool Item_sum_collect::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);
  result_field = nullptr;

  if (Super::fix_fields(thd, ref)) return true; /* purecov: inspected */

  if (init_sum_func_check(thd)) return true;

  assert(arg_count == 1);
  if ((!args[0]->fixed && args[0]->fix_fields(thd, args)) ||
      args[0]->check_cols(1))
    return true;

  if (resolve_type(thd)) return true;

  if (check_sum_func(thd, ref)) return true;

  set_nullable(true);
  null_value = true;
  fixed = true;
  return false;
}
bool Item_sum_collect::check_wf_semantics1(THD *, Query_block *,
                                           Window_evaluation_requirements *r) {
  const PT_frame *frame = m_window->frame();
  r->needs_buffer = !(frame->m_query_expression == WFU_ROWS &&
                      frame->m_from->m_border_type == WBT_UNBOUNDED_PRECEDING &&
                      frame->m_to->m_border_type == WBT_CURRENT_ROW);
  return false;
}

void Item_sum_collect::clear() {
  m_geometrycollection.reset();
  null_value = true;
  srid = std::optional<gis::srid_t>{};
}

bool Item_sum_collect::add() {
  assert(fixed == 1);
  assert(arg_count == 1);

  THD *thd = base_query_block->parent_lex->thd;

  GeometryExtractionResult geometryExtractionResult =
      ExtractGeometry(*args, thd, func_name());

  std::unique_ptr<gis::Geometry> currentGeometry;
  gis::srid_t currentSrid = 0;

  switch (geometryExtractionResult.GetResultType()) {
    case ResultType::Error:
      return true;
    case ResultType::NullValue:
      return false;
    case ResultType::Value:
      currentGeometry = geometryExtractionResult.GetValue();
      currentSrid = geometryExtractionResult.GetSrid();
      break;
  }

  if (m_geometrycollection.get() == nullptr) {
    m_geometrycollection = std::unique_ptr<gis::Geometrycollection>(
        gis::Geometrycollection::create_geometrycollection(
            currentGeometry->coordinate_system()));
    srid = currentSrid;
  }
  if (srid == currentSrid || (!srid.has_value() && currentSrid == 0)) {
    try {
      m_geometrycollection->push_back(*currentGeometry.get());
      null_value = false;
    } catch (...) {
      /* purecov: begin inspected */
      handle_std_exception(func_name());
      return true;
      /* purecov: end */
    }
  } else {  // srid mismatch
    my_error(ER_GIS_DIFFERENT_SRIDS_AGGREGATION, MYF(0), func_name(),
             srid.value(), currentSrid);
    return true;
  }
  return false;
}

Item *Item_sum_collect::copy_or_same(THD *thd) {
  return m_is_window_function ? this
                              : new (thd->mem_root) Item_sum_collect(thd, this);
}

void Item_sum_collect::read_result_field() {
  GeometryExtractionResult geometryExtractionResult =
      ExtractGeometry(result_field, current_thd, func_name());
  switch (geometryExtractionResult.GetResultType()) {
    case ResultType::Error:
      return;
    case ResultType::NullValue:
      clear();
      return;
    case ResultType::Value:
      std::unique_ptr<gis::Geometry> geo = geometryExtractionResult.GetValue();
      srid = geometryExtractionResult.GetSrid();
      switch (geo->type()) {
        case gis::Geometry_type::kGeometrycollection:
          m_geometrycollection = std::unique_ptr<gis::Geometrycollection>(
              down_cast<gis::Geometrycollection *>(geo.get())->clone());
          break;
        case gis::Geometry_type::kMultipoint:
        case gis::Geometry_type::kMultilinestring:
        case gis::Geometry_type::kMultipolygon: {
          m_geometrycollection = std::unique_ptr<gis::Geometrycollection>(
              gis::Geometrycollection::create_geometrycollection(
                  geo->coordinate_system()));
          gis::Geometrycollection *geometrycollection =
              down_cast<gis::Geometrycollection *>(geo.get());
          for (size_t i = 0; i < geometrycollection->size(); i++) {
            m_geometrycollection->push_back(geometrycollection->operator[](i));
          }
        } break;
        default: {
          assert(0);
        }
      }
  }
}

void Item_sum_collect::pop_front() {
  m_geometrycollection->pop_front();
  if (m_geometrycollection->size() == 0) {
    clear();
  }
}

String *Item_sum_collect::val_str(String *str) {
  if (m_is_window_function) {
    if (wf_common_init()) {
      return error_str();
    }
    if (m_window->do_inverse()) {
      String backing_arg_wkb;
      args[0]->val_str(&backing_arg_wkb);
      if (!args[0]->is_null()) {
        pop_front();
      }
    } else {
      if (add()) return error_str();
    }
  }
  const dd::Spatial_reference_system *srs = nullptr;
  auto releaser = std::make_unique<dd::cache::Dictionary_client::Auto_releaser>(
      current_thd->dd_client());
  if (srid.has_value() && srid.value() != 0) {
    Srs_fetcher fetcher(current_thd);
    if (fetcher.acquire(srid.value(), &srs)) {
      return error_str();
    }
    if (srs == nullptr) {
      my_error(ER_SRS_NOT_FOUND, MYF(0), srid.value());
      return error_str();
    }
  }

  if (m_geometrycollection.get() == nullptr) {
    null_value = true;
    return error_str();
  }
  std::unique_ptr<gis::Geometrycollection> narrowerCollection;
  if (has_with_distinct()) {
    narrowerCollection = narrowest_multigeometry(filtergeometries(
        std::unique_ptr<gis::Geometrycollection>(m_geometrycollection->clone()),
        srs));
  } else {
    narrowerCollection =
        gis::narrowest_multigeometry(std::unique_ptr<gis::Geometrycollection>(
            m_geometrycollection->clone()));
  }
  gis::write_geometry(srs, *narrowerCollection, str);
  return str;
}

void Item_sum_collect::update_field() {
  read_result_field();
  add();
  store_result_field();
}

void Item_sum_collect::store_result_field() {
  if (m_geometrycollection.get() != nullptr) {
    const dd::Spatial_reference_system *srs = nullptr;
    auto releaser =
        std::make_unique<dd::cache::Dictionary_client::Auto_releaser>(
            current_thd->dd_client());
    if (srid.has_value() && srid.value() != 0) {
      if (Srs_fetcher(current_thd).acquire(srid.value(), &srs) ||
          srs == nullptr) {
        // We may end up here in two cases:
        //
        // 1) Something went wrong during DD lookup and an error has
        // already been flagged in the thd. It's unclear if this may
        // actually happen at this point.
        //
        // 2) The SRS doesn't exist. This should not happen since the
        // SRS has been looked up earlier without error.
        //
        // Since this function doesn't have a way to signal errors, our
        // only option is to make sure an error is flagged in the thd
        // and return and hope it will caught by the caller. In case
        // (2), we have to report a new error. In case (1), an error has
        // already been reported, but it doesn't hurt to do it again.
        //
        // If any of these cases actually occur, the error handling in
        // and around this function must be reviewed.
        assert(false);
        my_error(ER_SRS_NOT_FOUND, MYF(0), srid.value());
        return;
      }
    }

    std::unique_ptr<gis::Geometrycollection> narrowerCollection;
    narrowerCollection =
        narrowest_multigeometry(std::unique_ptr<gis::Geometrycollection>(
            m_geometrycollection->clone()));

    String str;
    gis::write_geometry(srs, *narrowerCollection, &str);
    Field_geom *multipoint_field = down_cast<Field_geom *>(result_field);
    auto storeRes =
        multipoint_field->store(str.ptr(), str.length(), str.charset());
    if (storeRes) {
      return;
    }
    result_field->set_notnull();
  } else {
    result_field->reset();
    result_field->set_null();
  }
}

my_decimal *Item_sum_collect::val_decimal(my_decimal *decimal_value) {
  assert(fixed == 1);
  double2my_decimal(E_DEC_FATAL_ERROR, 0.0, decimal_value);
  return decimal_value;
}
void Item_sum_collect::reset_field() {
  clear();
  result_field->reset();
  result_field->set_null();
  add();
  store_result_field();
}
