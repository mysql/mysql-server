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

#pragma once

#include <string>
#include "duckdb_types.h"
#include "sql/field.h"

class BaseConvertor {
 public:
  /** Check if query can be executed by duckdb */
  virtual bool check() = 0;

  /** Get the query under duckdb syntax. */
  virtual std::string translate() = 0;

  virtual ~BaseConvertor() = default;
};

enum enum_ddl_convertor_type {
  /* Do nothing */
  NONE_OP = 0,

  /* Drop column */
  DROP_COLUMN,

  /* Add column */
  ADD_COLUMN,

  /* Alter column */
  ALTER_COLUMN,

  /* Add index */
  ADD_INDEX,

  /* Rename table */
  RENAME_TABLE,

  /* Drop partition */
  DROP_PARTITION,

  /* This should be the last ! */
  DDL_CONVERTOR_TYPE_END
};

class FieldConvertor : public BaseConvertor {
 public:
  FieldConvertor(Field *field, const dd::Table *dd_table)
      : m_field(field), m_dd_table(dd_table) {}

  bool check() override;

  std::string translate() override;

  static std::string convert_type(const Field *field);

 private:
  Field *m_field;
  const dd::Table *m_dd_table;
};

/** Convertor to translate "ALTER TABLE ..." */
class AlterTableConvertor : public BaseConvertor {
 public:
  AlterTableConvertor(const std::string &schema_name,
                      const std::string &table_name,
                      const enum_ddl_convertor_type type)
      : m_schema_name(schema_name), m_table_name(table_name), m_type(type) {}
  ~AlterTableConvertor() override = default;

  bool check() override { return false; }

  std::string translate() override { return ""; }

  std::string m_schema_name;
  std::string m_table_name;
  const enum_ddl_convertor_type m_type;
};

/** Convert create table from mysql to duckdb. */
class CreateTableConvertor : public BaseConvertor {
 public:
  CreateTableConvertor(THD *thd, const TABLE *table,
                       const HA_CREATE_INFO *create_info,
                       const dd::Table *dd_table)
      : m_schema_name(to_string(table->s->db)),
        m_table_name(to_string(table->s->table_name)),
        m_thd(thd),
        m_table(table),
        m_create_info(create_info),
        m_dd_table(dd_table) {}

  ~CreateTableConvertor() override = default;

  bool check() override;

  std::string translate() override;

 private:
  std::string m_schema_name;
  std::string m_table_name;

  /** Thread context */
  THD *m_thd;

  /** Table to create */
  const TABLE *m_table;

  /** Create info */
  const HA_CREATE_INFO *m_create_info;

  /** DD table */
  const dd::Table *m_dd_table;

  /** Append column definition to output.
  @param[in, out]  output  output stream */
  void append_column_definition(std::ostringstream &output);
};

/** Convertor to translate "RENAME TABLE ... to ..." or
"ALTER TABLE ... RENAME TO ..." */
class RenameTableConvertor : public AlterTableConvertor {
 public:
  RenameTableConvertor(const std::string &old_schema_name,
                       const std::string &old_table_name,
                       const std::string &new_schema_name,
                       const std::string &new_table_name,
                       const dd::Table *old_dd_table,
                       const dd::Table *new_dd_table)
      : AlterTableConvertor(old_schema_name, old_table_name, RENAME_TABLE),
        m_new_schema_name(new_schema_name),
        m_new_table_name(new_table_name),
        m_old_dd_table(old_dd_table),
        m_new_dd_table(new_dd_table) {}

  ~RenameTableConvertor() override = default;

  bool check() override;

  /** ALTER TABLE ... RENAME TO ... */
  std::string translate() override;

 private:
  /** New schema name */
  std::string m_new_schema_name;

  /** New table name */
  std::string m_new_table_name;

  /** Old dd table */
  const dd::Table *m_old_dd_table;

  /** New dd table */
  const dd::Table *m_new_dd_table;
};

/** Pair of Create_field(new) and Field(new). */
using Column = std::pair<Create_field *, Field *>;

/** Vector of columns to alter. */
using Columns = std::vector<Column>;

/** Convertor to translate "ALTER TABLE ... ADD COLUMN ..." */
class AddColumnConvertor : public AlterTableConvertor {
 public:
  AddColumnConvertor(const std::string &schema_name,
                     const std::string &table_name, const TABLE *altered_table,
                     Alter_info *alter_info, const dd::Table *dd_table)
      : AlterTableConvertor(schema_name, table_name, ADD_COLUMN),
        m_new_table(altered_table),
        m_alter_info(alter_info),
        m_new_dd_table(dd_table) {
    prepare_columns();
  }

  ~AddColumnConvertor() override = default;

  bool check() override;

  std::string translate() override;

 private:
  /** new TABLE */
  const TABLE *m_new_table;

