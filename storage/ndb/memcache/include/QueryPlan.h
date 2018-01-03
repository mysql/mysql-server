/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
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

/* Forward declarations: */
class Operation;
class ExternalValue;

class QueryPlan {
  friend class Operation;
  friend class ExternalValue;

  public: 
  QueryPlan() : initialized(0)  {};  
  QueryPlan(Ndb *, const TableSpec *, PlanOpts opts = NoOptions); 
  ~QueryPlan();
  bool canHaveExternalValue() const;
  bool shouldExternalizeValue(size_t length) const;
  bool canUseCommittedRead() const;
  Uint64 getAutoIncrement() const;
  void debug_dump() const;
  bool hasDataOnDisk() const;
  bool hasMathColumn() const;

  /* public instance variables */
  bool initialized;
  bool dup_numbers;                /* dup_numbers mode for ascii incr/decr */
  bool pk_access;                  /* access by primary key */
  bool is_scan;
  size_t max_value_len;
  const TableSpec *spec;
  NdbDictionary::Dictionary *dict;
  const NdbDictionary::Table *table;
  QueryPlan * extern_store;         /* QueryPlan for external stored values */
  short cas_column_id;
  short math_column_id;
  unsigned int static_flags;
  
  
  protected:
  /* Protected instance variables; visible to Operation class */
  Record * key_record;    /* Holds just the key */
  Record * val_record;    /* Holds just the values */
  Record * row_record;    /* Holds complete row for insert and SCAN */

  private:
  /* Private methods */
  const NdbDictionary::Index * chooseIndex();
  bool keyIsPrimaryKey() const;

  /* Private instance variables */
  Ndb *db;
  bool has_disk_storage;
};

inline bool QueryPlan::hasMathColumn() const {
  return spec->math_column;
}

inline bool QueryPlan::shouldExternalizeValue(size_t length) const {
  if(extern_store && val_record->value_length) 
    return (length > val_record->value_length);
  return false;
}

inline bool QueryPlan::canHaveExternalValue() const {
  return (extern_store);
}

inline bool QueryPlan::hasDataOnDisk() const {
  return has_disk_storage;
}

inline bool QueryPlan::canUseCommittedRead() const {
  return(pk_access && (! extern_store) && (! spec->exp_column));
}

#endif
