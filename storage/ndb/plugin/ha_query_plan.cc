/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
/**
  @file

  @brief
  This file contain methods for accessing query plan info used for
  pushing queries and conditions to the ndb data node
  (for execution by the SPJ block).
*/

#include "sql/item.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"

#include "storage/ndb/plugin/ha_ndbcluster_push.h"

class Join_scope;
class Query_scope;

//////////////////////////////////////////////////////////////////////////
////////////////// Implements new AccessPath query plan //////////////////

/**
 * The Join_nest class, and its subclasses, provides a hierarchical tree-like
 * representation of the query and its join operations. It is constructed
 * as a result of traversing the AccessPath structure, and collect
 * query plan information into a tabular structure more suitable for planning
 *
 * The Join_nest are related to a collection of tables being
 * members of the nest(s). There is also a JoinType (INNER, OUTER, SEMI, ANTI)
 * assigned to each nest which describes how tables in the nest is joined
 * relative to tables in the upper nest(s). Join_nests are contained within
 * each other, which is represented with an 'upper_nest' reference, which is
 * the nest we are contained within (An inverse tree, referring the parents, not
 * the children)
 *
 * JoinType::INNER is the default join type for a join nest. Note that there
 * will often be multiple such INNER-Join_nests nested inside each other.
 * Wrt join semantics, such nested INNER joins are redundant, and are thus
 * ignored when we need to find the real boundary of the INNER join nest.
 * (See get_inner_nest(), get_first_inner(), get_last_inner()). However, these
 * extra INNER-Join_nest are useful when representing the general structure
 * of the AccessPath, and the scope of condition filters attached to a subset
 * of the tables.
 *
 * The Join_nest class provide methods for finding the [first..last] range
 * of tables being members of the same set of INNER-joined tables. Such
 * functionality is also provided for a set of SEMI-joined tables.
 *
 * The Join_nest structure itself impose no restrictions on how tables
 * and rows from within the nests are referred. The query executor will iterate
 * the tables being members of the 'nests' in a left deep streaming pattern,
 * using a nested-loop like algorithm, without any temporary buffering or reorg
 * of the intermediate result sets in between. (This property is important for
 * NDB pushed join, as it basically supports only a set of nested-loop-joined
 * tables.)
 *
 * Furthermore, tables being members of different Join_nest branches, where
 * both branches has a common upper join_nest, are said to be in the same
 * join scope. See further detail of how a 'scope' is represented by a subclass
 * of the 'nest' below.
 */
class Join_nest {
 public:
  virtual ~Join_nest() = default;
  Join_nest() = default;

  Join_nest(Join_nest *upper, JoinType type = JoinType::INNER)
      : m_upper_join_scope(upper->get_join_scope()),
        m_type(type),
        m_upper_nest(upper),
        m_first_inner(upper->m_last_inner + 1),
        m_last_inner(upper->m_last_inner) {}

  JoinType get_JoinType() const { return m_type; }

  // Get [first..last] table in a set of INNER-joined tables.
  uint get_first_inner() const;
  uint get_last_inner() const;

  // Get [first..last] table in a set of SEMI-joined(SJ) tables.
  int get_first_sj_inner() const;
  int get_last_sj_inner() const;

  // Get [first..last] table in a set of ANTI-joined tables.
  // Note that an ANTI-join is an OUTER-join as well, the opposite is not true.
  int get_first_anti_inner() const;
  // int get_last_anti_inner() const;  // Not needed

  // If the INNER/SJ tables are nested, get the 'first' table in embedding nest
  int get_first_upper() const;
  int get_first_sj_upper() const;

  // Get a bitmap of tables affected by filter conditions attached to
  // Join_nest(s) between 'this' nest and the specified ancestor nest
  table_map get_filtered_tables(const Join_nest *ancestor) const;

  // Get the Join_scope containing this Join_nest.
  virtual Join_scope *get_join_scope() { return m_upper_join_scope; }

 protected:
  // Refers the Join_scope containing this 'nest'.
  // If this 'Join_nest 'is a' Join_scope', its embedding upper Join_scope
  // is referred (or nullptr)
  Join_scope *const m_upper_join_scope{nullptr};

 private:
  friend struct pushed_table;
  friend class ndb_pushed_builder_ctx;

  // Get the upper'most Join_nest, while still being within 'this' nest
  // (Ignores redundant INNER / SEMI nests)
  Join_nest *get_inner_nest();
  const Join_nest *get_inner_nest() const;
  const Join_nest *get_semi_nest() const;
  const Join_nest *get_anti_nest() const;

  const JoinType m_type{JoinType::INNER};

  Join_nest *const m_upper_nest{nullptr};

  /**
   * Some additional join_nest structures for navigating the nest hiararcy:
   */
  const uint m_first_inner{0};  // The first table represented in this Join_nest
  int m_last_inner{-1};         // The last table in this Join_nest

  /**
   * Map of all tables contained in this Join_nest. If there are multiple nested
   * inner-nests, only the upper most of these, as returned by get_inner_nest()
   * will maintain the inner_map of the nests.
   */
  ndb_table_map m_inner_map;

  /** An optional FILTER on the join_nest */
  const AccessPath *m_filter{nullptr};
};

/**
 * Relative to a Join_nest, tables within a Join_scope may only refer
 * rows or columns from other tables within the same scope or its
 * 'upper' scopes.
 * (In NDB terms: 'refer' -> 'Be part of the same pushed join')
 *
 * In addition to being a Join_nest itself, the Join_scope object contains
 * a bitmap representing all the tables in 'nests' being member of the scope,
 * as well as the sum of all tables in upper-scopes. Note that tables
 * in embedded sub-scopes are not considered to be 'contained'
 *
 * A new Join_scope is typically created when the AccessPath contains an
 * operation which will not be executed in a pure streaming fashion.
 * E.g a HASH_JOIN operand used to build the hash-bucket, a BKA_JOIN operand
 * used to build the key-set, a SORT or AGGREGATE operation first writing the
 * source to a temporary file....
 *
 * Note that from within a Join_scope we may still refer result rows from
 * tables being members of upper-scopes. (NDB: Be members of the same pushed
 * join). However, a Join_scope can only be referred _into_ from scopes/nests
 * having it as an upper_nest. e.g:
 * (Note that a Join_scope 'is a' Join_nest as well)
 *
 *                           ^
 *                      Join_scope(t1,t2)
 *                      ^          ^
 *               Join_nest(t1)     Join_scope(t4)
 *               ^        ^
 *       Join_nest(t2) Join_scope(t3)
 *
 * - t2 may refer only t1.
 *   (t3 and t4 could have been referred if not in 'scope')
 * - t3 may refer (t1,t2) which are member of its upper scope.
 * - t1 can only refer whatever might be 'upper' to scope(t1,t2)
 * - t4 may refer (t1,t2) which are member of its upper scope.
 */
