/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/window.h"

#include <sys/types.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

#include "field_types.h"
#include "my_alloc.h"  // destroy
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "my_time.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/derror.h"  // ER_THD
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"       // Item_sum
#include "sql/item_timefunc.h"  // Item_date_add_interval
#include "sql/join_optimizer/finalize_plan.h"
#include "sql/join_optimizer/replace_item.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld_cs.h"
#include "sql/parse_location.h"
#include "sql/parse_tree_nodes.h"   // PT_*
#include "sql/parse_tree_window.h"  // PT_window
#include "sql/sql_array.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_lex.h"                // Query_block
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_resolver.h"   // find_order_in_list
#include "sql/sql_show.h"
#include "sql/sql_tmp_table.h"  // free_tmp_table
#include "sql/table.h"
#include "sql/temp_table_param.h"  // Temp_table_param
#include "sql/window_lex.h"
#include "sql_string.h"
#include "template_utils.h"

/**
  Shallow clone the list of ORDER objects using mem_root and return
  the cloned list.
*/
static ORDER *clone(THD *thd, ORDER *order) {
  ORDER *clone = nullptr;
  ORDER **prev_next = &clone;
  for (; order != nullptr; order = order->next) {
    ORDER *o = new (thd->mem_root) PT_order_expr(POS(), nullptr, ORDER_ASC);
    std::memcpy(o, order, sizeof(*order));
    *prev_next = o;
    prev_next = &o->next;
  }

  *prev_next = nullptr;  // final object should have a null next pointer
  return clone;
}

/**
  Append order expressions at the end of *first_next ordering list
  representing the partitioning columns.
*/
static void append_to_back(ORDER **first_next, ORDER *column) {
  ORDER **prev_next = first_next;
  /*
    find last next pointer in list and make that point to column
    effectively appending it.
  */
  for (; *prev_next != nullptr; prev_next = &(*prev_next)->next) {
  }
  *prev_next = column;
}

ORDER *Window::first_partition_by() const {
  return m_partition_by != nullptr ? m_partition_by->value.first : nullptr;
}

ORDER *Window::first_order_by() const {
  return m_order_by != nullptr ? m_order_by->value.first : nullptr;
}

bool Window::check_window_functions1(THD *thd, Query_block *select) {
  m_static_aggregates =
      (m_frame->m_from->m_border_type == WBT_UNBOUNDED_PRECEDING &&
       m_frame->m_to->m_border_type == WBT_UNBOUNDED_FOLLOWING);

  // If static aggregates, inversion isn't necessary
  m_row_optimizable =
      (m_frame->m_query_expression == WFU_ROWS) && !m_static_aggregates;
  m_range_optimizable =
      (m_frame->m_query_expression == WFU_RANGE) && !m_static_aggregates;

  for (Item_sum &wf : m_functions) {
    Window_evaluation_requirements reqs;

    if (wf.check_wf_semantics1(thd, select, &reqs)) return true;

    // [Not] buffering depends only on facts known at resolution time
    m_needs_frame_buffering |= reqs.needs_buffer;
    if (reqs.needs_peerset) {
      /*
        A framing function looks at the frame only (which may or not include
        the peers, but it's irrelevant: what matters is the frame's set, not
        the peer set in itself).
      */
      assert(!wf.framing());
      m_needs_peerset = true;
    }
    if (reqs.needs_last_peer_in_frame) {
      assert(wf.framing());
      m_needs_last_peer_in_frame = true;
    }
    if (wf.needs_partition_cardinality()) {
      assert(!wf.framing());
      m_needs_partition_cardinality = true;
    }
    m_opt_first_row |= reqs.opt_first_row;
    m_opt_last_row |= reqs.opt_last_row;
    m_row_optimizable &= reqs.row_optimizable;
    m_range_optimizable &= reqs.range_optimizable;

    if (thd->lex->is_explain() && !m_frame->m_originally_absent &&
        !wf.framing()) {
      /*
        SQL2014 <window clause> SR6b: functions which do not respect frames
        shouldn't have any frame specification in their window; we are more
        relaxed, as some users may find it handy to have one single
        window definition for framing and non-framing functions; but in case
        it's a user's mistake, we send a Note in EXPLAIN.
      */
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_WINDOW_FUNCTION_IGNORES_FRAME,
                          ER_THD(thd, ER_WINDOW_FUNCTION_IGNORES_FRAME),
                          wf.func_name(), printable_name());
    }
  }

  return false;
}

static Item_cache *make_result_item(Item *value) {
  Item *order_expr = down_cast<Item_ref *>(value)->ref_item();
  Item_cache *result = nullptr;
  Item_result result_type = order_expr->result_type();

  // In case of enum/set type, ordering is based on numeric
  // comparison. So, we need to create items that will
  // evaluate to integers.
  if (order_expr->real_item()->type() == Item::FIELD_ITEM) {
    Item_field *field = down_cast<Item_field *>(order_expr->real_item());
    if (field->field->real_type() == MYSQL_TYPE_ENUM ||
        field->field->real_type() == MYSQL_TYPE_SET) {
      result_type = INT_RESULT;
    }
  }

  switch (result_type) {
    case INT_RESULT:
      result = new Item_cache_int(value->data_type());
      break;
    case REAL_RESULT:
      result = new Item_cache_real();
      break;
    case DECIMAL_RESULT:
      result = new Item_cache_decimal();
      break;
    case STRING_RESULT:
      if (value->is_temporal())
        result = new Item_cache_datetime(value->data_type());
      else if (value->data_type() == MYSQL_TYPE_JSON)
        result = new Item_cache_json();
      else
        result = new Item_cache_str(value);
      break;
    default:
      assert(false);
  }

  result->setup(value);
  return result;
}

/**
  Return element with index i from list

  @param list List of ORDER elements
  @param i zero-based index

  @return element at index i, or nullptr if i out of range
*/
static ORDER *elt(const SQL_I_List<ORDER> &list, uint i) {
  ORDER *o = list.first;
  while (o != nullptr) {
    if (i-- == 0) return o;
    o = o->next;
  }
  assert(false);
  return nullptr;
}

