/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <random.h>
#include <NdbConfig.hpp>


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

int runTimeoutTrans(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbConfig conf(GETNDB(step)->getNodeId()+1);
  unsigned int nodeId = conf.getMasterNodeId();
  int stepNo = step->getStepNo();
  Uint32 timeoutVal;
  if (!conf.getProperty(nodeId,
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
			&timeoutVal)){
    return NDBT_FAILED;
  }
  int minSleep = (int)(timeoutVal * 1.5);
  int maxSleep = timeoutVal * 2;
  ndbout << "TransactionInactiveTimeout="<<timeoutVal
	 << ", minSleep="<<minSleep
	 << ", maxSleep="<<maxSleep<<endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 0; l < loops && result == NDBT_OK; l++){

    do{
      // Commit transaction
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      CHECK(hugoOps.pkReadRecord(pNdb, stepNo, true) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      int sleep = minSleep + myRandom48(maxSleep-minSleep);   
      ndbout << "Sleeping for " << sleep << " milliseconds" << endl;
      NdbSleep_MilliSleep(sleep);
      
      // Expect that transaction has timed-out
      CHECK(hugoOps.execute_Commit(pNdb) == 237); 

    } while(false);

    hugoOps.closeTransaction(pNdb);
  
  }

  return result;
}

int runDontTimeoutTrans(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbConfig conf(GETNDB(step)->getNodeId()+1);
  unsigned int nodeId = conf.getMasterNodeId();
  int stepNo = step->getStepNo();
  Uint32 timeoutVal;
  if (!conf.getProperty(nodeId,
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
			&timeoutVal)){
    return NDBT_FAILED;
  }
  int maxSleep = (int)(timeoutVal * 0.5);
  ndbout << "TransactionInactiveTimeout="<<timeoutVal
	 << ", maxSleep="<<maxSleep<<endl;


  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 0; l < loops && result == NDBT_OK; l++){

    do{
      // Commit transaction
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      CHECK(hugoOps.pkReadRecord(pNdb, stepNo, true) == 0);
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

int runBuddyTransNoTimeout(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbConfig conf(GETNDB(step)->getNodeId()+1);
  unsigned int nodeId = conf.getMasterNodeId();
  int stepNo = step->getStepNo();
  Uint32 timeoutVal;
  if (!conf.getProperty(nodeId,
			NODE_TYPE_DB, 
			CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
			&timeoutVal)){
    return NDBT_FAILED;
  }
  int maxSleep = (int)(timeoutVal * 0.3);
  ndbout << "TransactionInactiveTimeout="<<timeoutVal
	 << ", maxSleep="<<maxSleep<<endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  for (int l = 1; l < loops && result == NDBT_OK; l++){

    do{
      // Start an insert trans
      CHECK(hugoOps.startTransaction(pNdb) == 0);
      int recordNo = records + (stepNo*loops) + l;
      CHECK(hugoOps.pkInsertRecord(pNdb, recordNo, true) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      for (int i = 0; i < 10; i++){
	// Perform buddy scan reads
	CHECK(hugoOps.scanReadRecords(pNdb) == 0);
	CHECK(hugoOps.executeScanRead(pNdb) == 0);

	int sleep = myRandom48(maxSleep);   	
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

NDBT_TESTSUITE(testTimeout);
TESTCASE("DontTimeoutTransaction", 
	 "Test that the transaction does not timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is smaller than TransactionInactiveTimeout"){
  INITIALIZER(runLoadTable);
  STEPS(runDontTimeoutTrans, 1); 
  FINALIZER(runClearTable);
}
TESTCASE("DontTimeoutTransaction5", 
	 "Test that the transaction does not timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is smaller than TransactionInactiveTimeout" \
	 "Five simultaneous threads"){
  INITIALIZER(runLoadTable);
  STEPS(runDontTimeoutTrans, 5); 
  FINALIZER(runClearTable);
}
TESTCASE("TimeoutTransaction", 
	 "Test that the transaction does timeout "\
	 "if we sleep during the transaction. Use a sleep "\
	 "value which is larger than TransactionInactiveTimeout"){
  INITIALIZER(runLoadTable);
  STEPS(runTimeoutTrans, 1);
  FINALIZER(runClearTable);
}
TESTCASE("TimeoutTransaction5", 
	 "Test that the transaction does timeout " \
	 "if we sleep during the transaction. Use a sleep " \
	 "value which is larger than TransactionInactiveTimeout" \
	 "Five simultaneous threads"){
  INITIALIZER(runLoadTable);
  STEPS(runTimeoutTrans, 5);
  FINALIZER(runClearTable);
}
TESTCASE("BuddyTransNoTimeout", 
	 "Start a transaction and perform an insert with NoCommit. " \
	 "Start a buddy transaction wich performs long running scans " \
	 "and sleeps. " \
	 "The total sleep time is longer than TransactionInactiveTimeout" \
	 "Commit the first transaction, it should not have timed out."){
  INITIALIZER(runLoadTable);
  STEPS(runBuddyTransNoTimeout, 1);
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
  STEPS(runBuddyTransNoTimeout, 5);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testTimeout);

int main(int argc, const char** argv){
  myRandom48Init(NdbTick_CurrentMillisecond());
  return testTimeout.execute(argc, argv);
}

