/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#ifndef ABSTRACT_QUERY_PLAN_H_INCLUDED
#define ABSTRACT_QUERY_PLAN_H_INCLUDED

#include <assert.h>
#include <sys/types.h>
#include "my_table_map.h"

#include "sql/join_type.h"
#include "sql/mem_root_array.h"

class Item;
class Item_equal;
class Item_field;
class JOIN;
class KEY_PART_INFO;
class THD;
struct AccessPath;
struct TABLE;
struct TABLE_REF;

/**
  Abstract query plan (AQP) is an interface for examining certain aspects of
  query plans without accessing the AccessPath directly.

  AQP maps join execution plans, as represented by mysqld internals, to a set
  of facade classes. Non-join operations such as sorting and aggregation is
  currently *not* modelled in the AQP.

  The AQP models an n-way join as a sequence of the n table access operations
  that the MySQL server would execute as part of its nested loop join
  execution. (Each such table access operation is a scan of a table or index,
  or an index lookup.) For each lookup operation, it is possible to examine
  the expression that represents each field in the key.

  A storage enging will typically use the AQP for finding sections of a join
  execution plan that may be executed in the engine rather than in mysqld. By
  using the AQP rather than the mysqld internals directly, the coupling between
  the engine and mysqld is reduced.

  Note that even the AQP was intended to be 'Abstract', it has some rather NDB
  specific logic. As NDB is the only user of it, it should probably be made
  a part of storage/ndb/plugin longer term.
*/
namespace AQP {

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

class Join_plan;
class Join_nest;
class Join_scope;  // 'is a' Join_nest as well.

/**
  This class represents an access operation on a table, such as a table
  scan, or a scan or lookup via an index. A Table_access object is always
  owned by a Join_plan object, such that the life time of the Table_access
  object ends when the life time of the owning Join_plan object ends.
 */
class Table_access {
 public:
  Table_access(Join_plan *plan, Join_nest *join_nest, AccessPath *table,
               AccessPath *filter);

  enum_access_type get_access_type() const;

  const char *get_other_access_reason() const;

  uint get_no_of_key_fields() const;

  const Item *get_key_field(uint field_no) const;

  const KEY_PART_INFO *get_key_part_info(uint field_no) const;

  uint get_access_no() const;

  int get_index_no() const;

  const TABLE *get_table() const;

  Item_equal *get_item_equal(const Item_field *field_item) const;

  table_map get_tables_in_this_query_scope() const;
  table_map get_tables_in_all_query_scopes() const;

  const char *get_scope_description() const;

  // Need to return rows in index sort order?
  bool use_order() const;

  // Get the condition for 'this' table.
  Item *get_condition() const;

  // Do we have some conditions (aka FILTERs) in the AccessPath
  // between 'this' table and the 'ancestor'
  bool has_condition_inbetween(const Table_access *ancestor) const;

  uint get_first_inner() const;
  uint get_last_inner() const;
  int get_first_upper() const;

  int get_first_sj_inner() const;
  int get_last_sj_inner() const;
  int get_first_sj_upper() const;

  // Is member of a SEMI-Join_nest, relative to ancestor?
  bool is_semi_joined(const Table_access *ancestor) const;

  // Is member of an ANTI-Join_nest, relative to ancestor?
  bool is_anti_joined(const Table_access *ancestor) const;

  /**
    Getter and setters for an opaque object for each table.
    Used by the handler's to persist 'pushability-flags' to avoid
    overhead by recalculating it for each ::engine_push()
  */
  uint get_table_properties() const;
  void set_table_properties(uint);

 private:
  Join_nest *const m_join_nest;

  const uint m_tab_no;

  /** Describes an AccessPath referring a TABLE* type */
  const AccessPath *const m_path;
  const TABLE *const m_table{nullptr};  // The TABLE accessed by m_path

  /** An optional AccessPath::FILTER in effect for this table */
  AccessPath *const m_filter;

  /** The access type used for this table */
  mutable enum_access_type m_access_type{AT_VOID};

  /**
    The reason for getting m_access_type==AT_OTHER. Used for EXPLAIN.
  */
  mutable const char *m_other_access_reason{nullptr};

  /** The index to use for this operation (if applicable )*/
  mutable int m_index_no{-1};

  /** May store an opaque property / flag */
  uint m_properties{0};

  const Join_scope *get_join_scope() const;
  const TABLE_REF *get_table_ref() const;

  void compute_type_and_index() const;
};  // class Table_access

/**
  This class represents a query plan for an n-way join, in the form of a
  sequence of n table access operations that will execute as a nested loop
  join.
*/
class Join_plan {
  friend class Table_access;

 public:
  explicit Join_plan(THD *thd, AccessPath *plan, const JOIN *join);

  Table_access *get_table_access(uint access_no);
  uint get_access_count() const;

 private:
  void construct(Join_nest *nest_ctx, AccessPath *plan);

  THD *const m_thd;
  const JOIN *const m_join;
  Mem_root_array<Table_access> m_table_accesses;

  // No copying.
  Join_plan(const Join_plan &) = delete;
  Join_plan &operator=(const Join_plan &) = delete;
};
// class Join_plan

/**
   @return The number of table access operations in the nested loop join.
*/
inline uint Join_plan::get_access_count() const {
  return m_table_accesses.size();
}

/**
  Get the n'th table access operation.
  @param access_no The index of the table access operation to fetch.
  @return The access_no'th table access operation.
*/
inline Table_access *Join_plan::get_table_access(uint access_no) {
  return (&m_table_accesses[access_no]);
}

/** Get the type of this operation.*/
inline enum_access_type Table_access::get_access_type() const {
  if (m_access_type == AT_VOID) compute_type_and_index();
  return m_access_type;
}

/**
  Get a description of the reason for getting access_type==AT_OTHER. To be
  used for informational messages.
  @return A string that should be assumed to have the same life time as the
  Table_access object.
*/
inline const char *Table_access::get_other_access_reason() const {
  if (m_access_type == AT_VOID) compute_type_and_index();
  return m_other_access_reason;
}

/**
  @return The number of the index to use for this access operation (
  or -1 for non-index operations).
*/
inline int Table_access::get_index_no() const {
  if (m_access_type == AT_VOID) compute_type_and_index();
  return m_index_no;
}

/**
  Get the number of this Table_access within the enclosing Join_plan.
  (This number will be in the range 0 to Join_plan::get_access_count() - 1.)
*/
inline uint Table_access::get_access_no() const { return m_tab_no; }

inline uint Table_access::get_table_properties() const { return m_properties; }

inline void Table_access::set_table_properties(uint val) { m_properties = val; }

}  // namespace AQP
// namespace AQP

#endif
