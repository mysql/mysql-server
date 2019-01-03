/* Copyright (c) 2000, 2018, Alibaba and/or its affiliates. All rights reserved.

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

#ifndef SQL_SEQUENCE_INCLUDED
#define SQL_SEQUENCE_INCLUDED

#include "handler.h"
#include "lex_string.h"  // LEX_STRING
#include "my_inttypes.h"
#include "sql/mem_root_array.h"  // MEM_ROOT
#include "sql/parse_tree_nodes.h"
#include "sql/sql_cmd_ddl_table.h"  // Sql_cmd_create_table

#include "sql/sequence_common.h"  // Sequence_info

/**
  Sequence engine as the builtin plugin.
  The base table engine now only support InnoDB plugin.
*/

/**
  The struture of SEQUENCE options parser
*/
template <Sequence_field field_num, typename FIELD_TYPE = ulonglong>
class PT_values_create_sequence_option : public PT_create_table_option {
 public:
  explicit PT_values_create_sequence_option(FIELD_TYPE value) : value(value) {}

  virtual ~PT_values_create_sequence_option() {}

  bool contextualize(Table_ddl_parse_context *pc) override {
    if (PT_create_table_option::contextualize(pc)) return true;
    pc->sequence_info->init_value(field_num, value);
    return false;
  }

 private:
  FIELD_TYPE value;
};

class PT_create_sequence_stmt : public PT_create_table_stmt {
 public:
  /**
    @param mem_root                   MEM_ROOT to use for allocation
    @param only_if_not_exists  True if @SQL{CREATE %TABLE ... @B{IF NOT EXISTS}}
    @param table_name                 @SQL{CREATE %TABLE ... @B{@<table name@>}}
    @param on_duplicate               DUPLICATE, IGNORE or fail with an error
                                      on data duplication errors (relevant
                                      for @SQL{CREATE TABLE ... SELECT}
                                      statements).
    @param opt_create_sequence_options
                                      For @SQL {CREATE SEQUENCE ...}
  */
  explicit PT_create_sequence_stmt(
      MEM_ROOT *mem_root, bool only_if_not_exists, Table_ident *table_name,
      On_duplicate on_duplicate, const Mem_root_array<PT_create_table_option *>
                                     *opt_create_sequence_options)
      : PT_create_table_stmt(mem_root, false, only_if_not_exists, table_name,
                             NULL, NULL, NULL, on_duplicate, NULL),
        opt_create_sequence_options(opt_create_sequence_options) {}

  /**
    CREATE SEQUENCE statement command

    @param[in]    thd         Connection context

    @retval       Sql_cmd     SQL command
  */
  Sql_cmd *make_cmd(THD *thd) override;
  /**
    Prepare sequence base engine

    @param[in]    thd         Connection context
    @param[in]    table       TABLE_LIST

    @retval       false       success
    @retval       true        failure
  */
  bool prepare_sequence_engine(const THD *thd, const TABLE_LIST *table);
  /**
    Prepare and check sequence table columns

    @param[in]    thd         Connection context

    @retval       false       success
    @retval       true        failure
  */
  bool prepare_sequence_fields(const THD *thd);
  /**
    Check the fields whether it is consistent with pre-defined.

    @param[in]    alter_info  All the DDL information

    @retval       false       success
    @retval       true        failure
  */
  bool check_sequence_fields(Alter_info *alter_info) const;

 private:
  const Mem_root_array<PT_create_table_option *> *opt_create_sequence_options;
  Sequence_info m_sequence_info;
};

/**
  CREATE SEQUENCE statement cmd
*/
class Sql_cmd_create_sequence : public Sql_cmd_create_table {
  typedef Sql_cmd_create_table super;

 public:
  explicit Sql_cmd_create_sequence(Alter_info *alter_info,
                                   TABLE_LIST *query_expression_tables,
                                   Sequence_info *sequence_info)
      : Sql_cmd_create_table(alter_info, query_expression_tables),
        m_sequence_info(sequence_info) {}

  /* CREATE SEQUENCE is also SQLCOM_CREATE_TABLE */
  enum_sql_command sql_command_code() const override {
    return SQLCOM_CREATE_TABLE;
  }
  bool execute(THD *thd) override;
  bool prepare(THD *thd) override;

 private:
  const Sequence_info *m_sequence_info;
};

class Open_sequence_table_ctx {
 public:
  /**
    Temporary TABLE_LIST object that is used to hold opened table.
  */
  class Table_list_state {
   public:
    /* Used by CREATE SEQUENCE. */
    Table_list_state(THD *thd, TABLE_LIST *table) {
      m_table = new (thd->mem_root) TABLE_LIST();
      m_table->init_one_table(table->db, table->db_length, table->table_name,
                              table->table_name_length, table->alias,
                              TL_WRITE_CONCURRENT_INSERT, MDL_SHARED_WRITE);
      m_table->open_strategy = TABLE_LIST::OPEN_IF_EXISTS;
      m_table->open_type = OT_BASE_ONLY;
    }

    /* Used by reload sequence share cache */
    Table_list_state(THD *thd, TABLE_SHARE *share) {
      m_table = new (thd->mem_root) TABLE_LIST();
      char *db = (char *)thd->memdup(share->db.str, share->db.length + 1);
      char *table_name = (char *)thd->memdup(share->table_name.str,
                                             share->table_name.length + 1);
      char *alias = (char *)thd->memdup(share->table_name.str,
                                        share->table_name.length + 1);

      m_table->init_one_table(db, share->db.length, table_name,
                              share->table_name.length, alias,
                              TL_WRITE_CONCURRENT_INSERT, MDL_SHARED_WRITE);
      m_table->open_strategy = TABLE_LIST::OPEN_IF_EXISTS;
      m_table->open_type = OT_BASE_ONLY;
      m_table->sequence_scan.set(Sequence_scan::ORIGINAL_SCAN);
    }

    /** The cloned TABLE_LIST will be free when statement end. */
    ~Table_list_state() {}

    TABLE_LIST *cloned_table() const { return m_table; }

   private:
    THD *thd;
    TABLE_LIST *m_table;
  };

  Open_sequence_table_ctx(THD *thd, TABLE_LIST *table_list);

  Open_sequence_table_ctx(THD *thd, TABLE_SHARE *share);

  virtual ~Open_sequence_table_ctx();
  /**
    Open and lock the sequence table.

    @retval       false     success
    @retval       true      failure
  */
  bool open_table();
  /**
    Get the TABLE object

    @retval       table     TABLE object
  */
  TABLE *get_table() const {
    if (m_inherit_table && m_inherit_table->table)
      return m_inherit_table->table;
    else
      return m_state.cloned_table()->table;
  }

 private:
  THD *m_thd;
  TABLE_LIST *m_inherit_table;
  Table_list_state m_state;
};

/**
  When CREATE SQUENCE, beside of creating table structure, also need to insert
  initial row into table.
*/
class Insert_sequence_table_ctx {
 public:
  Insert_sequence_table_ctx(THD *thd, TABLE_LIST *table_list,
                            const Sequence_info *seq_info);
  virtual ~Insert_sequence_table_ctx();
  /**
    Write the sequence initial row.

    @retval     false           success
    @retval     true            failure
  */
  bool write_record();

 private:
  Open_sequence_table_ctx otx;
  THD *m_thd;
  const Sequence_info *m_seq_info;
  bool m_save_binlog_row_based;
};

#endif
