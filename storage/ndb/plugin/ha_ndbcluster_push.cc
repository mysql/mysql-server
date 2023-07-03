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
/**
  @file

  @brief
  This file defines various classes and methods used for pushing queries
  to the ndb data node (for execution by the SPJ block).
*/

#include "storage/ndb/plugin/ha_ndbcluster_push.h"

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "storage/ndb/include/ndb_version.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/ndbapi/NdbInterpretedCode.hpp"
#include "storage/ndb/plugin/ha_ndbcluster.h"
#include "storage/ndb/plugin/ha_ndbcluster_cond.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/src/ndbapi/NdbQueryBuilder.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryOperation.hpp"

/*
  Explain why an operation could not be pushed
  @param[in] msgfmt printf style format string.
*/
#define EXPLAIN_NO_PUSH(msgfmt, ...)                                   \
  do {                                                                 \
    if (unlikely(current_thd->lex->is_explain())) {                    \
      push_warning_printf(current_thd, Sql_condition::SL_NOTE, ER_YES, \
                          (msgfmt), __VA_ARGS__);                      \
    }                                                                  \
  } while (0)

static const char *get_referred_field_name(const Item_field *field_item) {
  assert(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->field_name;
}

static const char *get_referred_table_access_name(
    const Item_field *field_item) {
  assert(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->table->alias;
}

static bool ndbcluster_is_lookup_operation(enum_access_type accessType) {
  return accessType == AT_PRIMARY_KEY || accessType == AT_UNIQUE_KEY ||
         accessType == AT_MULTI_PRIMARY_KEY ||
         accessType == AT_MULTI_UNIQUE_KEY;
}

/* Is some sort of Multi-range-read accessType ? */
static bool ndbcluster_is_mrr_operation(enum_access_type accessType) {
  return accessType == AT_MULTI_PRIMARY_KEY ||
         accessType == AT_MULTI_UNIQUE_KEY || accessType == AT_MULTI_MIXED;
}

uint ndb_table_map::first_table(uint start) const {
  for (uint table_no = start; table_no < length(); table_no++) {
    if (contain(table_no)) return table_no;
  }
  return length();
}

uint ndb_table_map::last_table(uint start) const {
  uint table_no = start;
  while (true) {
    if (contain(table_no))
      return table_no;
    else if (table_no == 0)
      return length();
    table_no--;
  }
}

/**
 * Get a ndb_table_map containg all tables [first..last]
 */
static ndb_table_map get_tables_in_range(uint first, uint last) {
  ndb_table_map table_map;
  for (uint i = first; i <= last; i++) {
    table_map.add(i);
  }
  return table_map;
}

ndb_pushed_join::ndb_pushed_join(const ndb_pushed_builder_ctx &builder,
                                 const NdbQueryDef *query_def)
    : m_query_def(query_def),
      m_operation_count(0),
      m_field_count(builder.m_fld_refs) {
  assert(query_def != nullptr);
  assert(builder.m_fld_refs <= MAX_REFERRED_FIELDS);
  ndb_table_map searched;
  for (uint tab_no = 0; searched != builder.m_join_scope; tab_no++) {
    const pushed_table *const join_tab = &builder.m_tables[tab_no];
    if (builder.m_join_scope.contain(tab_no)) {
      assert(m_operation_count < MAX_PUSHED_OPERATIONS);
      m_tables[m_operation_count++] = join_tab->get_table();
      searched.add(tab_no);
    }
  }
  for (uint i = 0; i < builder.m_fld_refs; i++) {
    m_referred_fields[i] = builder.m_referred_fields[i];
  }
}

ndb_pushed_join::~ndb_pushed_join() {
  if (m_query_def) m_query_def->destroy();
}

bool ndb_pushed_join::match_definition(int type,  // NdbQueryOperationDef::Type,
                                       const NDB_INDEX_DATA *idx,
                                       const char *&reason) const {
  const NdbQueryOperationDef *const root_operation =
      m_query_def->getQueryOperation((uint)0);
  const NdbQueryOperationDef::Type def_type = root_operation->getType();

  if (def_type != type) {
    DBUG_PRINT(
        "info",
        ("Cannot execute push join. Root operation prepared as %s "
         "not executable as %s",
         NdbQueryOperationDef::getTypeName(def_type),
         NdbQueryOperationDef::getTypeName((NdbQueryOperationDef::Type)type)));
    reason = "prepared with incompatible access type";
    return false;
  }
  const NdbDictionary::Index *const expected_index = root_operation->getIndex();

  // Check that we still use the same index as when the query was prepared.
  switch (def_type) {
    case NdbQueryOperationDef::PrimaryKeyAccess:
      assert(idx != nullptr);
      assert(idx->unique_index == expected_index);
      break;

    case NdbQueryOperationDef::UniqueIndexAccess:
      assert(idx != nullptr);
      // assert(idx->unique_index == expected_index);
      if (idx->unique_index != expected_index) {
        DBUG_PRINT("info",
                   ("Actual index %s differs from expected index %s."
                    "Therefore, join cannot be pushed.",
                    idx->unique_index->getName(), expected_index->getName()));
        reason = "prepared with another (unique) index";
        return false;
      }
      break;

    case NdbQueryOperationDef::TableScan:
      assert(idx == nullptr && expected_index == nullptr);
      break;

    case NdbQueryOperationDef::OrderedIndexScan:
      assert(idx != nullptr);
      // assert(idx->index == expected_index);
      if (idx->index != expected_index) {
        DBUG_PRINT("info", ("Actual index %s differs from expected index %s. "
                            "Therefore, join cannot be pushed.",
                            idx->index->getName(), expected_index->getName()));
        reason = "prepared with another (ordered) index";
        return false;
      }
      break;

    default:
      assert(false);
      break;
  }

  /**
   * There may be references to Field values from tables outside the scope of
   * our pushed join which are supplied as paramValues().
   * If any of these are NULL values, join can't be pushed.
   *
   * Note that the 'Late NULL filtering' in the Iterator::Read() methods will
   * eliminate such NULL-key Read's anyway, so not pushing these joins
   * should be a non-issue.
   */
  for (uint i = 0; i < get_field_referrences_count(); i++) {
    Field *field = m_referred_fields[i];
    if (field->is_real_null()) {
      DBUG_PRINT("info",
                 ("paramValue is NULL, can not execute as pushed join"));
      reason = "a paramValue was NULL";
      return false;
    }
  }

  return true;
}

#ifdef WORDS_BIGENDIAN
/**
 * Determine if a specific column type is represented in a format which is
 * sensitive to the endian format of the underlying platform.
 */
static bool is_endian_sensible_type(const Field *field) {
  const enum_field_types type = field->real_type();
  switch (type) {
    // Most numerics are endian sensible, note the int24 though.
    // Note: Enum dont have its own type, represented as an int.
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    // Deprecated temporal types were 8/4 byte integers
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;

    // The new temporal data types did it right, not endian sensitive
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    // The Tiny type is a single byte, so endianness does not matter
    case MYSQL_TYPE_TINY:
    // Year is also a 'tiny', single byte
    case MYSQL_TYPE_YEAR:
    // Oddly enough, The int24 is *not* stored in an endian sensible format
    case MYSQL_TYPE_INT24:
    // The (deprecated) Time type was handled as an int24.
    case MYSQL_TYPE_TIME:
    // Decimal is basically a char string variant.
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    // Other datatypes (char, blob, json, ..) is not an endian concern
    default:
      return false;
  }
}
#endif

NdbQuery *ndb_pushed_join::make_query_instance(
    NdbTransaction *trans, const NdbQueryParamValue *keyFieldParams,
    uint paramCnt) const {
  DBUG_TRACE;
  DBUG_PRINT("info", ("executing chain of %d pushed joins."
                      " First table is %s, accessed as %s.",
                      get_operation_count(), get_table(0)->alias,
                      NdbQueryOperationDef::getTypeName(
                          m_query_def->getQueryOperation((uint)0)->getType())));

  const NdbQueryParamValue *paramValues = keyFieldParams;

  /**
   * There may be references to Field values from tables outside the scope of
   * our pushed join: These are expected to be supplied as paramValues()
   * after the keyFieldParams[].
   */
  uint outer_fields = get_field_referrences_count();
  NdbQueryParamValue *extendedParams = nullptr;
  if (unlikely(outer_fields > 0)) {
    uint size = sizeof(NdbQueryParamValue) * (paramCnt + outer_fields);
    extendedParams = reinterpret_cast<NdbQueryParamValue *>(my_alloca(size));
    // Copy specified keyFieldParams[] first
    for (uint i = 0; i < paramCnt; i++) {
      new (extendedParams + i) NdbQueryParamValue(keyFieldParams[i]);
    }

    // There may be references to Field values from tables outside the scope of
    // our pushed join: These are expected to be supplied as paramValues()
    for (uint i = 0; i < outer_fields; i++) {
      Field *field = m_referred_fields[i];
      assert(!field->is_real_null());  // Checked by ::check_if_pushable()
      uchar *raw = field->field_ptr();

#ifdef WORDS_BIGENDIAN
      if (field->table->s->db_low_byte_first &&
          is_endian_sensible_type(field)) {
        const uint32 field_length = field->pack_length();
        raw = static_cast<uchar *>(my_alloca(field_length));

        // Byte order is swapped to get the correct endian format.
        const uchar *last = field->field_ptr() + field_length;
        for (uint pos = 0; pos < field_length; pos++) raw[pos] = *(--last);
      }
#else
      // Little endian platforms are expected to be only 'low_byte_first'
      assert(field->table->s->db_low_byte_first);
#endif

      new (extendedParams + paramCnt + i) NdbQueryParamValue(raw, false);
    }
    paramValues = extendedParams;
  }

  NdbQuery *query = trans->createQuery(&get_query_def(), paramValues);
  if (unlikely(extendedParams != nullptr)) {
    for (uint i = 0; i < paramCnt + outer_fields; i++) {
      extendedParams[i].~NdbQueryParamValue();
    }
  }
  return query;
}

/////////////////////////////////////////
ndb_pushed_builder_ctx::ndb_pushed_builder_ctx(const THD *thd,
                                               const AccessPath *root_path,
                                               const JOIN *join)
    : m_thd(thd),
      m_join(join),
      m_join_root(nullptr),
      m_join_scope(),
      m_const_scope(),
      m_scan_operations(),
      m_has_pending_cond(),
      m_skip_reads(),
      m_internal_op_count(0),
      m_fld_refs(0),
      m_builder(nullptr),
      m_table_count(0) {
  /**
   * Set up the ndb_pushed_builder_ctx, including its m_tables[],
   * from the AccessPath describing the 'query plan'.
   */
  construct(root_path);

  /**
   * Set up the nest structure in m_tables[]. Init even those
   * before the root (in case root_no > 0)
   */
  assert(m_table_count > 0);
  assert(m_table_count <= MAX_TABLES);

  /**
   * In addition to the structures set up below, the ndb_pushed_builder_ctx
   * need a later 'prepare' with the root we will try to build a pushed join
   * with. Note that the same ndb_pushed_builder_ctx might be reused when
   * trying to push with different roots.
   */
  for (uint tab_no = 0; tab_no < m_table_count; tab_no++) {
    pushed_table *const table = &m_tables[tab_no];

    /**
     * Build join-nest structure for tables:
     *
     * Collect the inner/outer join-nest structure from the m_tables[].
     * All tables between first/last_inner, and having the same 'first_inner',
     * are members of the same join-nest, thus they are inner joined with each
     * other. Furthermore, they are outer-joined with any tables in the nest
     * starting at 'first_upper'. 'm_inner_nest' and 'm_upper_nests' are the
     * respective bitmap of tables in these nests.
     */
    table->m_first_inner = table->get_first_inner();
    table->m_last_inner = table->get_last_inner();
    table->m_first_upper = table->get_first_upper();

    // By convention m_inner_nest contain only tables prior to this 'tab_no'
    table->m_inner_nest = table->get_inner_nest(tab_no);

    // Set up the nested set of m_upper_nests.
    if (tab_no != table->m_first_inner) {
      table->m_upper_nests = m_tables[table->m_first_inner].m_upper_nests;
    } else if (tab_no > 0) {
      // upper_nest = upper->upper_nest + upper->inner_nest
      assert(table->m_first_upper >= 0);
      pushed_table *const first_upper_table = &m_tables[table->m_first_upper];
      table->m_upper_nests = first_upper_table->m_upper_nests;
      table->m_upper_nests.add(first_upper_table->get_inner_nest(tab_no));
    }

    /**
     * Collect similar (simpler) info for semi_join nests.
     *
     * Note, that contrary to (outer-)join_nest, the sj_nest-bitmap will also
     * include any sub-sj_nest embedded within it. Reason: For outer join, the
     * existence of matches found in embedded sub-nest will *not* affect the
     * upper sj_nest itself - it will only by NULL-extended if no matches are
     * found. In a sj_nest however, matches in sub-sj_nests as well are
     * needed in order for rows in the upper sj_nest to exists as well.
     */
    table->m_first_sj_inner = table->get_first_sj_inner();
    table->m_last_sj_inner = table->get_last_sj_inner();
    table->m_first_sj_upper = table->get_first_sj_upper();

    if (table->m_first_sj_inner >= 0) {  // Is in a sj_nest
      const ndb_table_map sj_nest =
          get_tables_in_range(table->m_first_sj_inner, table->m_last_sj_inner);
      table->m_sj_nest = sj_nest;
    }

    // There may be anti_join_nest as well:
    table->m_first_anti_inner = table->get_first_anti_inner();

    if (!ndbcluster_is_lookup_operation(table->get_access_type())) {
      // A pushable table scan, collect in bitmap for later fast checks
      m_scan_operations.add(tab_no);
    }
  }  // for tab_no [0..m_table_count-1]
}

/**
 * Prepare the ndb_pushed_builder_ctx for a specific root.
 * Reset it for reuse in case it was alredy used for another root.
 */
void ndb_pushed_builder_ctx::prepare(pushed_table *join_root) {
  m_join_root = join_root;
  if (maybe_pushable(join_root, PUSHABLE_AS_PARENT)) {
    m_join_scope.clear_all();
    m_const_scope.clear_all();
    m_has_pending_cond.clear_all();
    m_skip_reads.clear_all();
    m_internal_op_count = 0;
    m_fld_refs = 0;

    for (uint tab_no = 0; tab_no < m_table_count; tab_no++) {
      pushed_table *const table = &m_tables[tab_no];
      table->m_key_parents = nullptr;
      table->m_ancestors.clear_all();
      table->m_ancestor_nests.clear_all();
      table->m_parent = MAX_TABLES;
      table->m_op = nullptr;
      m_skip_reads.add(table->m_sj_nest);
    }
  }
}

ndb_pushed_builder_ctx::~ndb_pushed_builder_ctx() {
  if (m_builder != nullptr) {
    m_builder->destroy();
  }
}

const NdbError &ndb_pushed_builder_ctx::getNdbError() const {
  assert(m_builder != nullptr);
  return m_builder->getNdbError();
}

bool ndb_pushed_builder_ctx::maybe_pushable(pushed_table *table,
                                            join_pushability check) {
  DBUG_TRACE;
  const TABLE *tab = table->get_table();

  if (tab == nullptr) {
    // There could be unused tables allocated in the 'plan', skip these
    return false;
  }

  if (tab->s->db_type()->db_type != DB_TYPE_NDBCLUSTER) {
    // Ignore non-NDBCLUSTER tables.
    DBUG_PRINT("info",
               ("Table '%s' not in ndb engine, not pushable", tab->alias));
    return false;
  }

  if (tab->file->member_of_pushed_join()) {
    return false;  // Already pushed
  }

  uint pushable = table->get_table_properties();
  if (pushable & PUSHABILITY_KNOWN) {
    return ((pushable & check) == check);
  }

  bool allowed = false;
  const char *reason = nullptr;
  pushable = 0;  // Assume not pushable

  switch (table->get_access_type()) {
    case AT_VOID:
      assert(false);
      reason = "UNKNOWN";
      break;

    case AT_FIXED:
      reason = "optimized away, or const'ified by optimizer";
      break;

    case AT_UNDECIDED:
      reason = "Access type was not chosen at 'prepare' time";
      break;

    case AT_OTHER:
      reason = table->get_other_access_reason();
      break;

    default:
      const ha_ndbcluster *handler = down_cast<ha_ndbcluster *>(tab->file);

      if (handler->maybe_pushable_join(reason)) {
        allowed = true;
        pushable = PUSHABLE_AS_CHILD | PUSHABLE_AS_PARENT;
      }
      break;
  }  // switch

  if (reason != nullptr) {
    assert(!allowed);
    EXPLAIN_NO_PUSH("Table '%s' is not pushable: %s", tab->alias, reason);
  }
  table->set_table_properties(pushable | PUSHABILITY_KNOWN);
  return allowed;
}  // ndb_pushed_builder_ctx::maybe_pushable

/**
 * Get *internal* table_no of table referred by 'key_item'
 */
uint ndb_pushed_builder_ctx::get_table_no(const Item *key_item) const {
  assert(key_item->type() == Item::FIELD_ITEM);
  const table_map bitmap = key_item->used_tables();

  for (uint i = 0; i < m_table_count; i++) {
    const TABLE *table = m_tables[i].get_table();
    if (table != nullptr && table->pos_in_table_list != nullptr) {
      const table_map map = table->pos_in_table_list->map();
      if (bitmap & map) {
        assert((bitmap & ~map) == 0);  // No other tables in 'bitmap'
        return i;
      }
    }
  }
  return MAX_TABLES;
}

/**
 * Translate a table_map from external to internal table enumeration
 */
ndb_table_map ndb_pushed_builder_ctx::get_table_map(
    const table_map external_map) const {
  ndb_table_map internal_map;
  table_map bitmap = (external_map & ~PSEUDO_TABLE_BITS);

  for (uint i = 0; bitmap != 0 && i < m_table_count; i++) {
    const TABLE *table = m_tables[i].get_table();
    if (table != nullptr && table->pos_in_table_list != nullptr) {
      const table_map map = table->pos_in_table_list->map();
      if (bitmap & map) {
        internal_map.add(i);
        bitmap &= ~map;  // clear handled table
      }
    }
  }
  assert(bitmap == 0);
  return internal_map;
}

/**
 *  Find the ancestor tables required on nest level
 */
ndb_table_map ndb_pushed_builder_ctx::required_ancestors(
    const pushed_table *table) const {
  ndb_table_map ancestors;

  for (uint i = table->m_first_inner; i <= table->m_last_inner; i++) {
    if (m_join_scope.contain(i)) {
      ancestors.add(m_tables[i].m_ancestors);
    }
  }
  ancestors.intersect(m_tables[table->m_first_inner].m_ancestor_nests);
  return ancestors;
}

/**
 * Get all parent tables possibly referable from the key_parent[].
 * Note that when 'equality sets' are applied by the optimizer,
 * each key reference may have multiple alternative parents.
 */
ndb_table_map ndb_pushed_builder_ctx::get_all_key_parents(
    const pushed_table *table) const {
  ndb_table_map all_key_parents;
  for (uint key_part_no = 0; key_part_no < table->get_no_of_key_fields();
       key_part_no++) {
    all_key_parents.add(table->m_key_parents[key_part_no]);
  }
  return all_key_parents;
}

/**
 * Main entry point to build a pushed join having 'join_root'
 * as it first operation.
 *
 * If the root operation is pushable, we append as many 'child'
 * operations as possible to the pushed join.
 *
 * This currently is implemented as a 3 pass algorithm:
 *
 *  1) Analyze each child and add it to 'm_join_scope' as
 *    'pushable' if it qualifies as such. Part of this phase
 *     is also calculations of possible parents for each table.
 *
 *  2) Determine the parent to be used among the set of possible
 *     parents. This is decided based on simple heuristic where
 *     the goal is to employ filters as soon as possible, and utilize
 *     the parallelism of the SPJ block whenever considered optimal.
 *
 *  3) Build the pushed query.
 */
int ndb_pushed_builder_ctx::make_pushed_join(
    const ndb_pushed_join *&pushed_join) {
  DBUG_TRACE;
  pushed_join = nullptr;

  if (is_pushable_with_root() && accept_query_plan()) {
    int error;
    optimize_query_plan();

    error = build_query();
    if (unlikely(error)) return error;

    const NdbQueryDef *const query_def =
        m_builder->prepare(get_thd_ndb(m_thd)->ndb);
    if (unlikely(query_def == nullptr))
      return -1;  // Get error with ::getNdbError()

    pushed_join = new ndb_pushed_join(*this, query_def);
    if (unlikely(pushed_join == nullptr)) return HA_ERR_OUT_OF_MEM;

    DBUG_PRINT("info", ("Created pushed join with %d child operations",
                        pushed_join->get_operation_count() - 1));
  }
  return 0;
}  // ndb_pushed_builder_ctx::make_pushed_join()

int ndb_pushed_builder_ctx::make_pushed_join() {
  DBUG_TRACE;
  assert(m_table_count <= MAX_TABLES);

  for (uint i = 0; i < m_table_count - 1; i++) {
    pushed_table *const join_root = &m_tables[i];

    if (join_root->get_table() == nullptr ||
        join_root->get_table()->file->member_of_pushed_join()) {
      // Not a real table, or already member of a pushed join -> skip
      continue;
    }

    // Try to build a pushed_join starting from this 'join_root'
    prepare(join_root);

    const ndb_pushed_join *pushed_join = nullptr;
    int error = make_pushed_join(pushed_join);
    if (unlikely(error)) {
      if (error < 0) {
        error = ndb_to_mysql_error(&getNdbError());
      }
      join_root->get_table()->file->print_error(error, MYF(0));
      return error;
    }

    // Assign any produced pushed_join definitions to
    // the ha_ndbcluster instance representing its root.
    if (pushed_join != nullptr) {
      for (uint i = 0; i < pushed_join->get_operation_count(); i++) {
        const TABLE *const tab = pushed_join->get_table(i);
        ha_ndbcluster *child = down_cast<ha_ndbcluster *>(tab->file);
        child->m_pushed_join_member = pushed_join;
        child->m_pushed_join_operation = i;
      }
      DBUG_PRINT("info", ("Assigned pushed join with %d child operations",
                          pushed_join->get_operation_count() - 1));

      get_thd_ndb(m_thd)->m_pushed_queries_defined++;
    }
  }
  return 0;
}  // make_pushed_join()

/**
 * Find the number SPJ operations needed to execute a given access type.
 * (Unique index lookups are translated to two single table lookups internally.)
 */
uint internal_operation_count(enum_access_type accessType) {
  switch (accessType) {
    case AT_PRIMARY_KEY:
    case AT_ORDERED_INDEX_SCAN:
    case AT_MULTI_PRIMARY_KEY:
    case AT_MULTI_MIXED:
    case AT_TABLE_SCAN:
      return 1;

      // Unique key lookups is mapped to two primary key lookups internally.
    case AT_UNIQUE_KEY:
    case AT_MULTI_UNIQUE_KEY:
      return 2;

    default:
      // Other access types are not pushable, so seeing them here is an error.
      assert(false);
      return 2;
  }
}

/**
 * We have found a possible pushed query-plan, starting a this 'root'.
 * We still have the option to not accept it for pushed execution e.g. if
 * we believe it might be better options. Currently we only use simple
 * heuristics to do a late reject of queries here by not accepting them.
 * That does not exclude e.g. cost based analysis to be used in the future.
 */
bool ndb_pushed_builder_ctx::accept_query_plan() {
  DBUG_TRACE;
  const uint root_no = m_join_root->get_table_no();
  ha_ndbcluster *handler =
      down_cast<ha_ndbcluster *>(m_join_root->get_table()->file);
  const NdbDictionary::Table *rootTable = handler->m_table;
  const uint rootFragments = rootTable->getFragmentCount();
  const uint small_table_limit = rootFragments * 3 / 4;

  /**
   * 1) Reject due to lack of parallelism:
   *   If the root-table is expected to return few rows, some of the
   *   SPJ-workers might not get any rows at all. Thus they may become idle.
   *   If the next table is expected to return considerable more rows,
   *   it might be better to start with that one.
   *
   *  a) This table has too few 'output_rows'.
   *  b) Next table is part of this pushed query and is a scan as well.
   *     (Which makes it likely that it could be made a 'root')
   *  c) Next table has considderable more(4x) 'output_rows' than this table.
   *  d) There are at least 2 more tables being pushed excluding 'this' root.
   *  e) Except for the next table, all of the currently pushed tables
   *     has ancestor candidates not requiring 'this' root.
   *     ( -> The existing children are, or can be made, children under
   *          next table as well)
   */
  if (m_scan_operations.contain(root_no) &&
      m_join_root->num_output_rows() < small_table_limit) {  // 1a

    if (m_join_scope.contain(root_no + 1) &&
        m_scan_operations.contain(root_no + 1)) {  // 1b
      pushed_table *const next_root = &m_tables[root_no + 1];

      if (next_root->num_output_rows() >=
          m_join_root->num_output_rows() * 4) {  // 1c
        ndb_table_map reduced_scope(m_join_scope);
        reduced_scope.clear_bit(root_no);

        if (reduced_scope.bits_set() >= 2) {  // 1d
          // 1e: Can all remaining children still be children under next_root?
          for (uint tab_no = root_no + 2; tab_no < m_table_count; tab_no++) {
            if (m_join_scope.contain(tab_no)) {
              ndb_table_map possible_ancestors(
                  get_all_key_parents(&m_tables[tab_no]));
              if (!possible_ancestors.is_overlapping(reduced_scope)) {
                return true;  // Failed '1e' -> keep original
              }
            }
          }
          /**
           * Pushing a 'reduced_scope' query seems to be possible, and is
           * believed to be more optimal. Thus we reject pushing with this
           * 'root' (even if we could). Upper layers will retry 'next_root'.
           */
          EXPLAIN_NO_PUSH(
              "Didn't push table '%s' as root, too few rows to enable "
              "full parallelism",
              m_join_root->get_table()->alias);
          return false;
        }
      }
    }
  }
  return true;
}

/**
 * If there is a pushable query starting with 'root'; add as many
 * child operations as possible to this 'ndb_pushed_builder_ctx' starting
 * with that join_root.
 */
bool ndb_pushed_builder_ctx::is_pushable_with_root() {
  DBUG_TRACE;

  if (!maybe_pushable(m_join_root, PUSHABLE_AS_PARENT)) {
    return false;
  }

  /**
   * Past this point we know at least root to be pushable as parent
   * operation. Search remaining tables appendable if '::is_pushable_as_child()'
   */
  DBUG_PRINT("info",
             ("Table %d is pushable as root", m_join_root->get_table_no()));

  /**
   * Analyze tables below 'm_join_root' as potential members of a pushed
   * join query starting with root.
   */
  const enum_access_type access_type = m_join_root->get_access_type();
  assert(access_type != AT_VOID);
  const uint root_no = m_join_root->get_table_no();

  m_fld_refs = 0;
  m_join_scope.add(root_no);
  m_internal_op_count = internal_operation_count(access_type);

  /**
   * Tables before 'root', which are in its 'scope', are 'const'
   */
  const ndb_table_map root_scope =
      m_join_root->get_tables_in_all_query_scopes();
  m_const_scope.set_prefix(root_no);
  m_const_scope.intersect(root_scope);

  {
    for (uint tab_no = root_no; tab_no < m_table_count; tab_no++) {
      pushed_table *const table = &m_tables[tab_no];

      /**
       * Push down join of table if supported:
       *
       * Use is_pushable_as_child() to analyze whether this table is
       * pushable as part of query starting with 'root'. Note that
       * outer- and semi-joined table scans can not be completely analyzed
       * by is_pushable_as_child(): Pushability also depends on that all
       * later tables in the same nest are pushed, and that there are no
       * unpushed conditions for any (later) tables in this nest.
       * These extra conditions are later checked by validate_join_nest(),
       * when the nest is completed. This may cause some tables which passed
       * the first pushability check, to later fail and be removed. This
       * also has a cascading effect on any tables depending on those
       * being removed. (See validate_join_nest() and remove_pushable())
       */
      if (table != m_join_root) {  // root, already known pushable
        // A child candidate to push under 'root'
        if (is_pushable_as_child(table)) {
        }
      }

      /**
       * Leave join-nests when at 'last_inner'
       *
       * This table can be the last inner table of join-nest(s).
       * That will require additional pushability checks of entire nest
       *
       * Note that the same tab_no may unwind several inner/semi join-nests.
       * ... all having the same 'last_inner' (this tab_no)
       */
      // First unwind the semi-join nests, if needed
      int first_sj_inner = table->m_first_sj_inner;
      int last_sj_inner = table->m_last_sj_inner;
      while ((int)tab_no == last_sj_inner &&   // Leaving the semi_join nest
             (int)root_no < first_sj_inner) {  // Is a SJ relative to root

        // Phase 2 of pushability check, see big comment above.
        assert(m_tables[first_sj_inner].isSemiJoined(*m_join_root));
        validate_join_nest(m_tables[first_sj_inner].m_sj_nest, first_sj_inner,
                           tab_no, "semi");

        // Possibly more nested sj-nests to unwind, or break out
        first_sj_inner = m_tables[first_sj_inner].m_first_sj_upper;
        if (first_sj_inner < 0) break;
        last_sj_inner = m_tables[first_sj_inner].m_last_sj_inner;
      }

      // Prepare inner/outer join-nest structure for unwind;
      uint first_inner = table->m_first_inner;
      uint last_inner = table->m_last_inner;

      while (tab_no == last_inner &&   // End of current join-nest, and
             first_inner > root_no) {  // has an embedding upper nest

        // Phase 2 of pushability check, see big comment above.
        assert(m_tables[first_inner].isOuterJoined(*m_join_root));
        ndb_table_map inner_nest(m_tables[first_inner].get_full_inner_nest());
        validate_join_nest(inner_nest, first_inner, tab_no, "outer");

        first_inner = m_tables[first_inner].m_first_upper;
        last_inner = m_tables[first_inner].m_last_inner;
      }  // while 'leaving a nest'
    }    // for tab_no [root_no..m_table_count-1]
  }
  assert(m_join_scope.contain(root_no));
  return (m_join_scope.last_table() > root_no);  // Anything pushed?
}  // ndb_pushed_builder_ctx::is_pushable_with_root()

/***************************************************************
 *  is_pushable_as_child()
 *
 * Determines if the specified child ('table') can be appended to
 * an existing chain of previously pushed join operations.
 *
 * To be considered pushable the child operation should:
 *
 *  1) Have an REF to the previous parent operations.
 *  2) Refer only a single parent, or a grandparent reachable through
 *     a single parent common to all key fields in the 'REF'
 *
 * In order to increase pushability we use the COND_EQUAL sets
 * to resolve cases (2) above) where multiple parents are referred.
 * If needed to make a child pushable, we replace parent
 * references with another from the COND_EQUAL sets which make
 * it pushable .
 ****************************************************************/
bool ndb_pushed_builder_ctx::is_pushable_as_child(pushed_table *table) {
  DBUG_TRACE;
  const uint root_no = m_join_root->get_table_no();
  const uint tab_no = table->get_table_no();
  assert(tab_no > root_no);

  if (!maybe_pushable(table, PUSHABLE_AS_CHILD)) {
    return false;
  }

  const enum_access_type access_type = table->get_access_type();

  if (ndbcluster_is_mrr_operation(access_type)) {
    const char *type = table->get_other_access_reason();
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, "
        "access type '%s' not implemented",
        table->get_table()->alias, type);
    table->set_table_properties(table->get_table_properties() &
                                ~PUSHABLE_AS_CHILD);
    return false;
  }

  if (!(ndbcluster_is_lookup_operation(access_type) ||
        access_type == AT_ORDERED_INDEX_SCAN)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, 'type' must be a 'ref' access",
        table->get_table()->alias);
    table->set_table_properties(table->get_table_properties() &
                                ~PUSHABLE_AS_CHILD);
    return false;
  }

  // There is a limitation in not allowing LOOKUP - (index)SCAN operations
  if (access_type == AT_ORDERED_INDEX_SCAN &&
      !m_scan_operations.contain(root_no)) {
    EXPLAIN_NO_PUSH(
        "Push of table '%s' as scan-child "
        "with lookup-root '%s' not implemented",
        table->get_table()->alias, m_join_root->get_table()->alias);
    // 'table' may still be PUSHABLE_AS_CHILD with another parent
    return false;
  }

  const uint no_of_key_fields = table->get_no_of_key_fields();
  if (unlikely(no_of_key_fields > ndb_pushed_join::MAX_LINKED_KEYS)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, "
        "too many ref'ed parent fields",
        table->get_table()->alias);
    table->set_table_properties(
        table->get_table_properties() &
        ~PUSHABLE_AS_CHILD);  // Permanently disable as child
    return false;
  }

  if (table->use_order() && table->m_first_sj_inner != (int)tab_no) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, can't provide rows in index order",
        table->get_table()->alias);
    table->set_table_properties(
        table->get_table_properties() &
        ~PUSHABLE_AS_CHILD);  // Permanently disable as child
    return false;
  }

  const ndb_table_map query_scope = table->get_tables_in_this_query_scope();
  if (!query_scope.contain(root_no)) {
    const char *scope_type = m_join_root->get_scope_description();
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "it is in a %s-branch which can't be referred.",
        table->get_table()->alias, m_join_root->get_table()->alias, scope_type);
    return false;
  }

  // Check that we do not exceed the max number of pushable operations.
  const uint internal_ops_needed = internal_operation_count(access_type);
  if (unlikely(m_internal_op_count + internal_ops_needed >
               NDB_SPJ_MAX_TREE_NODES)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s'. Max number"
        " of pushable tables exceeded.",
        table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
  }
  m_internal_op_count += internal_ops_needed;

  DBUG_PRINT("info",
             ("Table:%d, Checking %d REF keys", tab_no, no_of_key_fields));

  /**
   * Calculate the set of all-possible-parents (all_parents)for each
   * non-const_item KEY_PART from the table. In addition to the parent table
   * directly referred by the KEY_PART, any tables in *same join nest*,
   * available by usage of equality sets are also included as 'parents'
   *
   * The subset 'depend_parents' is also calculated as the set of upper most
   * key_parents needed to be available as ancestors of this table.
   *
   * The set of 'key_parents[]' are saved for later usage by ::optimize_*(),
   * which will select the actual parent to be used for each table.
   *
   * We also aggregate the set of 'all_parents' referred by the keys.
   * This is used for checking whether table is pushable.
   */
  ndb_table_map all_parents;
  ndb_table_map depend_parents;
  ndb_table_map *key_parents =
      new (m_thd->mem_root) ndb_table_map[no_of_key_fields];
  table->m_key_parents = key_parents;

  for (uint key_part_no = 0; key_part_no < no_of_key_fields; key_part_no++) {
    const Item *const key_item = table->get_key_field(key_part_no);
    const KEY_PART_INFO *key_part = table->get_key_part_info(key_part_no);

    if (key_item->const_for_execution()) {
      // REF is a literal or field from const-table
      DBUG_PRINT("info", (" Item type:%d is 'const_item'", key_item->type()));
      if (!is_const_item_pushable(key_item, key_part)) {
        return false;
      }
    } else if (key_item->type() == Item::FIELD_ITEM) {
      /**
       * Calculate all parents FIELD_ITEM may refer - Including those
       * available through usage of equality sets. All field_parents
       * will be from within the same join_nest.
       * Only parents within m_join_scope are considered.
       */
      ndb_table_map field_parents;
      if (!is_field_item_pushable(table, key_item, key_part, field_parents)) {
        return false;
      }
      // Save the found key_parents[], aggregate total set of parents referable.
      key_parents[key_part_no] = field_parents;
      all_parents.add(field_parents);

      if (!field_parents.is_clear_all()) {  // Key refers a parent field
#ifndef NDEBUG
        // Verify requirement that all field_parents are from within same nest
        {
          const uint last = field_parents.last_table(tab_no);
          ndb_table_map nest(m_tables[last].m_inner_nest);
          nest.add(last);
          assert(nest.contain(field_parents));
        }
#endif
        const uint first = field_parents.first_table();
        depend_parents.add(first);
      }
    } else {
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as child, "
          "column '%s' does neither 'ref' a column nor a constant",
          table->get_table()->alias, key_part->field->field_name);
      table->set_table_properties(
          table->get_table_properties() &
          ~PUSHABLE_AS_CHILD);  // Permanently disable as child

      return false;
    }
  }  // for (uint key_part_no= 0 ...

  // If no parent candidates within current m_join_scope, table is unpushable.
  if (all_parents.is_clear_all()) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "no parent-child dependency exists between these tables",
        table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
  }

  /**
   * There are limitations on index scans which are depending on other scans
   * which are skip-read as part of a semi-join. Due to the batch fetch
   * mechanisms in SPJ, we might see repeated duplicates of previous fetched
   * rows, when they are combined with new rows from later index-scans
   * depending on these. As the semi-join iterators will skip duplicates,
   * we might incorrrectly skip result rows iff:
   *
   *  1) This table is not part of a semi-join nest itself.
   *     (Thus, not intended to be a subject to skip-read)
   *  2) Table depends on other tables being skip-read.
   *     (Is in a query tree branch with ancestors being skip-read)
   */
  // Pushed tables being subject to skip-read:
  ndb_table_map pushed_skip_reads(m_skip_reads);
  pushed_skip_reads.intersect(m_join_scope);

  if (!table->m_sj_nest.contain(tab_no) &&                 // 1)
      depend_parents.is_overlapping(pushed_skip_reads)) {  // 2)

    // This table becomes an indirect subject to skip-read as well
    m_skip_reads.add(tab_no);

    // If both this table is a scan, and there are scans being skip read.
    // -> We might skip result rows from this non-semi-joined table
    if (m_scan_operations.contain(tab_no) &&
        m_scan_operations.is_overlapping(pushed_skip_reads)) {
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as scan-child of '%s', "
          "depends on tables being 'skip-read'",
          table->get_table()->alias, m_join_root->get_table()->alias);
      return false;
    }
  }

  /**
   * Try to push condition to 'table'. Whatever we could not push of the
   * condition is a 'server side condition' which the server has to
   * evaluate later. The existence of such conditions may effect the join
   * pushability of tables, so we need to try to push conditions first.
   */
  const Item *pending_cond = table->get_condition();
  if (pending_cond != nullptr &&
      m_thd->optimizer_switch_flag(
          OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)) {
    /**
     * Calculate full set of possible ancestors for this table in
     * the query tree. Note that they do not become mandatory ancestors
     * before being added to the m_ancestors bitmap (Further below)
     *
     * 1) All possible key parents, previously calculated as 'all_parents',
     *    as well as all nest-level required_ancestors() are initial parent
     *    candidates. (Note that no table-level m_ancestors are set yet)
     *
     * 2) For all tables being a 'possible_ancestor' of this table:
     *    a) Add the key_parents[] referred from such tables as well.
     *    b) Add any enforced 'm_ancestors'.
     *
     * 3) Add tables requiring existing ancestors as its own ancestors:
     *    For all preceeding table *not* yet being a 'possible_ancestor', if:
     *    a) Table is a member of the ancestor nests (I.e. the set of nests
     *       already being referred) ): Do not add new nest dependencies
     *    AND
     *    b) Table access is a 'single-row-lookup' ): Else we get a
     *       multiplicative access-fanout on all tables getting it as an
     *       ancestor.
     *
     *    Then we can add this table as a possible ancestor, if EITHER:
     *    c) All referred key_parents[] are possible_ancestors.
     *    d) All required_ancestors are possible_ancestors.
     *    -> I.e. Table will always be joined with these ancestors, still
     *       providing the existing possible_ancestor as grand-ancestors.
     */

    // 1) Start with all parent candidates for this 'table'
    ndb_table_map possible_ancestors(all_parents);
    possible_ancestors.add(required_ancestors(table));

    // 2) For all possible_ancestor tables, add its ancestors as well:
    for (uint i = tab_no - 1; i > root_no; i--) {
      if (possible_ancestors.contain(i)) {
        const ndb_table_map all_key_parents(get_all_key_parents(&m_tables[i]));
        possible_ancestors.add(all_key_parents);          // 2a)
        possible_ancestors.add(m_tables[i].m_ancestors);  // 2b)
      }
    }

    // 3) Add tables requiring existing ancestors as its own ancestors:
    const ndb_table_map ancestor_nests(table->ancestor_nests());
    for (uint i = root_no + 1; i < tab_no; i++) {
      if (m_join_scope.contain(i) &&         // Table is pushed
          !possible_ancestors.contain(i) &&  // Not already an ancestor
          ancestor_nests.contain(i) &&       // 3a) In ancestor-nests of table
          !m_scan_operations.contain(i)) {   // 3b) Is a single-row access type

        // 3c) All referred key_parents[] are possible_ancestors.
        const ndb_table_map all_key_parents(get_all_key_parents(&m_tables[i]));
        if (possible_ancestors.contain(all_key_parents)) {
          possible_ancestors.add(i);
        }
        // 3d) All required_ancestors are possible_ancestors.
        if (!m_tables[i].m_ancestors.is_clear_all() &&
            possible_ancestors.contain(m_tables[i].m_ancestors)) {
          possible_ancestors.add(i);
        }
      }
    }
    assert(m_join_scope.contain(possible_ancestors));

    /**
     * Calculate the set of tables where the referred Field values may be
     * handled as either constant or parameter values from a pushed condition.
     *
     * 1) const_expr_tables:
     *    Values from all tables in the 'm_const_scope' has been evalued prior
     *    to the query being pushed. Thus, their Field values are known and can
     *    be used to evaluated any expression they are part of into constants.
     *
     *    Note that we do not allow const_expr_tables if pushed join root is a
     *    lookup, where its EQRefIterator::Read may detect equal keys and
     *    optimize away the read of pushed join. (Note a similar limitation for
     *    keys in ::is_field_item_pushable()).
     *    TODO?: Integrate with setting of Index_lookup::disable_cache
     *    and lift these limitations when 'cache' is disabled.
     *
     * 2) param_expr_tables:
     *    The pushed join, including any pushed conditions embedded within it,
     *    is generated when the root of the pushed join is sent for execution.
     *    At this point in time the value of any Field from ancestor tables
     *    within the pushed join is still not known. However, when the
     *    SPJ block sends the REQuests to the LDMs, all ancestor tables in
     *    the pushed join are  available. This allows us to a build
     *    a parameter set containing the referred Field values, and supply
     *    it to the LDM's together with a pushed condition referring the
     *    parameters.
     */
    table_map const_expr_tables(0);
    if (m_scan_operations.contain(root_no)) {
      for (uint i = 0; i < root_no; i++) {
        if (m_const_scope.contain(i)) {
          const TABLE *table = m_tables[i].get_table();
          if (table != nullptr && table->pos_in_table_list != nullptr) {
            const_expr_tables |= table->pos_in_table_list->map();
          }
        }
      }
    }
    const Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    table_map param_expr_tables(0);
    if (ndbd_support_param_cmp(ndb->getMinDbNodeVersion())) {
      for (uint i = root_no; i < tab_no; i++) {
        if (possible_ancestors.contain(i)) {
          const TABLE *table = m_tables[i].get_table();
          param_expr_tables |= table->pos_in_table_list->map();
        }
      }
    }
    ha_ndbcluster *handler =
        down_cast<ha_ndbcluster *>(table->get_table()->file);
    handler->m_cond.prep_cond_push(pending_cond, const_expr_tables,
                                   param_expr_tables);
    pending_cond = handler->m_cond.m_remainder_cond;

    if (handler->m_cond.m_pushed_cond != nullptr) {
      const List<const Ndb_param> params =
          handler->m_cond.get_interpreter_params();
      if (unlikely(params.size() > ndb_pushed_join::MAX_LINKED_PARAMS)) {
        DBUG_PRINT("info",
                   ("Too many parameter Field refs ( >= MAX_LINKED_PARAMS) "
                    "encountered"));
        return false;
      }
      /* Force an ancestor dependency on tables referred as a parameter. */
      table_map used_tables(handler->m_cond.m_pushed_cond->used_tables());
      used_tables &= param_expr_tables;
      const ndb_table_map parents_of_condition(get_table_map(used_tables));
      table->m_ancestors.add(parents_of_condition);
      all_parents.add(parents_of_condition);
      depend_parents.add(parents_of_condition);
    }
  }
  if (pending_cond != nullptr) {
    m_has_pending_cond.add(tab_no);
  }
  if (m_scan_operations.contain(tab_no)) {
    // Check extra limitations on when index scan is pushable,
    if (!is_pushable_as_child_scan(table, all_parents)) {
      return false;
    }
  }

  /**
   * Set up the join-nest ancestor dependencies required by the set of
   * 'depend_tables' refered from this table. That is: the 'real'
   * outer-join nest dependencies, represented in m_ancestor_nests.
   */
  if (!set_ancestor_nests(table, depend_parents)) {
    return false;
  }

  /**
   * Calculate contribution to the required_ancestors() dependencies,
   * from this table. Each 'required_ancestor'-table need to be an
   * ancestor table when constructing the SPJ query-tree.
   * These ancestor dependencies are set as the required 'm_ancestors'
   * on each table, and the nest-level ancestors are provided
   * by required_ancestors(), and later used to enforce
   * ::optimize_query_plan() to use these tables as (grand-)parents
   */
  const uint first_inner = table->m_first_inner;
  // Only interested in the upper-nest-level dependencies:
  depend_parents.intersect(m_tables[first_inner].ancestor_nests());
  table->m_ancestors.add(depend_parents);

  // Our ancestor_nests need to cover required parents and ancestors
  assert(table->ancestor_nests().contain(depend_parents));
  assert(table->ancestor_nests().contain(required_ancestors(table)));

  // required_ancestors only cares about tables outside of inner_nest
  assert(!required_ancestors(table).contain(first_inner));
  assert(!required_ancestors(table).is_overlapping(table->m_inner_nest));

  m_join_scope.add(tab_no);
  return true;
}  // ndb_pushed_builder_ctx::is_pushable_as_child

