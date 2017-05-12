/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

#include <ndbapi/NdbApi.hpp>

class Item;
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
                                    const struct st_key_range *start_key,
                                    const struct st_key_range *end_key) const;
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
