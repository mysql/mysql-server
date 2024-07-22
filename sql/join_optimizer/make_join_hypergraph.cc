/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/make_join_hypergraph.h"

#include <assert.h>
#include <sys/types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <numeric>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "prealloced_array.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/common_subexpression_elimination.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/join_optimizer/find_contained_subqueries.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/nested_join.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "sql/table_function.h"
#include "template_utils.h"

using hypergraph::Hyperedge;
using hypergraph::Hypergraph;
using hypergraph::IsSimpleEdge;
using hypergraph::NodeMap;
using std::array;
using std::has_single_bit;
using std::max;
using std::min;
using std::popcount;
using std::string;
using std::swap;
using std::vector;

namespace {

RelationalExpression *MakeRelationalExpressionFromJoinList(
    THD *thd, const Query_block *query_block,
    const mem_root_deque<Table_ref *> &join_list, bool toplevel = false);
bool EarlyNormalizeConditions(THD *thd, const RelationalExpression *join,
                              Mem_root_array<Item *> *conditions,
                              bool *always_false);

inline bool IsMultipleEquals(const Item *cond) {
  return cond->type() == Item::FUNC_ITEM &&
         down_cast<const Item_func *>(cond)->functype() ==
             Item_func::MULTI_EQ_FUNC;
}

Item_func_eq *MakeEqItem(Item *a, Item *b,
                         Item_multi_eq *source_multiple_equality) {
  Item_func_eq *eq_item = new Item_func_eq(a, b);
  eq_item->set_cmp_func();
  eq_item->update_used_tables();
  eq_item->quick_fix_field();
  eq_item->source_multiple_equality = source_multiple_equality;
  return eq_item;
}

/**
  Helper function for ReorderConditions(), which counts how many tables are
  referenced by an equijoin condition. This enables ReorderConditions() to sort
  the conditions on their complexity (referencing more tables == more complex).
  Multiple equalities are considered simple, referencing two tables, regardless
  of how many tables are actually referenced by them. This is because multiple
  equalities will be split into one or more single equalities later, referencing
  no more than two tables each.
 */
int CountTablesInEquiJoinCondition(const Item *cond) {
  assert(
      cond->type() == Item::FUNC_ITEM &&
      down_cast<const Item_func *>(cond)->contains_only_equi_join_condition());
  if (IsMultipleEquals(cond)) {
    // It's not a join condition if it has a constant argument.
    assert(down_cast<const Item_multi_eq *>(cond)->const_arg() == nullptr);
    return 2;
  } else {
    return popcount(cond->used_tables());
  }
}

/**
  Reorders the predicates in such a way that equalities are placed ahead
  of other types of predicates. These will be followed by predicates having
  subqueries and the expensive predicates at the end.
  This is used in the early stage of optimization. Predicates are not ordered
  based on their selectivity yet. The call to optimize_cond() would have put
  all the equalities at the end (because it tries to create multiple
  equalities out of them). It is always better to see the equalties ahead of
  other types of conditions when pushing join conditions down.
  E.g:
   (t1.f1 != t2.f1) and (t1.f2 = t3.f2 OR t4.f1 = t5.f3) and (3 = select #2) and
   (t1.f3 = t3.f3) and multi_equal(t1.f2,t2.f3,t3.f4)
  will be split in this order
   (t1.f3 = t3.f3) and
   multi_equal(t1.f2,t2.f3,t3.f4) and
   (t1.f1 != t2.f1) and
   (t1.f2 = t3.f2 OR t4.f1 = t5.f3) and
   (3 = select #2)

   Simple equijoin conditions (like t1.x=t2.x) are placed ahead of more complex
   ones (like t1.x=t2.x+t3.x), so that we prefer making simple edges and avoid
   hyperedges when we can.
*/
void ReorderConditions(Mem_root_array<Item *> *condition_parts) {
  // First equijoin conditions, followed by other conditions, then
  // subqueries (which can be expensive), then stored procedures
  // (which are unknown, so potentially _very_ expensive).
  const auto equi_cond_end = std::stable_partition(
      condition_parts->begin(), condition_parts->end(), [](const Item *item) {
        return item->type() == Item::FUNC_ITEM &&
               down_cast<const Item_func *>(item)
                   ->contains_only_equi_join_condition();
      });

  std::stable_sort(condition_parts->begin(), equi_cond_end,
                   [](const Item *a, const Item *b) {
                     return CountTablesInEquiJoinCondition(a) <
                            CountTablesInEquiJoinCondition(b);
                   });

  std::stable_partition(condition_parts->begin(), condition_parts->end(),
                        [](const Item *item) { return !item->has_subquery(); });

  std::stable_partition(
      condition_parts->begin(), condition_parts->end(),
      [](const Item *item) { return !item->cost().IsExpensive(); });
}

/**
  For a multiple equality, split out any conditions that refer to the
  same table, without touching the multi-equality; e.g. for equal(t1.a, t2.a,
  t2.b, t3.a), will return t2.a=t2.b AND (original item). This means that later
  stages can ignore such duplicates, and also that we can push these parts
  independently of the multiple equality as a whole.
 */
void ExpandSameTableFromMultipleEquals(Item_multi_eq *equal,
                                       table_map tables_in_subtree,
                                       List<Item> *eq_items) {
  // Look for pairs of items that touch the same table.
  for (auto it1 = equal->get_fields().begin(); it1 != equal->get_fields().end();
       ++it1) {
    if (!Overlaps(it1->used_tables(), tables_in_subtree)) {
      continue;
    }
    for (auto it2 = std::next(it1); it2 != equal->get_fields().end(); ++it2) {
      if (it1->field->table == it2->field->table) {
        eq_items->push_back(MakeEqItem(&*it1, &*it2, equal));

        // If there are more, i.e., *it2 = *it3, they will be dealt with
        // in a future iteration of the outer loop; so stop now to avoid
        // duplicates.
        break;
      }
    }
  }
}

/**
  Expand multiple equalities that can (and should) be expanded before join
  pushdown. These are the ones that touch at most two tables, or that
  are against a constant. They can be expanded unambiguously; no matter the join
  order, they will be the same. Fields on tables not in “tables_in_subtree” are
  assumed to be irrelevant to the equality and ignored (see the comment on
  PushDownCondition() for more details).

  For multi-equalities that are kept, split out any conditions that refer to the
  same table. See ExpandSameTableFromMultipleEquals().

  The return value is an AND conjunction, so most likely, it needs to be split.
 */
Item *EarlyExpandMultipleEquals(Item *condition, table_map tables_in_subtree) {
  return CompileItem(
      condition, [](Item *) { return true; },
      [tables_in_subtree](Item *item) -> Item * {
        if (!IsMultipleEquals(item)) {
          return item;
        }
        Item_multi_eq *equal = down_cast<Item_multi_eq *>(item);

        List<Item> eq_items;
        // If this condition is a constant, do the evaluation
        // and add a "false" condition if needed.
        // This cannot be skipped as optimize_cond() expects
        // the value stored in "m_always_false" to be checked for
        // Item_multi_eq before creating equalities from it.
        // We do not need to check for the const item evaluating
        // to be "true", as that could happen only when const table
        // optimization is used (It is currently not done for
        // hypergraph).
        if (equal->const_item() && !equal->val_int()) {
          eq_items.push_back(new Item_func_false);
        } else if (equal->const_arg() != nullptr) {
          // If there is a constant element, do a simple expansion.
          for (Item_field &field : equal->get_fields()) {
            if (IsSubset(field.used_tables(), tables_in_subtree)) {
              eq_items.push_back(MakeEqItem(&field, equal->const_arg(), equal));
            }
          }
        } else if (popcount(equal->used_tables() & tables_in_subtree) > 2) {
          // Only look at partial expansion.
          ExpandSameTableFromMultipleEquals(equal, tables_in_subtree,
                                            &eq_items);
          eq_items.push_back(equal);
        } else {
          // Prioritize expanding equalities from the same table if possible;
          // e.g., if we have t1.a = t2.a = t2.b, we want to have t2.a = t2.b
          // included (ie., not t1.a = t2.a AND t1.a = t2.b). The primary reason
          // for this is that such single-table equalities will be pushable
          // as table filters, and not left on the joins. This means we avoid an
          // issue where we have a hypergraph cycle where the edge we do not
          // follow (and thus ignore) has more join conditions than we skip,
          // causing us to wrongly “forget” constraining one degree of freedom.
          //
          // Thus, we first pick out every equality that touches only one table,
          // and then link one equality from each table into an arbitrary one.
          //
          // It's not given that this will always give us the fastest possible
          // plan; e.g. if there's a composite index on (t1.a, t1.b), it could
          // be faster to use it for lookups against (t2.a, t2.b) instead of
          // pushing t1.a = t1.b. But it doesn't seem worth it to try to keep
          // multiple such variations around.
          ExpandSameTableFromMultipleEquals(equal, tables_in_subtree,
                                            &eq_items);

          table_map included_tables = 0;
          Item_field *base_item = nullptr;
          for (Item_field &field : equal->get_fields()) {
            assert(has_single_bit(field.used_tables()));
            if (!IsSubset(field.used_tables(), tables_in_subtree) ||
                Overlaps(field.used_tables(), included_tables)) {
              continue;
            }
            included_tables |= field.used_tables();
            if (base_item == nullptr) {
              base_item = &field;
              continue;
            }

            eq_items.push_back(MakeEqItem(base_item, &field, equal));

            // Since we have at most two tables, we can have only one link.
            break;
          }
        }
        assert(!eq_items.is_empty());
        return CreateConjunction(&eq_items);
      });
}

RelationalExpression *MakeRelationalExpression(THD *thd,
                                               const Query_block *query_block,
                                               const Table_ref *tl) {
  if (tl == nullptr) {
    // No tables.
    return nullptr;
  } else if (tl->nested_join == nullptr) {
    // A single table.
    RelationalExpression *ret = new (thd->mem_root) RelationalExpression(thd);
    ret->type = RelationalExpression::TABLE;
    ret->table = tl;
    ret->tables_in_subtree = tl->map();
    return ret;
  } else {
    // A join or multijoin.
    return MakeRelationalExpressionFromJoinList(thd, query_block,
                                                tl->nested_join->m_tables);
  }
}

/**
  Convert the Query_block's join lists into a RelationalExpression,
  ie., a join tree with tables at the leaves. If join order hints are
  specified, use the join order specified in the join order hints.

  @param thd           Current thread
  @param query_block   Current query block
  @param join_list_arg List of tables in this join
  @param toplevel      False for subqueries, true otherwise

  @return              RelationalExpression for all tables in join
*/
RelationalExpression *MakeRelationalExpressionFromJoinList(
    THD *thd, const Query_block *query_block,
    const mem_root_deque<Table_ref *> &join_list_arg, bool toplevel) {
  assert(!join_list_arg.empty());
  bool join_order_hinted = false;
  const mem_root_deque<Table_ref *> *join_list = &join_list_arg;

  if (query_block->opt_hints_qb &&
      query_block->opt_hints_qb->has_join_order_hints()) {
    join_order_hinted = true;
    join_list = query_block->opt_hints_qb->sort_tables_in_join_order(
        thd, join_list_arg, toplevel);
  }

  RelationalExpression *ret = nullptr;
  for (auto it = join_list->rbegin(); it != join_list->rend();
       ++it) {  // The list goes backwards.
    const Table_ref *tl = *it;
    if (ret == nullptr) {
      // The first table in the list.
      ret = MakeRelationalExpression(thd, query_block, tl);
      continue;
    }

    RelationalExpression *join = new (thd->mem_root) RelationalExpression(thd);
    join->left = ret;
    if (tl->is_sj_or_aj_nest()) {
      join->right = MakeRelationalExpressionFromJoinList(
          thd, query_block, tl->nested_join->m_tables);
      join->type = tl->is_sj_nest() ? RelationalExpression::SEMIJOIN
                                    : RelationalExpression::ANTIJOIN;
      if (tl->is_sj_nest()) {
        join->enable_semijoin_strategies(tl);
      }
    } else {
      join->right = MakeRelationalExpression(thd, query_block, tl);
      if (tl->outer_join) {
        join->type = RelationalExpression::LEFT_JOIN;
      } else if (tl->straight || Overlaps(query_block->active_options(),
                                          SELECT_STRAIGHT_JOIN)) {
        join->type = RelationalExpression::STRAIGHT_INNER_JOIN;
      } else if (join_order_hinted &&
                 query_block->opt_hints_qb->check_join_order_hints(
                     join->left, join->right, join_list)) {
        join->type = RelationalExpression::STRAIGHT_INNER_JOIN;
      } else if (join_order_hinted &&
                 query_block->opt_hints_qb->check_join_order_hints(
                     join->right, join->left, join_list)) {
        std::swap(join->left, join->right);
        join->type = RelationalExpression::STRAIGHT_INNER_JOIN;
      } else {
        join->type = RelationalExpression::INNER_JOIN;
      }
    }
    join->tables_in_subtree =
        join->left->tables_in_subtree | join->right->tables_in_subtree;
    if (tl->is_aj_nest()) {
      assert(tl->join_cond_optim() != nullptr);
    }
    if (tl->join_cond_optim() != nullptr) {
      Item *join_cond = EarlyExpandMultipleEquals(tl->join_cond_optim(),
                                                  join->tables_in_subtree);
      ExtractConditions(join_cond, &join->join_conditions);
      bool always_false = false;
      EarlyNormalizeConditions(thd, join, &join->join_conditions,
                               &always_false);
      ReorderConditions(&join->join_conditions);
    }
    ret = join;
  }
  return ret;
}

/**
  Convert a multi-join into a simple inner join. expr must already have
  the correct companion set filled out.

  Only the top level will be converted, so there may still be a multi-join
  below the modified node, e.g.:

  MULTIJOIN(a, b) -> a JOIN b
  MULTIJOIN(a, b, c, ...) -> a JOIN MULTIJOIN(b, c, ...)

  If you want full unflattening, call UnflattenInnerJoins(), which calls this
  function recursively.
 */
void CreateInnerJoinFromChildList(
    Mem_root_array<RelationalExpression *> children,
    RelationalExpression *expr) {
  expr->type = RelationalExpression::INNER_JOIN;
  expr->tables_in_subtree = 0;
  expr->nodes_in_subtree = 0;
  for (RelationalExpression *child : children) {
    expr->tables_in_subtree |= child->tables_in_subtree;
    expr->nodes_in_subtree |= child->nodes_in_subtree;
  }

  if (children.size() == 2) {
    expr->left = children[0];
    expr->right = children[1];
  } else {
    // Split arbitrarily.
    expr->right = children.back();
    children.pop_back();

    RelationalExpression *left =
        new (current_thd->mem_root) RelationalExpression(current_thd);
    left->type = RelationalExpression::MULTI_INNER_JOIN;
    left->tables_in_subtree = 0;
    left->nodes_in_subtree = 0;
    left->companion_set = expr->companion_set;
    for (RelationalExpression *child : children) {
      left->tables_in_subtree |= child->tables_in_subtree;
      left->nodes_in_subtree |= child->nodes_in_subtree;
    }
    left->multi_children = std::move(children);
    expr->left = left;
  }
  expr->multi_children.clear();
}

/**
  Find all inner joins under “expr” without a join condition, and convert them
  to a flattened join (MULTI_INNER_JOIN). We do this even for the joins that
  have only two children, as it makes it easier to absorb them into higher
  multi-joins.

  The primary motivation for flattening is more flexible pushdown; when there is
  a large multi-way join, we can push pretty much any equality condition down
  to it, no matter how the join tree was written by the user.
  See PartiallyUnflattenJoinForCondition() for details.

  Note that this (currently) does not do any rewrites to flatten even more.
  E.g., for the tree (a JOIN (b LEFT JOIN c)), it would be beneficial to use
  associativity to rewrite into (a JOIN b) LEFT JOIN c (assuming a and b
  could be combined further with other joins). This also means that there may
  be items in the companion set that are not part of the same multi-join.
 */
void FlattenInnerJoins(RelationalExpression *expr) {
  if (expr->type == RelationalExpression::MULTI_INNER_JOIN) {
    // Already flattened, but grandchildren might need re-flattening.
    for (RelationalExpression *child : expr->multi_children) {
      FlattenInnerJoins(child);
      assert(child->type != RelationalExpression::MULTI_INNER_JOIN);
    }
    return;
  }
  if (expr->type != RelationalExpression::TABLE) {
    FlattenInnerJoins(expr->left);
    FlattenInnerJoins(expr->right);
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  if (expr->type == RelationalExpression::INNER_JOIN &&
      expr->join_conditions.empty()) {
    // Collect and flatten children.
    assert(expr->multi_children.empty());
    expr->type = RelationalExpression::MULTI_INNER_JOIN;
    if (expr->left->type == RelationalExpression::MULTI_INNER_JOIN) {
      for (RelationalExpression *child : expr->left->multi_children) {
        expr->multi_children.push_back(child);
      }
    } else {
      expr->multi_children.push_back(expr->left);
    }
    if (expr->right->type == RelationalExpression::MULTI_INNER_JOIN) {
      for (RelationalExpression *child : expr->right->multi_children) {
        expr->multi_children.push_back(child);
      }
    } else {
      expr->multi_children.push_back(expr->right);
    }
    expr->left = nullptr;
    expr->right = nullptr;
  }
}

/**
  The opposite of FlattenInnerJoins(); converts all flattened joins to
  a series of (right-deep) binary joins.
 */
void UnflattenInnerJoins(RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  if (expr->type == RelationalExpression::MULTI_INNER_JOIN) {
    // Peel off one table, then recurse. We could probably be
    // somewhat more efficient than this if it's important.
    CreateInnerJoinFromChildList(std::move(expr->multi_children), expr);
  }
  UnflattenInnerJoins(expr->left);
  UnflattenInnerJoins(expr->right);
}

/**
  For the given flattened join (multi-join), pull out (only) the parts we need
  to push the given condition, and make a binary join for it. For instance,
  if we have

    MULTIJOIN(t1, t2, t3, t4 LJ t5)

  and we have a condition t2.x = t5.x, we need to pull out the parts referring
  to t2 and t5, partially exploding the multi-join:

    MULTIJOIN(t1, t3, t2 JOIN (t4 LJ t5))

  The newly created child will be returned, and the condition can be pushed
  onto it. Note that there may be flattened joins under it; it is only the
  returned node itself that is guaranteed to be a binary join.

  If the condition touches all tables in the flattened join, the newly created
  binary node will completely replace the former. (The simplest case of this is
  a multi-join with only two nodes, and a condition referring to both of them.)
  For instance, given

    MULTIJOIN(t1, t2, t3)

  and a condition t1.x = t2.x + t3.x, the entire node will be replaced by

    t1 JOIN MULTIJOIN(t2, t3)

  on which it is possible to push the condition. Which node is pulled out to
  the left side is undefined.

  See also CreateInnerJoinFromChildList().
 */
RelationalExpression *PartiallyUnflattenJoinForCondition(
    table_map used_tables, RelationalExpression *expr) {
  Mem_root_array<RelationalExpression *> affected_children(
      current_thd->mem_root);
  for (RelationalExpression *child : expr->multi_children) {
    if (Overlaps(used_tables, child->tables_in_subtree) ||
        Overlaps(used_tables, RAND_TABLE_BIT)) {
      affected_children.push_back(child);
    }
  }
  assert(affected_children.size() > 1);

  if (affected_children.size() == expr->multi_children.size()) {
    // We need all of the nodes, so replace ourself entirely.
    CreateInnerJoinFromChildList(std::move(affected_children), expr);
    return expr;
  }

  RelationalExpression *new_expr =
      new (current_thd->mem_root) RelationalExpression(current_thd);
  new_expr->companion_set = expr->companion_set;
  CreateInnerJoinFromChildList(std::move(affected_children), new_expr);

  // Insert the new node as one of the children, and take out
  // the ones we've moved down into it.
  auto new_end =
      std::remove_if(expr->multi_children.begin(), expr->multi_children.end(),
                     [used_tables](const RelationalExpression *child) {
                       return Overlaps(used_tables, child->tables_in_subtree) ||
                              Overlaps(used_tables, RAND_TABLE_BIT);
                     });
  expr->multi_children.erase(new_end, expr->multi_children.end());
  expr->multi_children.push_back(new_expr);
  return new_expr;
}

string PrintRelationalExpression(RelationalExpression *expr, int level) {
  string result;
  for (int i = 0; i < level * 2; ++i) result += ' ';

  switch (expr->type) {
    case RelationalExpression::TABLE:
      if (expr->companion_set != nullptr) {
        result += StringPrintf("* %s [companion set %p]\n", expr->table->alias,
                               expr->companion_set);
      } else {
        result += StringPrintf("* %s\n", expr->table->alias);
      }
      // Do not try to descend further.
      return result;
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::MULTI_INNER_JOIN:
      result += "* Inner join";
      break;
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      result += "* Inner join [forced noncommutative]";
      break;
    case RelationalExpression::LEFT_JOIN:
      result += "* Left join";
      break;
    case RelationalExpression::SEMIJOIN:
      result += "* Semijoin";
      break;
    case RelationalExpression::ANTIJOIN:
      result += "* Antijoin";
      break;
    case RelationalExpression::FULL_OUTER_JOIN:
      result += "* Full outer join";
      break;
  }
  if (expr->companion_set != nullptr) {
    result += StringPrintf(" [companion set %p]", expr->companion_set);
  }
  if (expr->type == RelationalExpression::MULTI_INNER_JOIN) {
    // Should only exist before pushdown.
    assert(expr->equijoin_conditions.empty() && expr->join_conditions.empty());
    result += " (flattened)\n";
    for (RelationalExpression *child : expr->multi_children) {
      result += PrintRelationalExpression(child, level + 1);
    }
    return result;
  }
  if (!expr->equijoin_conditions.empty() && !expr->join_conditions.empty()) {
    result += StringPrintf(" (equijoin condition = %s, extra = %s)",
                           ItemsToString(expr->equijoin_conditions).c_str(),
                           ItemsToString(expr->join_conditions).c_str());
  } else if (!expr->equijoin_conditions.empty()) {
    result += StringPrintf(" (equijoin condition = %s)",
                           ItemsToString(expr->equijoin_conditions).c_str());
  } else if (!expr->join_conditions.empty()) {
    result += StringPrintf(" (extra join condition = %s)",
                           ItemsToString(expr->join_conditions).c_str());
  } else {
    result += " (no join conditions)";
  }
  result += '\n';

  result += PrintRelationalExpression(expr->left, level + 1);
  result += PrintRelationalExpression(expr->right, level + 1);
  return result;
}

// Returns whether the join condition for “expr” is null-rejecting (also known
// as strong or strict) on the given relations; that is, if it is guaranteed to
// return FALSE or NULL if _all_ tables in “tables” consist only of NULL values.
// (This means that adding tables in “tables” which are not part of any of the
// predicates is legal, and has no effect on the result.)
//
// A typical example of a null-rejecting condition would be a simple equality,
// e.g. t1.x = t2.x, which would reject NULLs on t1 and t2.
bool IsNullRejecting(const RelationalExpression &expr, table_map tables) {
  for (Item *cond : expr.join_conditions) {
    if (Overlaps(tables, cond->not_null_tables())) {
      return true;
    }
  }
  for (Item *cond : expr.equijoin_conditions) {
    if (Overlaps(tables, cond->not_null_tables())) {
      return true;
    }
  }
  return false;
}

bool IsInnerJoin(RelationalExpression::Type type) {
  return type == RelationalExpression::INNER_JOIN ||
         type == RelationalExpression::STRAIGHT_INNER_JOIN ||
         type == RelationalExpression::MULTI_INNER_JOIN;
}

// Returns true if (t1 <a> t2) <b> t3 === t1 <a> (t2 <b> t3).
//
// Note that this is not symmetric; e.g.
//
//   (t1 JOIN t2) LEFT JOIN t3 === t1 JOIN (t2 LEFT JOIN t3)
//
// but
//
//   (t1 LEFT JOIN t2) JOIN t3 != t1 LEFT JOIN (t2 JOIN t3)
//
// Note that this does not check that the rewrite would be _syntatically_ valid,
// i.e., that <b> does not refer to tables from t1. That is the job of the SES
// (syntactic eligibility set), which forms the base of the hyperedge
// representing the join, and not conflict rules -- if <b> refers to t1, the
// edge will include t1 no matter what we return here. This also goes for
// l-asscom and r-asscom below.
//
// When generating conflict rules, we call this function in a generalized sense:
//
//  1. t1, t2 and t3 could be join expressions, not just single tables.
//  2. <a> may not be a direct descendant of <b>, but further down the tree.
//  3. <b> may be below <a> in the tree, instead of the other way round.
//
// Due to #1 and #2, we need to take care when checking for null-rejecting
// conditions. Specifically, when the tables say we should check whether a
// condition mentioning (t2,t3) is null-rejecting on t2, we need to check the
// left arm of <b> instead of the right arm of <a>, as the condition might
// refer to a table that is not even part of <a> (ie., the “t2” in the condition
// is not the same “t2” as is under <a>). Otherwise, we might be rejecting
// valid plans. An example (where LJmn is LEFT JOIN with a null-rejecting
// predicate between tables m and n):
//
//   ((t1 LJ12 t2) LJ23 t3) LJ34 t4
//
// At some point, we will be called with <a> = LJ12 and <b> = LJ34.
// If we check whether LJ34 is null-rejecting on t2 (a.right), instead of
// checking wheher it is null-rejecting on {t1,t2,t3} (b.left), we will
// erroneously create a conflict rule {t2} → {t1}, since we believe the
// LJ34 predicate is not null-rejecting on its left side.
//
// A special note on semijoins not covered in [Moe13]: If the inner side
// is known to be free of duplicates on the key (e.g. because we removed
// them), semijoin is equivalent to inner join and is both commutative
// and associative. (We use this in the join optimizer.) However, we don't
// actually need to care about this here, because the way semijoin is
// defined, it is impossible to do an associate rewrite without there being
// degenerate join predicates, and we already accept missing some rewrites
// for them. Ie., for associativity to matter, one would need to have a
// rewrite like
//
//   (t1 SJ12 t2) J23 t3 === t1 SJ12 (t2 J23 t3)
//
// but there's no way we could have a condition J23 on the left side
// to begin with; semijoin in SQL comes from IN or EXISTS, which makes
// the attributes from t2 inaccessible after the join. Thus, J23 would
// have to be J3 (degenerate). The same argument explains why we don't
// need to worry about r-asscom, and semijoins are already l-asscom.
bool OperatorsAreAssociative(const RelationalExpression &a,
                             const RelationalExpression &b) {
  // Table 2 from [Moe13]; which operator pairs are associative.

  if ((a.type == RelationalExpression::LEFT_JOIN ||
       a.type == RelationalExpression::FULL_OUTER_JOIN) &&
      b.type == RelationalExpression::LEFT_JOIN) {
    // True if and only if the second join predicate rejects NULLs
    // on all tables in e2.
    return IsNullRejecting(b, b.left->tables_in_subtree);
  }

  if (a.type == RelationalExpression::FULL_OUTER_JOIN &&
      b.type == RelationalExpression::FULL_OUTER_JOIN) {
    // True if and only if both join predicates rejects NULLs
    // on all tables in e2.
    return IsNullRejecting(a, a.right->tables_in_subtree) &&
           IsNullRejecting(b, b.left->tables_in_subtree);
  }

  // Secondary engine does not want us to treat STRAIGHT_JOINs as
  // associative.
  if ((a.type == RelationalExpression::STRAIGHT_INNER_JOIN ||
       b.type == RelationalExpression::STRAIGHT_INNER_JOIN)) {
    return false;
  }

  // For the operations we support, it can be collapsed into this simple
  // condition. (Cartesian products and inner joins are treated the same.)
  return IsInnerJoin(a.type) && b.type != RelationalExpression::FULL_OUTER_JOIN;
}

// Returns true if (t1 <a> t2) <b> t3 === (t1 <b> t3) <a> t2,
// ie., the order of right-applying <a> and <b> don't matter.
//
// This is a symmetric property. The name comes from the fact that
// associativity and commutativity together would imply l-asscom;
// however, the converse is not true, so this is a more lenient property.
//
// See comments on OperatorsAreAssociative().
bool OperatorsAreLeftAsscom(const RelationalExpression &a,
                            const RelationalExpression &b) {
  // Associative and asscom implies commutativity, and since STRAIGHT_JOIN
  // is associative and we don't want it to be commutative, we can't make it
  // asscom. As an example, a user writing
  //
  //   (t1 STRAIGHT_JOIN t2) STRAIGHT_JOIN t3
  //
  // would never expect it to be rewritten to
  //
  //   (t1 STRAIGHT_JOIN t3) STRAIGHT_JOIN t2
  //
  // since that would effectively switch the order of t2 and t3.
  // It's possible we could be slightly more lenient here for some cases
  // (e.g. if t1/t2 were a regular inner join), but presumably, people
  // write STRAIGHT_JOIN to get _less_ leniency, so we just block them
  // off entirely.
  if (a.type == RelationalExpression::STRAIGHT_INNER_JOIN ||
      b.type == RelationalExpression::STRAIGHT_INNER_JOIN) {
    return false;
  }

  // Table 3 from [Moe13]; which operator pairs are l-asscom.
  // (Cartesian products and inner joins are treated the same.)
  if (a.type == RelationalExpression::LEFT_JOIN) {
    if (b.type == RelationalExpression::FULL_OUTER_JOIN) {
      return IsNullRejecting(a, a.left->tables_in_subtree);
    } else {
      return true;
    }
  }
  if (a.type == RelationalExpression::FULL_OUTER_JOIN) {
    if (b.type == RelationalExpression::LEFT_JOIN) {
      return IsNullRejecting(b, b.right->tables_in_subtree);
    }
    if (b.type == RelationalExpression::FULL_OUTER_JOIN) {
      return IsNullRejecting(a, a.left->tables_in_subtree) &&
             IsNullRejecting(b, b.left->tables_in_subtree);
    }
    return false;
  }
  return b.type != RelationalExpression::FULL_OUTER_JOIN;
}

// Returns true if e1 <a> (e2 <b> e3) === e2 <b> (e1 <a> e3),
// ie., the order of left-applying <a> and <b> don't matter.
// Similar to OperatorsAreLeftAsscom().
bool OperatorsAreRightAsscom(const RelationalExpression &a,
                             const RelationalExpression &b) {
  // Table 3 from [Moe13]; which operator pairs are r-asscom.
  // (Cartesian products and inner joins are treated the same.)
  if (a.type == RelationalExpression::FULL_OUTER_JOIN &&
      b.type == RelationalExpression::FULL_OUTER_JOIN) {
    return IsNullRejecting(a, a.right->tables_in_subtree) &&
           IsNullRejecting(b, b.right->tables_in_subtree);
  }

  // See OperatorsAreLeftAsscom() for why we don't accept STRAIGHT_INNER_JOIN.
  return a.type == RelationalExpression::INNER_JOIN &&
         b.type == RelationalExpression::INNER_JOIN;
}

enum class AssociativeRewritesAllowed { ANY, RIGHT_ONLY, LEFT_ONLY };

/**
  Find a bitmap of used tables for all conditions on \<expr\>.
  Note that after all conditions have been pushed, you can check
  expr.conditions_used_tables instead (see FindConditionsUsedTables()).

  NOTE: The map might be wider than expr.tables_in_subtree due to
  multiple equalities; you should normally just ignore those bits.
 */
table_map UsedTablesForCondition(const RelationalExpression &expr) {
  assert(expr.equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  table_map used_tables = 0;
  for (Item *cond : expr.join_conditions) {
    used_tables |= cond->used_tables();
  }
  return used_tables;
}

/**
  Like UsedTablesForCondition(), but multiple equalities set no bits unless
  they're certain, i.e., cannot be avoided no matter how we break up the
  multiple equality. This is the case for tables that are the only ones on
  their side of the join. E.g.: For a multiple equality {A,C,D} on a join
  (A,B) JOIN (C,D), A is certain; either A=C or A=D has to be included
  no matter what.
 */
table_map CertainlyUsedTablesForCondition(const RelationalExpression &expr) {
  assert(expr.equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  table_map used_tables = 0;
  for (Item *cond : expr.join_conditions) {
    table_map this_used_tables = cond->used_tables();
    if (IsMultipleEquals(cond)) {
      table_map left_bits = this_used_tables & GetVisibleTables(expr.left);
      table_map right_bits = this_used_tables & GetVisibleTables(expr.right);
      if (has_single_bit(left_bits)) {
        used_tables |= left_bits;
      }
      if (has_single_bit(right_bits)) {
        used_tables |= right_bits;
      }
    } else {
      used_tables |= this_used_tables;
    }
  }
  return used_tables;
}

/**
  Check whether we are allowed to make an extra join edge with the given
  condition, instead of pushing the condition onto the given point in the
  join tree (which we have presumably found out that we don't want).
 */
bool IsCandidateForCycle(RelationalExpression *expr, Item *cond,
                         const CompanionSetCollection &companion_collection) {
  if (cond->type() != Item::FUNC_ITEM) {
    return false;
  }
  if (Overlaps(cond->used_tables(), PSEUDO_TABLE_BITS)) {
    return false;
  }
  Item_func *func_item = down_cast<Item_func *>(cond);
  if (!IsMultipleEquals(func_item)) {
    // Don't try to make cycle edges out of hyperpredicates, at least for now;
    // simple equalities and multi-equalities only.
    if (!func_item->contains_only_equi_join_condition()) {
      return false;
    }
    if (popcount(cond->used_tables()) != 2) {
      return false;
    }
  }

  // Check that we are not combining together anything that is not part of
  // the same companion set (either by means of the condition, or by making
  // a cycle through an already-existing condition).
  table_map used_tables = cond->used_tables();
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  for (Item *other_cond : expr->join_conditions) {
    used_tables |= other_cond->used_tables();
  }
  return companion_collection.Find(used_tables & expr->tables_in_subtree) !=
         nullptr;
}

bool ComesFromMultipleEquality(Item *item, Item_multi_eq *equal) {
  return is_function_of_type(item, Item_func::EQ_FUNC) &&
         down_cast<Item_func_eq *>(item)->source_multiple_equality == equal;
}

int FindSourceMultipleEquality(Item *item,
                               const Mem_root_array<Item_multi_eq *> &equals) {
  if (!is_function_of_type(item, Item_func::EQ_FUNC)) {
    return -1;
  }
  Item_func_eq *eq = down_cast<Item_func_eq *>(item);
  for (size_t equals_idx = 0; equals_idx < equals.size(); ++equals_idx) {
    if (eq->source_multiple_equality == equals[equals_idx]) {
      return static_cast<int>(equals_idx);
    }
  }
  return -1;
}

bool MultipleEqualityAlreadyExistsOnJoin(Item_multi_eq *equal,
                                         const RelationalExpression &expr) {
  // Could be called both before and after MakeHashJoinConditions(),
  // so check for join_conditions and equijoin_conditions.
  for (Item *item : expr.join_conditions) {
    if (ComesFromMultipleEquality(item, equal)) {
      return true;
    }
  }
  for (Item_eq_base *item : expr.equijoin_conditions) {
    if (item->source_multiple_equality == equal) {
      return true;
    }
  }
  return false;
}

bool AlreadyExistsOnJoin(Item *cond, const RelationalExpression &expr) {
  assert(expr.equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  for (Item *item : expr.join_conditions) {
    if (cond->eq(item)) {
      return true;
    }
  }

  // If "cond" is an equality created from a multiple equality, it might already
  // be present on the join in a slightly different shape, because it can be a
  // bit arbitrary exactly which single equalities a multiple equality is
  // expanded to.
  //
  // For example, a=b and b=a should be considered the same. Also, if we have a
  // multiple equality t1.x=t2.x=t2.y, we should consider t1.x=t2.x as present
  // on the join if t1.x=t2.y is already there. We can do this because we know
  // the t2.x=t2.y predicate will be pushed down as a table predicate (see
  // EarlyExpandMultipleEquals() and ExpandSameTableFromMultipleEquals()), and
  // t1.x=t2.x is implied by t1.x=t2.y and t2.x=t2.y.
  //
  // Similarly, if we have a hyperedge {t1,t2,t3}-{t4} and we already have
  // t1.x=t4.x, we shouldn't add t2.x=t4.x if it comes from the same multiple
  // equality, as in this case we know t1.x=t2.x will already have been applied
  // on the {t1,t2,t3} subplan, and t2.x=t4.x is therefore implied by t1.x=t4.x.
  //
  // This means we only need to check if the join condition already has another
  // equality that comes from the same multiple equality.
  if (is_function_of_type(cond, Item_func::EQ_FUNC)) {
    if (Item_multi_eq *equal =
            down_cast<Item_func_eq *>(cond)->source_multiple_equality;
        equal != nullptr && MultipleEqualityAlreadyExistsOnJoin(equal, expr)) {
      return true;
    }
  }

  return false;
}

/**
  Returns whether adding “cond” to the given join would unduly enlarge
  the number of tables it references, or create a degenerate join.
  The former is suboptimal since it would create a wider hyperedge
  than is usually needed, ie., it restricts join ordering.
  Consider for instance a join such as

    a JOIN (b JOIN c ON TRUE) ON a.x=b.x WHERE a.y=c.y

  If pushing the WHERE condition down on the a/bc join, that join would
  get a dependency on both b and c, hindering (ab) and (ac) as subplans.
  This function allows us to detect this and look for other opportunities
  (see AddJoinCondition()).
 */
bool IsBadJoinForCondition(const RelationalExpression &expr, Item *cond) {
  const table_map used_tables = cond->used_tables();

  // Making a degenerate join is rarely good.
  if (!Overlaps(used_tables, expr.left->tables_in_subtree) ||
      !Overlaps(used_tables, expr.right->tables_in_subtree)) {
    return true;
  }

  const table_map already_used_tables = CertainlyUsedTablesForCondition(expr);
  if (already_used_tables == 0) {
    // Making a Cartesian join into a proper join is good.
    return false;
  }

  if (IsMultipleEquals(cond)) {
    // Don't apply the same multi-equality twice on the same join. This fixes an
    // issue that goes roughly like this:
    //
    // 1. A multi-equality, e.g. (t1.x, t2.x, t3.x), is pushed on the lower
    //    level of a join like t1 JOIN (t2 JOIN (t3 JOIN t4)), and concretized
    //    to t2.x = t3.x (we happen to push the lower levels before the higher
    //    levels).
    // 2. Now we want to push the same multi-equality on the higher level,
    //    but assume there's already a condition there that makes it a bad join
    //    for us, e.g. t1.y = t4.y already exists. This causes us to try an
    //    associative rewrite to (t1 JOIN t2) JOIN (t3 JOIN t4). Note that
    //    the top join still carries the t2.x = t3.x condition.
    // 3. Now we see that we can reliably push the multi-equality onto the
    //    top join again without extending the join condition -- by concretizing
    //    it to t2.x = t3.x!
    //
    // This obviously subverts the requirement that we have (N-1) different
    // concretizations of the multi-equality, since two are the same. Thus,
    // we have this explicit check here.
    //
    // See the unit test MultipleEqualityIsNotPushedMultipleTimes for an example
    // that goes horribly wrong without this.
    if (MultipleEqualityAlreadyExistsOnJoin(down_cast<Item_multi_eq *>(cond),
                                            expr)) {
      return true;
    }

    // For multi-equalities, we can pick any table from the left and any table
    // from the right, so see if we can make any such choice that doesn't
    // broaden the condition.
    const table_map candidate_tables = used_tables & already_used_tables;
    if (Overlaps(candidate_tables, expr.left->tables_in_subtree) &&
        Overlaps(candidate_tables, expr.right->tables_in_subtree)) {
      return false;
    }
  }

  return !IsSubset(used_tables, already_used_tables);
}

/**
  Applies the following rewrite on \<op\>:

    A \<op\> (B \<op2\> C) => (A \<op\> B) \<op2\> C

  Importantly, the pointer \<op\> still points to the new top node
  (that is, \<op2\>), so you don't need to rewrite any nodes higher
  up in the tree. Join conditions and types are left as-is,
  ie., if \<op2\> is a LEFT JOIN, it will remain one.

  Does not check that the transformation is actually legal.
 */
void RotateRight(RelationalExpression *op) {
  RelationalExpression *op2 = op->right;
  RelationalExpression *b = op2->left;
  RelationalExpression *c = op2->right;

  op->right = b;
  op2->left = op;
  op2->right = c;

  // Update tables_in_subtree; order matters.
  op->tables_in_subtree =
      op->left->tables_in_subtree | op->right->tables_in_subtree;
  op2->tables_in_subtree =
      op2->left->tables_in_subtree | op2->right->tables_in_subtree;

  swap(*op, *op2);
  op->left = op2;
}

/**
  Opposite of RotateRight; that is:

    (A \<op2\> B) \<op\> C => A \<op2\> (B \<op\> C)

  See RotateRight for details.
 */
void RotateLeft(RelationalExpression *op) {
  RelationalExpression *op2 = op->left;
  RelationalExpression *a = op2->left;
  RelationalExpression *b = op2->right;

  op->left = b;
  op2->left = a;
  op2->right = op;

  // Update tables_in_subtree; order matters.
  op->tables_in_subtree =
      op->left->tables_in_subtree | op->right->tables_in_subtree;
  op2->tables_in_subtree =
      op2->left->tables_in_subtree | op2->right->tables_in_subtree;

  swap(*op, *op2);
  op->right = op2;
}

/**
  From “cond”, create exactly one simple equality that will connect the
  left and right sides of “expr”. E.g. for joining (A,B) and (C,D),
  and given the multi-equality (A.x,B.x,D.x), it may pick A.x = D.x
  or B.x = D.x (but never A.x = B.x).
 */
Item_func_eq *ConcretizeMultipleEquals(Item_multi_eq *cond,
                                       const RelationalExpression &expr) {
  const table_map already_used_tables = CertainlyUsedTablesForCondition(expr);

  Item_field *left = nullptr;
  Item_field *right = nullptr;

  // Go through and pick a candidate for each side of the equality.
  // This is fairly arbitrary (we will add cycles later), but if there is
  // already a condition present, we prefer to pick one that refers to an
  // already-used table.
  // Try to find a candidate from visible tables for this join.
  // It is correct indeed and also that HeatWave does not support
  // seeing inner tables of a semijoin from outside the semijoin.
  for (Item_field &item_field : cond->get_fields()) {
    if (Overlaps(item_field.used_tables(), GetVisibleTables(expr.left))) {
      if (left == nullptr ||
          !Overlaps(left->used_tables(), already_used_tables)) {
        left = &item_field;
      }
    } else if (Overlaps(item_field.used_tables(),
                        GetVisibleTables(expr.right))) {
      if (right == nullptr ||
          !Overlaps(right->used_tables(), already_used_tables)) {
        right = &item_field;
      }
    }
  }
  // If a candidate was not found from the visible tables, try with
  // all tables in the join. For certain cases, query transformations
  // could have placed a semijoin condition outside of the semijoin
  // or even as part of a WHERE condition. It might succeed here for
  // such conditions. Such queries are not offloaded to HeatWave.
  if (left == nullptr || right == nullptr) {
    for (Item_field &item_field : cond->get_fields()) {
      if (Overlaps(item_field.used_tables(), expr.left->tables_in_subtree)) {
        if (left == nullptr ||
            !Overlaps(left->used_tables(), already_used_tables)) {
          left = &item_field;
        }
      } else if (Overlaps(item_field.used_tables(),
                          expr.right->tables_in_subtree)) {
        if (right == nullptr ||
            !Overlaps(right->used_tables(), already_used_tables)) {
          right = &item_field;
        }
      }
    }
  }
  assert(left != nullptr);
  assert(right != nullptr);

  return MakeEqItem(left, right, cond);
}

/**
  From “cond”, create exactly as many simple equalities that are needed
  to connect all tables in “allowed_tables”. E.g. for joining (A,B) and (C,D)
  (ie., allowed_tables={A,B,C,D}), and given the multi-equality
  (A.x, B.x, D.x, E.x), it will generate A.x = B.x and B.x = D.x
  (E.x is ignored).

  The given container must support push_back(Item_func_eq *).
 */
template <class T>
static void FullyConcretizeMultipleEquals(Item_multi_eq *cond,
                                          table_map allowed_tables, T *result) {
  Item_field *last_field = nullptr;
  table_map seen_tables = 0;
  for (Item_field &field : cond->get_fields()) {
    if (!Overlaps(field.used_tables(), allowed_tables)) {
      // From outside this join.
      continue;
    }
    if (Overlaps(field.used_tables(), seen_tables)) {
      // We've already seen something from this table,
      // which has been dealt with in ExpandSameTableFromMultipleEquals().
      continue;
    }
    if (last_field != nullptr) {
      result->push_back(MakeEqItem(last_field, &field, cond));
    }
    last_field = &field;
    seen_tables |= field.used_tables();
  }
}

/**
  Finalize a condition (join condition or WHERE predicate); resolve any
  remaining multiple equalities.
  Caches around constant arguments are not added here but during finalize,
  since we might plan two times, and the caches from the first time may confuse
  remove_eq_cond() in the second.
 */
Item *CanonicalizeCondition(Item *condition, table_map visible_tables,
                            table_map all_tables) {
  // Convert any remaining (unpushed) multiple equals to a series of equijoins.
  // Note this is a last-ditch resort, and should almost never happen;
  // thus, it's fine just to fully expand the multi-equality, even though it
  // might mean adding conditions that have already been dealt with further down
  // the tree. This is also the only place that we expand multi-equalities
  // within OR conjunctions or the likes.
  condition = CompileItem(
      condition, [](Item *) { return true; },
      [visible_tables, all_tables](Item *item) -> Item * {
        if (!IsMultipleEquals(item)) {
          return item;
        }
        Item_multi_eq *equal = down_cast<Item_multi_eq *>(item);
        assert(equal->const_arg() == nullptr);
        List<Item> eq_items;
        FullyConcretizeMultipleEquals(equal, visible_tables, &eq_items);
        if (eq_items.is_empty()) {
          // It is possible that for some semijoin conditions, we might
          // not find replacements in only visible tables. So we try again
          // with all tables which includes the non-visible tables as well.
          FullyConcretizeMultipleEquals(equal, all_tables, &eq_items);
        }
        assert(!eq_items.is_empty());
        return CreateConjunction(&eq_items);
      });

  // Account for tables not in allowed_tables having been removed.
  condition->update_used_tables();
  return condition;
}

// Split any conditions that have been transformed into a conjunction (typically
// by expansion of multiple equalities or removal of constant subconditions).
Mem_root_array<Item *> ResplitConditions(
    THD *thd, const Mem_root_array<Item *> &conditions) {
  Mem_root_array<Item *> new_conditions(thd->mem_root);
  for (Item *condition : conditions) {
    ExtractConditions(condition, &new_conditions);
  }
  return new_conditions;
}

// Calls CanonicalizeCondition() for each condition in the given array.
bool CanonicalizeConditions(THD *thd, table_map visible_tables,
                            table_map all_tables,
                            Mem_root_array<Item *> *conditions) {
  bool need_resplit = false;
  for (Item *&condition : *conditions) {
    condition = CanonicalizeCondition(condition, visible_tables, all_tables);
    if (condition == nullptr) {
      return true;
    }
    if (IsAnd(condition)) {
      // Canonicalization converted something (probably an Item_multi_eq) to a
      // conjunction, which we need to split back to new conditions again.
      need_resplit = true;
    }
  }
  if (need_resplit) {
    *conditions = ResplitConditions(thd, *conditions);
  }
  return false;
}

/**
  Add “cond” as a join condition to “expr”, but if it would enlarge the set
  of referenced tables, try to rewrite the join tree using associativity
  (either left or right) to be able to put the condition on a more favorable
  node. (See IsBadJoinForCondition().)

    a JOIN (b JOIN c ON TRUE) ON a.x=b.x WHERE a.y=c.y

  In this case, we'd try rewriting the join tree into

    (a JOIN b ON a.x=b.x) JOIN c ON TRUE WHERE a.y=c.y

  which would then allow the push with no issues:

    (a JOIN b ON a.x=b.x) JOIN c ON a.y=c.y

  Note that with flattening, we don't need this for inner joins (flattening
  solves all inner-join cases without needing this machinery), so this is only
  ever called when outer joins are involved (inner joins are used in the example
  above for ease of exposition).

  This function works recursively, and returns true if the condition
  was pushed.
 */
bool AddJoinConditionPossiblyWithRewrite(THD *thd, RelationalExpression *expr,
                                         Item *cond,
                                         AssociativeRewritesAllowed allowed,
                                         bool used_commutativity,
                                         bool *need_flatten) {
  // We should never reach this from a top-level caller, and due to the call
  // to UnflattenInnerJoins() below, we should also never see it through
  // rotates.
  assert(expr->type != RelationalExpression::MULTI_INNER_JOIN);

  // We can only promote filters to join conditions on inner joins and
  // semijoins, but having a left join doesn't stop us from doing the rewrites
  // below. Due to special semijoin rules in MySQL (see comments in
  // PushDownCondition()), we also disallow making join conditions on semijoins.
  if (!IsBadJoinForCondition(*expr, cond) && IsInnerJoin(expr->type)) {
    if (IsMultipleEquals(cond)) {
      cond = ConcretizeMultipleEquals(down_cast<Item_multi_eq *>(cond), *expr);
    }

    expr->join_conditions.push_back(cond);
    if (TraceStarted(thd) && allowed != AssociativeRewritesAllowed::ANY) {
      Trace(thd) << StringPrintf(
          "- applied associativity%s to better push condition %s\n",
          used_commutativity ? " and commutativity" : "",
          ItemToString(cond).c_str());
    }
    return true;
  }

  // Flattening in itself causes some headaches (it's not obvious how to do
  // rotates), so before any such rewrites, we unflatten the tree. This isn't
  // particularly efficient, and it may also cause us to miss some rewrites,
  // but it's an OK tradeoff. The top-level caller will have to flatten again.
  //
  // NOTE: When/if we support rotating through flattened joins, we can
  // drop all the commutativity code.
  UnflattenInnerJoins(expr);
  *need_flatten = true;

  // Try (where ABC are arbitrary expressions, and <op1> is expr):
  //
  //   A <op1> (B <op2> C) => (A <op1> B) <op2> C
  //
  // and see if we can push upon <op2>, possibly doing the same
  // rewrite repeatedly if it helps.
  if (allowed != AssociativeRewritesAllowed::LEFT_ONLY &&
      expr->right->type != RelationalExpression::TABLE &&
      OperatorsAreAssociative(*expr, *expr->right)) {
    // Note that we need to use the conservative check here
    // (UsedTablesForCondition() instead of CertainlyUsedTablesForCondition()),
    // in order not to do possibly illegal rewrites. (It should only matter
    // for the rare case where we have unpushed multiple equalities.)
    if (!Overlaps(UsedTablesForCondition(*expr),
                  expr->right->right->tables_in_subtree)) {
      RotateRight(expr);
      if (AddJoinConditionPossiblyWithRewrite(
              thd, expr, cond, AssociativeRewritesAllowed::RIGHT_ONLY,
              used_commutativity, need_flatten)) {
        return true;
      }
      // It failed, so undo what we did.
      RotateLeft(expr);
    }
    if (OperatorIsCommutative(*expr->right) &&
        !Overlaps(UsedTablesForCondition(*expr),
                  expr->right->left->tables_in_subtree)) {
      swap(expr->right->left, expr->right->right);
      RotateRight(expr);
      if (AddJoinConditionPossiblyWithRewrite(
              thd, expr, cond, AssociativeRewritesAllowed::RIGHT_ONLY,
              /*used_commutativity=*/false, need_flatten)) {
        return true;
      }
      // It failed, so undo what we did.
      RotateLeft(expr);
      swap(expr->right->left, expr->right->right);
    }
  }

  // Similarly, try:
  //
  //   (A <op2> B) <op1> C => A <op2> (B <op1> C)
  //
  // and see if we can push upon <op2>.
  if (allowed != AssociativeRewritesAllowed::RIGHT_ONLY &&
      expr->left->type != RelationalExpression::TABLE &&
      OperatorsAreAssociative(*expr->left, *expr)) {
    if (!Overlaps(UsedTablesForCondition(*expr),
                  expr->left->left->tables_in_subtree)) {
      RotateLeft(expr);
      if (AddJoinConditionPossiblyWithRewrite(
              thd, expr, cond, AssociativeRewritesAllowed::LEFT_ONLY,
              used_commutativity, need_flatten)) {
        return true;
      }
      // It failed, so undo what we did.
      RotateRight(expr);
    }
    if (OperatorIsCommutative(*expr->left) &&
        !Overlaps(UsedTablesForCondition(*expr),
                  expr->left->right->tables_in_subtree)) {
      swap(expr->left->left, expr->left->right);
      RotateLeft(expr);
      if (AddJoinConditionPossiblyWithRewrite(
              thd, expr, cond, AssociativeRewritesAllowed::LEFT_ONLY,
              /*used_commutativity=*/true, need_flatten)) {
        return true;
      }
      // It failed, so undo what we did.
      RotateRight(expr);
      swap(expr->left->left, expr->left->right);
    }
  }

  return false;
}

/**
  Try to push down the condition “cond” down in the join tree given by “expr”,
  as far as possible. cond is either a join condition on expr
  (is_join_condition_for_expr=true), or a filter which is applied at some point
  after expr (...=false).

  If the condition was not pushable, ie., it couldn't be stored as a join
  condition on some lower place than it started, it will push it onto
  “remaining_parts”. remaining_parts can be nullptr, in which case the condition
  is simply dropped.

  Since PushDownAsMuchAsPossible() only calls us for join conditions, there is
  only one way we can push down something onto a single table (which naturally
  has no concept of “join condition”), and it does not affect the return
  condition. That is partial pushdown:

  In addition to regular pushdown, PushDownCondition() will do partial pushdown
  if appropriate. Some expressions cannot be fully pushed down, but we can
  push down necessary-but-not-sufficient conditions to get earlier filtering.
  (This is a performance win for e.g. hash join and the left side of a
  nested loop join, but not for the right side of a nested loop join. Note that
  we currently do not compensate for the errors in selectivity estimation
  this may incur.) An example would be

    (t1.x = 1 AND t2.y=2) OR (t1.x = 3 AND t2.y=4);

  we could push down the conditions (t1.x = 1 OR t1.x = 3) to t1 and similarly
  for t2, but we could not delete the original condition. If we get all the way
  down to a table, we store the condition in “table_filters”. These are
  conditions that can be evaluated directly on the given table, without any
  concern for what is joined in before (ie., TES = SES).


  Multiple equalities
  ===================

  Pushing down multiple equalities is somewhat tricky. To recap, a multiple
  equality (Item_multi_eq) is a set of N fields (a,b,c,...) that are all assumed
  to be equal to each other. As part of pushdown, we concretize these into
  (N-1) regular equalities (where every field is referred to at least once);
  this is enough for query correctness, and the remaining options will be added
  to the query graph later. E.g., if we have multiple equals (a,b,c), we could
  add a=b AND b=c, or equivalently a=c AND b=c. But for (a,b,c,d), we couldn't
  do with a=b AND a=c AND b=c; even though it would be (N-1) equalities,
  d still needs to be in the mix.

  We solve this by pushing down multiple equalities as usual down the tree until
  it becomes a join condition at the current node (ie., it refers to tables from
  both sides). At that point, we can pick an arbitrary table from each sides to
  create an equality. E.g. for (a,b,c,d) pushed onto (a,b) JOIN (c,d), we can
  choose an equality a=c, or a=d, or similar. However, this only resolves one
  equality; we need to keep pushing it down on both sides. This will create the
  (N-1) ones we want in the end. But at this point, the multiple equality will
  refer to tables not part of the join; e.g. trying to push down equals(a,b,c,d)
  onto a JOIN b. If so, we simply ignore the fields belonging to tables not part
  of the join, so we create a=b (the only possibility).

  If we at some point end up with a multiple equality we cannot push
  (e.g., because it hit an outer join), we will resolve it at the latest
  in CanonicalizeCondition().
 */
void PushDownCondition(THD *thd, Item *cond, RelationalExpression *expr,
                       bool is_join_condition_for_expr,
                       const CompanionSetCollection &companion_collection,
                       Mem_root_array<Item *> *table_filters,
                       Mem_root_array<Item *> *cycle_inducing_edges,
                       Mem_root_array<Item *> *remaining_parts) {
  if (expr->type == RelationalExpression::TABLE) {
    assert(!IsMultipleEquals(cond));
    table_filters->push_back(cond);
    return;
  }

  // Conditions are usually not attempted pushed down if they reference tables
  // outside of the subtree, so check that here. We do however allow multiple
  // equalities, as they are easily concretized into single equalities
  // referencing only the subtree. We also allow non-deterministic predicates to
  // be attempted pushed, but only in order to check if a partial pushdown of
  // non-deterministic parts is possible. The non-determinstic predicate itself
  // will be kept in the WHERE clause, even if parts of it is pushed down.
  assert(IsMultipleEquals(cond) ||
         IsSubset(cond->used_tables() & ~PSEUDO_TABLE_BITS,
                  expr->tables_in_subtree));
  const table_map used_tables =
      cond->used_tables() & (expr->tables_in_subtree | RAND_TABLE_BIT);

  if (expr->type == RelationalExpression::MULTI_INNER_JOIN) {
    // See if we can push this condition down to a single child.
    for (RelationalExpression *child : expr->multi_children) {
      if (IsSubset(used_tables, child->tables_in_subtree)) {
        PushDownCondition(thd, cond, child,
                          /*is_join_condition_for_expr=*/false,
                          companion_collection, table_filters,
                          cycle_inducing_edges, remaining_parts);
        return;
      }
    }

    // We couldn't, so we'll need to unflatten the join (either partially
    // or completely) to get a place where we can store the condition.
    expr = PartiallyUnflattenJoinForCondition(used_tables, expr);

    // Fall through, presumably storing the condition as a join condition
    // on the given node.
  }

  assert(
      !Overlaps(expr->left->tables_in_subtree, expr->right->tables_in_subtree));

  // See if we can push down into the left side, ie., it only touches
  // tables on the left side of the join.
  //
  // If the condition is a filter, we can do this for all join types
  // except FULL OUTER JOIN, which we don't support yet. If it's a join
  // condition for this join, we cannot push it for outer joins and
  // antijoins, since that would remove rows that should otherwise
  // be output (as NULL-complemented ones in the case if outer joins).
  const bool can_push_into_left =
      (IsInnerJoin(expr->type) ||
       expr->type == RelationalExpression::SEMIJOIN ||
       !is_join_condition_for_expr);
  if (IsSubset(used_tables, expr->left->tables_in_subtree)) {
    if (!can_push_into_left) {
      if (remaining_parts != nullptr) {
        remaining_parts->push_back(cond);
      }
      return;
    }
    PushDownCondition(thd, cond, expr->left,
                      /*is_join_condition_for_expr=*/false,
                      companion_collection, table_filters, cycle_inducing_edges,
                      remaining_parts);
    return;
  }

  // See if we can push down into the right side. For inner joins,
  // we can always do this, assuming the condition refers to the right
  // side only. For outer joins and antijoins, we cannot push conditions
  // _through_ them; that is, we can push them if they come directly from said
  // node's join condition, but not otherwise. (This is, incidentally, the exact
  // opposite condition from pushing into the left side.)
  //
  // Normally, this also goes for semijoins, except that MySQL's semijoin
  // rewriting causes conditions to appear higher up in the tree that we
  // _must_ push back down and through them for correctness. Thus, we have
  // no choice but to just trust that these conditions are pushable.
  // (The user cannot specify semijoins directly, so all such conditions
  // come from ourselves.)
  const bool can_push_into_right =
      (IsInnerJoin(expr->type) ||
       expr->type == RelationalExpression::SEMIJOIN ||
       is_join_condition_for_expr);
  if (IsSubset(used_tables, expr->right->tables_in_subtree)) {
    if (!can_push_into_right) {
      if (remaining_parts != nullptr) {
        remaining_parts->push_back(cond);
      }
      return;
    }
    PushDownCondition(thd, cond, expr->right,
                      /*is_join_condition_for_expr=*/false,
                      companion_collection, table_filters, cycle_inducing_edges,
                      remaining_parts);
    return;
  }

  // It's not a subset of left, it's not a subset of right, so it's a
  // filter that must either stay after this join, or it can be promoted
  // to a join condition for it.

  if (AlreadyExistsOnJoin(cond, *expr) &&
      !(expr->type == RelationalExpression::LEFT_JOIN ||
        expr->type == RelationalExpression::ANTIJOIN)) {
    // Redundant, so we can just forget about it.
    // (WHERE conditions are not pushable to outer joins or antijoins,
    // and thus not redundant, because post-join filters are not equivalent to
    // join conditions for those types. For outer joins, NULL-complemented rows
    // would need re-filtering, and for antijoins, the antijoin condition
    // repeated as a filter afterwards would simply return zero rows,
    // by definition.)
    return;
  }

  // Try partial pushdown into the left side (see function comment).
  if (can_push_into_left &&
      Overlaps(used_tables, expr->left->tables_in_subtree)) {
    Item *partial_cond = make_cond_for_table(
        current_thd, cond, expr->left->tables_in_subtree, /*used_table=*/0,
        /*exclude_expensive_cond=*/true);
    if (partial_cond != nullptr) {
      PushDownCondition(thd, partial_cond, expr->left,
                        /*is_join_condition_for_expr=*/false,
                        companion_collection, table_filters,
                        cycle_inducing_edges,
                        /*remaining_parts=*/nullptr);
    }
  }

  // Then the right side, if it's allowed.
  if (can_push_into_right &&
      Overlaps(used_tables, expr->right->tables_in_subtree)) {
    Item *partial_cond = make_cond_for_table(
        current_thd, cond, expr->right->tables_in_subtree, /*used_table=*/0,
        /*exclude_expensive_cond=*/true);
    if (partial_cond != nullptr) {
      PushDownCondition(thd, partial_cond, expr->right,
                        /*is_join_condition_for_expr=*/false,
                        companion_collection, table_filters,
                        cycle_inducing_edges,
                        /*remaining_parts=*/nullptr);
    }
  }

  // For multiple equalities, if there are multiple referred-to tables on one
  // side, then we must keep pushing down; there are still equalities left to
  // resolve. E.g. if we have equal(t1.x, t2.x, t3.x) and have (t1,t2) on the
  // left side and t3 on the right, we would pick e.g. t1.x=t3.x for this join,
  // but need to keep pushing down on the left side to get the t1.x=t2.x
  // condition further down.
  //
  // We can ignore the special case of a multi-equality referring to several
  // fields in the same table, as ExpandSameTableFromMultipleEquals()
  // has dealt with those for us.
  if (IsMultipleEquals(cond)) {
    table_map left_tables = cond->used_tables() & expr->left->tables_in_subtree;
    table_map right_tables =
        cond->used_tables() & expr->right->tables_in_subtree;
    if (popcount(left_tables) >= 2 && can_push_into_left) {
      PushDownCondition(thd, cond, expr->left,
                        /*is_join_condition_for_expr=*/false,
                        companion_collection, table_filters,
                        cycle_inducing_edges, remaining_parts);
    }
    if (popcount(right_tables) >= 2 && can_push_into_right) {
      PushDownCondition(thd, cond, expr->right,
                        /*is_join_condition_for_expr=*/false,
                        companion_collection, table_filters,
                        cycle_inducing_edges, remaining_parts);
    }
  }

  // Now that any partial pushdown has been done, see if we can promote
  // the original filter to a join condition.
  if (is_join_condition_for_expr) {
    // We were already a join condition on this join, so there's nothing to do.
    // (We leave any multiple equalities for LateConcretizeMultipleEqualities();
    // see comments there. We should also not push them further, unlike WHERE
    // conditions that induce inner joins.)
    if (remaining_parts != nullptr) {
      remaining_parts->push_back(cond);
    }
    return;
  }

  // We cannot promote filters to join conditions for outer joins
  // and antijoins, but we can on inner joins and semijoins.
  if (expr->type == RelationalExpression::LEFT_JOIN ||
      expr->type == RelationalExpression::ANTIJOIN) {
    // See if we can promote it by rewriting; if not, it has to be left
    // as a filter.
    bool need_flatten = false;
    if (!AddJoinConditionPossiblyWithRewrite(
            thd, expr, cond, AssociativeRewritesAllowed::ANY,
            /*used_commutativity=*/false, &need_flatten)) {
      if (remaining_parts != nullptr) {
        remaining_parts->push_back(cond);
      }
    }
    if (need_flatten) {
      FlattenInnerJoins(expr);
    }
    return;
  }

  // Promote the filter to a join condition on this join.
  // If it's an equijoin condition, MakeHashJoinConditions() will convert it to
  // one (in expr->equijoin_conditions) when it runs later.
  assert(expr->equijoin_conditions.empty());

  if (expr->type == RelationalExpression::SEMIJOIN) {
    // Special semijoin handling; the “WHERE conditions” from semijoins
    // are not really WHERE conditions, and must not be handled as such
    // (they cannot be moved to being conditions on inner joins).
    // See the comment about pushability of these above.
    // (Any multiple equalities should be simplified in
    // LateConcretizeMultipleEqualities(), but not pushed further,
    // unlike WHERE conditions that induce inner joins.)
    expr->join_conditions.push_back(cond);
    return;
  }

  bool need_flatten = false;
  if (!AddJoinConditionPossiblyWithRewrite(
          thd, expr, cond, AssociativeRewritesAllowed::ANY,
          /*used_commutativity=*/false, &need_flatten)) {
    if (expr->type == RelationalExpression::INNER_JOIN &&
        IsCandidateForCycle(expr, cond, companion_collection)) {
      // We couldn't push the condition to this join without broadening its
      // hyperedge, but we could add a simple edge (or multiple simple edges,
      // in the case of multiple equalities -- we defer the meshing of those
      // to later) to create a cycle, so we'll take it out now and then add such
      // an edge in AddCycleEdges().
      if (IsMultipleEquals(cond)) {
        // Some of these may induce cycles and some may not.
        // We need to split and push them separately.
        if (TraceStarted(thd)) {
          Trace(thd) << StringPrintf(
              "- condition %s may induce hypergraph cycles, splitting\n",
              ItemToString(cond).c_str());
        }
        Mem_root_array<Item *> possible_cycle_edges(current_thd->mem_root);
        FullyConcretizeMultipleEquals(down_cast<Item_multi_eq *>(cond),
                                      expr->tables_in_subtree,
                                      &possible_cycle_edges);
        for (Item *sub_cond : possible_cycle_edges) {
          PushDownCondition(thd, sub_cond, expr,
                            /*is_join_condition_for_expr=*/false,
                            companion_collection, table_filters,
                            cycle_inducing_edges, remaining_parts);
        }
      } else {
        if (TraceStarted(thd)) {
          Trace(thd) << StringPrintf(
              "- condition %s induces a hypergraph cycle\n",
              ItemToString(cond).c_str());
        }
        cycle_inducing_edges->push_back(CanonicalizeCondition(
            cond, expr->tables_in_subtree, expr->tables_in_subtree));
      }
      if (need_flatten) {
        FlattenInnerJoins(expr);
      }
      return;
    }
    if (TraceStarted(thd)) {
      Trace(thd) << StringPrintf(
          "- condition %s makes join reference more relations, "
          "but could not do anything about it\n",
          ItemToString(cond).c_str());
    }

    if (IsMultipleEquals(cond) &&
        !MultipleEqualityAlreadyExistsOnJoin(down_cast<Item_multi_eq *>(cond),
                                             *expr)) {
      expr->join_conditions.push_back(
          ConcretizeMultipleEquals(down_cast<Item_multi_eq *>(cond), *expr));
    } else if (IsSubset(used_tables, expr->tables_in_subtree)) {
      expr->join_conditions.push_back(cond);
    } else {
      if (remaining_parts != nullptr) {
        remaining_parts->push_back(cond);
      }
    }
  }
  if (need_flatten) {
    FlattenInnerJoins(expr);
  }
}

/**
  Try to push down conditions (like PushDownCondition()), but with the intent
  of pushing join conditions down to sargable conditions on tables.

  Equijoin conditions can often be pushed down into indexes; e.g. t1.x = t2.x
  could be pushed down into an index on t1.x. When we have pushed such a
  condition all the way down onto the t1/t2 join, we are ostensibly done
  with regular push (in PushDownCondition()), but here, we would push down the
  condition onto both sides if possible. (E.g.: If the join was a left join, we
  could push it down to t2, but not to t1.) When we hit a table in such a push,
  we store the conditions in “m_pushable_conditions“ for the table
  to signal that it should be investigated when we consider the table during
  join optimization.
 */
void PushDownToSargableCondition(Item *cond, RelationalExpression *expr,
                                 bool is_join_condition_for_expr) {
  if (expr->type == RelationalExpression::TABLE) {
    // We don't try to make sargable join predicates out of subqueries;
    // it is quite marginal, and our machinery for dealing with materializing
    // subqueries is not ready for it.
    if (cond->has_subquery()) {
      return;
    }
    if (!IsSubset(cond->used_tables() & ~PSEUDO_TABLE_BITS,
                  expr->tables_in_subtree)) {
      expr->AddPushable(cond);
    }
    return;
  }

  assert(
      !Overlaps(expr->left->tables_in_subtree, expr->right->tables_in_subtree));

  const table_map used_tables =
      cond->used_tables() & (expr->tables_in_subtree | RAND_TABLE_BIT);

  // See PushDownCondition() for explanation of can_push_into_{left,right}.
  const bool can_push_into_left =
      (IsInnerJoin(expr->type) ||
       expr->type == RelationalExpression::SEMIJOIN ||
       !is_join_condition_for_expr);
  const bool can_push_into_right =
      (IsInnerJoin(expr->type) ||
       expr->type == RelationalExpression::SEMIJOIN ||
       is_join_condition_for_expr);

  if (can_push_into_left &&
      !IsSubset(used_tables, expr->right->tables_in_subtree)) {
    PushDownToSargableCondition(cond, expr->left,
                                /*is_join_condition_for_expr=*/false);
  }
  if (can_push_into_right &&
      !IsSubset(used_tables, expr->left->tables_in_subtree)) {
    PushDownToSargableCondition(cond, expr->right,
                                /*is_join_condition_for_expr=*/false);
  }
}

/**
  Push down as many of the conditions in “conditions” as we can, into the join
  tree under “expr”. The parts that could not be pushed are returned.

  The conditions are nominally taken to be from higher up the tree than “expr”
  (e.g., WHERE conditions, or join conditions from a higher join), unless
  is_join_condition_for_expr is true, in which case they are taken to be
  posted as join conditions posted on “expr” itself. This causes them to be
  returned as remaining if “expr” is indeed their final lowest place
  in the tree (otherwise, they might get lost).
 */
Mem_root_array<Item *> PushDownAsMuchAsPossible(
    THD *thd, Mem_root_array<Item *> conditions, RelationalExpression *expr,
    bool is_join_condition_for_expr,
    const CompanionSetCollection &companion_collection,
    Mem_root_array<Item *> *table_filters,
    Mem_root_array<Item *> *cycle_inducing_edges) {
  Mem_root_array<Item *> remaining_parts(thd->mem_root);
  for (Item *item : conditions) {
    if (popcount(item->used_tables() & ~PSEUDO_TABLE_BITS) < 2 &&
        !is_join_condition_for_expr) {
      // Simple filters will stay in WHERE; we go through them with
      // AddPredicate() (in MakeJoinHypergraph()) and convert them into
      // table filters, then handle them separately in FoundSingleNode()
      // and FoundSubgraphPair().
      //
      // Note that filters that were part of a join condition
      // (e.g. an outer join) won't go through that path, so they will
      // be sent through PushDownCondition() below, and possibly end up
      // in table_filters.
      remaining_parts.push_back(item);
    } else if (is_join_condition_for_expr && !IsMultipleEquals(item) &&
               !IsSubset(item->used_tables() & ~PSEUDO_TABLE_BITS,
                         expr->tables_in_subtree)) {
      // Condition refers to tables outside this subtree, so it can not be
      // pushed (this can only happen with semijoins).
      remaining_parts.push_back(item);
    } else {
      PushDownCondition(thd, item, expr, is_join_condition_for_expr,
                        companion_collection, table_filters,
                        cycle_inducing_edges, &remaining_parts);
    }
  }

  return remaining_parts;
}

/**
  For each condition posted as a join condition on “expr”, try to push
  all of them further down the tree, as far as we can; then recurse to
  the child nodes, if any.

  This is needed because the initial optimization steps (before the join
  optimizer) try to hoist join conditions as far _up_ the tree as possible,
  normally all the way up to the WHERE, but could be stopped by outer joins and
  antijoins. E.g. assume what the user wrote was

     a LEFT JOIN (B JOIN C on b.x=c.x)

  This would be pulled up to

     a LEFT JOIN (B JOIN C) ON b.x=c.x

  ie., a pushable join condition posted on the LEFT JOIN, that could not go into
  the WHERE. When this function is called on the said join, it will push the
  join condition down again.
 */
void PushDownJoinConditions(THD *thd, RelationalExpression *expr,
                            const CompanionSetCollection &companion_collection,
                            Mem_root_array<Item *> *table_filters,
                            Mem_root_array<Item *> *cycle_inducing_edges) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  if (!expr->join_conditions.empty()) {
    expr->join_conditions = PushDownAsMuchAsPossible(
        thd, std::move(expr->join_conditions), expr,
        /*is_join_condition_for_expr=*/true, companion_collection,
        table_filters, cycle_inducing_edges);
  }
  if (expr->type == RelationalExpression::MULTI_INNER_JOIN) {
    for (RelationalExpression *child : expr->multi_children) {
      PushDownJoinConditions(thd, child, companion_collection, table_filters,
                             cycle_inducing_edges);
    }
  } else {
    PushDownJoinConditions(thd, expr->left, companion_collection, table_filters,
                           cycle_inducing_edges);
    PushDownJoinConditions(thd, expr->right, companion_collection,
                           table_filters, cycle_inducing_edges);
  }
}

/**
  Similar to PushDownJoinConditions(), but for push of sargable conditions
  (see PushDownJoinConditionsForSargable()). The reason this is a separate
  function, is that we want to run sargable push after all join conditions
  have been finalized; in particular, that multiple equalities have been
  concretized into single equalities. (We don't recognize multi-equalities
  as sargable predicates in their multi-form, since they could be matching
  multiple targets and generally are more complicated. It is much simpler
  to wait until they are concretized.)
 */
void PushDownJoinConditionsForSargable(THD *thd, RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  for (Item *item : expr->join_conditions) {
    // These are the same conditions as PushDownAsMuchAsPossible();
    // not filters (which shouldn't be here anyway), and not tables
    // outside the subtree.
    if (const table_map tables = item->used_tables() & ~PSEUDO_TABLE_BITS;
        popcount(tables) >= 2 && IsSubset(tables, expr->tables_in_subtree)) {
      PushDownToSargableCondition(item, expr,
                                  /*is_join_condition_for_expr=*/true);
    }
  }
  PushDownJoinConditionsForSargable(thd, expr->left);
  PushDownJoinConditionsForSargable(thd, expr->right);
}

/**
  Do a final pass of unexpanded (and non-degenerate) multiple equalities on join
  conditions, deciding on what equalities to concretize them into right before
  pushing join conditions to sargable predicates. The reason for doing it after
  all other pushing is that we want to make sure not to expand the hyperedges
  any more than necessary, and we don't know what “necessary” is before
  everything else is pushed.

  This is only relevant for antijoins and semijoins; inner joins (and partially
  left joins) get concretized as we push, since they can resolve such conflicts
  by associative rewrites and/or creating cycles in the graph. Normally,
  we probably wouldn't worry about such a narrow case, but there are specific
  benchmark queries that happen to exhibit this problem.

  There may still be remaining ones afterwards, such as those that are
  degenerate or within more complex expressions; CanonicalizeJoinConditions()
  will deal with them.
 */
void LateConcretizeMultipleEqualities(THD *thd, RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.

  for (Item *&item : expr->join_conditions) {
    if (IsMultipleEquals(item) &&
        Overlaps(item->used_tables(), expr->left->tables_in_subtree) &&
        Overlaps(item->used_tables(), expr->right->tables_in_subtree)) {
      item = ConcretizeMultipleEquals(down_cast<Item_multi_eq *>(item), *expr);
    }
  }
  LateConcretizeMultipleEqualities(thd, expr->left);
  LateConcretizeMultipleEqualities(thd, expr->right);
}

// Find tables that are guaranteed to either return zero or only NULL rows.
table_map FindNullGuaranteedTables(const RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return 0;
  }
  if (expr->join_conditions_reject_all_rows) {
    switch (expr->type) {
      case RelationalExpression::INNER_JOIN:
      case RelationalExpression::STRAIGHT_INNER_JOIN:
      case RelationalExpression::FULL_OUTER_JOIN:
      case RelationalExpression::SEMIJOIN:
        return expr->tables_in_subtree;
      case RelationalExpression::LEFT_JOIN:
        return expr->right->tables_in_subtree;
      case RelationalExpression::ANTIJOIN:
        return FindNullGuaranteedTables(expr->left);
      case RelationalExpression::TABLE:
      case RelationalExpression::MULTI_INNER_JOIN:
        assert(false);
    }
  }
  return FindNullGuaranteedTables(expr->left) |
         FindNullGuaranteedTables(expr->right);
}

// For joins where we earlier found that the join conditions would reject
// all rows, clear the equijoins (which we know is safe from side effects).
// Also propagate this property up the tree wherever we have other equijoins
// referring to the now-pruned tables.
void ClearImpossibleJoinConditions(RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }

  // Go through the equijoin conditions and check that all of them still
  // refer to tables that exist. If some table was pruned away, but the
  // equijoin condition still refers to it, it could become degenerate:
  // The only rows it could ever see would be NULL-complemented rows,
  // so if the join condition is NULL-rejecting on the pruned table,
  // it will never match. In this case, we can remove the entire build path
  // and propagate the zero-row property to our own join. This matches what we
  // do in CreateHashJoinAccessPath() in the old executor; see the code there
  // for some more comments.
  if (!expr->join_conditions_reject_all_rows) {
    const table_map pruned_tables = FindNullGuaranteedTables(expr);
    for (Item *item : expr->equijoin_conditions) {
      if (Overlaps(item->not_null_tables(), pruned_tables)) {
        expr->join_conditions_reject_all_rows = true;
        break;
      }
    }
  }
  if (expr->join_conditions_reject_all_rows) {
    expr->equijoin_conditions.clear();
  }
  ClearImpossibleJoinConditions(expr->left);
  ClearImpossibleJoinConditions(expr->right);
}

/**
  Find out whether we should create mesh edges (all-to-all) for this multiple
  equality. Currently, we only support full mesh, ie., those where all tables
  involved in the multi-equality are part of the same companion set. One could
  imagine a multi-equality where not all tables are possible to mesh, e.g.
  {t1,t2,t3,t4} where {t1,t2,t3} are on the left side of an outer join and t4 is
  on the right side (and thus not part of the same companion set); if so, we
  could have created a mesh of the three first ones, but we don't currently.
 */
bool ShouldCompleteMeshForCondition(
    Item_multi_eq *item_equal,
    const CompanionSetCollection &companion_collection) {
  if (companion_collection.Find(item_equal->used_tables()) == nullptr) {
    return false;
  }
  if (item_equal->const_arg() != nullptr) {
    return false;
  }
  return true;
}

// Extract multiple equalities that we should create mesh edges for.
// See ShouldCompleteMeshForCondition().
void ExtractCycleMultipleEqualities(
    const Mem_root_array<Item *> &conditions,
    const CompanionSetCollection &companion_collection,
    Mem_root_array<Item_multi_eq *> *multiple_equalities) {
  for (Item *item : conditions) {
    assert(!IsMultipleEquals(item));  // Should have been canonicalized earlier.
    if (is_function_of_type(item, Item_func::EQ_FUNC)) {
      Item_func_eq *eq_item = down_cast<Item_func_eq *>(item);
      if (eq_item->source_multiple_equality != nullptr &&
          ShouldCompleteMeshForCondition(eq_item->source_multiple_equality,
                                         companion_collection)) {
        multiple_equalities->push_back(eq_item->source_multiple_equality);
      }
    }
  }
}

// Extract multiple equalities that we should create mesh edges for.
// See ShouldCompleteMeshForCondition().
void ExtractCycleMultipleEqualitiesFromJoinConditions(
    const RelationalExpression *expr,
    const CompanionSetCollection &companion_collection,
    Mem_root_array<Item_multi_eq *> *multiple_equalities) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  for (Item_eq_base *eq_item : expr->equijoin_conditions) {
    if (eq_item->source_multiple_equality != nullptr &&
        ShouldCompleteMeshForCondition(eq_item->source_multiple_equality,
                                       companion_collection)) {
      multiple_equalities->push_back(eq_item->source_multiple_equality);
    }
  }
  ExtractCycleMultipleEqualitiesFromJoinConditions(
      expr->left, companion_collection, multiple_equalities);
  ExtractCycleMultipleEqualitiesFromJoinConditions(
      expr->right, companion_collection, multiple_equalities);
}

/**
  Similar to work done in JOIN::finalize_table_conditions() in the old
  optimizer. Non-join predicates are done near the start in
  MakeJoinHypergraph().
 */
bool CanonicalizeJoinConditions(THD *thd, RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return false;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  if (CanonicalizeConditions(
          thd, GetVisibleTables(expr->left) | GetVisibleTables(expr->right),
          expr->tables_in_subtree, &expr->join_conditions)) {
    return true;
  }

  // Find out if any of the conditions are plain “false”.
  // Note that we don't actually try to remove any of the other conditions
  // if so (although they may have been optimized away earlier);
  // Cartesian products make for very restrictive join edges, so it's actually
  // more flexible to leave them be, until after the hypergraph construction
  // (in ClearImpossibleJoinConditions(), where we also propagate this
  // property up the tree).
  for (Item *cond : expr->join_conditions) {
    if (cond->has_subquery() || cond->cost().IsExpensive()) {
      continue;
    }
    if (cond->const_for_execution() && cond->val_int() == 0) {
      expr->join_conditions_reject_all_rows = true;
      break;
    }
  }
  if (thd->is_error()) {
    // val_int() above failed.
    return true;
  }

  return CanonicalizeJoinConditions(thd, expr->left) ||
         CanonicalizeJoinConditions(thd, expr->right);
}

/**
  For all join conditions on “expr”, go through and figure out which ones are
  equijoin conditions, ie., suitable for hash join. An equijoin condition for us
  is one that is an equality comparison (=) and pulls in relations from both
  sides of the tree (so is not degenerate, and pushed as far down as possible).
  We also demand that it does not use row comparison, as our hash join
  implementation currently does not support that. Any condition that is found to
  be an equijoin condition is moved from expr->join_conditions to
  expr->equijoin_conditions.

  The function recurses down the join tree.
 */
void MakeHashJoinConditions(THD *thd, RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  if (!expr->join_conditions.empty()) {
    assert(expr->equijoin_conditions.empty());
    Mem_root_array<Item *> extra_conditions(thd->mem_root);

    for (Item *item : expr->join_conditions) {
      // See if this is a (non-degenerate) equijoin condition.
      if (item->type() == Item::FUNC_ITEM) {
        Item_func *func_item = down_cast<Item_func *>(item);
        if (func_item->contains_only_equi_join_condition()) {
          Item_eq_base *join_condition = down_cast<Item_eq_base *>(func_item);
          if (IsHashEquijoinCondition(join_condition,
                                      expr->left->tables_in_subtree,
                                      expr->right->tables_in_subtree)) {
            expr->equijoin_conditions.push_back(join_condition);
            continue;
          }
        }
      }
      // It was not.
      extra_conditions.push_back(item);
    }
    expr->join_conditions = std::move(extra_conditions);
  }
  MakeHashJoinConditions(thd, expr->left);
  MakeHashJoinConditions(thd, expr->right);
}

void FindConditionsUsedTables(THD *thd, RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  expr->conditions_used_tables = UsedTablesForCondition(*expr);
  FindConditionsUsedTables(thd, expr->left);
  FindConditionsUsedTables(thd, expr->right);
}

/**
  Run simple CSE on all conditions (see CommonSubexpressionElimination()).
 */
void CSEConditions(THD *thd, Mem_root_array<Item *> *conditions) {
  bool need_resplit = false;
  for (Item *&item : *conditions) {
    Item *new_item = CommonSubexpressionElimination(item);
    if (new_item != item) {
      need_resplit = true;
      item = new_item;
    }
  }
  if (need_resplit) {
    *conditions = ResplitConditions(thd, *conditions);
  }
}

/// Find (via a multiple equality) and return a constant that should replace
/// "item". If no such constant is found, return "item".
Item *GetSubstitutionConst(Item_field *item, Item_func *parent_func) {
  Item_multi_eq *equal = item->multi_equality();
  if (equal == nullptr) return item;
  Item *const_item = equal->const_arg();
  if (const_item == nullptr || !item->has_compatible_context(const_item) ||
      !parent_func->allow_replacement(item, const_item))
    return item;
  return equal->const_arg();
}

/// Replace fields with constants in "cond".
Item *PropagateConstants(Item *cond) {
  // While walking down the item tree, maintain a stack of pointers to enclosing
  // functions, so that the visitor can access the parent function.
  Prealloced_array<Item_func *, 10> stack(PSI_NOT_INSTRUMENTED);

  // Find all Item_fields in the condition and see if they can be replaced with
  // a constant item.
  return CompileItem(
      cond,
      [&stack](Item *item) {
        if (item->type() == Item::FUNC_ITEM) {
          stack.push_back(down_cast<Item_func *>(item));
        }
        return true;
      },
      [&stack](Item *item) -> Item * {
        switch (item->type()) {
          case Item::FUNC_ITEM:
            stack.pop_back();
            return item;
          case Item::FIELD_ITEM:
            return GetSubstitutionConst(down_cast<Item_field *>(item),
                                        stack.back());
          default:
            return item;
        }
      });
}

/// Find (via a multiple equality) a field that should replace "item". If no
/// such field is found, return "item".
///
/// @param item    The item to find a replacement for.
/// @param parent  The immediately enclosing function in which "item" is found.
/// @param allowed_tables  The set of tables the replacement can come from.
/// @return The field to replace "item" with, or "item".
Item_field *GetSubstitutionField(Item_field *item, Item_func *parent,
                                 table_map allowed_tables) {
  Item_multi_eq *item_equal = item->multi_equality();
  if (item_equal == nullptr || item_equal->const_arg() != nullptr) {
    return item;
  }

  for (Item_field &subst_field : item_equal->get_fields()) {
    if (IsSubset(subst_field.used_tables(), allowed_tables) &&
        item->has_compatible_context(&subst_field) &&
        parent->allow_replacement(item, &subst_field)) {
      return &subst_field;
    }
  }
  return item;
}

Item *PropagateEqualities(Item *cond, const RelationalExpression *join) {
  table_map tables_in_subtree = TablesBetween(0, MAX_TABLES);
  // If this is a degenerate join condition (that is, all fields in the
  // join condition come from the same side of the join), we need to
  // find replacements, if any, from the same side, so that the condition
  // continues to be pushable to that side.
  if (join != nullptr) {
    tables_in_subtree =
        IsSubset(cond->used_tables(), join->left->tables_in_subtree)
            ? join->left->tables_in_subtree
            : (IsSubset(cond->used_tables(), join->right->tables_in_subtree)
                   ? join->right->tables_in_subtree
                   : join->tables_in_subtree);
  }

  // While walking down the item tree, maintain a stack of pointers to enclosing
  // functions, so that the visitor can access the parent function.
  Prealloced_array<Item_func *, 10> stack(PSI_NOT_INSTRUMENTED);

  // Find all Item_fields in the condition and see if they can be replaced with
  // a more beneficial one.
  return CompileItem(
      cond,
      [&stack](Item *item) {
        if (item->type() == Item::FUNC_ITEM) {
          stack.push_back(down_cast<Item_func *>(item));
        }
        return true;
      },
      [tables_in_subtree, &stack](Item *item) -> Item * {
        switch (item->type()) {
          case Item::FUNC_ITEM:
            stack.pop_back();
            return item;
          case Item::FIELD_ITEM:
            return GetSubstitutionField(down_cast<Item_field *>(item),
                                        stack.back(), tables_in_subtree);
          default:
            return item;
        }
      });
}

/**
  Do some equality and constant propagation, conversion/folding work needed
  for correctness and performance.
 */
bool EarlyNormalizeConditions(THD *thd, const RelationalExpression *join,
                              Mem_root_array<Item *> *conditions,
                              bool *always_false) {
  CSEConditions(thd, conditions);
  bool need_resplit = false;
  for (auto it = conditions->begin(); it != conditions->end();) {
    /**
      For simple filters, propagate constants if there are any
      established through multiple equalities. Note that most of the
      propagation is already done in optimize_cond(). This is to handle
      only the corner cases where equality propagation in optimize_cond()
      would have been rejected (which is done in old optimizer at a later
      point).
      For join conditions which are not part of multiple equalities, try
      to substitute fields with the fields from available tables in the
      join. It's possible only if there are multiple equalities for the
      fields in the join condition.
      E.g.
      1. t1.a = t2.a and t1.a <> t2.a would be multi-equal(t1.a, t2.a)
      and t1.a <> t2.a post optimize_cond(). We could transform this
      condition into multi-equal(t1.a, t2.a) and t1.a <> t1.a.
      2. t1.a = t2.a + t3.a could be converted to t1.a = t2.a + t2.a
      if there is multiple equality (t2.a, t3.a). This makes it an
      equi-join condition rather than an extra predicate for the join.
    */
    const bool is_filter =
        popcount((*it)->used_tables() & ~PSEUDO_TABLE_BITS) < 2;
    if (is_filter) {
      *it = PropagateConstants(*it);
    } else if (!is_function_of_type(*it, Item_func::MULTI_EQ_FUNC)) {
      *it = PropagateEqualities(*it, join);
    }

    const Item *const old_item = *it;
    Item::cond_result res;
    if (remove_eq_conds(thd, *it, &*it, &res)) {
      return true;
    }

    if (res == Item::COND_TRUE) {
      // Remove always true conditions from the conjunction.
      it = conditions->erase(it);
    } else if (res == Item::COND_FALSE) {
      // One always false condition makes the entire conjunction always false.
      conditions->clear();
      conditions->push_back(new Item_func_false);
      *always_false = true;
      return false;
    } else {
      assert(*it != nullptr);
      // If the condition was replaced by a conjunction, we need to split it and
      // add its children to conditions, so that its individual elements can be
      // considered for condition pushdown later.
      if (*it != old_item && IsAnd(*it)) {
        need_resplit = true;
      }

      (*it)->update_used_tables();
      ++it;
    }
  }

  if (need_resplit) {
    *conditions = ResplitConditions(thd, *conditions);
  }

  return false;
}

string PrintJoinList(const mem_root_deque<Table_ref *> &join_list, int level) {
  string str;
  const char *join_types[] = {"inner", "left", "right"};
  std::vector<Table_ref *> list(join_list.begin(), join_list.end());
  for (Table_ref *tbl : list) {
    for (int i = 0; i < level * 2; ++i) str += ' ';
    if (tbl->join_cond_optim() != nullptr) {
      str += StringPrintf("* %s %s  join_type=%s\n", tbl->alias,
                          ItemToString(tbl->join_cond_optim()).c_str(),
                          join_types[tbl->outer_join]);
    } else {
      str += StringPrintf("* %s  join_type=%s\n", tbl->alias,
                          join_types[tbl->outer_join]);
    }
    if (tbl->nested_join != nullptr) {
      str += PrintJoinList(tbl->nested_join->m_tables, level + 1);
    }
  }
  return str;
}

/**
  For a condition with the SES (Syntactic Eligibility Set) “used_tables”,
  find all relations in or under “expr” that are part of the condition's TES
  (Total Eligibility Set). The SES contains all relations that are directly
  referenced by the predicate; the TES contains all relations that are needed
  to be available before the predicate can be evaluated.

  The TES always contains at least SES, but may be bigger. For instance,
  given the join tree (a LEFT JOIN b), a condition such as b.x IS NULL
  would have a SES of {b}, but a TES of {a,b}, since joining in a could
  synthesize NULLs from b. However, given (a JOIN b) (ie., an inner join
  instead of an outer join), the TES would be {b}, identical to the SES.

  NOTE: The terms SES and TES are often used about join conditions;
  the use here is for general conditions beyond just those.

  NOTE: This returns a table_map, which is later converted to a NodeMap.
 */
table_map FindTESForCondition(table_map used_tables,
                              const RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    // We're at the bottom of an inner join stack; nothing to see here.
    // (We could just as well return 0, but this at least makes sure the
    // SES is included in the TES.)
    return used_tables;
  } else if (expr->type == RelationalExpression::LEFT_JOIN ||
             expr->type == RelationalExpression::ANTIJOIN) {
    table_map tes = used_tables;
    if (Overlaps(used_tables, expr->left->tables_in_subtree)) {
      tes |= FindTESForCondition(used_tables, expr->left);
    }
    if (Overlaps(used_tables, expr->right->tables_in_subtree)) {
      // The predicate needs a table from the right-hand side, but this join can
      // cause that table to become NULL, so we need to delay until the join has
      // happened. We do this by demanding that all tables on the left side have
      // been joined in, and then at least the tables we need from the right
      // side (from the SES).
      //
      // Note that pruning aggressively on the left-hand side is prone to
      // failure due to associative rewriting of left joins; e.g., for left
      // joins and suitable join conditions:
      //
      //   (t1 <opA> t2) <opB> t3 <=> t1 <opA> (t2 <opB> t3)
      //
      // In particular, this means that if we have a WHERE predicate affecting
      // t2 and t3 (tested against <opB>), TES still has to be {t1,t2,t3};
      // if we limited it to {t2,t3}, we would push it below <opA> in the case
      // of the rewrite, which is wrong. So the entire left side needs to be
      // included, preventing us to push the condition down into the right side
      // in any case.
      tes |= expr->left->tables_in_subtree;
      for (Item *condition : expr->equijoin_conditions) {
        tes |= condition->used_tables();
      }
      for (Item *condition : expr->join_conditions) {
        tes |= condition->used_tables();
      }
    }
    return tes;
  } else {
    table_map tes = used_tables;
    if (Overlaps(used_tables, expr->left->tables_in_subtree)) {
      tes |= FindTESForCondition(used_tables, expr->left);
    }
    if (Overlaps(used_tables, expr->right->tables_in_subtree)) {
      tes |= FindTESForCondition(used_tables, expr->right);
    }
    return tes;
  }
}

}  // namespace

