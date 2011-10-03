/*
 Copyright (c) 2011 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include "NDBT_Test.hpp"
#include "NDBT_ReturnCodes.h"
#include "HugoTransactions.hpp"
#include "HugoAsynchTransactions.hpp"
#include "UtilTransactions.hpp"
#include "random.h"
#include "../../src/ndbapi/NdbWaitGroup.hpp"

NdbWaitGroup * global_poll_group;

#define check(b, e) \
  if (!(b)) { g_err << "ERR: " << step->getName() << " failed on line " \
  << __LINE__ << ": " << e.getNdbError() << endl; return NDBT_FAILED; }


int runSetup(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;
  Ndb* pNdb = GETNDB(step);

  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTableAsynch(pNdb, records, batchSize,
				transactions, operations) != 0){
    return NDBT_FAILED;
  }

  Ndb_cluster_connection* conn = &pNdb->get_ndb_cluster_connection();

  /* The first call to create_multi_ndb_wait_group() should succeed ... */
  global_poll_group = conn->create_ndb_wait_group(1000);
  if(global_poll_group == 0) {
    return NDBT_FAILED;
  }

  /* and subsequent calls should fail */
  if(conn->create_ndb_wait_group(1000) != 0) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runCleanup(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;
  Ndb* pNdb = GETNDB(step);

  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecordsAsynch(pNdb,  records, batchSize,
				   transactions, operations) != 0){
    return NDBT_FAILED;
  }

  pNdb->get_ndb_cluster_connection().release_ndb_wait_group(global_poll_group);

  return NDBT_OK;
}


int runPkReadMultiBasic(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  const int MAX_NDBS = 200;
  Ndb* pNdb = GETNDB(step);
  Ndb_cluster_connection* conn = &pNdb->get_ndb_cluster_connection();

  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());

  Ndb* ndbObjs[ MAX_NDBS ];
  NdbTransaction* transArray[ MAX_NDBS ];
  Ndb ** ready_ndbs;

  for (int j=0; j < MAX_NDBS; j++)
  {
    Ndb* ndb = new Ndb(conn);
    check(ndb->init() == 0, (*ndb));
    ndbObjs[ j ] = ndb;
  }

  while (i<loops) {
    ndbout << "Loop : " << i << ": ";
    int recordsLeft = records;

    do
    {
      /* Define and execute Pk read requests on
       * different Ndb objects
       */
      int ndbcnt = 0;
      int pollcnt = 0;
      int lumpsize = 1 + myRandom48(MIN(recordsLeft, MAX_NDBS));
      while(lumpsize &&
            recordsLeft &&
            ndbcnt < MAX_NDBS)
      {
        Ndb* ndb = ndbObjs[ ndbcnt ];
        NdbTransaction* trans = ndb->startTransaction();
        check(trans != NULL, (*ndb));
        NdbOperation* readOp = trans->getNdbOperation(ctx->getTab());
        check(readOp != NULL, (*trans));
        check(readOp->readTuple() == 0, (*readOp));
        check(hugoOps.equalForRow(readOp, recordsLeft) == 0, hugoOps);

        /* Read all other cols */
        for (int k=0; k < ctx->getTab()->getNoOfColumns(); k++)
        {
          check(readOp->getValue(ctx->getTab()->getColumn(k)) != NULL,
                (*readOp));
        }

        /* Now send em off */
        trans->executeAsynchPrepare(NdbTransaction::Commit,
                                    NULL,
                                    NULL,
                                    NdbOperation::AbortOnError);
        ndb->sendPreparedTransactions();

        transArray[ndbcnt] = trans;
        global_poll_group->addNdb(ndb);

        ndbcnt++;
        pollcnt++;
        recordsLeft--;
        lumpsize--;
      };

      /* Ok, now wait for the Ndbs to complete */
      while (pollcnt)
      {
        /* Occasionally check with no timeout */
        Uint32 timeout_millis = myRandom48(2)?10000:0;
        int count = global_poll_group->wait(ready_ndbs, timeout_millis);

        if (count > 0)
        {
          for (int y=0; y < count; y++)
          {
            Ndb *ndb = ready_ndbs[y];
            check(ndb->pollNdb(0, 1) != 0, (*ndb));
          }
          pollcnt -= count;
        }
      }

      /* Ok, now close the transactions */
      for (int t=0; t < ndbcnt; t++)
      {
        transArray[t]->close();
      }
    } while (recordsLeft);

    i++;
  }

  for (int j=0; j < MAX_NDBS; j++)
  {
    delete ndbObjs[ j ];
  }

  return NDBT_OK;
}

