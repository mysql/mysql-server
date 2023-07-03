/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <assert.h>

#include "sql/sql_bitmap.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ha_ndbcluster.h"

class Item_equal;
class NdbTransaction;
class NdbQueryBuilder;
class NdbQueryOperand;
class NdbQueryOperationDef;
class NdbQueryOptions;
class ndb_pushed_builder_ctx;
struct Index_lookup;
struct NdbError;

/**
 * This type is used in conjunction with the 'pushed_table' objects and
 * represents a set of the ndb_pushed_builder_ctx::m_tables[].
 * Had to subclass Bitmap as the default Bitmap<64> c'tor didn't initialize its
 * map.
 */
typedef Bitmap<(MAX_TABLES > 64 ? MAX_TABLES : 64)> table_bitmap;

class ndb_table_map : public table_bitmap {
 public:
  explicit ndb_table_map() : table_bitmap() {}

  void add(const ndb_table_map &table_map) { merge(table_map); }
  void add(uint table_no) { set_bit(table_no); }

  bool contain(const ndb_table_map &table_map) const {
    return table_map.is_subset(*this);
  }
  bool contain(uint table_no) const { return is_set(table_no); }

  uint first_table(uint start = 0) const;
  uint last_table(uint start = MAX_TABLES) const;

};  // class ndb_table_map

/** This class represents a prepared pushed (N-way) join operation.
 *
 *  It might be instantiated multiple times whenever the query,
 *  or this subpart of the query, is being (re-)executed by
 *  ::createQuery() or it's wrapper method
 *  ha_ndbcluster::create_pushed_join().
 */
class ndb_pushed_join {
 public:
  explicit ndb_pushed_join(const ndb_pushed_builder_ctx &builder_ctx,
                           const NdbQueryDef *query_def);

  ~ndb_pushed_join();

  /**
   * Check that this prepared pushed query matches the type
   * of operation specified by the arguments.
   */
  bool match_definition(int type,  // NdbQueryOperationDef::Type,
                        const NDB_INDEX_DATA *idx, const char *&reason) const;

  /** Create an executable instance of this defined query. */
  NdbQuery *make_query_instance(NdbTransaction *trans,
                                const NdbQueryParamValue *keyFieldParams,
                                uint paramCnt) const;

  /** Get the number of pushed table access operations.*/
  uint get_operation_count() const { return m_operation_count; }

  /**
   * In a pushed join, fields in lookup keys and scan bounds may refer to
   * result fields of table access operation that execute prior to the pushed
   * join. This method returns the number of such references.
   */
  uint get_field_referrences_count() const { return m_field_count; }

  const NdbQueryDef &get_query_def() const { return *m_query_def; }

  /** Get the table that is accessed by the i'th table access operation.*/
  const TABLE *get_table(uint i) const {
    assert(i < m_operation_count);
    return m_tables[i];
  }

  /**
   * This is the maximal number of fields in the key of any pushed table
   * access operation.
   */
  static constexpr uint MAX_KEY_PART = MAX_KEY;
  /**
   * In a pushed join, fields in lookup keys and scan bounds may refer to
   * result fields of table access operation that execute prior to the pushed
   * join. This constant specifies the maximal number of such references for
   * a query.
   */
  static constexpr uint MAX_REFERRED_FIELDS = 16;
  /**
   * For each table access operation in a pushed join, this is the maximal
   * number of key fields that may refer to the fields from ancestor operation.
   */
  static constexpr uint MAX_LINKED_KEYS = MAX_KEY;
  /**
   * Pushed conditions within the pushed join may refer Field values from
   * ancestor operations. This is the maximum number of such linkedValues
   * supported.
   */
  static constexpr uint MAX_LINKED_PARAMS = 16;
  /**
   * This is the maximal number of table access operations there can be in a
   * single pushed join.
   */
  static constexpr uint MAX_PUSHED_OPERATIONS = MAX_TABLES;

 private:
  const NdbQueryDef *const m_query_def;  // Definition of pushed join query

  /** This is the number of table access operations in the pushed join.*/
  uint m_operation_count;

