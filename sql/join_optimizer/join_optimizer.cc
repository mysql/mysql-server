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

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_bit.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/filesort.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/mem_root_array.h"
#include "sql/opt_range.h"
#include "sql/query_options.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_planner.h"
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"
#include "sql/table.h"

using hypergraph::Hyperedge;
using hypergraph::Hypergraph;
using hypergraph::NodeMap;
using std::array;
using std::string;
using std::swap;
using std::vector;

namespace {

// These are extremely arbitrary cost model constants. We should revise them
// based on actual query times (possibly using linear regression?), and then
// put them into the cost model to make them user-tunable. However, until
// we've fixed some glaring omissions such as lack of understanding of initial
// cost, any such estimation will be dominated by outliers/noise.
constexpr double kApplyOneFilterCost = 0.1;
constexpr double kAggregateOneRowCost = 0.1;
constexpr double kSortOneRowCost = 0.1;
constexpr double kHashBuildOneRowCost = 0.1;
constexpr double kHashProbeOneRowCost = 0.1;
constexpr double kMaterializeOneRowCost = 0.1;

/**
  CostingReceiver contains the main join planning logic, selecting access paths
  based on cost. It receives subplans from DPhyp (see enumerate_subgraph.h),
  assigns them costs based on a cost model, and keeps the ones that are
  cheapest. In the end, this means it will be left with a root access path that
  gives the lowest total cost for joining the tables in the query block, ie.,
  without ORDER BY etc.

  Currently, besides the expected number of produced rows (which is the same no
  matter how we access the table) we keep only a single value per subplan
  (total cost), and thus also only a single best access path. In the future,
  we will have more dimensions to worry about, such as initial cost versus total
  cost (relevant for LIMIT), ordering properties, and so on. At that point,
  there is not necessarily a single “best” access path anymore, and we will need
  to keep multiple ones around, and test all of them as candidates when building
  larger subplans.
 */
class CostingReceiver {
 public:
  CostingReceiver(
      THD *thd, const JoinHypergraph &graph,
      uint64_t supported_access_path_types,
      secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook,
      string *trace)
      : m_thd(thd),
        m_graph(graph),
        m_supported_access_path_types(supported_access_path_types),
        m_secondary_engine_cost_hook(secondary_engine_cost_hook),
        m_trace(trace) {
    // At least one join type must be supported.
    assert(Overlaps(supported_access_path_types,
                    AccessPathTypeBitmap(AccessPath::HASH_JOIN,
                                         AccessPath::NESTED_LOOP_JOIN)));
  }

  bool HasSeen(NodeMap subgraph) const {
    return m_access_paths.count(subgraph) != 0;
  }

  bool FoundSingleNode(int node_idx);

  // Called EmitCsgCmp() in the DPhyp paper.
  bool FoundSubgraphPair(NodeMap left, NodeMap right, int edge_idx);

  const Prealloced_array<AccessPath *, 4> &root_candidates() {
    const auto it = m_access_paths.find(TablesBetween(0, m_graph.nodes.size()));
    assert(it != m_access_paths.end());
    return it->second;
  }

  size_t num_access_paths() const { return m_access_paths.size(); }

 private:
  THD *m_thd;

  /**
    For each subset of tables that are connected in the join hypergraph,
    keeps the current best access paths for producing said subset.
    There can be several that are best in different ways; see comments
    on ProposeAccessPath().

    Also used for communicating connectivity information back to DPhyp
    (in HasSeen()); if there's an entry here, that subset will induce
    a connected subgraph of the join hypergraph.
   */
  std::unordered_map<NodeMap, Prealloced_array<AccessPath *, 4>> m_access_paths;

  /// The graph we are running over.
  const JoinHypergraph &m_graph;

  /// The supported access path types. Access paths of types not in
  /// this set should not be created. It is currently only used to
  /// limit which join types to use, so any bit that does not
  /// represent a join access path, is ignored for now.
  uint64_t m_supported_access_path_types;

  /// Pointer to a function that modifies the cost estimates of an access path
  /// for execution in a secondary storage engine, or nullptr otherwise.
  secondary_engine_modify_access_path_cost_t m_secondary_engine_cost_hook;

  /// If not nullptr, we store human-readable optimizer trace information here.
  string *m_trace;

  /// For trace use only.
  std::string PrintSet(NodeMap x) {
    std::string ret = "{";
    bool first = true;
    for (size_t node_idx : BitsSetIn(x)) {
      if (!first) {
        ret += ",";
      }
      first = false;
      ret += m_graph.nodes[node_idx].table->alias;
    }
    return ret + "}";
  }

  /// Is the given access path type supported?
  bool SupportedAccessPathType(AccessPath::Type type) const {
    return Overlaps(AccessPathTypeBitmap(type), m_supported_access_path_types);
  }

  AccessPath *ProposeAccessPathForNodes(NodeMap nodes, AccessPath *path,
                                        const char *description_for_trace);
  bool ProposeTableScan(TABLE *table, int node_idx);
  bool ProposeRefAccess(TABLE *table, int node_idx, KEY *key, unsigned key_idx);
  void ProposeNestedLoopJoin(NodeMap left, NodeMap right, AccessPath *left_path,
                             AccessPath *right_path, const JoinPredicate *edge);
  void ProposeHashJoin(NodeMap left, NodeMap right, AccessPath *left_path,
                       AccessPath *right_path, const JoinPredicate *edge,
                       bool *wrote_trace);
  void ApplyPredicatesForBaseTable(int node_idx, uint64_t applied_predicates,
                                   uint64_t subsumed_predicates,
                                   AccessPath *path);
  void ApplyDelayedPredicatesAfterJoin(NodeMap left, NodeMap right,
                                       const AccessPath *left_path,
                                       const AccessPath *right_path,
                                       AccessPath *join_path);
};

/// Finds the set of supported access path types.
uint64_t SupportedAccessPathTypes(const THD *thd) {
  const handlerton *secondary_engine = thd->lex->m_sql_cmd->secondary_engine();
  if (secondary_engine != nullptr) {
    return secondary_engine->secondary_engine_supported_access_paths;
  }

  // Outside of secondary storage engines, all access path types are supported.
  return ~uint64_t{0};
}

/// Gets the secondary storage engine cost modification function, if any.
secondary_engine_modify_access_path_cost_t SecondaryEngineCostHook(
    const THD *thd) {
  const handlerton *secondary_engine = thd->lex->m_sql_cmd->secondary_engine();
  if (secondary_engine == nullptr) {
    return nullptr;
  } else {
    return secondary_engine->secondary_engine_modify_access_path_cost;
  }
}

/**
  Called for each table in the query block, at some arbitrary point before we
  start seeing subsets where it's joined to other tables.

  Currently, we support table scan only, so we create a single access path
  corresponding to that and cost it. In this context, “tables” in a query block
  also includes virtual tables such as derived tables, so we need to figure out
  if there is a cost for materializing them.
 */
bool CostingReceiver::FoundSingleNode(int node_idx) {
  if (m_thd->is_error()) return true;

  TABLE *table = m_graph.nodes[node_idx].table;
  TABLE_LIST *tl = table->pos_in_table_list;

  // Ask the storage engine to update stats.records, if needed.
  // NOTE: ha_archive breaks without this call! (That is probably a bug in
  // ha_archive, though.)
  tl->fetch_number_of_rows();

  if (ProposeTableScan(table, node_idx)) {
    return true;
  }

  if (!Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS)) {
    for (unsigned key_idx = 0; key_idx < table->s->keys; ++key_idx) {
      if (ProposeRefAccess(table, node_idx, &table->key_info[key_idx],
                           key_idx)) {
        return true;
      }
    }
  }

  return false;
}

// Specifies a mapping in a TABLE_REF between an index keypart and a condition,
// with the intention to satisfy the condition with the index keypart (ref
// access). Roughly comparable to Key_use in the non-hypergraph optimizer.
struct KeypartForRef {
  // The condition we are pushing down (e.g. t1.f1 = 3).
  Item *condition;

  // The field that is to be matched (e.g. t1.f1).
  Field *field;

  // The value we are matching against (e.g. 3). Could be another field.
  Item *val;

  // Whether this condition would never match if either side is NULL.
  bool null_rejecting;

  // Tables used by the condition. Necessarily includes the table “field”
  // is part of.
  table_map used_tables;
};

int WasPushedDownToRef(Item *condition, const KeypartForRef *keyparts,
                       unsigned num_keyparts) {
  for (unsigned keypart_idx = 0; keypart_idx < num_keyparts; keypart_idx++) {
    if (condition->eq(keyparts[keypart_idx].condition,
                      /*binary_cmp=*/true)) {
      return keypart_idx;
    }
  }
  return -1;
}