/***************************************************************
 *  is_pushable_within_nest() / is_pushable_as_child_scan()
 *
 * There are additional limitation on when an index scan is pushable
 * relative to a (single row) primary key or unique key lookup operation.
 *
 * Such limitations exists for index scan operation being outer- or
 * semi-joined: Consider the query:
 *
 * select * from t1 left join t2
 *   on t1.attr=t2.ordered_index
 *   where predicate(t1.row, t2. row);
 *
 * Where 'predicate' cannot be pushed to the ndb. (a 'pending_cond', above!)
 * The ndb api may then return:
 *
 * +---------+---------+
 * | t1.row1 | t2.row1 | (First batch)
 * | t1.row2 | t2.row1 |
 * ..... (NextReq).....
 * | t1.row1 | t2.row2 | (Next batch)
 * +---------+---------+
 *
 * Since we could not return all t2 rows matching 't1.row1' in the first
 * batch, it is repeated for the next batch of t2 rows. From mysqld POW it
 * will appear as a different row, even if it is the same rows as returned
 * in the first batch. This works just fine when the nest loop joiner
 * create a plain INNER JOIN result; the different instances of 't1.row1'
 * would just appear a bit out of order. However OUTER JOIN is a different
 * matter:
 *
 * Assume that the rows [t1.row1, t2.row1] from the first batch does not
 * satisfies 'predicate'. As there are no more 't1.row1's in this batch,
 * mysqld will conclude it has seen all t1.row1's without any matching
 * t2 rows, Thus it will create a NULL extended t2 row in the (outer joined)
 * result set.
 *
 * As the same t1.row1 will be returned from the NDB API in the next batch,
 * mysqld will create a result row also for this instance - Either with yet
 * another NULL-extended t2 row, or possibly one or multiple matching rows.
 * In either case resulting in an incorrect result set. Like:
 * +---------+---------+
 * | t1.row1 | NULL    | -> Error!
 * | t1.row2 | t2.row1 |
 * | t1.row1 | t2.row2 |
 * +---------+---------+
 *
 * So in order to allow an outer joined index scan to be pushed, we need
 * to check that a row returned from a pushed index-scan will not later
 * be rejected by mysqld - i.e. the join has to be fully evaluated by SPJ
 * (in companion with the SPJ API):
 *
 *  1a) There should be no 'pending_cond' (unpushed conditions) on the
 *      table.
 *  1b) Neither could any *other* tables within the same inner_join nest
 *      have pending_cond's. (An inner join nest require matching rows
 *      from all tables in the nest: A non-matching pending_cond on a row
 *      from any table in the nest, will also eliminate the rows from the
 *      other tables. (Possibly creating false NULL-extensions)
 *  1c) Neither should any tables within the upper nests have
 *      pending_cond's. Consider the nest structure t1, (t2, (t3)),
 *      where t1 and 'this' table t3 are scans. If t2 has a pending
 *      condition, that condition may eliminate rows from the embedded
 *      (outer joined) t3 nest, and result in false NULL extended rows
 *      when t3 rows are fetched in multiple batches.
 *      (Note that this restriction does not apply to the uppermost nest
 *      containing t1: A non-matching condition on that table will eliminate
 *      the t1 row as well, thus there will be no extra NULL extended
 *      rows in the result set.
 * 1d)  There should not be any conditions on entire Join-nests (or sub-paths)
 *      between 'table' and the pushed join_root.
 *
 * 2)   There should be no unpushed tables in:
 * 2b)  In this inner_join nest.
 * 2c)  In any upper nests of this table.
 *
 *      This case is similar as for pending_cond: A non-match when mysqld
 *      joins in the rows from the unpushed table may eliminate rows
 *      returned from the pushed joins as well, resulting in extra
 *      NULL extended rows.
 *
 * 3)   In addition the join condition may explicitly specify dependencies
 *      on tables which are not in either of the upper_nests,
 *      eg t1, (t2,t3), (t4), where t4 has a join condition on t3.
 *      If either:
 * 3a)  t2 or t3 has an unpushed condition, possibly eliminating returned
 *      (t2,t3) rows, and the t4 rows depending on these being NOT NULL.
 * 3b)  t2 or t3 are not pushed, mysqld doesn't matching rows from these
 *      tables, which also eliminate the t4 rows, possibly resulting in
 *      extra NULL extended rows.
 *
 * Note that ::is_pushable_as_child_scan() can only check these conditions for
 * tables preceding it in the query plan. ::validate_join_nest() will later
 * do similar checks when we have completed a nest level. The later check
 * would be sufficient, however we prefer to 'fail fast'.
 *
 ****************************************************************/
