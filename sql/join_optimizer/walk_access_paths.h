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

#ifndef SQL_JOIN_OPTIMIZER_WALK_ACCESS_PATHS_H
#define SQL_JOIN_OPTIMIZER_WALK_ACCESS_PATHS_H

#include <type_traits>

#include "sql/join_optimizer/access_path.h"
#include "sql/range_optimizer/range_optimizer.h"

enum class WalkAccessPathPolicy {
  // Stop on _any_ MATERIALIZE, STREAM or TEMPTABLE_AGGREGATE paths, even if
  // they do not cross query blocks.
  // Also stops on APPEND paths, which always cross query blocks.
  STOP_AT_MATERIALIZATION,

  // Stop on MATERIALIZE, STREAM, TEMPTABLE_AGGREGATE or APPEND paths that
  // cross query blocks.
  ENTIRE_QUERY_BLOCK,

  // Do not stop at any kind of access path, unless func() returns true.
  ENTIRE_TREE
};

/**
  Traverse every access path below `path` (possibly limited to the current query
  block with the `cross_query_blocks` parameter), calling func() for each one
  with pre- or post-order traversal. If func() returns true, the traversal does
  not descend into the children of the current path. For post-order traversal,
  the children have already been traversed when func() is called, so it is too
  late to skip them, and the return value of func() is effectively ignored.

  The `join` parameter signifies what query block `path` is part of, since that
  is not implicit from the path itself. The function will track this as it
  changes throughout the tree (in MATERIALIZE or STREAM access paths), and
  will give the correct value to the func() callback. It is only used by
  WalkAccessPaths() itself if the policy is ENTIRE_QUERY_BLOCK; if not, it is
  only used for the func() callback, and you can set it to nullptr if you wish.
  func() must have signature func(AccessPath *, const JOIN *), or it could be
  JOIN * if a non-const JOIN is given in.
 */
template <class AccessPathPtr, class Func, class JoinPtr>
  requires std::is_convertible_v<AccessPathPtr, const AccessPath *> &&
           std::is_convertible_v<JoinPtr, const JOIN *> &&
           std::is_invocable_r_v<bool, Func, AccessPathPtr, JoinPtr>
void WalkAccessPaths(AccessPathPtr path, JoinPtr join,
                     WalkAccessPathPolicy cross_query_blocks, Func &&func,
                     bool post_order_traversal = false) {
  if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK) {
    assert(join != nullptr);
  }

  if (!post_order_traversal) {
    if (func(path, join)) {
      // Stop recursing in this branch.
      return;
    }
  }

  ForEachChild(path, join, cross_query_blocks,
               [&](auto &&subpath, auto &&subjoin) {
                 WalkAccessPaths(subpath, subjoin, cross_query_blocks, func,
                                 post_order_traversal);
               });

  if (post_order_traversal) {
    if (func(path, join)) {
      // Stop recursing in this branch. In practice a no-op, since we are
      // already done with this branch.
      return;
    }
  }
}

/**
  Call a function on every immediate child of the given access path.

  @param path The access path whose children to visit.
  @param join The JOIN object for the current query block. Can be nullptr if
  cross_query_blocks is not ENTIRE_QUERY_BLOCK.
  @param cross_query_blocks Tells whether to stop traversal at materialization
  or query block boundaries.
  @param func The function to call. It takes two arguments: a pointer to an
  access path (the child) and a pointer to the JOIN object for which the access
  path was created.
 */
template <class AccessPathPtr, class Func, class JoinPtr>
  requires std::is_convertible_v<AccessPathPtr, const AccessPath *> &&
           std::is_convertible_v<JoinPtr, const JOIN *> &&
           std::is_invocable_v<Func, AccessPathPtr, JoinPtr>