class Join_scope : public Join_nest {
 public:
  virtual ~Join_scope() = default;
  Join_scope() = default;

  Join_scope(Join_nest *upper, const char *descr,
             JoinType type = JoinType::INNER)
      : Join_nest(upper, type),
        m_descr(descr),
        m_upper_query_scope(m_upper_join_scope->get_query_scope()),
        m_all_upper_map(m_upper_join_scope->get_all_tables_map()) {}

  Join_scope *get_join_scope() final { return this; }
  // Get the Query_scope containing this Join_scope.
  virtual const Query_scope *get_query_scope() const {
    return m_upper_query_scope;
  }

 private:
  friend struct pushed_table;
  friend class ndb_pushed_builder_ctx;

  // Get all tables in this Join_scope as well as in upper scopes.
  ndb_table_map get_all_tables_map() const {
    ndb_table_map map(m_scope_map);
    map.add(m_all_upper_map);
    return map;
  }

  // Used only to provide useful explain info
  const char *m_descr{"query"};

  // Refers the Join_scope containing this 'nest'.
  // If this 'Join_nest *is a* Join_scope', the upper scope is referred (or
  // nullptr)
  const Query_scope *const m_upper_query_scope{nullptr};

  // m_all_upper_map are the tables in upper-scope(s) available when
  // constructed. Tables added to upper scopes later, are not available from
  // this scope!
  const ndb_table_map m_all_upper_map;

  // Tables in this Join_scope, not including upper- or sub-scopes.
  ndb_table_map m_scope_map;
};

/**
 * A Join_scope(above) is always contained within a Query_scope.
 * (which in turn might be contained in another Query_scope...)
 *
 * The Query_scope further restrict how a Join_scope may refer
 * tables in upper-scopes. This is related to the implementation
 * of the two methods:
 *
 * - get_tables_in_this_query_scope() -> Tables limited to this Query_scope
 * - get_tables_in_all_query_scopes() -> Include tables in upper scopes as well
 *
 * A Query_scope is typically created when there are some requirements of
 * the entire AccessPath branch to be fully evaluated before we return to
 * upper levels. This is typically for e.g.:
 *
 *  - An agreggate sub-path, where all rows matching some outer references
 *    must be collected before finding a min/max
 *  - A materialized sub query, where all rows fulfilling the outer/upper
 *    reference must be collected for the subquery to be complete.
 *
 * Note that the above 'fully evaluated' is not guaranteed if not enforced
 * with a Query_scope. (might come as a surprise). Due to the batching fetch
 * of rows from NDB, result generation may 'give up' in specific AccessPath
 * branch when all batched rows has been consumed there. It then returns
 * to upper levels to see what can be generated there. Finally when the all
 * batches rows are consumed, the entire pipeline is refilled, and we take
 * anothed dive into the branches. Thus result rows might be generated out of
 * sequence, in multiple rounds, relative to upper rows. This is within specs
 * for join operations as a join implies now ordering of the individual rows.
 *
 * However, some of the Iterators which implements the AccessPath operations
 * has such requirements. Thus a Query_scope must be inserted wherever required
 * by them.
 *
 * Wrt. NDB pushed joins it will be restricted to be entirely within
 * the Query_scope. However, (pushed-)conditions and keys used in the
 * pushed joins may still refer to table values from all upper-scopes,
 * even if the table itself can't be a member of the same pushed join.
 *
 * Note that in its most common form, the Join_nest structure will
 * consist of single Query_scope, (which 'is a' Join_scope as well,)
 * containing a set of Join_nests.
 *
 * The simplest form will be just a Query_scope, (which 'is a' Join_scope,
 * as well as a Join_nest), containing a single table.
 * (There is always a topmost Query_scope)
 *
 * Having multiple nested Query_scopes is more the exception, where there
 * is Aggregates, or materialize queries
 */
class Query_scope : public Join_scope {
 public:
  virtual ~Query_scope() = default;
  Query_scope() = default;

  Query_scope(Join_nest *upper, const char *descr) : Join_scope(upper, descr) {}

  const Query_scope *get_query_scope() const final { return this; }
};

static JoinType getHashJoinType(RelationalExpression::Type join_type) {
  switch (join_type) {
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      return JoinType::INNER;
    case RelationalExpression::LEFT_JOIN:
      return JoinType::OUTER;
    case RelationalExpression::ANTIJOIN:
      return JoinType::ANTI;
    case RelationalExpression::SEMIJOIN:
      return JoinType::SEMI;
    case RelationalExpression::FULL_OUTER_JOIN:
      assert(false);
      [[fallthrough]];
    case RelationalExpression::TABLE:
    case RelationalExpression::MULTI_INNER_JOIN:
    default:
      // Should never end up here, something need to be returned though...
      assert(false);
      return JoinType::INNER;
  }
}