bool ndb_pushed_builder_ctx::is_pushable_within_nest(const pushed_table *table,
                                                     ndb_table_map nest,
                                                     const char *nest_type) {
  DBUG_TRACE;

  const uint tab_no = table->get_table_no();
  assert(m_scan_operations.contain(tab_no));

  /**
   * 1) Check if outer- or semi-joined table depends on 'unpushed condition'
   */
  if (unlikely(m_has_pending_cond.contain(tab_no))) {  // 1a)
    // This table has unpushed condition
    EXPLAIN_NO_PUSH(
        "Can't push %s joined table '%s' as child of '%s', "
        "table condition can not be fully evaluated by pushed join",
        nest_type, table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
  }

  if (unlikely(m_has_pending_cond.is_overlapping(nest))) {  // 1b,1c:
    // Other (lookup tables) within nest has unpushed condition
    ndb_table_map pending_conditions(m_has_pending_cond);
    pending_conditions.intersect(nest);
    // Report the closest violating table, may be multiple.
    const uint violating = pending_conditions.last_table(tab_no);
    const TABLE *violating_table = m_tables[violating].get_table();
    if (violating_table != nullptr) {
      EXPLAIN_NO_PUSH(
          "Can't push %s joined table '%s' as child of '%s', "
          "condition on its dependant table '%s' is not pushed down",
          nest_type, table->get_table()->alias, m_join_root->get_table()->alias,
          violating_table->alias);
    } else {
      // The violating table was optimized away, e.g. 'zero rows'
      EXPLAIN_NO_PUSH(
          "Can't push %s joined table '%s' as child of '%s', "
          "a condition on a dependant table is not pushed down",
          nest_type, table->get_table()->alias,
          m_join_root->get_table()->alias);
    }
    return false;
  }

  // Unlike the 'pending conditions', which are (unpushed) conditions directly
  // on the tables, there can be conditions on top of entire join nests as well
  const bool has_filter_cond = table->has_condition_inbetween(m_join_root);
  if (unlikely(has_filter_cond)) {  // 1d
    EXPLAIN_NO_PUSH(
        "Can't push %s joined table '%s' as child of '%s', "
        "join-nest containing the table has FILTER conditions",
        nest_type, table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
  }

  /**
   * 2) Check if outer- or semi-joined table depends on 'unpushed tables'
   */
  if (unlikely(!m_join_scope.contain(nest))) {  // 2b,2c
    ndb_table_map unpushed_tables(nest);
    unpushed_tables.subtract(m_join_scope);
    // Report the closest unpushed table, may be multiple.
    const uint violating = unpushed_tables.last_table(tab_no);
    const TABLE *violating_table = m_tables[violating].get_table();
    if (violating_table != nullptr) {
      EXPLAIN_NO_PUSH(
          "Can't push %s joined table '%s' as child of '%s', "
          "table '%s' in its dependant join-nest(s) is not part of the "
          "pushed join",
          nest_type, table->get_table()->alias, m_join_root->get_table()->alias,
          violating_table->alias);
    } else {
      // The violating table was optimized away, e.g. 'zero rows'
      EXPLAIN_NO_PUSH(
          "Can't push %s joined table '%s' as child of '%s', "
          "a table in its dependant join-nest(s) is not part of the "
          "pushed join",
          nest_type, table->get_table()->alias,
          m_join_root->get_table()->alias);
    }
    return false;
  }
  return true;
}

bool ndb_pushed_builder_ctx::is_pushable_as_child_scan(
    const pushed_table *table, const ndb_table_map all_parents) {
  DBUG_TRACE;

  const uint root_no = m_join_root->get_table_no();
  const uint tab_no = table->get_table_no();
  assert(m_scan_operations.contain(tab_no));

  if (table->isOuterJoined(*m_join_root)) {
    /**
     * Is an outer join relative to root. Even if tab_no is inner_joined with
     * another parent than 'root', any restrictions on scan operations still
     * apply.
     */

    /**
     * Online upgrade, check if we are connected to a 'ndb' allowing us to push
     * outer joined scan operation (ver >= 8.0.20), Else we reject pushing.
     */
    const Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    if (unlikely(!NdbQueryBuilder::outerJoinedScanSupported(ndb))) {
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as child of '%s', "
          "outer join of scan-child not implemented",
          table->get_table()->alias, m_join_root->get_table()->alias);
      return false;
    }

    /**
     * Calculate the set of tables being outer joined relative to root.
     * i.e. the tables which may be incorrectly NULL extended due to
     * unpushed conditions and tables. These are the tables we check
     * the above 1b,1c,2b and 2c cases against.
     */
    ndb_table_map outer_join_nests(table->embedding_nests());
    outer_join_nests.subtract(m_join_root->get_full_inner_nest());
    outer_join_nests.subtract(m_join_root->m_upper_nests);

    const char *join_type =
        table->isAntiJoined(*m_join_root) ? "anti" : "outer";
    if (!is_pushable_within_nest(table, outer_join_nests, join_type)) {
      return false;
    }

    /**
     * 3) Check if any tables outside of the embedding nest are referred.
     */
    const ndb_table_map embedding_nests(table->embedding_nests());
    if (unlikely(!embedding_nests.contain(all_parents))) {           // 3)
      if (unlikely(!embedding_nests.contain(m_has_pending_cond))) {  // 3a)
        EXPLAIN_NO_PUSH(
            "Can't push %s joined table '%s' as child of '%s', "
            "exists unpushed condition in join-nests it depends on",
            join_type, table->get_table()->alias,
            m_join_root->get_table()->alias);
        return false;
      }

      // Calculate all unpushed tables prior to this table.
      ndb_table_map unpushed_tables;
      unpushed_tables.set_prefix(tab_no);
      unpushed_tables.subtract(m_join_scope);
      if (root_no > 0) {
        ndb_table_map root_prefix;
        root_prefix.set_prefix(root_no);
        unpushed_tables.subtract(root_prefix);
      }

      /**
       * Note that the check below is a bit too strict, we check:
       *  'Are there any unpushed tables outside of our embedding nests',
       *  instead of 'Do we refer tables from nests outside embedding nests,
       *  having unpushed tables'. As we already know 'all_parents' are not
       *  contained in 'embedding'.
       * The outcome should be the same except if we have parent refs to
       * multiple non-embedded nests. (very unlikely)
       */
      if (unlikely(!embedding_nests.contain(unpushed_tables))) {  // 3b)
        EXPLAIN_NO_PUSH(
            "Can't push %s joined table '%s' as child of '%s', "
            "table depends on join-nests with unpushed tables",
            join_type, table->get_table()->alias,
            m_join_root->get_table()->alias);
        return false;
      }
    }
  }  // end 'outer joined scan'

  /**
   * As for outer joins, there are restrictions for semi joins:
   *
   * Scan-scan result may return the same ancestor-scan rowset
   * multiple times when rowset from child scan has to be fetched
   * in multiple batches (as above). This is fine for nested loop
   * evaluations of pure loops, as it should just produce the total
   * set of join combinations - in any order.
   *
   * However, the different semi join strategies (FirstMatch,
   * Loosescan, Duplicate Weedout) requires that skipping
   * a row (and its nested loop ancestors) is 'permanent' such
   * that it will never reappear in later batches.
   *
   * So we do not (yet) allow an index-scan to be semi-joined.
   *
   * Note that it is the semi_join properties relative to the
   * other tables we join with which matter - A table joining
   * with another table within the same semi_join nest is an
   * INNER JOIN wrt. that other table. (Which is pushable)
   */

  ndb_table_map sj_nest(table->m_sj_nest);
  if (sj_nest.contain(tab_no)) {
    const Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    if (unlikely(!NdbQueryBuilder::outerJoinedScanSupported(ndb))) {
      // Semi-join need support by data nodes
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as child of '%s', "
          "semi join of scan-child not supported by data nodes",
          table->get_table()->alias, m_join_root->get_table()->alias);
      return false;
    }
    sj_nest.intersect(table->embedding_nests());
    if (!is_pushable_within_nest(table, sj_nest, "semi")) {
      return false;
    }
  }
  // end 'semi_join' handling

  /**
   * Note, for both 'outer join', and 'semi joins restriction above:
   *
   * The restriction could have been lifted if we could
   * somehow ensure that all rows from a child scan are fetched
   * before we move to the next ancestor row.
   *
   * Which is why we do not force the same restrictions on lookup.
   */

  return true;
}  // ndb_pushed_builder_ctx::is_pushable_as_child_scan

