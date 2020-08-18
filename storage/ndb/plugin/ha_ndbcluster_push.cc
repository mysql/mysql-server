/*
   Copyright (c) 2011, 2020, Oracle and/or its affiliates. All rights reserved.

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
#include "sql/abstract_query_plan.h"
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

/**
 * antijoin_null_cond is inserted by the optimizer when it create the
 * special antijoin-NULL-condition. It serves as a token to uniquely
 * identify such a NULL-condition. Also see similar usage of it
 * when building the iterators in sql_executor.cc
 */
extern const char *antijoin_null_cond;

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

static inline const char *get_referred_field_name(
    const Item_field *field_item) {
  DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->field_name;
}

static const char *get_referred_table_access_name(
    const Item_field *field_item) {
  DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->table->alias;
}

static bool ndbcluster_is_lookup_operation(AQP::enum_access_type accessType) {
  return accessType == AQP::AT_PRIMARY_KEY ||
         accessType == AQP::AT_MULTI_PRIMARY_KEY ||
         accessType == AQP::AT_UNIQUE_KEY;
}

uint ndb_table_access_map::first_table(uint start) const {
  for (uint table_no = start; table_no < length(); table_no++) {
    if (contain(table_no)) return table_no;
  }
  return length();
}

static const Item_func_trig_cond *GetTriggerCondOrNull(const Item *item) {
  if (item->type() == Item::FUNC_ITEM &&
      down_cast<const Item_func *>(item)->functype() ==
          Item_bool_func2::TRIG_COND_FUNC) {
    return down_cast<const Item_func_trig_cond *>(item);
  } else {
    return nullptr;
  }
}

/**
 * Check if the specified 'item' is a antijoin-NULL-condition.
 * This condition is constructed such that all rows being 'matches'
 * are filtered away, and only the non-(anti)matches will pass.
 *
 * Logic inspired by similar code in sql_executor.cc.
 */
static bool is_antijoin_null_cond(const Item *item) {
  const Item_func_trig_cond *trig_cond = GetTriggerCondOrNull(item);
  if (trig_cond != nullptr &&
      trig_cond->get_trig_type() == Item_func_trig_cond::IS_NOT_NULL_COMPL) {
    const Item *inner_cond = trig_cond->arguments()[0];
    const Item_func_trig_cond *inner_trig_cond =
        GetTriggerCondOrNull(inner_cond);
    if (inner_trig_cond != nullptr) {
      const Item *inner_inner_cond = inner_trig_cond->arguments()[0];
      if (inner_inner_cond->item_name.ptr() == antijoin_null_cond) {
        return true;
      }
    }
  }
  return false;
}

uint ndb_table_access_map::last_table(uint start) const {
  uint table_no = start;
  while (true) {
    if (contain(table_no))
      return table_no;
    else if (table_no == 0)
      return length();
    table_no--;
  }
}