bool Window::setup_range_expressions(THD *thd) {
  assert(m_frame->m_query_expression == WFU_RANGE);
  const PT_order_list *o = effective_order_by();

  if (o == nullptr) {
    /*
      Without ORDER BY, all rows are peers, so in a RANGE frame CURRENT ROW
      extends to infinity, which we rewrite accordingly.
      We do not touch other border types (e.g. N PRECEDING) as they must be
      checked in more detail later.
    */
    {
      if (m_frame->m_from->m_border_type == WBT_CURRENT_ROW)
        m_frame->m_from->m_border_type = WBT_UNBOUNDED_PRECEDING;
      if (m_frame->m_to->m_border_type == WBT_CURRENT_ROW)
        m_frame->m_to->m_border_type = WBT_UNBOUNDED_FOLLOWING;
    }
  }

  for (PT_border *border : {m_frame->m_from, m_frame->m_to}) {
    enum_window_border_type border_type = border->m_border_type;
    switch (border_type) {
      case WBT_UNBOUNDED_PRECEDING:
      case WBT_UNBOUNDED_FOLLOWING:
        /* no computation required */
        break;
      case WBT_VALUE_PRECEDING:
      case WBT_VALUE_FOLLOWING: {
        /*
          Frame uses RANGE <value>, require ORDER BY with one column
          cf. SQL 2014 7.15 <window clause>, SR 13.a.ii
        */
        if (!o || o->value.size() != 1) {
          my_error(ER_WINDOW_RANGE_FRAME_ORDER_TYPE, MYF(0), printable_name());
          return true;
        }

        /* check the ORDER BY type */
        Item *order_expr = *(o->value.first->item);
        switch (order_expr->result_type()) {
          case INT_RESULT:
          case REAL_RESULT:
          case DECIMAL_RESULT:
            goto ok;
          case STRING_RESULT:
            if (order_expr->is_temporal()) goto ok;
          default:;
        }
        my_error(ER_WINDOW_RANGE_FRAME_ORDER_TYPE, MYF(0), printable_name());
        return true;
      ok:;
      }
        [[fallthrough]];
      case WBT_CURRENT_ROW: {
        auto comparators = Bounds_checked_array<Arg_comparator>::Alloc(
            thd->mem_root, o->value.size());
        for (size_t i = 0; i < o->value.size(); ++i) {
          bool asc = elt(o->value, i)->direction == ORDER_ASC;
          Item *nr = m_order_by_items[i]->get_item();

          /*
            Below, "value" is the value of ORDER BY expr at current row for
            which we must compute the window function.
            "nr" is the value of the ORDER BY expr at another row in partition
            which we want to determine whether resided in the specified RANGE.

            We poke in the actual value of expr of the current row (cached) into
            value in reset_order_by_peer_set().
          */

          Item_cache *value = make_result_item(nr);
          if (value == nullptr) return true;

          /*
            See comments on m_comparator.

            WBT_CURRENT_ROW:
              nr ⋛ value   (where ⋛ is three-way comparison)

            If we have multiple ORDER BY expressions, before_or_after_frame()
            calls them in turn as needed, including special NULL handling.

            WBT_VALUE_PRECEDING:
               asc ? nr ⋛ value - border->val_int() :
                     nr ⋛ value + border->val_int())

            For WBT_VALUE_FOLLOWING, - becomes + and vice versa.
          */
          Item *cmp_arg;
          if (border_type == WBT_VALUE_PRECEDING ||
              border_type == WBT_VALUE_FOLLOWING) {
            assert(i == 0);  // only one expr allowed with WBT_VALUE_*
            cmp_arg = border->build_addop(
                value, border_type == WBT_VALUE_PRECEDING, asc, this);
            if (cmp_arg == nullptr) return true;
          } else {
            cmp_arg = value;
          }

          Item **left_args = new (thd->mem_root) Item *(nr);
          Item **right_args = new (thd->mem_root) Item *(cmp_arg);
          if (!cmp_arg->fixed) {
            if (cmp_arg->fix_fields(thd, right_args)) {
              return true;
            }
          }

          // Special case to handle "INTERVAL expr" border. It is special
          // because we allow a general expression there, not just a
          // literal. If expr is a constant subquery like (SELECT 1), it gets
          // replaced during fix_fields above with an Item_int. If it is not
          // constant, we will detect it later in check_constant_bound.
          if (border_type == WBT_VALUE_PRECEDING ||
              border_type == WBT_VALUE_FOLLOWING) {
            Item *border_val = down_cast<Item_func *>(cmp_arg)->arguments()[1];
            if (border_val != *border->border_ptr())  // replaced?
              *border->border_ptr() = border_val;
          }

          comparators[i] = Arg_comparator(left_args, right_args);
          bool compare_func_set = false;
          // In case of enum/set type, as ordering is based on
          // numeric comparison we need to setup comparison
          // functions to do numeric comparison.
          if (nr->real_item()->type() == Item::FIELD_ITEM) {
            Item_field *field = down_cast<Item_field *>(nr->real_item());
            if (field->field->real_type() == MYSQL_TYPE_ENUM ||
                field->field->real_type() == MYSQL_TYPE_SET) {
              if (comparators[i].set_cmp_func(/*owner_arg=*/nullptr, left_args,
                                              right_args, /*set_null_arg=*/true,
                                              INT_RESULT)) {
                return true;
              }
              compare_func_set = true;
            }
          }
          if (!compare_func_set) {
            if (comparators[i].set_cmp_func(/*owner_arg=*/nullptr, left_args,
                                            right_args,
                                            /*set_null_arg=*/true)) {
              return true;
            }
          }
        }
        m_comparators[border == m_frame->m_to] = comparators;
        break;
      }
    }
  }

  return false;
}

ORDER *Window::sorting_order(THD *thd, bool implicitly_grouped) {
  if (thd == nullptr) return m_sorting_order;

  if (implicitly_grouped) {
    m_sorting_order = nullptr;
    return nullptr;
  }

  ORDER *part = effective_partition_by() ? effective_partition_by()->value.first
                                         : nullptr;
  ORDER *ord =
      effective_order_by() ? effective_order_by()->value.first : nullptr;

  /*
    1. Copy both lists
    2. Append the ORDER BY list to the partition list.

    This ensures that all columns are present in the resulting sort ordering
    and that all ORDER BY expressions are at the end.
    The resulting sort can the be used to detect partition change and also
    satisfy the window ordering.
  */
  if (ord == nullptr)
    m_sorting_order = part;
  else if (part == nullptr)
    m_sorting_order = ord;
  else {
    ORDER *sorting = clone(thd, part);
    ORDER *ob = clone(thd, ord);
    append_to_back(&sorting, ob);
    m_sorting_order = sorting;
  }
  return m_sorting_order;
}