  /** This is the tables that are accessed by the pushed join.*/
  const TABLE *m_tables[MAX_PUSHED_OPERATIONS];

  /**
   * This is the number of referred fields of table access operation that
   * execute prior to the pushed join.
   */
  const uint m_field_count;

  /**
   * These are the referred fields of table access operation that execute
   * prior to the pushed join.
   */
  Field *m_referred_fields[MAX_REFERRED_FIELDS];
};  // class ndb_pushed_join

/** The type of a table access operation. */
enum enum_access_type {
  /** For default initialization.*/
  AT_VOID,
  /** Value has already been fetched / determined by optimizer.*/
  AT_FIXED,
  /** Do a lookup of a single primary key.*/
  AT_PRIMARY_KEY,
  /** Do a lookup of a single unique index key.*/
  AT_UNIQUE_KEY,
  /** Scan an ordered index with a single upper and lower bound pair.*/
  AT_ORDERED_INDEX_SCAN,
  /** Do a multi range read for a set of primary keys.*/
  AT_MULTI_PRIMARY_KEY,
  /** Do a multi range read for a set of unique index keys.*/
  AT_MULTI_UNIQUE_KEY,
  /**
    Do a multi range read for a mix of ranges (for which there is an
    ordered index), and either primary keys or unique index keys.
  */
  AT_MULTI_MIXED,
  /** Scan a table. (No index is assumed to be used.) */
  AT_TABLE_SCAN,
  /** Access method will not be chosen before the execution phase.*/
  AT_UNDECIDED,
  /**
    The access method has properties that prevents it from being pushed to a
    storage engine.
   */
  AT_OTHER
};

class Join_nest;
class Join_scope;  // 'is a' Join_nest as well.

struct pushed_table {
  pushed_table()
      : m_inner_nest(),
        m_upper_nests(),
        m_ancestor_nests(),
        m_first_inner(0),
        m_last_inner(0),
        m_first_upper(-1),
        m_sj_nest(),
        m_first_sj_inner(-1),
        m_last_sj_inner(-1),
        m_first_sj_upper(-1),
        m_first_anti_inner(-1),
        m_key_parents(nullptr),
        m_ancestors(),
        m_parent(MAX_TABLES),
        m_op(nullptr) {}

  /**
   * As part of analyzing the pushability of each table, the 'join-nest'
   * structure is collected for the tables. The 'map' represent the id
   * of each table in the 'nests':
   *
   * - The inner-nest contain the set of all *preceding* tables which
   *   this table has some INNER JOIN relation with. Either by the table(s)
   *   being directly referred by a inner-join condition on this table,
   *   or indirectly by being inner joined with one of the referred table(s).
   *
   *   Note that no result rows from any of the tables in the inner-nest can
   *   be produced if there is not a match on all join conditions between the
   *   tables in an inner-nest. (Except NULL complimented rows for
   *   entire inner-nest if the nest itself is outer joined.
   *   (-> has an upper-nest - see below.))
   *
   * - The upper-nest(s) are the set of tables which the tables in the
   *   inner-nest are outer joined with as defined by the nesting topology
   *   of the mysqld provided query-plan. There might be multiple levels
   *   of upper nesting (or outer joining), where m_upper_nests contain the
   *   aggregate of these nests as seen from this table. 'm_first_upper'
   *   can be used to iterate the m_upper_nests one nest at a time.
   *
   * - The m_ancestor_nests are the join-nests actually refered from an
   *   outer joined (nest-of-)tables. It is set up based on the tables/nests
   *   actally referred from the SPJ-query as built by this join-pushdown
   *   handler. It will usually be the same as the upper-nests, but it can
   *   differ in cases where we entirely skip references to a parent nest
   *   and just refer tables from its grand-parent nests, or refer 'sideways'
   *   into sibling nests.
   *
   * These two similar, but different, nest maps are required as both the
   * inner/outer join properties, as well as the out-of-nest references to
   * ancestor tables impose limitatins on whether a table can be pushed.
   * Note that the upper_nests are 'static' in the scope up pushdown handler:
   * They are set up entirely based on the query-plan provided to it.
   * The ancestor_nests however are build based on the SPJ query build by
   * this code.
   *
   * In the comments for the optimizer code, and this SPJ handler integration
   * code, we may use nested pairs of parentheses to express the nest
   * structures, like:
   * t1, (t2,t3,(t4)), which means:
   *
   * - t2 & t3 has the upper nest [t1], thus t2,t3 are outer joined with t1
   * - t2 & t3 is in same inner nest, thus they are inner joined with each
   * other.
   * - t4 has the upper nest [t2,t3], the aggregated upper nests
   * (m_upper_nests) for t4 will contain [t1,t2,t3]. (and outer joins with
   * these).
   *
   * eg. t3 will have the m_upper_nests=[t1], and m_inner_nest=[t2],
   * ('self' not included in inner_nest).
   *
   * Note that a table will be present in the m_inner_nest and/or m_upper_nest
   * even if the table is not join-pushed. 'Self' is not represented in
   * 'm_inner_nest'.
   */
  ndb_table_map m_inner_nest;

