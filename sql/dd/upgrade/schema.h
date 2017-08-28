/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD_UPGRADE__SCHEMA_H_INCLUDED
#define DD_UPGRADE__SCHEMA_H_INCLUDED

#include <vector>

#include "sql/dd/string_type.h"                // dd::String_type

class THD;

namespace dd {
namespace upgrade_57 {

/**
  Create entry in mysql.schemata for all the folders found in data directory.
  If db.opt file is not present in any folder, that folder will be treated as
  a database and a warning is issued.

  @param[in]  thd        Thread handle.
  @param[in]  dbname     Schema name.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_schema_to_dd(THD *thd, const char *dbname);

/**
  Find all the directories inside data directory. Every directory will be
  treated as a schema. These directories are in filename-encoded form.

  @param[out] db_name    An std::vector containing all database name.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool find_schema_from_datadir(std::vector<String_type> *db_name);

} // namespace upgrade
} // namespace dd

#endif // DD_UPGRADE__SCHEMA_H_INCLUDED