/**
 * Construct the 'ndb_pushed_builder_ctx', representing the 'query plan'.
 * It mainly consiste of an array of 'pushed_table' objects, each representing
 * a 'basic table' (a leaf) in the AccessPath. Furthermore, each pushed_table
 * has attached a Join_nest object, describing the INNER, OUTER, ... join
 * structure relating it to other tables.
 *
 * In its current implementation it is more a representation of how
 * the different tables are joined together (INNER, SEMI, OUTER or HASH)
 * than a complete 'query plan' as such.
 *
 * Operations not combining (i.e. 'joining') two tables, or branches of
 * the AccessPath are generally not represented (yet).
 * That is, operations like aggregate, sort, duplicate elimination are
 * simply skipped through. Thus left to the server to handle.
 * (Might change in the future)
 *
 * The exception is FILTER operations where the condition is attached
 * to the Join_nest they belongs to. They are candidates for later being
 * 'pushed conditions'.
 *
 * Construction of the query plan recursively traverse the AccessPath structure,
 * building a Join_nest structure as describe above to represent the join
 * structure in the query. Tables, being the leaf nodes in the AccessPath,
 * are additionally collected in their own list.
 *
 * Traversal of the AccessPath structure is done left-deep (except HASH),
 * reflecting the same execution order as used by the Iterators.
 * However, note that this ^^^^^ is only true when still inside the same
 * Join_nest. We construct new Join_scopes, and in particular Query_scopes.
 * when we encounter an AccessPath operation where such an 'always left deep'
 * access pattern can not longer be guaranteed.
 *
 * This becomes an important part of the Join_nest structures when we later
 * need to analyze which part of the query could be part of the same
 * pushed join
 */
void ndb_pushed_builder_ctx::construct(const AccessPath *root_path) {
  construct(new (m_thd->mem_root) Query_scope(), root_path);
}