  // Aggregate of the upper-nest, as defined by the query-plan
  ndb_table_map m_upper_nests;

  // The nests actually refered as ancestors, usually same as m_upper_nests.
  ndb_table_map m_ancestor_nests;

  /**
   * upper_nests / embedding_nests:
   *
   * The sum of the inner- and upper-nests is the 'embedding nests'
   * for the table. The join semantic for the tables in the
   * embedding nest is such that no result row can be created from
   * a row from this table without having a set of rows from all preceding
   * tables in the embedding nest. (Fulfilling any join conditions)
   *
   * The 'embedding nests' plays an important role when analyzing a
   * table for join pushability:
   *
   *  - Assume the previous nest structure: t1, (t2,t3,(t4))
   *  - t2, t3 both refer only t1 in their join conditions.
   *  - t4 refers both t2 and t3: 't4.a = t2.x and t4.b = t3.y'.
   *
   * Thus, the query has the dependency topology:
   *
   *                t1
   *               /  \
   *              t2  t3  (Not directly pushable, see below)
   *               \  /
   *                t4
   *
   * The SPJ implementation require the dependency topology for
   * a SPJ query to be a plain tree topology (implementation legacy).
   * Thus the query above is not directly pushable.
   *
   * We have two mechanisms for helping us in making such queries pushable:
   *  1) SPJ allows us to refer values from any ancestor tables.
   *     (grand-(grand-...)parents).
   *  2) A table is implicitly dependending on any table in the embedding
   *     nests, even if no join condition is referring that table.
   *
   * For the query above we may use this to add an extra dependency from
   * t3 on t2. Furthermore t3's join condition on t1 is made a grand-parent
   * reference to t1, via t2:
   *
   *                          t1
   *                         //
   *                        t2|  <- t1 values passed via t2
   *                         \\  <- Added extra dependency on t2
   *    t2 values, via t3 -> |t3
   *                         //
   *                        t4   (Has a join condition on t2 & t3)
   *
   * Thus we have transformed the query into a pushable query.
   * The SPJ handler integration, in combination with the SPJ API,
   * will add such extra parent dependencies where required, and
   * allowed by checking that references are from within the
   * embedding_nests().
   */
  ndb_table_map embedding_nests() const {
    ndb_table_map nests(m_inner_nest);
    nests.add(m_upper_nests);
    return nests;
  }

