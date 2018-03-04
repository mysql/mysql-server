/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_DD_TABLE_H
#define NDB_DD_TABLE_H

#include "sql/dd/string_type.h"

namespace dd {
  class Table;
}


/* Functions operating on dd::Table*, prefixed with ndb_dd_table_ */

/*
   Save the tables object id and version in table definition
*/
void ndb_dd_table_set_object_id_and_version(dd::Table* table_def,
                                            int object_id, int object_version);

/*
  Return table definitions object id and version
*/
bool
ndb_dd_table_get_object_id_and_version(const dd::Table* table_def,
                                       int& object_id, int& object_version);

/*
  Return engine of table definition
*/
dd::String_type ndb_dd_table_get_engine(const dd::Table* table_def);


/*
   Mark the table as being hidden, thus avoiding that it shows
   up in SHOW TABLES and information_schema queries.
*/
void ndb_dd_table_mark_as_hidden(dd::Table* table_def);

/*
   Return number of columns in the table definition
*/
size_t ndb_dd_table_get_num_columns(const dd::Table* table_def);

#endif
