/*
 Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.
 
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
#include "my_config.h"
#include <stdio.h>
#include <stddef.h>
#include <strings.h>

#include "NdbApi.hpp"
#include <memcached/extension_loggers.h>

#include "ndbmemcache_global.h"
#include "ndb_engine.h"
#include "debug.h"
#include "QueryPlan.h"
#include "ExternalValue.h"

extern EXTENSION_LOGGER_DESCRIPTOR *logger;
size_t global_max_item_size;

/* For each pair [TableSpec,NDB Object], we can cache some dictionary 
   lookups, acccess path information, NdbRecords, etc.
*/

bool is_integer(const NdbDictionary::Table *table, int col_idx);

bool is_integer(const NdbDictionary::Table *table, int col_idx) {
  const NdbDictionary::Column *col = table->getColumn(col_idx);
  switch(col->getType()) {
    case NdbDictionary::Column::Tinyint         :
    case NdbDictionary::Column::Tinyunsigned    :
    case NdbDictionary::Column::Smallint        :
    case NdbDictionary::Column::Smallunsigned   :
    case NdbDictionary::Column::Mediumint       :
    case NdbDictionary::Column::Mediumunsigned  :
    case NdbDictionary::Column::Int             :
    case NdbDictionary::Column::Unsigned        :
    case NdbDictionary::Column::Bigint          :
    case NdbDictionary::Column::Bigunsigned     :
      return true;
    
    default:
        return false;
  }
}


inline const NdbDictionary::Column *get_ndb_col(const TableSpec *spec, 
                                                const NdbDictionary::Table *table,
                                                const char *col_name) 
{
  const NdbDictionary::Column *col = table->getColumn(col_name);
  if(col == 0)
    logger->log(LOG_WARNING, 0, "Invalid column \"%s.%s.%s\"\n",
                spec->schema_name, spec->table_name, col_name);
  return col;
}