bool Window::resolve_reference(THD *thd, Item_sum *wf, PT_window **m_window) {
  assert(thd->lex->current_query_block()->first_execution);

  if (!(*m_window)->is_reference()) {
    (*m_window)->m_functions.push_back(wf);
    return false;
  }

  Query_block *curr = thd->lex->current_query_block();

  for (Window &w : curr->m_windows) {
    if (w.name() == nullptr) continue;

    if (my_strcasecmp(system_charset_info, (*m_window)->printable_name(),
                      w.printable_name()) == 0) {
      (*m_window)->~PT_window();  // destroy the reference, no further need

      /* Replace with pointer to the definition */
      (*m_window) = static_cast<PT_window *>(&w);
      (*m_window)->m_functions.base_list::push_back(wf);
      return false;
    }
  }

  my_error(ER_WINDOW_NO_SUCH_WINDOW, MYF(0), (*m_window)->printable_name());
  return true;
}

void Window::check_partition_boundary() {
  DBUG_TRACE;
  bool anything_changed = false;

  if (m_part_row_number == 0)  // first row in first partition
  {
    anything_changed = true;
  }

  /**
    If we have partitioning and any one of the partitioning columns have
    changed since last row, we have a new partition.
  */
  for (Cached_item *item : m_partition_items) {
    anything_changed |= item->cmp();
  }

  m_partition_border = anything_changed;

  if (m_partition_border) {
    m_part_row_number = 1;
    m_first_rowno_in_range_frame = 1;
  } else {
    m_part_row_number++;
  }
}

/*
  For a comparator from m_comparators, locate the Item_cache to update
  with a new reference value (see Window::m_comparators).

  The comparator is one of

    candidate {<, >} current row
    candidate {<, >} current row {-,+} constant

  The second form is used when the the RANGE frame boundary is
  WBT_VALUE_PRECEDING/WBT_VALUE_FOLLOWING, "constant" above being the
  value specified in the query, cf. the setup in
  Window::setup_range_expressions.
*/
static Item_cache *FindCacheInComparator(const Arg_comparator &cmp) {
  Item *to_update;
  if (cmp.get_right()->type() == Item::CACHE_ITEM) {
    to_update = cmp.get_right();
  } else {
    to_update = down_cast<Item_func *>(cmp.get_right())->get_arg(0);
  }
  return down_cast<Item_cache *>(to_update);
}

void Window::reset_order_by_peer_set() {
  DBUG_TRACE;

  for (Cached_item *item : m_order_by_items) {
    /*
      A side-effect of doing this comparison, is to update the cache, so that
      when we compare the new value to itself later, it is in its peer set.
    */
    (void)item->cmp();
  }

  // Update the reference value for ORDER BY elements as used by
  // before_or_after_frame(). These are the actual items used in the three-way
  // comparisons, whereas cached_item is used in in_new_order_by_peer_set().
  for (int i = 0; i < 2; ++i) {
    for (Arg_comparator &cmp : m_comparators[i]) {
      FindCacheInComparator(cmp)->cache_value();
    }
  }
}

bool Window::in_new_order_by_peer_set(bool compare_all_order_by_items) {
  DBUG_TRACE;
  bool anything_changed = false;

  for (Cached_item *item : m_order_by_items) {
    anything_changed |= item->cmp();
    if (!compare_all_order_by_items) break;
  }

  return anything_changed;
}

bool Window::before_or_after_frame(bool before) {
  PT_border *border;
  enum_window_border_type infinity;  // the extreme bound of the border
  if (before) {
    border = frame()->m_from;
    infinity = WBT_UNBOUNDED_PRECEDING;
  } else {
    border = frame()->m_to;
    infinity = WBT_UNBOUNDED_FOLLOWING;
  }

  const enum enum_window_border_type border_type = border->m_border_type;

  if (border_type == infinity) return false;  // all rows included

  /*
    If multiple ORDER BY expressions: only CURRENT ROW need be considered
    since infinity handled above.
  */
  assert(
      border_type == WBT_CURRENT_ROW ||
      (m_order_by_items.size() == 1 && (border_type == WBT_VALUE_PRECEDING ||
                                        border_type == WBT_VALUE_FOLLOWING)));

  uint i = 0;
  Bounds_checked_array<Arg_comparator> comparators = m_comparators[!before];

  ORDER *o_expr = effective_order_by()->value.first;

  for (auto it = m_order_by_items.begin(); it != m_order_by_items.end();
       ++it, o_expr = o_expr->next, ++i) {
    Cached_item *cur_row = *it;

    /*
      'cur_row' represents the value of the current row's windowing ORDER BY
      expression, and 'candidate' represents the same expression in the
      candidate row. Our caller is calculating the WF's value for 'cur_row';
      to this aim, here we want to know if 'candidate' is part of the frame of
      'cur_row'.

      First, as the candidate row has just been copied back from the frame
      buffer, we must update the item's null_value
    */
    Item *candidate = cur_row->get_item();
    if (candidate->update_null_value()) return true;

    const bool asc = o_expr->direction == ORDER_ASC;

    const bool nulls_at_infinity =  // true if NULLs stick to 'infinity'
        before ? asc : !asc;

    if (cur_row->null_value)  // Current row is NULL
    {
      /*
        Per the standard, if current row is NULL,
        <numeric value> PRECEDING/FOLLOWING is a bound which is positioned at
        "the NULLs" (=peers). So is CURRENT ROW. So, for example, in NULLS
        FIRST ordering, BETWEEN 2 FOLLOWING AND 3 FOLLOWING yields only the
        NULLs, while BETWEEN 2 FOLLOWING AND UNBOUNDED FOLLOWING yields the
        whole partition.
      */
      if (candidate->null_value)
        continue;  // peer, so can't be before or after
      else
        return !nulls_at_infinity;
    }

    if (candidate->null_value) return nulls_at_infinity;

    // Figure out if the candidate row is before/after the frame defined
    // wrt. the current row for which the window function value needs to be
    // calculated (see m_comparators for more details). If we are unequal,
    // we can say for sure based on this element alone that we are before
    // or after; if we are equal, we need to go on to the next element (if any).
    //
    // NOTE: The reference value has already been set in
    // reset_order_by_peer_set().
    int val = comparators[i].compare();
    if (val != 0) {
      if (!asc) val = -val;
      return before ? (val < 0) : (val > 0);
    }

    // Compared equal, so move to the next in the lexicographic comparison
    // (if any).
  }
  return false;
}

