/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef THD_RAII_INCLUDED
#define THD_RAII_INCLUDED 1

/**
 * @file thd_raii.h
 * Some RAII classes that set THD state.
 */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/query_options.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"

/*************************************************************************/

/** RAII class for temporarily turning off @@autocommit in the connection. */

class Disable_autocommit_guard {
 public:
  /**
    @param thd  non-NULL - pointer to the context of connection in which
                           @@autocommit mode needs to be disabled.
                NULL     - if @@autocommit mode needs to be left as is.
  */
  Disable_autocommit_guard(THD *thd)
      : m_thd(thd), m_save_option_bits(thd ? thd->variables.option_bits : 0) {
    if (m_thd) {
      /*
        We can't disable auto-commit if there is ongoing transaction as this
        might easily break statement/session transaction invariants.
      */
      DBUG_ASSERT(m_thd->get_transaction()->is_empty(Transaction_ctx::STMT) &&
                  m_thd->get_transaction()->is_empty(Transaction_ctx::SESSION));

      m_thd->variables.option_bits &= ~OPTION_AUTOCOMMIT;
      m_thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
    }
  }

  ~Disable_autocommit_guard() {
    if (m_thd) {
      /*
        Both session and statement transactions need to be finished by the
        time when we enable auto-commit mode back.
      */
      DBUG_ASSERT(m_thd->get_transaction()->is_empty(Transaction_ctx::STMT) &&
                  m_thd->get_transaction()->is_empty(Transaction_ctx::SESSION));
      m_thd->variables.option_bits = m_save_option_bits;
    }
  }

 private:
  THD *m_thd;
  ulonglong m_save_option_bits;
};

/**
  RAII class which allows to temporary disable updating Gtid_state.
*/

class Disable_gtid_state_update_guard {
 public:
  Disable_gtid_state_update_guard(THD *thd)
      : m_thd(thd),
        m_save_is_operating_substatement_implicitly(
            thd->is_operating_substatement_implicitly),
        m_save_skip_gtid_rollback(thd->skip_gtid_rollback) {
    m_thd->is_operating_substatement_implicitly = true;
    m_thd->skip_gtid_rollback = true;
  }

  ~Disable_gtid_state_update_guard() {
    m_thd->is_operating_substatement_implicitly =
        m_save_is_operating_substatement_implicitly;
    m_thd->skip_gtid_rollback = m_save_skip_gtid_rollback;
  }

 private:
  THD *m_thd;
  bool m_save_is_operating_substatement_implicitly;
  bool m_save_skip_gtid_rollback;
};

/**
  RAII class to temporarily disable binlogging.
*/

class Disable_binlog_guard {
 public:
  Disable_binlog_guard(THD *thd)
      : m_thd(thd),
        m_binlog_disabled(thd->variables.option_bits & OPTION_BIN_LOG) {
    thd->variables.option_bits &= ~OPTION_BIN_LOG;
  }

  ~Disable_binlog_guard() {
    if (m_binlog_disabled) m_thd->variables.option_bits |= OPTION_BIN_LOG;
  }

 private:
  THD *const m_thd;
  const bool m_binlog_disabled;
};

/**
  RAII class which allows to save, clear and store binlog format state
  There are two variables in THD class that will decide the binlog
  format of a statement
    i) THD::current_stmt_binlog_format
   ii) THD::variables.binlog_format
  Saving or Clearing or Storing of binlog format state should be done
  for these two variables together all the time.
*/
class Save_and_Restore_binlog_format_state {
 public:
  Save_and_Restore_binlog_format_state(THD *thd)
      : m_thd(thd),
        m_global_binlog_format(thd->variables.binlog_format),
        m_current_stmt_binlog_format(BINLOG_FORMAT_STMT) {
    if (thd->is_current_stmt_binlog_format_row())
      m_current_stmt_binlog_format = BINLOG_FORMAT_ROW;

    thd->variables.binlog_format = BINLOG_FORMAT_STMT;
    thd->clear_current_stmt_binlog_format_row();
  }

  ~Save_and_Restore_binlog_format_state() {
    DBUG_ASSERT(!m_thd->is_current_stmt_binlog_format_row());
    m_thd->variables.binlog_format = m_global_binlog_format;
    if (m_current_stmt_binlog_format == BINLOG_FORMAT_ROW)
      m_thd->set_current_stmt_binlog_format_row();
  }

 private:
  THD *m_thd;
  ulong m_global_binlog_format;
  enum_binlog_format m_current_stmt_binlog_format;
};

/**
  RAII class to temporarily turn off SQL modes that affect parsing
  of expressions. Can also be used when printing expressions even
  if it turns off more SQL modes than strictly necessary for it
  (these extra modes are harmless as they do not affect expression
  printing).
*/
class Sql_mode_parse_guard {
 public:
  Sql_mode_parse_guard(THD *thd)
      : m_thd(thd), m_old_sql_mode(thd->variables.sql_mode) {
    /*
      Switch off modes which can prevent normal parsing of expressions:

      - MODE_REAL_AS_FLOAT            affect only CREATE TABLE parsing
      + MODE_PIPES_AS_CONCAT          affect expression parsing
      + MODE_ANSI_QUOTES              affect expression parsing
      + MODE_IGNORE_SPACE             affect expression parsing
      - MODE_NOT_USED                 not used :)
      * MODE_ONLY_FULL_GROUP_BY       affect execution
      * MODE_NO_UNSIGNED_SUBTRACTION  affect execution
      - MODE_NO_DIR_IN_CREATE         affect table creation only
      - MODE_POSTGRESQL               compounded from other modes
      - MODE_ORACLE                   compounded from other modes
      - MODE_MSSQL                    compounded from other modes
      - MODE_DB2                      compounded from other modes
      - MODE_MAXDB                    affect only CREATE TABLE parsing
      - MODE_NO_KEY_OPTIONS           affect only SHOW
      - MODE_NO_TABLE_OPTIONS         affect only SHOW
      - MODE_NO_FIELD_OPTIONS         affect only SHOW
      - MODE_MYSQL323                 affect only SHOW
      - MODE_MYSQL40                  affect only SHOW
      - MODE_ANSI                     compounded from other modes
                                      (+ transaction mode)
      ? MODE_NO_AUTO_VALUE_ON_ZERO    affect UPDATEs
      + MODE_NO_BACKSLASH_ESCAPES     affect expression parsing
    */
    thd->variables.sql_mode &= ~(MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
                                 MODE_IGNORE_SPACE | MODE_NO_BACKSLASH_ESCAPES);
  }

  ~Sql_mode_parse_guard() { m_thd->variables.sql_mode = m_old_sql_mode; }

 private:
  THD *m_thd;
  const sql_mode_t m_old_sql_mode;
};

/**
  RAII class to temporarily swap thd->mem_root to a different mem_root.
*/
class Swap_mem_root_guard {
 public:
  Swap_mem_root_guard(THD *thd, MEM_ROOT *mem_root)
      : m_thd(thd), m_old_mem_root(thd->mem_root) {
    thd->mem_root = mem_root;
  }

  ~Swap_mem_root_guard() { m_thd->mem_root = m_old_mem_root; }

 private:
  THD *m_thd;
  MEM_ROOT *m_old_mem_root;
};

#endif  // THD_RAII_INCLUDED
