/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_DDL_DEFINITIONS_H
#define NDB_DDL_DEFINITIONS_H

#include "ndbapi/NdbDictionary.hpp"  // Required for NdbDictionary::Table

namespace dd {
class Table;
}

int rename_table_impl(class THD *thd, class Ndb *ndb,
                      class Ndb_schema_dist_client *schema_dist_client,
                      const NdbDictionary::Table *orig_tab,
                      dd::Table *to_table_def, const char *from, const char *to,
                      const char *old_dbname, const char *old_tabname,
                      const char *new_dbname, const char *new_tabname,
                      bool real_rename, const char *real_rename_db,
                      const char *real_rename_name, bool drop_events,
                      bool create_events, bool commit_alter);

int drop_table_impl(class THD *thd, class Ndb *ndb,
                    class Ndb_schema_dist_client *schema_dist_client,
                    const char *db, const char *table_name);

#endif /* NDB_DDL_DEFINITIONS_H */