/***************************************************************
 *
 * set_ancestor_nests()
 *
 * Check the ancestor dependencies required by a set of 'depend_parents'
 * tables for not conflicting with previous ancestor dependencies already
 * required. If not conflicting, set the ancestors required by this
 * table.
 *
 * Note that if we unintentionally add equality dependencies between
 * join nests, we could changed the join semantic specified in query
 *
 * If this table has multiple dependencies, it can only be added to
 * the set of pushed tables if the dependent tables themself
 * depends, or could be make dependent, on each other.
 *
 * Such new dependencies can only be added iff all 'depend_parents'
 * are in the same 'inner join nest', i.e. we can not add *new*
 * dependencies on outer joined tables (or nests).
 *
 * A typical example is t1 oj (t2) oj (t3) oj (t4), where t4.join_cond
 * refers *both* the non_embedding tables t2 and t3. In such cases t4 can not
 * be pushed unless t3 already has a join condition depending on t2.
 * Note that the header file ha_ndbcluster_push.h contains more
 * extensive comments regarding this.
 *
 * Algorithm:
 * 1. Calculate the minimum set of 'dependencies' for the
 *    key_parents[]. (Supplied as argument)
 *
 * 2. Check the 'dependencies' set, starting at the last (the
 *    table closest to this table). Check that it either already
 *    exists a dependency between each such table and the remaining
 *    dependent tables, or that we are allowed to add the required
 *    dependencies.
 ***************************************************************/
