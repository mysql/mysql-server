/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/build_interesting_orders.h"

#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <utility>

#include "ft_global.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "mysql/udf_registration_types.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_row.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/interesting_orders.h"
#include "sql/join_optimizer/interesting_orders_defs.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_resolver.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "sql/window.h"
#include "template_utils.h"

using hypergraph::NodeMap;
using std::string;
using std::swap;

/**
  Helper for CollectFunctionalDependenciesFromPredicates(); also used for
  non-equijoin predicates in CollectFunctionalDependenciesFromJoins().
 */
static int AddFunctionalDependencyFromCondition(THD *thd, Item *condition,
                                                bool always_active,
                                                LogicalOrderings *orderings) {
  if (condition->type() != Item::FUNC_ITEM) {
    return -1;
  }

  // We treat IS NULL as item = const.
  if (down_cast<Item_func *>(condition)->functype() == Item_func::ISNULL_FUNC) {
    Item_func_isnull *isnull = down_cast<Item_func_isnull *>(condition);

    FunctionalDependency fd;
    fd.type = FunctionalDependency::FD;
    fd.head = Bounds_checked_array<ItemHandle>();
    fd.tail = orderings->GetHandle(isnull->arguments()[0]);
    fd.always_active = always_active;

    return orderings->AddFunctionalDependency(thd, fd);
  }

  if (down_cast<Item_func *>(condition)->functype() != Item_func::EQ_FUNC) {
    // We only deal with equalities.
    // TODO(khatlen): Also collect functional dependencies from EQUAL_FUNC?
    return -1;
  }
  Item_func_eq *eq = down_cast<Item_func_eq *>(condition);
  Item *left = eq->arguments()[0];
  Item *right = eq->arguments()[1];
  if (left->const_for_execution()) {
    if (right->const_for_execution()) {
      // Ignore const = const.
      return -1;
    }
    swap(left, right);
  }
  if (equality_determines_uniqueness(eq, left, right)) {
    // item = const.
    FunctionalDependency fd;
    fd.type = FunctionalDependency::FD;
    fd.head = Bounds_checked_array<ItemHandle>();
    fd.tail = orderings->GetHandle(left);
    fd.always_active = always_active;

    return orderings->AddFunctionalDependency(thd, fd);
  } else if (!equality_has_no_implicit_casts(eq, left, right)) {
    // This is not a true equivalence; there is an implicit cast involved
    // that is potentially information-losing, so ordering by one will not
    // necessarily be the same as ordering by the other.
    // TODO(sgunders): Revisit this when we have explicit casts for
    // all comparisons, where we can generate potentially useful equivalences
    // involving the casts.
    return -1;
  } else {
    // item = item.
    FunctionalDependency fd;
    fd.type = FunctionalDependency::EQUIVALENCE;
    ItemHandle head = orderings->GetHandle(left);
    fd.head = Bounds_checked_array<ItemHandle>(&head, 1);
    fd.tail = orderings->GetHandle(right);
    fd.always_active = always_active;

    // Takes a copy if needed, so the stack reference is safe.
    return orderings->AddFunctionalDependency(thd, fd);
  }
}

/**
  Collect functional dependencies from joins. Currently, we apply
  item = item only, and only on inner joins and semijoins. Outer joins do not
  enforce their equivalences unconditionally (e.g. with an outer join on
  t1.a = t2.b, t1.a = t2.b does not hold afterwards; t2.b could be NULL).
  Semijoins do, and even though the attributes from the inner side are
  inaccessible afterwards, there could still be interesting constant FDs
  that are applicable to the outer side after equivalences.

  It is possible to generate a weaker form of FDs for outer joins,
  as described in sql/aggregate_check.h (and done for GROUP BY);
  e.g. from the join condition t1.x=t2.x AND t1.y=t2.y, one can infer a
  functional dependency {t1.x,t1.y} → t2.x and similar for t2.y.
  However, do note the comment about FD propagation in the calling function.
 */
