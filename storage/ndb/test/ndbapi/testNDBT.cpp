/*
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <DbUtil.hpp>
#include <AtrtClient.hpp>


int runTestAtrtClient(NDBT_Context* ctx, NDBT_Step* step){
  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters))
    return NDBT_FAILED;

  int i= 0;
  while(clusters.next())
  {
    ndbout << clusters.column("name") << endl;
    if (i++ == 1){
      ndbout << "removing: " << clusters.column("name") << endl;
      clusters.remove();
    }
  }

  clusters.reset();
  while(clusters.next())
  {
    ndbout << clusters.column("name") << endl;
  }

  return NDBT_OK;
}


int runTestDbUtil(NDBT_Context* ctx, NDBT_Step* step){
  DbUtil sql("test");

  {
    // Select all rows from mysql.user
    SqlResultSet result;
    if (!sql.doQuery("SELECT * FROM mysql.user", result))
      return NDBT_FAILED;
    // result.print();

    while(result.next())
    {
      ndbout << result.column("host") << ", "
             << result.column("uSer") << ", "
             << result.columnAsInt("max_updates") << ", "
             << endl;
    }

    result.reset();
    while(result.next())
    {
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
    if (!sql.doQuery("SELECT host, user FROM mysql.user WHERE host=?",
                     args, result))
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

    while(result.next())
    {
    }

    // Select second row from sql_client_test
    Properties args;
    args.put("0", 2);
    if (!sql.doQuery("SELECT * FROM sql_client_test WHERE a=?", args,result))
      return NDBT_FAILED;
    result.print();

    result.reset();
    while(result.next())
    {
      ndbout << "a: " << result.columnAsInt("a") << endl
             << "b: " << result.column("b") << endl
             << "c: " << result.columnAsLong("c") << endl;
      if (result.columnAsInt("a") != 2){
        ndbout << "hepp1" << endl;
        return NDBT_FAILED;
      }

      if (strcmp(result.column("b"), "bye")){
        ndbout << "hepp2" << endl;
        return NDBT_FAILED;
      }

      if (result.columnAsLong("c") != 9000000000ULL){
        ndbout << "hepp3" << endl;
        return NDBT_FAILED;
      }

    }

    if (sql.selectCountTable("sql_client_test") != 2)
    {
      ndbout << "Got wrong count" << endl;
      return NDBT_FAILED;
    }


    if (!sql.doQuery("DROP TABLE sql_client_test"))
      return NDBT_FAILED;

  }

  return NDBT_OK;
}

NDBT_TESTSUITE(testNDBT);
TESTCASE("AtrtClient",
	 "Test AtrtClient class"){
  INITIALIZER(runTestAtrtClient);
}
TESTCASE("DbUtil",
	 "Test DbUtil class"){
  INITIALIZER(runTestDbUtil);
}
NDBT_TESTSUITE_END(testNDBT);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNDBT);
  return testNDBT.execute(argc, argv);
}

