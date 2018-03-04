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

#ifndef NDB_DD_H
#define NDB_DD_H

#include "sql/dd/string_type.h"

namespace dd {
  class Table;
  typedef String_type sdi_t;
}

bool ndb_sdi_serialize(class THD *thd,
                       const dd::Table *table_def,
                       const char* schema_name,
                       dd::sdi_t& sdi);


void ndb_dd_fix_inplace_alter_table_def(dd::Table *table_def,
                                        const char* proper_table_name);

bool ndb_dd_drop_table(class THD* thd,
                       const char* schema_name,
                       const char* table_name);

bool ndb_dd_rename_table(class THD* thd,
                         const char* old_schema_name,
                         const char* old_table_name,
                         const char* new_schema_name,
                         const char* new_table_name,
                         int new_table_id, int new_table_version);

bool ndb_dd_get_engine_for_table(THD *thd,
                                 const char *schema_name,
                                 const char *table_name,
                                 dd::String_type* engine);

#endif
