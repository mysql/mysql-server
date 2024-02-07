/*
   Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_HA_NDBCLUSTER_COND_INCLUDED
#define SQL_HA_NDBCLUSTER_COND_INCLUDED

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

#include "my_table_map.h"
#include "sql/sql_list.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

class Item;
class Item_field;
struct key_range;
struct TABLE;
class Ndb_item;
class Ndb_param;
class ha_ndbcluster;
class SqlScanFilter;

class ha_ndbcluster_cond {
 public:
  ha_ndbcluster_cond(ha_ndbcluster *h);
  ~ha_ndbcluster_cond();

  void cond_clear();  // Clear entire ha_ndbcluster_cond state.
  void cond_close();  // Clean up after handler close, possibly reopen later

  // Prepare condition for being pushed. Need to call
  // use_cond_push() later to make it available for the handler
  void prep_cond_push(const Item *cond, table_map const_expr_tables,
                      table_map param_expr_tables);

  // Apply the 'cond_push', pre generate code if possible.
  // Return the pushed condition and the unpushable remainder
  int use_cond_push(const Item *&pushed_cond, const Item *&remainder_cond);

  int build_cond_push();

  int generate_scan_filter_from_cond(SqlScanFilter &filter,
                                     bool param_is_const = false);

  static int generate_scan_filter_from_key(SqlScanFilter &filter,
                                           const class KEY *key_info,
                                           const key_range *start_key,
                                           const key_range *end_key);

  // Get a possibly pre-generated Interpreter code for the pushed condition
  const NdbInterpretedCode &get_interpreter_code() const {
    return m_scan_filter_code;
  }

  // Get the list of Ndb_param's (opaque) referred by the interpreter code.
  // Use get_param_item() to get the Item_field being the param source
  const List<const Ndb_param> &get_interpreter_params() const {
    return m_scan_filter_params;
  }

  // Get the 'Field' referred by Ndb_param (from previous table in query plan).
  static const Item_field *get_param_item(const Ndb_param *param);

  void set_condition(const Item *cond);
  bool check_condition() const {
    return (m_unpushed_cond == nullptr || eval_condition());
  }

  static void add_read_set(TABLE *table, const Item *cond);
  void add_read_set(TABLE *table) { add_read_set(table, m_unpushed_cond); }

 private:
  int build_scan_filter_predicate(List_iterator<const Ndb_item> &cond,
                                  SqlScanFilter *filter, bool negated,
                                  bool param_is_const) const;
  int build_scan_filter_group(List_iterator<const Ndb_item> &cond,
                              SqlScanFilter *filter, bool negated,
                              bool param_is_const) const;

  bool eval_condition() const;

  bool isGeneratedCodeReusable() const;

  ha_ndbcluster *const m_handler;

  // The serialized pushed condition
  List<const Ndb_item> m_ndb_cond;

  // A pre-generated scan_filter
  NdbInterpretedCode m_scan_filter_code;

  // The list of Ndb_params referred by 'm_scan_filter_code'. (or empty)
  List<const Ndb_param> m_scan_filter_params;

 public:
  /**
   * Conditions prepared for pushing by prep_cond_push(), with a possible
   * m_remainder_cond, which is the part of the condition which still has
   * to be evaluated by the mysql server.
   */
  const Item *m_pushed_cond;
  const Item *m_remainder_cond;

 private:
  /**
   * Stores condition which we assumed could be pushed, but too late
   * turned out to be unpushable. (Failed to generate code, or another
   * access method not allowing push condition selected). In these cases
   * we need to emulate the effect of the (non-)pushed condition by
   * requiring ha_ndbclustet to evaluate 'm_unpushed_cond' before returning
   * only qualifying rows.
   */
  const Item *m_unpushed_cond;
};

#endif
