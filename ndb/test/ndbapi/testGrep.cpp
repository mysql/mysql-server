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
#include <NdbGrep.hpp>


#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 


int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runPkUpdate(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << "|- " << i << ": ";
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, batchSize) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int runRestartInitial(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  Ndb* pNdb = GETNDB(step);

  const NdbDictionary::Table *tab = ctx->getTab();
  pNdb->getDictionary()->dropTable(tab->getName());

  if (restarter.restartAll(true) != 0)
    return NDBT_FAILED;

  return NDBT_OK;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }
  
  loops *= restarter.getNumDbNodes();
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    
    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    

    if(restarter.waitClusterStarted(60) != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    NdbSleep_SecSleep(1);

    lastId++;
    i++;
  }

  ctx->stopTest();
  
  return result;
}

int runCheckAllNodesStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if(restarter.waitClusterStarted(1) != 0){
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}


bool testMaster = true;
bool testSlave = false;

int setMaster(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = true;
  testSlave = false;
  return NDBT_OK;
}
int setMasterAsSlave(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = true;
  testSlave = true;
  return NDBT_OK;
}
int setSlave(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = false;
  testSlave = true;
  return NDBT_OK;
}

int runAbort(NDBT_Context* ctx, NDBT_Step* step){
  

  NdbGrep grep(GETNDB(step)->getNodeId()+1);
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  if (testMaster) {
    if (testSlave) {
      if (grep.NFMasterAsSlave(restarter) == -1){
	return NDBT_FAILED;
      }
    } else {
      if (grep.NFMaster(restarter) == -1){
	return NDBT_FAILED;
      }
    }
  } else {
    if (grep.NFSlave(restarter) == -1){
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runFail(NDBT_Context* ctx, NDBT_Step* step){
  NdbGrep grep(GETNDB(step)->getNodeId()+1);

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  if (testMaster) {
    if (testSlave) {
      if (grep.FailMasterAsSlave(restarter) == -1){
	return NDBT_FAILED;
      }
    } else {
      if (grep.FailMaster(restarter) == -1){
	return NDBT_FAILED;
      }
    }
  } else {
    if (grep.FailSlave(restarter) == -1){
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runGrepBasic(NDBT_Context* ctx, NDBT_Step* step){
  NdbGrep grep(GETNDB(step)->getNodeId()+1);
  unsigned grepId = 0;

  if (grep.start() == -1){
    return NDBT_FAILED;
  }
  ndbout << "Started grep " << grepId << endl;
  ctx->setProperty("GrepId", grepId);

  return NDBT_OK;
}




int runVerifyBasic(NDBT_Context* ctx, NDBT_Step* step){
  NdbGrep grep(GETNDB(step)->getNodeId()+1, ctx->getRemoteMgm());
  ndbout_c("no of nodes %d" ,grep.getNumDbNodes());
  int result;
  if ((result = grep.verify(ctx)) == -1){
    return NDBT_FAILED;
  }
  return result;
}



int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

#include "bank/Bank.hpp"

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int wait = 30; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performIncreaseTime(wait, yield);
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int wait = 10; // Max ms between each transaction
  int yield = 100; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performTransactions(wait, yield);
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int yield = 20; // Loops before bank returns 
  int result = NDBT_OK;

  while (ctx->isTestStopped() == false) {
    if (bank.performMakeGLs(yield) != NDBT_OK){
      ndbout << "bank.performMakeGLs FAILED" << endl;
      result = NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runBankSum(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int wait = 2000; // Max ms between each sum of accounts
  int yield = 1; // Loops before bank returns 
  int result = NDBT_OK;

  while (ctx->isTestStopped() == false) {
    if (bank.performSumAccounts(wait, yield) != NDBT_OK){
      ndbout << "bank.performSumAccounts FAILED" << endl;
      result = NDBT_FAILED;
    }
  }
  return result ;
}

int runDropBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runGrepBank(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int l = 0;
  int maxSleep = 30; // Max seconds between each grep
  Ndb* pNdb = GETNDB(step);
  NdbGrep grep(GETNDB(step)->getNodeId()+1);
  unsigned minGrepId = ~0;
  unsigned maxGrepId = 0;
  unsigned grepId = 0;
  int result = NDBT_OK;

  while (l < loops && result != NDBT_FAILED){

    if (pNdb->waitUntilReady() != 0){
      result = NDBT_FAILED;
      continue;
    }

    // Sleep for a while
    NdbSleep_SecSleep(maxSleep);
    
    // Perform grep
    if (grep.start() != 0){
      ndbout << "grep.start failed" << endl;
      result = NDBT_FAILED;
      continue;
    }
    ndbout << "Started grep " << grepId << endl;

    // Remember min and max grepid
    if (grepId < minGrepId)
      minGrepId = grepId;

    if (grepId > maxGrepId)
      maxGrepId = grepId;
    
    ndbout << " maxGrepId = " << maxGrepId 
	   << ", minGrepId = " << minGrepId << endl;
    ctx->setProperty("MinGrepId", minGrepId);    
    ctx->setProperty("MaxGrepId", maxGrepId);    
    
    l++;
  }

  ctx->stopTest();

  return result;
}
/*
int runRestoreBankAndVerify(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  NdbGrep grep(GETNDB(step)->getNodeId()+1);
  unsigned minGrepId = ctx->getProperty("MinGrepId");
  unsigned maxGrepId = ctx->getProperty("MaxGrepId");
  unsigned grepId = minGrepId;
  int result = NDBT_OK;
  int errSumAccounts = 0;
  int errValidateGL = 0;

  ndbout << " maxGrepId = " << maxGrepId << endl;
  ndbout << " minGrepId = " << minGrepId << endl;
  
  while (grepId <= maxGrepId){

    // TEMPORARY FIX
    // To erase all tables from cache(s)
    // To be removed, maybe replaced by ndb.invalidate();
    {
      Bank bank;
      
      if (bank.dropBank() != NDBT_OK){
	result = NDBT_FAILED;
	break;
      }
    }
    // END TEMPORARY FIX

    ndbout << "Performing initial restart" << endl;
    if (restarter.restartAll(true) != 0)
      return NDBT_FAILED;

    if (restarter.waitClusterStarted() != 0)
      return NDBT_FAILED;

    ndbout << "Restoring grep " << grepId << endl;
    if (grep.restore(grepId) == -1){
      return NDBT_FAILED;
    }
    ndbout << "Grep " << grepId << " restored" << endl;

    // Let bank verify
    Bank bank;

    int wait = 0;
    int yield = 1;
    if (bank.performSumAccounts(wait, yield) != 0){
      ndbout << "bank.performSumAccounts FAILED" << endl;
      ndbout << "  grepId = " << grepId << endl << endl;
      result = NDBT_FAILED;
      errSumAccounts++;
    }

    if (bank.performValidateAllGLs() != 0){
      ndbout << "bank.performValidateAllGLs FAILED" << endl;
      ndbout << "  grepId = " << grepId << endl << endl;
      result = NDBT_FAILED;
      errValidateGL++;
    }

    grepId++;
  }
  
  if (result != NDBT_OK){
    ndbout << "Verification of grep failed" << endl
	   << "  errValidateGL="<<errValidateGL<<endl
	   << "  errSumAccounts="<<errSumAccounts<<endl << endl;
  }
  
  return result;
}
*/

NDBT_TESTSUITE(testGrep);
TESTCASE("GrepBasic", 
	 "Test that Global Replication works on one table \n"
	 "1. Load table\n"
	 "2. Grep\n"
	 "3. Restart -i\n"
	 "4. Restore\n"
	 "5. Verify count and content of table\n"){
  INITIALIZER(runLoadTable);
  VERIFIER(runVerifyBasic);
  FINALIZER(runClearTable);

}

TESTCASE("GrepNodeRestart", 
	 "Test that Global Replication works on one table \n"
	 "1. Load table\n"
	 "2. Grep\n"
	 "3. Restart -i\n"
	 "4. Restore\n"
	 "5. Verify count and content of table\n"){
  INITIALIZER(runLoadTable);
  STEP(runPkUpdate);
  STEP(runRestarter);
  VERIFIER(runVerifyBasic);
  FINALIZER(runClearTable);
}


TESTCASE("GrepBank", 
	 "Test that grep and restore works during transaction load\n"
	 " by backing up the bank"
	 "1.  Create bank\n"
	 "2a. Start bank and let it run\n"
	 "2b. Perform loop number of greps of the bank\n"
	 "    when greps are finished tell bank to close\n"
	 "3.  Restart ndb -i and reload each grep\n"
	 "    let bank verify that the grep is consistent\n"
	 "4.  Drop bank\n"){
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEP(runBankTransactions);
  STEP(runBankGL);
  // TODO  STEP(runBankSum);
  STEP(runGrepBank);
  //  VERIFIER(runRestoreBankAndVerify);
  //  FINALIZER(runDropBank);

}

TESTCASE("NFMaster", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setMaster);
  STEP(runAbort);

}
TESTCASE("NFMasterAsSlave", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setMasterAsSlave);
  STEP(runAbort);

}
TESTCASE("NFSlave", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setSlave);
  STEP(runAbort);

}
TESTCASE("FailMaster", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setMaster);
  STEP(runFail);

}
TESTCASE("FailMasterAsSlave", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setMasterAsSlave);
  STEP(runFail);

}
TESTCASE("FailSlave", 
	 "Test that grep behaves during node failiure\n"){
  INITIALIZER(setSlave);
  STEP(runFail);

}
NDBT_TESTSUITE_END(testGrep);

int main(int argc, const char** argv){
  return testGrep.execute(argc, argv);
}