bool ndb_pushed_builder_ctx::set_ancestor_nests(
    pushed_table *table, const ndb_table_map depend_parents) {
  DBUG_TRACE;

  const uint tab_no = table->get_table_no();
  const uint first_inner = table->m_first_inner;

  /**
   * Include nest-level ancestor dependencies already enforced.
   */
  ndb_table_map dependencies(depend_parents);
  dependencies.add(required_ancestors(table));

  /**
   * Check that all parents we depend on are available from within the
   * ancestor_nests. This include m_ancestor_nests previously extended
   * with previous references to tables not in the direct line of
   * upper nests. Which then become a part of later ancestor_nests being
   * referrable.
   */
  const uint parent_no = dependencies.last_table(tab_no - 1);
  dependencies.clear_bit(parent_no);
  const pushed_table *const parent = &m_tables[parent_no];

  // If remaining dependencies are unavailable from parent, we can't push
  if (!parent->ancestor_nests().contain(dependencies)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "as it would make the parent table '%s' "
        "depend on table(s) outside of its join-nest",
        table->get_table()->alias, m_join_root->get_table()->alias,
        parent->get_table()->alias);
    return false;
  }

  /**
   * Allow all tables in the referred parents nest to become
   * part of the set of later referrable ancestor_nests.
   */
  if (table->isInnerJoined(*parent)) {
    table->m_ancestor_nests = m_tables[first_inner].m_ancestor_nests;
  } else {  // Outer joins with parent
    /**
     * Referring an outer-joined parent: We verified above above that all
     * remaining depend_tables are available as 'ancestors' of the
     * selected parent - which also becomes ancestors of this table,
     * including all tables in the parent inner-nest.
     */
    assert(table->isOuterJoined(*parent));
    ndb_table_map ancestor_nests(parent->m_ancestor_nests);
    ancestor_nests.add(parent->get_inner_nest(tab_no));
    table->m_ancestor_nests = ancestor_nests;
    m_tables[first_inner].m_ancestor_nests = ancestor_nests;
  }
  return true;
}