/*    Create a QueryPlan for a [Ndb, TableSpec] pair.  */ 
QueryPlan::QueryPlan(Ndb *my_ndb, const TableSpec *my_spec, PlanOpts opts)  : 
  initialized(0), 
  dup_numbers(false),
  is_scan(false),
  spec(my_spec),
  extern_store(0),
  static_flags(spec->static_flags),
  key_record(0), 
  val_record(0),
  row_record(0),
  db(my_ndb)
{
  const NdbDictionary::Column *col;
  bool op_ok = false; 
  bool last_value_col_is_int = false;
  int  first_value_col_id = -1;
  
  if(! (spec->isValid())) {
    logger->log(LOG_WARNING, 0, "Container record (%s.%s) is not valid. %s\n",
                spec->schema_name ? spec->schema_name : "??",
                spec->table_name  ? spec->table_name  : "??",
                spec->nkeycols ? "" : "[No key columns defined]");
    return;
  }

  /* Get the data dictionary */
  db->setDatabaseName(spec->schema_name);
  dict = db->getDictionary(); 
  if(!dict) {
    logger->log(LOG_WARNING, 0,  "Could not get NDB dictionary.\n");
    return;
  }    
  /* Get the table */  
  table = dict->getTable(spec->table_name);
  if(! table) {
    logger->log(LOG_WARNING, 0, "Invalid table \"%s.%s\"\n", 
                spec->schema_name, spec->table_name);
    return;
  }

  /* Externalized long values */
  if(spec->external_table)
    extern_store = new QueryPlan(db, spec->external_table);
  else 
    extern_store = 0;

  /* Data on Disk? */
  has_disk_storage = 
    (table->getStorageType() == NdbDictionary::Column::StorageTypeDisk);
  if(extern_store && extern_store->has_disk_storage) 
    has_disk_storage = true;
  
  /* Process the TableSpec */
  int ncols  = spec->nkeycols + spec->nvaluecols 
             + ( spec->math_column    ? 1 : 0 )
             + ( spec->flags_column   ? 1 : 0 )
             + ( spec->cas_column     ? 1 : 0 )
             + ( spec->exp_column     ? 1 : 0 )
             + ( spec->external_table ? 2 : 0 );
  
  /* Instantiate the Records */
  key_record = new Record(spec->nkeycols);
  val_record = new Record(ncols - spec->nkeycols);
  row_record = new Record(ncols);

  /* Key Columns */
  for(int i = 0; i < spec->nkeycols ; i++) {
    col = get_ndb_col(spec, table, spec->key_columns[i]);
    // int this_col_id = col->getColumnNo();
    key_record->addColumn(COL_STORE_KEY, col);
    row_record->addColumn(COL_STORE_KEY, col);
  }

  /* Primary Key access path? */
  pk_access = keyIsPrimaryKey();

  /* Choose an access path and complete the key record*/
  if(pk_access && ! (opts == PKScan)) {
    op_ok = key_record->complete(dict, table);
  }
  else {
    const NdbDictionary::Index *plan_idx = chooseIndex();
    if(plan_idx) {
      DEBUG_PRINT("Using Index: %s on Table: %s %s", plan_idx->getName(), 
                  spec->table_name, is_scan ? "[SCAN]" : "");
      op_ok = key_record->complete(dict, plan_idx);
    }
    else {
      logger->log(LOG_WARNING, 0, "No usable keys found on %s.%s\n",
                  spec->schema_name, spec->table_name);
    }
  }
  if(op_ok == false) return;

  /* Create the value record, and the rest of the row record. */
  for(int i = 0; i < spec->nvaluecols ; i++) {
    col = get_ndb_col(spec, table, spec->value_columns[i]);
    int this_col_id = col->getColumnNo();
    row_record->addColumn(COL_STORE_VALUE, col);
    val_record->addColumn(COL_STORE_VALUE, col);
    if(i == 0) first_value_col_id = this_col_id;
    last_value_col_is_int = is_integer(table, this_col_id);
  }

  if(spec->cas_column) {                                        // CAS
    col = get_ndb_col(spec, table, spec->cas_column);
    cas_column_id = col->getColumnNo();
    row_record->addColumn(COL_STORE_CAS, col);
    val_record->addColumn(COL_STORE_CAS, col);
  }

  if(spec->math_column) {                                       // Arithmetic
    col = get_ndb_col(spec, table, spec->math_column);
    math_column_id = col->getColumnNo();
    row_record->addColumn(COL_STORE_MATH, col);
    val_record->addColumn(COL_STORE_MATH, col);
  }
  if(spec->flags_column) {                                      // Flags
    col = get_ndb_col(spec, table, spec->flags_column);
    row_record->addColumn(COL_STORE_FLAGS, col);
    val_record->addColumn(COL_STORE_FLAGS, col);
  }
  if(spec->exp_column) {                                        // Expires
    col = get_ndb_col(spec, table, spec->exp_column);
    row_record->addColumn(COL_STORE_EXPIRES, col);
    val_record->addColumn(COL_STORE_EXPIRES, col);             
  }
  if(spec->external_table) {                                    // Ext id & len
    col = get_ndb_col(spec, table, "ext_id");
    if(col == 0) {
      logger->log(LOG_WARNING,0, "Table must have column: `ext_id` INT UNSIGNED");
      return;
    }
    row_record->addColumn(COL_STORE_EXT_ID, col);
    val_record->addColumn(COL_STORE_EXT_ID, col);
    
    col = get_ndb_col(spec, table, "ext_size");
    if(col == 0) {
      logger->log(LOG_WARNING,0, "Table must have column: `ext_size` INT UNSIGNED");
      return;
    }
    row_record->addColumn(COL_STORE_EXT_SIZE, col);
    val_record->addColumn(COL_STORE_EXT_SIZE, col);    
  }

   /* Complete the records */
  if(row_record->complete(dict, table) == false) return;
  if(val_record->complete(dict, table) == false) return;

  /* Sanity Checks */
  if(spec->math_column) {                                      // Arithmetic
    if(! is_integer(table, math_column_id)) {
      logger->log(LOG_WARNING, 0, "Non-numeric column \"%s\" cannot be used "
                  "for arithmetic. \n", spec->math_column);
      return;
    }
    if((spec->nvaluecols == 1) && (! last_value_col_is_int)) {
      /* There is one varchar value column plus a math column. 
         Enable the special "duplicate math" behavior. */
      dup_numbers = true;
    }  
  }
  if(spec->cas_column && ! is_integer(table, cas_column_id)) {  // CAS
    logger->log(LOG_WARNING, 0, "Non-numeric column \"%s\" cannot be used "
                "for CAS. \n", spec->cas_column);
    return;
  }
  if(spec->external_table && spec->nvaluecols != 1) {
    logger->log(LOG_WARNING, 0, "Long external values are allowed only with 1 "
                "value column (%d on table %s).\n", 
                spec->nvaluecols, spec->table_name);
    return;
  }
  
  /* Maximum allowed value */  
  if(extern_store)
    max_value_len = EXTERN_VAL_MAX_PARTS * extern_store->max_value_len;
  else
    max_value_len = row_record->value_length;

  if(max_value_len > global_max_item_size)
    max_value_len = global_max_item_size;
  
  /* Success. */
  initialized = 1;
}


