/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

#include "storage/ndb/include/ndbapi/NdbApi.hpp"

class Item;
struct key_range;
struct TABLE;
class Ndb_cond;
class Ndb_cond_stack;

class ha_ndbcluster_cond
{
public:
  ha_ndbcluster_cond();
  ~ha_ndbcluster_cond();

  const Item *cond_push(const Item *cond, 
                        TABLE *table, const NdbDictionary::Table *ndb_table);
  void cond_pop();
  void cond_clear();
  int generate_scan_filter(NdbInterpretedCode* code, 
                           NdbScanOperation::ScanOptions* options) const;
  int generate_scan_filter_from_cond(NdbScanFilter& filter) const;
  int generate_scan_filter_from_key(NdbInterpretedCode* code,
                                    NdbScanOperation::ScanOptions* options,
                                    const class KEY* key_info,
                                    const key_range *start_key,
                                    const key_range *end_key) const;
private:
  bool serialize_cond(const Item *cond, Ndb_cond_stack *ndb_cond,
                      TABLE *table,
                      const NdbDictionary::Table *ndb_table) const;
  int build_scan_filter_predicate(Ndb_cond* &cond, 
                                  NdbScanFilter* filter,
                                  bool negated= false) const;
  int build_scan_filter_group(Ndb_cond* &cond, 
                              NdbScanFilter* filter) const;
  int build_scan_filter(Ndb_cond* &cond, NdbScanFilter* filter) const;

  Ndb_cond_stack *m_cond_stack;
};
