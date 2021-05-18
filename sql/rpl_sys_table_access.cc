/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/mysqld_thd_manager.h"
#include "sql/rpl_rli.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"

#include "sql/transaction.h"

void Rpl_sys_table_access::before_open(THD *) {
  DBUG_TRACE;

  m_flags = (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
             MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY | MYSQL_OPEN_IGNORE_FLUSH |
             MYSQL_LOCK_IGNORE_TIMEOUT | MYSQL_LOCK_RPL_INFO_TABLE);
}

bool Rpl_sys_table_access::init(std::string db, std::string table,
                                uint max_num_field,
                                enum thr_lock_type lock_type) {
  m_current_thd = current_thd;
  m_thd = create_thd();
  if (m_thd == nullptr) {
    set_error();
    return m_error;
  }

  m_thd->set_new_thread_id();

  if (m_current_thd != nullptr) {
    /*
      Copy option bits from original THD to newly created one.
      This allows to take into consideration session settings such as
      sql_log_bin value.
    */
    m_thd->variables.option_bits =
        m_current_thd->variables.option_bits & OPTION_BIN_LOG;
  }

  Global_THD_manager::get_instance()->add_thd(m_thd);

  if (this->open_table(m_thd, db, table, max_num_field, lock_type, &m_table,
                       &m_backup)) {
    set_error();
  }

  return m_error;
}

Rpl_sys_table_access::~Rpl_sys_table_access() { deinit(); }

bool Rpl_sys_table_access::deinit() {
  if (m_key_deinit) return false;

  if (!get_error()) {
    m_error = this->close_table(m_thd, m_table, &m_backup, m_error, true);
  }

  m_thd->release_resources();
  Global_THD_manager::get_instance()->remove_thd(m_thd);
  drop_thd(m_thd);
  if (m_current_thd) m_current_thd->store_globals();

  m_key_deinit = true;
  return m_error;
}