/**
  For the given hypergraph, make a textual representation in the form
  of a dotty graph. You can save this to a file and then use Graphviz
  to render this it a graphical representation of the hypergraph for
  easier debugging, e.g. like this:

    dot -Tps graph.dot > graph.ps
    display graph.ps

  See also Dbug_table_list_dumper.
 */
string PrintDottyHypergraph(const JoinHypergraph &graph) {
  string digraph;
  digraph =
      StringPrintf("digraph G {  # %zu edges\n", graph.graph.edges.size() / 2);

  // Create new internal node names for all nodes, resolving conflicts between
  // aliases as we go.
  vector<string> aliases;
  for (const JoinHypergraph::Node &node : graph.nodes) {
    string alias = node.table()->alias;
    while (std::find(aliases.begin(), aliases.end(), alias) != aliases.end()) {
      alias += '_';
    }
    if (alias != node.table()->alias) {
      digraph += StringPrintf("  %s [label=\"%s\"];\n", alias.c_str(),
                              node.table()->alias);
    }
    aliases.push_back(std::move(alias));
  }

  for (size_t edge_idx = 0; edge_idx < graph.graph.edges.size();
       edge_idx += 2) {
    const Hyperedge &e = graph.graph.edges[edge_idx];
    const RelationalExpression *expr = graph.edges[edge_idx / 2].expr;
    string label = GenerateExpressionLabel(expr);

    label += StringPrintf(" (%.3g)", graph.edges[edge_idx / 2].selectivity);

    // Add conflict rules to the label.
    for (const ConflictRule &rule : expr->conflict_rules) {
      label += " [conflict rule: {";
      bool first = true;
      for (int node_idx : BitsSetIn(rule.needed_to_activate_rule)) {
        if (!first) {
          label += ",";
        }
        label += aliases[node_idx];
        first = false;
      }
      label += "} -> {";
      first = true;
      for (int node_idx : BitsSetIn(rule.required_nodes)) {
        if (!first) {
          label += ",";
        }
        label += aliases[node_idx];
        first = false;
      }
      label += "}]";
    }

    // Draw inner joins as undirected; it is less confusing.
    // When we get full outer joins, maybe we should have double arrows here?
    const char *arrowhead_str =
        expr->type == RelationalExpression::INNER_JOIN ? ",arrowhead=none" : "";

    // Output the edge.
    if (IsSimpleEdge(e.left, e.right)) {
      // Simple edge.
      int left_node = FindLowestBitSet(e.left);
      int right_node = FindLowestBitSet(e.right);
      digraph += StringPrintf(
          "  %s -> %s [label=\"%s\"%s]\n", aliases[left_node].c_str(),
          aliases[right_node].c_str(), label.c_str(), arrowhead_str);
    } else {
      // Hyperedge; draw it as a tiny “virtual node”.
      digraph += StringPrintf(
          "  e%zu [shape=circle,width=.001,height=.001,label=\"\"]\n",
          edge_idx);

      // Print the label only once.
      string left_label, right_label;
      if (has_single_bit(e.right) && !has_single_bit(e.left)) {
        right_label = label;
      } else {
        left_label = label;
      }

      // Left side of the edge.
      for (int left_node : BitsSetIn(e.left)) {
        digraph += StringPrintf("  %s -> e%zu [arrowhead=none,label=\"%s\"]\n",
                                aliases[left_node].c_str(), edge_idx,
                                left_label.c_str());
        left_label = "";
      }

      // Right side of the edge.
      for (int right_node : BitsSetIn(e.right)) {
        digraph += StringPrintf("  e%zu -> %s [label=\"%s\"%s]\n", edge_idx,
                                aliases[right_node].c_str(),
                                right_label.c_str(), arrowhead_str);
        right_label = "";
      }
    }
  }
  digraph += "}\n";
  return digraph;
}