bool CostingReceiver::ProposeRefAccess(TABLE *table, int node_idx, KEY *key,
                                       unsigned key_idx) {
  // NOTE: visible_index claims to contain “visible and enabled” indexes,
  // but we still need to check keys_in_use to ignore disabled indexes.
  if (!table->keys_in_use_for_query.is_set(key_idx) ||
      (key->flags & HA_FULLTEXT)) {
    return false;
  }

  // Go through each of the sargable predicates and see how many key parts
  // we can match.
  unsigned matched_keyparts = 0;
  unsigned length = 0;
  const unsigned usable_keyparts = actual_key_parts(key);
  KeypartForRef keyparts[MAX_REF_PARTS];

  for (unsigned keypart_idx = 0;
       keypart_idx < usable_keyparts && keypart_idx < MAX_REF_PARTS;
       ++keypart_idx) {
    const KEY_PART_INFO &keyinfo = key->key_part[keypart_idx];
    bool matched_this_keypart = false;

    for (const SargablePredicate &sp :
         m_graph.nodes[node_idx].sargable_predicates) {
      if (!sp.field->part_of_key.is_set(key_idx)) {
        // Quick reject.
        continue;
      }
      // Only x = const for now. (And true const, not const_for_execution();
      // so no execution of queries during optimization.)
      Item_func *item = down_cast<Item_func *>(
          m_graph.predicates[sp.predicate_index].condition);
      if (sp.field->eq(keyinfo.field) && sp.other_side->const_item() &&
          comparable_in_index(item, sp.field, Field::itRAW, item->functype(),
                              sp.other_side) &&
          !(sp.field->cmp_type() == STRING_RESULT &&
            sp.field->match_collation_to_optimize_range() &&
            sp.field->charset() != item->compare_collation())) {
        matched_this_keypart = true;
        keyparts[keypart_idx].field = sp.field;
        keyparts[keypart_idx].condition = item;
        keyparts[keypart_idx].val = sp.other_side;
        keyparts[keypart_idx].null_rejecting = true;
        keyparts[keypart_idx].used_tables = item->used_tables();
        ++matched_keyparts;
        length += keyinfo.store_length;
        break;
      }
    }
    if (!matched_this_keypart) {
      break;
    }
  }
  if (matched_keyparts == 0) {
    return false;
  }

  if (matched_keyparts < usable_keyparts &&
      (table->file->index_flags(key_idx, 0, false) & HA_ONLY_WHOLE_INDEX)) {
    if (m_trace != nullptr) {
      *m_trace += StringPrintf(
          " - %s is whole-key only, and we could only match %d/%d "
          "key parts for ref access\n",
          key->name, matched_keyparts, usable_keyparts);
    }
    return false;
  }

  if (m_trace != nullptr) {
    if (matched_keyparts < usable_keyparts) {
      *m_trace += StringPrintf(
          " - %s is applicable for ref access (using %d/%d key parts only)\n",
          key->name, matched_keyparts, usable_keyparts);
    } else {
      *m_trace +=
          StringPrintf(" - %s is applicable for ref access\n", key->name);
    }
  }

  // Create TABLE_REF for this ref, and set it up based on the chosen keyparts.
  TABLE_REF *ref = new (m_thd->mem_root) TABLE_REF;
  if (init_ref(m_thd, matched_keyparts, length, key_idx, ref)) {
    return true;
  }

  uchar *key_buff = ref->key_buff;
  uchar *null_ref_key = nullptr;
  bool null_rejecting_key = true;
  for (unsigned keypart_idx = 0; keypart_idx < matched_keyparts;
       keypart_idx++) {
    KeypartForRef *keypart = &keyparts[keypart_idx];
    const KEY_PART_INFO *keyinfo = &key->key_part[keypart_idx];

    if (init_ref_part(m_thd, keypart_idx, keypart->val, /*cond_guard=*/nullptr,
                      keypart->null_rejecting, /*const_tables=*/0,
                      keypart->used_tables, keyinfo->null_bit, keyinfo,
                      key_buff, ref)) {
      return true;
    }
    // TODO(sgunders): When we get support for REF_OR_NULL,
    // set null_ref_key = key_buff here if appropriate.
    /*
      The selected key will reject matches on NULL values if:
       - the key field is nullable, and
       - predicate rejects NULL values (keypart->null_rejecting is true), or
       - JT_REF_OR_NULL is not effective.
    */
    if ((keyinfo->field->is_nullable() || table->is_nullable()) &&
        (!keypart->null_rejecting || null_ref_key != nullptr)) {
      null_rejecting_key = false;
    }
    key_buff += keyinfo->store_length;
  }

  double num_output_rows = table->file->stats.records;

  uint64_t applied_predicates = 0;
  uint64_t subsumed_predicates = 0;
  for (size_t i = 0; i < m_graph.predicates.size(); ++i) {
    int keypart_idx = WasPushedDownToRef(m_graph.predicates[i].condition,
                                         keyparts, matched_keyparts);
    if (keypart_idx == -1) {
      continue;
    }

    num_output_rows *= m_graph.predicates[i].selectivity;
    applied_predicates |= uint64_t{1} << i;

    const KeypartForRef &keypart = keyparts[keypart_idx];
    if (ref_lookup_subsumes_comparison(keypart.field, keypart.val)) {
      if (m_trace != nullptr) {
        *m_trace +=
            StringPrintf(" - %s is subsumed by ref access on %s.%s\n",
                         ItemToString(m_graph.predicates[i].condition).c_str(),
                         table->alias, keypart.field->field_name);
      };
      subsumed_predicates |= uint64_t{1} << i;
    } else {
      if (m_trace != nullptr) {
        *m_trace += StringPrintf(
            " - %s is not fully subsumed by ref access on %s.%s, keeping\n",
            ItemToString(m_graph.predicates[i].condition).c_str(), table->alias,
            keypart.field->field_name);
      }
    }
  }

  // We are guaranteed to get a single row back if all of these hold:
  //
  //  - The index must be unique.
  //  - We can never query it with NULL (ie., no keyparts are nullable,
  //    or our condition is already NULL-rejecting), since NULL is
  //    an exception for unique indexes.
  //  - We use all key parts.
  //
  // This matches the logic in create_ref_for_key().
  const bool single_row = Overlaps(actual_key_flags(key), HA_NOSAME) &&
                          (!Overlaps(actual_key_flags(key), HA_NULL_PART_KEY) ||
                           null_rejecting_key) &&
                          matched_keyparts == usable_keyparts;
  if (single_row) {
    num_output_rows = std::min(num_output_rows, 1.0);
  }

  const double table_scan_cost = table->file->table_scan_cost().total_cost();
  const double worst_seeks =
      find_worst_seeks(table->cost_model(), num_output_rows, table_scan_cost);
  const double cost =
      find_cost_for_ref(m_thd, table, key_idx, num_output_rows, worst_seeks);

  AccessPath path;
  if (single_row) {
    path.type = AccessPath::EQ_REF;
    path.eq_ref().table = table;
    path.eq_ref().ref = ref;
    path.eq_ref().use_order = false;
  } else {
    path.type = AccessPath::REF;
    path.ref().table = table;
    path.ref().ref = ref;
    path.ref().use_order = false;
    path.ref().reverse = false;
  }
  path.num_output_rows_before_filter = num_output_rows;
  path.cost_before_filter = cost;
  path.init_cost = 0.0;

  ApplyPredicatesForBaseTable(node_idx, applied_predicates, subsumed_predicates,
                              &path);

  ProposeAccessPathForNodes(TableBitmap(node_idx), &path, key->name);
  return false;
}

