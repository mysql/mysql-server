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
#ifndef NDBMEMCACHE_TABLESPEC_H
#define NDBMEMCACHE_TABLESPEC_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include "ndbmemcache_config.h"

/** @class TableSpec
 *  @brief In-memory representation of a containers record from the configuration. 
 */
class TableSpec {

  public:
  /* Public Constructors */
  TableSpec(int nkeys, int nvals);
  TableSpec(const char *db, const char *tab, int nkeys, int nvals);
  TableSpec(const char *sqltabname, const char *keycols, const char *valcols);
  TableSpec(const TableSpec &);

  /* Other Public Methods */
  ~TableSpec();
  void setTable(const char *db, const char *table);
  void setKeyColumns(const char *col1, ...);
  void setValueColumns(const char *col1, ...);
  bool isValid() const;
  
  /* Public instance variables */
  int nkeycols;
  int nvaluecols;
  const char *schema_name;
  const char *table_name;
  const char *math_column;
  const char *flags_column;
  const char *cas_column;
  const char *exp_column;
  Uint32 static_flags;
  const char ** const key_columns;
  const char ** const value_columns;
  TableSpec * external_table;

  private:
  /* private instance variables */
  struct {
    unsigned none         : 1;
    unsigned schema_name  : 1;
    unsigned table_name   : 1;
    unsigned first_key    : 1;
    unsigned all_key_cols : 1;
    unsigned first_val    : 1;
    unsigned all_val_cols : 1;
    unsigned special_cols : 1;   
  } must_free;

  /* private instance methods */
  void initialize_flags(void);

  /* private class methods */
  static int build_column_list(const char ** const &array, const char *list);  
};


/* Inline functions */

inline TableSpec::TableSpec(int nkeys, int nvals) : 
                            nkeycols(nkeys), 
                            nvaluecols(nvals),
                            schema_name(0), table_name(0),
                            math_column(0), flags_column(0), 
                            cas_column(0), exp_column(0), static_flags(0),                             
                            key_columns(new const char *[nkeys]),
                            value_columns(new const char *[nvals]),
                            external_table(0) { 
  must_free.none = 1; 
}

inline TableSpec::TableSpec(const char *db, const char *tab, 
                            int nkeys, int nvals) :
                            nkeycols(nkeys), nvaluecols(nvals), 
                            schema_name(db), table_name(tab),
                            math_column(0), flags_column(0), 
                            cas_column(0), exp_column(0), static_flags(0),
                            key_columns(new const char *[nkeys]),
                            value_columns(new const char *[nvals]),
                            external_table(0) {
  must_free.none = 1; 
}

inline void TableSpec::setTable(const char *db, const char *table) {
  schema_name = db;
  table_name = table;
  must_free.schema_name = 1;
  must_free.table_name  = 1;
}

inline bool TableSpec::isValid() const {
  return (schema_name && table_name && nkeycols);
}

#endif


