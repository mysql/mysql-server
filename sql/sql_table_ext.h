/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifndef SQL_TABLE_EXT_INCLUDED
#define SQL_TABLE_EXT_INCLUDED

extern bool duckdb_require_primary_key;
extern bool force_innodb_to_duckdb;
extern bool ignore_index_hint_error;

/**
  @brief Check if need to change original storage engine to InnoDB

  Checks if 'force_memory_to_innodb/force_myisam_to_innodb'
  is enabled, then check if the coverting conditions.

  @param[in]     thd          Thread descriptor.
  @param[in]     db           db for check schema mysql
  @param[in,out] create_info  Create info from parser, including engine.

*/
void force_convert_engine(THD *thd, const char *db,
                          HA_CREATE_INFO *create_info);

/**
  @brief Check if need to change original storage engine to InnoDB

  Checks if 'force_memory_to_innodb/force_myisam_to_innodb'
  is enabled, then check if the coverting conditions.

  @param[in]     thd          Thread descriptor.
  @param[in]     table        Target table handler, including target engine.
  @param[in]     db1          first db for check schema mysql
  @param[in]     db2          second db for check schema mysql
  @param[in,out] create_info  Create info from parser, including engine.

*/
void force_convert_engine(THD *thd, TABLE *table, const char *db1,
                          const char *db2, HA_CREATE_INFO *create_info);

#endif  // SQL_TABLE_EXT_INCLUDED