bool CostingReceiver::ProposeTableScan(TABLE *table, int node_idx) {
  AccessPath table_path;
  table_path.type = AccessPath::TABLE_SCAN;
  table_path.count_examined_rows = true;
  table_path.table_scan().table = table;

  // Doing at least one table scan (this one), so mark the query as such.
  // TODO(sgunders): Move out when we get more types and this access path could
  // be replaced by something else.
  m_thd->set_status_no_index_used();

  double num_output_rows = table->file->stats.records;
  double cost = table->file->table_scan_cost().total_cost();

  table_path.num_output_rows_before_filter = num_output_rows;
  table_path.init_cost = 0.0;
  table_path.cost_before_filter = cost;

  ApplyPredicatesForBaseTable(node_idx, /*applied_predicates=*/0,
                              /*subsumed_predicates=*/0, &table_path);

  if (m_trace != nullptr) {
    *m_trace += StringPrintf("Found node %s [rows=%.0f]\n",
                             m_graph.nodes[node_idx].table->alias,
                             table_path.num_output_rows);
    for (int pred_idx : BitsSetIn(table_path.filter_predicates)) {
      *m_trace += StringPrintf(
          " - applied predicate %s\n",
          ItemToString(m_graph.predicates[pred_idx].condition).c_str());
    }
  }

  // See if this is an information schema table that must be filled in before
  // we scan.
  TABLE_LIST *tl = table->pos_in_table_list;
  if (tl->schema_table != nullptr && tl->schema_table->fill_table) {
    // TODO(sgunders): We don't need to allocate materialize_path on the
    // MEM_ROOT.
    AccessPath *new_table_path = new (m_thd->mem_root) AccessPath(table_path);
    AccessPath *materialize_path =
        NewMaterializeInformationSchemaTableAccessPath(m_thd, new_table_path,
                                                       tl,
                                                       /*condition=*/nullptr);

    materialize_path->num_output_rows = table_path.num_output_rows;
    materialize_path->num_output_rows_before_filter =
        table_path.num_output_rows_before_filter;
    materialize_path->init_cost = table_path.cost;  // Rudimentary.
    materialize_path->cost_before_filter = table_path.cost;
    materialize_path->cost = table_path.cost;
    materialize_path->filter_predicates = table_path.filter_predicates;
    materialize_path->delayed_predicates = table_path.delayed_predicates;
    new_table_path->filter_predicates = new_table_path->delayed_predicates = 0;

    // Some information schema tables have zero as estimate, which can lead
    // to completely wild plans. Add a placeholder to make sure we have
    // _something_ to work with.
    if (materialize_path->num_output_rows_before_filter == 0) {
      new_table_path->num_output_rows = 1000;
      new_table_path->num_output_rows_before_filter = 1000;
      materialize_path->num_output_rows = 1000;
      materialize_path->num_output_rows_before_filter = 1000;
    }

    assert(!tl->uses_materialization());
    ProposeAccessPathForNodes(TableBitmap(node_idx), materialize_path, "");
    return false;
  }

  if (tl->uses_materialization()) {
    // TODO(sgunders): When we get multiple candidates for each table, don't
    // move table_path to MEM_ROOT storage unless ProposeAccessPath() keeps it.
    AccessPath *path = new (m_thd->mem_root) AccessPath(table_path);

    // TODO(sgunders): We don't need to allocate materialize_path on the
    // MEM_ROOT.
    AccessPath *materialize_path;
    if (tl->is_table_function()) {
      // TODO(sgunders): Queries with these are currently disabled,
      // since they may depend on fields from other tables (and then
      // hash join is not possible). When we support parametrized paths,
      // add the correct parameters here, compute some cost
      // and open up for the queries.
      assert(false);
      materialize_path = NewMaterializedTableFunctionAccessPath(
          m_thd, table, tl->table_function, path);
    } else {
      bool rematerialize = tl->derived_unit()->uncacheable != 0;
      if (tl->common_table_expr()) {
        // Handled in clear_corr_something_something, not here
        rematerialize = false;
      }
      materialize_path =
          GetAccessPathForDerivedTable(m_thd, tl, table, rematerialize,
                                       /*invalidators=*/nullptr, path);
    }

    // TODO(sgunders): Take rematerialization cost into account,
    // or maybe, more lack of it.
    materialize_path->filter_predicates = table_path.filter_predicates;
    materialize_path->delayed_predicates = table_path.delayed_predicates;
    path->filter_predicates = path->delayed_predicates = 0;

    ProposeAccessPathForNodes(TableBitmap(node_idx), materialize_path, "");
    return false;
  }

  ProposeAccessPathForNodes(TableBitmap(node_idx), &table_path, "");
  return false;
}

// See which predicates that apply to this table. Some can be applied right
// away, some require other tables first and must be delayed.
void CostingReceiver::ApplyPredicatesForBaseTable(int node_idx,
                                                  uint64_t applied_predicates,
                                                  uint64_t subsumed_predicates,
                                                  AccessPath *path) {
  const NodeMap my_map = TableBitmap(node_idx);
  path->num_output_rows = path->num_output_rows_before_filter;
  path->cost = path->cost_before_filter;
  path->filter_predicates = 0;
  path->delayed_predicates = 0;
  for (size_t i = 0; i < m_graph.predicates.size(); ++i) {
    if (subsumed_predicates & (uint64_t{1} << i)) {
      continue;
    }
    if (m_graph.predicates[i].total_eligibility_set == my_map) {
      path->filter_predicates |= uint64_t{1} << i;
      path->cost += path->num_output_rows * kApplyOneFilterCost;
      if (applied_predicates & (uint64_t{1} << i)) {
        // We already factored in this predicate when calculating
        // the selectivity of the ref access, so don't do it again.
      } else {
        path->num_output_rows *= m_graph.predicates[i].selectivity;
      }
    } else if (Overlaps(m_graph.predicates[i].total_eligibility_set, my_map)) {
      path->delayed_predicates |= uint64_t{1} << i;
    }
  }
}

/**
  Called to signal that it's possible to connect the non-overlapping
  table subsets “left” and “right” through the edge given by “edge_idx”
  (which corresponds to an index in m_graph.edges), ie., we have found
  a legal subplan for joining (left ∪ right). Assign it a cost based on
  the cost of the children and the join method we use. (Currently, there
  is only one -- hash join.)

  There may be multiple such calls for the same subplan; e.g. for
  inner-joining {t1,t2,t3}, we will get calls for both {t1}/{t2,t3}
  and {t1,t2}/{t3}, and need to assign costs to both and keep the
  cheapest one. However, we will not get calls with the two subsets
  in reversed order.
 */
bool CostingReceiver::FoundSubgraphPair(NodeMap left, NodeMap right,
                                        int edge_idx) {
  if (m_thd->is_error()) return true;

  assert(left != 0);
  assert(right != 0);
  assert((left & right) == 0);

  const JoinPredicate *edge = &m_graph.edges[edge_idx];

  auto left_it = m_access_paths.find(left);
  assert(left_it != m_access_paths.end());
  auto right_it = m_access_paths.find(right);
  assert(right_it != m_access_paths.end());

  bool wrote_trace = false;

  for (AccessPath *left_path : left_it->second) {
    for (AccessPath *right_path : right_it->second) {
      // For inner joins and Cartesian products, the order does not matter.
      // In lieu of a more precise cost model, always keep the one that hashes
      // the fewest amount of rows. (This has lower initial cost, and the same
      // cost.) When cost estimates are supplied by the secondary engine,
      // explore both orders, since the secondary engine might unilaterally
      // decide to prefer or reject one particular order.
      const bool operator_is_commutative =
          edge->expr->type == RelationalExpression::INNER_JOIN ||
          edge->expr->type == RelationalExpression::CARTESIAN_PRODUCT;
      if (operator_is_commutative && m_secondary_engine_cost_hook == nullptr) {
        if (left_path->num_output_rows < right_path->num_output_rows) {
          ProposeHashJoin(right, left, right_path, left_path, edge,
                          &wrote_trace);
        } else {
          ProposeHashJoin(left, right, left_path, right_path, edge,
                          &wrote_trace);
        }
      } else {
        ProposeHashJoin(left, right, left_path, right_path, edge, &wrote_trace);
        if (operator_is_commutative) {
          ProposeHashJoin(right, left, right_path, left_path, edge,
                          &wrote_trace);
        }
      }

      ProposeNestedLoopJoin(left, right, left_path, right_path, edge);
      if (operator_is_commutative) {
        ProposeNestedLoopJoin(right, left, right_path, left_path, edge);
      }

      if (m_access_paths.size() > 100000) {
        // Bail out; we're going to be needing graph simplification
        // (a separate worklog).
        return true;
      }
    }
  }
  return false;
}

double FindOutputRowsForJoin(AccessPath *left_path, AccessPath *right_path,
                             const JoinPredicate *edge) {
  const double outer_rows = left_path->num_output_rows;
  const double inner_rows = right_path->num_output_rows;
  const double selectivity = edge->selectivity;
  if (edge->expr->type == RelationalExpression::ANTIJOIN) {
    return outer_rows * (1.0 - selectivity);
  } else if (edge->expr->type == RelationalExpression::SEMIJOIN) {
    return outer_rows * selectivity;
  } else {
    double num_output_rows = outer_rows * inner_rows * selectivity;
    if (edge->expr->type == RelationalExpression::LEFT_JOIN) {
      num_output_rows = std::max(num_output_rows, outer_rows);
    }
    return num_output_rows;
  }
}

