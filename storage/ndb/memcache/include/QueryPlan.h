/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_QUERYPLAN_H
#define NDBMEMCACHE_QUERYPLAN_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <assert.h>

#include <NdbApi.hpp>

#include "ndbmemcache_global.h"
#include "Record.h"
#include "TableSpec.h"

/* For each pair [TableSpec,NDB Object], we can cache some dictionary 
   lookups, acccess path information, NdbRecords, etc.
*/

enum PlanOpts { NoOptions, PKScan };

class Operation;  // forward declaration

class QueryPlan {
  friend class Operation;

  public: 
  QueryPlan() : initialized(0)  {};  
  QueryPlan(Ndb *, const TableSpec *, PlanOpts opts = NoOptions); 
  ~QueryPlan();
  bool keyIsPrimaryKey() const;
  void debug_dump() const;
   
  /* public instance variables */
  bool initialized;
  bool dup_numbers;
  Ndb *db;
  const TableSpec *spec;
  NdbDictionary::Dictionary *dict;
  const NdbDictionary::Table *table;
  bool is_scan;
  short cas_column_id;
  short math_column_id;
  unsigned int static_flags;
  unsigned char math_mask_r[4];    /* column mask for INCR read operation */
  unsigned char math_mask_u[4];    /* column mask for INCR interpreted update */
  unsigned char math_mask_i[4];    /* column mask for INCR insert operation */
  
  
  protected:
  /* Protected instance variables; visible to Operation class */
  Record * key_record;    /* Holds just the key */
  Record * val_record;    /* Holds just the values */
  Record * row_record;    /* Holds complete row for insert and SCAN */

  private:
  /* Private methods */
  const NdbDictionary::Index * chooseIndex();

  /* Private instance variables */
};


#endif
