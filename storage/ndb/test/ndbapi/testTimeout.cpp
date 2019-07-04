/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <random.h>
#include <NdbConfig.hpp>
#include <signaldata/DumpStateOrd.hpp>

#define TIMEOUT (Uint32)3000
Uint32 g_org_timeout = 3000;
Uint32 g_org_deadlock = 3000;

int
setTransactionTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  int timeout = ctx->getProperty("TransactionInactiveTimeout",TIMEOUT);

  NdbConfig conf;
  if (!conf.getProperty(conf.getMasterNodeId(),
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
			&g_org_timeout)){
    return NDBT_FAILED;
  }

  int val[] = { DumpStateOrd::TcSetApplTransactionTimeout, timeout };
  if(restarter.dumpStateAllNodes(val, 2) != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
resetTransactionTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  
  // g_org_timeout will be passed as printed int to mgm,
  // then converted to Uint32 before sent to tc.
  // Check convert Uint32 -> int -> Uint32 is safe
  NDB_STATIC_ASSERT(UINT_MAX32 == (Uint32)(int)UINT_MAX32);
  int val[] = { DumpStateOrd::TcSetApplTransactionTimeout, (int)g_org_timeout };
  if(restarter.dumpStateAllNodes(val, 2) != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
setDeadlockTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  int timeout = ctx->getProperty("TransactionDeadlockTimeout", TIMEOUT);
  
  NdbConfig conf;
  if (!conf.getProperty(conf.getMasterNodeId(),
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT,
			&g_org_deadlock))
    return NDBT_FAILED;
  
  g_err << "Setting timeout: " << timeout << endl;
  int val[] = { DumpStateOrd::TcSetTransactionTimeout, timeout };
  if(restarter.dumpStateAllNodes(val, 2) != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
getDeadlockTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  
  Uint32 val = 0;
  NdbConfig conf;
  if (!conf.getProperty(conf.getMasterNodeId(),
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT,
			&val))
    return NDBT_FAILED;

  if (val < 120000)
    val = 120000;
  ctx->setProperty("TransactionDeadlockTimeout", 4*val);
  
  return NDBT_OK;
}

int
resetDeadlockTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  
  // g_org_deadlock will be passed as printed int to mgm,
  // then converted to Uint32 before sent to tc.
  // Check convert Uint32 -> int -> Uint32 is safe
  NDB_STATIC_ASSERT(UINT_MAX32 == (Uint32)(int)UINT_MAX32);
  int val[] = { DumpStateOrd::TcSetTransactionTimeout, (int)g_org_deadlock };
  if(restarter.dumpStateAllNodes(val, 2) != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}


int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; }

int runTimeoutTrans2(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int stepNo = step->getStepNo();
  int mul1 = ctx->getProperty("Op1", (Uint32)0);
  int mul2 = ctx->getProperty("Op2", (Uint32)0);
  int records = ctx->getNumRecords();

  int timeout = ctx->getProperty("TransactionInactiveTimeout",TIMEOUT);

  int minSleep = (int)(timeout * 1.5);
  int maxSleep = timeout * 2;
  
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 0; l<loops && !ctx->isTestStopped() && result == NDBT_OK; l++){
    
    int op1 = 0 + (l + stepNo) * mul1;
    int op2 = 0 + (l + stepNo) * mul2;

    op1 = (op1 % 5);
    op2 = (op2 % 5);

    ndbout << stepNo << ": TransactionInactiveTimeout="<< timeout
	   << ", minSleep="<<minSleep
	   << ", maxSleep="<<maxSleep
	   << ", op1=" << op1
	   << ", op2=" << op2 << endl;;
    
    do{
      // Commit transaction
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      
      switch(op1){
      case 0:
	break;
      case 1:
	if(hugoOps.pkReadRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 2:
	if(hugoOps.pkUpdateRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 3:
	if(hugoOps.pkDeleteRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 4:
	if(hugoOps.pkInsertRecord(pNdb, stepNo+records+l) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      }
      
      if(result != NDBT_OK)
	break;

      int res = hugoOps.execute_NoCommit(pNdb);
      if(res != 0){
	g_err << stepNo << ": Fail" << __LINE__ << endl;
	result = NDBT_FAILED; break;
      }
      
      int sleep = minSleep + myRandom48(maxSleep-minSleep);   
      ndbout << stepNo << ": Sleeping for "<< sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);
      
      switch(op2){
      case 0:
	break;
      case 1:
	if(hugoOps.pkReadRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 2:
	if(hugoOps.pkUpdateRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 3:
	if(hugoOps.pkDeleteRecord(pNdb, stepNo) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      case 4:
	if(hugoOps.pkInsertRecord(pNdb, stepNo+2*records+l) != 0){
	  g_err << stepNo << ": Fail" << __LINE__ << endl;
	  result = NDBT_FAILED; break;
	}
	break;
      }

      // Expect that transaction has timed-out
      res = hugoOps.execute_Commit(pNdb);
      if(op1 != 0 && res != 266){
	g_err << stepNo << ": Fail: " << res << "!= 237, op1=" 
	      << op1 << ", op2=" << op2 << endl;
	result = NDBT_FAILED; break;
      }
      
    } while(false);
    
    hugoOps.closeTransaction(pNdb);
  }

  return result;
}

int runDontTimeoutTrans(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int stepNo = step->getStepNo();

  int timeout = ctx->getProperty("TransactionInactiveTimeout",TIMEOUT);

  int maxSleep = (int)(timeout * 0.5);
  ndbout << "TransactionInactiveTimeout="<< timeout
	 << ", maxSleep="<<maxSleep<<endl;


  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 0; l < loops && result == NDBT_OK; l++){

    do{
      // Commit transaction
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      CHECK(hugoOps.pkReadRecord(pNdb, stepNo) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      int sleep = myRandom48(maxSleep);   
      ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);
      
      // Expect that transaction has NOT timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 0); 
    
    } while(false);

    hugoOps.closeTransaction(pNdb);
  }
    
  return result;
}

int runDeadlockTimeoutTrans(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int stepNo = step->getStepNo();

  Uint32 deadlock_timeout;
  NdbConfig conf;
  if (!conf.getProperty(conf.getMasterNodeId(),
                        NODE_TYPE_DB,
                        CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT,
                        &deadlock_timeout)){
    return NDBT_FAILED;
  }


  int do_sleep = (int)(deadlock_timeout * 0.5);


  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 0; l < loops && result == NDBT_OK; l++){

    do{
      // Commit transaction
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      CHECK(hugoOps.pkReadRecord(pNdb, stepNo) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

      int sleep = int(deadlock_timeout * 1.5 + myRandom48(do_sleep));
      ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);

      // Expect that transaction has NOT timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 0);

    } while(false);

    hugoOps.closeTransaction(pNdb);
  }

  return result;
}

int runBuddyTransNoTimeout(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int stepNo = step->getStepNo();
  int maxSleep = (int)(TIMEOUT * 0.3);
  ndbout << "TransactionInactiveTimeout="<< TIMEOUT
	 << ", maxSleep="<<maxSleep<<endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 1; l < loops && result == NDBT_OK; l++){

    do{
      // Start an insert trans
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      int recordNo = records + (stepNo*loops) + l;
      CHECK(hugoOps.pkInsertRecord(pNdb, recordNo) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      int remain = maxSleep;
      for (int i = 0; i < 3; i++)
      {
        NdbTransaction* pTrans = hugoOps.getTransaction();

        // Perform buddy scan reads
        NdbScanOperation* pOp = pTrans->getNdbScanOperation(ctx->getTab());
        CHECK(pOp != 0);
        CHECK(pOp->readTuples(NdbOperation::LM_Read, 0, 0, 1) == 0);
        CHECK(pTrans->execute(NoCommit) == 0);
        while(pOp->nextResult() == 0);
	
        int sleep = myRandom48(remain);
        remain = remain - sleep + 1;
        ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
        NdbSleep_MilliSleep(sleep);
      }

      // Expect that transaction has NOT timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 0); 
    
    } while(false);

    hugoOps.closeTransaction(pNdb);
  }

  return result;
}

int runBuddyTransTimeout(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //int stepNo = step->getStepNo();
  ndbout << "TransactionInactiveTimeout="<< TIMEOUT <<endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 1; l < loops && result == NDBT_OK; l++){

    NdbTransaction* pTrans = 0;
    do{
      pTrans = pNdb->startTransaction();
      NdbScanOperation* pOp = pTrans->getNdbScanOperation(ctx->getTab());
      CHECK(pOp->readTuples(NdbOperation::LM_Read, 0, 0, 1) == 0);
      CHECK(pTrans->execute(NoCommit) == 0);
      
      int sleep = 2 * TIMEOUT;
      ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);
    
      int res = 0;
      while((res = pOp->nextResult()) == 0);
      ndbout_c("res: %d", res);
      CHECK(res == -1);
      
    } while(false);
    
    if (pTrans)
    {
      pTrans->close();
    }
  }
  
  return result;
}

int runScanRefreshNoTimeout(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int stepNo = step->getStepNo();
  int maxSleep = (int)(TIMEOUT * 0.3);
  ndbout << "TransactionInactiveTimeout="<< TIMEOUT
	 << ", maxSleep="<<maxSleep<<endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 1; l < loops && result == NDBT_OK; l++){

    do{
      // Start an insert trans
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      int recordNo = records + (stepNo*loops) + l;
      CHECK(hugoOps.pkInsertRecord(pNdb, recordNo) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      for (int i = 0; i < 3; i++)
      {
        NdbTransaction* pTrans = hugoOps.getTransaction();

        Vector<NdbScanOperation*> ops;
        for (int j = 0; j <= i; j++)
        {
          // Perform buddy scan reads
          NdbScanOperation* pOp = pTrans->getNdbScanOperation(ctx->getTab());
          CHECK(pOp != 0);
          CHECK(pOp->readTuples(NdbOperation::LM_Read, 0, 0, 1) == 0);
          ops.push_back(pOp);
        }
        CHECK(pTrans->execute(NoCommit) == 0);

        for (unsigned i = 0; i<TIMEOUT; i += 1000)
        {
          pTrans->refresh();
          NdbSleep_MilliSleep(1000);
        }

        int res;
        for (unsigned j = 0; j < ops.size(); j++)
        {
          while((res = ops[j]->nextResult()) == 0);
          CHECK(res != -1);
        }
      }

      // Expect that transaction has NOT timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 0); 
    
    } while(false);

    hugoOps.closeTransaction(pNdb);
  }

  return result;
}

int 
runError4012(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  //int stepNo = step->getStepNo();
  
  int timeout = ctx->getProperty("TransactionDeadlockTimeout", TIMEOUT);

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkUpdateRecord(pNdb, 0) == 0);
    int ret = hugoOps.execute_NoCommit(pNdb);
    if (ret == 0)
    {
      int sleep = timeout;
      ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);
      
      // Expect that transaction has NOT timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 0);
    }
    else
    {
      CHECK(ret == 4012);
    }
  } while(false);
  
  hugoOps.closeTransaction(pNdb);
  
  return result;
}