void ndb_pushed_builder_ctx::construct(Join_nest *nest_ctx,
                                       const AccessPath *path) {
  switch (path->type) {
    // Basic access paths referring a table.
    case AccessPath::TABLE_SCAN:
    case AccessPath::INDEX_SCAN:
    case AccessPath::REF:
    case AccessPath::REF_OR_NULL:
    case AccessPath::EQ_REF:
    case AccessPath::PUSHED_JOIN_REF:
    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::CONST_TABLE:
    case AccessPath::MRR:
    case AccessPath::FOLLOW_TAIL:
    case AccessPath::INDEX_RANGE_SCAN:
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
    // INDEX_MERGE is not 'basic' as it also refer indexes,
    // but a 'table' nevertheless.
    case AccessPath::INDEX_MERGE: {
      // Add tab_no as member to Join_nest and Join_scope
      const uint tab_no = m_table_count++;
      Join_nest *join_nest = nest_ctx->get_inner_nest();
      join_nest->m_inner_map.add(tab_no);
      Join_scope *join_scope = nest_ctx->get_join_scope();
      join_scope->m_scope_map.add(tab_no);

      // Fill in m_tables[]
      TABLE *const table = GetBasicTable(path);
      assert(table != nullptr);
      m_tables[tab_no].m_join_nest = nest_ctx;
      m_tables[tab_no].m_tab_no = tab_no;
      m_tables[tab_no].m_path = path;
      m_tables[tab_no].m_table = table;
      m_tables[tab_no].m_filter = nest_ctx->m_filter;
      m_tables[tab_no].compute_type_and_index();
      nest_ctx->m_filter = nullptr;  // Transferred to m_tables[]
      break;
    }
    // Basic access paths that don't correspond to a specific table.
    // Register in m_tables[] anyway for completnes
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
    case AccessPath::ZERO_ROWS:
    case AccessPath::ZERO_ROWS_AGGREGATED:
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
    case AccessPath::UNQUALIFIED_COUNT: {
      // Add tab_no as member to Join_nest and Join_scope
      const uint tab_no = m_table_count++;
      Join_nest *join_nest = nest_ctx->get_inner_nest();
      join_nest->m_inner_map.add(tab_no);
      Join_scope *join_scope = nest_ctx->get_join_scope();
      join_scope->m_scope_map.add(tab_no);

      // Fill in m_tables[], note that there is no 'table'
      m_tables[tab_no].m_join_nest = nest_ctx;
      m_tables[tab_no].m_tab_no = tab_no;
      m_tables[tab_no].m_path = path;
      m_tables[tab_no].m_table = nullptr;
      m_tables[tab_no].m_filter = nullptr;
      break;
    }
    case AccessPath::NESTED_LOOP_JOIN: {
      const auto &param = path->nested_loop_join();
      const JoinType type = param.join_type;
      construct(new (m_thd->mem_root) Join_nest(nest_ctx), param.outer);
      construct(new (m_thd->mem_root) Join_nest(nest_ctx, type), param.inner);
      break;
    }
    case AccessPath::BKA_JOIN: {
      const auto &param = path->bka_join();
      const JoinType type = param.join_type;
      // BKA Keys are generated from the 'outer'(left) operand. These are
      // collected into the join_buffer, which has to fully contain a pushed
      // join. Thus they need to start its own Join_scope.
      construct(new (m_thd->mem_root) Join_scope(nest_ctx, "batched-keys"),
                param.outer);

      // The 'inner' operand is a MRR, using the collected keys.
      //
      // Note that even if we here allow the inner branch to continue in the
      // same Join_nest's as called with, NDB join push down does not implement
      // pushdown of MRR as a child operation. However that is a implementation
      // limitations, not a Join_scope / Join_nest issue.
      construct(new (m_thd->mem_root) Join_nest(nest_ctx, type), param.inner);
      break;
    }
    case AccessPath::HASH_JOIN: {
      const auto &param = path->hash_join();
      const RelationalExpression::Type relationalExprType =
          param.rewrite_semi_to_inner ? RelationalExpression::INNER_JOIN
                                      : param.join_predicate->expr->type;
      const JoinType type = getHashJoinType(relationalExprType);

      ///////////////////////////////////
      // Note that HASH-join do not access the tables in the left-deep
      // order as explained in the tree-format. The right inner-branch is
      // always read into the hash-bucket first, then the left outer-branch
      // is 'probed' against the rows in the bucket.
      //
      // The 'traditional' explain format however will list the table
      // with the lowest 'cost' first, independent of whether that table
      // goes into the hash bucket or not.
      // Thus, the two explain formats may be confusing wrt. the relative
      // access order of the tables. To create the least explain confusion
      // we try to mimic the same table order as the 'traditional' format.
      //
      // This explains the different inner/outer construct traversal order
      // below for different join types. Doing it the other way around should
      // work as well, but would be slightly less user frendly wrt. 'explain'
      // and 'show warning' (wrt. why a certain table was not-pushed.)
      if (type == JoinType::INNER) {  // Note: CROSS-join is an INNER-join
        // Note, 'inner' / 'outer' swapped relative to other operations.
        construct(new (m_thd->mem_root) Join_scope(nest_ctx, "hash-bucket"),
                  param.inner);
        construct(new (m_thd->mem_root) Join_nest(nest_ctx, type), param.outer);
      } else {
        // Note(NDB): If the 'probe' branch has pushed join members outside
        // of the branch, we need to disable 'spill_to_disk' strategy. That is
        // only possible for an INNER-join, thus for non-INNER the 'probe'
        // need to be embedded in its own Join_scope.
        // See fixupPushedAccessPaths() for 'spill_to_disk' disable.
        construct(new (m_thd->mem_root) Join_scope(nest_ctx, "hash-probe"),
                  param.outer);
        construct(new (m_thd->mem_root) Join_scope(nest_ctx, "hash-bucket"),
                  param.inner);
      }
      break;
    }
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL: {
      // Is a fused REMOVE_DUPLICATES_ON_INDEX + NESTED_LOOP_JOIN(SEMI)!
      //
      // NestedLoopSemiJoinWithDuplicateRemovalIterator require 'outer' to
      // be returned 'ordered' on an index. Duplicates are eliminated from
      // 'outer', and a single 'firstMatch' (->SEMI-join) found from 'inner'.
      // As we are effectively returning only a 'firstMatch' from the
      // de-duplicated 'outer', that becomes a SEMI-nets as well:
      // ( -> TWO SEMI-nests inside each other)
      // TODO: Invent a special Ordered_nest, (which is handled as a SEMI) ?:
      const auto &param = path->nested_loop_semijoin_with_duplicate_removal();
      Join_nest *join_nest =
          new (m_thd->mem_root) Join_nest(nest_ctx, JoinType::SEMI);
      construct(join_nest, param.outer);
      construct(new (m_thd->mem_root) Join_nest(join_nest, JoinType::SEMI),
                param.inner);
      break;
    }
    case AccessPath::SORT: {
      // Even if we could possibly have allowed some SORT variants to refer
      // tables outside the sorted-scope, we are conservative and always
      // embed the sorted branch in its own Join_scope.
      construct(new (m_thd->mem_root) Join_scope(nest_ctx, "sorted"),
                path->sort().child);
      break;
    }
    case AccessPath::LIMIT_OFFSET: {
      const uint limit =
          path->limit_offset().limit + path->limit_offset().offset;
      Join_nest *join_nest = nest_ctx;
      if (limit == 1) {
        // 'LIMIT 1' is often used as one (out of 5!) different ways of
        // implementation a 'confluent' semi-join.(See optimizer) We need to
        // recognize it as such and create a SEMI-join nest for it.
        // Note that even of it originated as a real 'LIMIT 1' clause, this
        // should not hurt either.
        join_nest = new (m_thd->mem_root) Join_nest(nest_ctx, JoinType::SEMI);
      } else {
        // Don't push a FILTER into tables below a LIMIT
        // Never seen this combination - Will like to investigate if seen.
        assert(join_nest->m_filter == nullptr);
        join_nest->m_filter = nullptr;
      }
      construct(join_nest, path->limit_offset().child);
      break;
    }
    case AccessPath::FILTER: {
      Join_nest *join_nest = new (m_thd->mem_root) Join_nest(nest_ctx);
      join_nest->m_filter = path;
      construct(join_nest, path->filter().child);
      break;
    }

    /**
     * Note that most of the AccessPath operation below construct
     * a 'Query_scope' for its child operations, thus limiting how
     * 'upper' scopes can be referred from the child source.
     * See further reasoning in 'class Query_scope' declaration.
     * ... For some AccessPath types, Query_scope may be used just
     * to be on the conservative side as well - A less restrictive
     * Join_scope could possibly have been sufficient.
     *
     * NDB: We find hardly any differences at all between constructing
     * a Join_scope vs. Query_scope. Seems to be partly due to these
     * operations not being so frequently used, and having other limitation
     * as well, restricting join pushdowns.
     */
    case AccessPath::AGGREGATE: {
      // Is 'streaming', e.i no temp storage and reordering of rows.
      // However, it also require all child rows to be returned
      // in same batch groups as its parent. -> Query_scope
      construct(new (m_thd->mem_root) Query_scope(nest_ctx, "aggregated"),
                path->aggregate().child);
      break;
    }
    case AccessPath::TEMPTABLE_AGGREGATE: {
      // Aggregate via a temporary file
      construct(new (m_thd->mem_root)
                    Query_scope(nest_ctx, "aggregated-tempfile"),
                path->temptable_aggregate().subquery_path);
      break;
    }
    case AccessPath::STREAM: {
      const auto &param = path->stream();
      if (param.join == m_join) {  // Within same Query_block?
        // A Join_scope context would possibly be sufficient?
        // To be safe we use the more restrictive Query_scope.
        construct(new (m_thd->mem_root) Query_scope(nest_ctx, "streamed"),
                  param.child);
      }
      break;
    }
    case AccessPath::MATERIALIZE: {
      MaterializePathParameters *param = path->materialize().param;
      for (const MaterializePathParameters::Operand &operand :
           param->m_operands) {
        // MATERIALIZE are evaluated and stored in a temporary table.
        // They comes in different variants, where they may be 'const',
        // later scanned, or a temporary index created for later lookups.
        // Generally we need to handle them as completely separate queries,
        // without any relation to an upper Join_scope -> 'Query_scope'
        if (operand.join == m_join) {  // Within Query_block?
          construct(new (m_thd->mem_root) Query_scope(nest_ctx, "materialized"),
                    operand.subquery_path);
        }
      }
      break;
    }
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE: {
      construct(new (m_thd->mem_root) Query_scope(nest_ctx, "schema"),
                path->materialize_information_schema_table().table_path);
      break;
    }
    case AccessPath::APPEND: {
      const auto &param = path->append();
      for (const AppendPathParameters &child : *param.children) {
        assert(child.join == m_join);  // Within Query_blocks
        construct(new (m_thd->mem_root) Query_scope(nest_ctx, "query_block"),
                  child.path);
      }
      break;
    }
    case AccessPath::WINDOW: {
      construct(new (m_thd->mem_root) Query_scope(nest_ctx, "window"),
                path->window().child);
      break;
    }

    /**
     * The optimizer use (at least) 5 different way of evaluating SEMI
     * joins. The more obvious ones are producing a NESTED_LOOP, BKA or HASH
     * join, with JoinType::SEMI specified.
     * In addition to that it might specify 'LIMIT 1' for the inner branch,
     * MATERIALIZE it, or use different duplicate elimination algorithms
     * as handled below - All these may occur within a Join_nest where
     * JoinType is _not_ specified as SEMI.
     * For NDB being able to correctly produce pushed SEMI join results,
     * it need to be aware of when the JoinType is SEMI. (Avoiding duplicates
     * when multiple scan batches are needed). Thus we add a SEMI
     * Join_scope around the child branches below.
     */
    case AccessPath::WEEDOUT: {  // Is only partly a SEMI-join
      // Weedout will do duplicate elimination on only *some* of the
      // tables in the weedout branch. Which tables are specified with
      // the 'tables_to_get_rowid_for' in the weedout struct. These are
      // the to-be-semi-joined tables in the weedout. In addition,
      // there will also be tables which we should not duplicate-eliminate
      // in the weedout-branch. (The not-semi-joined tables)
      //
      // Thus, we can unfortunately not make a SEMI-nest of the weedout
      // branch. Instead we need to take the more restrictive approach
      // of handling it as a separate Query_scope. That is: The weedout branch
      // is handled as a separate query, where no tables outside the branch
      // can be members of any pushed joins inside the branch.
      //
      construct(new (m_thd->mem_root)
                    Query_scope(nest_ctx, "duplicate-weedout"),
                path->weedout().child);
      break;
    }
    case AccessPath::REMOVE_DUPLICATES_ON_INDEX: {  // SEMIJOIN(LOOSESCAN)
      // Explain: 'Remove duplicates from input sorted on <index>'
      //
      // Use an ordered index, which returns rows in sorted order.
      // Duplicates on (part of) key are skipped, thus effectively defining
      // a 'firstMatch' (-> SEMI-join) operation on the child source
      //
      // FUTURE?: Use a 'NO_DUPLICATE' scope type, if an unpushed filter is not
      // encountered: Set a firstMatch join type from handler, else enforce
      // index order'ed result set (Which have significant performance impact)
      construct(new (m_thd->mem_root) Join_nest(nest_ctx, JoinType::SEMI),
                path->remove_duplicates_on_index().child);
      break;
    }
    case AccessPath::REMOVE_DUPLICATES: {  // is a SEMI-join
      // Explain: "Remove duplicates from input grouped on <columns>"
      //
      // Does a loosescan type access on (multiple) sources known to be
      // suitable sorted. Possible by either doing a sorted index scan,
      // or an explicit SORT added to the source table(s).
      //
      // Only generated from the (non-default) Hypergraph-optimizer.
      // It should be handled in a similar way as *_ON_INDEX above.
      //
      construct(new (m_thd->mem_root) Join_nest(nest_ctx, JoinType::SEMI),
                path->remove_duplicates().child);
      break;
    }
    case AccessPath::ALTERNATIVE: {
      // Access the same table, either with a scan or a lookup.
      // Following 'child' will bring us to the TABLE_SCAN access.
      construct(nest_ctx, path->alternative().child);
      break;
    }
    case AccessPath::CACHE_INVALIDATOR: {  // 'Invalidate materialized tables'
      construct(new (m_thd->mem_root) Join_nest(nest_ctx),
                path->cache_invalidator().child);
      break;
    }
    case AccessPath::DELETE_ROWS: {
      construct(new (m_thd->mem_root) Join_nest(nest_ctx),
                path->delete_rows().child);
      break;
    }
    case AccessPath::UPDATE_ROWS: {
      construct(new (m_thd->mem_root) Join_nest(nest_ctx),
                path->update_rows().child);
      break;
    }
    /////////////// Not fully supported yet ////////////////////////////
    // TODO: Some new Accesspath operations, how to handle these?
    // Believe they are only some unexposed path types used as part of
    // RANGE_SCAN. For now, just catch any use with an assert().
    case AccessPath::ROWID_INTERSECTION: {
      for (AccessPath *child : *path->rowid_intersection().children) {
        (void)child;  // Do nothing yet
      }
      assert(path->type != AccessPath::ROWID_INTERSECTION);
      break;
    }
    case AccessPath::ROWID_UNION: {
      for (AccessPath *child : *path->rowid_union().children) {
        (void)child;  // Do nothing yet
      }
      assert(path->type != AccessPath::ROWID_UNION);
      break;
    }
    ///////////////////
    // New table access types:
    // They are actually a combination of access type and operation as they
    // also control which rows being returned - not only 'how to' access it.
    // Believe they are only generated by the HG optimizer.
    // NOTE: Assumed to read the table in sorted order.
    // For now, just catch any use with an assert().
    case AccessPath::INDEX_SKIP_SCAN:
      (void)path->index_skip_scan().table;  // A SORTED table?
      assert(path->type != AccessPath::INDEX_SKIP_SCAN);
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:  // A SORTED table?
      (void)path->group_index_skip_scan().table;
      assert(path->type != AccessPath::GROUP_INDEX_SKIP_SCAN);
      break;
    default:
      assert(false);  // Detect unhandled AccessPath-type
      break;
  }
  const uint last_table = m_table_count - 1;
  nest_ctx->m_last_inner = last_table;

  // In case this is the last table in this nest before returning to upper-nest,
  // we need to update upper->last_inner as well
  if (nest_ctx->m_upper_nest != nullptr) {
    nest_ctx->m_upper_nest->m_last_inner = last_table;
  }
}