/*****************************************************************************
 * validate_join_nest()
 *
 * A join-nest has been completed by ::is_pushable_with_root().
 * If the join nest is outer joined with other tables in the pushed join, and
 * if this nest, or other nests embedded within it contains (outer joined)
 * table scans, an extra 'validate' of the pushed joins is required:
 *
 * We need to 'validate' that none of these 'invalid' cases exists for
 * the join nest:
 *
 *  1) Some of the tables in the nest were not pushed.
 *  2) Some of the pushed tables in the nest has (remaining parts of)
 *     conditions not being pushed.
 *
 * The above restrictions are similar to the ones checked for outer joined
 * table scans in is_pushable_as_child(), where we preferably try to catch
 * these restrictions. However, at that point in time we are not able to
 * perform this check for tables later in the query plan.
 *
 * So we need similar checks for validating the entire nest when it has been
 * completed. If the nest fails the 'validate', no outer joined table scans
 * should have been pushed as part of the nest, or in nests embedded within
 * this nest. Thus they have to be removed from the pushed join.
 * (Using ::remove_pushable())
 *
 * Note that validate_join_nest() check the entire nest, so the similar
 * checks on outer joined scans could have been skipped from
 * is_pushable_as_child(). However, we want to catch these non pushable
 * tables as early as possible, so we effectively duplicates these checks.
 ******************************************************************************/
void ndb_pushed_builder_ctx::validate_join_nest(ndb_table_map inner_nest,
                                                const uint first_inner,
                                                const uint last_inner,
                                                const char *nest_type) {
  DBUG_TRACE;
  if (first_inner <= m_join_root->get_table_no()) return;

  ndb_table_map scans_in_join_scope(m_scan_operations);
  scans_in_join_scope.intersect(m_join_scope);

  // This nest, or nests embedded within it, has pushed scan operations?
  const bool nest_has_scans =
      (scans_in_join_scope.first_table(first_inner) <= last_inner);
  if (nest_has_scans) {
    // Check both of the reject reasons from the topmost comment
    const bool nest_has_unpushed = !m_join_scope.contain(inner_nest);
    const bool nest_has_pending_cond =
        inner_nest.is_overlapping(m_has_pending_cond);

    if (nest_has_pending_cond || nest_has_unpushed) {
      /**
       * Check all pushed scan operations in this nest, and nests embedded
       * within it. Note that it is the rows from scans in the upper nest
       * which may be repeated, creating false NULL extended rows from scans
       * in inner_nests.
       */
      for (uint tab_no = scans_in_join_scope.first_table(first_inner);
           tab_no <= last_inner;
           tab_no = scans_in_join_scope.first_table(tab_no + 1)) {
        if (!m_join_scope.contain(tab_no)) {
          continue;  // Possibly already removed by remove_pushable()
        }
        pushed_table *const table = &m_tables[tab_no];

        /**
         * Could have checked both reject conditions at once, but would
         * like to provide separate EXPLAIN_NO_PUSH's for each of them.
         */
        if (nest_has_unpushed) {
          EXPLAIN_NO_PUSH(
              "Can't push %s joined table '%s' as child of '%s', "
              "some tables in embedding join-nest(s) are not part of pushed "
              "join",
              nest_type, table->get_table()->alias,
              m_join_root->get_table()->alias);
          remove_pushable(table);
        } else if (nest_has_pending_cond) {
          EXPLAIN_NO_PUSH(
              "Can't push %s joined table '%s' as child of '%s', "
              "join-nest containing the table has pending unpushed_conditions",
              nest_type, table->get_table()->alias,
              m_join_root->get_table()->alias);
          remove_pushable(table);
        }
      }
    }
  }  // nest_has_scans
}  // ndb_pushed_builder_ctx::validate_join_nest

/**********************************************************************
 * ::remove_pushable()
 *
 * A Table was first included in a pushed join query, but later found to
 * not be pushable. Thus it has to be removed by this method.
 *
 * All other pushed tables are checked for dependencies on the table
 * being removed, and possible cascade-removed if they can no longer
 * be part of the pushed join without the removed table(s).
 **********************************************************************/
void ndb_pushed_builder_ctx::remove_pushable(const pushed_table *const table) {
  DBUG_TRACE;

  const uint me = table->get_table_no();
  assert(m_join_scope.contain(me));
  m_join_scope.clear_bit(me);

  // Cascade remove of tables depending on 'me'
  for (uint tab_no = me + 1; tab_no < m_table_count; tab_no++) {
    pushed_table *other_table = &m_tables[tab_no];

    if (m_join_scope.contain(tab_no)) {
      ndb_table_map *key_parents = other_table->m_key_parents;
      for (uint i = 0; i < other_table->get_no_of_key_fields(); i++) {
        if (!key_parents[i].is_clear_all()) {
          // Was referring some parent field(s) (not const, or params)
          // Remove parent references not in join_scope any more
          key_parents[i].intersect(m_join_scope);

          if (key_parents[i].is_clear_all()) {
            // All preceding parent tables removed from join_scope.
            m_join_scope.clear_bit(tab_no);  // Cascade remove of this table
            break;
          }
        }
      }
    }

    if (m_join_scope.contain(tab_no)) {
      // Check if parents referred from pushed condition are still pushed.
      ha_ndbcluster *handler =
          down_cast<ha_ndbcluster *>(other_table->get_table()->file);
      if (handler->m_cond.m_pushed_cond != nullptr) {
        table_map used_tables(handler->m_cond.m_pushed_cond->used_tables());
        ndb_table_map parents_of_condition = get_table_map(used_tables);
        parents_of_condition.subtract(m_const_scope);
        if (!m_join_scope.contain(parents_of_condition)) {
          // Some tables referred from pushed condition removed from join_scope
          m_join_scope.clear_bit(tab_no);  // Cascade remove of this table
        }
      }
    }
  }
}  // ndb_pushed_builder_ctx::remove_pushable

/*********************
 * This method examines a key item (could be part of a lookup key or a scan
 * bound) for a table access operation and calculates the set of possible
 * parents. (These are possible parent table access operations in the query
 * tree that will be pushed to the ndb.)
 *
 * @param[in] table The table access operation to which the key item belongs.
 * @param[in] key_item The key_item to examine
 * @param[in] key_part Metadata about the key item.
 * @param[out] field_parents The set of possible parents for 'key_item'
 * ('join_root' if keys are constant).
 * @return True if at least one possible parent was found. (False means that
 * operation cannot be pushed).
 */
bool ndb_pushed_builder_ctx::is_field_item_pushable(
    pushed_table *table, const Item *key_item, const KEY_PART_INFO *key_part,
    ndb_table_map &field_parents) {
  DBUG_TRACE;
  const uint tab_no = table->get_table_no();
  assert(key_item->type() == Item::FIELD_ITEM);

  const Item_field *const key_item_field =
      static_cast<const Item_field *>(key_item);

  DBUG_PRINT(
      "info",
      ("keyPart:%d, field:%s.%s", (int)(key_item - table->get_key_field(0)),
       key_item_field->field->table->alias, key_item_field->field->field_name));

  if (!key_item_field->field->eq_def(key_part->field)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, "
        "column '%s' does not have same datatype as ref'ed "
        "column '%s.%s'",
        table->get_table()->alias, key_part->field->field_name,
        key_item_field->field->table->alias, key_item_field->field->field_name);
    table->set_table_properties(
        table->get_table_properties() &
        ~PUSHABLE_AS_CHILD);  // Permanently disable as child
    return false;
  }

  if (key_item_field->field->is_virtual_gcol()) {
    EXPLAIN_NO_PUSH("Can't push condition on virtual generated column '%s.%s'",
                    key_item_field->field->table->alias,
                    key_item_field->field->field_name);
    return false;
  }

  /**
   * Below this point 'key_item_field' is a candidate for referring a parent
   * table in a pushed join. It should either directly refer a parent common to
   * all FIELD_ITEMs, or refer a grandparent of this common parent. There are
   * different cases which should be handled:
   *
   *  1) 'key_item_field' may already refer one of the parent available within
   * our pushed scope. 2)  By using the equality set, we may find alternative
   * parent references which may make this a pushed join.
   */

  ///////////////////////////////////////////////////////////////////
  // 0) Prepare for calculating parent candidates for this FIELD_ITEM
  //
  field_parents.clear_all();

  ////////////////////////////////////////////////////////////////////
  // 1) Add our existing parent reference to the set of parent candidates
  //
  const uint referred_table_no = get_table_no(key_item_field);
  if (m_join_scope.contain(referred_table_no)) {
    field_parents.add(referred_table_no);
  }

  //////////////////////////////////////////////////////////////////
  // 2) Use the equality set to possibly find more parent candidates
  //    usable by substituting existing 'key_item_field'.
  //    The hypergraph optimizer do not provide a reliable Item_equal.
  //
  const Item_equal *item_equal = (!m_thd->lex->using_hypergraph_optimizer)
                                     ? table->get_item_equal(key_item_field)
                                     : nullptr;
  if (item_equal != nullptr) {
    for (const Item_field &substitute_field : item_equal->get_fields()) {
      if (&substitute_field != key_item_field) {
        const uint substitute_table_no = get_table_no(&substitute_field);

        // Substitute table need to:
        // 1) Be part of this pushed join,
        // 2) Should either not be part of a semi-join 'nest', or be part
        //    of the same sj-nest as either this 'table' or the referred
        //    table. This limitation is due to the batch fetch mechanism
        //    in SPJ: The 'firstMatch' duplicate elimination may
        //    break out from iterating all the scan-batch combinations,
        //    such that result rows may be omitted.
        if (!m_join_scope.contain(substitute_table_no)) continue;  // 1)
        const ndb_table_map sj_nest(m_tables[substitute_table_no].m_sj_nest);

        if (sj_nest.is_clear_all() ||  // 2)
            sj_nest.contain(referred_table_no) || sj_nest.contain(tab_no)) {
          DBUG_PRINT("info",
                     (" join_items[%d] %s.%s can be replaced with %s.%s",
                      (int)(key_item - table->get_key_field(0)),
                      get_referred_table_access_name(key_item_field),
                      get_referred_field_name(key_item_field),
                      get_referred_table_access_name(&substitute_field),
                      get_referred_field_name(&substitute_field)));

          field_parents.add(substitute_table_no);
        }
      }
    }  // for all item_equal->fields
  }
  if (!field_parents.is_clear_all()) {
    return true;
  }

  if (m_const_scope.contain(referred_table_no)) {
    // This key item is const. and did not cause the set of possible parents
    // to be recalculated. Reuse what we had before this key item.
    assert(field_parents.is_clear_all());

    /**
     * Field reference is a 'paramValue' to a column value evaluated
     * prior to the root of this pushed join candidate. Some restrictions
     * applies to when a field reference is allowed in a pushed join:
     */
    if (!m_scan_operations.contain(m_join_root->get_table_no())) {
      assert(!m_scan_operations.contain(tab_no));
      /**
       * EQRefIterator may optimize away key reads if the key
       * for a requested row is the same as the previous.
       * Thus, iff this is the root of a pushed lookup join
       * we do not want it to contain children with references
       * to columns 'outside' the the pushed joins, as these
       * may still change between calls to
       * EQRefIterator::Read() independent of the root key
       * itself being the same.
       */
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as child of '%s', since "
          "it referes to column '%s.%s' prior to a "
          "potential 'const' root.",
          table->get_table()->alias, m_join_root->get_table()->alias,
          get_referred_table_access_name(key_item_field),
          get_referred_field_name(key_item_field));
      return false;
    }  // if (!m_scan_operations...)
    return true;
  }

  /**
   * We have rejected this 'key_item' as not pushable, provide an explain:
   * There are 2 different cases:
   * 1) The table referred by key_item was not in the query_scope we were
   *    allowed to join with, and no substitutes existed.
   * 2) The referred table was not pushed, (and reported as such).
   *    Thus, we could not push the tables referring it either.
   */
  const ndb_table_map all_query_scopes =
      table->get_tables_in_all_query_scopes();

  if (!all_query_scopes.contain(referred_table_no)) {
    // Referred table was not in allowed query_scope.
    const char *scope_type;
    if (referred_table_no < tab_no) {
      const pushed_table *referred_table = &m_tables[referred_table_no];
      scope_type = referred_table->get_scope_description();
    } else {
      scope_type = "subquery";
    }
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "column '%s.%s' is in a %s-branch which can't be referred",
        table->get_table()->alias, m_join_root->get_table()->alias,
        get_referred_table_access_name(key_item_field),
        get_referred_field_name(key_item_field), scope_type);
  } else {
    // We referred a table which was not pushed.
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "column '%s.%s' refers a table which was not pushed",
        table->get_table()->alias, m_join_root->get_table()->alias,
        get_referred_table_access_name(key_item_field),
        get_referred_field_name(key_item_field));
  }
  return false;
}  // ndb_pushed_builder_ctx::is_field_item_pushable()