bool Window::check_unique_name(const List<Window> &windows) {
  if (m_name == nullptr) return false;

  for (const Window &w : windows) {
    if (w.name() == nullptr) continue;

    if (&w != this && m_name->eq(w.name())) {
      my_error(ER_WINDOW_DUPLICATE_NAME, MYF(0), printable_name());
      return true;
    }
  }

  return false;
}

bool Window::setup_ordering_cached_items(THD *thd, Query_block *select,
                                         const PT_order_list *o,
                                         bool partition_order) {
  if (o == nullptr) return false;

  for (ORDER *order = o->value.first; order; order = order->next) {
    if (partition_order) {
      Item_ref *ir =
          new Item_ref(&select->context, order->item, "<window partition by>");
      if (ir == nullptr) return true;

      Cached_item *ci = new_Cached_item(thd, ir);
      if (ci == nullptr) return true;

      m_partition_items.push_back(ci);
    } else {
      Item_ref *ir =
          new Item_ref(&select->context, order->item, "<window order by>");
      if (ir == nullptr) return true;

      Cached_item *ci = new_Cached_item(thd, ir);
      if (ci == nullptr) return true;

      m_order_by_items.push_back(ci);
    }
  }
  return false;
}

bool Window::resolve_window_ordering(THD *thd, Ref_item_array ref_item_array,
                                     Table_ref *tables,
                                     mem_root_deque<Item *> *fields, ORDER *o,
                                     bool partition_order) {
  DBUG_TRACE;
  assert(o);

  const char *sav_where = thd->where;
  thd->where = partition_order ? "window partition by" : "window order by";

  for (ORDER *order = o; order; order = order->next) {
    Item *oi = *order->item;

    /* Order by position is not allowed for windows: legacy SQL 1992 only */
    if (oi->type() == Item::INT_ITEM) {
      my_error(ER_WINDOW_ILLEGAL_ORDER_BY, MYF(0), printable_name());
      return true;
    }

    if (find_order_in_list(thd, ref_item_array, tables, order, fields, false,
                           true))
      return true;
    oi = *order->item;

    if (order->used_alias != nullptr) {
      /*
        Order by using alias is not allowed for windows, cf. SQL 2011, section
        7.11 <window clause>, SR 4. Throw the same error code as when alias is
        argument of a window function, or any function.
      */
      my_error(ER_BAD_FIELD_ERROR, MYF(0), order->used_alias, thd->where);
      return true;
    }

    if (!oi->fixed && oi->fix_fields(thd, order->item)) return true;
    oi = *order->item;  // fix_fields() may have changed *order->item

    /*
      Check SQL 2014 section 7.15 <window clause> SR 7 : A window cannot
      contain a windowing function without an intervening query expression.
    */
    if (oi->has_wf()) {
      my_error(ER_WINDOW_NESTED_WINDOW_FUNC_USE_IN_WINDOW_SPEC, MYF(0),
               printable_name());
      return true;
    }

    if (oi->propagate_type(thd, MYSQL_TYPE_VARCHAR)) return true;

    /*
      Call split_sum_func if an aggregate function is part of order by
      expression.
    */
    if (oi->has_aggregation() && oi->type() != Item::SUM_FUNC_ITEM) {
      if (oi->split_sum_func(thd, ref_item_array, fields)) {
        return true;
      }
    }
  }

  thd->where = sav_where;
  return false;
}

bool Window::equal_sort(Window *w1, Window *w2) {
  ORDER *o1 = w1->sorting_order();
  ORDER *o2 = w2->sorting_order();

  if (o1 == nullptr || o2 == nullptr) return false;

  while (o1 != nullptr && o2 != nullptr) {
    if (o1->direction != o2->direction || !(*o1->item)->eq(*o2->item))
      return false;

    o1 = o1->next;
    o2 = o2->next;
  }
  return o1 == nullptr && o2 == nullptr;  // equal so far, now also same length
}

void Window::reorder_and_eliminate_sorts(List<Window> *windows) {
  const size_t n = windows->size();
  std::vector<bool> redundant(n, false);
  for (uint i = 0; i < n - 1; i++) {
    for (uint j = i + 1; j < n; j++) {
      if (equal_sort((*windows)[i], (*windows)[j])) {
        if (j > i + 1) {
          // move up to right after window[i], so we can share sort
          windows->swap_elts(i + 1, j);
        }  // else already in right place
        redundant[i + 1] = true;
        break;
      }
    }
  }

  for (uint i = 0; i < n; i++)
    if (redundant[i]) (*windows)[i]->m_sorting_order = nullptr;
}

bool Window::check_constant_bound(THD *thd, PT_border *border) {
  const enum_window_border_type b_t = border->m_border_type;

  if (b_t == WBT_VALUE_PRECEDING || b_t == WBT_VALUE_FOLLOWING) {
    char const *save_where = thd->where;
    thd->where = "window frame bound";
    Item **border_ptr = border->border_ptr();

    /*
      For RANGE frames, resolving is already done in setup_range_expressions,
      so we need a test
    */
    assert(((*border_ptr)->fixed && m_frame->m_query_expression == WFU_RANGE) ||
           ((!(*border_ptr)->fixed || (*border_ptr)->basic_const_item()) &&
            m_frame->m_query_expression == WFU_ROWS));

    if (!(*border_ptr)->fixed && (*border_ptr)->fix_fields(thd, border_ptr))
      return true;

    if (!(*border_ptr)->const_for_execution() ||  // allow dyn. arg
        (*border_ptr)->has_subquery()) {
      my_error(ER_WINDOW_RANGE_BOUND_NOT_CONSTANT, MYF(0), printable_name());
      return true;
    }
    thd->where = save_where;
  }

  return false;
}