/**
  Compute the access type and index (if applicable) of this operation.
*/
void pushed_table::compute_type_and_index() {
  DBUG_TRACE;
  switch (m_path->type) {
    case AccessPath::EQ_REF: {
      const Index_lookup *ref = m_path->eq_ref().ref;
      m_index_no = ref->key;

      if (m_index_no == static_cast<int>(m_table->s->primary_key)) {
        DBUG_PRINT("info", ("Operation %d is a primary key lookup.", m_tab_no));
        m_access_type = AT_PRIMARY_KEY;
      } else {
        DBUG_PRINT("info",
                   ("Operation %d is a unique index lookup.", m_tab_no));
        m_access_type = AT_UNIQUE_KEY;
      }
      break;
    }
    case AccessPath::REF: {
      /**
       * NOTE: From optimizer POW, REF access means: 'may return multiple rows'.
       * This does not necessarily mean that a range type access operation is
       * used by the storage engine, even if that is the most likely case.
       * In particular, if the (UNIQUE) HASH-index type is used (NDB), we have
       * to take care: If the key contain NULL values it will degrade to a
       * full table scan, else it will be an unique single row lookup.
       * (i.e, can never be an index scan as suggested by type = REF!)
       */
      const Index_lookup *ref = m_path->ref().ref;
      m_index_no = ref->key;

      const KEY *key_info = m_table->s->key_info;
      if (unlikely(key_info[m_index_no].algorithm == HA_KEY_ALG_HASH)) {
        /**
         * Note that there can still be NULL values in the key if
         * it is constructed from Item_fields referring other tables.
         * This is not known until execution time, so below we do
         * a best guess about no NULL values:
         */
        // PK is fully null_rejecting, so can't be the PRIMARY KEY
        assert(m_index_no != static_cast<int>(m_table->s->primary_key));
        m_access_type = AT_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique key referrence.", m_tab_no));
      } else {
        m_access_type = AT_ORDERED_INDEX_SCAN;
        DBUG_PRINT("info", ("Operation %d is an index scan.", m_tab_no));
      }
      break;
    }
    case AccessPath::INDEX_SCAN: {
      // Note that an INDEX_SCAN usually 'use_order'.
      // In such cases it should only be either the root, or a child
      // being duplicate eliminated. (Checked in ::is_pushable_as_child())
      const auto &param = m_path->index_scan();
      m_index_no = param.idx;
      m_access_type = AT_ORDERED_INDEX_SCAN;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;
    }
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN: {
      /*
        It means that the decision on which access method to use
        will be taken late (as rows from the preceding operation arrive).
        This operation is therefore not pushable.
      */
      DBUG_PRINT("info", ("Operation %d has 'dynamic range' -> not pushable",
                          m_tab_no));
      m_access_type = AT_UNDECIDED;
      m_index_no = -1;
      break;
    }
    case AccessPath::INDEX_MERGE: {
      // Is a range_scan using the index_merge access type.
      // It produce a set of (MULTIPLE) PK's from the MERGEed indexes.
      m_index_no = m_table->s->primary_key;
      m_access_type = AT_MULTI_PRIMARY_KEY;
      m_other_access_reason = "Index-merge";
      break;
    }
    /**
     * Note that both INDEX_RANGE_SCAN and MRR use the 'multi-range-read'
     * handler interface, thus they are quite similar.
     *  - INDEX_RANGE_SCAN is generated by the range optimizer, while
     *  - MRR is the inner part of a BKA operation, getting its range_keys
     * from the outer BKA operand. Both operate on a set of ranges.
     */
    case AccessPath::INDEX_RANGE_SCAN: {
      const KEY *key_info = m_table->s->key_info;
      DBUG_EXECUTE("info", dbug_dump(m_path, 0, true););
      m_index_no = used_index(m_path);
      if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH) {
        m_access_type =
            (m_index_no == static_cast<int>(m_table->s->primary_key))
                ? AT_MULTI_PRIMARY_KEY
                : AT_MULTI_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique 'range' referrence.", m_tab_no));
      } else {
        // Note that there can still be single row lookups in the 'MIX'
        m_access_type = AT_MULTI_MIXED;
        DBUG_PRINT("info", ("Operation %d is an range scan.", m_tab_no));
      }
      m_other_access_reason = "Range-scan";
      break;
    }
    case AccessPath::MRR: {
      const Index_lookup *ref = m_path->mrr().ref;
      const KEY *key_info = m_table->s->key_info;
      m_index_no = ref->key;
      assert(m_index_no != MAX_KEY);

      if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH) {
        m_access_type =
            (m_index_no == static_cast<int>(m_table->s->primary_key))
                ? AT_MULTI_PRIMARY_KEY
                : AT_MULTI_UNIQUE_KEY;
        DBUG_PRINT("info",
                   ("Operation %d is an unique mrr-key referrence.", m_tab_no));
      } else {
        // Note that there can still be single row lookups in the 'MIX'
        m_access_type = AT_MULTI_MIXED;
        DBUG_PRINT("info", ("Operation %d is an mrr index scan.", m_tab_no));
      }

      if (m_table->in_use->lex->is_explain()) {
        // Align possible 'EXPLAIN_NO_PUSH' with explain format being used.
        // MRR is explained as a 'Multi range' with iterator-based formats
        // else 'Batched..'
        if (m_table->in_use->lex->explain_format->is_iterator_based(
                m_table->in_use, m_table->in_use)) {
          m_other_access_reason = "Multi-range";
        } else {
          m_other_access_reason = "Batched-key";
        }
      }
      break;
    }
    case AccessPath::TABLE_SCAN: {
      DBUG_PRINT("info", ("Operation %d is a table scan.", m_tab_no));
      m_access_type = AT_TABLE_SCAN;
      break;
    }
    case AccessPath::REF_OR_NULL: {
      DBUG_PRINT("info",
                 ("Operation %d is REF_OR_NULL. (REF + SCAN)", m_tab_no));
      m_access_type = AT_UNDECIDED;  // Is both a REF *and* a SCAN
      break;
    }

    /////////////////////////////////////////
    // Not yet seen *_SKIP_SCAN AccessPath in any test cases.
    // Believe they are only generated from the HG optimizer.
    // Handle them as required in later HG-integration bug reports
    case AccessPath::INDEX_SKIP_SCAN:
      m_access_type = AT_OTHER;
      m_other_access_reason = "'Index skip scan'-AccessPath not handled yet.";
      m_index_no = -1;  // used_index(m_path);
      assert(false);
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      m_access_type = AT_OTHER;
      m_other_access_reason =
          "'Group index skip scan'-AccessPath not handled yet.";
      m_index_no = -1;  // used_index(m_path);
      assert(false);
      break;
    case AccessPath::FOLLOW_TAIL:  // A recursive reference to table
      m_access_type = AT_OTHER;
      m_other_access_reason = "'Follow tail'-AccessPath not implemented.";
      m_index_no = -1;
      assert(false);
      break;

    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::CONST_TABLE:
    default:
      DBUG_PRINT("info",
                 ("Operation %d of AccessPath::Type %d. -> Not pushable.",
                  m_tab_no, m_path->type));
      m_access_type = AT_OTHER;
      m_index_no = -1;
      m_other_access_reason = "This table access method can not be pushed.";
      break;
  }
}