ndb_pushed_join::ndb_pushed_join(const ndb_pushed_builder_ctx &builder,
                                 const NdbQueryDef *query_def)
    : m_query_def(query_def),
      m_operation_count(0),
      m_field_count(builder.m_fld_refs) {
  DBUG_ASSERT(query_def != NULL);
  DBUG_ASSERT(builder.m_fld_refs <= MAX_REFERRED_FIELDS);
  ndb_table_access_map searched;
  for (uint tab_no = 0; searched != builder.m_join_scope; tab_no++) {
    const AQP::Table_access *const join_tab =
        builder.m_plan.get_table_access(tab_no);
    if (builder.m_join_scope.contain(tab_no)) {
      DBUG_ASSERT(m_operation_count < MAX_PUSHED_OPERATIONS);
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
                                       const NDB_INDEX_DATA *idx) const {
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
    return false;
  }
  const NdbDictionary::Index *const expected_index = root_operation->getIndex();

  // Check that we still use the same index as when the query was prepared.
  switch (def_type) {
    case NdbQueryOperationDef::PrimaryKeyAccess:
      DBUG_ASSERT(idx != NULL);
      DBUG_ASSERT(idx->unique_index == expected_index);
      break;

    case NdbQueryOperationDef::UniqueIndexAccess:
      DBUG_ASSERT(idx != NULL);
      // DBUG_ASSERT(idx->unique_index == expected_index);
      if (idx->unique_index != expected_index) {
        DBUG_PRINT("info",
                   ("Actual index %s differs from expected index %s."
                    "Therefore, join cannot be pushed.",
                    idx->unique_index->getName(), expected_index->getName()));
        return false;
      }
      break;

    case NdbQueryOperationDef::TableScan:
      DBUG_ASSERT(idx == NULL && expected_index == NULL);
      break;

    case NdbQueryOperationDef::OrderedIndexScan:
      DBUG_ASSERT(idx != NULL);
      // DBUG_ASSERT(idx->index == expected_index);
      if (idx->index != expected_index) {
        DBUG_PRINT("info", ("Actual index %s differs from expected index %s. "
                            "Therefore, join cannot be pushed.",
                            idx->index->getName(), expected_index->getName()));
        return false;
      }
      break;

    default:
      DBUG_ASSERT(false);
      break;
  }

  /**
   * There may be referrences to Field values from tables outside the scope of
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
    // Odly enough, The int24 is *not* stored in an endian sensible format
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
   * There may be referrences to Field values from tables outside the scope of
   * our pushed join: These are expected to be supplied as paramValues()
   * after the keyFieldParams[].
   */
  uint outer_fields = get_field_referrences_count();
  NdbQueryParamValue *extendedParams = NULL;
  if (unlikely(outer_fields > 0)) {
    uint size = sizeof(NdbQueryParamValue) * (paramCnt + outer_fields);
    extendedParams = reinterpret_cast<NdbQueryParamValue *>(my_alloca(size));
    // Copy specified keyFieldParams[] first
    for (uint i = 0; i < paramCnt; i++) {
      new (extendedParams + i) NdbQueryParamValue(keyFieldParams[i]);
    }

    // There may be referrences to Field values from tables outside the scope of
    // our pushed join: These are expected to be supplied as paramValues()
    for (uint i = 0; i < outer_fields; i++) {
      Field *field = m_referred_fields[i];
      DBUG_ASSERT(!field->is_real_null());  // Checked by ::check_if_pushable()
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
      DBUG_ASSERT(field->table->s->db_low_byte_first);
#endif

      new (extendedParams + paramCnt + i) NdbQueryParamValue(raw, false);
    }
    paramValues = extendedParams;
  }

  NdbQuery *query = trans->createQuery(&get_query_def(), paramValues);
  if (unlikely(extendedParams != NULL)) {
    for (uint i = 0; i < paramCnt + outer_fields; i++) {
      extendedParams[i].~NdbQueryParamValue();
    }
  }
  return query;
}

/////////////////////////////////////////

ndb_pushed_builder_ctx::ndb_pushed_builder_ctx(const Thd_ndb *thd_ndb,
                                               AQP::Table_access *root)
    : m_thd_ndb(thd_ndb),
      m_plan(*root->get_join_plan()),
      m_join_root(root),
      m_join_scope(),
      m_const_scope(),
      m_scan_operations(),
      m_has_pending_cond(),
      m_internal_op_count(0),
      m_fld_refs(0),
      m_builder(nullptr) {}

ndb_pushed_builder_ctx::~ndb_pushed_builder_ctx() {
  if (m_builder) {
    m_builder->destroy();
  }
}

const NdbError &ndb_pushed_builder_ctx::getNdbError() const {
  DBUG_ASSERT(m_builder);
  return m_builder->getNdbError();
}

bool ndb_pushed_builder_ctx::maybe_pushable(AQP::Table_access *table,
                                            join_pushability check) {
  DBUG_TRACE;
  TABLE *tab = table->get_table();

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

  if (table->get_table()->file->member_of_pushed_join()) {
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
    case AQP::AT_VOID:
      DBUG_ASSERT(false);
      reason = "UNKNOWN";
      break;

    case AQP::AT_FIXED:
      reason = "optimized away, or const'ified by optimizer";
      break;

    case AQP::AT_UNDECIDED:
      reason = "Access type was not chosen at 'prepare' time";
      break;

    case AQP::AT_OTHER:
      reason = table->get_other_access_reason();
      break;

    default:
      const ha_ndbcluster *handler =
          down_cast<ha_ndbcluster *>(table->get_table()->file);

      if (handler->maybe_pushable_join(reason)) {
        allowed = true;
        pushable = PUSHABLE_AS_CHILD | PUSHABLE_AS_PARENT;
      }
      break;
  }  // switch

  if (reason != nullptr) {
    DBUG_ASSERT(!allowed);
    EXPLAIN_NO_PUSH("Table '%s' is not pushable: %s", tab->alias, reason);
  }
  table->set_table_properties(pushable | PUSHABILITY_KNOWN);
  return allowed;
}  // ndb_pushed_builder_ctx::maybe_pushable

/**
 * Get *internal* table_no of table referred by 'key_item'
 */
uint ndb_pushed_builder_ctx::get_table_no(const Item *key_item) const {
  DBUG_ASSERT(key_item->type() == Item::FIELD_ITEM);
  const uint count = m_plan.get_access_count();
  const table_map bitmap = key_item->used_tables();

  for (uint i = 0; i < count; i++) {
    TABLE *table = m_plan.get_table_access(i)->get_table();
    if (table != nullptr && table->pos_in_table_list != nullptr) {
      const table_map map = table->pos_in_table_list->map();
      if (bitmap & map) {
        DBUG_ASSERT((bitmap & ~map) == 0);  // No other tables in 'bitmap'
        return i;
      }
    }
  }
  return MAX_TABLES;
}

/**
 * Get a ndb_table_access_map containg all tables [first..last]
 */
static ndb_table_access_map get_tables_in_range(uint first, uint last) {
  ndb_table_access_map table_map;
  for (uint i = first; i <= last; i++) {
    table_map.add(i);
  }
  return table_map;
}

/**
 * Translate a table_map from external to internal table enumeration
 */
ndb_table_access_map ndb_pushed_builder_ctx::get_table_map(
    table_map external_map) const {
  ndb_table_access_map internal_map;
  const uint count = m_plan.get_access_count();
  table_map bitmap = (external_map & ~PSEUDO_TABLE_BITS);

  for (uint i = 0; bitmap != 0 && i < count; i++) {
    TABLE *table = m_plan.get_table_access(i)->get_table();
    if (table != nullptr && table->pos_in_table_list != nullptr) {
      const table_map map = table->pos_in_table_list->map();
      if (bitmap & map) {
        internal_map.add(i);
        bitmap &= ~map;  // clear handled table
      }
    }
  }
  DBUG_ASSERT(bitmap == 0);
  return internal_map;
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
 *     the parallelism of the SPJ block whenever considdered optimal.
 *
 *  3) Build the pushed query.
 *
 */
int ndb_pushed_builder_ctx::make_pushed_join(
    const ndb_pushed_join *&pushed_join) {
  DBUG_TRACE;
  pushed_join = NULL;

  if (is_pushable_with_root()) {
    int error;
    error = optimize_query_plan();
    if (unlikely(error)) return error;

    error = build_query();
    if (unlikely(error)) return error;

    const NdbQueryDef *const query_def = m_builder->prepare(m_thd_ndb->ndb);
    if (unlikely(query_def == NULL))
      return -1;  // Get error with ::getNdbError()

    pushed_join = new ndb_pushed_join(*this, query_def);
    if (unlikely(pushed_join == NULL)) return HA_ERR_OUT_OF_MEM;

    DBUG_PRINT("info", ("Created pushed join with %d child operations",
                        pushed_join->get_operation_count() - 1));
  }
  return 0;
}  // ndb_pushed_builder_ctx::make_pushed_join()

/**
 * Find the number SPJ operations needed to execute a given access type.
 * (Unique index lookups are translated to two single table lookups internally.)
 */
uint internal_operation_count(AQP::enum_access_type accessType) {
  switch (accessType) {
    case AQP::AT_PRIMARY_KEY:
    case AQP::AT_ORDERED_INDEX_SCAN:
    case AQP::AT_MULTI_PRIMARY_KEY:
    case AQP::AT_MULTI_MIXED:
    case AQP::AT_TABLE_SCAN:
      return 1;

      // Unique key lookups is mapped to two primary key lookups internally.
    case AQP::AT_UNIQUE_KEY:
    case AQP::AT_MULTI_UNIQUE_KEY:
      return 2;

    default:
      // Other access types are not pushable, so seeing them here is an error.
      DBUG_ASSERT(false);
      return 2;
  }
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

  const uint root_no = m_join_root->get_access_no();
  const AQP::enum_access_type access_type = m_join_root->get_access_type();
  DBUG_ASSERT(access_type != AQP::AT_VOID);

  if (access_type == AQP::AT_MULTI_UNIQUE_KEY) {
    EXPLAIN_NO_PUSH(
        "Table '%s' is not pushable, "
        "access type 'MULTI_UNIQUE_KEY' not implemented",
        m_join_root->get_table()->alias);
    return false;
  }

  if (m_join_root->filesort_before_join()) {
    EXPLAIN_NO_PUSH(
        "Table '%s' is not pushable, "
        "need filesort before joining child tables",
        m_join_root->get_table()->alias);
    return false;
  }

  /**
   * Past this point we know at least root to be pushable as parent
   * operation. Search remaining tables appendable if '::is_pushable_as_child()'
   */
  DBUG_PRINT("info",
             ("Table %d is pushable as root", m_join_root->get_access_no()));
  DBUG_EXECUTE("info", m_join_root->dbug_print(););
  m_fld_refs = 0;
  m_const_scope.set_prefix(root_no);
  m_join_scope.add(root_no);
  m_internal_op_count = internal_operation_count(access_type);

  /**
   * Analyze tables below 'm_join_root' as potential members of a pushed
   * join query starting with root.
   * As part of analyzing the outer join and semi join structure,
   * we use the join- and semi-join-nest structures set up by the optimizer,
   * available trough the Abstract Query Plan (AQP) interface.
   * See further documentation of how the nest structure is
   * represented in m_tables[] in ha_ndbcluster_push.h.
   */
  {
    const uint last_table = m_plan.get_access_count() - 1;
    DBUG_ASSERT(m_plan.get_table_access(0)->get_first_inner() == 0);
    DBUG_ASSERT(m_plan.get_table_access(0)->get_last_inner() == last_table);

    ndb_table_access_map upper_nests;
    ndb_table_access_map inner_nest;
    ndb_table_access_map sj_nest;

    uint first_inner = m_join_root->get_first_inner();
    uint last_inner = m_join_root->get_last_inner();
    int first_upper = m_join_root->get_first_upper();
    if (root_no > first_inner) {
      // m_join_root was not the 'first_inner' in nest;
      // Last_inner / first_upper is only reliable read at first_inner:
      last_inner = m_plan.get_table_access(first_inner)->get_last_inner();
      first_upper = m_plan.get_table_access(first_inner)->get_first_upper();
    }
    int first_sj_inner = m_join_root->get_first_sj_inner();

    m_tables[root_no].m_first_inner = first_inner;
    m_tables[root_no].m_last_inner = last_inner;
    m_tables[root_no].m_first_upper = first_upper;

    for (uint tab_no = root_no; tab_no <= last_table; tab_no++) {
      AQP::Table_access *table = m_plan.get_table_access(tab_no);

      // Set up join-nest for this tab_no
      if (table->get_first_inner() == first_inner) {
        // Still in the join-nest starting at 'first_inner'
        m_tables[tab_no] = m_tables[first_inner];
      } else {
        DBUG_ASSERT(table->get_first_inner() == tab_no);

        // Enter new inner nest
        upper_nests = m_tables[first_inner].m_upper_nests;
        upper_nests.add(inner_nest);
        inner_nest.clear_all();
        first_upper = first_inner;
        first_inner = tab_no;
        last_inner = table->get_last_inner();

        m_tables[first_inner].m_first_inner = first_inner;
        m_tables[first_inner].m_last_inner = last_inner;
        m_tables[first_inner].m_first_upper = first_upper;
        m_tables[first_inner].m_upper_nests = upper_nests;
      }
      m_tables[tab_no].m_inner_nest = inner_nest;
      inner_nest.add(tab_no);

      /**
       * Build similar info for sj_nest. Note that sj_nests are not nested
       * inside other sj_nests. Thus there are no 'upper_sj_nests', and the
       * logic for leaving a sj_nest becomes much simpler.
       * (No un-nesting of nests)
       */
      if (table->get_first_sj_inner() >= 0) {
        if (table->get_first_sj_inner() == first_sj_inner) {
          // still within same sj_nest starting at first_sj_inner.
        } else if (table->get_first_sj_inner() == (int)tab_no) {
          // Start new sj_nest
          first_sj_inner = table->get_first_sj_inner();
          sj_nest.clear_all();
        }
        sj_nest.add(tab_no);
      } else {
        // Not in a sj_nest any longer
        first_sj_inner = -1;
        sj_nest.clear_all();
      }
      m_tables[tab_no].m_sj_nest = sj_nest;

      /**
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
      if (table == m_join_root ||         // root, already known pushable
          is_pushable_as_child(table)) {  // else, check child pushable
        if (!ndbcluster_is_lookup_operation(table->get_access_type())) {
          // A pushable table scan, collect in bitmap for later checks
          m_scan_operations.add(tab_no);
        }
      }

      /**
       * This table can be the last inner table of join-nest(s).
       * That will require additional pushability checks of entire nest
       */
      if (table->get_last_sj_inner() == (int)tab_no) {
        if (first_sj_inner > (int)root_no) {  // Leaving the semi_join nest
          // Phase 2 of pushability check, see big comment above.
          validate_join_nest(sj_nest, first_sj_inner, tab_no, "semi");
        }
        first_sj_inner = -1;
        sj_nest.clear_all();
      }

      /**
       * Note that the same tab_no may unwind several inner join-nests.
       * ... all having the same 'last_inner' (this tab_no)
       */
      while (tab_no == last_inner &&  // End of current join-nest, and
             first_upper >= 0) {      // has an embedding upper nest
        if (first_inner > root_no) {  // Leaving an outer joined nest
          // Phase 2 of pushability check, see big comment above.
          validate_join_nest(inner_nest, first_inner, tab_no, "outer");
        }

        // The upper_nest becomes our new inner_nest when we 'unwind'.
        ndb_table_access_map upper_nest(upper_nests);
        upper_nest.subtract(m_tables[first_upper].m_upper_nests);
        inner_nest = upper_nest;
        upper_nests = m_tables[first_upper].m_upper_nests;
        first_inner = first_upper;

        /**
         * Note that we may 'unwind' to a nest level above where we started as
         * root. m_tables[first_upper] will then not hold the last_inner,
         * first_upper, so we need to read it from the AQP interface instead.
         */
        last_inner = m_plan.get_table_access(first_upper)->get_last_inner();
        first_upper = m_plan.get_table_access(first_upper)->get_first_upper();

      }  // while 'leaving a nest'
    }    // for tab_no [root_no..last_table]
    DBUG_ASSERT(upper_nests.is_clear_all());
  }
  DBUG_ASSERT(m_join_scope.contain(root_no));
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
 * to resolve cases (2) above) where multiple parents are refered.
 * If needed to make a child pushable, we replace parent
 * references with another from the COND_EQUAL sets which make
 * it pushable .
 ****************************************************************/
bool ndb_pushed_builder_ctx::is_pushable_as_child(AQP::Table_access *table) {
  DBUG_TRACE;
  const uint root_no = m_join_root->get_access_no();
  const uint tab_no = table->get_access_no();
  DBUG_ASSERT(tab_no > root_no);

  if (!maybe_pushable(table, PUSHABLE_AS_CHILD)) {
    return false;
  }

  const AQP::enum_access_type root_type = m_join_root->get_access_type();
  const AQP::enum_access_type access_type = table->get_access_type();

  if (!(ndbcluster_is_lookup_operation(access_type) ||
        access_type == AQP::AT_ORDERED_INDEX_SCAN)) {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child, 'type' must be a 'ref' access",
        table->get_table()->alias);
    table->set_table_properties(table->get_table_properties() &
                                ~PUSHABLE_AS_CHILD);
    return false;
  }

  // There is a limitation in not allowing LOOKUP - (index)SCAN operations
  if (access_type == AQP::AT_ORDERED_INDEX_SCAN &&
      ndbcluster_is_lookup_operation(root_type)) {
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

  for (uint i = tab_no; i > root_no; i--) {
    if (m_plan.get_table_access(i)->uses_join_cache()) {
      EXPLAIN_NO_PUSH(
          "Cannot push table '%s' as child of table '%s'. Doing so "
          "would prevent using join buffer for table '%s'.",
          table->get_table()->alias, m_join_root->get_table()->alias,
          m_plan.get_table_access(i)->get_table()->alias);
      return false;
    }
  }

  // Check that we do not exceed the max number of pushable operations.
  const uint internal_ops_needed = internal_operation_count(access_type);
  if (unlikely(m_internal_op_count + internal_ops_needed >
               NDB_SPJ_MAX_TREE_NODES)) {
    EXPLAIN_NO_PUSH(
        "Cannot push table '%s' as child of '%s'. Max number"
        " of pushable tables exceeded.",
        table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
  }
  m_internal_op_count += internal_ops_needed;

  DBUG_PRINT("info",
             ("Table:%d, Checking %d REF keys", tab_no, no_of_key_fields));

  /**
   * Calculate the set of possible parents for each non-const_item KEY_PART
   * from the table. In addition to the parent table directly referred
   * by the KEY_PART, any tables in *same join nest*, available by usage of
   * equality sets are also added as a possible parent.
   *
   * The set of 'key_parents[]' are saved for later usage by ::optimize_*(),
   * which will select the actual parent to be used for each table.
   *
   * We also aggregate the set of 'all_parents' referred by the keys.
   * This is used for checking whether table is pushable.
   */
  ndb_table_access_map all_parents;
  ndb_table_access_map *key_parents =
      new (*THR_MALLOC) ndb_table_access_map[no_of_key_fields];
  m_tables[tab_no].m_key_parents = key_parents;

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
      ndb_table_access_map field_parents;
      if (!is_field_item_pushable(table, key_item, key_part, field_parents)) {
        return false;
      }
      // Save the found key_parents[], aggregate total set of parents referable.
      key_parents[key_part_no] = field_parents;
      all_parents.add(field_parents);
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
   * Try to push condition to 'table'. Whatever we could not push of the
   * condition is a 'server side condition' which the server has to
   * evaluate later. The existence of such conditions may effect the join
   * pushability of tables, so we need to try to push conditions first.
   */
  const Item *pending_cond = table->get_condition();
  if (pending_cond != nullptr &&
      current_thd->optimizer_switch_flag(
          OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)) {
    ha_ndbcluster *handler =
        down_cast<ha_ndbcluster *>(table->get_table()->file);

    const bool other_tbls_ok = false;
    handler->m_cond.prep_cond_push(pending_cond, other_tbls_ok);
    pending_cond = handler->m_cond.m_remainder_cond;
  }
  if (pending_cond != nullptr) {
    /**
     * An anti join will always have an 'antijoin_null_cond' attached.
     * The general rule is that we do not allow any tables having unpushed
     * conditions to be pushed as part of a SPJ operation. However, this
     * special 'antijoin_null_cond' could be ignored, as the same NULL-only
     * filtering is done by the antijoin execution at the server.
     */
    if (!(table->is_antijoin() && is_antijoin_null_cond(pending_cond))) {
      m_has_pending_cond.add(tab_no);
    }
  }
  if (!ndbcluster_is_lookup_operation(table->get_access_type())) {
    // Check extra limitations on when index scan is pushable,
    if (!is_pushable_as_child_scan(table, all_parents)) {
      return false;
    }
  }

  const ndb_table_access_map inner_nest(m_tables[tab_no].m_inner_nest);
  if (!inner_nest.contain(all_parents)) {  // Is not a plain inner-join

    ndb_table_access_map depend_parents;

    /**
     * Some key_parents[] could have dependencies outside of embedding_nests.
     * Calculate the actual nest dependencies and check join pushability.
     */
    for (uint i = 0; i < no_of_key_fields; i++) {
      if (!key_parents[i].is_clear_all()) {  // Key refers a parent field
#ifndef DBUG_OFF
        // Verify requirement that all field_parents are from within same nest
        {
          const uint last = key_parents[i].last_table(tab_no);
          ndb_table_access_map nest(m_tables[last].m_inner_nest);
          nest.add(last);
          DBUG_ASSERT(nest.contain(key_parents[i]));
        }
#endif
        const uint first = key_parents[i].first_table();
        depend_parents.add(first);
      }
    }

    /**
     * In the (unlikely) case of parent references to tables not
     * in our embedding join nests at all, we have to make sure that we do
     * not cause extra dependencies to be added between the referred join nests.
     */
    const ndb_table_access_map embedding_nests(
        m_tables[tab_no].embedding_nests());
    if (unlikely(!embedding_nests.contain(depend_parents))) {
      if (!is_outer_nests_referable(table, depend_parents)) {
        return false;
      }
    }

    /**
     * Calculate contribution to the 'nest dependency', which is the ancestor
     * dependencies to tables not being part of this inner_nest themself.
     * These ancestor dependencies are set as the required 'm_ancestors'
     * on the 'first_inner' table in each nest, and later used to enforce
     * ::optimize_query_plan() to use these tables as (grand-)parents
     */
    const uint first_inner = m_tables[tab_no].m_first_inner;
    // Only interested in the upper-nest-level dependencies:
    depend_parents.intersect(m_tables[first_inner].embedding_nests());

    // Can these outer parent dependencies co-exists with existing
    // ancestor dependencies?
    if (!depend_parents.is_clear_all() &&
        !m_tables[first_inner].m_ancestors.is_clear_all()) {
      ndb_table_access_map nest_dependencies(depend_parents);
      nest_dependencies.add(m_tables[first_inner].m_ancestors);

      uint ancestor_no = first_inner;
      while (!embedding_nests.contain(nest_dependencies)) {
        ancestor_no = nest_dependencies.last_table(ancestor_no - 1);
        nest_dependencies.clear_bit(ancestor_no);

        // If remaining dependencies are unavailable from parent, we can't push
        if (!m_tables[ancestor_no].embedding_nests().contain(
                nest_dependencies)) {
          const AQP::Table_access *const parent =
              m_plan.get_table_access(ancestor_no);
          EXPLAIN_NO_PUSH(
              "Can't push table '%s' as child of '%s', "
              "as it would make the parent table '%s' "
              "depend on table(s) outside of its join-nest",
              table->get_table()->alias, m_join_root->get_table()->alias,
              parent->get_table()->alias);
          return false;
        }
      }
    }
    m_tables[first_inner].m_ancestors.add(depend_parents);
    DBUG_ASSERT(!m_tables[first_inner].m_ancestors.contain(first_inner));
  }

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
 * tables preceeding it in the query plan. ::validate_join_nest() will later
 * do similar checks when we have completed a nest level. The later check
 * would be sufficient, however we prefer to 'fail fast'.
 *
 ****************************************************************/
bool ndb_pushed_builder_ctx::is_pushable_within_nest(
    const AQP::Table_access *table, ndb_table_access_map nest,
    const char *nest_type) {
  DBUG_TRACE;
  DBUG_ASSERT(!ndbcluster_is_lookup_operation(table->get_access_type()));
  const uint tab_no = table->get_access_no();

  // Logic below assume that 'this' table is not part of the 'nest'.
  nest.clear_bit(tab_no);

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
    // Other (lookup tables) withing nest has unpushed condition
    ndb_table_access_map pending_conditions(m_has_pending_cond);
    pending_conditions.intersect(nest);
    // Report the closest violating table, may be multiple.
    const uint violating = pending_conditions.last_table(tab_no);
    EXPLAIN_NO_PUSH(
        "Can't push %s joined table '%s' as child of '%s', "
        "condition on its dependant table '%s' is not pushed down",
        nest_type, table->get_table()->alias, m_join_root->get_table()->alias,
        m_plan.get_table_access(violating)->get_table()->alias);
    return false;
  }

  /**
   * 2) Check if outer- or semi-joined table depends on 'unpushed tables'
   */
  if (unlikely(!m_join_scope.contain(nest))) {  // 2b,2c
    ndb_table_access_map unpushed_tables(nest);
    unpushed_tables.subtract(m_join_scope);
    // Report the closest unpushed table, may be multiple.
    const uint violating = unpushed_tables.last_table(tab_no);
    EXPLAIN_NO_PUSH(
        "Can't push %s joined table '%s' as child of '%s', "
        "table '%s' in its dependant join-nest(s) is not part of the "
        "pushed join",
        nest_type, table->get_table()->alias, m_join_root->get_table()->alias,
        m_plan.get_table_access(violating)->get_table()->alias);
    return false;
  }
  return true;
}

bool ndb_pushed_builder_ctx::is_pushable_as_child_scan(
    const AQP::Table_access *table, const ndb_table_access_map all_parents) {
  DBUG_TRACE;
  DBUG_ASSERT(!ndbcluster_is_lookup_operation(table->get_access_type()));

  const uint root_no = m_join_root->get_access_no();
  const uint tab_no = table->get_access_no();

  if (m_tables[tab_no].isOuterJoined(m_tables[root_no])) {
    /**
     * Is an outer join relative to root. Even if tab_no is inner_joined with
     * another parent than 'root', any restrictions on scan operations still
     * apply.
     */

    /**
     * Online upgrade, check if we are connected to a 'ndb' allowing us to push
     * outer joined scan operation (ver >= 8.0.20), Else we reject pushing.
     */
    if (unlikely(!NdbQueryBuilder::outerJoinedScanSupported(m_thd_ndb->ndb))) {
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
    ndb_table_access_map outer_join_nests(m_tables[tab_no].embedding_nests());
    outer_join_nests.subtract(full_inner_nest(root_no, tab_no));

    const char *join_type = table->is_antijoin() ? "anti" : "outer";
    if (!is_pushable_within_nest(table, outer_join_nests, join_type)) {
      return false;
    }

    /**
     * 3) Check if any tables outside of the embedding nest are referred.
     */
    const ndb_table_access_map embedding_nests(
        m_tables[tab_no].embedding_nests());
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
      ndb_table_access_map unpushed_tables;
      unpushed_tables.set_prefix(tab_no);
      unpushed_tables.subtract(m_const_scope);
      unpushed_tables.subtract(m_join_scope);

      /**
       * Note that the check below is a bit too strict, we check:
       *  'Are there any unpushed tables outside of our embedding nests',
       *  instead of 'Do we refer tables from nests outside embedding nests,
       *  having unpushed tables'. As we alread know 'all_parents' are not
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

  if (table->is_sj_firstmatch() &&
      NdbQueryBuilder::outerJoinedScanSupported(m_thd_ndb->ndb)) {
    // 'table' is part of a semi-join
    // (We support semi-join only if firstMatch strategy is used)
    DBUG_ASSERT(m_tables[tab_no].m_sj_nest.contain(table->get_access_no()));

    if (!is_pushable_within_nest(table, m_tables[tab_no].m_sj_nest, "semi")) {
      return false;
    }
    if (table->get_first_sj_inner() == (int)tab_no) {
      /**
       * In order to do correct firstmatch duplicate elimination in
       * SPJ, we need to ensure that the table to eliminate duplicates
       * from is the parent of the firstmast-sj-nest -> enforce it
       * as a mandatory ancestor of the sj-nest.
       */
      const int firstmatch_return = table->get_firstmatch_return();
      if (!all_parents.contain(firstmatch_return)) {
        EXPLAIN_NO_PUSH(
            "Can't push table '%s' as child of '%s', "
            "the FirstMatch-return '%s' can not be made the parent of sj-nest",
            table->get_table()->alias, m_join_root->get_table()->alias,
            m_plan.get_table_access(firstmatch_return)->get_table()->alias);
        return false;
      }
      m_tables[tab_no].m_ancestors.add(firstmatch_return);
    }
  } else if (!m_tables[tab_no].m_sj_nest.is_clear_all()) {
    if (!m_tables[tab_no].m_sj_nest.contain(m_join_scope)) {
      // Semi-joined relative to some other tables in join_scope
      EXPLAIN_NO_PUSH(
          "Can't push table '%s' as child of '%s', "
          "semi join of scan-child not implemented",
          table->get_table()->alias, m_join_root->get_table()->alias);
      return false;
    }
  } else if (!m_tables[root_no].m_sj_nest.is_clear_all()) {
    // Root is part of a semi join, table is not
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "not members of same semi join 'nest'",
        table->get_table()->alias, m_join_root->get_table()->alias);
    return false;
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
 * is_outer_nests_referable()
 *
 * In the (unlikely) case of parent references to tables not
 * in our embedding join nests, we have to make sure that we do
 * not cause extra dependencies to be added between the join nests.
 * (Which would have changed the join semantic specified in query)
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
 *    key_parents[].
 *
 * 2. Check the 'dependencies' set, starting at the last (the
 *    table closest to this table). Check that it either already
 *    exists a dependency between each such table and the remaining
 *    dependant tables, or that we are allowed to add the required
 *    dependencies.
 ***************************************************************/
bool ndb_pushed_builder_ctx::is_outer_nests_referable(
    const AQP::Table_access *table, const ndb_table_access_map depend_parents) {
  DBUG_TRACE;

  const uint tab_no = table->get_access_no();
  const uint first_inner = m_tables[tab_no].m_first_inner;
  // Check that embedding nests does not already contain dependent parents
  DBUG_ASSERT(!m_tables[tab_no].embedding_nests().contain(depend_parents));

  /**
   * Include nest-level ancestor dependencies already enforced.
   */
  ndb_table_access_map dependencies(depend_parents);
  dependencies.add(m_tables[first_inner].m_ancestors);

  /**
   * Check that all parents we depend on are available from within the
   * embedding nests. This include upper_nests previously extended
   * with previous references to tables not in the direct line of
   * upper nests. Which then become a part of later embedded_nests being
   * referrable.
   */
  {
    const uint parent_no = dependencies.last_table(tab_no - 1);
    dependencies.clear_bit(parent_no);

    // If remaining dependencies are unavailable from parent, we can't push
    if (!m_tables[parent_no].embedding_nests().contain(dependencies)) {
      const AQP::Table_access *const parent =
          m_plan.get_table_access(parent_no);
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
     * part of the set of later referrable upper_nests.
     */
    if (parent_no < first_inner) {
      // referred nest is not embedded within current inner_nest
      DBUG_ASSERT(m_tables[parent_no].m_last_inner < tab_no);

      /**
       * Referring the outer-joined parent, introduce the requirement
       * that all our upper_nest table either has to be in the same
       * inner_nest as the parent, or be in the parents upper_nest.
       * Rebuild our upper_nests to reflect this.
       */
      ndb_table_access_map new_upper_nests(m_tables[parent_no].m_upper_nests);
      new_upper_nests.add(full_inner_nest(parent_no, tab_no));
      m_tables[first_inner].m_upper_nests = new_upper_nests;
      m_tables[tab_no].m_upper_nests = new_upper_nests;
    }
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
 *  3) This nest, or some nests embedded within it, has a 'FOUND_MATCH' trigger,
 *     condition covering tables in this nest. (Which effectively means the
 *     condition act as a filter condition on this nest, as in 2) )
 *
 * The above restriction are similar to the ones checked for outer joined
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
void ndb_pushed_builder_ctx::validate_join_nest(ndb_table_access_map inner_nest,
                                                const uint first_inner,
                                                const uint last_inner,
                                                const char *nest_type) {
  DBUG_TRACE;
  if (first_inner <= m_join_root->get_access_no()) return;

  // This nest, or nests embedded within it, has scan operations?
  const bool nest_has_scans =
      (m_scan_operations.first_table(first_inner) < m_plan.get_access_count());
  if (nest_has_scans) {
    ndb_table_access_map filter_cond;

    /**
     * Check conditions inside nest(s) for possible FOUND_MATCH-triggers.
     * These are effectively evaluated 'higher up' in the nest structure
     * when we have found a join-match, or created a null-extension
     * for all 'used_tables()' in the trigger condition.
     * So we collect the aggregated map of tables possibly affected by
     * these MATCH-filters in 'filter_cond'
     *
     * Example: select straight_join *
     *          from
     *            t1 left join
     *              (t1 as t2 join t1 as t3 on t3.a = t2.b)
     *            on t2.a = t1.b
     *          where (t2.c > t1.c or t1.c < 0);
     *
     * or: 't1 oj (t2,t3) where t2.c > t1.c or t1.c < 0'
     *
     * The where condition refers columns from the outer joined nest (t2,t3)
     * which are possibly NULL extended. Thus, the where cond is encapsulated in
     * a triggered-FOUND_MATCH(t2,t3), effectively forcing the cond. to be
     * evaluated only when we have a non-NULL extended match for t2,t3.
     * For some (legacy?) reason the optimizer will attach the trigger condition
     * to table t2 in the query plan 't1,t2,t3', as all referred tables(t1,t2)
     * are available at this point.
     * However, this ignores the encapsulating FOUND_MATCH(t2,t3) trigger,
     * which require the condition to also have a matching t3 row. The
     * WalkItem below will identify such triggers and calculate the real table
     * coverage of them.
     *
     * Note that 'explain format=tree' will represent such filters in a more
     * sensible way: (We don't use the Iterators here (yet) though)
     *
     * -> Filter: ((t2.c > t1.c) or (t1.c < 0))
     *   -> Nested loop left join
     *     -> Table scan on t1
     *     -> Nested loop inner join
     *       -> Index lookup on t2 using PRIMARY (a=t1.b),
     *       -> Index lookup on t3 using PRIMARY (a=t2.b)
     *
     * The Iterators place the filter on 'top of' the t1..t3 evaluation.
     * The FOUND_MATCH(t2,t3) has also been eliminated, as we know there is
     * a (t2,t3) match at this point of execution.
     */
    for (uint tab_no = first_inner; tab_no <= last_inner; tab_no++) {
      AQP::Table_access *table = m_plan.get_table_access(tab_no);
      const Item *cond = table->get_condition();
      if (cond != nullptr) {
        // Condition could possibly be a 'antijoin_null_cond', in which case
        // the pending_cond flag has been cleared, it should then be ignored.
        if (m_join_scope.contain(tab_no) && !m_has_pending_cond.contain(tab_no))
          continue;

        struct {
          table_map nest_scope;   // Aggregated 'inner_tables' scope of triggers
          table_map found_match;  // FOUND_MATCH-trigger scope
        } trig_cond = {0, 0};

        // Check 'cond' for match trigger / filters
        WalkItem(const_cast<Item *>(cond), enum_walk::PREFIX,
                 [&trig_cond](Item *item) {
                   const Item_func_trig_cond *func_trig =
                       GetTriggerCondOrNull(item);
                   if (func_trig != nullptr) {
                     /**
                      * The FOUND_MATCH-trigger may be encapsulated inside
                      * multiple IS_NOT_NULL_COMPL-triggers, which defines
                      * the scope of the triggers. Aggregate these
                      * 'inner_tables' scopes.
                      */
                     trig_cond.nest_scope |= func_trig->get_inner_tables();

                     if (func_trig->get_trig_type() ==
                         Item_func_trig_cond::FOUND_MATCH) {
                       // The FOUND_MATCH-trigger is evaluated on top of
                       // the collected trigger nest_scope.
                       trig_cond.found_match |= trig_cond.nest_scope;
                       return true;  // break out of this cond-branch
                     }
                   }
                   return false;  // continue WalkItem
                 });              // End WalkItem' and lambda func

        if (trig_cond.found_match != 0) {
          const ndb_table_access_map map = get_table_map(trig_cond.found_match);

          /**
           * Only FOUND_MATCH-triggers partly overlapping join_scope will
           * restrict push. (Else it is completely evaluated either before
           * or after the pushed_join, thus does not affect it.
           */
          if (map.is_overlapping(m_join_scope) && !map.contain(m_join_scope)) {
            filter_cond.add(map);
          }
        }
      }
    }

    // Check each of the 3 reject reasons from the topmost comment
    const bool nest_has_unpushed = !m_join_scope.contain(inner_nest);
    const bool nest_has_filter_cond = inner_nest.is_overlapping(filter_cond);
    const bool nest_has_pending_cond =
        inner_nest.is_overlapping(m_has_pending_cond);

    if (nest_has_pending_cond || nest_has_unpushed || nest_has_filter_cond) {
      /**
       * Check all pushed scan operations in this nest, and nests embedded
       * within it. Note that it is the rows from scans in the upper nest
       * which may be repeated, creating false NULL extended rows from scans
       * in inner_nests.
       */
      for (uint tab_no = m_scan_operations.first_table(first_inner);
           tab_no <= last_inner;
           tab_no = m_scan_operations.first_table(tab_no + 1)) {
        DBUG_ASSERT(m_join_scope.contain(tab_no));
        const AQP::Table_access *const table = m_plan.get_table_access(tab_no);

        /**
         * Could have checked all 3 reject conditions at once, but would
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
        } else if (nest_has_filter_cond) {
          EXPLAIN_NO_PUSH(
              "Can't push %s joined table '%s' as child of '%s', "
              "join-nest containing the table has a FILTER conditions",
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
 * be part of the pushed join without the romoved table(s).
 **********************************************************************/
void ndb_pushed_builder_ctx::remove_pushable(const AQP::Table_access *table) {
  DBUG_TRACE;

  const uint me = table->get_access_no();
  DBUG_ASSERT(m_join_scope.contain(me));
  m_join_scope.clear_bit(me);

  // Cascade remove of tables depending on 'me'
  for (uint tab_no = me + 1; tab_no < m_plan.get_access_count(); tab_no++) {
    if (m_join_scope.contain(tab_no)) {
      table = m_plan.get_table_access(tab_no);
      ndb_table_access_map *key_parents = m_tables[tab_no].m_key_parents;

      for (uint i = 0; i < table->get_no_of_key_fields(); i++) {
        if (!key_parents[i].is_clear_all()) {
          // Was referring some parent field(s) (not const, or params)
          // Remove parent referrences not in join_scope any more
          key_parents[i].intersect(m_join_scope);

          if (key_parents[i].is_clear_all()) {
            // All preceeding parent tables removed from join_scope.
            m_join_scope.clear_bit(tab_no);  // Cascade remove of this table
            break;
          }
        }
      }
    }
    m_tables[tab_no].m_ancestors.intersect(m_join_scope);
  }
  // Remove 'pending_cond' and 'scan_operations' not pushed any longer
  m_has_pending_cond.intersect(m_join_scope);
  m_scan_operations.intersect(m_join_scope);
}  // ndb_pushed_builder_ctx::remove_pushable

/*********************
 * This method examines a key item (could be part of a lookup key or a scan
 * bound) for a table access operation and calculates the set of possible
 * parents. (These are possible parent table access operations in the query
 * tree that will be pushed to the ndb.)
 *
 * @param[in] table The table access operation to which the key item belongs.
 * @param[in] key_item The key_item to examine
 * @param[in] key_part Metatdata about the key item.
 * @param[out] field_parents The set of possible parents for 'key_item'
 * ('join_root' if keys are constant).
 * @return True if at least one possible parent was found. (False means that
 * operation cannot be pushed).
 */
bool ndb_pushed_builder_ctx::is_field_item_pushable(
    AQP::Table_access *table, const Item *key_item,
    const KEY_PART_INFO *key_part, ndb_table_access_map &field_parents) {
  DBUG_TRACE;
  const uint tab_no = table->get_access_no();
  DBUG_ASSERT(key_item->type() == Item::FIELD_ITEM);

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
   * Below this point 'key_item_field' is a candidate for refering a parent
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
  //    usable by substituting existing 'key_item_field'
  //
  Item_equal *item_equal = table->get_item_equal(key_item_field);
  if (item_equal) {
    AQP::Equal_set_iterator equal_iter(*item_equal);
    const Item_field *substitute_field;
    while ((substitute_field = equal_iter.next()) != NULL) {
      if (substitute_field != key_item_field) {
        const uint substitute_table_no = get_table_no(substitute_field);
        if (m_join_scope.contain(substitute_table_no)) {
          DBUG_PRINT("info",
                     (" join_items[%d] %s.%s can be replaced with %s.%s",
                      (int)(key_item - table->get_key_field(0)),
                      get_referred_table_access_name(key_item_field),
                      get_referred_field_name(key_item_field),
                      get_referred_table_access_name(substitute_field),
                      get_referred_field_name(substitute_field)));

          field_parents.add(substitute_table_no);
        }
      }
    }  // while(substitute_field != NULL)
  }
  if (!field_parents.is_clear_all()) {
    return true;
  }

  if (m_const_scope.contain(referred_table_no)) {
    // This key item is const. and did not cause the set of possible parents
    // to be recalculated. Reuse what we had before this key item.
    DBUG_ASSERT(field_parents.is_clear_all());

    /**
     * Field referrence is a 'paramValue' to a column value evaluated
     * prior to the root of this pushed join candidate. Some restrictions
     * applies to when a field reference is allowed in a pushed join:
     */
    if (ndbcluster_is_lookup_operation(m_join_root->get_access_type())) {
      /**
       * EQRefIterator may optimize away key reads if the key
       * for a requested row is the same as the previous.
       * Thus, iff this is the root of a pushed lookup join
       * we do not want it to contain childs with references
       * to columns 'outside' the the pushed joins, as these
       * may still change between calls to
       * EQRefIterator::Read() independent of the root key
       * itself being the same.
       */
      EXPLAIN_NO_PUSH(
          "Cannot push table '%s' as child of '%s', since "
          "it referes to column '%s.%s' prior to a "
          "potential 'const' root.",
          table->get_table()->alias, m_join_root->get_table()->alias,
          get_referred_table_access_name(key_item_field),
          get_referred_field_name(key_item_field));
      return false;
    } else {
      /**
       * Scan queries cannot be pushed if the pushed query may refer column
       * values (paramValues) from rows stored in a join cache.
       */
      const TABLE *const referred_tab = key_item_field->field->table;
      uint access_no = tab_no;
      do {
        if (m_plan.get_table_access(access_no)->uses_join_cache()) {
          EXPLAIN_NO_PUSH(
              "Cannot push table '%s' as child of '%s', since "
              "it referes to column '%s.%s' which will be stored "
              "in a join buffer.",
              table->get_table()->alias, m_join_root->get_table()->alias,
              get_referred_table_access_name(key_item_field),
              get_referred_field_name(key_item_field));
          return false;
        }
        DBUG_ASSERT(access_no > 0);
        access_no--;
      } while (m_plan.get_table_access(access_no)->get_table() != referred_tab);

    }  // if (!ndbcluster_is_lookup_operation(root_type)
    return true;
  } else {
    EXPLAIN_NO_PUSH(
        "Can't push table '%s' as child of '%s', "
        "column '%s.%s' is outside scope of pushable join",
        table->get_table()->alias, m_join_root->get_table()->alias,
        get_referred_table_access_name(key_item_field),
        get_referred_field_name(key_item_field));
    return false;
  }
}  // ndb_pushed_builder_ctx::is_field_item_pushable()

bool ndb_pushed_builder_ctx::is_const_item_pushable(
    const Item *key_item, const KEY_PART_INFO *key_part) {
  DBUG_TRACE;
  DBUG_ASSERT(key_item->const_for_execution());

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
 * The m_parent is choosen based on the available m_key_parents[]
 * which were set up by ::is_pushable_as_child(), and possibly later
 * modified (reduced) by ::validate_join_nest().
 *
 * When multiple parent candidates are available, we choose the one
 * closest to the root, which will result in the most 'bushy' tree
 * structure and the highest possible parallelism. Note that SPJ block
 * will build its own execution plan (based on whats being set up here)
 * which possible sequentialize the execution of these parallel branches.
 * (See WL#11164)
 */
int ndb_pushed_builder_ctx::optimize_query_plan() {
  DBUG_TRACE;
  const uint root_no = m_join_root->get_access_no();
  const uint last_table = m_plan.get_access_count() - 1;

  // Find an optimal m_parent to be used when joining the tables
  for (uint tab_no = last_table; tab_no > root_no; tab_no--) {
    if (!m_join_scope.contain(tab_no)) continue;
    pushed_tables &table = m_tables[tab_no];

    /**
     * Calculate the set of possible parents for the table, where:
     *  - 'common' are those we may refer (possibly through the EQ-sets)
     *     such that all FIELD_ITEMs are from the same parent.
     *  - 'extended' are those parents refered from some of the
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
    ndb_table_access_map *key_parents = table.m_key_parents;
    ndb_table_access_map common_parents(m_join_scope);
    ndb_table_access_map extend_parents;
    ndb_table_access_map depend_parents;

    for (uint i = 0;
         i < m_plan.get_table_access(tab_no)->get_no_of_key_fields(); i++) {
      DBUG_ASSERT(m_join_scope.contain(key_parents[i]));
      if (!key_parents[i].is_clear_all()) {  // Key refers a parent field
        /**
         * Calculate 'common_parents' as the set of possible 'field_parents'
         * available from all 'key_part'.
         */
        common_parents.intersect(key_parents[i]);

        /**
         * 'Extended' parents are refered from some 'FIELD_ITEM', and contain
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
     * Previous childs might already have enforced some ancestors to be
     * available through this table due to some ancestors being referred by
     * them, add these.
     */
    depend_parents.add(table.m_ancestors);

    /**
     * Same goes for nest-level dependencies: The 'first' in each nest
     * may enforce ancestor dependencies on the members of the nest.
     * If this table is the 'first' itself, it is embedded within the
     * nest controlled by the 'first_upper'.
     */
    if (table.m_first_inner < tab_no)
      depend_parents.add(m_tables[table.m_first_inner].m_ancestors);
    else if (table.m_first_upper > 0)
      depend_parents.add(m_tables[table.m_first_upper].m_ancestors);

    /**
     * All 'depend_parents' has to be fulfilled, starting from the 'last',
     * closest to this tab_no. The 'depend_parents' not directly referred
     * as a parent from this table, will be fulfilled by adding them as required
     * ancestors of the choosen parent, see below.
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
    DBUG_ASSERT(parent_no < tab_no);
    table.m_parent = parent_no;

    /**
     * Any remaining ancestor dependencies for this table has to be
     * added to the selected parent in order to be taken into account
     * for parent calculation for its ancestors.
     */
    depend_parents.clear_bit(parent_no);
    m_tables[parent_no].m_ancestors.add(depend_parents);

    /**
     * Similar for nest-level dependencies: Any dependencies to tables outside
     * of this inner nest are enforces as mandatory nest-ancestor dependencies.
     */
    depend_parents.subtract(table.m_inner_nest);
    m_tables[table.m_first_inner].m_ancestors.add(depend_parents);
  }

  /* Collect the full set of ancestors available through the selected 'm_parent'
   */
  for (uint tab_no = root_no + 1; tab_no <= last_table; tab_no++) {
    if (m_join_scope.contain(tab_no)) {
      pushed_tables &table = m_tables[tab_no];
      const uint parent_no = table.m_parent;
      table.m_ancestors = m_tables[parent_no].m_ancestors;
      table.m_ancestors.add(parent_no);
    }
  }
  return 0;
}  // ndb_pushed_builder_ctx::optimize_query_plan

void ndb_pushed_builder_ctx::collect_key_refs(const AQP::Table_access *table,
                                              const Item *key_refs[]) const {
  DBUG_TRACE;

  const uint tab_no = table->get_access_no();
  const uint parent_no = m_tables[tab_no].m_parent;
  const ndb_table_access_map ancestors = m_tables[tab_no].m_ancestors;

  DBUG_ASSERT(m_join_scope.contain(ancestors));
  DBUG_ASSERT(ancestors.contain(parent_no));

  /**
   * If there are any key_fields with 'current_parents' different from
   * our selected 'parent', we have to find substitutes for
   * those key_fields within the equality set.
   **/
  for (uint key_part_no = 0; key_part_no < table->get_no_of_key_fields();
       key_part_no++) {
    const Item *const key_item = table->get_key_field(key_part_no);
    key_refs[key_part_no] = key_item;

    DBUG_ASSERT(key_item->const_for_execution() ||
                key_item->type() == Item::FIELD_ITEM);

    if (key_item->type() == Item::FIELD_ITEM) {
      const Item_field *join_item = static_cast<const Item_field *>(key_item);
      uint referred_table_no = get_table_no(join_item);
      Item_equal *item_equal;

      if (referred_table_no != parent_no &&
          (item_equal = table->get_item_equal(join_item)) != NULL) {
        AQP::Equal_set_iterator iter(*item_equal);
        const Item_field *substitute_field;
        while ((substitute_field = iter.next()) != NULL) {
          ///////////////////////////////////////////////////////////
          // Prefer to replace join_item with ref. to selected parent.
          //
          const uint substitute_table_no = get_table_no(substitute_field);
          if (substitute_table_no == parent_no) {
            DBUG_PRINT("info",
                       (" Replacing key_refs[%d] %s.%s with %s.%s (parent)",
                        key_part_no, get_referred_table_access_name(join_item),
                        get_referred_field_name(join_item),
                        get_referred_table_access_name(substitute_field),
                        get_referred_field_name(substitute_field)));

            referred_table_no = substitute_table_no;
            key_refs[key_part_no] = join_item = substitute_field;
            break;
          } else if (ancestors.contain(substitute_table_no)) {
            DBUG_ASSERT(substitute_table_no <= parent_no);

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
                   get_referred_table_access_name(substitute_field),
                   get_referred_field_name(substitute_field)));

              referred_table_no = substitute_table_no;
              key_refs[key_part_no] = join_item = substitute_field;
            }
          }
        }  // while (substitute...

        DBUG_ASSERT(referred_table_no == parent_no ||
                    ancestors.contain(referred_table_no) ||
                    m_const_scope.contain(
                        referred_table_no));  // Is a 'const' paramValue
      }
    }  // Item::FIELD_ITEM
  }

  key_refs[table->get_no_of_key_fields()] = NULL;
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
int ndb_pushed_builder_ctx::build_key(const AQP::Table_access *table,
                                      const NdbQueryOperand *op_key[],
                                      NdbQueryOptions *key_options) {
  DBUG_TRACE;
  DBUG_ASSERT(m_join_scope.contain(table->get_access_no()));

  const KEY *const key = &table->get_table()->key_info[table->get_index_no()];
  op_key[0] = NULL;

  if (table == m_join_root) {
    if (ndbcluster_is_lookup_operation(table->get_access_type())) {
      for (uint i = 0; i < key->user_defined_key_parts; i++) {
        op_key[i] = m_builder->paramValue();
        if (unlikely(op_key[i] == NULL)) {
          return -1;
        }
      }
      op_key[key->user_defined_key_parts] = NULL;
    }
  } else {
    const uint key_fields = table->get_no_of_key_fields();
    DBUG_ASSERT(key_fields > 0 && key_fields <= key->user_defined_key_parts);
    uint map[ndb_pushed_join::MAX_LINKED_KEYS + 1];

    if (ndbcluster_is_lookup_operation(table->get_access_type())) {
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

    ndb_table_access_map referred_parents;
    const KEY_PART_INFO *key_part = key->key_part;
    for (uint i = 0; i < key_fields; i++, key_part++) {
      const Item *const item = join_items[i];
      op_key[map[i]] = NULL;

      if (item->const_for_execution()) {
        /**
         * Propagate Items constant value to Field containing the value of this
         * key_part:
         */
        Field *const field = key_part->field;
        DBUG_ASSERT(!field->is_real_null());
        const uchar *const ptr =
            (field->real_type() == MYSQL_TYPE_VARCHAR)
                ? field->field_ptr() + field->get_length_bytes()
                : field->field_ptr();

        op_key[map[i]] = m_builder->constValue(ptr, field->data_length());
      } else {
        DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
        const Item_field *const field_item =
            static_cast<const Item_field *>(item);
        const uint referred_table_no = get_table_no(field_item);
        referred_parents.add(referred_table_no);

        if (m_join_scope.contain(referred_table_no)) {
          // Locate the parent operation for this 'join_items[]'.
          // May refer any of the preceding parent tables
          const NdbQueryOperationDef *const parent_op =
              m_tables[referred_table_no].m_op;
          DBUG_ASSERT(parent_op != NULL);

          // TODO use field_index ??
          op_key[map[i]] =
              m_builder->linkedValue(parent_op, field_item->field_name);
        } else {
          DBUG_ASSERT(m_const_scope.contain(referred_table_no));
          // Outside scope of join plan, Handle as parameter as its value
          // will be known when we are ready to execute this query.
          if (unlikely(m_fld_refs >= ndb_pushed_join::MAX_REFERRED_FIELDS)) {
            DBUG_PRINT("info", ("Too many Field refs ( >= MAX_REFERRED_FIELDS) "
                                "encountered"));
            return -1;  // TODO, handle gracefull -> continue?
          }
          m_referred_fields[m_fld_refs++] = field_item->field;
          op_key[map[i]] = m_builder->paramValue();
        }
      }

      if (unlikely(op_key[map[i]] == NULL)) {
        return -1;
      }
    }
    op_key[key_fields] = NULL;

    // Might have to explicit set the designated parent.
    const uint tab_no = table->get_access_no();
    const uint parent_no = m_tables[tab_no].m_parent;
    if (!referred_parents.contain(parent_no)) {
      // Add the parent as a new dependency
      DBUG_ASSERT(m_tables[parent_no].m_op != NULL);
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
             ("Table %d as root is pushable", m_join_root->get_access_no()));
  DBUG_EXECUTE("info", m_join_root->dbug_print(););

  const uint root_no = m_join_root->get_access_no();
  DBUG_ASSERT(m_join_scope.contain(root_no));

  if (m_builder == NULL) {
    m_builder = NdbQueryBuilder::create();
    if (unlikely(m_builder == NULL)) {
      return HA_ERR_OUT_OF_MEM;
    }
  }

  for (uint tab_no = root_no; tab_no < m_plan.get_access_count(); tab_no++) {
    if (!m_join_scope.contain(tab_no)) continue;

    const AQP::Table_access *const table = m_plan.get_table_access(tab_no);
    const AQP::enum_access_type access_type = table->get_access_type();
    ha_ndbcluster *handler =
        down_cast<ha_ndbcluster *>(table->get_table()->file);

    NdbQueryOptions options;
    const NdbQueryOperand *op_key[ndb_pushed_join::MAX_KEY_PART + 1];
    if (table->get_index_no() >= 0) {
      const int error = build_key(table, op_key, &options);
      if (unlikely(error)) return error;
    }

    if (table != m_join_root) {
      DBUG_ASSERT(m_tables[tab_no].m_parent != MAX_TABLES);
      const uint parent_no = m_tables[tab_no].m_parent;

      if (m_tables[tab_no].isInnerJoined(m_tables[parent_no])) {
        // 'tab_no' is inner joined with its parent
        options.setMatchType(NdbQueryOptions::MatchNonNull);
      }

      if (table->is_sj_firstmatch()) {
        /**
         * Is a Firstmatch'ed semijoin_nest. In order to let SPJ API
         * do firstMatch elimination of duplicated rows, we need to ensure:
         *  1) The entire semijoined-nest has been pushed down.
         *  2) There are no unpushed conditions in the above sj-nest.
         *
         * ... else we might end up returning a firstMatched'ed row,
         *  which later turns out to be a non-match due to eiter 1) or 2).
         */
        const int last_sj_inner = table->get_last_sj_inner();
        const ndb_table_access_map semijoin(m_tables[last_sj_inner].m_sj_nest);
        if (m_join_scope.contain(semijoin) &&
            !m_has_pending_cond.is_overlapping(semijoin)) {
          options.setMatchType(NdbQueryOptions::MatchFirst);
        }
      }

      if (table->is_antijoin()) {
        DBUG_ASSERT(m_tables[tab_no].isOuterJoined(m_tables[parent_no]));
        const ndb_table_access_map antijoin_scope(
            get_tables_in_range(tab_no, m_tables[tab_no].m_last_inner));

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
          const uint first_upper = m_tables[tab_no].m_first_upper;
          ndb_table_access_map upper_nest(full_inner_nest(first_upper, tab_no));
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
       * the NdbQuery interface is able to correcly generate NULL extended
       * rows.
       *
       * Below we add these nest dependencies even when not strictly required.
       * The API will just ignore such redundant nest dependencies.
       */
      if (m_tables[tab_no].isOuterJoined(m_tables[parent_no])) {
        ndb_table_access_map inner_nest(m_tables[tab_no].m_inner_nest);
        inner_nest.intersect(m_join_scope);
        if (!inner_nest.is_clear_all()) {
          // Table not first in its join_nest, set firstInner which it
          // depends on
          const uint real_first_inner =
              inner_nest.first_table(m_tables[tab_no].m_first_inner);
          options.setFirstInnerJoin(m_tables[real_first_inner].m_op);

        } else if (m_tables[tab_no].m_first_upper >= 0) {
          const uint first_upper = m_tables[tab_no].m_first_upper;
          ndb_table_access_map upper_nest(full_inner_nest(first_upper, tab_no));
          upper_nest.intersect(m_join_scope);
          if (!upper_nest.is_clear_all()) {
            // There is an upper nest which we outer join with
            const uint real_first_upper =
                upper_nest.first_table(m_tables[tab_no].m_first_upper);
            options.setUpperJoin(m_tables[real_first_upper].m_op);
          }
        }
      }
    }  // if '!m_join_root'

    const NdbQueryOperationDef *query_op = NULL;
    if (ndbcluster_is_lookup_operation(access_type)) {
      // Primary key access assumed
      if (access_type == AQP::AT_PRIMARY_KEY ||
          access_type == AQP::AT_MULTI_PRIMARY_KEY) {
        DBUG_PRINT("info", ("Operation is 'primary-key-lookup'"));
        query_op = m_builder->readTuple(handler->m_table, op_key, &options);
      } else {
        DBUG_ASSERT(access_type == AQP::AT_UNIQUE_KEY);
        DBUG_PRINT("info", ("Operation is 'unique-index-lookup'"));
        const NdbDictionary::Index *const index =
            handler->m_index[table->get_index_no()].unique_index;
        DBUG_ASSERT(index);
        query_op =
            m_builder->readTuple(index, handler->m_table, op_key, &options);
      }
    }  // ndbcluster_is_lookup_operation()

    /**
     * AT_MULTI_MIXED may have 'ranges' which are pure single key lookups also.
     * In our current implementation these are converted into range access in
     * the pushed MRR implementation. However, the future plan is to build both
     * RANGE and KEY pushable joins for these.
     */
    else if (access_type == AQP::AT_ORDERED_INDEX_SCAN ||
             access_type == AQP::AT_MULTI_MIXED) {
      DBUG_ASSERT(table->get_index_no() >= 0);
      DBUG_ASSERT(handler->m_index[table->get_index_no()].index != NULL);

      DBUG_PRINT("info", ("Operation is 'equal-range-lookup'"));
      DBUG_PRINT(
          "info",
          ("Creating scanIndex on index id:%d, name:%s", table->get_index_no(),
           handler->m_index[table->get_index_no()].index->getName()));

      const NdbQueryIndexBound bounds(op_key);
      query_op =
          m_builder->scanIndex(handler->m_index[table->get_index_no()].index,
                               handler->m_table, &bounds, &options);
    } else if (access_type == AQP::AT_TABLE_SCAN) {
      DBUG_PRINT("info", ("Operation is 'table scan'"));
      query_op = m_builder->scanTable(handler->m_table, &options);
    } else {
      DBUG_ASSERT(false);
    }

    if (unlikely(!query_op)) return -1;

    m_tables[tab_no].m_op = query_op;
  }  // for (join_cnt= m_join_root->get_access_no();
     // join_cnt<plan.get_access_count(); join_cnt++)

  return 0;
}  // ndb_pushed_builder_ctx::build_query()