size_t EstimateHashJoinKeyWidth(const RelationalExpression *expr) {
  size_t ret = 0;
  for (Item_eq_base *join_condition : expr->equijoin_conditions) {
    // We heuristically limit our estimate of blobs to 4 kB.
    // Otherwise, the mere presence of a LONGBLOB field would mean
    // we'd estimate essentially infinite row width for a join.
    //
    // TODO(sgunders): Do as we do in the old optimizer,
    // where we only store hashes for strings.
    const Item *left = join_condition->get_arg(0);
    const Item *right = join_condition->get_arg(1);
    ret += min<size_t>(
        max<size_t>(left->max_char_length(), right->max_char_length()),
        kMaxItemLengthEstimate);
  }
  return ret;
}

namespace {

table_map IntersectIfNotDegenerate(table_map used_tables,
                                   table_map available_tables) {
  if (!Overlaps(used_tables, available_tables)) {
    // Degenerate case.
    return available_tables;
  } else {
    return used_tables & available_tables;
  }
}

/**
  When we have the conflict rules, we want to fold them into the hyperedge
  we are about to create. This works by growing the TES (Total Eligibility
  Set), the set of tables that needs to be present before we can do the
  join; the TES will eventually be split into two and made into a hyperedge.

  The TES must obviously include the SES (Syntactic Eligibility Set),
  every table mentioned in the join condition. And if anything on the left
  side of a conflict rule overlaps with the TES, that conflict rule would
  always be active, and we can safely include the right side into the TES.
  Similarly, if the TES is a superset of what's on the right side of a conflict
  rule, that rule will never prevent anything (since we never see a subgraph
  unless we have everything touched by its hyperedge, ie., the TES), so it
  can be removed. We iterate over all the conflict rules until they are all
  gone or the TES has stopped growing; then we create our hyperedge by
  splitting the TES.
 */
NodeMap AbsorbConflictRulesIntoTES(
    NodeMap total_eligibility_set,
    Mem_root_array<ConflictRule> *conflict_rules) {
  NodeMap prev_total_eligibility_set;
  do {
    prev_total_eligibility_set = total_eligibility_set;
    for (const ConflictRule &rule : *conflict_rules) {
      if (Overlaps(rule.needed_to_activate_rule, total_eligibility_set)) {
        // This conflict rule will always be active, so we can add its right
        // side to the TES unconditionally. (The rule is now obsolete and
        // will be removed below.)
        total_eligibility_set |= rule.required_nodes;
      }
    }
    auto new_end = std::remove_if(
        conflict_rules->begin(), conflict_rules->end(),
        [total_eligibility_set](const ConflictRule &rule) {
          // If the right side of the conflict rule is
          // already part of the TES, it is obsolete
          // and can be removed. It will be dealt with
          // as a hyperedge.
          return IsSubset(rule.required_nodes, total_eligibility_set);
        });
    conflict_rules->erase(new_end, conflict_rules->end());
  } while (total_eligibility_set != prev_total_eligibility_set &&
           !conflict_rules->empty());
  return total_eligibility_set;
}

/**
  For the join operator in “expr”, build a hyperedge that encapsulates its
  reordering conditions as completely as possible. The conditions given by
  the hyperedge are necessary and usually sufficient; for the cases where
  they are not sufficient, we leave conflict rules on “expr” (see below).

  This function is almost verbatim the CD-C algorithm from “On the correct and
  complete enumeration of the core search space” by Moerkotte et al [Moe13].
  It works by the concept of conflict rules (CRs); if a CR A → B, for relation
  sets A and B, is attached on a given join, then if _any_ table from A is
  present in the join, then _all_ tables from B are required. As a trivial
  example, one can imagine t1 \<opA\> (t2 \<opB\> t3); if \<opA\> has a CR
  {t2} → {t3}, then the rewrite (t1 \<opA\> t2) \<opB\> t3 would not be allowed,
  since t2 is present but t3 is not. However, in the absence of other CRs,
  and given appropriate connectivity in the graph, the rewrite
  (t1 \<opA\> t3) \<opB\> t2 _would_ be allowed.

  Conflict rules are both expressive enough to precisely limit invalid rewrites,
  and in the majority of cases, can be folded into hyperedges, relegating
  the task of producing only valid plans to the subgraph enumeration (DPhyp),
  which is highly efficient at it. In the few cases that remain, they will need
  to be checked manually in CostingReceiver, but this is fast (only a few bitmap
  operations per remaining CR).

  The gist of the algorithm is to compare every operator with every operator
  below it in the join tree, looking for illegal rewrites between them, and
  adding precise CRs to stop only those rewrites. For instance, assume a query
  like

    t1 LEFT JOIN (t2 JOIN t3 USING (y)) ON t1.x=t2.x

  Looking at the root predicate (the LEFT JOIN), the question is what CRs
  and hyperedge to produce. The join predicate only mentions t1 and t2,
  so it only gives rise to the simple edge {t1}→{t2}. So without any conflict
  rules, nothing would stop us from joining t1/t2 without including t3,
  and we would allow a generated plan essentially equal to

    (t1 LEFT JOIN t2 ON t1.x=t2.x) JOIN t3 USING (y)

  which is illegal; we have attempted to use associativity illegally.
  So when we compare the LEFT JOIN (in the original query tree) with the JOIN,
  we look up those two operator types using OperatorsAreAssociative()
  (which essentially does a lookup into a small table), see that the combination
  LEFT JOIN and JOIN is not associative, and thus create a conflict rule that
  prevents this:

    {t2} → {t3}

  t2 here is everything on the left side of the inner join, and t3 is every
  table on the right side of the inner join that is mentioned in the join
  condition (which happens to also be everything on the right side).
  This rule, posted on the LEFT JOIN, prevents it from including t2 until
  it has been combined with t3, which is exactly what we want. There are some
  tweaks for degenerate conditions, but that's really all for associativity
  conflict rules.

  The other source of conflict rules comes from a parallel property
  called l-asscom and r-asscom; see OperatorsAreLeftAsscom() and
  OperatorsAreRightAsscom(). They work in exactly the same way; look at
  every pair between and operator and its children, look it up in a table,
  and add a conflict rule that prevents the rewrite if it is illegal.

  When we have the CRs, we want to fold them into the hyperedge
  we are about to create. See AbsorbConflictRulesIntoTES() for details.

  Note that in the presence of degenerate predicates or Cartesian products,
  we may make overly broad hyperedges, ie., we will disallow otherwise
  valid plans (but never allow invalid plans). This is the only case where
  the algorithm misses a valid join ordering, and also the only place where
  we diverge somewhat from the paper, which doesn't discuss hyperedges in
  the presence of such cases.
 */
Hyperedge FindHyperedgeAndJoinConflicts(THD *thd, NodeMap used_nodes,
                                        RelationalExpression *expr,
                                        const JoinHypergraph *graph) {
  assert(expr->type != RelationalExpression::TABLE);

  Mem_root_array<ConflictRule> conflict_rules(thd->mem_root);
  ForEachJoinOperator(
      expr->left, [expr, graph, &conflict_rules](RelationalExpression *child) {
        if (!OperatorsAreAssociative(*child, *expr)) {
          // Prevent associative rewriting; we cannot apply this operator
          // (rule kicks in as soon as _any_ table from the right side
          // is seen) until we have all nodes mentioned on the left side of
          // the join condition.
          const table_map left = IntersectIfNotDegenerate(
              child->conditions_used_tables, child->left->tables_in_subtree);
          conflict_rules.emplace_back(ConflictRule{
              child->right->nodes_in_subtree,
              GetNodeMapFromTableMap(left & ~PSEUDO_TABLE_BITS,
                                     graph->table_num_to_node_num)});
        }
        if (!OperatorsAreLeftAsscom(*child, *expr)) {
          // Prevent l-asscom rewriting; we cannot apply this operator
          // (rule kicks in as soon as _any_ table from the left side
          // is seen) until we have all nodes mentioned on the right side of
          // the join condition.
          const table_map right = IntersectIfNotDegenerate(
              child->conditions_used_tables, child->right->tables_in_subtree);
          conflict_rules.emplace_back(ConflictRule{
              child->left->nodes_in_subtree,
              GetNodeMapFromTableMap(right & ~PSEUDO_TABLE_BITS,
                                     graph->table_num_to_node_num)});
        }
      });

  // Exactly the same as the previous, just mirrored left/right.
  ForEachJoinOperator(
      expr->right, [expr, graph, &conflict_rules](RelationalExpression *child) {
        if (!OperatorsAreAssociative(*expr, *child)) {
          const table_map right = IntersectIfNotDegenerate(
              child->conditions_used_tables, child->right->tables_in_subtree);
          conflict_rules.emplace_back(ConflictRule{
              child->left->nodes_in_subtree,
              GetNodeMapFromTableMap(right & ~PSEUDO_TABLE_BITS,
                                     graph->table_num_to_node_num)});
        }
        if (!OperatorsAreRightAsscom(*expr, *child)) {
          const table_map left = IntersectIfNotDegenerate(
              child->conditions_used_tables, child->left->tables_in_subtree);
          conflict_rules.emplace_back(ConflictRule{
              child->right->nodes_in_subtree,
              GetNodeMapFromTableMap(left & ~PSEUDO_TABLE_BITS,
                                     graph->table_num_to_node_num)});
        }
      });

  // Now go through all of the conflict rules and use them to grow the
  // hypernode, making it more restrictive if possible/needed.
  NodeMap total_eligibility_set =
      AbsorbConflictRulesIntoTES(used_nodes, &conflict_rules);

  // Check for degenerate predicates and Cartesian products;
  // we cannot have hyperedges with empty end points. If we have to
  // go down this path, re-check if there are any conflict rules
  // that we can now get rid of.
  if (!Overlaps(total_eligibility_set, expr->left->nodes_in_subtree)) {
    total_eligibility_set |= expr->left->nodes_in_subtree;
    total_eligibility_set =
        AbsorbConflictRulesIntoTES(total_eligibility_set, &conflict_rules);
  }
  if (!Overlaps(total_eligibility_set, expr->right->nodes_in_subtree)) {
    total_eligibility_set |= expr->right->nodes_in_subtree;
    total_eligibility_set =
        AbsorbConflictRulesIntoTES(total_eligibility_set, &conflict_rules);
  }
  expr->conflict_rules = std::move(conflict_rules);

  const NodeMap left = total_eligibility_set & expr->left->nodes_in_subtree;
  const NodeMap right = total_eligibility_set & expr->right->nodes_in_subtree;
  return {left, right};
}

size_t EstimateRowWidthForJoin(const JoinHypergraph &graph,
                               const RelationalExpression *expr) {
  // Estimate size of the join keys.
  size_t ret = EstimateHashJoinKeyWidth(expr);

  // Estimate size of the values.
  for (int node_idx : BitsSetIn(expr->nodes_in_subtree)) {
    const TABLE *table = graph.nodes[node_idx].table();
    for (uint i = 0; i < table->s->fields; ++i) {
      if (bitmap_is_set(table->read_set, i)) {
        Field *field = table->field[i];

        // See above.
        ret += min<size_t>(field->max_data_length(), kMaxItemLengthEstimate);
      }
    }
  }

  // Heuristically add 20 bytes for LinkedImmutableString and hash table
  // overhead. (The actual overhead will vary with hash table fill factor
  // and the number of keys that have multiple rows.)
  ret += 20;

  return ret;
}

/**
  Sorts the given range of predicates so that the most selective and least
  expensive predicates come first, and the less selective and more expensive
  ones come last.
 */
void SortPredicates(Predicate *begin, Predicate *end) {
  if (std::distance(begin, end) <= 1) return;  // Nothing to sort.

  /*
    By ordering the predicates by rank(predicate) in ascending order, we should
    minimize the expected cost of evaluating the conjunction.

    The formulae is taken from:
    "J. M. Hellerstein, M. Stonebraker,
    Predicate migration: Optimizing queries with expensive predicates,
    In Proceedings of the ACM SIGMOD Conference, 1993".
  */
  const auto rank = [](const Predicate &p) {
    return (p.selectivity - 1.0) / std::max(1e-18,  // Prevent divide by zero.
                                            p.condition->cost().FieldCost());
  };

  // Order the predicates so that we minimize the expected cost of evaluating
  // the conjuction.
  std::stable_sort(begin, end, [&](const Predicate &p1, const Predicate &p2) {
    return rank(p1) < rank(p2);
  });

  // If the predicates contain subqueries, move them towards the end, regardless
  // of their selectivity, since they could be expensive to evaluate. We could
  // refine this by looking at the estimated cost of the contained subqueries.
  std::stable_partition(begin, end, [](const Predicate &pred) {
    return !pred.condition->has_subquery();
  });

  // UDFs and stored procedures have unknown and potentially very high cost.
  // Move them last.
  std::stable_partition(begin, end, [](const Predicate &p) {
    return !p.condition->cost().IsExpensive();
  });
}

/**
  Add the given predicate to the list of WHERE predicates, doing some
  bookkeeping that such predicates need.
 */
int AddPredicate(THD *thd, Item *condition, bool was_join_condition,
                 int source_multiple_equality_idx,
                 const RelationalExpression *root,
                 const CompanionSetCollection *companion_collection,
                 JoinHypergraph *graph) {
  if (source_multiple_equality_idx != -1) {
    assert(was_join_condition);
  }

  Predicate pred;
  pred.condition = condition;

  const table_map used_tables =
      condition->used_tables() & ~(INNER_TABLE_BIT | OUTER_REF_TABLE_BIT);
  pred.used_nodes =
      GetNodeMapFromTableMap(used_tables, graph->table_num_to_node_num);

  const bool references_regular_tables =
      Overlaps(used_tables, ~PSEUDO_TABLE_BITS);

  table_map total_eligibility_set;
  if (was_join_condition || !references_regular_tables) {
    total_eligibility_set = used_tables;
  } else {
    total_eligibility_set = FindTESForCondition(used_tables, root) &
                            ~(INNER_TABLE_BIT | OUTER_REF_TABLE_BIT);
  }
  pred.total_eligibility_set = GetNodeMapFromTableMap(
      total_eligibility_set, graph->table_num_to_node_num);

  // If the query is a join, we may get selectivity information from the
  // companion set of the tables referenced by the predicate. For single-table
  // or table-less queries, there is no companion set. Tables not involved in
  // any equijoins do not have a companion set.
  const CompanionSet *companion_set = nullptr;
  if (references_regular_tables && companion_collection != nullptr) {
    companion_set = companion_collection->Find(used_tables);
  }

  pred.selectivity = companion_set != nullptr
                         ? EstimateSelectivity(thd, condition, *companion_set)
                         : EstimateSelectivity(thd, condition, CompanionSet());

  pred.was_join_condition = was_join_condition;
  pred.source_multiple_equality_idx = source_multiple_equality_idx;
  pred.functional_dependencies_idx.init(thd->mem_root);

  // Cache information about which subqueries are contained in this
  // predicate, if any.
  pred.contained_subqueries.init(thd->mem_root);
  FindContainedSubqueries(condition, graph->query_block(),
                          [&pred](const ContainedSubquery &subquery) {
                            pred.contained_subqueries.push_back(subquery);
                          });

  graph->predicates.push_back(std::move(pred));

  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf("Total eligibility set for %s: {",
                               ItemToString(condition).c_str());
    bool first = true;
    for (Table_ref *tl = graph->query_block()->leaf_tables; tl != nullptr;
         tl = tl->next_leaf) {
      if (tl->map() & total_eligibility_set) {
        if (!first) Trace(thd) << ',';
        Trace(thd) << tl->alias;
        first = false;
      }
    }
    Trace(thd) << "}\n";
  }