NDBT_TESTSUITE(testTimeout);
TESTCASE("DontTimeoutTransaction", 
	 "Test that the transaction does not timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is smaller than TransactionInactiveTimeout"){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runDontTimeoutTrans, 1); 
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("Bug11290",
         "Setting TransactionInactiveTimeout to 0(zero) "\
         "should result in infinite timeout, and not as "\
         "was the bug, a timeout that is equal to the deadlock timeout"){
  TC_PROPERTY("TransactionInactiveTimeout",(Uint32)0);
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runDeadlockTimeoutTrans, 1);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("DontTimeoutTransaction5", 
	 "Test that the transaction does not timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is smaller than TransactionInactiveTimeout" \
	 "Five simultaneous threads"){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runDontTimeoutTrans, 5); 
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("TimeoutRandTransaction", 
	 "Test that the transaction does timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is larger than TransactionInactiveTimeout"){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  TC_PROPERTY("Op1", 7);
  TC_PROPERTY("Op2", 11);
  STEPS(runTimeoutTrans2, 5);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("BuddyTransNoTimeout", 
	 "Start a transaction and perform an insert with NoCommit. " \
	 "Start a buddy transaction wich performs long running scans " \
	 "and sleeps. " \
	 "The total sleep time is longer than TransactionInactiveTimeout" \
	 "Commit the first transaction, it should not have timed out."){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runBuddyTransNoTimeout, 1);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("BuddyTransNoTimeout5", 
	 "Start a transaction and perform an insert with NoCommit. " \
	 "Start a buddy transaction wich performs long running scans " \
	 "and sleeps. " \
	 "The total sleep time is longer than TransactionInactiveTimeout" \
	 "Commit the first transaction, it should not have timed out." \
	 "Five simultaneous threads"){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runBuddyTransNoTimeout, 5);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("BuddyTransTimeout1", 
	 "Start a scan and check that it gets aborted"){
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runBuddyTransTimeout, 1);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRefreshNoTimeout", 
	 "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(setTransactionTimeout);
  STEPS(runScanRefreshNoTimeout, 1);
  FINALIZER(resetTransactionTimeout);
  FINALIZER(runClearTable);
}
#if 0
TESTCASE("Error4012", ""){
  TC_PROPERTY("TransactionDeadlockTimeout", 120000);
  INITIALIZER(runLoadTable);
  INITIALIZER(getDeadlockTimeout);
  INITIALIZER(setDeadlockTimeout);
  STEPS(runError4012, 2);
  FINALIZER(runClearTable);
}
#endif
NDBT_TESTSUITE_END(testTimeout)

int main(int argc, const char** argv){
  ndb_init();
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  NDBT_TESTSUITE_INSTANCE(testTimeout);
  return testTimeout.execute(argc, argv);
}

template class Vector<NdbScanOperation*>;
