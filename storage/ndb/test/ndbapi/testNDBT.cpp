/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <AtrtClient.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include "SqlClient.hpp"

// Create the minimal schema required for testing AtrtClient
int runCreateAtrtSchema(NDBT_Context *ctx, NDBT_Step *step) {
  SqlClient sql("");

  if (!sql.doQuery("DROP DATABASE IF EXISTS atrt")) {
    return NDBT_FAILED;
  }

  if (!sql.doQuery("CREATE DATABASE atrt")) {
    return NDBT_FAILED;
  }

  if (!sql.doQuery("CREATE TABLE atrt.cluster ("
                   "   id int primary key,"
                   "   name varchar(255),"
                   "   unique(name)"
                   "   ) engine = innodb")) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

// Drop the minimal atrt schema
int runDropAtrtSchema(NDBT_Context *ctx, NDBT_Step *step) {
  SqlClient sql("");

  if (!sql.doQuery("DROP DATABASE IF EXISTS atrt")) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runTestAtrtClient(NDBT_Context *ctx, NDBT_Step *step) {
  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  int i = 0;
  while (clusters.next()) {
    ndbout << clusters.column("name") << endl;
    if (i++ == 1) {
      ndbout << "removing: " << clusters.column("name") << endl;
      clusters.remove();
    }
  }

  clusters.reset();
  while (clusters.next()) {
    ndbout << clusters.column("name") << endl;
  }

  return NDBT_OK;
}

int runTestSqlClient(NDBT_Context *ctx, NDBT_Step *step) {
  SqlClient sql("test");

  {
    // Select all rows from mysql.user
    SqlResultSet result;
    if (!sql.doQuery("SELECT * FROM mysql.user", result)) return NDBT_FAILED;
    // result.print();

    while (result.next()) {
      ndbout << result.column("host") << ", " << result.column("uSer") << ", "
             << result.columnAsInt("max_updates") << ", " << endl;
    }

    result.reset();
    while (result.next()) {
      ndbout << result.column("host") << endl;
    }
  }

  {
    // No column name, query should fail
    Properties args;
    SqlResultSet result;
    if (sql.doQuery("SELECT * FROM mysql.user WHERE name=?", args, result))
      return NDBT_FAILED;
    result.print();
  }

  {
    // Select nonexisiting rows from mysql.user
    Properties args;
    SqlResultSet result;
    args.put("0", "no_such_host");
    if (!sql.doQuery("SELECT * FROM mysql.user WHERE host=?", args, result))
      return NDBT_FAILED;
    ndbout << "no rows" << endl;
    result.print();

    // Change args to an find one row
    args.clear();
    args.put("0", "localhost");
    if (!sql.doQuery("SELECT host, user FROM mysql.user WHERE host=?", args,
                     result))
      return NDBT_FAILED;
    result.print();
  }

  {
    if (!sql.doQuery("DROP TABLE IF EXISTS sql_client_test"))
      return NDBT_FAILED;

    if (!sql.doQuery("CREATE TABLE sql_client_test"
                     "(a int, b varchar(255), c bigint)"))
      return NDBT_FAILED;

    if (!sql.doQuery("INSERT INTO sql_client_test VALUES"
                     "(1, 'hello', 456456456789),"
                     "(2, 'bye', 9000000000)"))
      return NDBT_FAILED;

    // Select all rows from sql_client_test
    SqlResultSet result;
    if (!sql.doQuery("SELECT * FROM sql_client_test", result))
      return NDBT_FAILED;
    // result.print();

    while (result.next()) {
    }

    {
      auto check_result = [](SqlResultSet &result) {
        result.reset();
        while (result.next()) {
          ndbout << "a: " << result.columnAsInt("a") << endl
                 << "b: " << result.column("b") << endl
                 << "c: " << result.columnAsLong("c") << endl;
          if (result.columnAsInt("a") != 2) {
            ndbout << "Unexpected value for a" << endl;
            return false;
          }

          if (strcmp(result.column("b"), "bye")) {
            ndbout << "Unexpected value for b" << endl;
            return false;
          }

          if (result.columnAsLong("c") != 9000000000ULL) {
            ndbout << "Unexpected value for c" << endl;
            return false;
          }
        }
        return true;
      };

      // Select second row from sql_client_test using placeholders and check
      // expected result, this will use prepared statement behind the scenes
      Properties args;
      args.put("0", 2);
      if (!sql.doQuery("SELECT * FROM sql_client_test WHERE a=?", args, result))
        return NDBT_FAILED;
      result.print();
      if (!check_result(result)) return NDBT_FAILED;

      // Select second row from sql_client_test without placeholders and check
      // expected result
      if (!sql.doQuery("SELECT * FROM sql_client_test WHERE a=2", result))
        return NDBT_FAILED;
      result.print();
      if (!check_result(result)) return NDBT_FAILED;
    }

    if (sql.selectCountTable("sql_client_test") != 2) {
      ndbout << "Got wrong count" << endl;
      return NDBT_FAILED;
    }

    if (!sql.doQuery("DROP TABLE sql_client_test")) return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runTestSqlClientThread(NDBT_Context *ctx, NDBT_Step *step) {
  SqlClient sql("");

  // Select all rows from mysql.user
  SqlResultSet result;
  if (!sql.doQuery("SELECT * FROM mysql.user", result)) return NDBT_FAILED;
  // result.print();

  return NDBT_OK;
}

NDBT_TESTSUITE(testNDBT);

/*
  $> testNDBT -n AtrtClient
*/
TESTCASE("AtrtClient", "Test AtrtClient class") {
  INITIALIZER(runCreateAtrtSchema);
  INITIALIZER(runTestAtrtClient);
  FINALIZER(runDropAtrtSchema);
}
/*
  $> testNDBT -n SqlClient
*/
TESTCASE("SqlClient", "Test SqlClient class") { INITIALIZER(runTestSqlClient); }
TESTCASE("SqlClientThreads", "Test SqlClient class with threads") {
  STEPS(runTestSqlClientThread, 10);
}
NDBT_TESTSUITE_END(testNDBT)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNDBT);
  testNDBT.setCreateTable(false);
  testNDBT.setRunAllTables(true);
  return testNDBT.execute(argc, argv);
}