  return graph->predicates.size() - 1;
}

/**
  Return whether we can find a path from “source” to “destination”, without
  using forbidden_edge_idx.
 */
bool AreNodesConnected(const Hypergraph &graph, int source, int destination,
                       int forbidden_edge_idx, NodeMap *seen_nodes) {
  if (source == destination) {
    return true;
  }
  if (Overlaps(*seen_nodes, TableBitmap(source))) {
    // We've been here before and not found anything, so drop out.
    // This also keeps us from getting stuck in other cycles.
    return false;
  }
  *seen_nodes |= TableBitmap(source);
  for (int edge_idx : graph.nodes[source].simple_edges) {
    if (edge_idx != forbidden_edge_idx) {
      if (AreNodesConnected(graph,
                            *BitsSetIn(graph.edges[edge_idx].right).begin(),
                            destination, forbidden_edge_idx, seen_nodes)) {
        return true;
      }
    }
  }
  for (int edge_idx : graph.nodes[source].complex_edges) {
    if (edge_idx != forbidden_edge_idx) {
      for (int middle : BitsSetIn(graph.edges[edge_idx].right)) {
        if (AreNodesConnected(graph, middle, destination, forbidden_edge_idx,
                              seen_nodes)) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
  Returns whether the given edge is part of a graph cycle; if so, its join
  condition might not actually get evaluated as part of the regular structure,
  and we need to take special precautions (make backup WHERE conditions for
  them).

  Edges that are _not_ part of a cycle are called “bridges” in graph theory.
  There are efficient algorithms for finding all bridges in a graph (see e.g.
  Schmidt: “A Simple Test on 2-Vertex- and 2-Edge-Connectivity”), but our graph
  is small, so we opt for simplicity by simply doing a depth-first search for
  all edges. We only need to consider the part of the subgraph given by inner
  joins (the companion set) -- but we cannot ignore hyperedges, since we
  determine companion sets before we know all the join predicates.
 */
bool IsPartOfCycle(const JoinHypergraph *graph, int edge_idx) {
  const RelationalExpression *expr = graph->edges[edge_idx / 2].expr;
  if (expr->type != RelationalExpression::INNER_JOIN) {
    // Outer joins are always a bridge; we also ignore straight joins
    // (they are a sign the user doesn't want a different ordering anyway).
    return false;
  }

  const Hyperedge &edge = graph->graph.edges[edge_idx];

  // If we can find a path from one end of an edge to the other,
  // ignoring this specific edge, then we have a cycle (pretty much
  // by definition).
  for (int left_idx : BitsSetIn(edge.left)) {
    for (int right_idx : BitsSetIn(edge.right)) {
      NodeMap seen_nodes = 0;
      if (AreNodesConnected(graph->graph, left_idx, right_idx, edge_idx,
                            &seen_nodes)) {
        return true;
      }
    }
  }
  return false;
}

/**
  For each of the given join conditions, add a cycle-inducing edge to the
  hypergraph.
 */
void AddCycleEdges(THD *thd, const Mem_root_array<Item *> &cycle_inducing_edges,
                   CompanionSetCollection &companion_collection,
                   JoinHypergraph *graph) {
  for (Item *cond : cycle_inducing_edges) {
    const NodeMap used_nodes = GetNodeMapFromTableMap(
        cond->used_tables(), graph->table_num_to_node_num);
    RelationalExpression *expr = nullptr;
    JoinPredicate *pred = nullptr;

    const NodeMap left = IsolateLowestBit(used_nodes);  // Arbitrary.
    const NodeMap right = used_nodes & ~left;

    // See if we already have a suitable edge.
    for (size_t edge_idx = 0; edge_idx < graph->edges.size(); ++edge_idx) {
      Hyperedge edge = graph->graph.edges[edge_idx * 2];
      if ((edge.left | edge.right) == used_nodes &&
          graph->edges[edge_idx].expr->type ==
              RelationalExpression::INNER_JOIN) {
        pred = &graph->edges[edge_idx];
        expr = pred->expr;
        break;
      }
    }

    if (expr == nullptr) {
      graph->graph.AddEdge(left, right);

      expr = new (thd->mem_root) RelationalExpression(thd);
      expr->type = RelationalExpression::INNER_JOIN;

      // TODO(sgunders): This does not really make much sense, but
      // estimated_bytes_per_row doesn't make that much sense to begin with; it
      // will depend on the join order. See if we can replace it with a
      // per-table width calculation that we can sum up in the join optimizer.
      expr->tables_in_subtree = cond->used_tables();
      expr->nodes_in_subtree =
          GetNodeMapFromTableMap(cond->used_tables() & ~PSEUDO_TABLE_BITS,
                                 graph->table_num_to_node_num);

      expr->companion_set = companion_collection.Find(expr->tables_in_subtree);

      double selectivity = EstimateSelectivity(thd, cond, *expr->companion_set);
      const size_t estimated_bytes_per_row =
          EstimateRowWidthForJoin(*graph, expr);
      graph->edges.push_back(JoinPredicate{
          expr, selectivity, estimated_bytes_per_row,
          /*functional_dependencies=*/0, /*functional_dependencies_idx=*/{}});
    } else {
      // Skip this item if it is a duplicate (this can
      // happen with multiple equalities in particular).
      bool dup = false;
      for (Item *other_cond : expr->equijoin_conditions) {
        if (other_cond->eq(cond)) {
          dup = true;
          break;
        }
      }
      if (dup) {
        continue;
      }
      for (Item *other_cond : expr->join_conditions) {
        if (other_cond->eq(cond)) {
          dup = true;
          break;
        }
      }
      if (dup) {
        continue;
      }
      pred->selectivity *= EstimateSelectivity(thd, cond, *expr->companion_set);
    }
    if (cond->type() == Item::FUNC_ITEM &&
        down_cast<Item_func *>(cond)->contains_only_equi_join_condition()) {
      expr->equijoin_conditions.push_back(down_cast<Item_eq_base *>(cond));
    } else {
      expr->join_conditions.push_back(cond);
    }

    // Make this predicate potentially sargable (cycle edges are always
    // simple equalities).
    assert(IsSimpleEdge(left, right));
    const int left_node_idx = *BitsSetIn(left).begin();
    const int right_node_idx = *BitsSetIn(right).begin();
    graph->nodes[left_node_idx].AddPushable(cond);
    graph->nodes[right_node_idx].AddPushable(cond);
  }
}

/**
  Promote join predicates that became part of (newly-formed) cycles to
  WHERE predicates.

  The reason for this is that when we have cycles in the graph, we can no
  longer guarantee that all join predicates will be seen; e.g. if we have a
  cycle A - B - C - A, and choose to complete the join by using the A-B and
  C-A edges, we would miss the B-C join predicate. Thus, we promote all join
  edges involved in cycles to WHERE predicates; however, we mark them as coming
  from a join condition, and we also note in the join edge what the indexes of
  the added predicate are. Thus, for A-B and C-A in the given example, we would
  ignore the corresponding WHERE predicates so they do not get double-applied.

  We need to mark which predicates came from which multiple equalities,
  so that they are not added when they are redundant; see the comment on top of
  CostingReceiver::ApplyDelayedPredicatesAfterJoin().

  Note that join predicates may actually get added as predicates a second
  time, if they are found to be sargable. However, in that case they are not
  counted as WHERE predicates (they are never automatically applied), so this
  is a separate use.
 */
void PromoteCycleJoinPredicates(
    THD *thd, const RelationalExpression *root,
    const Mem_root_array<Item_multi_eq *> &multiple_equalities,
    const CompanionSetCollection &companion_collection, JoinHypergraph *graph) {
  for (size_t edge_idx = 0; edge_idx < graph->graph.edges.size();
       edge_idx += 2) {
    if (!IsPartOfCycle(graph, edge_idx)) {
      continue;
    }
    RelationalExpression *expr = graph->edges[edge_idx / 2].expr;
    expr->join_predicate_first = graph->predicates.size();
    for (Item *condition : expr->equijoin_conditions) {
      AddPredicate(thd, condition, /*was_join_condition=*/true,
                   FindSourceMultipleEquality(condition, multiple_equalities),
                   root, &companion_collection, graph);
    }
    for (Item *condition : expr->join_conditions) {
      AddPredicate(thd, condition, /*was_join_condition=*/true,
                   FindSourceMultipleEquality(condition, multiple_equalities),
                   root, &companion_collection, graph);
    }
    expr->join_predicate_last = graph->predicates.size();
    SortPredicates(graph->predicates.begin() + expr->join_predicate_first,
                   graph->predicates.begin() + expr->join_predicate_last);
  }
}

}  // namespace

/**
  Convert a join rooted at “expr” into a join hypergraph that encapsulates
  the constraints given by the relational expressions (e.g. inner joins are
  more freely reorderable than outer joins).

  The function in itself only does some bookkeeping around node bitmaps,
  and then defers the actual conflict detection logic to
  FindHyperedgeAndJoinConflicts().
 */
void MakeJoinGraphFromRelationalExpression(THD *thd, RelationalExpression *expr,
                                           JoinHypergraph *graph) {
  if (expr->type == RelationalExpression::TABLE) {
    graph->graph.AddNode();
    JoinHypergraph::Node node{thd->mem_root, expr->table->table,
                              expr->companion_set};

    for (Item *cond : expr->pushable_conditions()) {
      node.AddPushable(cond);
    }

    graph->nodes.push_back(std::move(node));

    assert(expr->table->tableno() < MAX_TABLES);
    graph->table_num_to_node_num[expr->table->tableno()] =
        graph->graph.nodes.size() - 1;
    expr->nodes_in_subtree = NodeMap{1} << (graph->graph.nodes.size() - 1);
    return;
  }

  MakeJoinGraphFromRelationalExpression(thd, expr->left, graph);
  MakeJoinGraphFromRelationalExpression(thd, expr->right, graph);
  expr->nodes_in_subtree =
      expr->left->nodes_in_subtree | expr->right->nodes_in_subtree;

  table_map used_tables = 0;
  for (Item *condition : expr->join_conditions) {
    used_tables |= condition->used_tables();
  }
  for (Item *condition : expr->equijoin_conditions) {
    used_tables |= condition->used_tables();
  }
  const NodeMap used_nodes = GetNodeMapFromTableMap(
      used_tables & ~PSEUDO_TABLE_BITS, graph->table_num_to_node_num);

  const Hyperedge edge =
      FindHyperedgeAndJoinConflicts(thd, used_nodes, expr, graph);
  graph->graph.AddEdge(edge.left, edge.right);

  // Figure out whether we have two left joins that are associatively
  // reorderable, which can trigger a bug in our row count estimation. See the
  // definition of has_reordered_left_joins for more information.
  if (!graph->has_reordered_left_joins &&
      expr->type == RelationalExpression::LEFT_JOIN) {
    ForEachJoinOperator(expr->left, [expr, graph](RelationalExpression *child) {
      if (child->type == RelationalExpression::LEFT_JOIN &&
          OperatorsAreAssociative(*child, *expr)) {
        graph->has_reordered_left_joins = true;
      }
    });
    ForEachJoinOperator(expr->right,
                        [expr, graph](RelationalExpression *child) {
                          if (child->type == RelationalExpression::LEFT_JOIN &&
                              OperatorsAreAssociative(*expr, *child)) {
                            graph->has_reordered_left_joins = true;
                          }
                        });
  }

  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf("Selectivity of join %s:\n",
                               GenerateExpressionLabel(expr).c_str());
  }
  double selectivity = 1.0;
  for (Item *item : expr->equijoin_conditions) {
    selectivity *= EstimateSelectivity(current_thd, item, *expr->companion_set);
  }
  for (Item *item : expr->join_conditions) {
    selectivity *= EstimateSelectivity(current_thd, item, CompanionSet());
  }
  if (TraceStarted(thd) &&
      expr->equijoin_conditions.size() + expr->join_conditions.size() > 1) {
    Trace(thd) << StringPrintf("  - total: %.g\n", selectivity);
  }

  const size_t estimated_bytes_per_row = EstimateRowWidthForJoin(*graph, expr);
  graph->edges.push_back(JoinPredicate{
      expr, selectivity, estimated_bytes_per_row,
      /*functional_dependencies=*/0, /*functional_dependencies_idx=*/{}});
}

NodeMap GetNodeMapFromTableMap(
    table_map map, const array<int, MAX_TABLES> &table_num_to_node_num) {
  NodeMap ret = 0;
  if (Overlaps(map, RAND_TABLE_BIT)) {  // Special case.
    ret |= RAND_TABLE_BIT;
    map &= ~RAND_TABLE_BIT;
  }
  for (int table_num : BitsSetIn(map)) {
    assert(table_num < int(MAX_TABLES));
    assert(table_num_to_node_num[table_num] != -1);
    ret |= TableBitmap(table_num_to_node_num[table_num]);
  }
  return ret;
}

namespace {

void AddMultipleEqualityPredicate(THD *thd,
                                  CompanionSetCollection &companion_collection,
                                  Item_multi_eq *item_equal,
                                  Item_field *left_field, int left_table_idx,
                                  Item_field *right_field, int right_table_idx,
                                  double selectivity, JoinHypergraph *graph) {
  const int left_node_idx = graph->table_num_to_node_num[left_table_idx];
  const int right_node_idx = graph->table_num_to_node_num[right_table_idx];

  // See if there is already an edge between these two tables. Since the tables
  // are in the same companion set, they are not outerjoined to each other, so
  // it's enough to check the simple neighborhood. They could already be
  // connected through complex edges due to hyperpredicates, but in this case we
  // still want to add a simple edge, as it could in some cases be advantageous
  // to join along the simple edge before applying the hyperpredicate.
  RelationalExpression *expr = nullptr;
  if (IsSubset(TableBitmap(right_node_idx),
               graph->graph.nodes[left_node_idx].simple_neighborhood)) {
    for (int edge_idx : graph->graph.nodes[left_node_idx].simple_edges) {
      if (graph->graph.edges[edge_idx].right == TableBitmap(right_node_idx)) {
        expr = graph->edges[edge_idx / 2].expr;
        if (MultipleEqualityAlreadyExistsOnJoin(item_equal, *expr)) {
          return;
        }
        graph->edges[edge_idx / 2].selectivity *= selectivity;
        break;
      }
    }
    assert(expr != nullptr);
  } else {
    // There was none, so create a new one.
    graph->graph.AddEdge(TableBitmap(left_node_idx),
                         TableBitmap(right_node_idx));
    expr = new (thd->mem_root) RelationalExpression(thd);
    expr->type = RelationalExpression::INNER_JOIN;

    // TODO(sgunders): This does not really make much sense, but
    // estimated_bytes_per_row doesn't make that much sense to begin with;
    // it will depend on the join order. See if we can replace it with a
    // per-table width calculation that we can sum up in the join
    // optimizer.
    expr->tables_in_subtree =
        TableBitmap(left_table_idx) | TableBitmap(right_table_idx);
    expr->nodes_in_subtree =
        TableBitmap(left_node_idx) | TableBitmap(right_node_idx);

    expr->companion_set = companion_collection.Find(expr->tables_in_subtree);

    const size_t estimated_bytes_per_row =
        EstimateRowWidthForJoin(*graph, expr);
    graph->edges.push_back(JoinPredicate{expr, selectivity,
                                         estimated_bytes_per_row,
                                         /*functional_dependencies=*/0,
                                         /*functional_dependencies_idx=*/{}});
  }

  Item_func_eq *eq_item = MakeEqItem(left_field, right_field, item_equal);
  expr->equijoin_conditions.push_back(
      eq_item);  // NOTE: We run after MakeHashJoinConditions().

  // Make this predicate potentially sargable.
  graph->nodes[left_node_idx].AddPushable(eq_item);
  graph->nodes[right_node_idx].AddPushable(eq_item);
}

/**
  For each relevant multiple equality, add edges so that there are direct
  connections between all the involved tables (full mesh). The tables must
  all be in the same companion set (ie., no outer joins in the way).

  Must run after equijoin conditions are extracted. _Should_ be run after
  trivial conditions have been removed.
 */
void CompleteFullMeshForMultipleEqualities(
    THD *thd, const Mem_root_array<Item_multi_eq *> &multiple_equalities,
    CompanionSetCollection &companion_collection, JoinHypergraph *graph) {
  for (Item_multi_eq *item_equal : multiple_equalities) {
    double selectivity = EstimateSelectivity(
        thd, item_equal, *companion_collection.Find(item_equal->used_tables()));

    for (Item_field &left_field : item_equal->get_fields()) {
      const int left_table_idx = left_field.m_table_ref->tableno();
      for (Item_field &right_field : item_equal->get_fields()) {
        const int right_table_idx = right_field.m_table_ref->tableno();
        if (right_table_idx <= left_table_idx) {
          continue;
        }

        AddMultipleEqualityPredicate(thd, companion_collection, item_equal,
                                     &left_field, left_table_idx, &right_field,
                                     right_table_idx, selectivity, graph);
      }
    }
  }
}

/**
  Returns a map of all tables that are on the inner side of some outer join or
  antijoin.
 */
table_map GetTablesInnerToOuterJoinOrAntiJoin(
    const RelationalExpression *expr) {
  switch (expr->type) {
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::SEMIJOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      return GetTablesInnerToOuterJoinOrAntiJoin(expr->left) |
             GetTablesInnerToOuterJoinOrAntiJoin(expr->right);
    case RelationalExpression::LEFT_JOIN:
    case RelationalExpression::ANTIJOIN:
      return GetTablesInnerToOuterJoinOrAntiJoin(expr->left) |
             expr->right->tables_in_subtree;
    case RelationalExpression::FULL_OUTER_JOIN:
      return expr->tables_in_subtree;
    case RelationalExpression::MULTI_INNER_JOIN:
      assert(false);  // Should have been unflattened by now.
      return 0;
    case RelationalExpression::TABLE:
      return 0;
  }
  assert(false);
  return 0;
}

/**
  Fully expand a multiple equality for a single table as simple equalities and
  append each equality to the array of conditions. Only expected to be called on
  multiple equalities that do not have an already known value, as such
  equalities should be eliminated by constant folding instead of being expanded.
 */
bool ExpandMultipleEqualsForSingleTable(Item_multi_eq *equal,
                                        Mem_root_array<Item *> *conditions) {
  assert(!equal->const_item());
  assert(has_single_bit(equal->used_tables() & ~PSEUDO_TABLE_BITS));
  Item *const_arg = equal->const_arg();
  if (const_arg != nullptr) {
    for (Item_field &field : equal->get_fields()) {
      if (conditions->push_back(MakeEqItem(&field, const_arg, equal))) {
        return true;
      }
    }
  } else {
    Item_field *prev = nullptr;
    for (Item_field &field : equal->get_fields()) {
      if (prev != nullptr) {
        if (conditions->push_back(MakeEqItem(prev, &field, equal))) {
          return true;
        }
      }
      prev = &field;
    }
  }
  return false;
}

/**
  Extract all WHERE conditions in a single-table query. Multiple equalities are
  fully expanded unconditionally, since there is only one way to expand them
  when there is only a single table (no need to consider that they should be
  pushable to joins). Normalization will also be performed if necessary.
 */
bool ExtractWhereConditionsForSingleTable(THD *thd, Item *condition,
                                          Mem_root_array<Item *> *conditions,
                                          bool *where_is_always_false) {
  bool need_normalization = false;
  if (WalkConjunction(condition, [conditions, &need_normalization](Item *cond) {
        if (IsMultipleEquals(cond)) {
          Item_multi_eq *equal = down_cast<Item_multi_eq *>(cond);
          if (equal->const_item()) {
            // This equality is known to evaluate to a constant value. Don't
            // expand it, but rather let constant folding remove it. Flag that
            // normalization is needed, so that constant folding kicks in.
            need_normalization = true;
            return conditions->push_back(equal);
          } else {
            // Expand the multiple equality. Normalization does not do anything
            // useful if all conditions are multiple equalities.
            return ExpandMultipleEqualsForSingleTable(equal, conditions);
          }
        } else {
          // Some other kind of condition. We might be able to simplify it in
          // normalization, so flag that we need normalization.
          need_normalization = true;
          return ExtractConditions(
              EarlyExpandMultipleEquals(cond, TablesBetween(0, MAX_TABLES)),
              conditions);
        }
      })) {
    return true;
  }

  if (need_normalization) {
    if (EarlyNormalizeConditions(thd, /*join=*/nullptr, conditions,
                                 where_is_always_false)) {
      return true;
    }
  }

  return false;
}

/// Fast path for MakeJoinHypergraph() when the query accesses a single table or
/// no table.
bool MakeSingleTableHypergraph(THD *thd, const Query_block *query_block,
                               JoinHypergraph *graph,
                               bool *where_is_always_false) {
  RelationalExpression *root = nullptr;
  if (Table_ref *const table_ref = query_block->leaf_tables;
      table_ref != nullptr) {
    assert(table_ref->next_leaf == nullptr);
    if (const int error = table_ref->fetch_number_of_rows(kRowEstimateFallback);
        error) {
      table_ref->table->file->print_error(error, MYF(0));
      return true;
    }
    root = MakeRelationalExpression(thd, query_block, table_ref);
    MakeJoinGraphFromRelationalExpression(thd, root, graph);
  }

  if (Item *const where_cond = query_block->join->where_cond;
      where_cond != nullptr) {
    Mem_root_array<Item *> where_conditions(thd->mem_root);
    if (ExtractWhereConditionsForSingleTable(thd, where_cond, &where_conditions,
                                             where_is_always_false)) {
      return true;
    }

    for (Item *item : where_conditions) {
      AddPredicate(thd, item, /*was_join_condition=*/false,
                   /*source_multiple_equality_idx=*/-1, root,
                   /*companion_collection=*/nullptr, graph);
    }
    graph->num_where_predicates = graph->predicates.size();

    SortPredicates(graph->predicates.begin(), graph->predicates.end());
  }

  if (TraceStarted(thd)) {
    Trace(thd) << "\nConstructed hypergraph:\n" << PrintDottyHypergraph(*graph);
  }

  return false;
}

void FindLateralDependencies(JoinHypergraph *graph) {
  for (JoinHypergraph::Node &node : graph->nodes) {
    assert(node.lateral_dependencies() == 0);  // Not set yet.
    const Table_ref *const table_ref = node.table()->pos_in_table_list;
    table_map deps = 0;
    if (table_ref->is_derived()) {
      deps = table_ref->derived_query_expression()->m_lateral_deps;
    } else if (table_ref->is_table_function()) {
      deps = table_ref->table_function->used_tables();
    } else {
      continue;
    }
    node.set_lateral_dependencies(GetNodeMapFromTableMap(
        deps & ~PSEUDO_TABLE_BITS, graph->table_num_to_node_num));
  }
}

}  // namespace

const JOIN *JoinHypergraph::join() const { return m_query_block->join; }

bool MakeJoinHypergraph(THD *thd, JoinHypergraph *graph,
                        bool *where_is_always_false) {
  const Query_block *query_block = graph->query_block();

  if (TraceStarted(thd)) {
    // TODO(sgunders): Do we want to keep this in the trace indefinitely?
    // It's only useful for debugging, not as much for understanding what's
    // going on.
    Trace(thd) << "Join list after simplification:\n"
               << PrintJoinList(query_block->m_table_nest, /*level=*/0) << "\n";
  }

  const size_t num_tables = query_block->leaf_table_count;
  if (graph->nodes.reserve(num_tables) ||
      graph->graph.nodes.reserve(num_tables)) {
    return true;
  }

  // Fast path for single-table queries. We can skip all the logic that analyzes
  // join conditions, as there is no join.
  if (num_tables <= 1) {
    return MakeSingleTableHypergraph(thd, query_block, graph,
                                     where_is_always_false);
  }

  RelationalExpression *root = MakeRelationalExpressionFromJoinList(
      thd, query_block, query_block->m_table_nest, /*toplevel=*/true);

  CompanionSetCollection companion_collection(thd, root);
  FlattenInnerJoins(root);

  const JOIN *join = query_block->join;
  if (TraceStarted(thd)) {
    // TODO(sgunders): Same question as above; perhaps the version after
    // pushdown is sufficient.
    Trace(thd) << StringPrintf(
                      "Made this relational tree; WHERE condition is %s:\n",
                      ItemToString(join->where_cond).c_str())
               << PrintRelationalExpression(root, 0) << "\n";
  }

  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf("Pushing conditions down.\n");
  }

  Mem_root_array<Item *> table_filters(thd->mem_root);
  Mem_root_array<Item *> cycle_inducing_edges(thd->mem_root);
  PushDownJoinConditions(thd, root, companion_collection, &table_filters,
                         &cycle_inducing_edges);

  // Split up WHERE conditions, and push them down into the tree as much as
  // we can. (They have earlier been hoisted up as far as possible; see
  // comments on PushDownAsMuchAsPossible() and PushDownJoinConditions().)
  // Note that we do this after pushing down join conditions, so that we
  // don't push down WHERE conditions to join conditions and then re-process
  // them later.
  Mem_root_array<Item *> where_conditions(thd->mem_root);
  if (join->where_cond != nullptr) {
    Item *where_cond = EarlyExpandMultipleEquals(join->where_cond,
                                                 /*tables_in_subtree=*/~0);
    if (ExtractConditions(where_cond, &where_conditions)) {
      return true;
    }
    if (EarlyNormalizeConditions(thd, /*join=*/nullptr, &where_conditions,
                                 where_is_always_false)) {
      return true;
    }
    ReorderConditions(&where_conditions);
    where_conditions = PushDownAsMuchAsPossible(
        thd, std::move(where_conditions), root,
        /*is_join_condition_for_expr=*/false, companion_collection,
        &table_filters, &cycle_inducing_edges);

    // We're done pushing, so unflatten so that the rest of the algorithms
    // don't need to worry about it.
    UnflattenInnerJoins(root);

    if (CanonicalizeConditions(thd, GetVisibleTables(root),
                               TablesBetween(0, MAX_TABLES),
                               &where_conditions)) {
      return true;
    }

    // NOTE: Any remaining WHERE conditions, whether single-table or multi-table
    // (join conditions), are left up here for a reason (i.e., they are
    // nondeterministic and/or blocked by outer joins), so they should not be
    // attempted pushed as sargable predicates.
  } else {
    // We're done pushing, so unflatten so that the rest of the algorithms
    // don't need to worry about it.
    UnflattenInnerJoins(root);
  }

  // Now that everything is pushed, we can concretize any multiple equalities
  // that are left on antijoins and semijoins.
  LateConcretizeMultipleEqualities(thd, root);

  // Now see if we can push down join conditions to sargable predicates.
  // We do this after we're done pushing, since pushing can change predicates
  // (in particular, it can concretize multiple equalities).
  PushDownJoinConditionsForSargable(thd, root);

  if (CanonicalizeJoinConditions(thd, root)) {
    return true;
  }
  FindConditionsUsedTables(thd, root);
  MakeHashJoinConditions(thd, root);

  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf(
                      "\nAfter pushdown; remaining WHERE conditions are %s, "
                      "table filters are %s:\n",
                      ItemsToString(where_conditions).c_str(),
                      ItemsToString(table_filters).c_str())
               << PrintRelationalExpression(root, 0) << '\n';
  }

  // Ask the storage engine to update stats.records, if needed.
  // We need to do this before MakeJoinGraphFromRelationalExpression(),
  // which determines selectivities that are in part based on it.
  // NOTE: ha_archive breaks without this call! (That is probably a bug in
  // ha_archive, though.)
  for (Table_ref *tl = graph->query_block()->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (const int error = tl->fetch_number_of_rows(kRowEstimateFallback);
        error) {
      tl->table->file->print_error(error, MYF(0));
      return true;
    }
  }

  // Build sets of equal fields in each CompanionSet.
  ForEachOperator(root, [&](RelationalExpression *expr) {
    if (expr->type == RelationalExpression::TABLE) {
      for (const Item *condition : expr->pushable_conditions()) {
        if (is_function_of_type(condition, Item_func::EQ_FUNC)) {
          expr->companion_set->AddEquijoinCondition(
              thd, down_cast<const Item_func_eq &>(*condition));
        }
      }
    } else {
      for (const Item_eq_base *condition : expr->equijoin_conditions) {
        if (condition->functype() == Item_func::EQ_FUNC) {
          expr->companion_set->AddEquijoinCondition(
              thd, down_cast<const Item_func_eq &>(*condition));
        }
      }
    }
  });

  if (TraceStarted(thd)) {
    Trace(thd) << companion_collection.ToString();
  }

  // Construct the hypergraph from the relational expression.
#ifndef NDEBUG
  std::fill(begin(graph->table_num_to_node_num),
            end(graph->table_num_to_node_num), -1);
#endif
  MakeJoinGraphFromRelationalExpression(thd, root, graph);
  FindLateralDependencies(graph);

  // Now that we have the hypergraph construction done, it no longer hurts
  // to remove impossible conditions.
  ClearImpossibleJoinConditions(root);

  graph->tables_inner_to_outer_or_anti =
      GetTablesInnerToOuterJoinOrAntiJoin(root);

  // Add cycles.
  size_t old_graph_edges = graph->graph.edges.size();
  if (!cycle_inducing_edges.empty()) {
    AddCycleEdges(thd, cycle_inducing_edges, companion_collection, graph);
  }
  // Now that all trivial conditions have been removed and all equijoin
  // conditions extracted, go ahead and extract all the multiple
  // equalities that are in actual use, and present as part of the base
  // conjunctions (ie., not OR-ed with anything).
  Mem_root_array<Item_multi_eq *> multiple_equalities(thd->mem_root);
  ExtractCycleMultipleEqualitiesFromJoinConditions(root, companion_collection,
                                                   &multiple_equalities);
  ExtractCycleMultipleEqualities(where_conditions, companion_collection,
                                 &multiple_equalities);
  if (multiple_equalities.size() > 64) {
    multiple_equalities.resize(64);
  }
  std::sort(multiple_equalities.begin(), multiple_equalities.end());
  multiple_equalities.erase(
      std::unique(multiple_equalities.begin(), multiple_equalities.end()),
      multiple_equalities.end());
  CompleteFullMeshForMultipleEqualities(thd, multiple_equalities,
                                        companion_collection, graph);
  if (graph->graph.edges.size() != old_graph_edges) {
    // We added at least one cycle-inducing edge.
    PromoteCycleJoinPredicates(thd, root, multiple_equalities,
                               companion_collection, graph);
  }

  if (TraceStarted(thd)) {
    Trace(thd) << "\nConstructed hypergraph:\n" << PrintDottyHypergraph(*graph);

    if (DEBUGGING_DPHYP) {
      // DPhyp printouts talk mainly about R1, R2, etc., so if debugging
      // the algorithm, it is useful to have a link to the table names.
      Trace(thd) << "Node mappings, for reference:\n";
      for (size_t i = 0; i < graph->nodes.size(); ++i) {
        Trace(thd) << StringPrintf("  R%zu = %s\n", i + 1,
                                   graph->nodes[i].table()->alias);
      }
    }
    Trace(thd) << "\n";
  }

#ifndef NDEBUG
  {
    // Verify we have no duplicate edges.
    const Mem_root_array<Hyperedge> &edges = graph->graph.edges;
    for (size_t edge1_idx = 0; edge1_idx < edges.size(); ++edge1_idx) {
      for (size_t edge2_idx = edge1_idx + 1; edge2_idx < edges.size();
           ++edge2_idx) {
        const Hyperedge &e1 = edges[edge1_idx];
        const Hyperedge &e2 = edges[edge2_idx];
        assert(e1.left != e2.left || e1.right != e2.right);
      }
    }
  }

#endif

  // The predicates added so far are join conditions that have been promoted to
  // WHERE predicates by PromoteCycleJoinPredicates().
  const size_t num_cycle_predicates = graph->predicates.size();

  // Find TES and selectivity for each WHERE predicate that was not pushed
  // down earlier.
  for (Item *condition : where_conditions) {
    AddPredicate(thd, condition, /*was_join_condition=*/false,
                 /*source_multiple_equality_idx=*/-1, root,
                 &companion_collection, graph);
  }

  // Table filters should be applied at the bottom, without extending the TES.
  for (Item *condition : table_filters) {
    Predicate pred;
    pred.condition = condition;
    pred.used_nodes = pred.total_eligibility_set = GetNodeMapFromTableMap(
        condition->used_tables() & ~(INNER_TABLE_BIT | OUTER_REF_TABLE_BIT),
        graph->table_num_to_node_num);
    assert(has_single_bit(pred.total_eligibility_set));
    pred.selectivity = EstimateSelectivity(
        thd, condition, *companion_collection.Find(condition->used_tables()));
    pred.functional_dependencies_idx.init(thd->mem_root);
    graph->predicates.push_back(std::move(pred));
  }

  // Sort the predicates so that filters created from them later automatically
  // evaluate the most selective and least expensive predicates first. Don't
  // touch the join (cycle) predicates at the beginning, as they are already
  // sorted, and reordering them would make the join_predicate_first and
  // join_predicate_last pointers in the corresponding RelationalExpression
  // incorrect.
  SortPredicates(graph->predicates.begin() + num_cycle_predicates,
                 graph->predicates.end());

  graph->num_where_predicates = graph->predicates.size();

  return false;
}

// Returns the tables in this subtree that are visible higher up in
// the join tree. This includes all tables in this subtree, except
// those that are on the inner side of a semijoin or an antijoin.
table_map GetVisibleTables(const RelationalExpression *expr) {
  switch (expr->type) {
    case RelationalExpression::TABLE:
      return expr->tables_in_subtree;
    case RelationalExpression::SEMIJOIN:
    case RelationalExpression::ANTIJOIN:
      // Inner side of a semijoin or an antijoin should not
      // be visible outside of the join.
      return GetVisibleTables(expr->left);
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
    case RelationalExpression::LEFT_JOIN:
    case RelationalExpression::FULL_OUTER_JOIN:
      return GetVisibleTables(expr->left) | GetVisibleTables(expr->right);
    case RelationalExpression::MULTI_INNER_JOIN:
      return std::accumulate(
          expr->multi_children.begin(), expr->multi_children.end(),
          table_map{0},
          [](table_map tables, const RelationalExpression *child) {
            return tables | GetVisibleTables(child);
          });
  }
  assert(false);
  return 0;
}