bool ndb_pushed_builder_ctx::is_const_item_pushable(
    const Item *key_item, const KEY_PART_INFO *key_part) {
  DBUG_TRACE;
  assert(key_item->const_for_execution());

  /**
   * Propagate Items constant value to Field containing the value of this
   * key_part:
   */
  Field *const field = key_part->field;
  const int error =
      const_cast<Item *>(key_item)->save_in_field_no_warnings(field, true);
  if (unlikely(error)) {
    DBUG_PRINT("info", ("Failed to store constant Item into Field -> not"
                        " pushable"));
    return false;
  }
  if (field->is_real_null()) {
    DBUG_PRINT("info", ("NULL constValues in key -> not pushable"));
    return false;  // TODO, handle graceful -> continue?
  }
  return true;
}  // ndb_pushed_builder_ctx::is_const_item_pushable()

/**
 * ::optimize_query_plan()
 *
 * Decide the final execution order for the pushed joins. That mainly
 * involves deciding which table to be used as the 'm_parent'.
 *
 * The m_parent is chosen based on the available m_key_parents[]
 * which were set up by ::is_pushable_as_child(), and possibly later
 * modified (reduced) by ::validate_join_nest().
 *
 * When multiple parent candidates are available, we choose the one
 * closest to the root, which will result in the most 'bushy' tree
 * structure and the highest possible parallelism. Note that SPJ block
 * will build its own execution plan (based on what's being set up here)
 * which possible sequentialize the execution of these parallel branches.
 * (See WL#11164)
 */
void ndb_pushed_builder_ctx::optimize_query_plan() {
  DBUG_TRACE;
  const uint root_no = m_join_root->get_table_no();

  // Find an optimal m_parent to be used when joining the tables
  for (uint tab_no = m_table_count - 1; tab_no > root_no; tab_no--) {
    if (!m_join_scope.contain(tab_no)) continue;
    pushed_table &table = m_tables[tab_no];

    /**
     * Calculate the set of possible parents for the table, where:
     *  - 'common' are those we may refer (possibly through the EQ-sets)
     *     such that all FIELD_ITEMs are from the same parent.
     *  - 'extended' are those parents referred from some of the
     *     FIELD_ITEMs, and having the rest of the referred FIELD_ITEM
     *     tables available as 'grandparent refs'
     *     (The SPJ block can handle field references to any ancestor
     *      operation, not just the (direct) parent).
     *
     * In addition there are firm dependencies between some parents
     * such that all 'depend_parents' must be referred as an ancestors
     * of the table. By default 'depend_parents' will at least contain
     * the most 'grandparent' of the extended parents.
     */
    ndb_table_map *key_parents = table.m_key_parents;
    ndb_table_map common_parents(m_join_scope);
    ndb_table_map extend_parents;
    ndb_table_map depend_parents;

    for (uint i = 0; i < table.get_no_of_key_fields(); i++) {
      assert(m_join_scope.contain(key_parents[i]));
      if (!key_parents[i].is_clear_all()) {  // Key refers a parent field
        /**
         * Calculate 'common_parents' as the set of possible 'field_parents'
         * available from all 'key_part'.
         */
        common_parents.intersect(key_parents[i]);

        /**
         * 'Extended' parents are referred from some 'FIELD_ITEM', and contain
         * all parents directly referred, or available as 'depend_parents'.
         * The later excludes those before the first (grand-)parent
         * available from all 'field_parents' (first_grandparent).
         * However, it also introduce a dependency of those
         * tables to really be available as grand parents.
         */
        extend_parents.add(key_parents[i]);

        const uint first = key_parents[i].first_table(root_no);
        depend_parents.add(first);
      }
    }
    /**
     * The uppermost table referred by the keys decides the 'span'
     * of join nests we depends upon.
     */
    const int first_key_parent = depend_parents.first_table(root_no);

    /**
     * Previous children might already have enforced some ancestors to be
     * available through this table due to some ancestors being referred by
     * them, add these.
     */
    depend_parents.add(table.m_ancestors);

    /**
     * Same goes for nest-level dependencies: The 'first' in each nest
     * may enforce ancestor dependencies on the members of the nest.
     * Add enforcement of these up to the 'first' parent referred by
     * the 'key_parents[]'.
     */
    int first_in_nest = table.m_first_inner;
    while (first_key_parent < first_in_nest) {
      depend_parents.add(required_ancestors(&m_tables[first_in_nest]));
      first_in_nest = m_tables[first_in_nest].m_first_upper;
    }

    /**
     * All 'depend_parents' has to be fulfilled, starting from the 'last',
     * closest to this tab_no. The 'depend_parents' not directly referred
     * as a parent from this table, will be fulfilled by adding them as required
     * ancestors of the chosen parent, see below.
     * Find the first dependency to fulfill:
     */
    const uint depends_on_parent = depend_parents.last_table(tab_no - 1);

    /**
     * We try to find a parent within our own nest among the common_
     * or extend_parents, but also takes the required depends_on_parent
     * into consideration. Establish the lowest parent candidate
     * we may accept.
     */
    const uint first_candidate =
        std::max(depends_on_parent, table.m_first_inner);

    /**
     * Find a parent among common_parent (preferred) or extend_parent
     * if possible, else choose the first we depends_on.
     *
     * Choose parent to be the first possible among 'parents'.
     * Result in the most 'bushy' query plan, enabling most parallelism
     */
    uint parent_no = common_parents.first_table(first_candidate);
    if (parent_no >= tab_no) {  // Not found
      parent_no = extend_parents.first_table(first_candidate);
      if (parent_no >= tab_no) {  // Not found
        parent_no = depends_on_parent;
      }
    }
    assert(parent_no < tab_no);
    table.m_parent = parent_no;

    /**
     * Record the ancestors this table now depends on.
     * Will be included in later required_ancestors() calculations
     * for other tables depending on nests which 'table' is included in.
     */
    table.m_ancestors.add(parent_no);
    table.m_ancestors.add(depend_parents);

    /**
     * Any remaining ancestor dependencies for this table has to be
     * added to the selected parent in order to be taken into account
     * for parent calculation for its ancestors.
     */
    depend_parents.intersect(m_tables[parent_no].ancestor_nests());
    m_tables[parent_no].m_ancestors.add(depend_parents);
  }

  /* Collect the full set of ancestors available through the selected 'm_parent'
   */
  for (uint tab_no = root_no + 1; tab_no < m_table_count; tab_no++) {
    if (m_join_scope.contain(tab_no)) {
      pushed_table &table = m_tables[tab_no];
      const uint parent_no = table.m_parent;
      table.m_ancestors = m_tables[parent_no].m_ancestors;
      table.m_ancestors.add(parent_no);
    }
  }
}  // ndb_pushed_builder_ctx::optimize_query_plan

void ndb_pushed_builder_ctx::collect_key_refs(const pushed_table *table,
                                              const Item *key_refs[]) const {
  DBUG_TRACE;

  const uint parent_no = table->m_parent;
  const ndb_table_map ancestors(table->m_ancestors);

  assert(m_join_scope.contain(ancestors));
  assert(ancestors.contain(parent_no));

  /**
   * If there are any key_fields with 'current_parents' different from
   * our selected 'parent', we have to find substitutes for
   * those key_fields within the equality set.
   * When using the Hypergraph optimizer we can't use the Item_equal's.
   **/
  const bool use_item_equal = !m_thd->lex->using_hypergraph_optimizer;

  for (uint key_part_no = 0; key_part_no < table->get_no_of_key_fields();
       key_part_no++) {
    const Item *const key_item = table->get_key_field(key_part_no);
    key_refs[key_part_no] = key_item;

    assert(key_item->const_for_execution() ||
           key_item->type() == Item::FIELD_ITEM);

    if (use_item_equal && key_item->type() == Item::FIELD_ITEM) {
      const Item_field *join_item = static_cast<const Item_field *>(key_item);
      uint referred_table_no = get_table_no(join_item);
      Item_equal *item_equal = table->get_item_equal(join_item);

      if (referred_table_no != parent_no && item_equal != nullptr) {
        for (const Item_field &substitute_field : item_equal->get_fields()) {
          ///////////////////////////////////////////////////////////
          // Prefer to replace join_item with ref. to selected parent.
          //
          const uint substitute_table_no = get_table_no(&substitute_field);
          if (substitute_table_no == parent_no) {
            DBUG_PRINT("info",
                       (" Replacing key_refs[%d] %s.%s with %s.%s (parent)",
                        key_part_no, get_referred_table_access_name(join_item),
                        get_referred_field_name(join_item),
                        get_referred_table_access_name(&substitute_field),
                        get_referred_field_name(&substitute_field)));

            referred_table_no = substitute_table_no;
            key_refs[key_part_no] = &substitute_field;
            break;
          }

          if (ancestors.contain(substitute_table_no)) {
            assert(substitute_table_no <= parent_no);

            //////////////////////////////////////////////////////////////////////
            // Second best is to replace join_item with closest grandparent ref.
            // In this case we will continue to search for the common parent
            // match: Updates key_refs[] if:
            //   1): Replace incorrect refs of tables not being an 'ancestor'.
            //   2): Found a better substitute closer to selected parent
            //
            if (!ancestors.contain(referred_table_no) ||  // 1
                referred_table_no < substitute_table_no)  // 2)
            {
              DBUG_PRINT(
                  "info",
                  (" Replacing key_refs[%d] %s.%s with %s.%s (grandparent)",
                   key_part_no, get_referred_table_access_name(join_item),
                   get_referred_field_name(join_item),
                   get_referred_table_access_name(&substitute_field),
                   get_referred_field_name(&substitute_field)));

              referred_table_no = substitute_table_no;
              key_refs[key_part_no] = join_item = &substitute_field;
            }
          }
        }  // for all item_equal->fields

        assert(referred_table_no == parent_no ||
               ancestors.contain(referred_table_no) ||
               m_const_scope.contain(
                   referred_table_no));  // Is a 'const' paramValue
      }
    }  // Item::FIELD_ITEM
  }

  key_refs[table->get_no_of_key_fields()] = nullptr;
}  // ndb_pushed_builder_ctx::collect_key_refs()

/**
 * For the specified table; build the set of NdbQueryOperands defining
 * the (index-) key value for fetching rows from the table.
 *
 * Key values may consist of a mix of const-, param- and linkedValue(),
 * as collected by the utility method ::collect_key_refs().
 *
 * A linkedValue() should preferably refer a value from the 'm_parent'
 * of the table. If the referred field is not available from parent,
 * another ancestor may also be used. In the later case, SPJ will
 * need to store the referred ancestor value, such that it can be located
 * by the correlation-ids through the chain of ancestors.
 *
 * SPJ API will normally deduct the parent / ancestor topology based
 * on the table(s) being referred by the linkedValues(). In case of multiple
 * tables being referred, the API will check that the set of ancestors
 * depends on (are ancestors of-) each other, such that all referred tables
 * are available through the chain of ancestors.
 *
 * In rare cases we may introduce extra parent dependencies in order to
 * establish a common set of ancestors. To maintain the join semantics, this
 * is only supported when the added dependencies are to tables in same
 * inner join-nest. Restriction applying to the above is checked by
 * is_pushable_as_child(). However ::build_key() need to enforce the
 * added dependencies by calling NdbQueryOptions::setParent(). (below)
 */