  /** Alter options, fields and keys for the new version of table. */
  Alter_info *m_alter_info;

  /** new DD table */
  const dd::Table *m_new_dd_table;

  /** Columns to add */
  Columns m_columns_to_add;

  /** Columns to set not null */
  Columns m_columns_to_set_not_null;

  /** Prepare columns to add and set not null. */
  void prepare_columns();
};

/** Convertor to translate "ALTER TABLE ... DROP COLUMN ..." */
class DropColumnConvertor : public AlterTableConvertor {
 public:
  DropColumnConvertor(const std::string &schema_name,
                      const std::string &table_name, const TABLE *old_table)
      : AlterTableConvertor(schema_name, table_name, DROP_COLUMN),
        m_old_table(old_table) {
    prepare_columns();
  }
  ~DropColumnConvertor() override = default;

  bool check() override;

  std::string translate() override;

 private:
  /** old TABLE */
  const TABLE *m_old_table;

  /** Columns to drop */
  Columns m_columns_to_drop;

  /** Prepare columns to drop. */
  void prepare_columns();
};

/** Convertor to translate change column default value. */
class ChangeColumnDefaultConvertor : public AlterTableConvertor {
 public:
  ChangeColumnDefaultConvertor(const std::string &schema_name,
                               const std::string &table_name,
                               const TABLE *new_table,
                               const dd::Table *new_dd_table,
                               Alter_info *alter_info)
      : AlterTableConvertor(schema_name, table_name, ALTER_COLUMN),
        m_new_table(new_table),
        m_new_dd_table(new_dd_table),
        m_alter_info(alter_info) {
    prepare_columns();
  }

  ~ChangeColumnDefaultConvertor() override = default;

  std::string translate() override;

 private:
  /** new TABLE */
  const TABLE *m_new_table;

  /** new DD table */
  const dd::Table *m_new_dd_table;

  /** Alter options, fields and keys for the new version of table. */
  Alter_info *m_alter_info;

  /** Columns to set default */
  Columns m_columns_to_set_default;

  /** Columns to drop default */
  Columns m_columns_to_drop_default;

  /** Prepare columns to set default and drop default. */
  void prepare_columns();
};

/** Convertor to translate "ALTER TABLE ... [ CHANGE | MODIFY | RENAME ]
COLUMN ..." */
class ChangeColumnConvertor : public AlterTableConvertor {
 public:
  ChangeColumnConvertor(const std::string &schema_name,
                        const std::string &table_name, const TABLE *new_table,
                        const dd::Table *new_dd_table, Alter_info *alter_info)
      : AlterTableConvertor(schema_name, table_name, ALTER_COLUMN),
        m_new_table(new_table),
        m_new_dd_table(new_dd_table),
        m_alter_info(alter_info) {
    prepare_columns();
  }

  ~ChangeColumnConvertor() override = default;

  bool check() override;

  std::string translate() override;

 private:
  /** new TABLE */
  const TABLE *m_new_table;

  /** new DD table */
  const dd::Table *m_new_dd_table;

  /** Alter options, fields and keys for the new version of table. */
  Alter_info *m_alter_info;

  /** Columns to change */
  Columns m_columns;

  /** Columns to change type */
  Columns m_columns_to_change_type;

  /** Columns to set not null */
  Columns m_columns_to_set_not_null;

  /** Columns to drop not null */
  Columns m_columns_to_drop_not_null;

  /** Columns to rename */
  Columns m_columns_to_rename;

  /** Prepare columns to change. */
  void prepare_columns();
};

/** Convertor to set primary key column not null. */
class ChangeColumnForPrimaryKeyConvertor : public AlterTableConvertor {
 public:
  ChangeColumnForPrimaryKeyConvertor(const std::string &schema_name,
                                     const std::string &table_name,
                                     const TABLE *new_table)
      : AlterTableConvertor(schema_name, table_name, ALTER_COLUMN),
        m_new_table(new_table) {
    prepare_columns();
  }

  std::string translate() override;

 private:
  /** new TABLE */
  const TABLE *m_new_table;

  /** Columns to set not null */
  std::vector<Field *> m_columns_to_set_not_null;

  /** Prepare columns to set not null. */
  void prepare_columns();
};

class DropPartitionConvertor : public AlterTableConvertor {
 public:
  DropPartitionConvertor(const std::string &schema_name,
                         const std::string &table_name,
                         partition_info *part_info)
      : AlterTableConvertor(schema_name, table_name, DROP_PARTITION),
        m_part_info(part_info) {}

  bool check() override;

  std::string translate() override;

 private:
  partition_info *m_part_info;
  std::string m_query;
};

/* Convert a binary string to a SQL bit-string literal: x'010AAC'::BLOB */
std::string toHex(const char *data, size_t length);