static void CollectFunctionalDependenciesFromJoins(
    THD *thd, JoinHypergraph *graph, LogicalOrderings *orderings) {
  for (JoinPredicate &pred : graph->edges) {
    const RelationalExpression *expr = pred.expr;
    if (expr->type != RelationalExpression::INNER_JOIN &&
        expr->type != RelationalExpression::STRAIGHT_INNER_JOIN &&
        expr->type != RelationalExpression::SEMIJOIN) {
      continue;
    }
    pred.functional_dependencies_idx.init(thd->mem_root);
    pred.functional_dependencies_idx.reserve(expr->equijoin_conditions.size() +
                                             expr->join_conditions.size());
    for (Item_eq_base *join_condition : expr->equijoin_conditions) {
      int fd_idx = AddFunctionalDependencyFromCondition(
          thd, join_condition, /*always_active=*/false, orderings);
      if (fd_idx != -1) {
        pred.functional_dependencies_idx.push_back(fd_idx);
      }
    }
    for (Item *join_condition : expr->join_conditions) {
      int fd_idx = AddFunctionalDependencyFromCondition(
          thd, join_condition, /*always_active=*/false, orderings);
      if (fd_idx != -1) {
        pred.functional_dependencies_idx.push_back(fd_idx);
      }
    }
  }
}

/**
  Collect functional dependencies from non-join predicates.
  Again, we only do item = item, and more interesting; we only take the
  raw items, where we could have been much more sophisticated.
  Imagine a predicate like a = b + c; we will add a FD saying exactly
  that (which may or may not be useful, if b + c shows up in ORDER BY),
  but we should probably also have added {b,c} → a, if b and c could
  be generated somehow.

  However, we _do_ special-case item = const, since they are so useful;
  they become {} → item instead.
 */
static void CollectFunctionalDependenciesFromPredicates(
    THD *thd, JoinHypergraph *graph, LogicalOrderings *orderings) {
  for (size_t i = 0; i < graph->num_where_predicates; ++i) {
    Predicate &pred = graph->predicates[i];
    bool always_active =
        !Overlaps(pred.total_eligibility_set, PSEUDO_TABLE_BITS) &&
        IsSingleBitSet(pred.total_eligibility_set);
    int fd_idx = AddFunctionalDependencyFromCondition(thd, pred.condition,
                                                      always_active, orderings);
    if (fd_idx != -1) {
      pred.functional_dependencies_idx.push_back(fd_idx);
    }
  }
}

static void CollectFunctionalDependenciesFromUniqueIndexes(
    THD *thd, JoinHypergraph *graph, LogicalOrderings *orderings) {
  // Collect functional dependencies from unique indexes.
  for (JoinHypergraph::Node &node : graph->nodes) {
    TABLE *table = node.table;
    for (unsigned key_idx = 0; key_idx < table->s->keys; ++key_idx) {
      KEY *key = &table->key_info[key_idx];
      if (!Overlaps(actual_key_flags(key), HA_NOSAME)) {
        // Not a unique index.
        continue;
      }
      if (Overlaps(actual_key_flags(key), HA_NULL_PART_KEY)) {
        // Some part of the index could be NULL,
        // with special semantics; so ignore it.
        continue;
      }

      FunctionalDependency fd;
      fd.type = FunctionalDependency::FD;
      fd.head = Bounds_checked_array<ItemHandle>::Alloc(thd->mem_root,
                                                        actual_key_parts(key));
      for (unsigned keypart_idx = 0; keypart_idx < actual_key_parts(key);
           ++keypart_idx) {
        fd.head[keypart_idx] = orderings->GetHandle(
            new Item_field(key->key_part[keypart_idx].field));
      }
      fd.always_active = true;

      // Add a FD for each field in the table that is not part of the key.
      for (unsigned field_idx = 0; field_idx < table->s->fields; ++field_idx) {
        Field *field = table->field[field_idx];
        bool in_key = false;
        for (unsigned keypart_idx = 0; keypart_idx < actual_key_parts(key);
             ++keypart_idx) {
          if (field->eq(key->key_part[keypart_idx].field)) {
            in_key = true;
            break;
          }
        }
        if (!in_key) {
          fd.tail = orderings->GetHandle(new Item_field(field));
          orderings->AddFunctionalDependency(thd, fd);
        }
      }
    }
  }
}

