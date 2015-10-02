/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql_formatter_options.h"

using namespace Mysql::Tools::Dump;

void Sql_formatter_options::create_options()
{
  this->create_new_option(&m_add_locks, "add-locks",
    "Wrap data inserts on table with write lock on that table in output. "
    "This doesn't work with parallelism.");
  this->create_new_option(&m_drop_database, "add-drop-database",
    "Add a DROP DATABASE before each CREATE DATABASE.");
  this->create_new_option(&m_drop_table, "add-drop-table",
    "Add a DROP TABLE before each CREATE TABLE.");
  this->create_new_option(&m_drop_user, "add-drop-user",
    "Add a DROP USER before each CREATE USER.");
  this->create_new_option(&m_dump_column_names, "complete-insert",
    "Use complete insert statements, include column names.");
  this->create_new_option(&m_deffer_table_indexes, "defer-table-indexes",
    "Defer addition of indexes of table to be added after all rows are "
    "dumped.")
    ->set_value(true);
  this->create_new_option(&m_insert_type_replace, "replace",
    "Use REPLACE INTO for dumped rows instead of INSERT INTO.");
  this->create_new_option(&m_insert_type_ignore, "insert-ignore",
    "Use INSERT IGNORE INTO for dumped rows instead of INSERT INTO.");
  this->create_new_option(&m_suppress_create_table, "no-create-info",
    "Suppress CREATE TABLE statements.")
    ->set_short_character('t');
  this->create_new_option(&m_suppress_create_database, "no-create-db",
	  "Suppress CREATE DATABASE statements.");
  this->create_new_option(&m_hex_blob, "hex-blob",
    "Dump binary strings (in fields of type BINARY, VARBINARY, BLOB, ...) "
    "in hexadecimal format.");
  this->create_new_option(&m_timezone_consistent, "tz-utc",
    "SET TIME_ZONE='+00:00' at top of dump to allow dumping of TIMESTAMP "
    "data when a server has data in different time zones or data is being "
    "moved between servers with different time zones.")
    ->set_value(true);
  this->create_new_option(&m_charsets_consistent, "set-charset",
    "Add 'SET NAMES default_character_set' to the output to keep charsets "
    "consistent.")
    ->set_value(true);
  this->create_new_option(&m_skip_definer, "skip-definer",
    "Skip DEFINER and SQL SECURITY clauses for Views and Stored Routines.");
}

Sql_formatter_options::Sql_formatter_options(
  const Mysql_chain_element_options* mysql_chain_element_options)
  : m_mysql_chain_element_options(mysql_chain_element_options)
{}
