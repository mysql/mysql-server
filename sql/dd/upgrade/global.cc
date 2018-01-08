/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/upgrade/global.h"

#include <stdarg.h>

#include "my_loglevel.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "sql/handler.h"
#include "sql/log.h"                           // LogErr
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"

namespace dd {
namespace upgrade_57 {

const char *TRN_EXT= ".TRN";
const char *TRG_EXT= ".TRG";

System_table_close_guard::System_table_close_guard(THD *thd, TABLE *table)
{
  m_thd= thd;
  m_table= table;
  m_mem_root= m_thd->mem_root;
}

System_table_close_guard::~System_table_close_guard()
{
  m_thd->mem_root= m_mem_root;
  if (m_table->file->inited)
    (void) m_table->file->ha_index_end();
  close_thread_tables(m_thd);
}


void Check_table_intact::report_error(uint, const char *fmt, ...)
{
  va_list args;
  char buff[MYSQL_ERRMSG_SIZE];
  va_start(args, fmt);
  my_vsnprintf(buff, sizeof(buff), fmt, args);
  va_end(args);

  LogErr(ERROR_LEVEL, ER_DD_UPGRADE_TABLE_INTACT_ERROR, buff);
}

Routine_event_context_guard:: Routine_event_context_guard(THD *thd): m_thd(thd)
{
  m_thd= thd;
  m_sql_mode= m_thd->variables.sql_mode;
  m_client_cs= m_thd->variables.character_set_client;
  m_connection_cl= m_thd->variables.collation_connection;
  m_saved_time_zone= m_thd->variables.time_zone;
}
Routine_event_context_guard::~Routine_event_context_guard()
{
  m_thd->variables.sql_mode= m_sql_mode;
  m_thd->variables.character_set_client= m_client_cs;
  m_thd->variables.collation_connection= m_connection_cl;
  m_thd->variables.time_zone= m_saved_time_zone;
}

bool Bootstrap_error_handler::m_log_error= true;
bool Bootstrap_error_handler::abort_on_error= false;
} // namespace upgrade
} // namespace dd
