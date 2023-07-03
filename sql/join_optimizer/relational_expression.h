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

#ifndef SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H
#define SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H

#include <stdint.h>

#include "sql/item.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/sql_class.h"

struct AccessPath;
class Item_eq_base;

// Some information about each predicate that the join optimizer would like to
// have available in order to avoid computing it anew for each use of that
// predicate.
struct CachedPropertiesForPredicate {
  Mem_root_array<ContainedSubquery> contained_subqueries;
  double selectivity;

  // For equijoins only: A bitmap of which sargable predicates
  // are part of the same multi-equality as this one (except the
  // condition itself, which is excluded), and thus are redundant
  // against it. This is used in AlreadyAppliedThroughSargable()
  // to quickly find out if we already have applied any of them
  // as a join condition.
  OverflowBitset redundant_against_sargable_predicates;
};

// Describes a rule disallowing specific joins; if any tables from
// needed_to_activate_rule is part of the join, then _all_ tables from
// required_nodes must also be present.
//
// See FindHyperedgeAndJoinConflicts() for details.
struct ConflictRule {
  hypergraph::NodeMap needed_to_activate_rule;
  hypergraph::NodeMap required_nodes;
};

/**
  Represents an expression tree in the relational algebra of joins.
  Expressions are either tables, or joins of two expressions.
  (Joins can have join conditions, but more general filters are
  not represented in this structure.)

  These are used as an abstract precursor to the join hypergraph;
  they represent the joins in the query block more or less directly,
  without any reordering. (The parser should largely have output a
  structure like this instead of Table_ref, but we are not there yet.)
  The only real manipulation we do on them is pushing down conditions,
  identifying equijoin conditions from other join conditions,
  and identifying join conditions that touch given tables (also a form
  of pushdown).
 */
struct RelationalExpression {
  explicit RelationalExpression(THD *thd)
      : multi_children(thd->mem_root),
        join_conditions(thd->mem_root),
        equijoin_conditions(thd->mem_root),
        properties_for_join_conditions(thd->mem_root),
        properties_for_equijoin_conditions(thd->mem_root) {}

  enum Type {
    INNER_JOIN = static_cast<int>(JoinType::INNER),
    LEFT_JOIN = static_cast<int>(JoinType::OUTER),
    SEMIJOIN = static_cast<int>(JoinType::SEMI),
    ANTIJOIN = static_cast<int>(JoinType::ANTI),

    // STRAIGHT_JOIN is an inner join that the user has specified
    // is noncommutative (as a hint, but one we are not allowed to
    // disregard).
    STRAIGHT_INNER_JOIN = 101,

    // Generally supported by the conflict detector only, not the parser
    // or any iterators. We include this because we will be needing it
    // when we actually implement full outer join, and because it helps
    // verifying semijoin correctness in the unit tests (see the CountPlans*
    // tests).
    FULL_OUTER_JOIN = static_cast<int>(JoinType::FULL_OUTER),

    // An inner join between two _or more_ tables, with no join conditions.
    // This is a special form used only during pushdown, for increased
    // flexibility in reordering. MULTI_INNER_JOIN nodes do not use
    // left and right, but rather store all its children in multi_children
    // (which is empty for all other types).
    MULTI_INNER_JOIN = 102,

    TABLE = 100
  } type;
  table_map tables_in_subtree;

  // Exactly the same as tables_in_subtree, just with node indexes instead of
  // table indexes. This is stored alongside tables_in_subtree to save the cost
  // and convenience of doing repeated translation between the two.
  hypergraph::NodeMap nodes_in_subtree;

  // If type == TABLE.
  const Table_ref *table;
  Mem_root_array<Item *> join_conditions_pushable_to_this;
  // Tables in the same companion set are those that are inner-joined
  // against each other; we use this to see in what parts of the graph
  // we allow cycles. (Within companion sets, we are also allowed to
  // add Cartesian products if we deem that an advantage, but we don't
  // do it currently.) -1 means that the table is not part of a companion
  // set, e.g. because it only participates in outer joins. Tables may
  // also be alone in their companion sets, which essentially means
  // the same thing as -1. The companion sets are just opaque identifiers;
  // the number itself doesn't mean much.
  int companion_set{-1};