static Ordering::Elements CollectInterestingOrder(THD *thd, ORDER *order,
                                                  int order_len,
                                                  bool unwrap_rollup,
                                                  LogicalOrderings *orderings) {
  Ordering::Elements elements =
      Ordering::Elements::Alloc(thd->mem_root, order_len);

  int i = 0;
  for (; order != nullptr; order = order->next, ++i) {
    Item *item = *order->item;
    if (unwrap_rollup) {
      item = unwrap_rollup_group(item);
    }
    elements[i].item = orderings->GetHandle(item);
    elements[i].direction = order->direction;
  }
  return elements;
}

// A convenience form of the above.
static Ordering::Elements CollectInterestingOrder(
    THD *thd, const SQL_I_List<ORDER> &order_list, bool unwrap_rollup,
    LogicalOrderings *orderings) {
  return CollectInterestingOrder(thd, order_list.first, order_list.size(),
                                 unwrap_rollup, orderings);
}

ORDER *BuildSortAheadOrdering(THD *thd, const LogicalOrderings *orderings,
                              Ordering ordering) {
  ORDER *order = nullptr;
  ORDER *last_order = nullptr;
  for (OrderElement element : ordering.GetElements()) {
    ORDER *new_ptr = new (thd->mem_root) ORDER;
    new_ptr->item_initial = orderings->item(element.item);
    new_ptr->item = &new_ptr->item_initial;
    new_ptr->direction = element.direction;

    if (order == nullptr) {
      order = new_ptr;
    }
    if (last_order != nullptr) {
      last_order->next = new_ptr;
    }
    last_order = new_ptr;
  }
  return order;
}

static int AddOrdering(THD *thd, Ordering ordering, bool used_at_end,
                       table_map homogenize_tables,
                       LogicalOrderings *orderings) {
  if (ordering.GetElements().empty()) {
    return 0;
  }

  return orderings->AddOrdering(thd, ordering, /*interesting=*/true,
                                used_at_end, homogenize_tables);
}

static void CanonicalizeGrouping(Ordering::Elements *elements) {
  for (OrderElement &elem : *elements) {
    elem.direction = ORDER_NOT_RELEVANT;
  }
  std::sort(elements->begin(), elements->end(),
            [](const OrderElement &a, const OrderElement &b) {
              return a.item < b.item;
            });
  elements->resize(std::unique(elements->begin(), elements->end()) -
                   elements->begin());
}

/**
  Find the ORDER objects pointing corresponding to a given OrderElement. That
  is, return the first ORDER that has the same item and direction as the given
  OrderElement. It is assumed that there is a corresponding one.
 */
static ORDER *FindOrderElementInORDER(OrderElement element, ORDER *order,
                                      const LogicalOrderings &orderings) {
  const Item *search_item = orderings.item(element.item);
  while (true) {
    assert(order != nullptr);
    if (*order->item == search_item && element.direction == order->direction) {
      return order;
    }
    order = order->next;
  }
}

/**
  Remove all redundant elements from a chain of ORDERs by modifying the next
  pointers in the intrusive list.

  @param order Pointer to the first element of the original ORDER BY clause.
  @param reduced_ordering An Ordering object that contains only the
         non-redundant elements of "order".
  @param orderings The logical orderings.

  @return Pointer to the first element of the reduced ordering.
 */
static ORDER *RemoveRedundantOrderElements(ORDER *order,
                                           Ordering reduced_ordering,
                                           const LogicalOrderings &orderings) {
  ORDER *first = nullptr;
  ORDER *prev = nullptr;
  ORDER *current = order;

  for (OrderElement element : reduced_ordering.GetElements()) {
    ORDER *next = FindOrderElementInORDER(element, current, orderings);
    assert(next != nullptr);
    if (first == nullptr) {
      first = next;
    } else {
      prev->next = next;
    }
    prev = next;
    current = next->next;
  }

  if (prev != nullptr) {
    prev->next = nullptr;
  }

  return first;
}

