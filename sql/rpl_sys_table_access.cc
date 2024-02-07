/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_sys_table_access.h"

#include <stddef.h>
#include <sstream>  // std::ostringstream

#include "my_dbug.h"
#include "sql-common/json_dom.h"  // Json_wrapper
#include "sql/current_thd.h"
#include "sql/log.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "sql/transaction.h"

struct CHARSET_INFO;

Rpl_sys_table_access::Rpl_sys_table_access(const std::string &schema_name,
                                           const std::string &table_name,
                                           uint max_num_field)
    : m_lock_type(TL_READ),
      m_schema_name(schema_name),
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

bool Rpl_sys_table_access::open(enum thr_lock_type lock_type) {
  assert(nullptr == m_thd);
  m_lock_type = lock_type;
  m_current_thd = current_thd;
  m_error = false;

  THD *thd = nullptr;
  thd = new THD;
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  thd->security_context()->skip_grants();
  thd->system_thread = SYSTEM_THREAD_BACKGROUND;
  thd->set_new_thread_id();
  thd->variables.option_bits &= ~OPTION_BIN_LOG;
  thd->variables.option_bits &= ~OPTION_AUTOCOMMIT;
  thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
  thd->set_skip_readonly_check();
  m_thd = thd;

  // m_table_list[0] is m_schema_name.m_table_name
  // m_table_list[1] is m_schema_version_name.m_table_version_name
  m_table_list = new Table_ref[m_table_list_size];

  Table_ref *table_version = &m_table_list[m_table_version_index];
  *table_version =
      Table_ref(m_schema_version_name.c_str(), m_schema_version_name.length(),
                m_table_version_name.c_str(), m_table_version_name.length(),
                m_table_version_name.c_str(), m_lock_type);
  table_version->open_strategy = Table_ref::OPEN_IF_EXISTS;
  table_version->next_local = nullptr;
  table_version->next_global = nullptr;

  Table_ref *table_data = &m_table_list[m_table_data_index];
  *table_data = Table_ref(m_schema_name.c_str(), m_schema_name.length(),
                          m_table_name.c_str(), m_table_name.length(),
                          m_table_name.c_str(), m_lock_type);
  table_data->open_strategy = Table_ref::OPEN_IF_EXISTS;
  table_data->next_local = table_version;
  table_data->next_global = table_version;

  uint flags =
      (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK | MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY |
       MYSQL_OPEN_IGNORE_FLUSH | MYSQL_LOCK_IGNORE_TIMEOUT);
  if (open_and_lock_tables(m_thd, m_table_list, flags)) {
    /* purecov: begin inspected */
    m_error = true;
    goto err;
    /* purecov: end */
  }

  if (table_data->table->s->fields < m_max_num_field) {
    /* purecov: begin inspected */
    m_error = true;
    goto err;
    /* purecov: end */
  }

  table_version->table->use_all_columns();
  table_data->table->use_all_columns();

err:
  if (m_error) {
    /* purecov: begin inspected */
    close(true);
    /* purecov: end */
  }

  return m_error;
}

bool Rpl_sys_table_access::close(bool error, bool ignore_global_read_lock) {
  DBUG_EXECUTE_IF("force_error_on_configuration_table_close", m_error = true;);

  if (!m_thd) return false;

  if (error || m_error) {
    trans_rollback_stmt(m_thd);
    trans_rollback(m_thd);
    m_error = true;
  } else {
    m_error = trans_commit_stmt(m_thd, ignore_global_read_lock) ||
              trans_commit(m_thd, ignore_global_read_lock);
  }

  close_thread_tables(m_thd);
  delete[] m_table_list;
  m_table_list = nullptr;

  m_thd->release_resources();
  delete m_thd;
  m_thd = nullptr;

  if (m_current_thd) m_current_thd->store_globals();
  m_current_thd = nullptr;

  m_lock_type = TL_READ;

  return m_error;
}

