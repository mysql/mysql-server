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
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include "ScanFunctions.hpp"
#include <random.h>

const NdbDictionary::Table *
getTable(Ndb* pNdb, int i){
  const NdbDictionary::Table* t = NDBT_Tables::getTable(i);
  if (t == NULL){
    return 0;
  }
  return pNdb->getDictionary()->getTable(t->getName());
}


int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){
  
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


int runCreateAllTables(NDBT_Context* ctx, NDBT_Step* step){

  int a = NDBT_Tables::createAllTables(GETNDB(step), false, true); 
  return a;
}

int runDropAllTablesExceptTestTable(NDBT_Context* ctx, NDBT_Step* step){

  for (int i=0; i < NDBT_Tables::getNumTables(); i++){

    const NdbDictionary::Table* tab = NDBT_Tables::getTable(i);
    if (tab == NULL){
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    // Don't drop test table
    if (strcmp(tab->getName(), ctx->getTab()->getName()) == 0){
      continue;
    }
	    
    int res = GETNDB(step)->getDictionary()->dropTable(tab->getName());
    if(res == -1){
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}


int runLoadAllTables(NDBT_Context* ctx, NDBT_Step* step){
  
  int records = ctx->getNumRecords();
  for (int i=0; i < NDBT_Tables::getNumTables(); i++){

    const NdbDictionary::Table* tab = getTable(GETNDB(step), i);
    if (tab == NULL){ 
      return NDBT_FAILED;
    }
    HugoTransactions hugoTrans(*tab);
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }    
  }
  return NDBT_OK;
}

int runScanReadRandomTable(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb");
  
  int i = 0;
  while (i<loops) {
    
    int tabNum = myRandom48(NDBT_Tables::getNumTables());
    const NdbDictionary::Table* tab = getTable(GETNDB(step), tabNum);
    if (tab == NULL){
      g_info << "tab == NULL" << endl;
      return NDBT_FAILED;
    }
    
    g_info << "Scan reading from table " << tab->getName() << endl;
    HugoTransactions hugoTrans(*tab);
    
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runInsertUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (hugoTrans.loadTable(GETNDB(step), records, 1) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runInsertDelete(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int loops = ctx->getNumLoops();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";    
    if (hugoTrans.loadTable(GETNDB(step), records, 1) != 0){
      result = NDBT_FAILED;
      break;
    }
    if (utilTrans.clearTable(GETNDB(step),  records) != 0){
      result = NDBT_FAILED;
      break;
    }
    i++;
  }

  ctx->stopTest();

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

int runScanDelete(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();

  int i = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (utilTrans.clearTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    // Load table, don't allow any primary key violations
    if (hugoTrans.loadTable(GETNDB(step), records, 512, false) != 0){
      return NDBT_FAILED;
    }
    i++;
  }  
  return NDBT_OK;
}

int runScanDelete2(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  
  int i = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  while (i<loops) {
    g_info << i << ": ";
    if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
      return NDBT_FAILED;
    }
    // Load table, don't allow any primary key violations
    if (hugoTrans.loadTable(GETNDB(step), records, 512, false) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runVerifyTable(NDBT_Context* ctx, NDBT_Step* step){
  return NDBT_OK;
}

int runScanRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb");

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadCommitted(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb");

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    if (hugoTrans.scanReadCommittedRecords(GETNDB(step), records, 
					   abort, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadError(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = 240; // Max parallelism
  int error = ctx->getProperty("ErrorCode");
  NdbRestarter restarter;
  
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    
    ndbout << "insertErrorInAllNodes("<<error<<")"<<endl;
    if (restarter.insertErrorInAllNodes(error) != 0){
      ndbout << "Could not insert error in all nodes "<<endl;
      return NDBT_FAILED;
    }
    
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, parallelism) != 0){
      result = NDBT_FAILED;
    }
    i++;
  }
  
  restarter.insertErrorInAllNodes(0);
  return result;
}
		     
int runScanReadErrorOneNode(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = 240; // Max parallelism
  int error = ctx->getProperty("ErrorCode");
  NdbRestarter restarter;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
      ctx->stopTest();
      return NDBT_OK;
  }

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && result == NDBT_OK) {
    g_info << i << ": ";
        
    int nodeId = restarter.getDbNodeId(lastId);
    lastId = (lastId + 1) % restarter.getNumDbNodes();
    ndbout << "insertErrorInNode("<<nodeId<<", "<<error<<")"<<endl;
    if (restarter.insertErrorInNode(nodeId, error) != 0){
      ndbout << "Could not insert error in node="<<nodeId<<endl;
      return NDBT_FAILED;
    }
    
    for (int j=0; j<10; j++){
      if (hugoTrans.scanReadRecords(GETNDB(step), 
				    records, 0, parallelism) != 0)
	result = NDBT_FAILED;
    }
	

    if(restarter.waitClusterStarted(120) != 0){
      g_err << "Cluster failed to restart" << endl;
      result = NDBT_FAILED;
    }
    restarter.insertErrorInAllNodes(0);
    
    i++;
  }
  restarter.insertErrorInAllNodes(0);
  return result;
}

int runRestartAll(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;

  if (restarter.restartAll() != 0){
    ndbout << "Could not restart all nodes"<<endl;
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted(120) != 0){
    ndbout << "Could not restarted" << endl;
    return NDBT_FAILED;
  }
      
  return NDBT_OK;
}

static int RANDOM_PARALLELISM = 9999;

int runScanReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;

  int parallelism = ctx->getProperty("Parallelism", 240);
  int para = parallelism;

  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    if (parallelism == RANDOM_PARALLELISM)
      para = myRandom48(239)+1;

    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, para) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadUntilStoppedNoCount(NDBT_Context* ctx, NDBT_Step* step){
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), 0) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadUntilStoppedPrintTime(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  int parallelism = ctx->getProperty("Parallelism", 240);
  NdbTimer timer;
  Ndb* ndb = GETNDB(step);


  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    timer.doReset();
    timer.doStart();
    g_info << i << ": ";
    if (ndb->waitUntilReady() != 0)
      return NDBT_FAILED;      
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, parallelism) != 0)
      return NDBT_FAILED;
    timer.doStop();
    if ((timer.elapsedTime()/1000) > 1)
      timer.printTotalTime();
    i++;
  }
  return NDBT_OK;
}


int runPkRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanUpdate(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int abort = ctx->getProperty("AbortProb");
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";

    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, abort, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;

  int parallelism = ctx->getProperty("Parallelism", 240);
  int para = parallelism;

  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    if (parallelism == RANDOM_PARALLELISM)
      para = myRandom48(239)+1;

    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, 0, para) == NDBT_FAILED){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}