bool Window::check_border_sanity1(THD *thd) {
  const PT_frame &fr = *m_frame;

  for (PT_border *border : {fr.m_from, fr.m_to}) {
    enum_window_border_type border_t = border->m_border_type;
    switch (fr.m_query_expression) {
      case WFU_ROWS:
      case WFU_RANGE:

        // A check specific of the frame's start
        if (border == fr.m_from) {
          if (border_t == WBT_UNBOUNDED_FOLLOWING) {
            /*
              SQL 2014 section 7.15 <window clause>, SR 8.a
            */
            my_error(ER_WINDOW_FRAME_START_ILLEGAL, MYF(0), printable_name());
            return true;
          }
        }
        // A check specific of the frame's end
        else {
          if (border_t == WBT_UNBOUNDED_PRECEDING) {
            /*
              SQL 2014 section 7.15 <window clause>, SR 8.b
            */
            my_error(ER_WINDOW_FRAME_END_ILLEGAL, MYF(0), printable_name());
            return true;
          }
          enum_window_border_type from_t = fr.m_from->m_border_type;
          if ((from_t == WBT_CURRENT_ROW && border_t == WBT_VALUE_PRECEDING) ||
              (border_t == WBT_CURRENT_ROW &&
               (from_t == WBT_VALUE_FOLLOWING)) ||
              (from_t == WBT_VALUE_FOLLOWING &&
               border_t == WBT_VALUE_PRECEDING)) {
            /*
              SQL 2014 section 7.15 <window clause>, SR 8.c and 8.d
            */
            my_error(ER_WINDOW_FRAME_ILLEGAL, MYF(0), printable_name());
            return true;
          }
        }

        // Common code for start and end
        if (border_t == WBT_VALUE_PRECEDING ||
            border_t == WBT_VALUE_FOLLOWING) {
          // INTERVAL only allowed with RANGE
          if (fr.m_query_expression == WFU_ROWS && border->m_date_time) {
            my_error(ER_WINDOW_ROWS_INTERVAL_USE, MYF(0), printable_name());
            return true;
          }

          if (check_constant_bound(thd, border)) return true;

          /*
            ROWS ? PRECEDING/FOLLOWING: impose an integer type to '?'.
            For RANGE ? PRECEDING/FOLLOWING: type of '?' may be any
            numeric (int, decimal, int in the definition an interval): we
            try integer, if wrong we will reprepare.
          */
          if (border->m_value->propagate_type(
                  thd, MYSQL_TYPE_LONGLONG, fr.m_query_expression == WFU_ROWS))
            return true;
        }
        break;
      case WFU_GROUPS:
        assert(false);  // not yet implemented
        break;
    }
  }

  return false;
}

bool Window::check_border_sanity2(THD *thd) {
  const PT_frame &fr = *m_frame;

  PT_border *ba[] = {fr.m_from, fr.m_to};
  constexpr size_t siz = sizeof(ba) / sizeof(PT_border *);

  for (PT_border *border : Bounds_checked_array<PT_border *>(ba, siz)) {
    enum_window_border_type border_t = border->m_border_type;
    switch (fr.m_query_expression) {
      case WFU_ROWS:
      case WFU_RANGE:

        // Common code for start and end
        if (border_t == WBT_VALUE_PRECEDING ||
            border_t == WBT_VALUE_FOLLOWING) {
          if (!border->m_value->const_for_execution()) goto err;
          Item *o_item = nullptr;

          /*
            Only integer values can be specified as args for ROW frames.
            Note that due to type pinning, if the argument is a PS param its
            supplied value is silently cast to an integer before coming here.
            That explains why we accept 3.14 in '?', but not as a literal.
          */
          if (fr.m_query_expression == WFU_ROWS &&
              border->m_value->result_type() != INT_RESULT)
            goto err;
          else if (fr.m_query_expression == WFU_RANGE &&
                   (o_item = m_order_by_items[0]->get_item())->result_type() ==
                       STRING_RESULT &&
                   o_item->is_temporal()) {
            /*
              SQL 2014 section 7.15 <window clause>, GR 5.b.i.1.B.I.1: if value
              is NULL or negative, we should give an error.
            */
            Interval interval;
            char buffer[STRING_BUFFER_USUAL_SIZE];
            String value(buffer, sizeof(buffer), thd->collation());
            get_interval_value(border->m_value, border->m_int_type, &value,
                               &interval);
            if (border->m_value->null_value || interval.neg) goto err;
          } else if (border->m_value->val_real() < 0.0 ||
                     border->m_value->null_value) {
            // numeric type (integer, floating-point...) must not be negative
            goto err;  // GR 5.b.i.1.B.I.1
          }
        }
        break;
      case WFU_GROUPS:
        assert(false);  // not yet implemented
        break;
    }
  }

  return false;
err:
  my_error(ER_WINDOW_FRAME_ILLEGAL, MYF(0), printable_name());
  return true;
}

/**
  Simplified adjacency list: a window can maximum reference (depends on)
  one other window due to syntax restrictions. If there is no dependency,
  m_list[wno] == UNUSED. If w1 depends on w2, m_list[w1] == w2.
*/
class AdjacencyList {
 public:
  static constexpr uint UNUSED = std::numeric_limits<uint>::max();
  uint *const m_list;
  const uint m_size;
  AdjacencyList(uint elements) : m_list(new uint[elements]), m_size(elements) {
    for (auto &i : Bounds_checked_array<uint>(m_list, elements)) {
      i = UNUSED;
    }
  }
  ~AdjacencyList() { delete[] m_list; }

  /**
    Add a dependency.
    @param wno        the window that references another in its definition
    @param depends_on the window referenced
  */
  void add(uint wno, uint depends_on) {
    assert(wno <= m_size && depends_on <= m_size);
    assert(m_list[wno] == UNUSED);
    m_list[wno] = depends_on;
  }

  /**
    If the window depends on another window, return 1, else 0.

    @param wno the window
    @returns the out degree
  */
  uint out_degree(uint wno) {
    assert(wno <= m_size);
    return m_list[wno] == UNUSED ? 0 : 1;
  }

  /**
    Return the number of windows that depend on this one.

    @param wno the window
    @returns the in degree
  */
  uint in_degree(uint wno) {
    assert(wno <= m_size);
    uint degree = 0;  // a priori

    for (uint i : Bounds_checked_array<uint>(m_list, m_size)) {
      degree += i == wno ? 1 : 0;
    }
    return degree;
  }

