/*
 Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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


class NdbPool : private NdbLockable {
  public:
    NdbPool(Ndb_cluster_connection *_conn) : conn(_conn), list(0), size(0), 
                                             created(0) {};
    Ndb * getNdb();
    void recycleNdb(Ndb *n);
    void closeAll();
    
  private:
    Ndb_cluster_connection *conn;
    Ndb * list;
    int size, created;
};

Ndb * NdbPool::getNdb() {
  Ndb * n;
  lock();
  if(list) {
    n = list;
    list = (Ndb *) n->getCustomData();
    size--;
  }
  else {
    n = new Ndb(conn);
    n->init();
    created++;
  }
  unlock();
  return n;
}

void NdbPool::recycleNdb(Ndb *n) {
  lock();
  n->setCustomData(list);
  list = n;
  size++;
  unlock();
}
  
void NdbPool::closeAll() {
  lock();
  while(list) {
    Ndb *n = list;
    list = (Ndb *) n->getCustomData();
    delete n;
  }
  size = 0;
  unlock();
}

NdbWaitGroup * global_poll_group;
NdbPool * global_ndb_pool;

#define check(b, e) \
  if (!(b)) { g_err << "ERR: " << step->getName() << " failed on line " \
  << __LINE__ << ": " << e.getNdbError() << endl; return NDBT_FAILED; }


int runSetup(NDBT_Context* ctx, NDBT_Step* step, int waitGroupSize){

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
  global_poll_group = conn->create_ndb_wait_group(waitGroupSize);
  if(global_poll_group == 0) {
    return NDBT_FAILED;
  }

  /* and subsequent calls should fail */
  if(conn->create_ndb_wait_group(waitGroupSize) != 0) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/* NdbWaitGroup version 1 API uses a fixed-size wait group.
   It cannot grow.  We size it at 1000 Ndbs.
*/
int runSetup_v1(NDBT_Context* ctx, NDBT_Step* step) {
  return runSetup(ctx, step, 1000);
}

