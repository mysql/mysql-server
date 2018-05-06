/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/sql_service/sql_command_test.h"

/*
  The basic test method to check for the execution of the CRUD command.

  Case 1 - Test the creation of the create command.
           Creates two tables in the test database. Does a select and check
           that the tables exist.

  Case 2 - Test the creation of the insert command.
           Insert values in the tables. Do a select to see that the values
           exist in the table.

  Case 3 - Test the creation of the update command.
           Update the values inserted in the Case 2. Do a select to see that
           the new values are now there in the table.

  Case 4 - Test the creation of the delete command.
           Delete values from the table. Do a select to see the values do not
           exist. Drop the table and verify that the tables are deleted.
*/

void check_sql_command_create(Sql_service_interface *srvi) {
  Sql_resultset rset;
  int srv_err =
      srvi->execute_query("CREATE TABLE test.t1 (i INT PRIMARY KEY NOT NULL);");
  if (srv_err == 0) {
    srvi->execute_query("SHOW TABLES IN test;", &rset);
    std::string str = "t1";
    DBUG_ASSERT(rset.getString(0) == str);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_QUERY_FAIL,
                 srv_err); /* purecov: inspected */
  }
}

void check_sql_command_insert(Sql_service_interface *srvi) {
  Sql_resultset rset;
  int srv_err;
  srv_err = srvi->execute_query("INSERT INTO test.t1 VALUES(1);");
  srv_err = srvi->execute_query("INSERT INTO test.t1 VALUES(2);");
  srv_err = srvi->execute_query("INSERT INTO test.t1 VALUES(3);");
  if (srv_err == 0) {
    srvi->execute_query("SELECT * FROM test.t1", &rset);
    uint i = 0;
    std::vector<std::string> insert_values;
    insert_values.push_back("1");
    insert_values.push_back("2");
    insert_values.push_back("3");
    DBUG_ASSERT(rset.get_rows() == 3);
    while (i < rset.get_rows()) {
      DBUG_ASSERT(rset.getString(0) == insert_values[i]);
      rset.next();
      i++;
    }
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_QUERY_FAIL,
                 srv_err); /* purecov: inspected */
  }
}

void check_sql_command_update(Sql_service_interface *srvi) {
  Sql_resultset rset;
  int srv_err;
  srv_err = srvi->execute_query("UPDATE test.t1 SET i=4 WHERE i=1;");
  srv_err = srvi->execute_query("UPDATE test.t1 SET i=5 WHERE i=2;");
  srv_err = srvi->execute_query("UPDATE test.t1 SET i=6 WHERE i=3;");
  if (srv_err == 0) {
    srvi->execute_query("SELECT * FROM test.t1", &rset);
    uint i = 0;
    std::vector<std::string> update_values;
    update_values.push_back("4");
    update_values.push_back("5");
    update_values.push_back("6");
    DBUG_ASSERT(rset.get_rows() == 3);
    while (i < rset.get_rows()) {
      DBUG_ASSERT(rset.getString(0) == update_values[i]);
      rset.next();
      i++;
    }
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_QUERY_FAIL,
                 srv_err); /* purecov: inspected */
  }
}

void check_sql_command_drop(Sql_service_interface *srvi) {
  Sql_resultset rset;
  int srv_err = srvi->execute_query("DROP TABLE test.t1;");
  if (srv_err == 0) {
    srvi->execute_query("SELECT TABLES IN test", &rset);
    std::string str = "t1";
    DBUG_ASSERT(rset.get_rows() == 0);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_QUERY_FAIL,
                 srv_err); /* purecov: inspected */
  }
}

int sql_command_check() {
  int error = 1;
  Sql_service_interface *srvi = new Sql_service_interface();

  if (srvi == NULL) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_CREATE_SESSION_UNABLE); /* purecov: inspected */
    return error;
    /* purecov: end */
  }

  error = srvi->open_session();
  DBUG_ASSERT(!error);

  /* Case 1 */

  check_sql_command_create(srvi);

  /* Case 2 */

  check_sql_command_insert(srvi);

  /* Case 3 */

  check_sql_command_update(srvi);

  /* Case 4 */

  check_sql_command_drop(srvi);

  delete srvi;
  return error;
}