//////////////////////////////////////////
// Get'ers for Join_nest

/**
 * Get the 'real' enclosing Join_nest:
 * Note that additional inner Join_nest might be added inside other Join_nests.
 * This is used when the 'plan' is construct'ed to represent the
 * query tree structure, where e.g. a filter spawns only part of the tables
 * in an inner Join_nest.
 *
 * As all tables inside the same Join_nest are inner-joined with each other,
 * such nested inner Join_nest are redundant wrt the join semantic.
 *
 * Thus this method finds the upper Join_nest which is not an INNER, which
 * defines the join semantic relative to tables not in this upper Join_nest.
 *
 * Note: SEMI is (kind of) an INNER join, returning only the first row.
 * Note: The uppermost Join_scope is always an INNER-join.
 */
const Join_nest *Join_nest::get_inner_nest() const {
  const Join_nest *nest = this;
  while (nest->m_upper_nest != nullptr &&
         (nest->get_JoinType() == JoinType::INNER ||
          nest->get_JoinType() == JoinType::SEMI)) {
    nest = nest->m_upper_nest;
  }
  return nest;
}
Join_nest *Join_nest::get_inner_nest() {
  Join_nest *nest = this;
  while (nest->m_upper_nest != nullptr &&
         (nest->get_JoinType() == JoinType::INNER ||
          nest->get_JoinType() == JoinType::SEMI)) {
    nest = nest->m_upper_nest;
  }
  return nest;
}