  /**
    Return true of there is a circularity in the graph
  */
  bool check_circularity() {
    if (m_size == 1)
      return m_list[0] != UNUSED;  // could have been resolved to itself

    /*
      After a node has been added to 'completed', if we meet it again we don't
      need to explore the nodes it depends on.
    */
    std::unordered_set<uint> completed;

    for (uint i = 0; i < m_size; i++) {
      // Look for loop in the chain which starts at node #i

      if (completed.count(i) != 0) continue;  // Chain already checked.

      // Nodes visited in this chain:
      std::unordered_set<uint> visited;
      visited.insert(i);
      completed.insert(i);

      for (uint dep = m_list[i]; dep != UNUSED; dep = m_list[dep]) {
        assert(dep <= m_size);
        if (visited.count(dep) != 0) return true;  // found circularity
        visited.insert(dep);
        completed.insert(dep);
      }
    }
    return false;
  }
};

void Window::eliminate_unused_objects(List<Window> *windows) {
  /*
    Go through the list. Check if a window is used by any function. If not,
    check if any other window (used by window functions) is actually inheriting
    from this window. If not, remove this window definition.
  */
  List_iterator<Window> wi1(*windows);
  for (Window *w1 = wi1++; w1 != nullptr; w1 = wi1++) {
    if (w1->m_functions.is_empty()) {
      /*
        No window functions use this window, so check if other used window
        definitions inherit from this window.
      */
      bool window_used = false;
      for (const Window &w2 : *windows) {
        if (!w2.m_functions.is_empty()) {
          /*
            Go through the ancestor list and see if the current window
            definition is used by this window.
          */
          for (const Window *w_a = w2.m_ancestor; w_a != nullptr;
               w_a = w_a->m_ancestor) {
            // Can't inherit from unnamed window:
            assert(w_a->m_name != nullptr);

            if (my_strcasecmp(system_charset_info, w1->printable_name(),
                              w_a->printable_name()) == 0) {
              window_used = true;
              break;
            }
          }
        }
        if (window_used) break;
        // We check if partition by or order by of this window has subqueries.
        // If so, we cannot remove this window. Removing subqueries would need
        // removal of their entries in ref_item_array (added when setting up
        // order by/partition by fields in find_order_in_list).
        for (PT_order_list *it : {w1->m_partition_by, w1->m_order_by}) {
          if (it != nullptr) {
            for (ORDER *o = it->value.first; o != nullptr; o = o->next) {
              if ((*o->item)->has_subquery()) {
                window_used = true;
                break;
              }
            }
          }
          if (window_used) break;
        }
      }
      if (!window_used) {
        w1->cleanup();
        w1->destroy();
        wi1.remove();
      }
    }
  }
  // Eliminate redundant ordering after unused window definitions are removed.
  // Otherwise we risk removing order for a window based on ordering of an
  // unused window.
  if (!windows->is_empty()) {
    reorder_and_eliminate_sorts(windows);
    /* Do this last, after any re-ordering */
    (*windows)[windows->size() - 1]->m_last = true;
  }
}

bool Window::setup_windows1(THD *thd, Query_block *select,
                            Ref_item_array ref_item_array, Table_ref *tables,
                            mem_root_deque<Item *> *fields,
                            List<Window> *windows) {
  // Only possible at resolution time.
  assert(thd->lex->current_query_block()->first_execution);

  if (windows->elements > kMaxWindows) {
    my_error(ER_TOO_MANY_WINDOWS, MYF(0), windows->elements, kMaxWindows);
    return true;
  }

  /*
    We can encounter aggregate functions in the ORDER BY and PARTITION clauses
    of window function, so make sure we allow it:
  */
  const nesting_map save_allow_sum_func = thd->lex->allow_sum_func;
  thd->lex->allow_sum_func |= (nesting_map)1 << select->nest_level;

  for (Window &w : *windows) {
    w.m_query_block = select;

    if (w.m_partition_by != nullptr &&
        w.resolve_window_ordering(thd, ref_item_array, tables, fields,
                                  w.m_partition_by->value.first, true))
      return true;

    if (w.m_order_by != nullptr &&
        w.resolve_window_ordering(thd, ref_item_array, tables, fields,
                                  w.m_order_by->value.first, false))
      return true;
  }

  thd->lex->allow_sum_func = save_allow_sum_func;

  /* Our adjacency list uses std::unordered_set which may throw, so "try" */
  try {
    /*
      If window N depends on (references) window M for its definition,
      we add the relation n->m to the adjacency list, cf.
      w1->set_ancestor(w2) vs. adj.add(i, j) below.
    */
    AdjacencyList adj(windows->size());

    /* Resolve inter-window references */
    {
      uint i = 0;
      for (auto wi1 = windows->begin(); wi1 != windows->end(); ++wi1, ++i) {
        Window *w1 = &*wi1;
        if (w1->m_inherit_from != nullptr) {
          bool resolved = false;
          uint j = 0;
          for (auto wi2 = windows->begin(); wi2 != windows->end(); ++wi2, ++j) {
            Window *w2 = &*wi2;
            if (w2->m_name == nullptr) continue;
            String str;
            if (my_strcasecmp(system_charset_info,
                              w1->m_inherit_from->val_str(&str)->ptr(),
                              w2->printable_name()) == 0) {
              w1->set_ancestor(w2);
              resolved = true;
              adj.add(i, j);
              break;
            }
          }

          if (!resolved) {
            String str;
            my_error(ER_WINDOW_NO_SUCH_WINDOW, MYF(0),
                     w1->m_inherit_from->val_str(&str)->ptr());
            return true;
          }
        }
      }
    }

    if (adj.check_circularity()) {
      my_error(ER_WINDOW_CIRCULARITY_IN_WINDOW_GRAPH, MYF(0));
      return true;
    }

    /* We now know all references are resolved and they form a DAG */
    for (uint i = 0; i < windows->size(); i++) {
      if (adj.out_degree(i) != 0) {
        /* Only the root can specify partition. SR 10.c) */
        const Window *const non_root = (*windows)[i];

        if (non_root->m_partition_by != nullptr) {
          my_error(ER_WINDOW_NO_CHILD_PARTITIONING, MYF(0));
          return true;
        }
      }

      if (adj.in_degree(i) == 0) {
        /* All windows that nobody depend on (leaves in DAG tree). */
        const Window *const leaf = (*windows)[i];
        const Window *seen_orderer = nullptr;

        /* SR 10.d) No redefines of ORDER BY along inheritance path */
        for (const Window *w3 = leaf; w3 != nullptr; w3 = w3->m_ancestor) {
          if (w3->m_order_by != nullptr) {
            if (seen_orderer != nullptr) {
              my_error(ER_WINDOW_NO_REDEFINE_ORDER_BY, MYF(0),
                       seen_orderer->printable_name(), w3->printable_name());
              return true;
            } else {
              seen_orderer = w3;
            }
          }
        }
      } else {
        /*
          This window has at least one dependent SQL 2014 section
          7.15 <window clause> SR 10.e
        */
        const Window *const ancestor = (*windows)[i];
        if (!ancestor->m_frame->m_originally_absent) {
          my_error(ER_WINDOW_NO_INHERIT_FRAME, MYF(0),
                   ancestor->printable_name());
          return true;
        }
      }
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception("setup_windows1");
    return true;
    /* purecov: end */
  }

  for (Window &w : *windows) {
    const PT_frame *f = w.frame();
    const PT_order_list *o = w.effective_order_by();

    if (w.m_order_by == nullptr && o != nullptr &&
        w.m_frame->m_originally_absent) {
      /*
        Since we had an empty frame specification, but inherit an ORDER BY (we
        cannot inherit a frame specification), we need to adjust the a priori
        border type now that we know what we inherit (not known before binding
        above).
      */
      assert(w.m_frame->m_query_expression == WFU_RANGE);
      w.m_frame->m_to->m_border_type = WBT_CURRENT_ROW;
    }

    if (w.check_unique_name(*windows)) return true;

    if (w.setup_ordering_cached_items(thd, select, o, false)) return true;

    if (w.setup_ordering_cached_items(thd, select, w.effective_partition_by(),
                                      true))
      return true;

    if (w.check_window_functions1(thd, select)) return true;

    /*
      initialize the physical sorting order by merging the partition clause
      and the ordering clause of the window specification.
    */
    (void)w.sorting_order(thd, select->is_implicitly_grouped());

    /* For now, we do not support EXCLUDE */
    if (f->m_exclusion != nullptr) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "EXCLUDE");
      return true;
    }

    /* For now, we do not support GROUPS */
    if (f->m_query_expression == WFU_GROUPS) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "GROUPS");
      return true;
    }
    /*
      So we can determine if a row's value falls within range of current row
    */
    if (f->m_query_expression == WFU_RANGE && w.setup_range_expressions(thd))
      return true;

    if (w.check_border_sanity1(thd)) return true;
  }

  return false;
}