void CostingReceiver::ProposeHashJoin(NodeMap left, NodeMap right,
                                      AccessPath *left_path,
                                      AccessPath *right_path,
                                      const JoinPredicate *edge,
                                      bool *wrote_trace) {
  if (!SupportedAccessPathType(AccessPath::HASH_JOIN)) return;

  AccessPath join_path;
  join_path.type = AccessPath::HASH_JOIN;
  join_path.hash_join().outer = left_path;
  join_path.hash_join().inner = right_path;
  join_path.hash_join().join_predicate = edge;
  join_path.hash_join().store_rowids = false;
  join_path.hash_join().tables_to_get_rowid_for = 0;
  join_path.hash_join().allow_spill_to_disk = true;

  double num_output_rows = FindOutputRowsForJoin(left_path, right_path, edge);

  // TODO(sgunders): Add estimates for spill-to-disk costs.
  const double build_cost =
      right_path->cost + right_path->num_output_rows * kHashBuildOneRowCost;
  double cost = left_path->cost + build_cost +
                left_path->num_output_rows * kHashProbeOneRowCost;

  // Note: This isn't strictly correct if the non-equijoin conditions
  // have selectivities far from 1.0; the cost should be calculated
  // on the number of rows after the equijoin conditions, but before
  // the non-equijoin conditions.
  cost += num_output_rows * edge->expr->join_conditions.size() *
          kApplyOneFilterCost;

  join_path.num_output_rows_before_filter = num_output_rows;
  join_path.cost_before_filter = cost;
  join_path.num_output_rows = num_output_rows;
  join_path.init_cost = build_cost + left_path->init_cost;
  join_path.cost = cost;

  ApplyDelayedPredicatesAfterJoin(left, right, left_path, right_path,
                                  &join_path);

  // Only trace once; the rest ought to be identical.
  if (m_trace != nullptr && !*wrote_trace) {
    *m_trace += StringPrintf(
        "Found sets %s and %s, connected by condition %s [rows=%.0f]\n",
        PrintSet(left).c_str(), PrintSet(right).c_str(),
        GenerateExpressionLabel(edge->expr).c_str(), join_path.num_output_rows);
    for (int pred_idx : BitsSetIn(join_path.filter_predicates)) {
      *m_trace += StringPrintf(
          " - applied (delayed) predicate %s\n",
          ItemToString(m_graph.predicates[pred_idx].condition).c_str());
    }
    *wrote_trace = true;
  }

  ProposeAccessPathForNodes(left | right, &join_path, "hash join");
}

// Of all delayed predicates, see which ones we can apply now, and which
// ones that need to be delayed further.
void CostingReceiver::ApplyDelayedPredicatesAfterJoin(
    NodeMap left, NodeMap right, const AccessPath *left_path,
    const AccessPath *right_path, AccessPath *join_path) {
  join_path->filter_predicates = 0;
  join_path->delayed_predicates =
      left_path->delayed_predicates ^ right_path->delayed_predicates;
  const NodeMap ready_tables = left | right;
  for (int pred_idx : BitsSetIn(left_path->delayed_predicates &
                                right_path->delayed_predicates)) {
    if (IsSubset(m_graph.predicates[pred_idx].total_eligibility_set,
                 ready_tables)) {
      join_path->filter_predicates |= uint64_t{1} << pred_idx;
      join_path->cost += join_path->num_output_rows * kApplyOneFilterCost;
      join_path->num_output_rows *= m_graph.predicates[pred_idx].selectivity;
    } else {
      join_path->delayed_predicates |= uint64_t{1} << pred_idx;
    }
  }
}

void CostingReceiver::ProposeNestedLoopJoin(NodeMap left, NodeMap right,
                                            AccessPath *left_path,
                                            AccessPath *right_path,
                                            const JoinPredicate *edge) {
  if (!SupportedAccessPathType(AccessPath::NESTED_LOOP_JOIN)) return;

  AccessPath join_path, filter_path;
  join_path.type = AccessPath::NESTED_LOOP_JOIN;
  join_path.nested_loop_join().outer = left_path;
  join_path.nested_loop_join().inner = right_path;
  if (edge->expr->type == RelationalExpression::CARTESIAN_PRODUCT) {
    join_path.nested_loop_join().join_type = JoinType::INNER;
  } else {
    join_path.nested_loop_join().join_type =
        static_cast<JoinType>(edge->expr->type);
  }
  join_path.nested_loop_join().pfs_batch_mode = false;

  bool added_filter = false;
  if (edge->expr->equijoin_conditions.size() != 0 ||
      edge->expr->join_conditions.size() != 0) {
    // Apply join filters. Don't update num_output_rows, as the join's
    // selectivity was already applied in FindOutputRowsForJoin().
    // NOTE(sgunders): We don't model the effect of short-circuiting filters on
    // the cost here.
    filter_path.type = AccessPath::FILTER;
    filter_path.filter().child = right_path;
    join_path.nested_loop_join().inner =
        &filter_path;  // Will be updated to not point to the stack later,
                       // if needed.

    CopyCosts(*right_path, &filter_path);

    // cost and num_output_rows are only for display purposes;
    // the actual selectivity estimates are based on a somewhat richer
    // variety of data, and is the same no matter what join type we're using
    List<Item> items;
    for (Item_func_eq *condition : edge->expr->equijoin_conditions) {
      items.push_back(condition);
      filter_path.cost += filter_path.num_output_rows * kApplyOneFilterCost;
      filter_path.num_output_rows *=
          EstimateSelectivity(m_thd, condition, m_trace);
    }
    for (Item *condition : edge->expr->join_conditions) {
      items.push_back(condition);
      filter_path.cost += filter_path.num_output_rows * kApplyOneFilterCost;
      filter_path.num_output_rows *=
          EstimateSelectivity(m_thd, condition, m_trace);
    }
    Item *condition;
    if (items.size() == 1) {
      condition = items.head();
    } else {
      condition = new Item_cond_and(items);
      condition->quick_fix_field();
      condition->update_used_tables();
      condition->apply_is_true();
    }
    filter_path.filter().condition = condition;
    added_filter = true;
  }

  // Ignores the cost information from filter_path; see above.
  join_path.num_output_rows_before_filter = join_path.num_output_rows =
      FindOutputRowsForJoin(left_path, right_path, edge);
  join_path.init_cost = left_path->init_cost;
  join_path.cost_before_filter = join_path.cost =
      left_path->cost + right_path->cost * left_path->num_output_rows;

  ApplyDelayedPredicatesAfterJoin(left, right, left_path, right_path,
                                  &join_path);

  AccessPath *insert_position =
      ProposeAccessPathForNodes(left | right, &join_path, "nested loop");
  if (insert_position != nullptr && added_filter) {
    // We inserted the join path, so give filter_path stable storage
    // in the MEM_ROOT, too.
    insert_position->nested_loop_join().inner =
        new (m_thd->mem_root) AccessPath(filter_path);
  }
}

enum class PathComparisonResult {
  FIRST_DOMINATES,
  SECOND_DOMINATES,
  DIFFERENT_STRENGTHS,
  IDENTICAL,
};

// See if one access path is better than the other across all cost dimensions
// (if so, we say it dominates the other one). If not, we return
// DIFFERENT_STRENGTHS so that both must be kept.
//
// TODO(sgunders): If one path is better than the other in cost, and only
// slightly worse (e.g. 1%) in a less important metric such as init_cost,
// consider pruning the latter.
//
// TODO(sgunders): Support turning off certain cost dimensions; e.g., init_cost
// only matters if we have a LIMIT or nested loop semijoin somewhere in the
// query, and it might not matter for secondary engine.
static inline PathComparisonResult CompareAccessPaths(const AccessPath &a,
                                                      const AccessPath &b) {
  bool a_is_better = false, b_is_better = false;
  if (a.cost < b.cost) {
    a_is_better = true;
  } else if (b.cost < a.cost) {
    b_is_better = true;
  }

  if (a.init_cost < b.init_cost) {
    a_is_better = true;
  } else if (b.init_cost < a.init_cost) {
    b_is_better = true;
  }

  if (!a_is_better && !b_is_better) {
    return PathComparisonResult::IDENTICAL;
  } else if (a_is_better && !b_is_better) {
    return PathComparisonResult::FIRST_DOMINATES;
  } else if (!a_is_better && b_is_better) {
    return PathComparisonResult::SECOND_DOMINATES;
  } else {
    return PathComparisonResult::DIFFERENT_STRENGTHS;
  }
}