/**
 * Get enclosing SEMI- or ANTI-join nest, or nullptr
 * if no such nests exists.
 */
const Join_nest *Join_nest::get_semi_nest() const {
  // Sufficient that any ancestor-nest is a SEMI join
  const Join_nest *nest = this;
  do {
    if (nest->get_JoinType() == JoinType::SEMI) {
      return nest;
    }
    nest = nest->m_upper_nest;
  } while (nest != nullptr);
  return nest;
}
const Join_nest *Join_nest::get_anti_nest() const {
  // Sufficient that any ancestor-nest is an ANTI join
  const Join_nest *nest = this;
  do {
    if (nest->get_JoinType() == JoinType::ANTI) {
      return nest;
    }
    nest = nest->m_upper_nest;
  } while (nest != nullptr);
  return nest;
}

/**
 * Returns the first/last table in the join-nest this table is a member of.
 * We enumerate the uppermost nest to range from [0..#tables-1].
 *
 * The first_upper reference to this range is '0'.
 * Note, that first_upper of the uppermost nest is still negative.
 */
uint Join_nest::get_first_inner() const {
  const Join_nest *inner_nest = get_inner_nest();
  return inner_nest->m_first_inner;
}
uint Join_nest::get_last_inner() const {
  const Join_nest *inner_nest = get_inner_nest();
  return static_cast<uint>(inner_nest->m_last_inner);
}
int Join_nest::get_first_upper() const {
  const Join_nest *inner_nest = get_inner_nest();
  if (inner_nest->m_upper_nest == nullptr) return -1;
  return inner_nest->m_upper_nest->get_first_inner();
}

/**
 * Returns the first/last table in a semi-join nest.
 * Returns <0 if table is not part of a semi-join nest.
 */
int Join_nest::get_first_sj_inner() const {
  const Join_nest *nest = get_semi_nest();
  return (nest != nullptr) ? nest->m_first_inner : -1;
}
int Join_nest::get_last_sj_inner() const {
  const Join_nest *nest = get_semi_nest();
  return (nest != nullptr) ? nest->m_last_inner : -1;
}
int Join_nest::get_first_sj_upper() const {
  const Join_nest *nest = get_semi_nest();
  if (nest == nullptr) return -1;
  // A SJ nest will have at least an inner-nest as 'upper'
  assert(nest->m_upper_nest != nullptr);
  return nest->m_upper_nest->get_first_sj_inner();
}

/**
 * Returns the first table in this anti-join nest.
 * Returns <0 if table is not part of an anti-join nest.
 */
int Join_nest::get_first_anti_inner() const {
  const Join_nest *nest = get_anti_nest();
  return (nest != nullptr) ? nest->m_first_inner : -1;
}

/**
 * Get a bitmap of all tables between this nest and ancestor nest
 * affected by FILTER(s)
 */
table_map Join_nest::get_filtered_tables(const Join_nest *ancestor) const {
  const Join_nest *nest = this;
  const uint ancestor_first_inner = ancestor->m_first_inner;
  table_map filter_map(0);
  while (nest->m_first_inner > ancestor_first_inner) {
    if (nest->m_filter != nullptr) {
      filter_map |=
          GetUsedTableMap(nest->m_filter, /*include_pruned_tables=*/false);
    }
    nest = nest->m_upper_nest;
  }
  return filter_map;
}

//////////////////////////////////////////
// Get'ers for pushed_table