int runScanUpdate2(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb");
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords2(GETNDB(step), records, abort, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runLocker(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  
  if (hugoTrans.lockRecords(GETNDB(step), records, 5, 500) != 0){
    result = NDBT_FAILED;
  }
  ctx->stopTest();
  
  return result;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;
  int timeout = 240;

  if (restarter.getNumDbNodes() < 2){
      ctx->stopTest();
      return NDBT_OK;
  }
  while(i<loops && result != NDBT_FAILED){
    if(restarter.waitClusterStarted(timeout) != 0){
      g_err << "Cluster failed to start 1" << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_SecSleep(10);
    
    int nodeId = restarter.getDbNodeId(lastId);
    lastId = (lastId + 1) % restarter.getNumDbNodes();
    if(restarter.restartOneDbNode(nodeId) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    
    i++;
  }
  if(restarter.waitClusterStarted(timeout) != 0){
    g_err << "Cluster failed to start 2" << endl;
    result = NDBT_FAILED;
  }

  ctx->stopTest();
  
  return result;
}


int runStopAndStartNode(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;
  int timeout = 240;

  if (restarter.getNumDbNodes() < 2){
      ctx->stopTest();
      return NDBT_OK;
  }
  while(i<loops && result != NDBT_FAILED){
    if(restarter.waitClusterStarted(timeout) != 0){
      g_err << "Cluster failed to start 1" << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_SecSleep(1);
    int nodeId = restarter.getDbNodeId(lastId);
    lastId = (lastId + 1) % restarter.getNumDbNodes();
    g_err << "Stopping node " << nodeId << endl;

    if(restarter.restartOneDbNode(nodeId, false, true) != 0){
      g_err << "Failed to restartOneDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }
 
    if(restarter.waitNodesNoStart(&nodeId, 1, timeout) != 0){
      g_err << "Node failed to reach NoStart" << endl;
      result = NDBT_FAILED;
      break;
    }   

    g_info << "Sleeping for 10 secs" << endl;
    NdbSleep_SecSleep(10);
    
    g_err << "Starting node " << nodeId << endl;
    if(restarter.startNodes(&nodeId, 1) != 0){
      g_err << "Failed to start the node" << endl;
      result = NDBT_FAILED;
      break;
    }    

    i++;
  }
  if(restarter.waitClusterStarted(timeout) != 0){
    g_err << "Cluster failed to start 2" << endl;
    result = NDBT_FAILED;
  }

  ctx->stopTest();
  
  return result;
}

int runRestarter9999(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
      ctx->stopTest();
      return NDBT_OK;
  }
  while(i<loops && result != NDBT_FAILED){
    if(restarter.waitClusterStarted(120) != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_SecSleep(10);
    
    int nodeId = restarter.getDbNodeId(lastId);
    lastId = (lastId + 1) % restarter.getNumDbNodes();
    if(restarter.insertErrorInNode(nodeId, 9999) != 0){
      g_err << "Failed to insertErrorInNode="<<nodeId << endl;
      result = NDBT_FAILED;
      break;
    }    
    NdbSleep_SecSleep(10);
    i++;
  }
  if(restarter.waitClusterStarted(120) != 0){
    g_err << "Cluster failed to start" << endl;
    result = NDBT_FAILED;
  }

  ctx->stopTest();
  
  return result;
}


int runCheckGetValue(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int records = ctx->getNumRecords();
  int numFailed = 0;
  AttribList alist; 
  alist.buildAttribList(pTab);
  UtilTransactions utilTrans(*pTab);  
  for(size_t i = 0; i < alist.attriblist.size(); i++){
    g_info << (unsigned)i << endl;
    if(utilTrans.scanReadRecords(GETNDB(step), 
				 parallelism,
				 false,
				 records,
				 alist.attriblist[i]->numAttribs,
				 alist.attriblist[i]->attribs) != 0){
      numFailed++;
    }
    if(utilTrans.scanReadRecords(GETNDB(step), 
				 parallelism,
				 true,
				 records,
				 alist.attriblist[i]->numAttribs,
				 alist.attriblist[i]->attribs) != 0){
      numFailed++;
    }
  }
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runCloseWithoutStop(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;
  ScanFunctions scanF(*pTab);
  // Iterate over all possible parallelism valuse
  for (int p = 1; p<240; p++){
    g_info << p << " CloseWithoutStop openScan" << endl;
    if (scanF.scanReadFunctions(GETNDB(step), 
				records, 
				p,
				ScanFunctions::CloseWithoutStop,
				false) != 0){
      numFailed++;
    }
    g_info << p << " CloseWithoutStop openScanExclusive" << endl;
    if (scanF.scanReadFunctions(GETNDB(step), 
				records, 
				p,
				ScanFunctions::CloseWithoutStop,
				true) != 0){
      numFailed++;
    }
  }
    
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runNextScanWhenNoMore(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;
  ScanFunctions scanF(*pTab);
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::NextScanWhenNoMore,
			      false) != 0){
    numFailed++;
  }
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::NextScanWhenNoMore,
			      true) != 0){
    numFailed++;
  }
  
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runEqualAfterOpenScan(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;
  ScanFunctions scanF(*pTab);
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::EqualAfterOpenScan,
			      false) == NDBT_OK){
    numFailed++;
  }
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::EqualAfterOpenScan,
			      true) == NDBT_OK){
    numFailed++;
  }
  
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runOnlyOpenScanOnce(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;
  ScanFunctions scanF(*pTab);
  g_info << "OnlyOpenScanOnce openScanRead" << endl;
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::OnlyOpenScanOnce,
			      false) == 0){
    numFailed++;
  }
  g_info << "OnlyOpenScanOnce openScanExclusive" << endl;
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      6,
			      ScanFunctions::OnlyOpenScanOnce,
			      true) == 0){
    numFailed++;
  }
  
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runOnlyOneOpInScanTrans(NDBT_Context* ctx, NDBT_Step* step){
  return NDBT_OK;
}