Ordering ReduceFinalOrdering(THD *thd, const LogicalOrderings &orderings,
                             int ordering_idx) {
  Ordering full_ordering = orderings.ordering(ordering_idx);
  return orderings.ReduceOrdering(
      full_ordering, /*all_fds=*/true,
      Ordering::Elements::Alloc(thd->mem_root, full_ordering.size()));
}

void BuildInterestingOrders(
    THD *thd, JoinHypergraph *graph, Query_block *query_block,
    LogicalOrderings *orderings,
    Mem_root_array<SortAheadOrdering> *sort_ahead_orderings,
    int *order_by_ordering_idx, int *group_by_ordering_idx,
    int *distinct_ordering_idx, Mem_root_array<ActiveIndexInfo> *active_indexes,
    Mem_root_array<FullTextIndexInfo> *fulltext_searches, string *trace) {
  // Collect ordering from ORDER BY.
  if (query_block->is_ordered()) {
    Ordering::Elements elements =
        CollectInterestingOrder(thd, query_block->order_list,
                                /*unwrap_rollup=*/false, orderings);

    *order_by_ordering_idx =
        AddOrdering(thd, Ordering(elements, Ordering::Kind::kOrder),
                    /*used_at_end=*/true, /*homogenize_tables=*/0, orderings);
  }

  // Collect grouping from GROUP BY.
  if (query_block->is_explicitly_grouped()) {
    Ordering::Elements elements =
        CollectInterestingOrder(thd, query_block->group_list,
                                /*unwrap_rollup=*/true, orderings);

    if (query_block->join->rollup_state == JOIN::RollupState::NONE) {
      CanonicalizeGrouping(&elements);
      *group_by_ordering_idx =
          AddOrdering(thd, Ordering(elements, Ordering::Kind::kGroup),
                      /*used_at_end=*/true, /*homogenize_tables=*/0, orderings);
    } else {
      for (OrderElement &elem : elements) {
        elem.direction = ORDER_NOT_RELEVANT;
      }
      *group_by_ordering_idx =
          AddOrdering(thd, Ordering(elements, Ordering::Kind::kRollup),
                      /*used_at_end=*/true, /*homogenize_tables=*/0, orderings);
    }
  }

  // Collect orderings/groupings from window functions.
  //
  // Note that window functions may contain hybrid groupings/orderings,
  // e.g. PARTITION BY a,b ORDER BY c,d. In this case, several orderings
  // (eight of them) would satisfy the query:
  //
  //   1. (a,b,c,d)
  //   2. (b,a,c,d)
  //   3. (a↓,b,c,d)
  //   4. (b↓,a↓,c,d)
  //   5. etc..
  //
  // However, since we don't support hybrid groupings/orderings,
  // just pure groupings or pure orderings, we only accept #1 here.
  // For PARTITION BY with no ORDER BY, we use a grouping as usual.
  for (Window &window : query_block->join->m_windows) {
    ORDER *order = window.sorting_order(thd);
    if (order == nullptr) {
      window.m_ordering_idx = 0;
      continue;
    }

    const bool mixed_grouping = (window.effective_order_by() != nullptr &&
                                 window.effective_partition_by() != nullptr);
    int order_len = 0;
    for (ORDER *ptr = order; ptr != nullptr; ptr = ptr->next) {
      if (mixed_grouping && ptr->direction == ORDER_NOT_RELEVANT) {
        ptr->direction = ORDER_ASC;
      }
      ++order_len;
    }

    Ordering::Elements elements =
        CollectInterestingOrder(thd, order, order_len,
                                /*unwrap_rollup=*/false, orderings);
    Ordering::Kind kind;
    if (window.effective_order_by() == nullptr) {
      CanonicalizeGrouping(&elements);
      kind = Ordering::Kind::kGroup;
    } else {
      kind = Ordering::Kind::kOrder;
    }
    window.m_ordering_idx =
        AddOrdering(thd, Ordering(elements, kind),
                    /*used_at_end=*/true, /*homogenize_tables=*/0, orderings);
  }

  // Collect grouping from DISTINCT.
  //
  // Note that we don't give in the ORDER BY ordering here, and thus also don't
  // care about all_order_by_fields_used (which says whether the DISTINCT
  // ordering was able to also satisfy the ORDER BY); group coverings will be
  // dealt with by the more general intesting order framework, which can also
  // combine e.g. GROUP BY groupings with ORDER BY.
  if (query_block->join->select_distinct) {
    bool all_order_fields_used = false;
    ORDER *order = create_order_from_distinct(
        thd, Ref_item_array(), /*order=*/nullptr, query_block->join->fields,
        /*skip_aggregates=*/false, /*convert_bit_fields_to_long=*/false,
        &all_order_fields_used);

    if (order == nullptr) {
      *distinct_ordering_idx = 0;  // 0 is the empty ordering.
    } else {
      int order_len = 0;
      for (ORDER *ptr = order; ptr != nullptr; ptr = ptr->next) {
        ++order_len;
      }

      Ordering::Elements elements =
          CollectInterestingOrder(thd, order, order_len,
                                  /*unwrap_rollup=*/false, orderings);

      CanonicalizeGrouping(&elements);
      *distinct_ordering_idx =
          AddOrdering(thd, Ordering(elements, Ordering::Kind::kGroup),
                      /*used_at_end=*/true, /*homogenize_tables=*/0, orderings);
    }
  }

  // Collect groupings from semijoins (because we might want to do duplicate
  // removal on the inner side, which will allow us to convert the join to an
  // inner join and invert it).
  for (JoinPredicate &pred : graph->edges) {
    if (pred.expr->type != RelationalExpression::SEMIJOIN) {
      continue;
    }
    if (!pred.expr->join_conditions.empty()) {
      // Most semijoins (e.g. from IN) are pure equijoins, but due to
      // outer references, there may also be non-equijoin conditions
      // involved. If so, we can no longer rewrite to a regular inner
      // join (at least not in the general case), so skip these.
      continue;
    }
    const table_map inner_tables = pred.expr->right->tables_in_subtree;
    Ordering::Elements elements = Ordering::Elements::Alloc(
        thd->mem_root, pred.expr->equijoin_conditions.size());

    bool contains_row_item = false;
    for (size_t i = 0; i < pred.expr->equijoin_conditions.size(); ++i) {
      Item *item = pred.expr->equijoin_conditions[i]->get_arg(1);
      if (!IsSubset(item->used_tables() & ~PSEUDO_TABLE_BITS, inner_tables)) {
        item = pred.expr->equijoin_conditions[i]->get_arg(0);
        assert(
            IsSubset(item->used_tables() & ~PSEUDO_TABLE_BITS, inner_tables));
      }
      if (item->result_type() == ROW_RESULT) {
        // In rare cases, the optimizer may set up semijoins where the
        // items themselves are ROW() items. RemoveDuplicatesIterator
        // isn't ready for ROW_RESULT type, so we unwrap the simple ones
        // and simply ignore semijoins over more complex row-type items.
        if (item->type() == Item::ROW_ITEM && item->cols() == 1) {
          item = down_cast<Item_row *>(item)->element_index(0);
        } else {
          contains_row_item = true;
          break;
        }
      }
      elements[i].item = orderings->GetHandle(item);
    }
    if (contains_row_item) {
      continue;
    }
    CanonicalizeGrouping(&elements);

    pred.ordering_idx_needed_for_semijoin_rewrite = AddOrdering(
        thd,
        Ordering(elements, elements.empty() ? Ordering::Kind::kEmpty
                                            : Ordering::Kind::kGroup),
        /*used_at_end=*/false, /*homogenize_tables=*/inner_tables, orderings);
  }

  // Collect list of all active indexes. We will be needing this for ref access
  // and full-text index search even if we don't have any interesting orders.
  for (unsigned node_idx = 0; node_idx < graph->nodes.size(); ++node_idx) {
    TABLE *table = graph->nodes[node_idx].table;
    for (unsigned key_idx = 0; key_idx < table->s->keys; ++key_idx) {
      // NOTE: visible_index claims to contain “visible and enabled” indexes,
      // but we still need to check keys_in_use to ignore disabled indexes.
      if (!table->keys_in_use_for_query.is_set(key_idx)) {
        continue;
      }
      ActiveIndexInfo index_info;
      index_info.table = table;
      index_info.key_idx = key_idx;
      active_indexes->push_back(index_info);
    }
  }

  // Collect list of full-text searches that can be satisfied by an active
  // full-text index.
  if (query_block->has_ft_funcs()) {
    for (const ActiveIndexInfo &index_info : *active_indexes) {
      const TABLE *table = index_info.table;
      const unsigned key_idx = index_info.key_idx;
      const KEY &key = table->key_info[key_idx];

      if (!Overlaps(key.flags, HA_FULLTEXT)) continue;

      for (Item_func_match &ftfunc : *query_block->ftfunc_list) {
        if (ftfunc.get_master() == &ftfunc &&
            ftfunc.table_ref->table == table && ftfunc.key == key_idx) {
          fulltext_searches->push_back(FullTextIndexInfo{&ftfunc, 0});
        }
      }
    }
  }

  // Early exit if we don't have any interesting orderings.
  if (orderings->num_orderings() <= 1) {
    if (trace != nullptr) {
      *trace +=
          "\nNo interesting orders found. Not collecting functional "
          "dependencies.\n\n";
    }
    orderings->Build(thd, trace);
    return;
  }

  // Collect orderings from indexes. Note that these are not interesting
  // in themselves, so they will be rapidly pruned away if they cannot lead
  // to an interesting order.
  for (ActiveIndexInfo &index_info : *active_indexes) {
    TABLE *table = index_info.table;
    KEY *key = &table->key_info[index_info.key_idx];

    // Find out how many usable keyparts there are. We have to stop
    // at the first that is partial (if any), or if the index is
    // nonorderable (e.g. a hash index), which we can seemingly only
    // query by keypart.
    int sortable_key_parts = 0;
    for (unsigned keypart_idx = 0; keypart_idx < actual_key_parts(key);
         ++keypart_idx, ++sortable_key_parts) {
      if (Overlaps(key->key_part[keypart_idx].key_part_flag, HA_PART_KEY_SEG) ||
          !Overlaps(
              table->file->index_flags(index_info.key_idx, keypart_idx, true),
              HA_READ_ORDER)) {
        break;
      }
    }

    if (sortable_key_parts == 0) {
      continue;
    }

    // First add the forward order.
    Ordering::Elements elements =
        Ordering::Elements::Alloc(thd->mem_root, sortable_key_parts);
    for (int keypart_idx = 0; keypart_idx < sortable_key_parts; ++keypart_idx) {
      const KEY_PART_INFO &key_part = key->key_part[keypart_idx];
      elements[keypart_idx].item =
          orderings->GetHandle(new Item_field(key_part.field));
      elements[keypart_idx].direction =
          Overlaps(key_part.key_part_flag, HA_REVERSE_SORT) ? ORDER_DESC
                                                            : ORDER_ASC;
    }
    index_info.forward_order = orderings->AddOrdering(
        thd, Ordering(elements, Ordering::Kind::kOrder), /*interesting=*/false,
        /*used_at_end=*/true, /*homogenize_tables=*/0);

    // And now the reverse, if the index allows it.
    if (Overlaps(table->file->index_flags(index_info.key_idx,
                                          sortable_key_parts - 1, true),
                 HA_READ_PREV)) {
      for (int keypart_idx = 0; keypart_idx < sortable_key_parts;
           ++keypart_idx) {
        if (elements[keypart_idx].direction == ORDER_ASC) {
          elements[keypart_idx].direction = ORDER_DESC;
        } else {
          elements[keypart_idx].direction = ORDER_ASC;
        }
      }
      index_info.reverse_order = orderings->AddOrdering(
          thd, Ordering(elements, Ordering::Kind::kOrder),
          /*interesting=*/false,
          /*used_at_end=*/true, /*homogenize_tables=*/0);

      // Reverse index range scans need to know whether they should use the
      // extended key parts (key parts from the primary key that are appended to
      // the keys in a secondary index). So we also keep the ordering for a
      // reverse scan that only uses the user-defined key parts.
      if (const int user_defined_key_parts = key->user_defined_key_parts;
          sortable_key_parts <= user_defined_key_parts) {
        index_info.reverse_order_without_extended_key_parts =
            index_info.reverse_order;
      } else {
        index_info.reverse_order_without_extended_key_parts =
            orderings->AddOrdering(
                thd,
                Ordering(elements.prefix(user_defined_key_parts),
                         Ordering::Kind::kOrder),
                /*interesting=*/false,
                /*used_at_end=*/true,
                /*homogenize_tables=*/0);
      }
    }
  }

  // Collect orderings from full-text indexes. Note that these are not
  // interesting in themselves, so they will be rapidly pruned away if they
  // cannot lead to an interesting order. Full-text indexes can only provide
  // results ordered descending on the result returned by MATCH ... AGAINST.
  for (FullTextIndexInfo &info : *fulltext_searches) {
    // MyISAM does not support ordering on queries in boolean mode.
    if (Overlaps(info.match->flags, FT_BOOL) &&
        !Overlaps(info.match->table_ref->table->file->ha_table_flags(),
                  HA_CAN_FULLTEXT_EXT)) {
      continue;
    }

    ItemHandle item = orderings->GetHandle(info.match);
    OrderElement order_element{item, ORDER_DESC};
    Ordering::Elements elements{&order_element, 1};
    info.order = orderings->AddOrdering(
        thd, Ordering(elements, Ordering::Kind::kOrder), /*interesting=*/false,
        /*used_at_end=*/true,
        /*homogenize_tables=*/0);
  }

  // Collect functional dependencies. Currently, there are many kinds
  // we don't do; see sql/aggregate_check.h. In particular, we don't
  // collect FDs from:
  //
  //  - Unique indexes that are nullable, but that are made non-nullable
  //    by WHERE predicates.
  //  - Generated columns. [*]
  //  - Join conditions from outer joins. [*]
  //  - Non-merged derived tables (including views and CTEs). [*]
  //
  // Note that the points marked with [*] introduce special problems related
  // to propagation of FDs; aggregate_check.h contains more details around
  // so-called “NULL-friendly functional dependencies”. If we include any
  // of them, we need to take more care about propagating them through joins.
  //
  // We liberally insert FDs here, even if they are not obviously related
  // to interesting orders; they may be useful at a later stage, when
  // other FDs can use them as a stepping stone. Optimization in Build()
  // will remove them if they are indeed not useful.
  CollectFunctionalDependenciesFromJoins(thd, graph, orderings);
  CollectFunctionalDependenciesFromPredicates(thd, graph, orderings);
  CollectFunctionalDependenciesFromUniqueIndexes(thd, graph, orderings);

  // Collect the GROUP BY expression, which will be used by
  // AddFDsFromAggregateItems() later.
  if (query_block->is_explicitly_grouped()) {
    auto head = Bounds_checked_array<ItemHandle>::Alloc(
        thd->mem_root, query_block->group_list.size());
    int idx = 0;
    for (ORDER *group = query_block->group_list.first; group != nullptr;
         group = group->next, ++idx) {
      head[idx] = orderings->GetHandle(*group->item);
    }
    orderings->SetHeadForAggregates(head);
  }
  orderings->SetRollup(query_block->join->rollup_state !=
                       JOIN::RollupState::NONE);

  orderings->Build(thd, trace);

  if (*order_by_ordering_idx != -1) {
    *order_by_ordering_idx =
        orderings->RemapOrderingIndex(*order_by_ordering_idx);

    // See if we're able to eliminate any redundant elements completely from the
    // ORDER BY clause. If so, store the reduced ordering in join->order.
    if (const Ordering reduced_ordering =
            ReduceFinalOrdering(thd, *orderings, *order_by_ordering_idx);
        reduced_ordering.size() < query_block->order_list.elements) {
      query_block->join->order = ORDER_with_src(
          RemoveRedundantOrderElements(query_block->join->order.order,
                                       reduced_ordering, *orderings),
          query_block->join->order.src);
    }
  }
  if (*group_by_ordering_idx != -1) {
    *group_by_ordering_idx =
        orderings->RemapOrderingIndex(*group_by_ordering_idx);
  }
  if (*distinct_ordering_idx != -1) {
    *distinct_ordering_idx =
        orderings->RemapOrderingIndex(*distinct_ordering_idx);
  }
  for (Window &window : query_block->join->m_windows) {
    if (window.m_ordering_idx != -1) {
      window.m_ordering_idx =
          orderings->RemapOrderingIndex(window.m_ordering_idx);
    }
  }

  for (JoinPredicate &pred : graph->edges) {
    for (int fd_idx : pred.functional_dependencies_idx) {
      pred.functional_dependencies |= orderings->GetFDSet(fd_idx);
    }
  }
  for (Predicate &pred : graph->predicates) {
    for (int fd_idx : pred.functional_dependencies_idx) {
      pred.functional_dependencies |= orderings->GetFDSet(fd_idx);
    }
  }

  for (JoinPredicate &pred : graph->edges) {
    if (pred.ordering_idx_needed_for_semijoin_rewrite != -1) {
      pred.ordering_idx_needed_for_semijoin_rewrite =
          orderings->RemapOrderingIndex(
              pred.ordering_idx_needed_for_semijoin_rewrite);

      // Set up the elements to deduplicate against. Note that we don't do this
      // before after Build(), because Build() may have simplified away some
      // (or all) elements using functional dependencies.
      Ordering::Elements grouping =
          orderings->ordering(pred.ordering_idx_needed_for_semijoin_rewrite)
              .GetElements();
      pred.semijoin_group_size = grouping.size();
      if (!grouping.empty()) {
        pred.semijoin_group =
            thd->mem_root->ArrayAlloc<Item *>(grouping.size());
        for (size_t i = 0; i < grouping.size(); ++i) {
          pred.semijoin_group[i] = orderings->item(grouping[i].item);
        }
      }
    }
  }

  for (FullTextIndexInfo &info : *fulltext_searches) {
    info.order = orderings->RemapOrderingIndex(info.order);
  }

  // Now collect all orderings we have that we can try as sort-ahead,
  // including both the orderings we originally added, group covers,
  // and homogenized orders.
  for (int ordering_idx = 0; ordering_idx < orderings->num_orderings();
       ++ordering_idx) {
    if (!orderings->ordering_is_relevant_for_sortahead(ordering_idx)) {
      continue;
    }

    table_map used_tables = 0;
    bool aggregates_required = false;
    bool sort_ahead_only = false;
    for (OrderElement element :
         orderings->ordering(ordering_idx).GetElements()) {
      Item *item = orderings->item(element.item);
      used_tables |= item->used_tables();
      aggregates_required |= (item->has_aggregation() || item->has_wf());
      const Item *real_item = item->real_item();
      sort_ahead_only =
          sort_ahead_only ||
          std::none_of(query_block->join->fields->cbegin(),
                       query_block->join->fields->cend(),
                       [real_item](const Item *field) {
                         return real_item->eq(field->real_item(),
                                              /*binary_cmp=*/true);
                       });
    }
    NodeMap required_nodes = GetNodeMapFromTableMap(
        used_tables & ~(INNER_TABLE_BIT | OUTER_REF_TABLE_BIT),
        graph->table_num_to_node_num);

    ORDER *order = BuildSortAheadOrdering(thd, orderings,
                                          orderings->ordering(ordering_idx));
    sort_ahead_orderings->push_back(
        SortAheadOrdering{ordering_idx, required_nodes, aggregates_required,
                          sort_ahead_only, order});
  }
}