  /**
   * ancestor_nests:
   *
   * In additions to the above inner- and upper-nest dependencies, there
   * may be dependencies on tables outside of the set of inner_ & upper_nests.
   * Such dependencies are caused by explicit references to non-embedded
   * tables from the join conditions.
   *
   * One such case is the nest structure: t1,(t2),(t3),(t4), where t4 has the
   * same join condition as above: 't4.a = t2.x and t4.b = t3.y'.
   * (Join condition is referring both the outer joined t2 and t3, while the
   * 't4.embedding_nest' contains only t1. Thus neither of t2 and t3 referred
   * in the t4's join condition is in its embedding_nests())
   *
   * Note that this is a perfectly legal join condition, possibly a bit
   * unusual though.
   *
   * If we employed the same rewrite as above (Adding a dependency
   * on (t2) from (t3)) we would effectively also add the extra condition
   * 't2 IS NOT NULL, which would have changed the semantic of the query.
   * Thus, pushing this query could have resulted in an incorrect result
   * set being returned.
   *
   *                 t1
   *                /  \
   *              (t2)(t3)  --> Can't be made join pushable!
   *                \  /
   *                (t4)
   *
   * An exception exists to the above: What if t3 already has a join
   * condition making it dependent on (the outer joined) t2?
   * Like the join condition: 'on t3.a = t1.x and t3.b = t2.y'.
   * That would introduce an explicit dependency between t2 and t3,
   * similar to the one we added in an example further up. For t3 it also
   * implies ''t2 IS NOT NULL'.  -> Query becomes pushable.
   *
   * We handle this by allowing such explicit 'out of nests' references
   * to become members of the ancestor_nests of the nest tables referring
   * them. In the case above the 'outer' references to t2 from t3 will
   * result in t2 being added to the ancestor_nests of t3.
   * When analyzing t4 pushability, we will find that the parent table t3
   * already has t2 as part of its ancestor_nests. Thus t4 becomes pushable.
   *
   * When we leave the nest containing the table(s) which made the
   * 'out of nests' references, such added ancestor_nests references will
   * also go out of scope.
   */
  ndb_table_map ancestor_nests() const {
    ndb_table_map nests(m_inner_nest);
    nests.add(m_ancestor_nests);
    return nests;
  }

  /**
   * Joined tables are collected in 'nests of tables' being INNER,
   * OUTER, ANTI or SEMI joined (the JoinType). All tables in the same
   * nest are joined with the specific JoinType relative to any ancestors
   * outside of its nest. As tables are collected and enumerated left deep,
   * we can easily check the Join properties (-> 'nest membership') by
   * comparing 'm_tab_no' and the 'first_inner' table in the specific nest.
   */
  bool isInnerJoined(const pushed_table &ancestor) const {
    assert(ancestor.m_tab_no <= m_tab_no);  // Is an ancestor
    return m_first_inner <= ancestor.m_tab_no;
  }
  bool isOuterJoined(const pushed_table &ancestor) const {
    assert(ancestor.m_tab_no <= m_tab_no);  // Is an ancestor
    return m_first_inner > ancestor.m_tab_no;
  }
  bool isSemiJoined(const pushed_table &ancestor) const {
    assert(ancestor.m_tab_no <= m_tab_no);  // Is an ancestor
    return m_first_sj_inner > static_cast<int>(ancestor.m_tab_no);
  }
  bool isAntiJoined(const pushed_table &ancestor) const {
    assert(ancestor.m_tab_no <= m_tab_no);  // Is an ancestor
    return m_first_anti_inner > static_cast<int>(ancestor.m_tab_no);
  }

  /**
   * Some additional join_nest structure for navigating the nest hiararcy:
   */
  uint m_first_inner;  // The first table represented in current m_inner_nest
  uint m_last_inner;   // The last table in current m_inner_nest
  int m_first_upper;   // The first table in the upper_nest of this table.

  /**
   * A similar, but simpler, nest topology exists for the semi-joins.
   * Tables in the semi-join nest are semi joined with any tables outside
   * the sj_nest.
   *
   * Note that tables can still be inner- and outer-joined inside the sj_nest.
   * Unlike the inner_ and upper_nest maps representing these joins, the
   * sj_nest for a particular table contains all tables in the sj_nest. (Not
   * only the preceding tables.)
   */
  ndb_table_map m_sj_nest;

  int m_first_sj_inner;  // The first table in m_sj_nest
  int m_last_sj_inner;   // The last table in m_sj_nest

  /**
   * The semi-join nests may be nested inside each other as well.
   * In such cases 'm_first_sj_upper' will refer the start of the sj_nest
   * we are inside.
   *
   * Note that for nested sj_nests, the 'upper' 'm_sj_nest' bitmap will also
   * contain any semi-join'ed tables in sj_nest's inside it - Contrary to
   * the inner_nest bitmaps which only contain the tables in each inner-nest.
   */
  int m_first_sj_upper;

