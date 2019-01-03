/* Copyright (c) 2015, 2018, Alibaba and/or its affiliates. All rights reserved.

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

/**
  @file

  Implementation of SEQUENCE object

  CREATE SEQUENCE syntax:

    CREATE SEQUENCE [IF NOT EXISTS] schema.seqName
     [START WITH <constant>]
     [MINVALUE <constant>]
     [MAXVALUE <constant>]
     [INCREMENT BY <constant>]
     [CACHE <constant> | NOCACHE]
     [CYCLE | NOCYCLE]
    ;
  Or:
    CREATE TABLE `s` (
      `currval` bigint(21) NOT NULL COMMENT 'current value',
      `nextval` bigint(21) NOT NULL COMMENT 'next value',
      `minvalue` bigint(21) NOT NULL COMMENT 'min value',
      `maxvalue` bigint(21) NOT NULL COMMENT 'max value',
      `start` bigint(21) NOT NULL COMMENT 'start value',
      `increment` bigint(21) NOT NULL COMMENT 'increment value',
      `cache` bigint(21) NOT NULL COMMENT 'cache size',
      `cycle` bigint(21) NOT NULL COMMENT 'cycle state',
      `round` bigint(21) NOT NULL COMMENT 'already how many round'
    ) ENGINE=Sequence DEFAULT CHARSET=latin1
    insert into s values(0,0,1,10,1,1,2,0,0);
    commit;

  SHOW syntax:

    SHOW CREATE TABLE schema.seqName;

  QUERY syntax:   

    1.   
      SELECT [nextval | currval | *] FROM schema.seqName;

    2. TODO: 
      SELECT seqName.nextval from dual;
      SELECT seqName.currval from dual;

    3.
      SELECT NEXTVAL(seq);
      SELECT CURRVAL(seq);

  Usage:
    use test; create sequence s;
    create table t(id int);
    insert into t values (nextval(s))
*/

/**
  @addtogroup Sequence Engine

  Sequence Engine syntax statement implementation.

  @{
*/
#include <limits>
#include "sql/dd/types/abstract_table.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/parse_tree_column_attrs.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_table.h"
#include "sql/transaction.h"

#include "sql/sequence_common.h"
#include "sql/sql_sequence.h"
/**
  Prepare sequence base engine.

  @param[in]    thd         Connection context
  @param[in]    table       TABLE_LIST object

  @retval       false       success
  @retval       true        failure
*/
bool PT_create_sequence_stmt::prepare_sequence_engine(const THD *thd,
                                                      const TABLE_LIST *table) {
  DBUG_ENTER("PT_create_sequence_stmt::prepare_sequence_engine");

  /* Step 1: prepare the db and table_name */
  m_sequence_info.db = table->db;
  m_sequence_info.table_name = table->table_name;

  /* Step 2: prepare the base engine */
  plugin_ref sequence_plugin = ha_resolve_sequence(thd);
  plugin_ref base_plugin = ha_resolve_sequence_base(thd);

  DBUG_EXECUTE_IF("sequence_engine_error", { sequence_plugin = NULL; });

  if (sequence_plugin == nullptr ||
      plugin_data<handlerton *>(sequence_plugin) == nullptr) {
    my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), SEQUENCE_ENGINE_NAME.str);
    DBUG_RETURN(true);
  }
  /* We will change the create_info.db_type when create_table_impl() */
  if (!((m_create_info.db_type = m_sequence_info.base_db_type =
             (base_plugin != nullptr ? plugin_data<handlerton *>(base_plugin)
                                     : nullptr)))) {
    my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), SEQUENCE_BASE_ENGINE_NAME.str);
    DBUG_RETURN(true);
  }

  m_create_info.used_fields |= HA_CREATE_USED_ENGINE;

  DBUG_RETURN(false);
}

