/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <string>

#include "sql/dd/string_type.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

namespace dd {
class Table;
typedef String_type sdi_t;
}  // namespace dd

class THD;

bool ndb_sdi_serialize(THD *thd, const dd::Table *table_def,
                       const char *schema_name, dd::sdi_t &sdi);

void ndb_dd_fix_inplace_alter_table_def(dd::Table *table_def,
                                        const char *proper_table_name);

bool ndb_dd_update_schema_version(THD *thd, const char *schema_name,
                                  unsigned int counter, unsigned int node_id,
                                  bool skip_commit = false);

bool ndb_dd_has_local_tables_in_schema(THD *thd, const char *schema_name,
                                       bool &tables_exist_in_database);

const std::string ndb_dd_fs_name_case(const dd::String_type &name);

bool ndb_dd_get_schema_uuid(THD *thd, dd::String_type *schema_uuid);

bool ndb_dd_update_schema_uuid(THD *thd, const std::string &ndb_schema_uuid);

bool ndb_dd_upgrade_foreign_keys(dd::Table *dd_table_def, Ndb *ndb,
                                 const char *schema_name,
                                 const NdbDictionary::Table *ndb_table);

#endif
