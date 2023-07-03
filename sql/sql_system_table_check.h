/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_SYSTEM_TABLE_CHECK_INCLUDED
#define SQL_SYSTEM_TABLE_CHECK_INCLUDED

#include "my_loglevel.h"                             // enum loglevel
#include "mysql/components/services/log_builtins.h"  // LogErr, LogEvent
#include "mysqld_error.h"                            // ER_*
#include "sql/table.h"                               // Table_check_intact

class THD;

/**
  Class to check if system table is intact.
*/
class System_table_intact : public Table_check_intact {
 public:
  explicit System_table_intact(THD *thd, enum loglevel log_level = ERROR_LEVEL)
      : m_thd(thd), m_log_level(log_level) {
    has_keys = true;
  }

  THD *thd() { return m_thd; }

 protected:
  void report_error(uint code, const char *fmt, ...) override
      MY_ATTRIBUTE((format(printf, 3, 4))) {
    va_list args;
    va_start(args, fmt);

    if (code == 0)
      LogEvent()
          .prio(WARNING_LEVEL)
          .errcode(ER_SERVER_TABLE_CHECK_FAILED)
          .subsys(LOG_SUBSYSTEM_TAG)
          .source_file(MY_BASENAME)
          .messagev(fmt, args);
    else if (code == ER_CANNOT_LOAD_FROM_TABLE_V2) {
      char *db_name, *table_name;
      db_name = va_arg(args, char *);
      table_name = va_arg(args, char *);
      my_error(code, MYF(0), db_name, table_name);
      LogErr(m_log_level, ER_SERVER_CANNOT_LOAD_FROM_TABLE_V2, db_name,
             table_name);
    } else {
      my_printv_error(code, ER_THD_NONCONST(m_thd, code), MYF(0), args);
      va_end(args);

      if (code == ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2)
        code = ER_SERVER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2;
      else if (code == ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2)
        code = ER_SERVER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2;
      else
        code = ER_SERVER_ACL_TABLE_ERROR;

      va_start(args, fmt);
      LogEvent()
          .prio(m_log_level)
          .errcode(code)
          .subsys(LOG_SUBSYSTEM_TAG)
          .source_file(MY_BASENAME)
          .messagev(fmt, args);
    }

    va_end(args);
  }

 private:
  THD *m_thd;
  enum loglevel m_log_level;
};
#endif  // SQL_SYSTEM_TABLE_CHECK_INCLUDED