int ndb_pushed_builder_ctx::build_key(const pushed_table *table,
                                      const NdbQueryOperand *op_key[],
                                      NdbQueryOptions *key_options) {
  DBUG_TRACE;
  const uint tab_no = table->get_table_no();
  assert(m_join_scope.contain(tab_no));

  const KEY *const key = &table->get_table()->key_info[table->get_index_no()];
  op_key[0] = nullptr;

  if (table == m_join_root) {
    if (!m_scan_operations.contain(tab_no)) {
      for (uint i = 0; i < key->user_defined_key_parts; i++) {
        op_key[i] = m_builder->paramValue();
        if (unlikely(op_key[i] == nullptr)) {
          return -1;
        }
      }
      op_key[key->user_defined_key_parts] = nullptr;
    }
  } else {
    const uint key_fields = table->get_no_of_key_fields();
    assert(key_fields > 0 && key_fields <= key->user_defined_key_parts);
    uint map[ndb_pushed_join::MAX_LINKED_KEYS + 1];

    if (!m_scan_operations.contain(tab_no)) {
      const ha_ndbcluster *handler =
          down_cast<ha_ndbcluster *>(table->get_table()->file);
      const NDB_INDEX_DATA &index = handler->m_index[table->get_index_no()];
      index.fill_column_map(key, map);
    } else {
      for (uint ix = 0; ix < key_fields; ix++) {
        map[ix] = ix;
      }
    }

    const Item *join_items[ndb_pushed_join::MAX_LINKED_KEYS + 1];
    collect_key_refs(table, join_items);

    ndb_table_map referred_parents;
    const KEY_PART_INFO *key_part = key->key_part;
    for (uint i = 0; i < key_fields; i++, key_part++) {
      const Item *const item = join_items[i];
      op_key[map[i]] = nullptr;

      if (item->const_for_execution()) {
        /**
         * Propagate Items constant value to Field containing the value of this
         * key_part:
         */
        Field *const field = key_part->field;
        assert(!field->is_real_null());
        const uchar *const ptr =
            (field->real_type() == MYSQL_TYPE_VARCHAR)
                ? field->field_ptr() + field->get_length_bytes()
                : field->field_ptr();

        op_key[map[i]] = m_builder->constValue(ptr, field->data_length());
      } else {
        assert(item->type() == Item::FIELD_ITEM);
        const Item_field *const field_item =
            static_cast<const Item_field *>(item);
        const uint referred_table_no = get_table_no(field_item);
        referred_parents.add(referred_table_no);

        if (m_join_scope.contain(referred_table_no)) {
          // Locate the parent operation for this 'join_items[]'.
          // May refer any of the preceding parent tables
          const NdbQueryOperationDef *const parent_op =
              m_tables[referred_table_no].m_op;
          assert(parent_op != nullptr);

          // TODO use field_index ??
          op_key[map[i]] = m_builder->linkedValue(
              parent_op, field_item->original_field_name());
        } else {
          assert(m_const_scope.contain(referred_table_no));
          // Outside scope of join plan, Handle as parameter as its value
          // will be known when we are ready to execute this query.
          if (unlikely(m_fld_refs >= ndb_pushed_join::MAX_REFERRED_FIELDS)) {
            DBUG_PRINT("info", ("Too many Field refs ( >= MAX_REFERRED_FIELDS) "
                                "encountered"));
            return -1;  // TODO, handle gracefully -> continue?
          }
          m_referred_fields[m_fld_refs++] = field_item->field;
          op_key[map[i]] = m_builder->paramValue();
        }
      }

      if (unlikely(op_key[map[i]] == nullptr)) {
        return -1;
      }
    }
    op_key[key_fields] = nullptr;

    // Might have to explicit set the designated parent.
    const uint parent_no = table->m_parent;
    if (!referred_parents.contain(parent_no)) {
      // Add the parent as a new dependency
      assert(m_tables[parent_no].m_op != nullptr);
      key_options->setParent(m_tables[parent_no].m_op);
    }
  }
  return 0;
}  // ndb_pushed_builder_ctx::build_key()

/**
 * Call SPJ API to build a NdbQuery
 */
int ndb_pushed_builder_ctx::build_query() {
  DBUG_TRACE;

  DBUG_PRINT("enter",
             ("Table %d as root is pushable", m_join_root->get_table_no()));

  const uint root_no = m_join_root->get_table_no();
  assert(m_join_scope.contain(root_no));

  if (m_builder == nullptr) {
    m_builder = NdbQueryBuilder::create();
    if (unlikely(m_builder == nullptr)) {
      return HA_ERR_OUT_OF_MEM;
    }
  }

  for (uint tab_no = root_no; tab_no < m_table_count; tab_no++) {
    if (!m_join_scope.contain(tab_no)) continue;

    pushed_table *const table = &m_tables[tab_no];
    const enum_access_type access_type = table->get_access_type();
    ha_ndbcluster *handler =
        down_cast<ha_ndbcluster *>(table->get_table()->file);

    NdbQueryOptions options;
    const NdbQueryOperand *op_key[ndb_pushed_join::MAX_KEY_PART + 1];
    if (table->get_index_no() >= 0) {
      const int error = build_key(table, op_key, &options);
      if (unlikely(error)) return error;
    }

    if (table != m_join_root) {
      assert(table->m_parent != MAX_TABLES);
      const uint parent_no = table->m_parent;
      const pushed_table *const parent = &m_tables[parent_no];

      if (table->isInnerJoined(*parent)) {
        // 'tab_no' is inner joined with its parent
        options.setMatchType(NdbQueryOptions::MatchNonNull);
      }

      if (table->isSemiJoined(*m_join_root)) {
        /**
         * We already concluded in is_pushable_as_child() that the semi-join
         * was pushable, we can't undo that now! However, we do assert some of
         * the restrictions for pushing scans as part of a semi_join:
         */
        if (m_scan_operations.contain(tab_no)) {
          // 'Having no unpushed conditions' is only a restriction for scans:
          assert(!table->has_condition_inbetween(m_join_root));
          assert(!table->has_condition_inbetween(parent));
          assert(!m_has_pending_cond.is_overlapping(table->m_sj_nest));
          // As well as: 'All tables in this sj_nest are pushed'
          assert(m_join_scope.contain(table->m_sj_nest));
        }
        options.setMatchType(NdbQueryOptions::MatchFirst);
      }

      if (table->isAntiJoined(*parent)) {
        // An antijoin is a variant of outer join, returning only a
        // 'firstMatch' or the NULL-extended outer rows
        assert(table->isOuterJoined(*parent));
        const ndb_table_map antijoin_scope(
            get_tables_in_range(tab_no, table->m_last_inner));

        /**
         * From SPJ point of view, antijoin is a normal outer join. So once
         * we have accounted for the special antijoin_null_cond added to such
         * queries, no special handling is required for antijoin's wrt.
         * query correctness.
         *
         * However, as an added optimization, the SPJ API may eliminate the
         * upper-table rows not matching the 'Not exists' requirement, if:
         *  1) The entire (anti-)outer-joined-nest has been pushed down
         *  2) There are no unpushed conditions in the above join-nest.
         * -> or: 'antijoin-nest is completely evaluated by SPJ'
         *
         * Note that this is a pure optimization: Any returned rows supposed
         * to 'Not exist' will simply be eliminated by the mysql server.
         * -> We do join-pushdown of such antijoins even if the check below
         * does not allow us to setMatchType('MatchNullOnly')
         */
        if (m_join_scope.contain(antijoin_scope) &&
            !m_has_pending_cond.is_overlapping(antijoin_scope)) {
          const uint first_upper = table->m_first_upper;
          ndb_table_map upper_nest(
              m_tables[first_upper].get_inner_nest(tab_no));
          upper_nest.intersect(m_join_scope);

          if (upper_nest.contain(parent_no)) {
            /**
             * Antijoin is relative to the *upper_nest*. Thus we can only
             * eliminate found matches if they are relative the upper_nest.
             * Example: '(t1 oj (t2)) where not exists (t3 where t3.x = t1.y)'
             *
             * This nest structure is such that the upper of 'antijoin t3' is
             * t1. Thus we can only do match elimination of such a query when it
             * is built with 't3.parent == t1'.
             */
            options.setMatchType(NdbQueryOptions::MatchNullOnly);
          } else {
            /**
             * Else, subquery condition do not refer upper_nest.
             * Example: '(t1 oj (t2)) where not exists (t3 where t3.x = t2.y)'
             * Due to the nest structure, we still have t3.upper = t1.
             * However, the where condition dependencies will result in:
             * '3.parent == t2'. Specifying antijoin for this query may
             * eliminate matching rows from t2, while t1 rows will still
             * exists (with t2 NULL-extended).
             * However, we can still specify the less restrictive firstMatch
             * for such queries.
             */
            options.setMatchType(NdbQueryOptions::MatchFirst);
          }
        }
      }

      /**
       * Inform SPJ API about the join nest dependencies. Needed in those
       * cases where the are no linkedValues determining which inner_
       * and upper_nest a table is a member of. SPJ API need this info
       * in order to correctly generate NULL extended outer join results.
       *
       * Example: t1 outer join (t2 inner join t3), where t3s join condition
       * does not refer t2. Thus, t3 will likely become an outer joined
       * child of t1 in the QueryTree. From the parent-child POW, t2,t3
       * will look like two separate outer joined tables, like:
       * 't1, outer join (t2), outer join (t3)'.
       *
       * Such queries need to set the join nest dependencies, such that
       * the NdbQuery interface is able to correctly generate NULL extended
       * rows.
       *
       * Below we add these nest dependencies even when not strictly required.
       * The API will just ignore such redundant nest dependencies.
       */
      if (table->isOuterJoined(*parent)) {
        ndb_table_map inner_nest(table->m_inner_nest);
        inner_nest.intersect(m_join_scope);
        if (!inner_nest.is_clear_all()) {
          // Table not first in its join_nest, set firstInner which it
          // depends on
          const uint real_first_inner =
              inner_nest.first_table(table->m_first_inner);
          options.setFirstInnerJoin(m_tables[real_first_inner].m_op);

        } else if (table->m_first_upper >= 0) {
          const uint first_upper = table->m_first_upper;
          ndb_table_map upper_nest(
              m_tables[first_upper].get_inner_nest(tab_no));
          upper_nest.intersect(m_join_scope);
          if (!upper_nest.is_clear_all()) {
            // There is an upper nest which we outer join with
            const uint real_first_upper = upper_nest.first_table(first_upper);
            options.setUpperJoin(m_tables[real_first_upper].m_op);
          }
        }
      }
    }  // if '!m_join_root'

    /**
     * The NdbQuery API need any parameters referred in pushed conditions to
     * be represented as linkedValues. Create these from the List-of-Ndb_param
     * set up when the pushed condition was prepared.
     */
    if (tab_no > root_no) {  // Is a child
      const NdbQueryOperand *parameters[ndb_pushed_join::MAX_LINKED_PARAMS + 1];
      uint cnt = 0;

      const Ndb_param *ndb_param;
      List<const Ndb_param> params = handler->m_cond.get_interpreter_params();
      List_iterator<const Ndb_param> li(params);
      assert(params.size() <= ndb_pushed_join::MAX_LINKED_PARAMS);

      // Iterate over the list of Ndb_params
      while ((ndb_param = li++)) {
        // Get Field and ancestor operation being referred by Ndb_parm
        const Item_field *item_field =
            handler->m_cond.get_param_item(ndb_param);
        const uint referred_table_no = get_table_no(item_field);
        assert(m_join_scope.contain(referred_table_no));
        const NdbQueryOperationDef *const ancestor_op =
            m_tables[referred_table_no].m_op;

        // Convert into array of linkedValue's
        parameters[cnt++] = m_builder->linkedValue(
            ancestor_op, item_field->original_field_name());
      }

      if (cnt > 0) {
        parameters[cnt] = nullptr;
        options.setParameters(parameters);
      }
    }

    const NdbQueryOperationDef *query_op = nullptr;
    if (!m_scan_operations.contain(tab_no)) {
      // Primary key access assumed
      if (access_type == AT_PRIMARY_KEY ||
          access_type == AT_MULTI_PRIMARY_KEY) {
        DBUG_PRINT("info", ("Operation is 'primary-key-lookup'"));
        query_op = m_builder->readTuple(handler->m_table, op_key, &options);
      } else {
        assert(access_type == AT_UNIQUE_KEY ||
               access_type == AT_MULTI_UNIQUE_KEY);
        DBUG_PRINT("info", ("Operation is 'unique-index-lookup'"));
        const NdbDictionary::Index *const index =
            handler->m_index[table->get_index_no()].unique_index;
        assert(index);
        query_op =
            m_builder->readTuple(index, handler->m_table, op_key, &options);
      }
    }  // !m_scan_operation

    /**
     * AT_MULTI_MIXED may have 'ranges' which are pure single key lookups also.
     * In our current implementation these are converted into range access in
     * the pushed MRR implementation. However, the future plan is to build both
     * RANGE and KEY pushable joins for these.
     */
    else if (access_type == AT_ORDERED_INDEX_SCAN ||
             access_type == AT_MULTI_MIXED) {
      assert(table->get_index_no() >= 0);
      assert(handler->m_index[table->get_index_no()].index != nullptr);

      DBUG_PRINT("info", ("Operation is 'equal-range-lookup'"));
      DBUG_PRINT(
          "info",
          ("Creating scanIndex on index id:%d, name:%s", table->get_index_no(),
           handler->m_index[table->get_index_no()].index->getName()));

      const NdbQueryIndexBound bounds(op_key);
      query_op =
          m_builder->scanIndex(handler->m_index[table->get_index_no()].index,
                               handler->m_table, &bounds, &options);
    } else if (access_type == AT_TABLE_SCAN) {
      DBUG_PRINT("info", ("Operation is 'table scan'"));
      query_op = m_builder->scanTable(handler->m_table, &options);
    } else {
      assert(false);
    }

    if (unlikely(!query_op)) return -1;

    table->m_op = query_op;
  }
  return 0;
}  // ndb_pushed_builder_ctx::build_query()
