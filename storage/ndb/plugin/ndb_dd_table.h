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

#ifndef NDB_DD_TABLE_H
#define NDB_DD_TABLE_H

#include <my_inttypes.h>  // ulong

#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"

namespace dd {
class Table;
typedef unsigned long long Object_id;
}  // namespace dd

struct Ndb_dd_handle {
  const dd::Object_id spi;
  const int version;

  Ndb_dd_handle() : spi(dd::INVALID_OBJECT_ID), version(0) {}
  Ndb_dd_handle(int spi, int version) : spi(spi), version(version) {}
  Ndb_dd_handle(dd::Object_id spi, int version) : spi(spi), version(version) {}

  bool operator==(const Ndb_dd_handle &other) const {
    return spi == other.spi && version == other.version;
  }
  bool operator!=(const Ndb_dd_handle &other) const {
    return spi != other.spi || version != other.version;
  }
  bool valid() const { return spi != dd::INVALID_OBJECT_ID; }

  std::string to_string() const {
    return {"se_private_id=" + std::to_string(spi) +
            " version=" + std::to_string(version)};
  }
};

/* Functions operating on dd::Table*, prefixed with ndb_dd_table_ */

/*
   Save the tables object id and version in table definition
*/
void ndb_dd_table_set_spi_and_version(dd::Table *table_def, int spi,
                                      int version);

void ndb_dd_table_set_spi_and_version(dd::Table *table_def,
                                      Ndb_dd_handle handle);

/*
  Return table definitions object id and version
*/
Ndb_dd_handle ndb_dd_table_get_spi_and_version(const dd::Table *table_def);

/*
  Return engine of table definition
*/
dd::String_type ndb_dd_table_get_engine(const dd::Table *table_def);

/*
   Mark the table as being hidden, thus avoiding that it shows
   up in SHOW TABLES and information_schema queries.
*/
void ndb_dd_table_mark_as_hidden(dd::Table *table_def);

/*
   Return number of columns in the table definition
*/
size_t ndb_dd_table_get_num_columns(const dd::Table *table_def);

/*
  Return true if table is using fixed row format
*/
bool ndb_dd_table_is_using_fixed_row_format(const dd::Table *table_def);

/*
  Set the row format of the table
*/
void ndb_dd_table_set_row_format(dd::Table *table_def,
                                 const bool force_var_part);

/*
  Check if the number of partitions in DD match the number of
  partitions in NDB Dictionary. Return true if they are equal,
  false if not
*/
bool ndb_dd_table_check_partition_count(const dd::Table *table_def,
                                        size_t ndb_num_partitions);

/*
  If the upstream assumption about number of partitions is wrong,
  correct the number of partitions in DD to match the number
  of partitions in NDB. This mismatch occurs when NDB specific
  partitioning schemes are specified

  NOTE: Whether the number of partitions should be decided upstream
        at all is another question
*/
void ndb_dd_table_fix_partition_count(dd::Table *table_def,
                                      size_t ndb_num_partitions);

/*
  Save the previous mysql version of the table. Applicable only for tables that
  have been upgraded
*/
void ndb_dd_table_set_previous_mysql_version(dd::Table *table_def,
                                             ulong previous_mysql_version);

/*
  Return the previous mysql version of the table. Returns false if
  previous_mysql_version is not set or invalid, true on success
*/
bool ndb_dd_table_get_previous_mysql_version(const dd::Table *table_def,
                                             ulong &previous_mysql_version);

/*
  Set tablespace id for the table
*/
void ndb_dd_table_set_tablespace_id(dd::Table *table_def,
                                    dd::Object_id tablespace_id);

/*
  Save the schema UUID in table definition
*/
void ndb_dd_table_set_schema_uuid(dd::Table *table_def, const char *value);

/*
  Retrieve the schema UUID from table definition
*/
bool ndb_dd_table_get_schema_uuid(const dd::Table *table_def,
                                  dd::String_type *value);

/*
  Check if column with given name exists and has varbinary type.
*/
bool ndb_dd_table_check_column_varbinary(const dd::Table *table_def,
                                         const dd::String_type &col_name);

/*
  Check if table has associated triggers
*/
bool ndb_dd_table_has_trigger(const dd::Table *table_def);

/* interface for working with opaque dd::Table RAII-style */
class Ndb_dd_table {
  class THD *const m_thd;
  dd::Table *m_table_def;

 public:
  Ndb_dd_table() = delete;
  Ndb_dd_table(const Ndb_dd_table &) = delete;
  Ndb_dd_table(THD *thd);

  ~Ndb_dd_table();

  dd::Table *get_table_def() { return m_table_def; }
};

#endif