QueryPlan::~QueryPlan() {
  if(row_record) delete row_record;
  if(key_record) delete key_record;
  if(val_record) delete val_record;
  if(extern_store) delete extern_store;
}


Uint64 QueryPlan::getAutoIncrement() const {
  Uint64 auto_inc;
  db->getAutoIncrementValue(table, auto_inc, 10);
  return auto_inc;
}


void QueryPlan::debug_dump() const {
  if(key_record) {
    DEBUG_PRINT("Key record:");
    key_record->debug_dump();
  }
  if(row_record) {
    DEBUG_PRINT("Row record:");
    row_record->debug_dump();
  }
  if(val_record) {
    DEBUG_PRINT("val_record");
    val_record->debug_dump();
  }
  if(extern_store) {
    DEBUG_PRINT("extern_store");
    extern_store->debug_dump();
  }
}


bool QueryPlan::keyIsPrimaryKey() const {
  if(spec->nkeycols == table->getNoOfPrimaryKeys()) {
    for(int i = 0 ; i < spec->nkeycols ; i++) 
      if(strcmp(spec->key_columns[i], table->getPrimaryKey(i)))
        return false;
    return true;
  }
  return false;
}


const NdbDictionary::Index * QueryPlan::chooseIndex() {
  NdbDictionary::Dictionary::List list;
  const NdbDictionary::Index *idx;
  dict->listIndexes(list, spec->table_name);

  /* First look for a unique index.  All columns must match. */
  for(unsigned int i = 0; i < list.count ; i++) {
  unsigned int nmatches, j;
    idx = dict->getIndex(list.elements[i].name, spec->table_name);
    if(idx && idx->getType() == NdbDictionary::Index::UniqueHashIndex) {
      if((int) idx->getNoOfColumns() == spec->nkeycols) { 
        for(nmatches = 0, j = 0; j < idx->getNoOfColumns() ; j++) 
          if(! strcmp(spec->key_columns[j], idx->getColumn(j)->getName()))
             nmatches++;
        if(nmatches == idx->getNoOfColumns()) return idx;   // bingo!
      }
    }
  }

  /* Then look for an ordered index.  A prefix match is OK. */
  /* Return the first suitable index we find (which might not be the best) */
  for(unsigned int i = 0; i < list.count ; i++) {
    idx = dict->getIndex(list.elements[i].name, spec->table_name);
    if(idx && idx->getType() == NdbDictionary::Index::OrderedIndex) {
      if((int) idx->getNoOfColumns() >= spec->nkeycols) {  
        if(! strcmp(spec->key_columns[0], idx->getColumn(0)->getName())) {
          is_scan = true;
          return idx;
        }
      }
    }
  }
  
  return NULL;
}