/**
  Prepare and check sequence table columns

  @param[in]    thd         Connection context

  @retval       false       success
  @retval       true        failure
*/
bool PT_create_sequence_stmt::prepare_sequence_fields(const THD *thd) {
  const st_sequence_field_info *field_def;
  MEM_ROOT *mem_root;
  LEX_STRING field_name;
  size_t field_name_len;
  PT_column_attr_base *not_null_attr;
  PT_column_attr_base *comment_attr;
  Mem_root_array<PT_column_attr_base *> *column_attrs;

  PT_type *field_type;
  PT_field_def_base *field_def_base;
  PT_table_element *table_element;
  Mem_root_array<PT_table_element *> *table_element_list;
  DBUG_ENTER("PT_create_sequence_stmt::prepare_sequence_fields");
  DBUG_ASSERT(opt_table_element_list == NULL);
  /**
    Columns definition structure:

      1.-- PT_table_element list
        2.-- PT_column_def
          3.-- Field_name (LEX_STRING)
          3.-- PT_field_def
            4.-- PT_numeric_type
            4.-- PT_column_attr_base list
              5.-- PT_not_null_column_attr
              5.-- PT_comment_column_attr
  */
  field_def = seq_fields;
  mem_root = thd->mem_root;

  /* Native sequence create syntax */
  table_element_list =
      new (mem_root) Mem_root_array<PT_table_element *>(mem_root);
  while (field_def->field_name) {
    /* Column attrs include not_null and comment */
    column_attrs =
        new (mem_root) Mem_root_array<PT_column_attr_base *>(mem_root);
    comment_attr = new (mem_root) PT_comment_column_attr(field_def->comment);
    not_null_attr = new (mem_root) PT_not_null_column_attr;

    column_attrs->push_back(not_null_attr);
    column_attrs->push_back(comment_attr);

    /* Column type is bigint(21) */
    field_type = new (mem_root) PT_numeric_type(
        Int_type::BIGINT, field_def->field_length, Field_option::NONE);
    field_def_base = new (mem_root) PT_field_def(field_type, column_attrs);

    field_name_len = strlen(field_def->field_name);
    field_name.str = thd->strmake(field_def->field_name, field_name_len);
    field_name.length = field_name_len;

    /* Column def and column name constitute column element */
    table_element =
        new (mem_root) PT_column_def(field_name, field_def_base, NULL);
    table_element_list->push_back(table_element);
    field_def++;
  }
  /* Assign the PT_create_table_stmt attribute */
  opt_table_element_list = table_element_list;
  DBUG_RETURN(false);
}
/**
  Check the fields whether they are consistent with pre-defined.

  @param[in]    alter_info  All the DDL information

  @retval       false       success
  @retval       true        failure
*/
bool PT_create_sequence_stmt::check_sequence_fields(
    Alter_info *alter_info) const {
  bool error = false;
  DBUG_ENTER("PT_create_sequence_stmt::check_sequence_fields");

  if ((error = check_sequence_fields_valid(alter_info)))
    my_error(ER_SEQUENCE_INVALID, MYF(0), m_sequence_info.db,
             m_sequence_info.table_name);

  DBUG_RETURN(error);
}
/**
  CREATE SEQUENCE statement command

  @param[in]    thd         Connection context

  @retval       Sql_cmd     SQL command
*/
Sql_cmd *PT_create_sequence_stmt::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  lex->sql_command = SQLCOM_CREATE_TABLE;

  Parse_context pc(thd, lex->current_select());

  TABLE_LIST *table = pc.select->add_table_to_list(
      thd, table_name, NULL, TL_OPTION_UPDATING, TL_WRITE, MDL_SHARED);
  if (table == NULL) return NULL;

  table->open_strategy = TABLE_LIST::OPEN_FOR_CREATE;

  /* Step 1: prepare sequence engine. */
  if (prepare_sequence_engine(thd, table)) return NULL;

  /* Step 2: prepare sequence table columns */
  if (prepare_sequence_fields(thd)) return NULL;

  lex->create_info = &m_create_info;
  lex->sequence_info = &m_sequence_info;
  lex->create_info->sequence_info = &m_sequence_info;

  Table_ddl_parse_context pc2(thd, pc.select, &m_alter_info);

  pc2.create_info->options = 0;
  if (only_if_not_exists)
    pc2.create_info->options |= HA_LEX_CREATE_IF_NOT_EXISTS;

  pc2.create_info->default_table_charset = NULL;

  lex->name.str = 0;
  lex->name.length = 0;

  /* Step 3: Contextualize sequence row values */
  if (opt_create_sequence_options) {
    for (auto option : *opt_create_sequence_options)
      if (option->contextualize(&pc2)) return NULL;
  }
  /* Step 4: check sequence row values */
  if (pc2.sequence_info->check_valid()) return NULL;

  /* Step 5: Contextualize sequence columns */
  TABLE_LIST *qe_tables = nullptr;
  if (opt_table_element_list) {
    for (auto element : *opt_table_element_list) {
      if (element->contextualize(&pc2)) return NULL;
    }
  }

  if (check_sequence_fields(&m_alter_info)) return NULL;

  switch (on_duplicate) {
    case On_duplicate::IGNORE_DUP:
      lex->set_ignore(true);
      break;
    case On_duplicate::REPLACE_DUP:
      lex->duplicates = DUP_REPLACE;
      break;
    case On_duplicate::ERROR:
      lex->duplicates = DUP_ERROR;
      break;
  }

  DBUG_ASSERT(opt_query_expression == NULL);
  lex->set_current_select(pc.select);

  DBUG_ASSERT((pc2.create_info->used_fields & HA_CREATE_USED_ENGINE) &&
              pc2.create_info->db_type);

  create_table_set_open_action_and_adjust_tables(lex);

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root)
      Sql_cmd_create_sequence(&m_alter_info, qe_tables, &m_sequence_info);
}