const Index_lookup *pushed_table::get_table_ref() const {
  switch (m_path->type) {
    case AccessPath::EQ_REF:
      return m_path->eq_ref().ref;
    case AccessPath::REF:
      return m_path->ref().ref;
    case AccessPath::MRR:
      return m_path->mrr().ref;
    case AccessPath::REF_OR_NULL:
      return m_path->ref_or_null().ref;
    case AccessPath::FULL_TEXT_SEARCH:
      return m_path->full_text_search().ref;
    case AccessPath::CONST_TABLE:
      return m_path->const_table().ref;
    case AccessPath::INDEX_SCAN:
    case AccessPath::INDEX_RANGE_SCAN:
      // Might be requested, but rejected later
      return nullptr;
    default:
      return nullptr;
  }
}

/**
   Estimate number of rows returned from data nodes.
   We assume that any filters are pushed down.
*/
double pushed_table::num_output_rows() const {
  if (m_filter != nullptr)
    return m_filter->num_output_rows();
  else
    return m_path->num_output_rows();
}

/**
   Check if the specified AccessPath operation require the result
   to be returned using the index order.
*/
bool pushed_table::use_order() const {
  switch (m_path->type) {
    // case AccessPath::EQ_REF:
    //  return m_path->eq_ref().use_order;
    case AccessPath::REF:
      return m_path->ref().use_order;
    case AccessPath::REF_OR_NULL:
      return m_path->ref_or_null().use_order;
    case AccessPath::INDEX_SCAN:
      return m_path->index_scan().use_order;
    case AccessPath::FULL_TEXT_SEARCH:
      return m_path->full_text_search().use_order;

    // MRR based access methods might be sorted as well.
    // Included for completeness, but seems to be unused wrt. SPJ.
    case AccessPath::INDEX_RANGE_SCAN:
      return m_path->index_range_scan().mrr_flags & HA_MRR_SORTED;
    case AccessPath::MRR:
      return m_path->mrr().mrr_flags & HA_MRR_SORTED;
    default:
      return false;
  }
}

/**
  Get the number of key values for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
uint pushed_table::get_no_of_key_fields() const {
  const Index_lookup *ref = get_table_ref();
  if (ref == nullptr) return 0;
  return ref->key_parts;
}

/**
  Get the field_no'th key values for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
const Item *pushed_table::get_key_field(uint field_no) const {
  const Index_lookup *ref = get_table_ref();
  if (ref == nullptr) return nullptr;
  assert(field_no < get_no_of_key_fields());
  return ref->items[field_no];
}

/**
  Get the field_no'th KEY_PART_INFO for this operation. It is an error
  to call this method on an operation that is not an index lookup
  operation.
*/
const KEY_PART_INFO *pushed_table::get_key_part_info(uint field_no) const {
  assert(field_no < get_no_of_key_fields());
  assert(m_index_no >= 0);
  const KEY *key = &m_table->key_info[m_index_no];
  return &key->key_part[field_no];
}

/** Get the Item_multi_eq's set relevant for the specified 'Item_field' */
Item_multi_eq *pushed_table::get_item_equal(
    const Item_field *item_field) const {
  assert(item_field->type() == Item::FIELD_ITEM);
  const Table_ref *const table_ref = m_table->pos_in_table_list;
  COND_EQUAL *const cond_equal = table_ref->query_block->join->cond_equal;
  if (cond_equal != nullptr) {
    return item_field->find_multi_equality(cond_equal);
  }
  return nullptr;
}

const Join_scope *pushed_table::get_join_scope() const {
  return m_join_nest->get_join_scope();
}

// All tables in 'this' Join_scope, as well as any 'upper' scopes embedding it.
ndb_table_map pushed_table::get_tables_in_all_query_scopes() const {
  const Join_scope *join_scope = get_join_scope();
  return join_scope->get_all_tables_map();
}

// The upper Join_scopes, limited to those within current 'Query_scope'
ndb_table_map pushed_table::get_tables_in_this_query_scope() const {
  const Join_scope *join_scope = get_join_scope();
  const Query_scope *query_scope = join_scope->get_query_scope();
  ndb_table_map map(join_scope->get_all_tables_map());
  map.subtract(query_scope->m_all_upper_map);
  return map;
}

const char *pushed_table::get_scope_description() const {
  const Join_scope *join_scope = get_join_scope();
  return join_scope->m_descr;
}

// Get map of tables in the inner nest, prior to 'last',
// which this table is a member of
ndb_table_map pushed_table::get_inner_nest(uint last) const {
  ndb_table_map nest(get_full_inner_nest());
  ndb_table_map prefix;
  prefix.set_prefix(last);
  nest.intersect(prefix);
  return nest;
}

// Get map of all tables in the join_nest this table is a member of.
ndb_table_map pushed_table::get_full_inner_nest() const {
  Join_nest *inner_nest = m_join_nest->get_inner_nest();
  return inner_nest->m_inner_map;
}

/**
 * Returns the first/last table in the join-nest this table is a member of.
 * We enumerate the uppermost nest to range from [0..#tables-1].
 *
 * The first_upper reference to this range is '0'.
 * Note, that first_upper of the uppermost nest is still negative.
 */
uint pushed_table::get_first_inner() const {
  return m_join_nest->get_first_inner();
}
uint pushed_table::get_last_inner() const {
  return m_join_nest->get_last_inner();
}
int pushed_table::get_first_upper() const {
  return m_join_nest->get_first_upper();
}

/**
 * Returns the first/last table in a semi-join nest.
 * Returns <0 if table is not part of a semi-join nest.
 */
int pushed_table::get_first_sj_inner() const {
  return m_join_nest->get_first_sj_inner();
}
int pushed_table::get_last_sj_inner() const {
  return m_join_nest->get_last_sj_inner();
}
int pushed_table::get_first_sj_upper() const {
  return m_join_nest->get_first_sj_upper();
}

/**
 * Returns the first table in an anti-join nest.
 * Returns <0 if table is not part of an anti-join nest.
 */
int pushed_table::get_first_anti_inner() const {
  return m_join_nest->get_first_anti_inner();
}

bool pushed_table::has_condition_inbetween(const pushed_table *ancestor) const {
  const table_map filtered_tables =
      m_join_nest->get_filtered_tables(ancestor->m_join_nest);
  return (filtered_tables & m_table->pos_in_table_list->map()) != 0;
}

Item *pushed_table::get_condition() const {
  if (m_filter == nullptr) return nullptr;
  return m_filter->filter().condition;
}