static string PrintCost(const AccessPath &path,
                        const char *description_for_trace) {
  if (strcmp(description_for_trace, "") == 0) {
    return StringPrintf("{cost=%.1f, init_cost=%.1f}", path.cost,
                        path.init_cost);
  } else {
    return StringPrintf("{cost=%.1f, init_cost=%.1f} [%s]", path.cost,
                        path.init_cost, description_for_trace);
  }
}

/**
  Propose the given access path as an alternative to the existing access paths
  for the same task (assuming any exist at all), and hold a “tournament” to find
  whether it is better than the others. Only the best alternatives are kept,
  as defined by CompareAccessPaths(); a given access path is kept only if
  it is not dominated by any other path in the group (ie., the Pareto frontier
  is computed). This means that the following are all possible outcomes of the
  tournament:

   - The path is discarded, without ever being inserted in the list
     (dominated by at least one existing entry).
   - The path is inserted as a new alternative in the list (dominates none
     but it also not dominated by any -- or the list was empty), leaving it with
     N+1 entries.
   - The path is inserted as a new alternative in the list, but replaces one
     or more entries (dominates them).
   - The path replaces all existing alternatives, and becomes the sole entry
     in the list.

  “description_for_trace” is a short description of the inserted path
  to distinguish it in optimizer trace, if active. For instance, one might
  write “hash join” when proposing a hash join access path. It may be
  the empty string.
 */
static AccessPath *ProposeAccessPath(
    THD *thd, AccessPath *path,
    Prealloced_array<AccessPath *, 4> *existing_paths,
    secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook,
    const char *description_for_trace, string *trace) {
  if (secondary_engine_cost_hook != nullptr) {
    // If an error was raised by a previous invocation of the hook, reject all
    // paths.
    if (thd->is_error()) {
      return nullptr;
    }

    if (secondary_engine_cost_hook(thd, path)) {
      // Rejected by the secondary engine.
      return nullptr;
    }
    assert(!thd->is_error());
  }

  if (existing_paths->empty()) {
    if (trace != nullptr) {
      *trace += " - " + PrintCost(*path, description_for_trace) +
                " is first alternative, keeping\n";
    }
    AccessPath *insert_position = new (thd->mem_root) AccessPath(*path);
    existing_paths->push_back(insert_position);
    return insert_position;
  }

  AccessPath *insert_position = nullptr;
  int num_dominated = 0;
  for (size_t i = 0; i < existing_paths->size(); ++i) {
    PathComparisonResult result =
        CompareAccessPaths(*path, *((*existing_paths)[i]));
    if (result == PathComparisonResult::DIFFERENT_STRENGTHS) {
      continue;
    }
    if (result == PathComparisonResult::IDENTICAL ||
        result == PathComparisonResult::SECOND_DOMINATES) {
      assert(insert_position == nullptr);
      if (trace != nullptr) {
        *trace += " - " + PrintCost(*path, description_for_trace) +
                  " is not better than existing path " +
                  PrintCost(*(*existing_paths)[i], "") + ", discarding\n";
      }
      return nullptr;
    }
    if (result == PathComparisonResult::FIRST_DOMINATES) {
      ++num_dominated;
      if (insert_position == nullptr) {
        // Replace this path by the new, better one. We continue to search for
        // other paths to dominate. Note that we don't overwrite just yet,
        // because we might want to print out the old one in optimizer trace
        // below.
        insert_position = (*existing_paths)[i];
      } else {
        // The new path is better than the old one, but we don't need to insert
        // it again. Delete the old one by moving the last one into its place
        // (this may be a no-op) and then chopping one off the end.
        (*existing_paths)[i] = existing_paths->back();
        existing_paths->pop_back();
        --i;
      }
    }
  }

  if (insert_position == nullptr) {
    if (trace != nullptr) {
      *trace += " - " + PrintCost(*path, description_for_trace) +
                " is potential alternative, appending to existing list: (";
      bool first = true;
      for (const AccessPath *other_path : *existing_paths) {
        if (!first) {
          *trace += ", ";
        }
        *trace += PrintCost(*other_path, "");
        first = false;
      }
      *trace += ")\n";
    }
    insert_position = new (thd->mem_root) AccessPath(*path);
    existing_paths->emplace_back(insert_position);
    return insert_position;
  }

  if (trace != nullptr) {
    if (existing_paths->size() == 1) {  // Only one left.
      if (num_dominated == 1) {
        *trace += " - " + PrintCost(*path, description_for_trace) +
                  " is better than previous " +
                  PrintCost(*insert_position, "") + ", replacing\n";
      } else {
        *trace += " - " + PrintCost(*path, description_for_trace) +
                  " is better than all previous alternatives, replacing all\n";
      }
    } else {
      assert(num_dominated > 0);
      *trace += StringPrintf(
          " - %s is better than %d others, replacing them, remaining are: ",
          PrintCost(*path, description_for_trace).c_str(), num_dominated);
      bool first = true;
      for (const AccessPath *other_path : *existing_paths) {
        if (other_path == insert_position) {
          // Will be replaced by ourselves momentarily, so don't print it.
          continue;
        }
        if (!first) {
          *trace += ", ";
        }
        *trace += PrintCost(*other_path, "");
        first = false;
      }
      *trace += ")\n";
    }
  }
  *insert_position = *path;
  return insert_position;
}

AccessPath *CostingReceiver::ProposeAccessPathForNodes(
    NodeMap nodes, AccessPath *path, const char *description_for_trace) {
  // Insert an empty array if none exists.
  auto it_and_inserted = m_access_paths.emplace(
      nodes, Prealloced_array<AccessPath *, 4>{PSI_NOT_INSTRUMENTED});
  return ProposeAccessPath(m_thd, path, &it_and_inserted.first->second,
                           m_secondary_engine_cost_hook, description_for_trace,
                           m_trace);
}

/**
  Create a table array from a table bitmap.
 */
Mem_root_array<TABLE *> CollectTables(SELECT_LEX *select_lex, table_map tmap) {
  Mem_root_array<TABLE *> tables(select_lex->join->thd->mem_root);
  for (TABLE_LIST *tl = select_lex->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (tl->map() & tmap) {
      tables.push_back(tl->table);
    }
  }
  return tables;
}

bool CheckSupportedQuery(THD *thd, JOIN *join) {
  if (join->rollup_state != JOIN::RollupState::NONE) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "ROLLUP");
    return true;
  }
  if (join->select_lex->has_ft_funcs()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "fulltext search");
    return true;
  }
  if (join->select_lex->is_recursive()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "recursive CTEs");
    return true;
  }
  if (thd->lex->m_sql_cmd->using_secondary_storage_engine() &&
      SupportedAccessPathTypes(thd) == 0) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "the secondary engine in use");
    return true;
  }
  if (join->select_lex->has_windows()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "windowing functions");
    return true;
  }
  if (join->select_lex->active_options() & OPTION_BUFFER_RESULT) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "SQL_BUFFER_RESULT");
    return true;
  }
  for (TABLE_LIST *tl = join->select_lex->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (tl->is_derived() && tl->derived_unit()->m_lateral_deps) {
      my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "LATERAL");
      return true;
    }
    // We actually support table functions, but not joins against them.
    if (tl->is_table_function()) {
      my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "table functions");
      return true;
    }
  }
  return false;
}

void EstimateSortCost(AccessPath *path) {
  AccessPath *child = path->sort().child;
  const double num_rows = child->num_output_rows;
  double sort_cost;
  if (num_rows <= 1.0) {
    // Avoid NaNs from log2().
    sort_cost = kSortOneRowCost;
  } else {
    sort_cost = kSortOneRowCost * num_rows * std::max(log2(num_rows), 1.0);
  }

  path->num_output_rows = num_rows;
  path->cost = path->init_cost = child->cost + sort_cost;
  path->num_output_rows_before_filter = path->num_output_rows;
  path->cost_before_filter = path->cost;
}

/**
  If we have both ORDER BY and GROUP BY, we need a materialization step
  after the grouping -- although in most cases, we only need to
  materialize one row at a time (streaming), so the performance loss
  should be very slight. This is because when filesort only really deals
  with fields, not values; when it is to “output” a row, it puts back the
  contents of the sorted table's (or tables') row buffer(s). For
  expressions that only depend on the current row, such as (f1 + 1),
  this is fine, but aggregate functions (Item_sum) depend on multiple
  rows, so we need a field where filesort can put back its value
  (and of course, subsequent readers need to read from that field
  instead of trying to evaluate the Item_sum). A temporary table provides
  just that, so we create one based on the current field list;
  StreamingIterator (or MaterializeIterator, if we actually need to
  materialize) will evaluate all the Items in turn and put their values
  into the temporary table's fields.

  For simplicity, we materialize all items in the SELECT list, even those
  that are not aggregate functions. This is a tiny performance loss,
  but makes things simpler.

  Note that we cannot set up an access path for this temporary table yet;
  that needs to wait until we know whether the sort decided to use row IDs
  or not, and Filesort cannot be set up until it knows what tables to sort.
  Thus, that job is deferred to CreateMaterializationPathForSortingAggregates().
 */
