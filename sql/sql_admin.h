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
  Analyze_statement represents the ANALYZE TABLE statement.
*/
class Analyze_table_statement : public Sql_statement
{
public:
  /**
    Constructor, used to represent a ANALYZE TABLE statement.
    @param lex the LEX structure for this statement.
  */
  Analyze_table_statement(LEX *lex)
    : Sql_statement(lex)
  {}

  ~Analyze_table_statement()
  {}

  /**
    Execute a ANALYZE TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};



/**
  Check_table_statement represents the CHECK TABLE statement.
*/
class Check_table_statement : public Sql_statement
{
public:
  /**
    Constructor, used to represent a CHECK TABLE statement.
    @param lex the LEX structure for this statement.
  */
  Check_table_statement(LEX *lex)
    : Sql_statement(lex)
  {}

  ~Check_table_statement()
  {}

  /**
    Execute a CHECK TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};



/**
  Optimize_table_statement represents the OPTIMIZE TABLE statement.
*/
class Optimize_table_statement : public Sql_statement
{
public:
  /**
    Constructor, used to represent a OPTIMIZE TABLE statement.
    @param lex the LEX structure for this statement.
  */
  Optimize_table_statement(LEX *lex)
    : Sql_statement(lex)
  {}

  ~Optimize_table_statement()
  {}

  /**
    Execute a OPTIMIZE TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};



/**
  Repair_table_statement represents the REPAIR TABLE statement.
*/
class Repair_table_statement : public Sql_statement
{
public:
  /**
    Constructor, used to represent a REPAIR TABLE statement.
    @param lex the LEX structure for this statement.
  */
  Repair_table_statement(LEX *lex)
    : Sql_statement(lex)
  {}

  ~Repair_table_statement()
  {}

  /**
    Execute a REPAIR TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};

#endif