bool Window::check_window_functions2(THD *thd) {
  m_opt_nth_row.m_offsets.clear();
  m_opt_lead_lag.m_offsets.clear();
  m_opt_nth_row.m_offsets.init(thd->mem_root);
  m_opt_lead_lag.m_offsets.init(thd->mem_root);

  for (Item_sum &wf : m_functions) {
    Window_evaluation_requirements reqs;
    if (wf.check_wf_semantics2(&reqs)) return true;
    if (reqs.opt_nth_row.m_rowno > 0)
      m_opt_nth_row.m_offsets.push_back(reqs.opt_nth_row);
    /*
      INT_MIN64 can't be specified due 2's complement range.
      Offset is always given as a positive value; lead converted to negative
      but can't get to INT_MIN64. So, if we see this value, this window
      function isn't LEAD or LAG.
    */
    if (reqs.opt_ll_row.m_rowno != INT_MIN64)
      m_opt_lead_lag.m_offsets.push_back(reqs.opt_ll_row);
  }

  /*
    We do not allow FROM_LAST yet, so sorting guarantees sequential traversal
    of the frame buffer under evaluation of several NTH_VALUE functions invoked
    on a window, which is important for the optimized wf eval strategy
  */
  std::sort(m_opt_nth_row.m_offsets.begin(), m_opt_nth_row.m_offsets.end());
  std::sort(m_opt_lead_lag.m_offsets.begin(), m_opt_lead_lag.m_offsets.end());
  m_is_last_row_in_frame = !m_needs_frame_buffering;

  return false;
}

bool Window::setup_windows2(THD *thd, List<Window> *windows) {
  for (Window &w : *windows) {
    /*
      In execution of PS we need to check again in case ? parameters are used
      for window borders, or for offsets in window functions..
    */
    if (w.check_border_sanity2(thd) || w.check_window_functions2(thd))
      return true;
  }
  return false;
}

bool Window::make_special_rows_cache(THD *thd, TABLE *out_tbl) {
  // Each row may come either from frame buffer or out-table
  size_t l = std::max((needs_buffering() ? m_frame_buffer->s->reclength : 0),
                      out_tbl->s->reclength);
  if (m_special_rows_cache_max_length != 0) {
    // Could already be set up, if the query block is planned twice
    // (for in2exists).
    assert(m_special_rows_cache_max_length == l);
    return false;
  }
  m_special_rows_cache_max_length = l;
  return !(m_special_rows_cache =
               (uchar *)thd->alloc((FBC_FIRST_KEY - FBC_LAST_KEY + 1) * l));
}

void Window::cleanup() {
  if (m_needs_frame_buffering && m_frame_buffer != nullptr) {
    (void)m_frame_buffer->file->ha_index_or_rnd_end();
    close_tmp_table(m_frame_buffer);
    free_tmp_table(m_frame_buffer);
    ::destroy_at(m_frame_buffer_param);
  }

  m_frame_buffer_positions.clear();
  m_special_rows_cache_max_length = 0;

  m_frame_buffer_param = nullptr;
  m_frame_buffer = nullptr;
}

void Window::destroy()  // called only at stmt destruction
{
  for (Cached_item *ci : m_order_by_items) {
    ::destroy_at(ci);
  }
  for (Cached_item *ci : m_partition_items) {
    ::destroy_at(ci);
  }
  std::destroy_n(m_comparators[0].data(), m_comparators[0].size());
  std::destroy_n(m_comparators[1].data(), m_comparators[1].size());
}

void Window::reset_lead_lag() {
  for (Item_sum &f : m_functions) {
    if (f.sum_func() == Item_sum::LEAD_LAG_FUNC) {
      down_cast<Item_lead_lag &>(f).set_has_value(false);
      down_cast<Item_lead_lag &>(f).set_use_default(false);
    }
  }
}