TABLE *CreateTemporaryTableForSortingAggregates(
    THD *thd, SELECT_LEX *select_lex, Temp_table_param **temp_table_param_arg) {
  JOIN *join = select_lex->join;

  Temp_table_param *temp_table_param = new (thd->mem_root) Temp_table_param;
  *temp_table_param_arg = temp_table_param;
  temp_table_param->precomputed_group_by = false;
  temp_table_param->hidden_field_count = CountHiddenFields(*join->fields);
  temp_table_param->skip_create_table = false;
  count_field_types(select_lex, temp_table_param, *join->fields,
                    /*reset_with_sum_func=*/true, /*save_sum_fields=*/true);

  TABLE *temp_table = create_tmp_table(
      thd, temp_table_param, *join->fields,
      /*group=*/nullptr, /*distinct=*/false, /*save_sum_fields=*/true,
      select_lex->active_options(), /*rows_limit=*/HA_POS_ERROR, "");
  temp_table->alias = "<temporary>";

  // Replace the SELECT list with items that read from our temporary table.
  auto fields = new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
  for (Item *item : *join->fields) {
    Field *field = item->get_tmp_table_field();
    Item *temp_table_item;
    if (field == nullptr) {
      assert(item->const_for_execution());
      temp_table_item = item;
    } else {
      temp_table_item = new Item_field(field);
      // Field items have already been turned into copy_fields entries
      // in create_tmp_table(), but non-field items have not.
      if (item->type() != Item::FIELD_ITEM) {
        temp_table_param->items_to_copy->push_back(Func_ptr{item});
      }
    }
    temp_table_item->hidden = item->hidden;
    fields->push_back(temp_table_item);
  }
  join->fields = fields;

  // Change all items in the ORDER list to point to the temporary table.
  // This isn't important for streaming (the items would get the correct
  // value anyway -- although possibly with some extra calculations),
  // but it is for materialization.
  for (ORDER *order = select_lex->order_list.first; order != nullptr;
       order = order->next) {
    Field *field = (*order->item)->get_tmp_table_field();
    if (field != nullptr) {
      Item_field *temp_field_item = new Item_field(field);

      // *order->item points into a memory area (the “base ref slice”)
      // where HAVING might expect to find items _not_ pointing into the
      // temporary table (if there is true materialization, it should run
      // before it to minimize the size of the sorted input), so in order to
      // not disturb it, we create a whole new place for the Item pointer
      // to live.
      //
      // TODO(sgunders): When we get rid of slices altogether,
      // we can skip this.
      thd->change_item_tree(pointer_cast<Item **>(&order->item),
                            pointer_cast<Item *>(new (thd->mem_root) Item *));
      thd->change_item_tree(order->item, temp_field_item);
    }
  }

  // We made a new table, so make sure it gets properly cleaned up
  // at the end of execution.
  join->temp_tables.push_back(
      JOIN::TemporaryTableToCleanup{temp_table, temp_table_param});

  return temp_table;
}

/**
  Set up an access path for streaming or materializing between grouping
  and sorting. See CreateTemporaryTableForSortingAggregates() for details.
 */
AccessPath *CreateMaterializationPathForSortingAggregates(
    THD *thd, JOIN *join, AccessPath *path, TABLE *temp_table,
    Temp_table_param *temp_table_param, bool using_addon_fields) {
  if (using_addon_fields) {
    // The common case; we can use streaming.
    AccessPath *stream_path =
        NewStreamingAccessPath(thd, path, join, temp_table_param, temp_table,
                               /*ref_slice=*/-1);
    stream_path->num_output_rows = path->num_output_rows;
    stream_path->cost = path->cost;
    stream_path->init_cost = path->init_cost;
    stream_path->num_output_rows_before_filter = stream_path->num_output_rows;
    stream_path->cost_before_filter = stream_path->cost;
    return stream_path;
  } else {
    // Filesort needs sort by row ID, possibly because large blobs are
    // involved, so we need to actually materialize. (If we wanted a
    // smaller temporary table at the expense of more seeks, we could
    // materialize only aggregate functions and do a multi-table sort
    // by docid, but this situation is rare, so we go for simplicity.)
    AccessPath *table_path =
        NewTableScanAccessPath(thd, temp_table, /*count_examined_rows=*/false);
    AccessPath *materialize_path = NewMaterializeAccessPath(
        thd,
        SingleMaterializeQueryBlock(thd, path, /*select_number=*/-1, join,
                                    /*copy_fields_and_items=*/true,
                                    temp_table_param),
        /*invalidators=*/nullptr, temp_table, table_path, /*cte=*/nullptr,
        /*unit=*/nullptr, /*ref_slice=*/-1, /*rematerialize=*/true,
        /*limit_rows=*/HA_POS_ERROR, /*reject_multiple_rows=*/false);

    EstimateMaterializeCost(materialize_path);
    return materialize_path;
  }
}

}  // namespace

// Very rudimentary (assuming no deduplication; it's better to overestimate
// than to understimate), so that we get something that isn't “unknown”.
void EstimateMaterializeCost(AccessPath *path) {
  AccessPath *table_path = path->materialize().table_path;

  path->cost = 0;
  path->num_output_rows = 0;
  for (const MaterializePathParameters::QueryBlock &block :
       path->materialize().param->query_blocks) {
    if (block.subquery_path->num_output_rows >= 0.0) {
      path->num_output_rows += block.subquery_path->num_output_rows;
      path->cost += block.subquery_path->cost;
    }
  }
  path->cost += kMaterializeOneRowCost * path->num_output_rows;

  // Try to get usable estimates. Ignored by InnoDB, but used by
  // TempTable.
  if (table_path->type == AccessPath::TABLE_SCAN) {
    TABLE *temp_table = table_path->table_scan().table;
    temp_table->file->stats.records = path->num_output_rows;

    table_path->num_output_rows = path->num_output_rows;
    table_path->init_cost = 0.0;
    table_path->cost = temp_table->file->table_scan_cost().total_cost();
  }

  path->init_cost = path->cost + std::max(table_path->init_cost, 0.0);
  path->cost = path->cost + std::max(table_path->cost, 0.0);
}

JoinHypergraph::Node *FindNodeWithTable(JoinHypergraph *graph, TABLE *table) {
  for (JoinHypergraph::Node &node : graph->nodes) {
    if (node.table == table) {
      return &node;
    }
  }
  return nullptr;
}

