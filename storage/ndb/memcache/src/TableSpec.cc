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
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "TableSpec.h"
#include "debug.h"
#include "Record.h" 

/* MAX_KEY_COLUMNS and MAX_VAL_COLUMNS are defined in Record.h */

/* Parse a comma-separated string like "column1, column2".
   Makes a copy of "list".
*/
int TableSpec::build_column_list(const char ** const &col_array, 
                                 const char *list) {
  int n = 0;
  if(list == 0) return 0;
  char *next = strdup(list);  
  while(next && n < (MAX_KEY_COLUMNS + MAX_VAL_COLUMNS)) {
    char *item = strsep(& next, ", ");
    if(*item != '\0')
    {
      col_array[n++] = item;
    }
  }  
  return n;
}


/* This constructor takes comma-separated lists of key-columns and value columns
*/
TableSpec::TableSpec(const char *sqltable,
                     const char *keycols, const char *valcols) :
  math_column(0), flags_column(0), static_flags(0), 
  cas_column(0), exp_column(0),
  key_columns(new const char *[MAX_KEY_COLUMNS]) ,
  value_columns(new const char *[MAX_VAL_COLUMNS]) 
{
  nkeycols = build_column_list(key_columns, keycols);
  if(nkeycols) must_free.first_key = 1;
  nvaluecols = build_column_list(value_columns, valcols);
  if(nvaluecols) must_free.first_val = 1;
  if(sqltable) {
    char *sqltabname = strdup(sqltable);
    char *s;
    for(s = sqltabname ; *s && *s != '.' ; s++);
    schema_name = sqltabname;
    must_free.schema_name = 1;
    if(*s) {
      assert(*s == '.');
      *s = '\0' ;
      table_name = s+1;
    }
    must_free.table_name = 0;
  }
}


/* copy constructor -- deep copy */
TableSpec::TableSpec(const TableSpec &t) :
  nkeycols(t.nkeycols) ,
  nvaluecols(t.nvaluecols) ,
  key_columns(new const char *[t.nkeycols]) ,
  value_columns(new const char *[t.nvaluecols]) ,  
  schema_name(strdup(t.schema_name)) ,
  table_name(strdup(t.table_name)) , 
  math_column(strdup(t.math_column)) 
{ 
   must_free.schema_name = must_free.table_name = 1;
   must_free.special_cols = 1;
   if(nkeycols) {
     for(int i = 0; i < nkeycols ; i++) 
       key_columns[i] = strdup(t.key_columns[i]);
     must_free.all_key_cols = 1;
   }
   if(nvaluecols) {
     for(int i = 0; i < nvaluecols ; i++) 
       value_columns[i] = strdup(t.value_columns[i]);
     must_free.all_val_cols = 1;
  }
};


/* destructor */
TableSpec::~TableSpec() {
  if(! must_free.none) {
    if(must_free.schema_name && schema_name) free((void *) schema_name);
    if(must_free.table_name && table_name) free((void *) table_name);
    if(must_free.first_key)  free((void *) key_columns[0]);
    else if(must_free.all_key_cols)
      for(int i = 0 ; i < nkeycols ; i++)
        free((void *) key_columns[i]);
    if(must_free.first_val) free((void *) value_columns[0]);
    else if(must_free.all_val_cols)
      for(int i = 0 ; i < nvaluecols ; i++) 
        free((void *) value_columns[i]);
    if(must_free.special_cols) {
      if(flags_column) free((void *) flags_column);
      if(math_column)  free((void *) math_column);
      if(cas_column)   free((void *) cas_column);
      if(exp_column)   free((void *) exp_column);
    }
  }  
  delete[] key_columns;
  delete[] value_columns;
};


void TableSpec::setKeyColumns(const char *col1, ...) {
  va_list ap;
  va_start(ap, col1);
  key_columns[0] = col1;
  for(int idx = 1 ; idx < nkeycols ; idx++) 
    key_columns[idx] = va_arg(ap, const char *);
  
  assert(va_arg(ap, const char *) == 0);  // Require the final one to be NULL
  va_end(ap);
  must_free.first_key = must_free.all_key_cols = 0;
}


void TableSpec::setValueColumns(const char *col1, ...) {
  va_list ap;
  va_start(ap, col1);
  value_columns[0] = col1;
  for(int idx = 1 ; idx < nvaluecols ; idx++) 
    value_columns[idx] = va_arg(ap, const char *);
  assert(va_arg(ap, const char *) == 0);  // Require the final one to be NULL
  va_end(ap);
  must_free.first_val = must_free.all_val_cols = 0;
}