int runExecuteScanWithoutOpenScan(NDBT_Context* ctx, NDBT_Step* step){
  return NDBT_OK;
}

int runOnlyOneOpBeforeOpenScan(NDBT_Context* ctx, NDBT_Step* step){
    return NDBT_OK;
}

int runOnlyOneScanPerTrans(NDBT_Context* ctx, NDBT_Step* step){
  return NDBT_OK;
}

int runNoCloseTransaction(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int numFailed = 0;

  ScanFunctions scanF(*pTab);
  int l = 0;
  while(l < loops){
    if (scanF.scanReadFunctions(GETNDB(step), 
				records, 
				6,
				ScanFunctions::NoCloseTransaction,
				false) != 0){
      numFailed++;
    }
    if (scanF.scanReadFunctions(GETNDB(step), 
				records, 
				6,
				ScanFunctions::NoCloseTransaction,
				true) != 0){
      numFailed++;
    }
    l++;
  }
  
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;

}

int runCheckInactivityTimeOut(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;

  ScanFunctions scanF(*pTab);
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      1,
			      ScanFunctions::CheckInactivityTimeOut,
			      false) != NDBT_OK){
    numFailed++;
  }
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      240,
			      ScanFunctions::CheckInactivityTimeOut,
			      true) != NDBT_OK){
    numFailed++;
  }
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;

}

