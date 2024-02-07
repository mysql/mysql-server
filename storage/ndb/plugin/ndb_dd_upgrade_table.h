/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_DD_UPGRADE_TABLE_H_INCLUDED
#define NDB_DD_UPGRADE_TABLE_H_INCLUDED

#include "sql/dd/string_type.h"

class THD;
class Ndb_dd_client;

namespace dd {
namespace ndb_upgrade {

/**
  Migrate table to Data Dictionary.

  @param[in]  thd                        Thread handle.
  @param[in]  dd_client                  Pointer to Ndb_dd_client object.
  @param[in]  schema_name                Name of the database.
  @param[in]  table_name                 Name of the table.
  @param[in]  frm_data                   Unpacked frm data.
  @param[in]  unpacked_len               Unpacked length of frm data.

  @retval true   ON SUCCESS
  @retval false  ON FAILURE
*/
bool migrate_table_to_dd(THD *thd, Ndb_dd_client *dd_client,
                         const String_type &schema_name,
                         const String_type &table_name,
                         const unsigned char *frm_data,
                         const unsigned int unpacked_len);

}  // namespace ndb_upgrade
}  // namespace dd

#endif  // NDB_DD_UPGRADE_TABLE_H_INCLUDED