  /**
   * The first table in an ANTI-join nest, iff we are member of such a nest.
   * This table is ANTI-joined with any ancestor tables with
   * 'tab_no < first_anti_inner.
   * Note that an ANTI-join is an OUTER join as well.
   */
  int m_first_anti_inner;

  /**
   * For each KEY_PART referred in the join conditions, we find the set of
   * possible parent tables for each non-const_item KEY_PART.
   * In addition to the parent table directly referred by the join condition,
   * any tables *in same join nest*, available by usage of
   * equality sets, are also added as a possible parent.
   *
   * The set of 'key_parents[]' are collected when analyzing query for
   * join pushability, and saved for later usage by ::optimize_query_plan(),
   * which will select the actual m_parent to be used for each table.
   */
  ndb_table_map *m_key_parents;

  /**
   * The m_ancestor map serves two slightly different purposes:
   *
   * 1) During ::optimize_query_plan() we may enforce parent
   *    dependencies on the ancestor tables by setting the ancestors.
   *    Such enforcement means that no rows from this table will be
   *    requested until result row(s) are available from all ancestors.
   *    Normally any parent tables referred by the join conditions are
   *    added to m_ancestors at this stage.
   *
   * 2) After ::optimize_query_plan() m_ancestors will contain all
   *    ancestor tables reachable through the m_parent chain
   *
   * Note that there are mandatory nest level dependencies as well.
   * The aggregate of all table ancestors in the same nest
   * are mandatory ancestors for the nest. This also include
   * other join_nests embedded with this nest. The method
   * get_nest_ancestors() is provided for collecting such
   * dependencies.
   *
   * There are implementation limitations in the SPJ-API
   * ::prepareResultSet() which calls for such ancestor dependencies
   * to be enforced. It requires an outer-joined treeNode to be either
   * a child or a sibling of the 'first-inner' of the join nest it
   * is embedded within. (or: The first_upper)
   *
   * The MySQL query optimizer may use the equality set to
   * 'move up' a joined table such that it is not depending on any
   * tables in its own join nest (inner joined), or on tables from its
   * upper-nest (outer joined), which breaks the requirement above.
   * One such case may be the generic join structure:
   *
   *    t1 ij t2 oj (t3 oj (t4))
   *
   * Assume that we have the join conditions between these tables:
   *
   * - t1 ij t2:  t2.b = t1.a
   * - t2 oj t3:  t3.c = t2.b
   * - t3 oj t4:  t4.x = t3.c
   *
   * Thus t4 has the equality set: t4.x = [t1.a,t2.b,t3.c].
   * The optimizer may choose to use this to change the t4 join condition
   * to t4.x = t1.a, resulting in the dependency tree:
   *
   *                t1
   *         (ij)  /  \ (oj)
   *              t2  (t4))
   *         (oj) |
   *             (t3
   *
   * Directly transforming this onto a pushed-join queryTree, will break
   * the ::prepareResultSet(), 'child or sibling'-rule. (t4 is not a child
   * or sibling of its 'first_upper' t3).
   * Such cases are resolved by the nest level m_ancestor enforcement:
   *
   *    - We have: t3.m_ancestors = [t1,t2]
   *    - 'first_upper ancestors are enforced on t4:
   *      -> t4.m_ancestors = [t1,t2]
   *
   * Thus, we will enforce the below SPJ-queryTree to be produced:
   *
   *                t1
   *         (ij)  /
   *              t2
   *       (oj)  /  \  (oj)
   *           (t3 (t4))
   *
   * (Note also the nest-dependency-comments above regarding how extra
   * dependencies between tables in the same inner-nest may be added)
   */
  ndb_table_map m_ancestors;

  /**
   * The actual parent as chosen by ::optimize_query_plan()
   */
  uint m_parent;

  // The NdbQueryOperationDef produced when pushing this table
  const NdbQueryOperationDef *m_op;