Prealloced_array<AccessPath *, 4> ApplyDistinctAndOrder(
    THD *thd, SELECT_LEX *select_lex,
    Prealloced_array<AccessPath *, 4> root_candidates,
    secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook,
    string *trace) {
  JOIN *join = select_lex->join;
  assert(join->select_distinct || select_lex->is_ordered());

  TABLE *temp_table = nullptr;
  Temp_table_param *temp_table_param = nullptr;

  Mem_root_array<TABLE *> tables = CollectTables(
      select_lex,
      GetUsedTables(root_candidates[0]));  // Should be same for all paths.

  // If we have grouping followed by a sort, we need to bounce via
  // the buffers of a temporary table. See the comments on
  // CreateTemporaryTableForSortingAggregates().
  if (select_lex->is_grouped()) {
    temp_table = CreateTemporaryTableForSortingAggregates(thd, select_lex,
                                                          &temp_table_param);
    // Filesort now only needs to worry about one input -- this temporary
    // table. This holds whether we are actually materializing or just
    // using streaming.
    tables.clear();
    tables.push_back(temp_table);
  }

  // Set up the filesort objects. They have some interactions
  // around addon fields vs. sort by row ID, so we need to do this
  // before we decide on iterators.
  bool need_rowid = false;
  Filesort *filesort = nullptr;
  if (select_lex->is_ordered()) {
    Mem_root_array<TABLE *> table_copy(thd->mem_root, tables.begin(),
                                       tables.end());
    filesort = new (thd->mem_root)
        Filesort(thd, std::move(table_copy), /*keep_buffers=*/false,
                 select_lex->order_list.first,
                 /*limit_arg=*/HA_POS_ERROR, /*force_stable_sort=*/false,
                 /*remove_duplicates=*/false, /*force_sort_positions=*/false,
                 /*unwrap_rollup=*/false);
    join->filesorts_to_cleanup.push_back(filesort);
    need_rowid = !filesort->using_addon_fields();
  }
  Filesort *filesort_for_distinct = nullptr;
  bool order_by_subsumed_by_distinct = false;
  if (join->select_distinct) {
    bool force_sort_positions = false;
    if (filesort != nullptr && !filesort->using_addon_fields()) {
      // We have the rather unusual situation here that we have two sorts
      // directly after each other, with no temporary table in-between,
      // and filesort expects to be able to refer to rows by their position.
      // Usually, the sort for DISTINCT would be a superset of the sort for
      // ORDER BY, but not always (e.g. when sorting by some expression),
      // so we could end up in a situation where the first sort is by addon
      // fields and the second one is by positions.
      //
      // Thus, in this case, we force the first sort to be by positions,
      // so that the result comes from SortFileIndirectIterator or
      // SortBufferIndirectIterator. These will both position the cursor
      // on the underlying temporary table correctly before returning it,
      // so that the successive filesort will save the right position
      // for the row.
      if (trace != nullptr) {
        *trace +=
            "Forcing DISTINCT to sort row IDs, since ORDER BY sort does\n";
      }
      force_sort_positions = true;
    }

    // If there's an ORDER BY on the query, it needs to be heeded in the
    // re-sort for DISTINCT.
    ORDER *desired_order =
        select_lex->is_ordered() ? select_lex->order_list.first : nullptr;

    ORDER *order = create_order_from_distinct(
        thd, Ref_item_array(), desired_order, join->fields,
        /*skip_aggregates=*/false, /*convert_bit_fields_to_long=*/false,
        &order_by_subsumed_by_distinct);

    if (order != nullptr) {
      filesort_for_distinct = new (thd->mem_root)
          Filesort(thd, std::move(tables),
                   /*keep_buffers=*/false, order, HA_POS_ERROR,
                   /*force_stable_sort=*/false,
                   /*remove_duplicates=*/true, force_sort_positions,
                   /*unwrap_rollup=*/false);
      join->filesorts_to_cleanup.push_back(filesort_for_distinct);
      if (!filesort_for_distinct->using_addon_fields()) {
        need_rowid = true;
      }
    }
  }

  // Apply streaming between grouping and us, if needed (see above).
  if (temp_table != nullptr) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      root_path = CreateMaterializationPathForSortingAggregates(
          thd, join, root_path, temp_table, temp_table_param,
          /*using_addon_fields=*/!need_rowid);
      ProposeAccessPath(thd, root_path, &new_root_candidates,
                        secondary_engine_cost_hook, "", trace);
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Now create iterators for DISTINCT, if applicable.
  if (join->select_distinct) {
    if (trace != nullptr) {
      *trace += "Applying sort for DISTINCT\n";
    }

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      if (filesort_for_distinct == nullptr) {
        // Only const fields.
        AccessPath *limit_path = NewLimitOffsetAccessPath(
            thd, root_path, /*limit=*/1, /*offset=*/0, join->calc_found_rows,
            /*reject_multiple_rows=*/false,
            /*send_records_override=*/nullptr);
        ProposeAccessPath(thd, limit_path, &new_root_candidates,
                          secondary_engine_cost_hook, "", trace);
      } else {
        AccessPath *sort_path =
            NewSortAccessPath(thd, root_path, filesort_for_distinct,
                              /*count_examined_rows=*/false);
        EstimateSortCost(sort_path);

        if (!filesort_for_distinct->using_addon_fields()) {
          FindTablesToGetRowidFor(sort_path);
        }
        ProposeAccessPath(thd, sort_path, &new_root_candidates,
                          secondary_engine_cost_hook, "", trace);
      }
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Apply ORDER BY, if applicable.
  if (select_lex->is_ordered() && order_by_subsumed_by_distinct) {
    // The ordering for DISTINCT already gave us the right sort order,
    // so no need to sort again.
    //
    // TODO(sgunders): If there are elements in desired_order that are not
    // in fields_list, consider whether it would be cheaper to add them on
    // the end to avoid the second sort, even though it would make the
    // first one more expensive. See e.g. main.distinct for a case.
    if (trace != nullptr) {
      *trace += "ORDER BY subsumed by sort for DISTINCT, ignoring\n";
    }
  } else if (select_lex->is_ordered() && !order_by_subsumed_by_distinct) {
    if (trace != nullptr) {
      *trace += "Applying sort for ORDER BY\n";
    }

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      AccessPath *sort_path = NewSortAccessPath(thd, root_path, filesort,
                                                /*count_examined_rows=*/false);
      EstimateSortCost(sort_path);

      if (!filesort->using_addon_fields()) {
        FindTablesToGetRowidFor(sort_path);
      }
      ProposeAccessPath(thd, sort_path, &new_root_candidates,
                        secondary_engine_cost_hook, "", trace);
    }
    root_candidates = std::move(new_root_candidates);
  }
  return root_candidates;
}