void ForEachChild(AccessPathPtr path, JoinPtr join,
                  WalkAccessPathPolicy cross_query_blocks, Func &&func) {
  if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK) {
    assert(join != nullptr);
  }

  switch (path->type) {
    case AccessPath::TABLE_SCAN:
    case AccessPath::SAMPLE_SCAN:
    case AccessPath::INDEX_SCAN:
    case AccessPath::INDEX_DISTANCE_SCAN:
    case AccessPath::REF:
    case AccessPath::REF_OR_NULL:
    case AccessPath::EQ_REF:
    case AccessPath::PUSHED_JOIN_REF:
    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::CONST_TABLE:
    case AccessPath::MRR:
    case AccessPath::FOLLOW_TAIL:
    case AccessPath::INDEX_RANGE_SCAN:
    case AccessPath::INDEX_SKIP_SCAN:
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
    case AccessPath::ZERO_ROWS:
    case AccessPath::ZERO_ROWS_AGGREGATED:
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
    case AccessPath::UNQUALIFIED_COUNT:
      // No children.
      break;
    case AccessPath::NESTED_LOOP_JOIN:
      func(path->nested_loop_join().outer, join);
      func(path->nested_loop_join().inner, join);
      break;
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      func(path->nested_loop_semijoin_with_duplicate_removal().outer, join);
      func(path->nested_loop_semijoin_with_duplicate_removal().inner, join);
      break;
    case AccessPath::BKA_JOIN:
      func(path->bka_join().outer, join);
      func(path->bka_join().inner, join);
      break;
    case AccessPath::HASH_JOIN:
      func(path->hash_join().inner, join);
      func(path->hash_join().outer, join);
      break;
    case AccessPath::FILTER:
      func(path->filter().child, join);
      break;
    case AccessPath::SORT:
      func(path->sort().child, join);
      break;
    case AccessPath::AGGREGATE:
      func(path->aggregate().child, join);
      break;
    case AccessPath::TEMPTABLE_AGGREGATE:
      if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_TREE ||
          (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK &&
           path->temptable_aggregate().join == join)) {
        func(path->temptable_aggregate().subquery_path, join);
      }
      func(path->temptable_aggregate().table_path, join);
      break;
    case AccessPath::LIMIT_OFFSET:
      func(path->limit_offset().child, join);
      break;
    case AccessPath::STREAM:
      if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_TREE ||
          (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK &&
           path->stream().join == join)) {
        func(path->stream().child, path->stream().join);
      }
      break;
    case AccessPath::MATERIALIZE:
      func(path->materialize().table_path, join);
      for (const MaterializePathParameters::Operand &operand :
           path->materialize().param->m_operands) {
        if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_TREE ||
            (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK &&
             operand.join == join)) {
          func(operand.subquery_path, operand.join);
        }
      }
      break;
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      func(path->materialize_information_schema_table().table_path, join);
      break;
    case AccessPath::APPEND:
      if (cross_query_blocks == WalkAccessPathPolicy::ENTIRE_TREE) {
        for (const AppendPathParameters &child : *path->append().children) {
          func(child.path, child.join);
        }
      }
      break;
    case AccessPath::WINDOW:
      func(path->window().child, join);
      break;
    case AccessPath::WEEDOUT:
      func(path->weedout().child, join);
      break;
    case AccessPath::REMOVE_DUPLICATES:
      func(path->remove_duplicates().child, join);
      break;
    case AccessPath::REMOVE_DUPLICATES_ON_INDEX:
      func(path->remove_duplicates_on_index().child, join);
      break;
    case AccessPath::ALTERNATIVE:
      func(path->alternative().child, join);
      break;
    case AccessPath::CACHE_INVALIDATOR:
      func(path->cache_invalidator().child, join);
      break;
    case AccessPath::INDEX_MERGE:
      for (AccessPath *child : *path->index_merge().children) {
        func(child, join);
      }
      break;
    case AccessPath::ROWID_INTERSECTION:
      for (AccessPath *child : *path->rowid_intersection().children) {
        func(child, join);
      }
      break;
    case AccessPath::ROWID_UNION:
      for (AccessPath *child : *path->rowid_union().children) {
        func(child, join);
      }
      break;
    case AccessPath::DELETE_ROWS:
      func(path->delete_rows().child, join);
      break;
    case AccessPath::UPDATE_ROWS:
      func(path->update_rows().child, join);
      break;
  }
}

