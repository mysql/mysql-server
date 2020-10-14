/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/make_join_hypergraph.h"

#include <assert.h>
#include <stddef.h>
#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "limits.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/nested_join.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "template_utils.h"

using hypergraph::Hyperedge;
using hypergraph::Hypergraph;
using hypergraph::NodeMap;
using std::array;
using std::string;
using std::vector;

namespace {

RelationalExpression *MakeRelationalExpressionFromJoinList(
    THD *thd, const mem_root_deque<TABLE_LIST *> &join_list);

RelationalExpression *MakeRelationalExpression(THD *thd, const TABLE_LIST *tl) {
  if (tl->nested_join == nullptr) {
    // A single table.
    RelationalExpression *ret = new (thd->mem_root) RelationalExpression(thd);
    ret->type = RelationalExpression::TABLE;
    ret->table = tl;
    ret->tables_in_subtree = tl->map();
    return ret;
  } else {
    // A join or multijoin.
    return MakeRelationalExpressionFromJoinList(thd,
                                                tl->nested_join->join_list);
  }
}

/**
  Convert the SELECT_LEX's join lists into a RelationalExpression,
  ie., a join tree with tables at the leaves.
 */
RelationalExpression *MakeRelationalExpressionFromJoinList(
    THD *thd, const mem_root_deque<TABLE_LIST *> &join_list) {
  assert(!join_list.empty());
  RelationalExpression *ret = nullptr;
  for (auto it = join_list.rbegin(); it != join_list.rend();
       ++it) {  // The list goes backwards.
    const TABLE_LIST *tl = *it;
    if (ret == nullptr) {
      // The first table in the list.
      ret = MakeRelationalExpression(thd, tl);
      continue;
    }

    RelationalExpression *join = new (thd->mem_root) RelationalExpression(thd);
    join->left = ret;
    if (tl->is_sj_or_aj_nest()) {
      join->right =
          MakeRelationalExpressionFromJoinList(thd, tl->nested_join->join_list);
      join->type = tl->is_sj_nest() ? RelationalExpression::SEMIJOIN
                                    : RelationalExpression::ANTIJOIN;
    } else {
      join->right = MakeRelationalExpression(thd, tl);
      join->type = tl->outer_join ? RelationalExpression::LEFT_JOIN
                                  : RelationalExpression::INNER_JOIN;
    }
    if (tl->is_aj_nest()) {
      assert(tl->join_cond() != nullptr);
    }
    if (tl->join_cond() != nullptr) {
      ExtractConditions(tl->join_cond(), &join->join_conditions);
    }
    join->tables_in_subtree =
        join->left->tables_in_subtree | join->right->tables_in_subtree;
    ret = join;
  }
  return ret;
}

string PrintRelationalExpression(RelationalExpression *expr, int level) {
  string result;
  for (int i = 0; i < level * 2; ++i) result += ' ';

  switch (expr->type) {
    case RelationalExpression::TABLE:
      result += StringPrintf("* %s\n", expr->table->alias);
      // Do not try to descend further.
      return result;
    case RelationalExpression::CARTESIAN_PRODUCT:
      result += "* Cartesian product";
      break;
    case RelationalExpression::INNER_JOIN:
      result += "* Inner join";
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
  }
  if (expr->type != RelationalExpression::CARTESIAN_PRODUCT) {
    if (expr->equijoin_conditions.empty() && expr->join_conditions.empty()) {
      result += " (no join conditions)";
    } else if (!expr->equijoin_conditions.empty()) {
      result += StringPrintf(" (equijoin condition = %s)",
                             ItemsToString(expr->equijoin_conditions).c_str());
    } else if (!expr->join_conditions.empty()) {
      result += StringPrintf(" (extra join condition = %s)",
                             ItemsToString(expr->join_conditions).c_str());
    } else {
      result += StringPrintf(" (equijoin condition = %s, extra = %s)",
                             ItemsToString(expr->equijoin_conditions).c_str(),
                             ItemsToString(expr->join_conditions).c_str());
    }
  }
  result += '\n';

  result += PrintRelationalExpression(expr->left, level + 1);
  result += PrintRelationalExpression(expr->right, level + 1);
  return result;
}

/**
  Go through all inner joins that have no (non-degenerate) join conditions,
  and mark them as Cartesian products. This is currently mostly for display
  purposes, but it will be important for proper conflict detection later.
 */
void MakeCartesianProducts(RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }

  if (expr->type == RelationalExpression::INNER_JOIN &&
      expr->equijoin_conditions.empty()) {
    // See if any of the non-equijoin conditions are non-degenerate.
    bool any_join_condition = false;
    for (Item *cond : expr->join_conditions) {
      if (Overlaps(cond->used_tables(), expr->left->tables_in_subtree) &&
          Overlaps(cond->used_tables(), expr->right->tables_in_subtree)) {
        any_join_condition = true;
        break;
      }
    }
    if (!any_join_condition) {
      expr->type = RelationalExpression::CARTESIAN_PRODUCT;
    }
  }

  // Recurse further down into the tree.
  MakeCartesianProducts(expr->left);
  MakeCartesianProducts(expr->right);
}

/**
  Try to push down the condition “cond” down in the join tree given by “expr”,
  as far as possible. cond is either a join condition on expr
  (is_join_condition_for_expr=true), or a filter which is applied at some point
  after expr (...=false).

  Returns false if cond was pushed down and stored as a join condition on some
  lower place than it started, ie., the caller no longer needs to worry about
  it.

  In addition to regular pushdown, PushDownCondition() will do partial pushdown
  if appropriate. Some expressions cannot be fully pushed down, but we can
  push down necessary-but-not-sufficient conditions to get earlier filtering.
  (This is a performance win for e.g. hash join and the left side of a
  nested loop join, but not for the right side of a nested loop join. Note that
  we currently do not compensate for the errors in selectivity estimation
  this may incur.) An example would be

    (t1.x = 1 AND t2.y=2) OR (t1.x = 3 AND t2.y=4);

  we could push down the conditions (t1.x = 1 OR t1.x = 3) to t1 and similarly
  for t2, but we could not delete the original condition. This does not affect
  the return value. Since PushDownAsMuchAsPossible() only calls us for join
  conditions, this is the only way we can push down something onto a single
  table (which naturally has no concept of “join condition”). If this happens,
  we push the resulting condition(s) onto “extra_where_conditions”.
 */