int runPkReadMultiWakeupT1(NDBT_Context* ctx, NDBT_Step* step)
{
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* ndb = GETNDB(step);
  Uint32 phase = ctx->getProperty("PHASE");

  if (phase != 0)
  {
    ndbout << "Thread 1 : Error, initial phase should be 0 not " << phase << endl;
    return NDBT_FAILED;
  };

  /* We now start a transaction, locking row 0 */
  ndbout << "Thread 1 : Starting transaction locking row 0..." << endl;
  check(hugoOps.startTransaction(ndb) == 0, hugoOps);
  check(hugoOps.pkReadRecord(ndb, 0, 1, NdbOperation::LM_Exclusive) == 0,
        hugoOps);
  check(hugoOps.execute_NoCommit(ndb) == 0, hugoOps);

  ndbout << "Thread 1 : Lock taken." << endl;
  ndbout << "Thread 1 : Triggering Thread 2 by move to phase 1" << endl;
  /* Ok, now get thread 2 to try to read row */
  ctx->incProperty("PHASE"); /* Set to 1 */

  /* Here, loop waking up waiter on the cluster connection */
  /* Check the property has not moved to phase 2 */
  ndbout << "Thread 1 : Performing async wakeup until phase changes to 2"
  << endl;
  while (ctx->getProperty("PHASE") != 2)
  {
    global_poll_group->wakeup();
    NdbSleep_MilliSleep(500);
  }

  ndbout << "Thread 1 : Phase changed to 2, committing transaction "
  << "and releasing lock" << endl;

  /* Ok, give them a break, commit transaction */
  check(hugoOps.execute_Commit(ndb) ==0, hugoOps);
  hugoOps.closeTransaction(ndb);

  ndbout << "Thread 1 : Finished" << endl;
  return NDBT_OK;
}

int runPkReadMultiWakeupT2(NDBT_Context* ctx, NDBT_Step* step)
{
  ndbout << "Thread 2 : Waiting for phase 1 notification from Thread 1" << endl;
  ctx->getPropertyWait("PHASE", 1);

  /* Ok, now thread 1 has locked row 1, we'll attempt to read
   * it, using the multi_ndb_wait Api to block
   */
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* ndb = GETNDB(step);

  ndbout << "Thread 2 : Starting async transaction to read row" << endl;
  check(hugoOps.startTransaction(ndb) == 0, hugoOps);
  check(hugoOps.pkReadRecord(ndb, 0, 1, NdbOperation::LM_Exclusive) == 0,
        hugoOps);
  /* Prepare, Send */
  check(hugoOps.execute_async(ndb,
                              NdbTransaction::Commit,
                              NdbOperation::AbortOnError) == 0,
        hugoOps);

  global_poll_group->addNdb(ndb);
  Ndb ** ready_ndbs;
  int wait_rc = 0;
  int acknowledged = 0;
  do
  {
    ndbout << "Thread 2 : Calling NdbWaitGroup::wait()" << endl;
    wait_rc = global_poll_group->wait(ready_ndbs, 10000);
    ndbout << "           Result : " << wait_rc << endl;
    if (wait_rc == 0)
    {
      if (!acknowledged)
      {
        ndbout << "Thread 2 : Woken up, moving to phase 2" << endl;
        ctx->incProperty("PHASE");
        acknowledged = 1;
      }
    }
    else if (wait_rc > 0)
    {
      ndbout << "Thread 2 : Transaction completed" << endl;
      ndb->pollNdb(1,0);
      hugoOps.closeTransaction(ndb);
    }
  } while (wait_rc == 0);

  return (wait_rc == 1 ? NDBT_OK : NDBT_FAILED);
}

NDBT_TESTSUITE(testAsynchMultiwait);
TESTCASE("AsynchMultiwaitPkRead",
         "Verify NdbWaitGroup API (1 thread)") {
  INITIALIZER(runSetup);
  STEP(runPkReadMultiBasic);
  FINALIZER(runCleanup);
}
TESTCASE("AsynchMultiwaitWakeup",
         "Verify wait-multi-ndb wakeup Api code") {
  INITIALIZER(runSetup);
  TC_PROPERTY("PHASE", Uint32(0));
  STEP(runPkReadMultiWakeupT1);
  STEP(runPkReadMultiWakeupT2);
  FINALIZER(runCleanup);
}
NDBT_TESTSUITE_END(testAsynchMultiwait);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testAsynchMultiwait);
  return testAsynchMultiwait.execute(argc, argv);
}

