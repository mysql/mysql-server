/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include "ScanFunctions.hpp"
#include <random.h>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbConfig.hpp>

const NdbDictionary::Table *
getTable(Ndb* pNdb, int i){
  const NdbDictionary::Table* t = NDBT_Tables::getTable(i);
  if (t == NULL){
    return 0;
  }
  return pNdb->getDictionary()->getTable(t->getName());
}


int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){
  
  int records = ctx->getProperty("Rows", ctx->getNumRecords());

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

    GETNDB(step)->getDictionary()->dropTable(tab->getName());
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

char orderedPkIdxName[255];

int createOrderedPkIndex(NDBT_Context* ctx, NDBT_Step* step){

  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  
  // Create index    
  BaseString::snprintf(orderedPkIdxName, sizeof(orderedPkIdxName), 
		       "IDC_O_PK_%s", pTab->getName());
  NdbDictionary::Index pIdx(orderedPkIdxName);
  pIdx.setTable(pTab->getName());
  pIdx.setType(NdbDictionary::Index::OrderedIndex);
  pIdx.setLogging(false);

  for (int c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey()){
      pIdx.addIndexColumn(col->getName());
    }
  }
  
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    ndbout << "FAILED! to create index" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    ERR(err);
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int createOrderedPkIndex_Drop(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  
  // Drop index
  if (pNdb->getDictionary()->dropIndex(orderedPkIdxName, 
				       pTab->getName()) != 0){
    ndbout << "FAILED! to drop index" << endl;
    ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}


int runScanReadRandomTable(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb", 5);
  
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

int runScanReadRandomTableExceptTestTable(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb", 5);
  
  int i = 0;
  while (i<loops) {
    const NdbDictionary::Table* tab= NULL;
    bool chosenTable=false;
    while (!chosenTable)
    {
      int tabNum = myRandom48(NDBT_Tables::getNumTables());
      tab = getTable(GETNDB(step), tabNum);
      if (tab == NULL){
        g_info << "tab == NULL" << endl;
        return NDBT_FAILED;
      }
      // Skip test table
      chosenTable= (strcmp(tab->getName(), ctx->getTab()->getName()));
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
  int records = ctx->getProperty("Rows", ctx->getNumRecords());
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb", 5);
  int tupscan = ctx->getProperty("TupScan", (Uint32)0);
  int lockmode = ctx->getProperty("LockMode", NdbOperation::LM_CommittedRead);

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";

    int scan_flags = 0;
    if (tupscan == 1)
    {
      scan_flags |= NdbScanOperation::SF_TupScan;
      if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism,
                                    NdbOperation::LockMode(lockmode),
                                    scan_flags) != 0)
        return NDBT_FAILED;
    }
    else if (hugoTrans.scanReadRecords(GETNDB(step), records, abort,
                                       parallelism,
                                       NdbOperation::LockMode(lockmode)) != 0)
    {
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runRandScanRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb", 5);
  int tupscan = ctx->getProperty("TupScan", (Uint32)0);
  int lmarg = ctx->getProperty("LockMode", ~Uint32(0));
  int nocount = ctx->getProperty("NoCount", Uint32(0));

  if (nocount)
    records = 0;

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    NdbOperation::LockMode lm = (NdbOperation::LockMode)(rand() % 3);
    if (lmarg != ~0)
      lm = (NdbOperation::LockMode)lmarg;
    int scan_flags = 0;
  
    if (tupscan == 1)
      scan_flags |= NdbScanOperation::SF_TupScan;
    else if (tupscan == 2 && ((rand() & 0x800)))
    {
      scan_flags |= NdbScanOperation::SF_TupScan;
    }

    if (hugoTrans.scanReadRecords(GETNDB(step),
				  records, abort, parallelism,
				  lm,
				  scan_flags) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadIndex(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getProperty("Rows", ctx->getNumRecords());
  int parallelism = ctx->getProperty("Parallelism", 240);
  int abort = ctx->getProperty("AbortProb", 5);
  int lockmode = ctx->getProperty("LockMode", NdbOperation::LM_CommittedRead);
  int rand_mode = ctx->getProperty("RandScanOptions", Uint32(1));
  const NdbDictionary::Index * pIdx =
    GETNDB(step)->getDictionary()->getIndex(orderedPkIdxName,
					    ctx->getTab()->getName());

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (pIdx && i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    bool sort = (rand() % 100) > 50 ? true : false;
    bool desc = (rand() % 100) > 50 ? true : false;
    NdbOperation::LockMode lm = (NdbOperation::LockMode)(rand() % 3);
    desc = false;       // random causes too many deadlocks
    if (rand_mode == 0)
    {
      sort = false;
      desc = false;
      lm = (NdbOperation::LockMode)lockmode;
    }
    int scan_flags =
      (NdbScanOperation::SF_OrderBy & -(int)sort) |
      (NdbScanOperation::SF_Descending & -(int)desc);
    if (hugoTrans.scanReadRecords(GETNDB(step), pIdx,
				  records, abort, parallelism,
				  lm,
				  scan_flags) != 0){
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
  int abort = ctx->getProperty("AbortProb", 5);
  bool tupScan = ctx->getProperty("TupScan");
  int scan_flags = (NdbScanOperation::SF_TupScan & -(int)tupScan);

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 
				  abort, parallelism, 
				  NdbOperation::LM_CommittedRead,
                                  scan_flags) != 0){
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

int runScanReadExhaust(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = 240; // Max parallelism
  int error = 8093;
  NdbRestarter restarter;
  
  /* First take a TC resource snapshot */
  int savesnapshot= DumpStateOrd::TcResourceSnapshot;
  int checksnapshot= DumpStateOrd::TcResourceCheckLeak;
  
  restarter.dumpStateAllNodes(&savesnapshot, 1);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  hugoTrans.setRetryMax(1);
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";
    
    ndbout << "insertErrorInAllNodes("<<error<<")"<<endl;
    if (restarter.insertErrorInAllNodes(error) != 0){
      ndbout << "Could not insert error in all nodes "<<endl;
      return NDBT_FAILED;
    }
    
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, parallelism) == 0)
    {
      /* Expect error 291 */
      result = NDBT_FAILED;
      break;
    }
    i++;
  }
  
  restarter.insertErrorInAllNodes(0);

  restarter.dumpStateAllNodes(&checksnapshot, 1);
  return result;
}

int
runInsertError(NDBT_Context* ctx, NDBT_Step* step){
  int error = ctx->getProperty("ErrorCode");
  NdbRestarter restarter;

  ctx->setProperty("ErrorCode", (Uint32)0);
  if (restarter.insertErrorInAllNodes(error) != 0){
    ndbout << "Could not insert error in all nodes "<<endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
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
  int abort = ctx->getProperty("AbortProb", 5);
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
  //int records = ctx->getNumRecords();
  int i = 0;

  int parallelism = ctx->getProperty("Parallelism", 240);
  int para = parallelism;

  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    if (parallelism == RANDOM_PARALLELISM)
      para = myRandom48(239)+1;

    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords(GETNDB(step), 0, 0, para) == NDBT_FAILED){
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
  int abort = ctx->getProperty("AbortProb", 5);
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
    if(restarter.restartOneDbNode(nodeId, false, false, true) != 0){
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
				 NdbOperation::LM_Read,
				 records,
				 alist.attriblist[i]->numAttribs,
				 alist.attriblist[i]->attribs) != 0){
      numFailed++;
    }
    if(utilTrans.scanReadRecords(GETNDB(step), 
				 parallelism,
				 NdbOperation::LM_Read,
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

int 
runScanParallelism(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops() + 3;
  int records = ctx->getNumRecords();
  int abort = ctx->getProperty("AbortProb", 15);
  
  Uint32 fib[] = { 1, 2 };
  Uint32 parallelism = 0; // start with 0
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops && !ctx->isTestStopped()) {
    g_info << i << ": ";

    if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism,
				  NdbOperation::LM_Read) != 0){
      return NDBT_FAILED;
    }
    if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism,
				  NdbOperation::LM_Exclusive) != 0){
      return NDBT_FAILED;
    }
    if (hugoTrans.scanReadRecords(GETNDB(step), records, abort, parallelism,
				  NdbOperation::LM_CommittedRead) != 0){
      return NDBT_FAILED;
    }
    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, abort, parallelism)
	!= 0){
      return NDBT_FAILED;
    }
    i++;
    parallelism = fib[0];
    Uint32 next = fib[0] + fib[1];
    fib[0] = fib[1];
    fib[1] = next;
  }
  return NDBT_OK;
}

int
runScanVariants(NDBT_Context* ctx, NDBT_Step* step)
{
  //int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  Ndb * pNdb = GETNDB(step);
  const NdbDictionary::Table*  pTab = ctx->getTab();
  
  HugoCalculator calc(* pTab);
  NDBT_ResultRow tmpRow(* pTab);

  for(int lm = 0; lm <= NdbOperation::LM_CommittedRead; lm++)
  {
    for(int flags = 0; flags < 4; flags++)
    {
      for (int batch = 0; batch < 100; batch += (1 + batch + (batch >> 3)))
      {
	for (int par = 0; par < 16; par += 1 + (rand() % 3))
	{
	  bool disk = flags & 1;
	  bool tups = flags & 2;
	  g_info << "lm: " << lm 
		 << " disk: " << disk 
		 << " tup scan: " << tups 
		 << " par: " << par 
		 << " batch: " << batch 
		 << endl;
	  
	  NdbConnection* pCon = pNdb->startTransaction();
	  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
	  if (pOp == NULL) {
	    ERR(pCon->getNdbError());
	    return NDBT_FAILED;
	  }
	  
	  if( pOp->readTuples((NdbOperation::LockMode)lm,
			      tups ? NdbScanOperation::SF_TupScan : 0,
			      par,
			      batch) != 0) 
	  {
	    ERR(pCon->getNdbError());
	    return NDBT_FAILED;
	  }
	  
	  // Define attributes to read  
	  bool found_disk = false;
	  for(int a = 0; a<pTab->getNoOfColumns(); a++){
	    if (pTab->getColumn(a)->getStorageType() == 
		NdbDictionary::Column::StorageTypeDisk)
	    {
	      found_disk = true;
	      if (!disk)
		continue;
	    }
	    
	    if((pOp->getValue(pTab->getColumn(a)->getName())) == 0) {
	      ERR(pCon->getNdbError());
	      return NDBT_FAILED;
	    }
	  } 
	  
	  if (! (disk && !found_disk))
	  {
	    int check = pCon->execute(NoCommit);
	    if( check == -1 ) {
	      ERR(pCon->getNdbError());
	      return NDBT_FAILED;
	    }
	    
	    int res;
	    //int row = 0;
	    while((res = pOp->nextResult()) == 0);
	  }
	  pCon->close();
	}
      }
    }
  }
  return NDBT_OK;
}

int
runBug36124(NDBT_Context* ctx, NDBT_Step* step){
  Ndb * pNdb = GETNDB(step);
  const NdbDictionary::Table*  pTab = ctx->getTab();

  NdbTransaction* pCon = pNdb->startTransaction();
  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
  if (pOp == NULL) {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }
  
  if( pOp->readTuples(NdbOperation::LM_Read) != 0) 
  {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }

  if( pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }

  /* Old style interpreted code api should fail when 
   * we try to use it 
   */
  if( pOp->interpret_exit_last_row() == 0)
  {
    return NDBT_FAILED;
  }

  pOp->close();

  pCon->close();

  return NDBT_OK;
}

int
runBug24447(NDBT_Context* ctx, NDBT_Step* step){
  int loops = 1; //ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int abort = ctx->getProperty("AbortProb", 15);
  NdbRestarter restarter;
  HugoTransactions hugoTrans(*ctx->getTab());
  int i = 0;
  while (i<loops && !ctx->isTestStopped()) 
  {
    g_info << i++ << ": ";

    int nodeId = restarter.getRandomNotMasterNodeId(rand());
    if (nodeId == -1)
      nodeId = restarter.getMasterNodeId();
    if (restarter.insertErrorInNode(nodeId, 8038) != 0)
    {
      ndbout << "Could not insert error in node="<<nodeId<<endl;
      return NDBT_FAILED;
    }

    for (Uint32 j = 0; j<10; j++)
    {
      hugoTrans.scanReadRecords(GETNDB(step), records, abort, 0, 
				NdbOperation::LM_CommittedRead);
    }

  }
  restarter.insertErrorInAllNodes(0);
  
  return NDBT_OK;
}

int runBug42545(NDBT_Context* ctx, NDBT_Step* step){

  int loops = ctx->getNumLoops();

  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  const NdbDictionary::Index * pIdx = 
    GETNDB(step)->getDictionary()->getIndex(orderedPkIdxName, 
					    ctx->getTab()->getName());
  

  int i = 0;
  while (pIdx && i++ < loops && !ctx->isTestStopped()) 
  {
    g_info << i << ": ";
    NdbTransaction* pTrans = pNdb->startTransaction();
    int nodeId = pTrans->getConnectedNodeId();
    
    {
      Uint32 cnt = 0;
      Vector<NdbTransaction*> translist;
      while (cnt < 3)
      {
        NdbTransaction* p2 = pNdb->startTransaction();
        translist.push_back(p2);
        if (p2->getConnectedNodeId() == (Uint32)nodeId)
          cnt++;
      }
      
      for (size_t t = 0; t < translist.size(); t++)
        translist[t]->close();
      translist.clear();
    }

    NdbIndexScanOperation* 
      pOp = pTrans->getNdbIndexScanOperation(pIdx, ctx->getTab());
    
    int r0 = pOp->readTuples(NdbOperation::LM_CommittedRead,
                             NdbScanOperation::SF_OrderBy);

    ndbout << "Restart node " << nodeId << endl; 
    res.restartOneDbNode(nodeId,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);
    res.startNodes(&nodeId, 1);
    res.waitNodesStarted(&nodeId, 1);

    int r1 = pTrans->execute(NdbTransaction::NoCommit);

    int r2;
    while ((r2 = pOp->nextResult()) == 0);

    ndbout_c("r0: %d r1: %d r2: %d", r0, r1, r2);

    pTrans->close();
  }
  
  return NDBT_OK;
}

int
initBug42559(NDBT_Context* ctx, NDBT_Step* step){
  
  int dump[] = { 7017  }; // Max LCP speed
  NdbRestarter res;
  res.dumpStateAllNodes(dump, 1);

  return NDBT_OK;
}
int
finalizeBug42559(NDBT_Context* ctx, NDBT_Step* step){
  
  int dump[] = { 7017, 1  }; // Restore config value
  NdbRestarter res;
  res.dumpStateAllNodes(dump, 2);

  return NDBT_OK;
}

int takeResourceSnapshot(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  
  int checksnapshot = DumpStateOrd::TcResourceSnapshot;
  restarter.dumpStateAllNodes(&checksnapshot, 1);

  /* TODO : Check other block's resources? */
  return NDBT_OK;
}

int runScanReadIndexWithBounds(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int numRanges = ctx->getProperty("NumRanges", 1);
  int maxRunSecs = ctx->getProperty("MaxRunSecs", 60);
  int maxRetries = ctx->getProperty("MaxRetries", 1000000);
  
  const NdbDictionary::Index * pIdx = 
    GETNDB(step)->getDictionary()->getIndex(orderedPkIdxName, 
					    ctx->getTab()->getName());

  int i = 0;
  HugoCalculator calc(*ctx->getTab());
  NDBT_ResultRow row (*ctx->getTab());
  Ndb* ndb = GETNDB(step);

  Uint64 start = NdbTick_CurrentMillisecond();
  Uint64 end = start + (1000*maxRunSecs);
  int retries = 0;

  /* Here we run an ordered index scan, with a bound.
   * There are numRanges sub-scans with the same bound
   * This is done to use up some KeyInfo, and expose bugs in that area
   * If we run many of these in parallel we may exhaust the available KeyInfo storage,
   * which may expose some bugs.
   */
  while (pIdx &&
         i<loops && 
         !ctx->isTestStopped() &&
         NdbTick_CurrentMillisecond() < end) {
    g_info << "Step " << step->getStepNo() 
           << "Loop : " << i << ": ";
    
    /* Use specific-partition variant of startTransaction to ensure a single
     * TC node is used
     */
    NdbTransaction* trans = ndb->startTransaction(ctx->getTab(), Uint32(0));
    if (trans == NULL)
    {
      g_err << "Transaction start failed " << ndb->getNdbError() << endl;
      return NDBT_FAILED;
    }

    NdbIndexScanOperation* iso = trans->getNdbIndexScanOperation(pIdx->getName(), 
                                                                 ctx->getTab()->getName());
    if (iso == NULL)
    {
      g_err << "Error obtaining IndexScanOperation : " << trans->getNdbError() << endl;
      trans->close();
      return NDBT_FAILED;
    }

    if (iso->readTuples(NdbOperation::LM_CommittedRead, 
                        (NdbScanOperation::SF_OrderBy |
                         NdbScanOperation::SF_ReadRangeNo |
                         NdbScanOperation::SF_MultiRange), 
                        0) != 0)
    {
      g_err << "Error calling readTuples : " << iso->getNdbError() << endl;
      trans->close();
      return NDBT_FAILED;
    }

    for (int range = 0; range < numRanges; range++)
    {
      /* Now define a bound... */
      for (Uint32 k=0; k<pIdx->getNoOfColumns(); k++)
      {
        const NdbDictionary::Column* idxCol = pIdx->getColumn(k);
        const char* colName = idxCol->getName();
        /* Lower bound of <= NULL should return all rows */
        if (iso->setBound(colName, NdbIndexScanOperation::BoundLE, NULL) != 0)
        {
          g_err << "Error setting bound for column %s. " 
                << iso->getNdbError() << endl;
          trans->close();
          return NDBT_FAILED;
        }
      }
      
      if (iso->end_of_bound(range) != 0)
      {
        g_err << "Error closing range " << range << endl;
        g_err << iso->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
    
    const NdbDictionary::Table& tab= *ctx->getTab();

    /* Now request all columns in result projection */
    for (int a=0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = 
	  iso->getValue(tab.getColumn(a)->getName())) == 0) {
	g_err << "Error defining read value " << trans->getNdbError() << endl;
        trans->close();
	return NDBT_FAILED;
      }
    }
          
    /* Ready to go... */
    trans->execute(NoCommit, AbortOnError);

    if (trans->getNdbError().code != 0)
    {
      if (trans->getNdbError().code == 218)
      {
        /* Out of KeyInfo buffers in TC - that's ok, let's try again */
        trans->close();
        if (retries++ < maxRetries)
        {
          g_err << "Step " << step->getStepNo()
                << " TC out of Keyinfo buffers (218) - retrying" << endl;
          continue;
        }
      }

      g_err << "Error on execution : " << trans->getNdbError() << endl;
      trans->close();
      return NDBT_FAILED;
    }
    
    int eof;
    int rows = 0;
    while ((eof = iso->nextResult(true)) == 0)
    {
      rows++;
      if (calc.verifyRowValues(&row) != 0)
      {
        g_err << "Verification failed." << endl;
        trans->close();
        return NDBT_FAILED;
      }

#ifdef BUG_14388257_FIXED
      int rangeNum = (rows -1) / records;
      if (iso->get_range_no() != rangeNum)
      {
        g_err << "Expected row " << rows 
              << " to be in range " << rangeNum
              << " but it reports range num " << iso->get_range_no()
              << " : " << row
              << endl;
        return NDBT_FAILED;
      }
#endif

      //g_err << row << endl;
    }

    if (eof != 1)
    {
      g_err << "nextResult() returned " << eof << endl;
      g_err << "Scan error : " << iso->getNdbError() << endl;

      if (iso->getNdbError().status == NdbError::TemporaryError)
      {
        if (retries++ < maxRetries)
        {
          g_err << "Step "
                << step->getStepNo()
                << "  Temporary, retrying on iteration " 
                << i << " rows so far : " << rows << endl;
          trans->close();
          NdbSleep_MilliSleep(2500);
          continue;
        }
      }

      trans->close();
      return NDBT_FAILED;
    }
    
    g_err << "Read " << rows << " rows." << endl;
    
    if (records != 0 && rows != (numRanges * records))
    {
      g_err << "Expected " << records << " rows"
            << ", read " << rows << endl;
#ifdef BUG_14388257_FIXED
      trans->close();
      assert(false);
      return NDBT_FAILED;
#endif
    }

    trans->close();
    i++;
  }
  return NDBT_OK;
}

int checkResourceSnapshot(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  
  int checksnapshot = DumpStateOrd::TcResourceCheckLeak;
  restarter.dumpStateAllNodes(&checksnapshot, 1);

  /* TODO : Check other block's resources? */
  return NDBT_OK;
}


int
runBug54945(NDBT_Context* ctx, NDBT_Step* step)
{

  int loops = ctx->getNumLoops();
  const NdbDictionary::Table*  pTab = ctx->getTab();

  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  while (loops--)
  {
    int node = res.getNode(NdbRestarter::NS_RANDOM);
    int err = 0;
    printf("node: %u ", node);
    switch(loops % 2){
    case 0:
      if (res.getNumDbNodes() >= 2)
      {
        err = 8088;
        int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
        res.dumpStateOneNode(node, val, 2);
        res.insertErrorInNode(node, 8088);
        ndbout_c("error 8088");
        break;
      }
      // fall through
    case 1:
      err = 5057;
      res.insertErrorInNode(node, 5057);
      ndbout_c("error 5057");
      break;
    }

    for (int i = 0; i< 25; i++)
    {
      NdbTransaction* pCon = pNdb->startTransaction();
      NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
      if (pOp == NULL) {
        ERR(pCon->getNdbError());
        return NDBT_FAILED;
      }
      
      if( pOp->readTuples(NdbOperation::LM_Read) != 0) 
      {
        ERR(pCon->getNdbError());
        return NDBT_FAILED;
      }
      
      if( pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
      {
        ERR(pCon->getNdbError());
        return NDBT_FAILED;
      }
      
      pCon->execute(NoCommit);
      pCon->close();
    } 
    if (err == 8088)
    {
      res.waitNodesNoStart(&node, 1);
      res.startAll();
      res.waitClusterStarted();
    }
  }

  return NDBT_OK;
}

int
runCloseRefresh(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb * pNdb = GETNDB(step);

  const Uint32 codeWords= 1;
  Uint32 codeSpace[ codeWords ];
  NdbInterpretedCode code(NULL, // Table is irrelevant
                          &codeSpace[0],
                          codeWords);
  if ((code.interpret_exit_last_row() != 0) ||
      (code.finalise() != 0))
  {
    ERR(code.getNdbError());
    return NDBT_FAILED;
  }

  const NdbDictionary::Table*  pTab = ctx->getTab();
  NdbTransaction* pTrans = pNdb->startTransaction();
  NdbScanOperation* pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->readTuples(NdbOperation::LM_CommittedRead) != 0)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->setInterpretedCode(&code) == -1 )
  {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  pTrans->execute(NdbTransaction::NoCommit);
  pOp->close(); // close this

  pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->readTuples(NdbOperation::LM_CommittedRead) != 0)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->setInterpretedCode(&code) == -1 )
  {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  pTrans->execute(NdbTransaction::NoCommit);
  pTrans->refresh();
  pTrans->close();
  return NDBT_OK;
}