TABLE *Rpl_sys_table_access::get_table() {
  if (nullptr != m_table_list) {
    return m_table_list[m_table_data_index].table;
  }

  return nullptr; /* purecov: inspected */
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

bool Rpl_sys_table_access::delete_all_rows() {
  bool error = false;

  TABLE *table = get_table();
  Rpl_sys_key_access key_access;
  int key_error =
      key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT);
  if (!key_error) {
    do {
      error |= (table->file->ha_delete_row(table->record[0]) != 0);
      if (error) {
        return true;
      }
    } while (!error && !key_access.next());
  } else if (HA_ERR_END_OF_FILE == key_error) {
    /* Table is already empty, nothing to delete. */
  } else {
    return true;
  }

  return key_access.deinit();
}

bool Rpl_sys_table_access::increment_version() {
  DBUG_TRACE;
  assert(m_lock_type >= TL_WRITE_ALLOW_WRITE);
  longlong version = 0;

  TABLE *table_version = m_table_list[m_table_version_index].table;
  Field **fields = table_version->field;
  fields[0]->set_notnull();
  fields[0]->store(m_table_name.c_str(), m_table_name.length(),
                   &my_charset_bin);

  Rpl_sys_key_access key_access;
  int error = key_access.init(table_version);

  if (HA_ERR_KEY_NOT_FOUND == error) {
    /* purecov: begin inspected */
    error = 0;
    version = 1;
    /* purecov: end */
  } else if (error) {
    return true; /* purecov: inspected */
  } else {
    version = 1 + fields[1]->val_int();
    error |= table_version->file->ha_delete_row(table_version->record[0]);
  }

  if (!error) {
    fields[1]->set_notnull();
    fields[1]->store(version, true);
    error |= table_version->file->ha_write_row(table_version->record[0]);
  }

  error |= (key_access.deinit() != 0);

  return error;
}

bool Rpl_sys_table_access::update_version(ulonglong version) {
  DBUG_TRACE;
  assert(m_lock_type >= TL_WRITE_ALLOW_WRITE);
  assert(version > 0);

  TABLE *table_version = m_table_list[m_table_version_index].table;
  Field **fields = table_version->field;
  fields[0]->set_notnull();
  fields[0]->store(m_table_name.c_str(), m_table_name.length(),
                   &my_charset_bin);

  Rpl_sys_key_access key_access;
  int error = key_access.init(table_version);

  if (HA_ERR_KEY_NOT_FOUND == error) {
    error = 0; /* purecov: inspected */
  } else if (error) {
    return true; /* purecov: inspected */
  } else {
    error |= table_version->file->ha_delete_row(table_version->record[0]);
  }

  if (!error) {
    fields[1]->set_notnull();
    fields[1]->store(version, true);
    error |= table_version->file->ha_write_row(table_version->record[0]);
  }

  error |= (key_access.deinit() != 0);

  return error;
}

ulonglong Rpl_sys_table_access::get_version() {
  DBUG_TRACE;
  longlong version = 0;

  TABLE *table_version = m_table_list[m_table_version_index].table;
  Field **fields = table_version->field;
  fields[0]->set_notnull();
  fields[0]->store(m_table_name.c_str(), m_table_name.length(),
                   &my_charset_bin);

  Rpl_sys_key_access key_access;
  int error = key_access.init(table_version);

  if (!error) {
    version = fields[1]->val_int();
  }

  key_access.deinit();

  return version;
}

bool Rpl_sys_table_access::delete_version() {
  DBUG_TRACE;
  assert(m_lock_type >= TL_WRITE_ALLOW_WRITE);

  TABLE *table_version = m_table_list[m_table_version_index].table;
  Field **fields = table_version->field;
  fields[0]->set_notnull();
  fields[0]->store(m_table_name.c_str(), m_table_name.length(),
                   &my_charset_bin);

  Rpl_sys_key_access key_access;
  int error = key_access.init(table_version);

  if (HA_ERR_KEY_NOT_FOUND == error) {
    error = 0; /* purecov: inspected */
  } else if (error) {
    return true; /* purecov: inspected */
  } else {
    error |= table_version->file->ha_delete_row(table_version->record[0]);
  }

  error |= (key_access.deinit() != 0);

  return error;
}