/* Version 2 of the API will allow the wait group to grow on 
   demand, so we start small.
*/
int runSetup_v2(NDBT_Context* ctx, NDBT_Step* step) {
  Ndb* pNdb = GETNDB(step);
  Ndb_cluster_connection* conn = &pNdb->get_ndb_cluster_connection();
  global_ndb_pool = new NdbPool(conn);
  return runSetup(ctx, step, 7);
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

/* Version 2 API tests */
#define V2_NLOOPS 32

/* Producer thread */
int runV2MultiWait_Producer(NDBT_Context* ctx, NDBT_Step* step,
                           int thd_id, int nthreads)
{
  int records = ctx->getNumRecords();
  HugoOperations hugoOps(*ctx->getTab());

  /* For three threads (2 producers + 1 consumer) we loop 0-7.
     producer 0 is slow if (loop & 1)  
     producer 1 is slow if (loop & 2)
     consumer is slow if (loop & 4)
  */
  for (int loop = 0; loop < V2_NLOOPS; loop++) 
  {
    ctx->getPropertyWait("LOOP", loop+1);
    bool slow = loop & (thd_id+1);
    for (int j=0; j < records; j++)
    {
      if(j % nthreads == thd_id) 
      {
        Ndb* ndb = global_ndb_pool->getNdb();
        NdbTransaction* trans = ndb->startTransaction();
        check(trans != NULL, (*ndb));
        ndb->setCustomData(trans);

        NdbOperation* readOp = trans->getNdbOperation(ctx->getTab());
        check(readOp != NULL, (*trans));
        check(readOp->readTuple() == 0, (*readOp));
        check(hugoOps.equalForRow(readOp, j) == 0, hugoOps);

        /* Read all other cols */
        for (int k=0; k < ctx->getTab()->getNoOfColumns(); k++)
        {
          check(readOp->getValue(ctx->getTab()->getColumn(k)) != NULL,
                (*readOp));
        }

        trans->executeAsynchPrepare(NdbTransaction::Commit,
                                    NULL,
                                    NULL,
                                    NdbOperation::AbortOnError);
        ndb->sendPreparedTransactions();
        global_poll_group->push(ndb);
        if(slow) 
        {
          int tm = myRandom48(3) * myRandom48(3);
          if(tm) NdbSleep_MilliSleep(tm);
        }
      }
    }
  } 
  return NDBT_OK;
}

int runV2MultiWait_Push_Thd0(NDBT_Context* ctx, NDBT_Step* step)
{
  return runV2MultiWait_Producer(ctx, step, 0, 2);
}

int runV2MultiWait_Push_Thd1(NDBT_Context* ctx, NDBT_Step* step)
{
  return runV2MultiWait_Producer(ctx, step, 1, 2);
}


/* Consumer */
int runV2MultiWait_WaitPop_Thread(NDBT_Context* ctx, NDBT_Step* step)
{
  static int iter = 0;  // keeps incrementing when test case is repeated
  int records = ctx->getNumRecords();
  const char * d[5] = { " fast"," slow"," slow",""," slow" };
  const int timeout[3] = { 100, 1, 0 };
  const int pct_wait[9] = { 0,0,0,50,50,50,100,100,100 };
  for (int loop = 0; loop < V2_NLOOPS; loop++, iter++) 
  {
    ctx->incProperty("LOOP");
    ndbout << "V2 test: " << d[loop&1] << d[loop&2] << d[loop&4];
    ndbout << " " << timeout[iter%3] << "/" << pct_wait[iter%9] << endl;
    bool slow = loop & 4; 
    int nrec = 0;
    while(nrec < records) 
    {
      /* Occasionally check with no timeout */
      global_poll_group->wait(timeout[iter%3], pct_wait[iter%9]);
      Ndb * ndb = global_poll_group->pop();
      while(ndb) 
      {
        check(ndb->pollNdb(0, 1) != 0, (*ndb));
        nrec++;
        NdbTransaction *tx = (NdbTransaction *) ndb->getCustomData();
        tx->close();
        global_ndb_pool->recycleNdb(ndb);
        ndb = global_poll_group->pop();
      }
      if(slow) 
      {
         NdbSleep_MilliSleep(myRandom48(6));
      }  
    }
  }
  ctx->stopTest();
  global_ndb_pool->closeAll();
  return NDBT_OK;
}

int runMiscUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  Ndb * ndb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    int r = 0;
    switch(i % 5) {
      case 0:  // batch size = 2, random = 1
        r = hugoTrans.pkReadRecords(ndb, records / 20, 2, 
                                    NdbOperation::LM_Read, 1);
        break;
      case 1:
        r = hugoTrans.pkUpdateRecords(ndb, records / 20);
        break;
      case 2:
        r = hugoTrans.scanReadRecords(ndb, records);
        break;
      case 3:
        r = hugoTrans.scanUpdateRecords(ndb, records / 10);
        break;
      case 4:
        NdbSleep_MilliSleep(records);
        break;
    }
    if(r != 0) return NDBT_FAILED;
    i++;
  }
  ndbout << "V2 Test misc thread: " << i << " transactions" << endl;
  return NDBT_OK;
}

int sleepAndStop(NDBT_Context* ctx, NDBT_Step* step){
  sleep(20);
  ctx->stopTest();
  return NDBT_OK;
}  

NDBT_TESTSUITE(testAsynchMultiwait);
TESTCASE("AsynchMultiwaitPkRead",
         "Verify NdbWaitGroup API (1 thread)") {
  INITIALIZER(runSetup_v1);
  STEP(runPkReadMultiBasic);
  FINALIZER(runCleanup);
}
TESTCASE("AsynchMultiwaitWakeup",
         "Verify wait-multi-ndb wakeup Api code") {
  INITIALIZER(runSetup_v1);
  TC_PROPERTY("PHASE", Uint32(0));
  STEP(runPkReadMultiWakeupT1);
  STEP(runPkReadMultiWakeupT2);
  FINALIZER(runCleanup);
}
TESTCASE("AsynchMultiwait_Version2",
         "Verify NdbWaitGroup API version 2") {
  INITIALIZER(runSetup_v2);
  TC_PROPERTY("LOOP", Uint32(0));
  STEP(runV2MultiWait_Push_Thd0);
  STEP(runV2MultiWait_Push_Thd1);  
  STEP(runV2MultiWait_WaitPop_Thread);
  STEP(runMiscUntilStopped);
  FINALIZER(runCleanup);
}
TESTCASE("JustMisc", "Just run the Scan test") {
  INITIALIZER(runSetup_v2);
  STEP(runMiscUntilStopped);
  STEP(sleepAndStop);
  FINALIZER(runCleanup);
}
NDBT_TESTSUITE_END(testAsynchMultiwait);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testAsynchMultiwait);
  return testAsynchMultiwait.execute(argc, argv);
}