int runCheckInactivityBeforeClose(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table*  pTab = ctx->getTab();
  int records = ctx->getNumRecords();
  int numFailed = 0;

  ScanFunctions scanF(*pTab);
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      16,
			      ScanFunctions::CheckInactivityBeforeClose,
			      false) != 0){
    numFailed++;
  }
  if (scanF.scanReadFunctions(GETNDB(step), 
			      records, 
			      240,
			      ScanFunctions::CheckInactivityBeforeClose,
			      true) != 0){
    numFailed++;
  }
  
  if(numFailed > 0)
    return NDBT_FAILED;
  else
    return NDBT_OK;

}

int runScanRestart(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb * pNdb = GETNDB(step);
  const NdbDictionary::Table*  pTab = ctx->getTab();

  HugoCalculator calc(* pTab);
  NDBT_ResultRow tmpRow(* pTab);

  int i = 0;
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i++ << ": ";
    const int record = (rand() % records);
    g_info << " row=" << record;

    NdbConnection* pCon = pNdb->startTransaction();
    NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());	
    if (pOp == NULL) {
      ERR(pCon->getNdbError());
      return NDBT_FAILED;
    }
    
    NdbResultSet* rs = pOp->readTuples();
    if( rs == 0 ) {
      ERR(pCon->getNdbError());
      return NDBT_FAILED;
    }
  
    int check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pCon->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define attributes to read  
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      if((tmpRow.attributeStore(a) = 
	  pOp->getValue(pTab->getColumn(a)->getName())) == 0) {
	ERR(pCon->getNdbError());
	return NDBT_FAILED;
      }
    } 
    
    check = pCon->execute(NoCommit);
    if( check == -1 ) {
      ERR(pCon->getNdbError());
      return NDBT_FAILED;
    }

    int res;
    int row = 0;
    while(row < record && (res = rs->nextResult()) == 0) {
      if(calc.verifyRowValues(&tmpRow) != 0){
	abort();
	return NDBT_FAILED;
      }
      row++;
    }
    if(row != record){
      ERR(pCon->getNdbError());
      abort();
      return NDBT_FAILED;
    }
    g_info << " restarting" << endl;
    if((res = rs->restart()) != 0){
      ERR(pCon->getNdbError());
      abort();
      return NDBT_FAILED;
    }      

    row = 0;
    while((res = rs->nextResult()) == 0) {
      if(calc.verifyRowValues(&tmpRow) != 0){
	abort();
	return NDBT_FAILED;
      }
      row++;
    }
    if(res != 1 || row != records){
      ERR(pCon->getNdbError());
      abort();
      return NDBT_FAILED;
    }
    pCon->close();
  }
  return NDBT_OK;
}