#define CHK_RET_FAILED(x) if (!(x)) { ndbout_c("Failed on line: %u", __LINE__); return NDBT_FAILED; }

int
runMixedDML(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  unsigned seed = (unsigned)NdbTick_CurrentMillisecond();

  const int rows = ctx->getNumRecords();
  const int loops = 10 * ctx->getNumLoops();
  const int until_stopped = ctx->getProperty("UntilStopped");
  const int batch = ctx->getProperty("Batch", Uint32(50));

  const NdbRecord * pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 * pRow = new Uint8[len];

  int count_ok = 0;
  int count_failed = 0;
  for (int i = 0; i < loops || (until_stopped && !ctx->isTestStopped()); i++)
  {
    NdbTransaction* pTrans = pNdb->startTransaction();
    CHK_RET_FAILED(pTrans != 0);

    int lastrow = 0;
    int result = 0;
    for (int rowNo = 0; rowNo < batch; rowNo++)
    {
      int left = rows - lastrow;
      int rowId = lastrow;
      if (left)
      {
        rowId += ndb_rand_r(&seed) % (left / 10 + 1);
      }
      else
      {
        break;
      }
      lastrow = rowId;

      bzero(pRow, len);

      HugoCalculator calc(* pTab);
      calc.setValues(pRow, pRowRecord, rowId, rand());

      NdbOperation::OperationOptions opts;
      bzero(&opts, sizeof(opts));

      const NdbOperation* pOp = 0;
      switch(ndb_rand_r(&seed) % 3){
      case 0:
        pOp = pTrans->writeTuple(pRowRecord, (char*)pRow,
                                 pRowRecord, (char*)pRow,
                                 0,
                                 &opts,
                                 sizeof(opts));
        break;
      case 1:
        pOp = pTrans->deleteTuple(pRowRecord, (char*)pRow,
                                  pRowRecord, (char*)pRow,
                                  0,
                                  &opts,
                                  sizeof(opts));
        break;
      case 2:
        pOp = pTrans->updateTuple(pRowRecord, (char*)pRow,
                                  pRowRecord, (char*)pRow,
                                  0,
                                  &opts,
                                  sizeof(opts));
        break;
      }
      CHK_RET_FAILED(pOp != 0);
      result = pTrans->execute(NoCommit, AO_IgnoreError);
      if (result != 0)
      {
        goto found_error;
      }
    }

    result = pTrans->execute(Commit, AO_IgnoreError);
    if (result != 0)
    {
  found_error:
      count_failed++;
      NdbError err = pTrans->getNdbError();
      ndbout << err << endl;
      CHK_RET_FAILED(err.status == NdbError::TemporaryError ||
                     err.classification == NdbError::NoDataFound ||
                     err.classification == NdbError::ConstraintViolation);
    }
    else
    {
      count_ok++;
    }
    pTrans->close();
  }

  ndbout_c("count_ok: %d count_failed: %d",
           count_ok, count_failed);
  delete [] pRow;

  return NDBT_OK;
}

