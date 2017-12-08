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

#ifndef DD_UPGRADE__TABLE_H_INCLUDED
#define DD_UPGRADE__TABLE_H_INCLUDED

class THD;

namespace dd {
namespace upgrade_57 {

/**
  Migrate plugin table to Data Dictionary.

  All plugin initialization should happens before
  user tables upgrade. It is needed to initialize
  all the Storage Engines.

  @param[in]  thd        Thread handle.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_plugin_table_to_dd(THD *thd);

/**
  Identify all .frm files to upgrade in a database.

  @param[in]  thd                        Thread handle.
  @param[in]  dbname                     Pointer for database name.
  @param[in]  is_fix_view_cols_and_deps  Fix view col data, table and
                                         routines dependency.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool migrate_all_frm_to_dd(THD *thd, const char *dbname,
                           bool is_fix_view_cols_and_deps);

} // namespace upgrade
} // namespace dd

#endif // DD_UPGRADE__TABLE_H_INCLUDED
