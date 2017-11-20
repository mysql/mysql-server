/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/dd/upgrade/global.h"

#include <stdarg.h>

#include "my_loglevel.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "sql/handler.h"
#include "sql/log.h"                          // sql_print_error
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"

namespace dd {
namespace upgrade {

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

  sql_print_error("%s", buff);
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


} // namespace upgrade
} // namespace dd