/**
  A wrapper around WalkAccessPaths() that collects all tables under
  “root_path” and calls the given functor, stopping at materializations.
  This is typically used to know which tables to sort or the like.

  func() must have signature func(TABLE *), and return true upon error.
 */
template <class Func>
void WalkTablesUnderAccessPath(AccessPath *root_path, Func &&func,
                               bool include_pruned_tables) {
  WalkAccessPaths(
      root_path, /*join=*/nullptr,
      WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
      [&](AccessPath *path, const JOIN *) {
        switch (path->type) {
          case AccessPath::TABLE_SCAN:
            return func(path->table_scan().table);
          case AccessPath::SAMPLE_SCAN:
            // SampleScan can be executed only in the secondary engine.
            return false; /* LCOV_EXCL_LINE */
          case AccessPath::INDEX_SCAN:
            return func(path->index_scan().table);
          case AccessPath::INDEX_DISTANCE_SCAN:
            return func(path->index_distance_scan().table);
          case AccessPath::REF:
            return func(path->ref().table);
          case AccessPath::REF_OR_NULL:
            return func(path->ref_or_null().table);
          case AccessPath::EQ_REF:
            return func(path->eq_ref().table);
          case AccessPath::PUSHED_JOIN_REF:
            return func(path->pushed_join_ref().table);
          case AccessPath::FULL_TEXT_SEARCH:
            return func(path->full_text_search().table);
          case AccessPath::CONST_TABLE:
            return func(path->const_table().table);
          case AccessPath::MRR:
            return func(path->mrr().table);
          case AccessPath::FOLLOW_TAIL:
            return func(path->follow_tail().table);
          case AccessPath::INDEX_RANGE_SCAN:
            return func(path->index_range_scan().used_key_part[0].field->table);
          case AccessPath::INDEX_SKIP_SCAN:
            return func(path->index_skip_scan().table);
          case AccessPath::GROUP_INDEX_SKIP_SCAN:
            return func(path->group_index_skip_scan().table);
          case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
            return func(path->dynamic_index_range_scan().table);
          case AccessPath::STREAM:
            return func(path->stream().table);
          case AccessPath::MATERIALIZED_TABLE_FUNCTION:
            return func(path->materialized_table_function().table);
          case AccessPath::ALTERNATIVE:
            return func(
                path->alternative().table_scan_path->table_scan().table);
          case AccessPath::UNQUALIFIED_COUNT:
            // Should never be below anything that needs
            // WalkTablesUnderAccessPath().
            assert(false);
            return true;
          case AccessPath::ZERO_ROWS:
            if (include_pruned_tables && path->zero_rows().child != nullptr) {
              WalkTablesUnderAccessPath(path->zero_rows().child, func,
                                        include_pruned_tables);
            }
            return false;
          case AccessPath::WINDOW:
            return func(path->window().temp_table);
          case AccessPath::AGGREGATE:
          case AccessPath::APPEND:
          case AccessPath::BKA_JOIN:
          case AccessPath::CACHE_INVALIDATOR:
          case AccessPath::FAKE_SINGLE_ROW:
          case AccessPath::FILTER:
          case AccessPath::HASH_JOIN:
          case AccessPath::LIMIT_OFFSET:
          case AccessPath::MATERIALIZE:
          case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
          case AccessPath::NESTED_LOOP_JOIN:
          case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
          case AccessPath::REMOVE_DUPLICATES:
          case AccessPath::REMOVE_DUPLICATES_ON_INDEX:
          case AccessPath::SORT:
          case AccessPath::TABLE_VALUE_CONSTRUCTOR:
          case AccessPath::TEMPTABLE_AGGREGATE:
          case AccessPath::WEEDOUT:
          case AccessPath::ZERO_ROWS_AGGREGATED:
          case AccessPath::INDEX_MERGE:
          case AccessPath::ROWID_INTERSECTION:
          case AccessPath::ROWID_UNION:
          case AccessPath::DELETE_ROWS:
          case AccessPath::UPDATE_ROWS:
            return false;
        }
        assert(false);
        return true;
      });
}

#endif  // SQL_JOIN_OPTIMIZER_WALK_ACCESS_PATHS_H