  /********
   * The interface for accessing the query plan collected from AccessPath.
   * This used to be known as the AbstractQueryPlan (AQP), but is now an
   * integrated part of the join-pushdown-handler.
   */

  /** Get the access type of this operation.*/
  enum_access_type get_access_type() const { return m_access_type; }

  /** Estimate number of rows returned from data nodes. */
  double num_output_rows() const;

  /**
    Get a description of the reason for getting access_type==AT_OTHER. To be
    used for informational messages.
    @return A string that should be assumed to have the same life time as
    this pushed_table object.
  */
  const char *get_other_access_reason() const { return m_other_access_reason; }

  uint get_no_of_key_fields() const;
  const Item *get_key_field(uint field_no) const;

  const KEY_PART_INFO *get_key_part_info(uint field_no) const;

  /**
    Get the number of this table in ndb_pushed_builder_ctx::m_tables[].
  */
  uint get_table_no() const { return m_tab_no; }

  /**
    @return The number of the index to use for this access operation (
    or -1 for non-index operations).
  */
  int get_index_no() const { return m_index_no; }

  const TABLE *get_table() const { return m_table; }

  Item_equal *get_item_equal(const Item_field *field_item) const;

  ndb_table_map get_tables_in_this_query_scope() const;
  ndb_table_map get_tables_in_all_query_scopes() const;

  const char *get_scope_description() const;

  // Need to return rows in index sort order?
  bool use_order() const;

  // Get the condition for 'this' table.
  Item *get_condition() const;

  // Do we have some conditions (aka FILTERs) in the AccessPath
  // between 'this' table and the 'ancestor'
  bool has_condition_inbetween(const pushed_table *ancestor) const;

  // Get map of tables in the inner nest, prior to 'last',
  // which 'this' is a member of
  ndb_table_map get_inner_nest(uint last) const;

  // Get map of all tables in the inner nest which 'this' is a member of
  ndb_table_map get_full_inner_nest() const;

  uint get_first_inner() const;
  uint get_last_inner() const;
  int get_first_upper() const;

  int get_first_sj_inner() const;
  int get_last_sj_inner() const;
  int get_first_sj_upper() const;

  int get_first_anti_inner() const;

  /**
    Getter and setters for an opaque object for each table.
    Used by the handler's to persist 'pushability-flags' to avoid
    overhead by recalculating it for each ::engine_push()
  */
  uint get_table_properties() const { return m_properties; }
  void set_table_properties(uint val) { m_properties = val; }

 private:
  friend ndb_pushed_builder_ctx;

  Join_nest *m_join_nest{nullptr};

  // The table number within ndb_pushed_builder_ctx::m_tables[]
  uint m_tab_no;

  // A 'basic' AccessPath having a table reference
  const AccessPath *m_path{nullptr};

  // The table accessed by m_path
  const TABLE *m_table{nullptr};

  // An optional AccessPath::FILTER in effect for this table
  const AccessPath *m_filter{nullptr};

  /** The access type used for this table. */
  enum_access_type m_access_type{AT_VOID};

  /** The reason for getting m_access_type==AT_OTHER. Used for EXPLAIN. */
  const char *m_other_access_reason{nullptr};

  /** The index to use for this operation (if applicable )*/
  int m_index_no{-1};

  /** May store an opaque property / flag */
  uint m_properties{0};

  const Join_scope *get_join_scope() const;
  const Index_lookup *get_table_ref() const;

  void compute_type_and_index();

};  // struct pushed_table

/**
 * Contains the context and helper methods used during ::make_pushed_join().
 *
 * Collect the query-plan from the AccessPath constructed by the mysql optimizer
 * and provide a more tabel focused representation of the query.
 *
 * Execution plans built for pushed joins are stored inside this builder
 * context.
 */

class ndb_pushed_builder_ctx {
  friend ndb_pushed_join::ndb_pushed_join(
      const ndb_pushed_builder_ctx &builder_ctx, const NdbQueryDef *query_def);

  friend int ndbcluster_push_to_engine(THD *thd, AccessPath *root_path,
                                       JOIN *join);

