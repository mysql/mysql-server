/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "sql/rpl_sys_table_access.h"

#include <stddef.h>
#include <sstream>  // std::ostringstream

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/json_dom.h"  // Json_wrapper
#include "sql/mysqld_thd_manager.h"
#include "sql/rpl_rli.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "sql/transaction.h"

Rpl_sys_table_access::Rpl_sys_table_access(const std::string &schema_name,
                                           const std::string &table_name,
                                           uint max_num_field)
    : m_schema_name(schema_name),
      m_table_name(table_name),
      m_max_num_field(max_num_field) {}

Rpl_sys_table_access::~Rpl_sys_table_access() { close(true); }

bool Rpl_sys_table_access::store_field(Field *field, std::string fld,
                                       CHARSET_INFO *cs) {
  DBUG_TRACE;
  field->set_notnull();
  return field->store(fld.c_str(), fld.length(), cs) != TYPE_OK;
}

bool Rpl_sys_table_access::store_field(Field *field, long long fld,
                                       bool unsigned_val) {
  DBUG_TRACE;
  field->set_notnull();
  return field->store(fld, unsigned_val) != TYPE_OK;
}

bool Rpl_sys_table_access::store_field(Field *field,
                                       const Json_wrapper &wrapper) {
  DBUG_TRACE;
  if (field) {
    field->set_notnull();
    Field_json *json_field = down_cast<Field_json *>(field);
    return json_field->store_json(&wrapper) != TYPE_OK;
  }
  return true;
}

bool Rpl_sys_table_access::get_field(Field *field, std::string &fld,
                                     CHARSET_INFO *cs) {
  DBUG_TRACE;

  char buff[MAX_FIELD_WIDTH];
  String buff_str(buff, sizeof(buff), cs);

  field->val_str(&buff_str);
  fld.assign(buff_str.c_ptr_safe(), buff_str.length());
  return false;
}

bool Rpl_sys_table_access::get_field(Field *field, uint &fld) {
  fld = (uint)field->val_int();
  return false;
}

bool Rpl_sys_table_access::get_field(Field *field, Json_wrapper &fld) {
  DBUG_TRACE;
  if (field && field->type() == MYSQL_TYPE_JSON) {
    Field_json *json_field = down_cast<Field_json *>(field);
    return json_field->val_json(&fld);
  }

  return true;
}

void Rpl_sys_table_access::before_open(THD *) {
  DBUG_TRACE;

  m_flags = (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
             MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY | MYSQL_OPEN_IGNORE_FLUSH |
             MYSQL_LOCK_IGNORE_TIMEOUT | MYSQL_LOCK_RPL_INFO_TABLE);
}

bool Rpl_sys_table_access::open(enum thr_lock_type lock_type) {
  assert(nullptr == m_thd);
  m_current_thd = current_thd;
  m_thd = create_thd();
  if (m_thd == nullptr) {
    set_error();
    return m_error;
  }

  m_thd->set_new_thread_id();
  m_thd->variables.option_bits &= ~OPTION_BIN_LOG;
  m_thd->variables.option_bits &= ~OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
  m_thd->set_skip_readonly_check();

  Global_THD_manager::get_instance()->add_thd(m_thd);

  if (this->open_table(m_thd, m_schema_name, m_table_name, m_max_num_field,
                       lock_type, &m_table, &m_backup)) {
    set_error();
  }

  return m_error;
}

bool Rpl_sys_table_access::close(bool error) {
  if (m_key_deinit) return false;

  if (this->close_table(m_thd, m_table, &m_backup, m_error, true))
    m_error = true;

  if (error || m_error) {
    trans_rollback_stmt(m_thd);
    trans_rollback(m_thd);
  } else {
    m_error = trans_commit_stmt(m_thd) || trans_commit(m_thd);
  }

  m_thd->release_resources();
  Global_THD_manager::get_instance()->remove_thd(m_thd);
  drop_thd(m_thd);
  m_thd = nullptr;
  if (m_current_thd) m_current_thd->store_globals();

  m_key_deinit = true;
  return m_error;
}

void Rpl_sys_table_access::handler_write_row_func(
    Rpl_sys_table_access &table_op, bool &err_val, std::string &err_msg, uint,
    key_part_map) {
  TABLE *table = table_op.get_table();

  int error = table->file->ha_write_row(table->record[0]);
  if (error) {
    table->file->print_error(error, MYF(0));
    err_msg.assign("Error inserting row to the table.");
    err_val = true;
  }
}

void Rpl_sys_table_access::handler_delete_row_func(
    Rpl_sys_table_access &table_op, bool &err_val, std::string &err_msg,
    uint table_index, key_part_map keypart_map) {
  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  int error = 0;
  int key_error =
      key_access.init(table, table_index, true, keypart_map, HA_READ_KEY_EXACT);

  if (key_error == HA_ERR_KEY_NOT_FOUND) {
    err_msg.assign("Error no matching row was found to be deleted.");
    err_val = true;
  }

  if (!key_error) {
    do {
      error = table->file->ha_delete_row(table->record[0]);
    } while (!key_access.next() && !error);
  }

  if (error) {
    err_val = true;
    err_msg.assign("Error deleting row from the table.");
    table->file->print_error(error, MYF(0));
  }

  if (key_access.deinit()) {
    err_msg.assign("Error ending key access.");
    err_val = true;
  }
}

std::string Rpl_sys_table_access::get_field_error_msg(
    std::string field_name) const {
  std::ostringstream str_stream;
  str_stream << "Error saving " << field_name << " field of " << m_schema_name
             << "." << m_table_name << ".";
  return str_stream.str();
}
