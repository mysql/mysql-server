/* 
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#include <NdbRestarts.hpp>


int runCreateTable(NDBT_Context* ctx, NDBT_Step* step){
  DbUtil sql("test");

  if (!sql.doQuery("CREATE TABLE reconnect ("
                   "pk bigint, "
                   "thread int, "
                   "b varchar(32) NOT NULL, "
                   "PRIMARY KEY(pk, thread)"
                   ") engine = NDB;"))
    return NDBT_FAILED;
  return NDBT_OK;
}

int runDropTable(NDBT_Context* ctx, NDBT_Step* step){
  DbUtil sql("test");

  if (!sql.doQuery("DROP TABLE IF EXISTS reconnect"))
    return NDBT_FAILED;
  return NDBT_OK;
}


int runSQLQueries(NDBT_Context* ctx, NDBT_Step* step,
                  const char* query)
{
  int result = -1;
  DbUtil sql("test");

  unsigned failed = 0;
  unsigned i = 0;
  unsigned shutdown_counter= 0;
  while (result == -1)
  {
    Properties args;
    args.put("0", i);
    if (!sql.doQuery(query, args))
    {
      switch(sql.last_errno()){
      case 2006: // MySQL server has gone away(ie. crash)
        g_err << "Fatal error: " << sql.last_error() << endl;
        g_err.print("query: %s", query);
        result = NDBT_FAILED;
        break;
      default:
        // Ignore
        failed++;
        break;
      }
    }
    else
      g_info << query << endl;
    sql.silent(); // Late, to catch any SQL syntax errors
    i++;

    if (ctx->isTestStopped())
    {
      // When the test is stopped we run additional queries
      // that all should work

      if (shutdown_counter == 0)
      {
        shutdown_counter = i;
      }
      else
      {
        unsigned extra_loops= i - shutdown_counter;

        if (extra_loops < 10)
        {
          // Check that last query succeeded
          if (sql.last_errno() != 0)
          {
            g_err << "Fatal error during shutdown queries: "
                  << sql.last_error() << endl;
            g_err.print("query: %s", query);
            result = NDBT_FAILED;
          }
        }
        else
        {
          // We are done, signal success
          result= NDBT_OK;
        }
      }
    }

  }
  ctx->stopTest();
  g_info << i - failed << " queries completed and "
         << failed << " failed" << endl;
  return result;
}


int runINSERT(NDBT_Context* ctx, NDBT_Step* step){
  BaseString query;
  query.assfmt("INSERT INTO reconnect "
               "(pk, thread, b) VALUES (?, %d, 'data%d')",
               step->getStepNo(), step->getStepNo());
  return runSQLQueries(ctx, step, query.c_str());

}


int runSELECT(NDBT_Context* ctx, NDBT_Step* step){
  return runSQLQueries(ctx, step, "SELECT * FROM reconnect");
}


int runDELETE(NDBT_Context* ctx, NDBT_Step* step){
  BaseString query;
  query.assfmt("DELETE from reconnect WHERE thread=%d LIMIT 10",
               step->getStepNo());
  return runSQLQueries(ctx, step, query.c_str());
}


int runRestartCluster(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;

    if(restarts.executeRestart(ctx, "RestartAllNodesAbort", timeout) != 0){
      g_err << "Failed to restart all nodes with abort" << endl;
      result = NDBT_FAILED;
      break;
    }

    NdbSleep_SecSleep(10);
    i++;
  }
  ctx->stopTest();
  return result;
}


NDBT_TESTSUITE(testReconnect);
TESTCASE("InsertAndRestart",
	 "Run INSERTs while cluster restart"){
  INITIALIZER(runDropTable);
  INITIALIZER(runCreateTable);
  STEP(runINSERT);
  STEP(runRestartCluster);
}
TESTCASE("SelectAndRestart",
	 "Run SELECTs while cluster restart"){
  INITIALIZER(runDropTable);
  INITIALIZER(runCreateTable);
  STEP(runSELECT);
  STEP(runRestartCluster);
}
TESTCASE("DeleteAndRestart",
	 "Run DELETEs while cluster restart"){
  INITIALIZER(runDropTable);
  INITIALIZER(runCreateTable);
  STEP(runDELETE);
  STEP(runRestartCluster);
}
TESTCASE("AllAndRestart",
	 "Run all kind of statements while cluster restart"){
  INITIALIZER(runDropTable);
  INITIALIZER(runCreateTable);
  STEPS(runSELECT, 5);
  STEPS(runINSERT, 25);
  STEPS(runDELETE, 2);
  STEP(runRestartCluster);
}

NDBT_TESTSUITE_END(testReconnect);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testReconnect);
  return testReconnect.execute(argc, argv);
}