AccessPath *FindBestQueryPlan(THD *thd, SELECT_LEX *select_lex, string *trace) {
  JOIN *join = select_lex->join;
  if (CheckSupportedQuery(thd, join)) return nullptr;

  assert(join->temp_tables.empty());
  assert(join->filesorts_to_cleanup.empty());

  // Convert the join structures into a hypergraph.
  JoinHypergraph graph(thd->mem_root);
  if (MakeJoinHypergraph(thd, select_lex, trace, &graph)) {
    return nullptr;
  }

  // Find sargable predicates, ie., those that we can push down into indexes.
  // See add_key_field().
  //
  // TODO(sgunders): Include predicates from join conditions, x=y OR NULL
  // predicates,
  // <=> and IS NULL predicates, and the special case of COLLATION accepted in
  // add_key_field().
  //
  // TODO(sgunders): Integrate with the range optimizer, or find some other way
  // of accepting <, >, <= and >= predicates.
  if (trace != nullptr) {
    *trace += "\n";
  }
  for (int i = 0; i < static_cast<int>(graph.predicates.size()); ++i) {
    if (!IsSingleBitSet(graph.predicates[i].total_eligibility_set)) {
      continue;
    }
    Item *item = graph.predicates[i].condition;
    if (item->type() != Item::FUNC_ITEM ||
        down_cast<Item_func *>(item)->functype() != Item_bool_func2::EQ_FUNC) {
      continue;
    }
    Item_func_eq *eq_item = down_cast<Item_func_eq *>(item);
    if (eq_item->get_comparator()->get_child_comparator_count() >= 2) {
      continue;
    }
    for (unsigned arg_idx = 0; arg_idx < 2; ++arg_idx) {
      Item *left = eq_item->arguments()[arg_idx];
      Item *right = eq_item->arguments()[1 - arg_idx];
      if (left->type() != Item::FIELD_ITEM) {
        continue;
      }
      Field *field = down_cast<Item_field *>(left)->field;
      if (field->part_of_key.is_clear_all()) {
        // Not part of any key, so not sargable. (It could be part of a prefix
        // keys, though, but we include them for now.)
        continue;
      }
      JoinHypergraph::Node *node = FindNodeWithTable(&graph, field->table);
      if (node == nullptr) {
        // A field in a different query block, so not sargable for us.
        continue;
      }

      node->sargable_predicates.push_back({i, field, right});
      if (trace != nullptr) {
        *trace += "Found sargable condition " + ItemToString(item) + "\n";
      }
    }
  }

  // Run the actual join optimizer algorithm. This creates an access path
  // for the join as a whole (with lowest possible cost, and thus also
  // hopefully optimal execution time), with all pushable predicates applied.
  if (trace != nullptr) {
    *trace += "\nEnumerating subplans:\n";
  }
  for (const JoinHypergraph::Node &node : graph.nodes) {
    node.table->init_cost_model(thd->cost_model());
  }
  const secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook =
      SecondaryEngineCostHook(thd);
  CostingReceiver receiver(thd, graph, SupportedAccessPathTypes(thd),
                           secondary_engine_cost_hook, trace);
  if (EnumerateAllConnectedPartitions(graph.graph, &receiver) &&
      !thd->is_error()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "large join graphs");
    return nullptr;
  }
  if (thd->is_error()) return nullptr;

  // Get the root candidates. If there is a secondary engine cost hook, there
  // may be no candidates, as the hook may have rejected all valid plans.
  // Otherwise, expect there to be at least one candidate.
  Prealloced_array<AccessPath *, 4> root_candidates(PSI_NOT_INSTRUMENTED);
  if (secondary_engine_cost_hook == nullptr ||
      receiver.HasSeen(TablesBetween(0, graph.nodes.size()))) {
    root_candidates = receiver.root_candidates();
    assert(!root_candidates.empty());
  }
  thd->m_current_query_partial_plans += receiver.num_access_paths();
  if (trace != nullptr) {
    *trace += StringPrintf(
        "\nEnumerated %zu subplans, got %zu candidate(s) to finalize:\n",
        receiver.num_access_paths(), root_candidates.size());
  }

  // Now we have one or more access paths representing joining all the tables
  // together. (There may be multiple ones because they can be better at
  // different metrics.) We apply the post-join operations to all of them in
  // turn, and then finally pick out the one with the lowest total cost,
  // because at the end, other metrics don't really matter any more.
  //
  // We could have stopped caring about e.g. init_cost after LIMIT
  // has been applied (after which it no longer matters), so that we'd get
  // fewer candidates in each step, but this part is so cheap that it's
  // unlikely to be worth it. We go through ProposeAccessPath() mainly
  // because it gives us better tracing.
  if (trace != nullptr) {
    *trace += "Adding final predicates\n";
  }
  for (AccessPath *root_path : root_candidates) {
    // Apply any predicates that don't belong to any specific table,
    // or which are nondeterministic.
    for (size_t i = 0; i < graph.predicates.size(); ++i) {
      if (!Overlaps(graph.predicates[i].total_eligibility_set,
                    TablesBetween(0, graph.nodes.size())) ||
          Overlaps(graph.predicates[i].total_eligibility_set, RAND_TABLE_BIT)) {
        root_path->filter_predicates |= uint64_t{1} << i;
        root_path->cost += root_path->num_output_rows * kApplyOneFilterCost;
        root_path->num_output_rows *= graph.predicates[i].selectivity;
        if (trace != nullptr) {
          *trace +=
              StringPrintf(" - applied predicate %s\n",
                           ItemToString(graph.predicates[i].condition).c_str());
        }
      }
    }

    // Now that we have decided on a full plan, expand all the applied
    // filter maps into proper FILTER nodes for execution.
    ExpandFilterAccessPaths(thd, root_path, join, graph.predicates);
  }

  // Apply GROUP BY, if applicable. We currently always do this by sorting
  // first and then using streaming aggregation.
  if (select_lex->is_grouped()) {
    if (join->make_sum_func_list(*join->fields, /*before_group_by=*/true))
      return nullptr;

    if (trace != nullptr) {
      *trace += "Applying aggregation for GROUP BY\n";
    }

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      if (select_lex->is_explicitly_grouped()) {
        Mem_root_array<TABLE *> tables =
            CollectTables(select_lex, GetUsedTables(root_path));
        Filesort *filesort = new (thd->mem_root) Filesort(
            thd, std::move(tables), /*keep_buffers=*/false,
            select_lex->group_list.first,
            /*limit_arg=*/HA_POS_ERROR, /*force_stable_sort=*/false,
            /*remove_duplicates=*/false, /*force_sort_positions=*/false,
            /*unwrap_rollup=*/false);
        AccessPath *sort_path =
            NewSortAccessPath(thd, root_path, filesort,
                              /*count_examined_rows=*/false);
        EstimateSortCost(sort_path);

        root_path = sort_path;

        if (!filesort->using_addon_fields()) {
          FindTablesToGetRowidFor(sort_path);
        }
      }

      // TODO(sgunders): We don't need to allocate this on the MEM_ROOT.
      AccessPath *aggregate_path =
          NewAggregateAccessPath(thd, root_path, /*rollup=*/false);

      // TODO(sgunders): How do we estimate how many rows aggregation
      // will be reducing the output by?
      aggregate_path->num_output_rows = root_path->num_output_rows;
      aggregate_path->init_cost = root_path->init_cost;
      aggregate_path->cost =
          root_path->cost + kAggregateOneRowCost * root_path->num_output_rows;
      aggregate_path->num_output_rows_before_filter =
          aggregate_path->num_output_rows;
      aggregate_path->cost_before_filter = aggregate_path->cost;

      ProposeAccessPath(thd, aggregate_path, &new_root_candidates,
                        secondary_engine_cost_hook, "", trace);
    }
    root_candidates = std::move(new_root_candidates);

    Item_sum **func_ptr = join->sum_funcs;
    Item_sum *func;
    bool need_distinct = true;  // We don't support loose index scan yet.
    while ((func = *(func_ptr++))) {
      Aggregator::Aggregator_type type =
          need_distinct && func->has_with_distinct()
              ? Aggregator::DISTINCT_AGGREGATOR
              : Aggregator::SIMPLE_AGGREGATOR;
      if (func->set_aggregator(type) || func->aggregator_setup(thd)) {
        return nullptr;
      }
    }
    if (make_group_fields(join, join)) {
      return nullptr;
    }
  }

  // Apply HAVING, if applicable.
  if (join->having_cond != nullptr) {
    if (trace != nullptr) {
      *trace += "Applying filter for HAVING\n";
    }

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      AccessPath filter_path;
      NewFilterAccessPath(thd, root_path, join->having_cond);
      filter_path.type = AccessPath::FILTER;
      filter_path.filter().child = root_path;
      filter_path.filter().condition = join->having_cond;
      filter_path.num_output_rows =
          root_path->num_output_rows *
          EstimateSelectivity(thd, join->having_cond, trace);
      filter_path.init_cost = root_path->init_cost;
      filter_path.cost =
          root_path->cost + root_path->num_output_rows * kApplyOneFilterCost;
      filter_path.num_output_rows_before_filter = filter_path.num_output_rows;
      filter_path.cost_before_filter = filter_path.cost;

      ProposeAccessPath(thd, &filter_path, &new_root_candidates,
                        secondary_engine_cost_hook, "", trace);
    }
    root_candidates = std::move(new_root_candidates);
  }

  if (join->select_distinct || select_lex->is_ordered()) {
    root_candidates =
        ApplyDistinctAndOrder(thd, select_lex, std::move(root_candidates),
                              secondary_engine_cost_hook, trace);
  }

  // Apply LIMIT, if applicable.
  SELECT_LEX_UNIT *unit = join->unit;
  if (unit->select_limit_cnt != HA_POS_ERROR || unit->offset_limit_cnt != 0) {
    if (trace != nullptr) {
      *trace += "Applying LIMIT\n";
    }
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      AccessPath *limit_path = NewLimitOffsetAccessPath(
          thd, root_path, unit->select_limit_cnt, unit->offset_limit_cnt,
          join->calc_found_rows,
          /*reject_multiple_rows=*/false,
          /*send_records_override=*/nullptr);
      ProposeAccessPath(thd, limit_path, &new_root_candidates,
                        secondary_engine_cost_hook, "", trace);
    }
    root_candidates = std::move(new_root_candidates);
  }

  if (thd->is_error()) return nullptr;

  if (root_candidates.empty()) {
    // The secondary engine has rejected so many access paths that we could not
    // build a complete plan.
    assert(secondary_engine_cost_hook != nullptr);
    my_error(ER_SECONDARY_ENGINE, MYF(0),
             "All plans were rejected by the secondary storage engine.");
    return nullptr;
  }

  // TODO(sgunders): If we are part of e.g. a derived table and are streamed,
  // we might want to keep multiple root paths around for future use, e.g.,
  // if there is a LIMIT higher up.
  AccessPath *root_path =
      *std::min_element(root_candidates.begin(), root_candidates.end(),
                        [](const AccessPath *a, const AccessPath *b) {
                          return a->cost < b->cost;
                        });
  if (trace != nullptr) {
    *trace += StringPrintf("Final cost is %.1f.\n", root_path->cost);
  }

#ifndef DBUG_OFF
  WalkAccessPaths(root_path, join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
                  [&](const AccessPath *path, const JOIN *) {
                    assert(path->cost >= path->init_cost);
                    return false;
                  });
#endif

  join->best_rowcount = lrint(root_path->num_output_rows);
  join->best_read = root_path->cost;

  // 0 or 1 rows has a special meaning; it means a _guarantee_ we have no more
  // than one (so-called “const tables”). Make sure we don't give that
  // guarantee unless we have a LIMIT.
  if (join->best_rowcount <= 1 &&
      unit->select_limit_cnt - unit->offset_limit_cnt > 1) {
    join->best_rowcount = PLACEHOLDER_TABLE_ROW_ESTIMATE;
  }

  return root_path;
}