  // If type != TABLE. Note that equijoin_conditions will be split off
  // from join_conditions fairly late (at CreateHashJoinConditions()),
  // so often, you will see equijoin conditions in join_condition..
  RelationalExpression *left, *right;
  Mem_root_array<RelationalExpression *>
      multi_children;  // See MULTI_INNER_JOIN.
  Mem_root_array<Item *> join_conditions;
  Mem_root_array<Item_eq_base *> equijoin_conditions;

  // For each element in join_conditions and equijoin_conditions (respectively),
  // contains some cached properties that the join optimizer would like to have
  // available for frequent reuse.
  //
  // It is a bit awkward to have these separate instead of in the same arrays,
  // but the latter would complicate MakeJoinHypergraph() a fair amount,
  // as this information is private to the join optimizer (ie., it is not
  // generated along with the hypergraph; it is added after MakeJoinHypergraph()
  // is completed).
  Mem_root_array<CachedPropertiesForPredicate> properties_for_join_conditions;
  Mem_root_array<CachedPropertiesForPredicate>
      properties_for_equijoin_conditions;

  // If true, at least one condition under “join_conditions” is a false (0)
  // constant. (Such conditions can never be under “equijoin_conditions”.)
  bool join_conditions_reject_all_rows{false};
  table_map conditions_used_tables{0};
  // If the join conditions were also added as predicates due to cycles
  // in the graph (see comment in AddCycleEdges()), contains a range of
  // which indexes they got in the predicate list. This is so that we know that
  // they are redundant and don't have to apply them if we actually apply this
  // join (as opposed to getting the edge implicitly by means of joining the
  // tables along some other way in the cycle).
  int join_predicate_first{0}, join_predicate_last{0};

  // Conflict rules that must be checked before making a subgraph
  // out of this join; this is in addition to the regular connectivity
  // check. See FindHyperedgeAndJoinConflicts() for more details.
  Mem_root_array<ConflictRule> conflict_rules;
};

// Check conflict rules; usually, they will be empty, but the hyperedges are
// not able to encode every single combination of disallowed joins.
inline bool PassesConflictRules(hypergraph::NodeMap joined_tables,
                                const RelationalExpression *expr) {
  for (const ConflictRule &rule : expr->conflict_rules) {
    if (Overlaps(joined_tables, rule.needed_to_activate_rule) &&
        !IsSubset(rule.required_nodes, joined_tables)) {
      return false;
    }
  }
  return true;
}

// Whether (a <expr> b) === (b <expr> a). See also OperatorIsAssociative(),
// OperatorsAreAssociative() // and OperatorsAre{Left,Right}Asscom()
// in make_join_hypergraph.cc.
inline bool OperatorIsCommutative(const RelationalExpression &expr) {
  return expr.type == RelationalExpression::INNER_JOIN ||
         expr.type == RelationalExpression::FULL_OUTER_JOIN;
}

// Call the given functor on each non-table operator in the tree below expr,
// including expr itself, in post-traversal order.
template <class Func>
void ForEachJoinOperator(RelationalExpression *expr, Func &&func) {
  if (expr->type == RelationalExpression::TABLE) {
    return;
  }
  ForEachJoinOperator(expr->left, std::forward<Func &&>(func));
  ForEachJoinOperator(expr->right, std::forward<Func &&>(func));
  func(expr);
}

template <class Func>
void ForEachOperator(RelationalExpression *expr, Func &&func) {
  if (expr->type != RelationalExpression::TABLE) {
    ForEachOperator(expr->left, std::forward<Func &&>(func));
    ForEachOperator(expr->right, std::forward<Func &&>(func));
  }
  func(expr);
}

#endif  // SQL_JOIN_OPTIMIZER_RELATIONAL_EXPRESSION_H
