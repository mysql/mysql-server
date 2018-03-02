/*
   Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements
#include "sql/ndb_binlog_thread.h"

// Using
#include "sql/current_thd.h"  // current_thd
#include "sql/ndb_local_connection.h"
#include "sql/ndb_log.h"

int Ndb_binlog_thread::do_init() {
  if (!binlog_hooks.register_hooks(do_after_reset_master)) {
    ndb_log_error("Failed to register binlog hooks");
    return 1;
  }
  return 0;
}

int Ndb_binlog_thread::do_deinit() {
  binlog_hooks.unregister_all();
  return 0;
}

/*
  @brief Callback called when RESET MASTER has successfully removed binlog and
  reset index. This means that ndbcluster also need to clear its own binlog
  index(which is stored in the mysql.ndb_binlog_index table).

  @return 0 on sucess
*/
int Ndb_binlog_thread::do_after_reset_master(void*)
{
  DBUG_ENTER("Ndb_binlog_thread::do_after_reset_master");

  // Truncate the mysql.ndb_binlog_index table
  // - if table does not exist ignore the error as it is a
  // "consistent" behavior
  Ndb_local_connection mysqld(current_thd);
  const bool ignore_no_such_table = true;
  if (mysqld.truncate_table("mysql", "ndb_binlog_index",
                            ignore_no_such_table))
  {
    // Failed to truncate table
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}
