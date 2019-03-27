/* Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_DD_METADATA_H
#define SQL_DD_METADATA_H

#include <mysql/plugin.h>  // st_plugin_int

#include "sql/dd/string_type.h"  // dd::String_type

class THD;
struct st_plugin_int;

namespace dd {
namespace info_schema {

/**
  The version of the current information_schema system views.

  This version number is stored on disk in the data dictionary.
  Every time the information_schema structure changes,
  this version number must change.

  The numbering to use is the MySQL version number
  of the first MySQL version that published a given database schema.
  The format is Mmmdd with M=Major, m=minor, d=dot,
  so that MySQL 8.0.4 is encoded as 80004.

  Historical I_S version number published:

  1: Published in 8.0.3-RC.
  ------------------------
  Introduced in MySQL 8.0.0 by WL#6599. Never published in a GA version.

  80011: Published in 8.0 GA.
  ------------------------------------
  Changes from version 1:

  - Bug#27309116: Add a new column `external_language` to `mysql`.`routines`
    and update `information_schema`.`routines` to reflect this column.

  - Bug#27593348: INFORMATION_SCHEMA.STATISTICS FIELD TYPE CHANGE.
    Changes the column I_S.STATISTICS.NON_UNIQUE type from VARCHAR
    to INT.

  Current 80012: Published in 8.0.12
  ------------------------------------
  Changes from version 80011:

  - Bug#27945704 UNABLE TO JOIN TABLE_CONSTRAINTS AND REFERENTIAL_CONSTRAINTS
    Changes the collation of I_S columns that project index name and
    constraint name to use utf8_tolower_ci.

  - WL#11864 Implement I_S.VIEW_TABLE_USAGE and I_S.VIEW_ROUTINE_USAGE

  - WL#1075 adds one column to INFORMATION_SCHEMA.STATISTICS: "EXPRESSION".
    This column prints out the expression for functional key parts, or SQL NULL
    if it is a regular key part. For functional key parts, COLUMN_NAME is set to
    SQL NULL.

  80013: Published in 8.0.13
  ------------------------------------
  Changes from version 80012

  - WL#11000 ST_Distance with units
    Adds a new view `information_schema`.`st_units_of_measure` with columns
    `UNIT_NAME`, `CONVERSION_FACTOR`, `DESCRIPTION`, and `UNIT_TYPE`. This view
    contains the supported spatial units.

  80014: Published in 8.0.14
  ------------------------------------
  There are no changes from version 80013. Hence server version 80014 used
  I_S version 80013.

  80015: Not published.
  ----------------------------------------------------------------------------
  There are no changes from version 80014. Hence server version 80015 used
  I_S version 80013.

  80016: Published in 8.0.14
  ------------------------------------
  Changes from version 80015.

  - WL#929 - CHECK CONSTRAINTS
    New INFORMATION_SCHMEA table CHECK_CONSTRAINTS is introduced and
    INFORMATION_SCHMEA.TABLE_CONSTRAINTS is modified to include check
    constraints defined on the table.

  - WL#12261 Control (enforce and disable) table encryption
    - Add new column information_schema.schemata.default_encryption
    - information_schema.tables.options UDF definition is changed to pass
      schema default encryption.

  80017: Current
  ----------------------------------------------------------------------------
  Changes from version 80016:

  - WL#12984 INFORMATION_SCHEMA and metadata related to secondary engine.
    Changes system view definitions of
    INFORMATION_SCHEMA.TABLES.CREATE_OPTIONS and
  INFORMATION_SCHEMA.COLUMNS.EXTRA.

  80018: Next IS version number after the previous is public.
  ----------------------------------------------------------------------------
  Changes from version 80016:
  - No changes, this version number is not active yet.

*/

static const uint IS_DD_VERSION = 80017;

/**
  Initialize INFORMATION_SCHEMA system views.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool initialize(THD *thd);

/**
  Create INFORMATION_SCHEMA system views.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool create_system_views(THD *thd);

/**
  Store the server I_S table metadata into dictionary, once during MySQL
  server bootstrap.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool store_server_I_S_metadata(THD *thd);

/**
  Store I_S table metadata into dictionary, during MySQL server startup.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool update_I_S_metadata(THD *thd);

/**
  Store dynamic I_S plugin table metadata into dictionary, during INSTALL
  command execution.

  @param thd         Thread context.
  @param plugin_int  I_S Plugin of which the metadata is to be stored.

  @return       Upon failure, return true, otherwise false.
*/
bool store_dynamic_plugin_I_S_metadata(THD *thd, st_plugin_int *plugin_int);

/**
  Remove I_S view metadata from dictionary. This is used
  UNINSTALL and server restart procedure when I_S version is changed.

  @param thd         Thread context.
  @param view_name   I_S view name of which the metadata is to be stored.

  @return       Upon failure, return true, otherwise false.
*/
bool remove_I_S_view_metadata(THD *thd, const dd::String_type &view_name);

}  // namespace info_schema
}  // namespace dd

#endif  // SQL_DD_METADATA_H