void Window::reset_execution_state(Reset_level level) {
  switch (level) {
    case RL_ROUND:
      if (m_frame_buffer != nullptr) (void)m_frame_buffer->empty_result_table();
      m_frame_buffer_total_rows = 0;
      m_frame_buffer_partition_offset = 0;
      m_part_row_number = 0;
      [[fallthrough]];
    case RL_PARTITION:
      /*
        Forget positions in the frame buffer: they won't be valid in a new
        partition.
      */
      if (!m_frame_buffer_positions.empty()) {
        for (Frame_buffer_position &it : m_frame_buffer_positions) {
          it.m_rowno = -1;
        }
      }  // else not allocated, empty result set

      m_tmp_pos.m_rowno = -1;
      /*
        w.frame_buffer()->file->ha_reset();
        We could truncate the file here if it is not too expensive..? FIXME
      */
      break;
  }

  /*
    These state variables are always set per row processed, so no need to
    reset here:
        m_rowno_being_visited
        m_last_rowno_in_peerset
        m_is_last_row_in_peerset_within_frame
        m_partition_border
        m_inverse_aggregation
        m_rowno_in_frame
        m_rowno_in_partition
        m_do_copy_null
        m_is_last_row_in_frame

    But these need resetting for all levels
  */
  m_last_row_output = 0;
  m_last_rowno_in_cache = 0;
  m_aggregates_primed = false;
  m_first_rowno_in_range_frame = 1;
  m_last_rowno_in_range_frame = 0;
  m_first_rowno_in_rows_frame = 1;
  m_row_has_fields_in_out_table = 0;
}

void Window::print_border(const THD *thd, String *str, PT_border *border,
                          enum_query_type qt) const {
  const PT_border &b = *border;
  switch (b.m_border_type) {
    case WBT_CURRENT_ROW:
      str->append("CURRENT ROW");
      break;
    case WBT_VALUE_FOLLOWING:
    case WBT_VALUE_PRECEDING:

      if (b.m_date_time) {
        str->append("INTERVAL ");
        b.m_value->print(thd, str, qt);
        str->append(' ');
        str->append(interval_names[b.m_int_type]);
        str->append(' ');
      } else
        b.m_value->print(thd, str, qt);

      str->append(b.m_border_type == WBT_VALUE_PRECEDING ? " PRECEDING"
                                                         : " FOLLOWING");
      break;
    case WBT_UNBOUNDED_FOLLOWING:
      str->append("UNBOUNDED FOLLOWING");
      break;
    case WBT_UNBOUNDED_PRECEDING:
      str->append("UNBOUNDED PRECEDING");
      break;
  }
}

void Window::print_frame(const THD *thd, String *str,
                         enum_query_type qt) const {
  const PT_frame &f = *m_frame;
  str->append(f.m_query_expression == WFU_ROWS
                  ? "ROWS "
                  : (f.m_query_expression == WFU_RANGE ? "RANGE " : "GROUPS "));

  str->append("BETWEEN ");
  print_border(thd, str, f.m_from, qt);
  str->append(" AND ");
  print_border(thd, str, f.m_to, qt);
}

void Window::print(const THD *thd, String *str, enum_query_type qt,
                   bool expand_definition) const {
  if (m_name != nullptr && !expand_definition) {
    append_identifier(thd, str, m_name->item_name.ptr(),
                      m_name->item_name.length());
  } else {
    str->append('(');

    if (m_ancestor) {
      append_identifier(thd, str, m_ancestor->m_name->item_name.ptr(),
                        strlen(m_ancestor->m_name->item_name.ptr()));
      str->append(' ');
    }

    if (m_partition_by != nullptr) {
      str->append("PARTITION BY ");
      Query_block::print_order(thd, str, m_partition_by->value.first, qt);
      str->append(' ');
    }

    if (m_order_by != nullptr) {
      str->append("ORDER BY ");
      Query_block::print_order(thd, str, m_order_by->value.first, qt);
      str->append(' ');
    }

    if (!m_frame->m_originally_absent) {
      print_frame(thd, str, qt);
    }

    str->append(") ");
  }
}

const char *Window::printable_name() const {
  if (m_name == nullptr) return "<unnamed window>";
  // Since Item_string::val_str() ignores the argument, it is safe
  // to use nullptr as argument.
  return m_name->val_str(nullptr)->ptr();
}

void Window::reset_all_wf_state() {
  for (Item_sum &sum : m_functions) {
    for (bool framing : {false, true}) {
      (void)sum.walk(&Item::reset_wf_state, enum_walk::POSTFIX,
                     (uchar *)&framing);
    }
  }
}

bool Window::has_windowing_steps() const {
  return m_query_block != nullptr && m_query_block->join != nullptr &&
         m_query_block->join->m_windowing_steps;
}

double Window::compute_cost(double cost, const List<Window> &windows) {
  double total_cost = 0.0;
  for (const Window &w : windows)
    if (w.needs_sorting()) total_cost += cost;
  return total_cost;
}

void Window::apply_temp_table(THD *thd, const Func_ptr_array &items_to_copy,
                              bool first) {
  // Window::setup_ordering_cached_items() adds Item_ref wrappers around the
  // ordering and partitioning items. We need to see through them, so we unwrap
  // them here. Since they get removed on the first call to apply_temp_table(),
  // only unwrap on the first call.
  const auto unwrap = [first](Item *item) {
    return first ? down_cast<Item_ref *>(item)->ref_item() : item;
  };

  for (Mem_root_array<Cached_item *> *cached_items :
       {&m_partition_items, &m_order_by_items}) {
    for (Cached_item *&ci : *cached_items) {
      Item *item = FindReplacementOrReplaceMaterializedItems(
          thd, unwrap(ci->get_item()), items_to_copy,
          /*need_exact_match=*/true);
      thd->change_item_tree(ci->get_item_ptr(), item);
    }
  }

  // Item_rank looks directly into the ORDER *, so we need to update
  // that as well.
  if (m_order_by != nullptr) {
    ReplaceOrderItemsWithTempTableFields(thd, m_order_by->value.first,
                                         items_to_copy);
  }
  for (int i = 0; i < 2; ++i) {
    for (Arg_comparator &cmp : m_comparators[i]) {
      Item **left_ptr = cmp.get_left_ptr();
      Item *new_item = FindReplacementOrReplaceMaterializedItems(
          thd, unwrap(*left_ptr), items_to_copy,
          /*need_exact_match=*/true);
      thd->change_item_tree(left_ptr, new_item);

      Item_cache *cache = FindCacheInComparator(cmp);
      Item *new_cache_item = FindReplacementOrReplaceMaterializedItems(
          thd, unwrap(cache->get_example()), items_to_copy,
          /*need_exact_match=*/true);
      thd->change_item_tree(cache->get_example_ptr(), new_cache_item);
    }
  }
}