bool PushDownCondition(Item *cond, RelationalExpression *expr,
                       bool is_join_condition_for_expr,
                       Mem_root_array<Item *> *extra_where_conditions) {
  if (expr->type == RelationalExpression::TABLE) {
    extra_where_conditions->push_back(cond);
    return true;
  }

  assert(
      !Overlaps(expr->left->tables_in_subtree, expr->right->tables_in_subtree));

  table_map used_tables =
      cond->used_tables() & ~(OUTER_REF_TABLE_BIT | INNER_TABLE_BIT);

  // See if we can push down into the left side, ie., it only touches
  // tables on the left side of the join.
  //
  // If the condition is a filter, we can do this for all join types
  // except FULL OUTER JOIN, which we don't support yet. If it's a join
  // condition for this join, we cannot push it for outer joins and
  // antijoins, since that would remove rows that should otherwise
  // be output (as NULL-complemented ones in the case if outer joins).
  if (IsSubset(used_tables, expr->left->tables_in_subtree)) {
    if (expr->type != RelationalExpression::INNER_JOIN &&
        expr->type != RelationalExpression::SEMIJOIN &&
        is_join_condition_for_expr) {
      return true;
    }
    return PushDownCondition(cond, expr->left,
                             /*is_join_condition_for_expr=*/false,
                             extra_where_conditions);
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
  // (The user cannot cannot specify semijoins directly, so all such conditions
  // come from ourselves.)
  bool can_push_into_right = (expr->type == RelationalExpression::INNER_JOIN ||
                              expr->type == RelationalExpression::SEMIJOIN ||
                              is_join_condition_for_expr);
  if (IsSubset(used_tables, expr->right->tables_in_subtree)) {
    if (!can_push_into_right) {
      return true;
    }
    return PushDownCondition(cond, expr->right,
                             /*is_join_condition_for_expr=*/false,
                             extra_where_conditions);
  }

  // It's not a subset of left, it's not a subset of right, so it's a
  // filter that must either stay after this join, or it can be promoted
  // to a join condition for it.

  // Try partial pushdown into the left side (see function comment).
  {
    Item *partial_cond = make_cond_for_table(
        current_thd, cond, expr->left->tables_in_subtree, /*used_table=*/0,
        /*exclude_expensive_cond=*/true);
    if (partial_cond != nullptr) {
      PushDownCondition(partial_cond, expr->left,
                        /*is_join_condition_for_expr=*/false,
                        extra_where_conditions);
    }
  }

  // Then the right side, if it's allowed.
  if (can_push_into_right) {
    Item *partial_cond = make_cond_for_table(
        current_thd, cond, expr->right->tables_in_subtree, /*used_table=*/0,
        /*exclude_expensive_cond=*/true);
    if (partial_cond != nullptr) {
      PushDownCondition(partial_cond, expr->right,
                        /*is_join_condition_for_expr=*/false,
                        extra_where_conditions);
    }
  }

  // Now that any partial pushdown has been done, see if we can promote
  // the original filter to a join condition.
  if (is_join_condition_for_expr) {
    // We were already a join condition on this join, so there's nothing to do.
    return true;
  }

  // We cannot promote filters to join conditions for outer joins
  // and antijoins, but we can on inner joins and semijoins.
  if (expr->type == RelationalExpression::LEFT_JOIN ||
      expr->type == RelationalExpression::ANTIJOIN) {
    return true;
  }

  // Promote the filter to a join condition on this join.
  // If it's an equijoin condition, MakeHashJoinConditions() will convert it to
  // one (in expr->equijoin_conditions) when it runs later.
  assert(expr->equijoin_conditions.empty());
  expr->join_conditions.push_back(cond);
  return false;
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
    Mem_root_array<Item *> *extra_where_conditions) {
  Mem_root_array<Item *> remaining_parts(thd->mem_root);
  for (Item *item : conditions) {
    if (IsSingleBitSet(item->used_tables() & ~PSEUDO_TABLE_BITS)) {
      // Only push down join conditions, not filters; they will stay in WHERE,
      // as we handle them separately in FoundSingleNode() and
      // FoundSubgraphPair().
      remaining_parts.push_back(item);
    } else {
      if (PushDownCondition(item, expr, is_join_condition_for_expr,
                            extra_where_conditions)) {
        // Pushdown failed.
        remaining_parts.push_back(item);
      }
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
                            Mem_root_array<Item *> *where_conditions) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  if (!expr->join_conditions.empty()) {
    expr->join_conditions = PushDownAsMuchAsPossible(
        thd, std::move(expr->join_conditions), expr,
        /*is_join_condition_for_expr=*/true, where_conditions);
  }
  PushDownJoinConditions(thd, expr->left, where_conditions);
  PushDownJoinConditions(thd, expr->right, where_conditions);
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
      if ((item->used_tables() & expr->left->tables_in_subtree) &&
          (item->used_tables() & expr->right->tables_in_subtree) &&
          (item->type() == Item::FUNC_ITEM ||
           item->type() == Item::COND_ITEM)) {
        Item_func *func_item = down_cast<Item_func *>(item);
        if (func_item->contains_only_equi_join_condition()) {
          Item_func_eq *join_condition = down_cast<Item_func_eq *>(func_item);
          // Join conditions with items that returns row values (subqueries or
          // row value expression) are set up with multiple child comparators,
          // one for each column in the row. As long as the row contains only
          // one column, use it as a join condition. If it has more than one
          // column, attach it as an extra condition. Note that join
          // conditions that does not return row values are not set up with
          // any child comparators, meaning that get_child_comparator_count()
          // will return 0.
          if (join_condition->get_comparator()->get_child_comparator_count() <
              2) {
            expr->equijoin_conditions.push_back(
                down_cast<Item_func_eq *>(func_item));
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

/**
  Convert multi-equalities to simple equalities. This is a hack until we get
  real handling of multi-equalities (in which case it would be done much later,
  after the join order has been determined); however, note that
  remove_eq_conds() also does some constant conversion/folding work that is
  important for correctness in general.
 */
bool ConcretizeMultipleEquals(THD *thd, Mem_root_array<Item *> *conditions) {
  for (auto it = conditions->begin(); it != conditions->end();) {
    Item::cond_result res;
    if (remove_eq_conds(thd, *it, &*it, &res)) {
      return true;
    }

    if (res == Item::COND_TRUE) {
      it = conditions->erase(it);
    } else if (res == Item::COND_FALSE) {
      conditions->clear();
      conditions->push_back(new Item_int(0));
      return false;
    } else {
      ++it;
    }
  }
  return false;
}

/**
  Convert all multi-equalities in join conditions under “expr” into simple
  equalities. See ConcretizeMultipleEquals() for more information.
 */
bool ConcretizeAllMultipleEquals(THD *thd, RelationalExpression *expr,
                                 Mem_root_array<Item *> *where_conditions) {
  if (expr->type == RelationalExpression::TABLE) {
    return false;
  }
  assert(expr->equijoin_conditions
             .empty());  // MakeHashJoinConditions() has not run yet.
  if (ConcretizeMultipleEquals(thd, &expr->join_conditions)) {
    return true;
  }
  PushDownJoinConditions(thd, expr->left, where_conditions);
  PushDownJoinConditions(thd, expr->right, where_conditions);
  return false;
}

string PrintJoinList(const mem_root_deque<TABLE_LIST *> &join_list, int level) {
  string str;
  const char *join_types[] = {"inner", "left", "right"};
  std::vector<TABLE_LIST *> list(join_list.begin(), join_list.end());
  for (TABLE_LIST *tbl : list) {
    for (int i = 0; i < level * 2; ++i) str += ' ';
    if (tbl->join_cond() != nullptr) {
      str += StringPrintf("* %s %s  join_type=%s\n", tbl->alias,
                          ItemToString(tbl->join_cond()).c_str(),
                          join_types[tbl->outer_join]);
    } else {
      str += StringPrintf("* %s  join_type=%s\n", tbl->alias,
                          join_types[tbl->outer_join]);
    }
    if (tbl->nested_join != nullptr) {
      str += PrintJoinList(tbl->nested_join->join_list, level + 1);
    }
  }
  return str;
}

NodeMap GetNodeMapFromTableMap(
    table_map table_map, const array<int, MAX_TABLES> &table_num_to_node_num) {
  NodeMap ret = 0;
  for (int table_num : BitsSetIn(table_map)) {
    assert(table_num_to_node_num[table_num] != -1);
    ret |= TableBitmap(table_num_to_node_num[table_num]);
  }
  return ret;
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
                              RelationalExpression *expr) {
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
      tes |= FindTESForCondition(used_tables, expr->right);

      // The predicate needs a table from the right-hand side, but this join can
      // cause that table to become NULL, so we need to delay until the join has
      // happened. Notwithstanding any reordering on the left side, the join
      // cannot happen until all the join condition's used tables are in place,
      // so for non-degenerate conditions, that is a neccessary and sufficient
      // condition for the predicate to be applied.
      for (Item *condition : expr->equijoin_conditions) {
        tes |= condition->used_tables();
      }
      for (Item *condition : expr->join_conditions) {
        tes |= condition->used_tables();
      }

      // If all conditions were degenerate (and not left-degenerate, ie.,
      // referenced the left-hand side only), simply add all tables from the
      // left-hand side as required, so that it will not be pushed into the
      // right-hand side in any case.
      if (!Overlaps(tes, expr->left->tables_in_subtree)) {
        tes |= expr->left->tables_in_subtree;
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

/**
  Returns whether there are only inner joins in the join tree under “expr”.
 */
bool ConsistsOfInnerJoinsOnly(const RelationalExpression *expr) {
  if (expr->type == RelationalExpression::TABLE) {
    return true;
  }
  if (expr->type != RelationalExpression::INNER_JOIN &&
      expr->type != RelationalExpression::CARTESIAN_PRODUCT) {
    return false;
  }
  return ConsistsOfInnerJoinsOnly(expr->left) &&
         ConsistsOfInnerJoinsOnly(expr->right);
}

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
  for (size_t edge_idx = 0; edge_idx < graph.graph.edges.size();
       edge_idx += 2) {
    const Hyperedge &e = graph.graph.edges[edge_idx];
    string label = GenerateExpressionLabel(graph.edges[edge_idx / 2].expr);
    if (IsSingleBitSet(e.left) && IsSingleBitSet(e.right)) {
      // Simple edge.
      int left_node = FindLowestBitSet(e.left);
      int right_node = FindLowestBitSet(e.right);
      digraph += StringPrintf("  %s -> %s [label=\"%s\"]\n",
                              graph.nodes[left_node]->alias,
                              graph.nodes[right_node]->alias, label.c_str());
    } else {
      // Hyperedge; draw it as a tiny “virtual node”.
      digraph += StringPrintf(
          "  e%zu [shape=circle,width=.001,height=.001,label=\"\"]\n",
          edge_idx);

      // Print the label only once.
      string left_label, right_label;
      if (IsSingleBitSet(e.right) && !IsSingleBitSet(e.left)) {
        right_label = label;
      } else {
        left_label = label;
      }

      // Left side of the edge.
      for (int left_node : BitsSetIn(e.left)) {
        digraph += StringPrintf("  %s -> e%zu [arrowhead=none,label=\"%s\"]\n",
                                graph.nodes[left_node]->alias, edge_idx,
                                left_label.c_str());
        left_label = "";
      }

      // Right side of the edge.
      for (int right_node : BitsSetIn(e.right)) {
        digraph +=
            StringPrintf("  e%zu -> %s [label=\"%s\"]\n", edge_idx,
                         graph.nodes[right_node]->alias, right_label.c_str());
        right_label = "";
      }
    }
  }
  digraph += "}\n";
  return digraph;
}

/**
  Convert a join rooted at “expr” into a join hypergraph that encapsulates
  the constraints given by the relational expressions (e.g. inner joins are
  more freely reorderable than outer joins).

  Making a hypergraph that accurately and minimally expresses the constraints
  of a given join tree is nontrivial (see “On the correct and complete
  enumeration of the core search space” by Moerkotte et al). Since this a
  prototype, we make no attempt at optimality; that will come later.
  Instead, we opt for a conservative approach, where outer joins block all
  reordering (and inner joins are freely reorderable). This keeps us from
  producing all valid join orders, but makes sure we do not create any invalid
  ones.
 */
void MakeJoinGraphFromRelationalExpression(const RelationalExpression *expr,
                                           string *trace,
                                           JoinHypergraph *graph) {
  if (expr->type == RelationalExpression::TABLE) {
    graph->graph.AddNode();
    graph->nodes.push_back(expr->table->table);
    assert(expr->table->tableno() < MAX_TABLES);
    graph->table_num_to_node_num[expr->table->tableno()] =
        graph->graph.nodes.size() - 1;
    return;
  }

  MakeJoinGraphFromRelationalExpression(expr->left, trace, graph);
  MakeJoinGraphFromRelationalExpression(expr->right, trace, graph);

  table_map used_tables = 0;
  for (Item *condition : expr->join_conditions) {
    used_tables |= condition->used_tables();
  }
  for (Item *condition : expr->equijoin_conditions) {
    used_tables |= condition->used_tables();
  }

  // Very conservative conflict detector.
  NodeMap left =
      GetNodeMapFromTableMap(used_tables & expr->left->tables_in_subtree,
                             graph->table_num_to_node_num);
  NodeMap right =
      GetNodeMapFromTableMap(used_tables & expr->right->tables_in_subtree,
                             graph->table_num_to_node_num);
  NodeMap left_full = GetNodeMapFromTableMap(expr->left->tables_in_subtree,
                                             graph->table_num_to_node_num);
  NodeMap right_full = GetNodeMapFromTableMap(expr->right->tables_in_subtree,
                                              graph->table_num_to_node_num);
  if (expr->type == RelationalExpression::INNER_JOIN) {
    // Reordering is fine, although not into parts that may contain
    // outer joins. For instance, reordering (a LEFT JOIN b) JOIN c
    // is not fine if the outermost join condition can depend on
    // a NULL-complemented row from b.
    if (!ConsistsOfInnerJoinsOnly(expr->left)) {
      left = left_full;
    }
    if (!ConsistsOfInnerJoinsOnly(expr->right)) {
      right = right_full;
    }
  } else {
    // Absolutely no reordering. (We can't even always reorder on
    // the left side, e.g. for (a LEFT JOIN b) SEMIJOIN c ON b.x=c.x,
    // we really need {a,b} on the left side, not just {b}.)
    left = left_full;
    right = right_full;
  }

  // On degenerate predicates, stop all reordering for now.
  if (left == 0 || right == 0) {
    left = left_full;
    right = right_full;
  }

  assert(left != 0);
  assert(right != 0);
  graph->graph.AddEdge(left, right);

  if (trace != nullptr) {
    *trace += StringPrintf("Selectivity of join %s:\n",
                           GenerateExpressionLabel(expr).c_str());
  }
  double selectivity = 1.0;
  for (Item *item : expr->equijoin_conditions) {
    selectivity *= EstimateSelectivity(current_thd, item, trace);
  }
  for (Item *item : expr->join_conditions) {
    selectivity *= EstimateSelectivity(current_thd, item, trace);
  }
  if (trace != nullptr &&
      expr->equijoin_conditions.size() + expr->join_conditions.size() > 1) {
    *trace += StringPrintf("  - total: %.3f\n", selectivity);
  }

  graph->edges.push_back(JoinPredicate{expr, selectivity});
}

}  // namespace

bool MakeJoinHypergraph(THD *thd, SELECT_LEX *select_lex, string *trace,
                        JoinHypergraph *graph) {
  JOIN *join = select_lex->join;
  if (trace != nullptr) {
    // TODO(sgunders): Do we want to keep this in the trace indefinitely?
    // It's only useful for debugging, not as much for understanding what's
    // going on.
    *trace += "Join list after simplification:\n";
    *trace += PrintJoinList(select_lex->top_join_list, /*level=*/0);
    *trace += "\n";
  }

  RelationalExpression *root =
      MakeRelationalExpressionFromJoinList(thd, select_lex->top_join_list);

  if (trace != nullptr) {
    // TODO(sgunders): Same question as above; perhaps the version after
    // pushdown is sufficient.
    *trace +=
        StringPrintf("Made this relational tree; WHERE condition is %s:\n",
                     ItemToString(join->where_cond).c_str());
    *trace += PrintRelationalExpression(root, 0);
    *trace += "\n";
  }

  Mem_root_array<Item *> extra_where_conditions(thd->mem_root);
  if (ConcretizeAllMultipleEquals(thd, root, &extra_where_conditions)) {
    return true;
  }
  PushDownJoinConditions(thd, root, &extra_where_conditions);

  // Split up WHERE conditions, and push them down into the tree as much as
  // we can. (They have earlier been hoisted up as far as possible; see
  // comments on PushDownAsMuchAsPossible() and PushDownJoinConditions().)
  // Note that we do this after pushing down join conditions, so that we
  // don't push down WHERE conditions to join conditions and then re-process
  // them later.
  Mem_root_array<Item *> where_conditions(thd->mem_root);
  if (join->where_cond != nullptr) {
    ExtractConditions(join->where_cond, &where_conditions);
    if (ConcretizeMultipleEquals(thd, &where_conditions)) {
      return true;
    }
    where_conditions = PushDownAsMuchAsPossible(
        thd, std::move(where_conditions), root,
        /*is_join_condition_for_expr=*/false, &extra_where_conditions);
  }

  for (Item *cond : extra_where_conditions) {
    where_conditions.push_back(cond);
  }

  MakeHashJoinConditions(thd, root);
  MakeCartesianProducts(root);

  if (trace != nullptr) {
    *trace +=
        StringPrintf("After pushdown; remaining WHERE conditions are %s:\n",
                     ItemsToString(where_conditions).c_str());
    *trace += PrintRelationalExpression(root, 0);
    *trace += '\n';
  }

  // Construct the hypergraph from the relational expression.
#ifndef DBUG_OFF
  std::fill(begin(graph->table_num_to_node_num),
            end(graph->table_num_to_node_num), -1);
#endif
  MakeJoinGraphFromRelationalExpression(root, trace, graph);

  if (trace != nullptr) {
    *trace += "\nConstructed hypergraph:\n";
    *trace += PrintDottyHypergraph(*graph);

    if (DEBUGGING_DPHYP) {
      // DPhyp printouts talk mainly about R1, R2, etc., so if debugging
      // the algorithm, it is useful to have a link to the table names.
      *trace += "Node mappings, for reference:\n";
      for (size_t i = 0; i < graph->nodes.size(); ++i) {
        *trace += StringPrintf("  R%zu = %s\n", i + 1, graph->nodes[i]->alias);
      }
    }
    *trace += "\n";
  }

  // Find TES and selectivity for each WHERE predicate that was not pushed
  // down earlier.
  for (Item *condition : where_conditions) {
    Predicate pred;
    pred.condition = condition;
    table_map total_eligibility_set =
        FindTESForCondition(condition->used_tables(), root) &
        ~(INNER_TABLE_BIT | OUTER_REF_TABLE_BIT);
    pred.total_eligibility_set =
        GetNodeMapFromTableMap(total_eligibility_set & ~RAND_TABLE_BIT,
                               graph->table_num_to_node_num) |
        (total_eligibility_set & RAND_TABLE_BIT);
    pred.selectivity = EstimateSelectivity(thd, condition, trace);
    graph->predicates.push_back(pred);

    if (trace != nullptr) {
      *trace += StringPrintf("Total eligibility set for %s: {",
                             ItemToString(condition).c_str());
      bool first = true;
      for (TABLE_LIST *tl = select_lex->leaf_tables; tl != nullptr;
           tl = tl->next_leaf) {
        if (tl->map() & total_eligibility_set) {
          if (!first) *trace += ',';
          *trace += tl->alias;
          first = false;
        }
      }
      *trace += "}\n";
    }
  }
  if (graph->predicates.size() > sizeof(table_map) * CHAR_BIT) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "more than 64 WHERE/ON predicates");
    return true;
  }

  return false;
}