NDBT_TESTSUITE(testScan);
TESTCASE("ScanRead", 
	 "Verify scan requirement: It should be possible "\
	 "to read all records in a table without knowing their "\
	 "primary key."){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 1);
  STEP(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead16", 
	 "Verify scan requirement: It should be possible to scan read "\
	 "with parallelism, test with parallelism 16"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 16);
  STEP(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead240", 
	 "Verify scan requirement: It should be possible to scan read with "\
	 "parallelism, test with parallelism 240(240 would automatically be "\
	 "downgraded to the maximum parallelism value for the current config)"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadCommitted240", 
	 "Verify scan requirement: It should be possible to scan read committed with "\
	 "parallelism, test with parallelism 240(240 would automatically be "\
	 "downgraded to the maximum parallelism value for the current config)"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanReadCommitted);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdate", 
	 "Verify scan requirement: It should be possible "\
	 "to update all records in a table without knowing their"\
	 " primary key."){
  INITIALIZER(runLoadTable);
  STEP(runScanUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdate2", 
	 "Verify scan requirement: It should be possible "\
	 "to update all records in a table without knowing their"\
	 " primary key. Do this efficently by calling nextScanResult(false) "\
	 "in order to update the records already fetched to the api in one batch."){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanUpdate2);
  FINALIZER(runClearTable);
}
TESTCASE("ScanDelete", 
	 "Verify scan requirement: It should be possible "\
	 "to delete all records in a table without knowing their"\
	 " primary key."){
  INITIALIZER(runLoadTable);
  STEP(runScanDelete);
  FINALIZER(runClearTable);
}
TESTCASE("ScanDelete2", 
	 "Verify scan requirement: It should be possible "\
	 "to delete all records in a table without knowing their"\
	 " primary key. Do this efficently by calling nextScanResult(false) "\
	 "in order to delete the records already fetched to the api in one batch."){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanDelete2);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdateAndScanRead", 
	 "Verify scan requirement: It should be possible to run "\
	 "scan read and scan update at the same time"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 16);
  STEP(runScanRead);
  STEP(runScanUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadAndLocker", 
	 "Verify scan requirement: The locks are not kept throughout "\
	 "the entire scan operation. This means that a scan does not "\
	 "lock the entire table, only the records it's currently "\
	 "operating on. This will test how scan performs when there are "\
	 " a number of 1 second locks in the table"){
  INITIALIZER(runLoadTable);
  STEP(runScanReadUntilStopped);
  STEP(runLocker);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadAndPkRead", 
	 "Verify scan requirement: The locks are not kept throughout "\
	 "the entire scan operation. This means that a scan does not "\
	 "lock the entire table, only the records it's currently "\
	 "operating on. This will test how scan performs when there are "\
	 " a pk reads "){
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 2);
  STEPS(runPkRead, 2);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead488", 
	 "Verify scan requirement: It's only possible to have 11 concurrent "\
	 "scans per fragment running in Ndb kernel at the same time. "\
	 "When this limit is exceeded the scan will be aborted with errorcode "\
	 "488."){
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 70);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead488Timeout", 
	 ""){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5034);
  STEPS(runScanRead, 30);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead40", 
	 "Verify scan requirement: Scan with 40 simultaneous threads"){
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 40);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead100", 
	 "Verify scan requirement: Scan with 100 simultaneous threads"){
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 100);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead40RandomTable", 
	 "Verify scan requirement: Scan with 40 simultaneous threads. "\
	 "Use random table for the scan"){
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAllTables);
  STEPS(runScanReadRandomTable, 40);
  FINALIZER(runDropAllTablesExceptTestTable);
}
TESTCASE("ScanRead100RandomTable", 
	 "Verify scan requirement: Scan with 100 simultaneous threads. "\
	 "Use random table for the scan"){
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAllTables);
  STEPS(runScanReadRandomTable, 100);
  FINALIZER(runDropAllTablesExceptTestTable);
}
TESTCASE("ScanReadRandomPrepare",
	 "Create and load tables for ScanRead40RandomNoTableCreate."){
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAllTables);
}
TESTCASE("ScanRead40RandomNoTableCreate", 
	 "Verify scan requirement: Scan with 40 simultaneous threads. "\
	 "Use random table for the scan. Dont create or load the tables."){
  STEPS(runScanReadRandomTable, 40);
}
TESTCASE("ScanRead100RandomNoTableCreate", 
	 "Verify scan requirement: Scan with 100 simultaneous threads. "\
	 "Use random table for the scan. Dont create or load the tables."){
  STEPS(runScanReadRandomTable, 100);
}
TESTCASE("ScanWithLocksAndInserts", 
	 "TR457: This test is added to verify that an insert of a records "\
	 "that is already in the database does not delete the record"){  
  INITIALIZER(runLoadTable);
  STEPS(runScanReadUntilStopped, 2);
  STEP(runLocker);
  STEP(runInsertUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadAbort", 
	 "Scan requirement: A scan may be aborted by the application "\
	 "at any time. This can be performed even if there are more "\
	 "tuples to scan."){  
  INITIALIZER(runLoadTable);
  TC_PROPERTY("AbortProb", 90);
  STEPS(runScanRead, 3);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadAbort15", 
	 "Scan requirement: A scan may be aborted by the application "\
	 "at any time. This can be performed even if there are more "\
	 "tuples to scan. Use parallelism 15"){  
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 15);
  TC_PROPERTY("AbortProb", 90);
  STEPS(runScanRead, 3);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadAbort240", 
	 "Scan requirement: A scan may be aborted by the application "\
	 "at any time. This can be performed even if there are more "\
	 "tuples to scan. Use parallelism 240(it will be downgraded to max para for this config)"){  
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  TC_PROPERTY("AbortProb", 90);
  STEPS(runScanRead, 3);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdateAbort16", 
	 "Scan requirement: A scan may be aborted by the application "\
	 "at any time. This can be performed even if there are more "\
	 "tuples to scan. Use parallelism 16"){  
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 16);
  TC_PROPERTY("AbortProb", 90);
  STEPS(runScanUpdate, 3);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdateAbort240", 
	 "Scan requirement: A scan may be aborted by the application "\
	 "at any time. This can be performed even if there are more "\
	 "tuples to scan. Use parallelism 240(it will be downgraded to max para for this config)"){  
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  TC_PROPERTY("AbortProb", 90);
  STEPS(runScanUpdate, 3);
  FINALIZER(runClearTable);
}
TESTCASE("CheckGetValue", 
	 "Check that we can call getValue to read attributes"\
	 "Especially interesting to see if we can read only the"\
	 " first, last or any two attributes from the table"){
  INITIALIZER(runLoadTable);
  STEP(runCheckGetValue);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("CloseWithoutStop", 
	 "Check that we can close the scanning transaction without calling "\
	 "stopScan"){
  INITIALIZER(runLoadTable);
  STEP(runCloseWithoutStop);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("NextScanWhenNoMore", 
	 "Check that we can call nextScanResult when there are no more "\
	 "records, and that it returns a valid value"){
  INITIALIZER(runLoadTable);
  STEP(runNextScanWhenNoMore);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("EqualAfterOpenScan", 
	 "Check that we can't call equal after openScan"){
  STEP(runEqualAfterOpenScan);
}
TESTCASE("ExecuteScanWithoutOpenScan", 
	 "Check that we can't call executeScan without defining a scan "\
         "with openScan"){
  INITIALIZER(runLoadTable);
  STEP(runExecuteScanWithoutOpenScan);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("OnlyOpenScanOnce", 
	 "Check that we may only call openScan once in the same trans"){
  INITIALIZER(runLoadTable);
  STEP(runOnlyOpenScanOnce);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("OnlyOneOpInScanTrans", 
	 "Check that we can have only one operation in a scan trans"){
  INITIALIZER(runLoadTable);
  STEP(runOnlyOneOpInScanTrans);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("OnlyOneOpBeforeOpenScan", 
	 "Check that we can have only one operation in a trans defined "\
	 "when calling openScan "){
  INITIALIZER(runLoadTable);
  STEP(runOnlyOneOpBeforeOpenScan);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("OnlyOneScanPerTrans", 
	 "Check that we can have only one scan operation in a trans"){
  INITIALIZER(runLoadTable);
  STEP(runOnlyOneScanPerTrans);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("NoCloseTransaction", 
	 "Check behaviour when close transaction is not called "){
  INITIALIZER(runLoadTable);
  STEP(runNoCloseTransaction);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("CheckInactivityTimeOut", 
	 "Check behaviour when the api sleeps for a long time before continuing scan "){
  INITIALIZER(runLoadTable);
  STEP(runCheckInactivityTimeOut);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("CheckInactivityBeforeClose", 
	 "Check behaviour when the api sleeps for a long time before calling close scan "){
  INITIALIZER(runLoadTable);
  STEP(runCheckInactivityBeforeClose);
  VERIFIER(runScanRead);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5021", 
	 "Scan and insert error 5021, one node is expected to crash"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5021);
  STEP(runScanReadErrorOneNode);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5022", 
	 "Scan and insert error 5022, one node is expected to crash"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5022);
  TC_PROPERTY("NodeNumber", 2);
  STEP(runScanReadErrorOneNode);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5023", 
	 "Scan and insert error 5023"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5023);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5024", 
	 "Scan and insert error 5024"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5024);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5025", 
	 "Scan and insert error 5025"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5025);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError5030", 
	 "Scan and insert error 5030."\
	 "Drop all SCAN_NEXTREQ signals in LQH until the node is "\
	 "shutdown with SYSTEM_ERROR because of scan fragment timeout"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 5030);
  STEP(runScanReadErrorOneNode);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadRestart", 
	 "Scan requirement:A scan should be able to start and "\
	 "complete during node recovery and when one or more nodes "\
	 "in the cluster is down.Use random parallelism "){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", RANDOM_PARALLELISM); // Random
  STEP(runScanReadUntilStopped);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdateRestart", 
	 "Scan requirement:A scan should be able to start and "\
	 "complete during node recovery and when one or more nodes "\
	 "in the cluster is down. Use random parallelism"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", RANDOM_PARALLELISM); // Random
  STEP(runScanUpdateUntilStopped);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
#if 0
TESTCASE("ScanReadRestart9999", 
	 "Scan requirement:A scan should be able to start and "\
	 "complete during node recovery and when one or more nodes "\
	 "in the cluster is down. Use parallelism 240."\
	 "Restart using error insert 9999"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanReadUntilStopped);
  STEP(runRestarter9999);
  FINALIZER(runClearTable);
}
TESTCASE("ScanUpdateRestart9999", 
	 "Scan requirement:A scan should be able to start and "\
	 "complete during node recovery and when one or more nodes "\
	 "in the cluster is down. Use parallelism 240."\
	 "Restart using error insert 9999"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  STEP(runRestarter9999);
  FINALIZER(runClearTable);
}
#endif
TESTCASE("InsertDelete", 
	 "Load and delete all while scan updating and scan reading\n"\
	 "Alexander Lukas special"){
  INITIALIZER(runClearTable);
  STEP(runScanReadUntilStoppedNoCount);
  STEP(runScanUpdateUntilStopped);
  STEP(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("CheckAfterTerror", 
	 "Check that we can still scan read after this terror of NdbApi"){
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 5);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadWhileNodeIsDown", 
	 "Scan requirement:A scan should be able to run as fast when  "\
	 "one or more nodes in the cluster is down."){
  INITIALIZER(runLoadTable);
  STEP(runScanReadUntilStoppedPrintTime);
  STEP(runStopAndStartNode);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRestart", 
	 "Verify restart functionallity"){
  INITIALIZER(runLoadTable);
  STEP(runScanRestart);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testScan);

int main(int argc, const char** argv){
  myRandom48Init(NdbTick_CurrentMillisecond());
  return testScan.execute(argc, argv);
}

template class Vector<Attrib*>;