 public:
  ndb_pushed_builder_ctx(const THD *thd, const AccessPath *root_path,
                         const JOIN *join);
  ~ndb_pushed_builder_ctx();

  void prepare(pushed_table *join_root);

  /**
   * Build the pushed query identified with 'is_pushable_with_root()'.
   * Returns:
   *   = 0: A NdbQueryDef has successfully been prepared for execution.
   *   > 0: Returned value is the error code.
   */
  int make_pushed_join();

  const NdbError &getNdbError() const;

 private:
  // 'pushability' is stored in ::set_table_properties()
  enum join_pushability {
    PUSHABILITY_UNKNOWN = 0x00,  // Initial 'unknown' value, calculate it
    PUSHABILITY_KNOWN = 0x10,
    PUSHABLE_AS_PARENT = 0x01,
    PUSHABLE_AS_CHILD = 0x02
  };

  void construct(const AccessPath *plan);
  void construct(Join_nest *nest_ctx, const AccessPath *plan);

  bool maybe_pushable(pushed_table *table, join_pushability check);

  int make_pushed_join(const ndb_pushed_join *&pushed_join);

  /**
   * Collect all tables which may be pushed together with 'root'.
   * Returns 'true' if anything is pushable.
   */
  bool is_pushable_with_root();

  bool is_pushable_as_child(pushed_table *table);

  bool is_pushable_as_child_scan(const pushed_table *table,
                                 ndb_table_map all_key_parents);

  bool is_pushable_within_nest(const pushed_table *table, ndb_table_map nest,
                               const char *nest_type);

  bool set_ancestor_nests(pushed_table *table, ndb_table_map key_parents);

  bool is_const_item_pushable(const Item *key_item,
                              const KEY_PART_INFO *key_part);

  bool is_field_item_pushable(pushed_table *table, const Item *key_item,
                              const KEY_PART_INFO *key_part,
                              ndb_table_map &parents);

  void validate_join_nest(ndb_table_map nest, uint first, uint last,
                          const char *nest_type);

  void remove_pushable(const pushed_table *table);

  /**
   * We have a plan being pushable, we can still choose to not accept
   * it for execution. Used as a hook for rejecting pushing when we
   * believe there are better options.
   */
  bool accept_query_plan();

  void optimize_query_plan();

  int build_query();

  void collect_key_refs(const pushed_table *table,
                        const Item *key_refs[]) const;

  int build_key(const pushed_table *table, const NdbQueryOperand *op_key[],
                NdbQueryOptions *key_options);

  // Get all parent tables referred by key
  ndb_table_map get_all_key_parents(const pushed_table *table) const;

  uint get_table_no(const Item *key_item) const;

  ndb_table_map get_table_map(table_map external_map) const;

  // get required nest level ancestor
  ndb_table_map required_ancestors(const pushed_table *table) const;

  const THD *const m_thd;
  const JOIN *const m_join;

  pushed_table *m_join_root;

  // Scope of tables covered by this pushed join
  ndb_table_map m_join_scope;

  // Scope of tables evaluated prior to 'm_join_root'
  // These are effectively const or params wrt. the pushed join
  ndb_table_map m_const_scope;

  // Set of tables in join scope requiring (index-)scan access
  ndb_table_map m_scan_operations;

  // Tables in this join-scope having remaining conditions not being pushed
  ndb_table_map m_has_pending_cond;

  // Tables which are subject to some form of skip-read. That is:
  // - All tables in semi-join nests.
  // - All tables having (grand-)parents in semi-join nests.
  //   (Indirectly skipped when parent rows are skipped.
  ndb_table_map m_skip_reads;

  // Number of internal operations used so far (unique lookups count as two).
  uint m_internal_op_count;

  uint m_fld_refs;
  Field *m_referred_fields[ndb_pushed_join::MAX_REFERRED_FIELDS];

  // Handle to the NdbQuery factory.
  // Possibly reused if multiple NdbQuery's are pushed.
  NdbQueryBuilder *m_builder;

  // Number of pushed_tables
  uint m_table_count;

  pushed_table m_tables[MAX_TABLES];

};  // class ndb_pushed_builder_ctx
