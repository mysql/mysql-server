/*
   Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_HA_NDBCLUSTER_COND_INCLUDED
#define SQL_HA_NDBCLUSTER_COND_INCLUDED

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "sql/sql_list.h"

class Item;
struct key_range;
struct TABLE;
class Ndb_item;

class ha_ndbcluster_cond
{
public:
  ha_ndbcluster_cond();
  ~ha_ndbcluster_cond();

  const Item *cond_push(const Item *cond, 
                        TABLE *table, const NdbDictionary::Table *ndb_table,
                        bool other_tbls_ok,
                        Item *&pushed_cond);

  void cond_clear();
  int generate_scan_filter_from_cond(NdbScanFilter& filter);

  static
  int generate_scan_filter_from_key(NdbScanFilter& filter,
                                    const class KEY* key_info,
                                    const key_range *start_key,
                                    const key_range *end_key);

  void set_condition(const Item *cond);
  bool check_condition() const
  {
    return (m_unpushed_cond == nullptr || eval_condition());
  }

  static void add_read_set(TABLE *table, const Item *cond);
  void add_read_set(TABLE *table)
  {
    add_read_set(table, m_unpushed_cond);
  }

private:
  int build_scan_filter_predicate(List_iterator<Ndb_item> &cond,
                                  NdbScanFilter* filter,
                                  bool negated) const;
  int build_scan_filter_group(List_iterator<Ndb_item> &cond,
                              NdbScanFilter* filter,
                              bool negated) const;

  bool eval_condition() const;

  List<Ndb_item> m_ndb_cond;   //The serialized pushed condition

  /**
   * Stores condition which can't be pushed to NDB, need to be evaluated by
   * ha_ndbcluster before returning rows.
   */
  const Item *m_unpushed_cond;
};

#endif
