/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/* Implementation of SHOW commands. */

#include "sql_cmd_show.h"

#include "sql_parse.h" // execute_sqlcom_select

/*
  MAINTAINER:
  This code is a backport from MySQL 8.0 into MySQL 5.7,
  stripped down to the bare minimum.

  See the full implementation in 8.0
*/

bool Sql_cmd_show::execute(THD *thd) {
  if (check_privileges(thd)) {
    return true;
  }

  return execute_inner(thd);
}

bool Sql_cmd_show::check_privileges(THD *thd) {
  LEX *lex = thd->lex;
  if (lex->query_tables == NULL) return false;
  // If SHOW command is represented by a plan, ensure user has privileges:
  return check_table_access(thd, SELECT_ACL, lex->query_tables, false, UINT_MAX,
                            false);
}

bool Sql_cmd_show::execute_inner(THD *thd) {
   return execute_sqlcom_select(thd, thd->lex->query_tables);
}

bool Sql_cmd_show_processlist::check_privileges(THD *thd) {
  if (!thd->security_context()->priv_user().str[0] &&
      check_global_access(thd, PROCESS_ACL)) {
    return true;
  }
  return Sql_cmd_show::check_privileges(thd);
}

bool Sql_cmd_show_processlist::execute_inner(THD *thd) {
  bool rc;
  /*
    If the Performance Schema is configured to support SHOW PROCESSLIST,
    then execute a query on performance_schema.processlist. Otherwise,
    fall back to the legacy method.
  */
  if (use_pfs()) {
    DEBUG_SYNC(thd, "pfs_show_processlist_performance_schema");
    rc = Sql_cmd_show::execute_inner(thd);
  } else {
    DEBUG_SYNC(thd, "pfs_show_processlist_legacy");
    mysqld_list_processes(thd,
                          thd->security_context()->check_access(PROCESS_ACL)
                              ? NullS
                              : thd->security_context()->priv_user().str,
                          m_verbose);
    rc = false;
  }
  return rc;
}
