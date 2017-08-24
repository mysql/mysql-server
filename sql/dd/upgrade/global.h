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

#ifndef DD_UPGRADE__GLOBAL_H_INCLUDED
#define DD_UPGRADE__GLOBAL_H_INCLUDED

#include <sys/types.h>

#include "my_inttypes.h"
#include "sql/dd/string_type.h"
#include "sql/item_create.h"
#include "sql/sql_class.h"
#include "sql/sql_servers.h"
#include "sql/table.h"                            // Table_check_intact

class THD;
class Time_zone;

using sql_mode_t= ulonglong;

namespace dd {
namespace upgrade {

const String_type ISL_EXT= ".isl";
const String_type PAR_EXT= ".par";
const String_type OPT_EXT= ".opt";
extern const char *TRN_EXT;
extern const char *TRG_EXT;

const String_type IBD_EXT= ".ibd";
const String_type index_stats= "innodb_index_stats";
const String_type index_stats_backup= "innodb_index_stats_backup57";
const String_type table_stats= "innodb_table_stats";
const String_type table_stats_backup= "innodb_table_stats_backup57";

/**
   RAII for handling open and close of event and proc tables.
*/

class System_table_close_guard
{
  THD* m_thd;
  TABLE *m_table;
  MEM_ROOT *m_mem_root;

public:
  System_table_close_guard(THD *thd, TABLE *table);
  ~System_table_close_guard();
};


/**
  Class to check the system tables we are using from 5.7 are
  not corrupted before migrating the information to new DD.
*/
class Check_table_intact : public Table_check_intact
{
protected:
  void report_error(uint, const char *fmt, ...);
};


/**
   RAII for handling creation context of Events and
   Stored routines.
*/

class Routine_event_context_guard
{
  THD *m_thd;
  sql_mode_t m_sql_mode;
  ::Time_zone *m_saved_time_zone;
  const CHARSET_INFO *m_client_cs;
  const CHARSET_INFO *m_connection_cl;

public:
  Routine_event_context_guard(THD *thd);
  ~Routine_event_context_guard();
};

} // namespace upgrade
} // namespace dd
#endif // DD_UPGRADE__GLOBAL_H_INCLUDED
