/* Copyright (c) 2012 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>

int create100Tables(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;

  for (Uint32 t=0; t < 100; t++)
  {
    char tabnameBuff[10];
    snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", t);
    
    NdbDictionary::Table tab;
    tab.setName(tabnameBuff);
    NdbDictionary::Column pk;
    pk.setName("PK");
    pk.setType(NdbDictionary::Column::Varchar);
    pk.setLength(20);
    pk.setNullable(false);
    pk.setPrimaryKey(true);
    tab.addColumn(pk);

    pNdb->getDictionary()->dropTable(tab.getName());
    if(pNdb->getDictionary()->createTable(tab) != 0)
    {
      ndbout << "Create table failed with error : "
             << pNdb->getDictionary()->getNdbError().code
             << " "
             << pNdb->getDictionary()->getNdbError().message
             << endl;
      return NDBT_FAILED;
    }
    
    ndbout << "Created table " << tabnameBuff << endl;
  }

  return NDBT_OK;
}

int drop100Tables(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();

  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;
    
  for (Uint32 t=0; t < 100; t++)
  {
    char tabnameBuff[10];
    snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", t);
    
    if (pNdb->getDictionary()->dropTable(tabnameBuff) != 0)
    {
      ndbout << "Drop table failed with error : "
             << pNdb->getDictionary()->getNdbError().code
             << " "
             << pNdb->getDictionary()->getNdbError().message
             << endl;
    }
    else
    {
      ndbout << "Dropped table " << tabnameBuff << endl;
    }
  }
  
  return NDBT_OK;
}

int dropTable(NDBT_Context* ctx, NDBT_Step* step, Uint32 num)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();

  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;
    
  char tabnameBuff[10];
  snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", num);
  
  if (pNdb->getDictionary()->dropTable(tabnameBuff) != 0)
  {
    ndbout << "Drop table failed with error : "
           << pNdb->getDictionary()->getNdbError().code
           << " "
           << pNdb->getDictionary()->getNdbError().message
           << endl;
  }
  else
  {
    ndbout << "Dropped table " << tabnameBuff << endl;
  }
  
  return NDBT_OK;
}


enum Scenarios
{
//  NORMAL,  // Commented to save some time.
  DROP_TABLE,
  RESTART_MASTER,
  RESTART_SLAVE,
  NUM_SCENARIOS
};


enum Tasks
{
  WAIT = 0,
  DROP_TABLE_REQ = 1,
  MASTER_RESTART_REQ = 2,
  SLAVE_RESTART_REQ = 3
};

int testWorker(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(ctx->getTab()->getName(), "T1") != 0)
    return NDBT_OK;

  /* Worker step to run in a separate thread for
   * blocking activities
   * Generally the blocking of the DIH table definition flush
   * blocks the completion of the drop table/node restarts,
   * so this must be done in a separate thread to avoid
   * deadlocks.
   */
  
  while (!ctx->isTestStopped())
  {
    ndbout_c("Worker : waiting for request...");
    ctx->getPropertyWait("DIHWritesRequest", 1);
  
    if (!ctx->isTestStopped())
    {
      Uint32 req = ctx->getProperty("DIHWritesRequestType", (Uint32)0);

      switch ((Tasks) req)
      {
      case DROP_TABLE_REQ:
      {
        /* Drop table */
        ndbout_c("Worker : dropping table");
        if (dropTable(ctx, step, 2) != NDBT_OK)
        {
          return NDBT_FAILED;
        }
        ndbout_c("Worker : table dropped.");
        break;
      }
      case MASTER_RESTART_REQ:
      {
        ndbout_c("Worker : restarting Master");
        
        NdbRestarter restarter;
        int master_nodeid = restarter.getMasterNodeId();
        ndbout_c("Worker : Restarting Master (%d)...", master_nodeid);
        if (restarter.restartOneDbNode2(master_nodeid, 
                                        NdbRestarter::NRRF_NOSTART |
//                                        NdbRestarter::NRRF_FORCE |
                                        NdbRestarter::NRRF_ABORT) ||
            restarter.waitNodesNoStart(&master_nodeid, 1) ||
            restarter.startAll())
        {
          ndbout_c("Worker : Error restarting Master.");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Waiting for master to recover...");
        if (restarter.waitNodesStarted(&master_nodeid, 1))
        {
          ndbout_c("Worker : Error waiting for Master restart");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Master recovered.");
        break;
      }
      case SLAVE_RESTART_REQ:
      {
        NdbRestarter restarter;
        int slave_nodeid = restarter.getRandomNotMasterNodeId(rand());
        ndbout_c("Worker : Restarting non-master (%d)...", slave_nodeid);
        if (restarter.restartOneDbNode2(slave_nodeid, 
                                        NdbRestarter::NRRF_NOSTART |
//                                        NdbRestarter::NRRF_FORCE |
                                        NdbRestarter::NRRF_ABORT) ||
            restarter.waitNodesNoStart(&slave_nodeid, 1) ||
            restarter.startAll())
        {
          ndbout_c("Worker : Error restarting Slave.");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Waiting for slave to recover...");
        if (restarter.waitNodesStarted(&slave_nodeid, 1))
        {
          ndbout_c("Worker : Error waiting for Slave restart");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Slave recovered.");
        break;
      }
      default:
      { 
        break;
      }
      }
    }
    ctx->setProperty("DIHWritesRequestType", (Uint32) 0);
    ctx->setProperty("DIHWritesRequest", (Uint32) 2);
  }
  
  ndbout_c("Worker, done.");
  return NDBT_OK;
}

int testSlowDihFileWrites(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Testcase checks behaviour with slow flushing of DIH table definitions
   * This caused problems in the past by exhausting the DIH page pool
   * Now there's a concurrent operations limit.
   * Check that it behaves with many queued ops, parallel drop/node restarts
   */
  
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(ctx->getTab()->getName(), "T1") != 0)
    return NDBT_OK;

  /* 1. Activate slow write error insert
   * 2. Trigger LCP
   * 3. Wait some time, periodically producing info on 
   *    the internal state
   * 4. Perform some parallel action (drop table/node restarts)
   * 5. Wait some time, periodically producing info on 
   *    the internal state
   * 6. Clear the error insert
   * 7. Wait a little longer
   * 8. Done.
   */
  NdbRestarter restarter;

  for (Uint32 scenario = 0;  scenario < NUM_SCENARIOS; scenario++)
  {
    ndbout_c("Inserting error 7235");
    restarter.insertErrorInAllNodes(7235);
    
    ndbout_c("Triggering LCP");
    int dumpArg = 7099;
    restarter.dumpStateAllNodes(&dumpArg, 1);
    
    const Uint32 periodSeconds = 10;
    Uint32 waitPeriods = 6;
    dumpArg = 7032;
    
    for (Uint32 p=0; p<waitPeriods; p++)
    {
      if (p == 3)
      {
        switch ((Scenarios) scenario)
        {
        case DROP_TABLE:
        {
          /* Drop one of the early-created tables */
          ndbout_c("Requesting DROP TABLE");
          ctx->setProperty("DIHWritesRequestType", (Uint32) DROP_TABLE_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);
          break;
        }
        case RESTART_MASTER:
        {
          ndbout_c("Requesting Master restart");
          ctx->setProperty("DIHWritesRequestType", (Uint32) MASTER_RESTART_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);

          break;
        }
        case RESTART_SLAVE:
        {
          ndbout_c("Requesting Slave restart");
          ctx->setProperty("DIHWritesRequestType", (Uint32) SLAVE_RESTART_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);

          break;
        }
        default:
          break;
        }
      }

      ndbout_c("Dumping DIH page info to ndbd stdout");
      restarter.dumpStateAllNodes(&dumpArg, 1);
      NdbSleep_MilliSleep(periodSeconds * 1000);
    }
    
    ndbout_c("Clearing error insert...");
    restarter.insertErrorInAllNodes(0);
    
    waitPeriods = 2;
    for (Uint32 p=0; p<waitPeriods; p++)
    {
      ndbout_c("Dumping DIH page info to ndbd stdout");
      restarter.dumpStateAllNodes(&dumpArg, 1);
      NdbSleep_MilliSleep(periodSeconds * 1000);
    }
    
    ndbout_c("Waiting for worker to finish task...");
    ctx->getPropertyWait("DIHWritesRequest", 2);
    
    if (ctx->isTestStopped())
      return NDBT_OK;

    ndbout_c("Done.");
  }  

  /* Finish up */
  ctx->stopTest();

  return NDBT_OK;
}



NDBT_TESTSUITE(testLimits);

TESTCASE("SlowDihFileWrites",
         "Test behaviour of slow Dih table file writes")
{
  INITIALIZER(create100Tables);
  STEP(testWorker);
  STEP(testSlowDihFileWrites);
  FINALIZER(drop100Tables);
}

NDBT_TESTSUITE_END(testLimits);

int main(int argc, const char** argv){
  ndb_init();
  return testLimits.execute(argc, argv);
}