/**
  Create the sequence table and insert a row into table.

  @param[in]      thd       User connection context

  @retval         false     Success
  @retval         true      Failure
*/
bool Sql_cmd_create_sequence::execute(THD *thd) {
  LEX *const lex = thd->lex;
  DBUG_ENTER("Sql_cmd_create_sequence::execute");
  DBUG_ASSERT(lex->sequence_info);

  DBUG_RETURN(super::execute(thd));
}

bool Sql_cmd_create_sequence::prepare(THD *thd) { return super::prepare(thd); }

/**
  Construtor of Open_sequence_table_ctx
*/
Open_sequence_table_ctx::Open_sequence_table_ctx(THD *thd,
                                                 TABLE_LIST *table_list)
    : m_thd(thd), m_inherit_table(table_list), m_state(thd, table_list) {}

Open_sequence_table_ctx::Open_sequence_table_ctx(THD *thd, TABLE_SHARE *share)
    : m_thd(thd), m_inherit_table(NULL), m_state(thd, share) {}
/**
  Open and lock the sequence table.

  @retval     false       success
  @retval     true        failure
*/
bool Open_sequence_table_ctx::open_table() {
  DBUG_ENTER("Open_sequence_table_ctx::open_table");

  if (!m_inherit_table || !(m_inherit_table->table)) {
    /**
      Clone a TABLE_LIST from table, then open and lock the table for the
      sequence dml.
    */
    if (open_and_lock_tables(m_thd, m_state.cloned_table(),
                             MYSQL_LOCK_IGNORE_TIMEOUT))
      DBUG_RETURN(true);
  }

  DBUG_ASSERT((m_thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::TABLE, m_state.cloned_table()->db,
      m_state.cloned_table()->table_name, MDL_SHARED_WRITE)));

  /**
    Insert the initial row soon after CREATE SEQUENCE,
    we will inherit transaction context.
  */
  DBUG_RETURN(false);
}

/**
  We will inherit the CREATE SEQUENCE transaction context,
  so didn't need to close opened_table and release MDL explicitly.

  Opened table and MDL wil be released when statement end that will incur
  implicit commit.
*/
Open_sequence_table_ctx::~Open_sequence_table_ctx() {}

/**
  Construtor of Insert_sequence_table_ctx, for saving current context.
*/
Insert_sequence_table_ctx::Insert_sequence_table_ctx(
    THD *thd, TABLE_LIST *table_list, const Sequence_info *seq_info)
    : otx(thd, table_list),
      m_thd(thd),
      m_seq_info(seq_info),
      m_save_binlog_row_based(false) {
  /* Sequence will be replicated by statement, so disable row binlog */
  if ((m_save_binlog_row_based = m_thd->is_current_stmt_binlog_format_row()))
    m_thd->clear_current_stmt_binlog_format_row();
}

Insert_sequence_table_ctx::~Insert_sequence_table_ctx() {
  if (m_save_binlog_row_based) m_thd->set_current_stmt_binlog_format_row();
}
/**
  Write the sequence initial row.

  @retval     false       success
  @retval     true        failure
*/
bool Insert_sequence_table_ctx::write_record() {
  bool error = false;
  TABLE *table;
  st_sequence_field_info *field_info;
  Sequence_field field_num;
  ulonglong field_value;
  DBUG_ENTER("Insert_sequence_table_ctx::write_record");

  if ((error = otx.open_table())) DBUG_RETURN(error);

  table = otx.get_table();

  DBUG_ASSERT(table && table->in_use == m_thd);
  table->use_all_columns();

  field_info = seq_fields;
  while (field_info->field_name) {
    field_num = field_info->field_num;
    field_value = m_seq_info->get_value(field_num);
    table->field[field_num]->store((longlong)(field_value), true);
    field_info++;
  }
  if ((error = table->file->ha_write_row(table->record[0]))) {
    table->file->print_error(error, MYF(0));
  }
  DBUG_RETURN(error);
}

/// @} (end of group Sequence Engine)