int
runBug13394788(NDBT_Context* ctx, NDBT_Step* step)
{
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  const NdbDictionary::Index * pIdx =
    GETNDB(step)->getDictionary()->getIndex(orderedPkIdxName,
					    ctx->getTab()->getName());
  HugoTransactions hugoTrans(*ctx->getTab(), pIdx);

  NdbRestarter res;
  for (int i = 0; i < loops; i++)
  {
    res.insertErrorInAllNodes(5074);
    // this will actually be a mrr scan...
    int batch = 1 + (rand() % records);
    // this should be error...
    hugoTrans.pkReadRecords(GETNDB(step), records, batch);

    // make it should work again...
    res.insertErrorInAllNodes(0);
    if (hugoTrans.pkReadRecords(GETNDB(step), records, batch) != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

/**
 * This is a regression test for bug #11748194 "TRANSACTION OBJECT CREATED 
 * AND UNRELEASED BY EXTRA CALL TO NEXTRESULT()".
 * If a transaction made an extra call to nextResult() after getting
 * end-of-scan from nextResult(), the API would leak transaction objects. 
 */
static int
runExtraNextResult(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * pTab = ctx->getTab();
  // Fill table with 10 rows.
  HugoTransactions hugoTrans(*pTab);
  Ndb* const ndb = GETNDB(step);
  hugoTrans.loadTable(ndb, 10);
  // Read MaxNoOfConcurrentTransactions configuration value.
  Uint32 maxTrans = 0;
  NdbConfig conf;
  require(conf.getProperty(conf.getMasterNodeId(),
                           NODE_TYPE_DB,
                           CFG_DB_NO_TRANSACTIONS,
                           &maxTrans));
  require(maxTrans > 0);
  
  /**
   * The bug causes each scan to leak one object.
   */
  int result = NDBT_OK;
  Uint32 i = 0;
  while (i < maxTrans+1)
  {
    NdbTransaction* const trans = ndb->startTransaction();
    if (trans == NULL)
    {
      g_err << "ndb->startTransaction() gave unexpected error : " 
            << ndb->getNdbError() << " in the " << i << "th iteration." <<endl;
    }
    
    // Do a random numer of scans in this transaction.
    const int scanCount = rand()%4;
    for (int j=0; j < scanCount; j++)
    {
      NdbScanOperation* const scan = trans->getNdbScanOperation(pTab);
      if (scan == NULL)
      {
        g_err << "trans->getNdbScanOperation() gave unexpected error : " 
              << trans->getNdbError() << " in the " << i
              << "th iteration." <<endl;
        return NDBT_FAILED;
      }
      
      require(scan->readTuples(NdbOperation::LM_CommittedRead) == 0);
      require(scan->getValue(0u) != 0);
      require(trans->execute(NoCommit) == 0);
      
      // Scan table until end.
      int scanResult;
      do
      {
        // Fetch new batch.
        scanResult = scan->nextResult(true);
        while (scanResult == 0)
        {
          // Iterate over batch.
          scanResult = scan->nextResult(false);
        }
      } while (scanResult == 0 || scanResult == 2);

      /** 
       * Do extra nextResult. This is the application error that triggers the 
       * bug.
       */
      scanResult = scan->nextResult(true);
      require(scanResult < 0);
      // Here we got the undefined error code -1. So check for that too.
      // if (trans->getNdbError().code != 4120
      if (scan->getNdbError().code != 4120
          && result == NDBT_OK)
      {
        g_err << "scan->nextResult() gave unexpected error : " 
              << scan->getNdbError() << " in the " << i << "th iteration." 
              << endl;
        result = NDBT_FAILED;
      }
      i++;
    }
    ndb->closeTransaction(trans);
  } // while (i < maxTrans+1)

  // Delete table rows.
  require(UtilTransactions(*ctx->getTab()).clearTable(ndb) == 0);
  return result;
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
  TC_PROPERTY("TupScan", (Uint32)0);
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
  STEPS(runRandScanRead, 70);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead488T", 
	 "Verify scan requirement: It's only possible to have 11 concurrent "\
	 "scans per fragment running in Ndb kernel at the same time. "\
	 "When this limit is exceeded the scan will be aborted with errorcode "\
	 "488."){
  TC_PROPERTY("TupScan", 1);
  INITIALIZER(runLoadTable);
  STEPS(runRandScanRead, 70);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead488O", 
	 "Verify scan requirement: It's only possible to have 11 concurrent "\
	 "scans per fragment running in Ndb kernel at the same time. "\
	 "When this limit is exceeded the scan will be aborted with errorcode "\
	 "488."){
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runScanReadIndex, 70);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("ScanRead488_Mixed", 
	 "Verify scan requirement: It's only possible to have 11 concurrent "\
	 "scans per fragment running in Ndb kernel at the same time. "\
	 "When this limit is exceeded the scan will be aborted with errorcode "\
	 "488."){
  TC_PROPERTY("TupScan", 2);
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runRandScanRead, 50);
  STEPS(runScanReadIndex, 50);
  FINALIZER(createOrderedPkIndex_Drop);
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
TESTCASE("TupScanRead100",
	 "Verify scan requirement: Scan with 100 simultaneous threads"){
  TC_PROPERTY("TupScan", 1);
  INITIALIZER(runLoadTable);
  STEPS(runScanRead, 100);
  FINALIZER(runClearTable);
}
TESTCASE("Scan-bug8262", 
	 ""){
  TC_PROPERTY("Rows", 1);
  TC_PROPERTY("ErrorCode", 8035);
  INITIALIZER(runLoadTable);
  INITIALIZER(runInsertError); // Will reset error code
  STEPS(runScanRead, 25);
  FINALIZER(runInsertError);
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
  STEPS(runScanReadRandomTableExceptTestTable, 40);
}
TESTCASE("ScanRead100RandomNoTableCreate", 
	 "Verify scan requirement: Scan with 100 simultaneous threads. "\
	 "Use random table for the scan. Dont create or load the tables."){
  STEPS(runScanReadRandomTableExceptTestTable, 100);
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
TESTCASE("ScanReadError8081",
	 "Scan and insert error 8081"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 8081);
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
TESTCASE("ScanReadError8095", 
	 "Scan and insert error 8095. "\
         "TC fails to send a DIH_SCAN_GET_NODES_REQ due to "\
         "'out of LongMessageBuffers' -> terminate scan."){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 8095);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError7234", 
	 "Scan and insert error 7234. "\
         "DIH fails to send a DIH_SCAN_GET_NODES_CONF due to "\
         "'out of LongMessageBuffers' -> send DIH_SCAN_GET_NODES_REF."){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 7234);
  STEP(runScanReadError);
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
TESTCASE("Bug48700", 
	 "Load and delete all while scan updating and scan reading\n"\
	 "Alexander Lukas special"){
  TC_PROPERTY("AbortProb", Uint32(0));
  TC_PROPERTY("NoCount", 1);
  TC_PROPERTY("LockMode", NdbOperation::LM_CommittedRead);
  INITIALIZER(runClearTable);
  STEPS(runRandScanRead, 10);
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
TESTCASE("ScanParallelism", 
	 "Test scan with different parallelism"){
  INITIALIZER(runLoadTable);
  STEP(runScanParallelism);
  FINALIZER(runClearTable);
}
TESTCASE("ScanVariants", 
	 "Test different scan variants"){
  INITIALIZER(runLoadTable);
  STEP(runScanVariants);
  FINALIZER(runClearTable);
}
TESTCASE("Bug24447",
	 ""){
  INITIALIZER(runLoadTable);
  STEP(runBug24447);
  FINALIZER(runClearTable);
}
TESTCASE("Bug36124",
         "Old interpreted Api usage"){
  INITIALIZER(runLoadTable);
  STEP(runBug36124);
  FINALIZER(runClearTable);
}
TESTCASE("Bug42545", "")
{
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug42545);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug42559", "") 
{
  INITIALIZER(initBug42559);
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runScanReadIndex, 70);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(finalizeBug42559);
  FINALIZER(runClearTable);
}
TESTCASE("CloseRefresh", "")
{
  INITIALIZER(runCloseRefresh);
}
TESTCASE("Bug54945", "")
{
  INITIALIZER(runBug54945);
}
TESTCASE("ScanFragRecExhaust", 
         "Test behaviour when TC scan frag recs exhausted")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(runScanReadExhaust);
  FINALIZER(runClearTable);
}
TESTCASE("Bug12324191", "")
{
  TC_PROPERTY("LockMode", Uint32(NdbOperation::LM_Read));
  TC_PROPERTY("TupScan", Uint32(1));
  TC_PROPERTY("Rows", Uint32(0));
  INITIALIZER(runLoadTable);
  STEP(runScanRead);
  STEPS(runMixedDML,10);
}
TESTCASE("Bug13394788", "")
{
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug13394788);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("extraNextResultBug11748194",
         "Regression test for bug #11748194")
{
  INITIALIZER(runExtraNextResult);
}
TESTCASE("ScanRealKeyInfoExhaust",
         "Test behaviour when TC keyinfo buffers exhausted 4real")
{
  /* 55 threads, each setting 200 ranges in their keyinfo
   * For the lightest single column PK case, each range should
   * use 2 words, 200 ranges = 400 words per scan thread = 
   * 400/4 = 100 Databuffers used.
   * 55 threads = 55*100 = 5500 Databuffers which is >
   * the 4000 statically allocated in 6.3
   */
  TC_PROPERTY("NumRanges", 200);
  TC_PROPERTY("MaxRunSecs", 120);
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  INITIALIZER(takeResourceSnapshot);
  STEPS(runScanReadIndexWithBounds,55);
  FINALIZER(checkResourceSnapshot);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("ScanKeyInfoExhaust",
         "Test behaviour when TC keyinfo buffers exhausted with error insert")
{
  /* Use error insert 8094 to cause keyinfo exhaustion, then run a single scan
   * with keyinfo to hit the error path
   */
  TC_PROPERTY("MaxRunSecs", 10);
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable);
  INITIALIZER(takeResourceSnapshot);
  TC_PROPERTY("ErrorCode", 8094);
  INITIALIZER(runInsertError);
  STEP(runScanReadIndexWithBounds);
  FINALIZER(checkResourceSnapshot);
  FINALIZER(runInsertError);
  FINALIZER(createOrderedPkIndex_Drop);
  FINALIZER(runClearTable);
}
  


NDBT_TESTSUITE_END(testScan);

int main(int argc, const char** argv){
  ndb_init();
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  NDBT_TESTSUITE_INSTANCE(testScan);
  return testScan.execute(argc, argv);
}

template class Vector<Attrib*>;
template class Vector<NdbTransaction*>;
