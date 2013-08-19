/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_TABLE_MAINTENANCE_H
#define SQL_TABLE_MAINTENANCE_H

/* Must be able to hold ALTER TABLE t PARTITION BY ... KEY ALGORITHM = 1 ... */
#define SQL_ADMIN_MSG_TEXT_SIZE 128 * 1024

bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* table_list,
                              LEX_STRING *key_cache_name);
bool mysql_preload_keys(THD* thd, TABLE_LIST* table_list);
int reassign_keycache_tables(THD* thd, KEY_CACHE *src_cache,
                             KEY_CACHE *dst_cache);

/**
  Sql_cmd_analyze_table represents the ANALYZE TABLE statement.
*/
class Sql_cmd_analyze_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a ANALYZE TABLE statement.
  */
  Sql_cmd_analyze_table()
  {}

  ~Sql_cmd_analyze_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_ANALYZE;
  }
};



/**
  Sql_cmd_check_table represents the CHECK TABLE statement.
*/
class Sql_cmd_check_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a CHECK TABLE statement.
  */
  Sql_cmd_check_table()
  {}

  ~Sql_cmd_check_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CHECK;
  }
};


/**
  Sql_cmd_optimize_table represents the OPTIMIZE TABLE statement.
*/
class Sql_cmd_optimize_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a OPTIMIZE TABLE statement.
  */
  Sql_cmd_optimize_table()
  {}

  ~Sql_cmd_optimize_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_OPTIMIZE;
  }
};



/**
  Sql_cmd_repair_table represents the REPAIR TABLE statement.
*/
class Sql_cmd_repair_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a REPAIR TABLE statement.
  */
  Sql_cmd_repair_table()
  {}

  ~Sql_cmd_repair_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_REPAIR;
  }
};

#endif
