/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
  g_err << "loadTable with latest GCI = " << hugoTrans.get_high_latest_gci()
        << endl;
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
  Uint32 max_gci = 0;
  for (int i=0; i < NDBT_Tables::getNumTables(); i++){

    const NdbDictionary::Table* tab = getTable(GETNDB(step), i);
    if (tab == NULL){ 
      return NDBT_FAILED;
    }
    
    HugoTransactions hugoTrans(*tab);
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    max_gci = hugoTrans.get_high_latest_gci();
  }
  g_err << "loadAllTables with latest GCI = " << max_gci << endl;
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
    NDB_ERR(err);
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
    NDB_ERR(pNdb->getDictionary()->getNdbError());
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
  g_err << "ClearTable with latest GCI = " << utilTrans.get_high_latest_gci()
        << endl;
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
  g_err << "Latest GCI = " << hugoTrans.get_high_latest_gci() << endl;
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
  g_err << "Latest GCI = " << hugoTrans.get_high_latest_gci() << endl;
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

#include "../../src/ndbapi/ndb_internal.hpp"

int runScanReadExhaust(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = 240; // Max parallelism
  int error = 8093;
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  
  /* First take a TC resource snapshot */
  int savesnapshot= DumpStateOrd::TcResourceSnapshot;
  Uint32 checksnapshot= DumpStateOrd::TcResourceCheckLeak;
  
  restarter.dumpStateAllNodes(&savesnapshot, 1);
  Ndb_internal::set_TC_COMMIT_ACK_immediate(pNdb, true);

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
  pDict->forceGCPWait(1);
  if (Ndb_internal::send_dump_state_all(pNdb, &checksnapshot, 1) != 0)
  {
    return NDBT_FAILED;
  }
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
      {
        // Remember that one scan read failed, but continue to
        // read to put load on the system
	result = NDBT_FAILED;
      }
    }
	

    if(restarter.waitClusterStarted(120) != 0){
      g_err << "Cluster failed to restart" << endl;
      result = NDBT_FAILED;
    }
    CHK_NDB_READY(GETNDB(step));
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
  for(unsigned i = 0; i < alist.attriblist.size(); i++){
    g_info << i << endl;
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
          if (pCon == NULL)
          {
            NDB_ERR(pNdb->getNdbError());
            return NDBT_FAILED;
          }

	  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
	  if (pOp == NULL) {
	    NDB_ERR(pCon->getNdbError());
	    return NDBT_FAILED;
	  }
	  
	  if( pOp->readTuples((NdbOperation::LockMode)lm,
			      tups ? NdbScanOperation::SF_TupScan : 0,
			      par,
			      batch) != 0) 
	  {
	    NDB_ERR(pCon->getNdbError());
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
	      NDB_ERR(pCon->getNdbError());
	      return NDBT_FAILED;
	    }
	  } 
	  
	  if (! (disk && !found_disk))
	  {
	    int check = pCon->execute(NoCommit);
	    if( check == -1 ) {
	      NDB_ERR(pCon->getNdbError());
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
  if (pCon == NULL)
  {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
  if (pOp == NULL) {
    NDB_ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }
  
  if( pOp->readTuples(NdbOperation::LM_Read) != 0) 
  {
    NDB_ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }

  if( pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    NDB_ERR(pCon->getNdbError());
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
    if (pTrans == NULL)
    {
      NDB_ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }

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
      
      for (unsigned t = 0; t < translist.size(); t++)
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
  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;
  
  int checksnapshot = DumpStateOrd::TcResourceSnapshot;
  restarter.dumpStateAllNodes(&checksnapshot, 1);
  Ndb_internal::set_TC_COMMIT_ACK_immediate(pNdb, true);

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
      require(false);
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
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  
  Uint32 checksnapshot = DumpStateOrd::TcResourceCheckLeak;
  pDict->forceGCPWait(1);
  if (Ndb_internal::send_dump_state_all(pNdb, &checksnapshot, 1) != 0)
  {
    return NDBT_FAILED;
  }
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
      if (pCon == NULL)
      {
        NDB_ERR(pNdb->getNdbError());
        return NDBT_FAILED;
      }

      NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
      if (pOp == NULL)
      {
        NDB_ERR(pCon->getNdbError());
        return NDBT_FAILED;
      }
      
      if (pOp->readTuples(NdbOperation::LM_Read) != 0) 
      {
        NDB_ERR(pCon->getNdbError());
        return NDBT_FAILED;
      }
      
      if (pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
      {
        NDB_ERR(pCon->getNdbError());
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
      if (pNdb->waitUntilReady() != 0)
        return NDBT_FAILED;
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
    NDB_ERR(code.getNdbError());
    return NDBT_FAILED;
  }

  const NdbDictionary::Table*  pTab = ctx->getTab();
  NdbTransaction* pTrans = pNdb->startTransaction();
  if (pTrans == NULL)
  {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->readTuples(NdbOperation::LM_CommittedRead) != 0)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->setInterpretedCode(&code) == -1 )
  {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  pTrans->execute(NdbTransaction::NoCommit);
  pOp->close(); // close this

  pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->readTuples(NdbOperation::LM_CommittedRead) != 0)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->setInterpretedCode(&code) == -1 )
  {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::ROW_COUNT) == 0)
  {
    NDB_ERR(pTrans->getNdbError());
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
 * Tests reated to TupKeyRef (cf. Bug#16176006: TUPLE WITH CHECKSUM ERROR
 * SILENTLY DISCARDED).
 */
namespace TupErr
{
  static const char* const tabName = "tupErrTab";
  static const int totalRowCount = 2000;
  
  struct Row
  {
    int pk1;
    int pk2;
    int a1;
  };

  static int
  createDataBase(NDBT_Context* ctx, NDBT_Step* step)
  {
    // Create table.
    NDBT_Attribute pk1("pk1", NdbDictionary::Column::Int, 1, true);
    NDBT_Attribute pk2("pk2", NdbDictionary::Column::Int, 1, true);
    NDBT_Attribute a1("a1", NdbDictionary::Column::Int, 1);
  
    NdbDictionary::Column* columns[] = {&pk1, &pk2, &a1};
  
    const NDBT_Table tabDef(tabName, sizeof columns/sizeof columns[0], 
                            columns);
    Ndb* const ndb = step->getNdb();
  
    NdbDictionary::Dictionary* const dictionary = ndb->getDictionary();
  
    dictionary->dropTable(tabName);
    require(dictionary->createTable(tabDef) == 0);

    // Populate table.
    const NdbDictionary::Table* const tab = dictionary->getTable(tabName);
    const NdbRecord* const record = tab->getDefaultRecord();

    NdbTransaction* const trans = ndb->startTransaction();
    if (trans == NULL)
    {
      NDB_ERR(ndb->getNdbError());
      return NDBT_FAILED;
    }

    for (int i = 0; i<totalRowCount; i++)
    {
      const Row row = {i, 0, i};

      const NdbOperation* const operation=
        trans->insertTuple(record, reinterpret_cast<const char*>(&row));
      require(operation != NULL);
    }
    require(trans->execute( NdbTransaction::Commit) != -1);
    ndb->closeTransaction(trans);

    return NDBT_OK;
  }

  static int
  doCheckSumQuery(NDBT_Context* ctx, NDBT_Step* step)
  {
    // Insert error.
    const int errInsertNo = 4036;
    NdbRestarter restarter;
    const int nodeId = restarter.getDbNodeId(0);

    /**
     * Let the first tuple from one fragment cause error 896 
     * (tuple checksum error).
     */
    g_info << "Inserting error " << errInsertNo << " in node " << nodeId 
           << endl;
    require(restarter.insertErrorInNode(nodeId, errInsertNo) == 0);
  
    // Build query.
    Ndb* const ndb = step->getNdb();
  
    NdbDictionary::Dictionary* const dictionary = ndb->getDictionary();
    const NdbDictionary::Table* const tab = dictionary->getTable(tabName);
    const NdbRecord* const record = tab->getDefaultRecord();

  
    NdbTransaction* const trans = ndb->startTransaction();
    if (trans == NULL)
    {
      NDB_ERR(ndb->getNdbError());
      return NDBT_FAILED;
    }

    NdbScanOperation* const scanOp = trans->scanTable(record);
    require(scanOp != NULL);
    require(trans->execute(Commit) == 0);
  
    int queryRes = 0;
  
    // Loop through the result set.
    int rowCount = -1;
    while(queryRes == 0)
    {
      const Row* resRow;
      queryRes = scanOp->nextResult(reinterpret_cast<const char**>(&resRow), 
                                    true, false);
      rowCount++;
    }

    int res = NDBT_OK;
    switch (queryRes)
    {
    case 1: // Scan complete
      g_err << "Did not get expected error 896. Query returned " << rowCount 
            << " rows out of " << totalRowCount << endl;
      res = NDBT_FAILED;
      break;
    
    case -1: // Error
      {
        const int errCode = trans->getNdbError().code;
        if (errCode == 896)
        {
          g_info << "Got expected error 896. Query returned " << rowCount
                 << " rows." << endl;
        }
        else
        {
          g_err << "Got unexpected error " << errCode << ". Query returned " 
                << rowCount << " rows." << endl;
          res = NDBT_FAILED;
        }
      }
      break;
    
    default:
      require(false);
    }
    ndb->closeTransaction(trans);
    dictionary->dropTable(tabName);

    return res;
  }

  static int
  doInterpretNok6000Query(NDBT_Context* ctx, NDBT_Step* step)
  {
    // Build query.
    Ndb* const ndb = step->getNdb();
  
    NdbDictionary::Dictionary* const dictionary = ndb->getDictionary();
    const NdbDictionary::Table* const tab = dictionary->getTable(tabName);
    const NdbRecord* const record = tab->getDefaultRecord();

    NdbTransaction* const trans = ndb->startTransaction();
    if (trans == NULL)
    {
      NDB_ERR(ndb->getNdbError());
      return NDBT_FAILED;
    }

    NdbInterpretedCode code(tab);

    /**
     * Build an interpreter code sequence that causes rows with pk1==50 to 
     * abort the scan, and that skips all other rows.
     */ 
    const NdbDictionary::Column* const col = tab->getColumn("pk1");
    require(col != NULL);
    require(code.read_attr(1, col) == 0);
    require(code.load_const_u32(2, 50) == 0);
    require(code.branch_eq(1, 2, 0) == 0);

    // Exit here if pk1!=50. Skip this row.
    require(code.interpret_exit_nok(626) == 0);

    // Go here if pk1==50. Abort scan.
    require(code.def_label(0) == 0);
    require(code.interpret_exit_nok(6000) == 0);
    require(code.finalise() == 0);

    NdbScanOperation::ScanOptions opts;
    opts.optionsPresent = NdbScanOperation::ScanOptions::SO_INTERPRETED;
    opts.interpretedCode = &code;

    //NdbScanOperation* const scanOp = trans->scanTable(record);
    NdbScanOperation* const scanOp = trans->scanTable(record, 
                                                      NdbOperation::LM_Read,
                                                      NULL,
                                                      &opts,
                                                      sizeof(opts));
    require(scanOp != NULL);
    require(trans->execute(Commit) == 0);
  
    int queryRes = 0;
  
    // Loop through the result set.
    int rowCount = -1;
    while(queryRes == 0)
    {
      const Row* resRow;
      queryRes = 
        scanOp->nextResult(reinterpret_cast<const char**>(&resRow), true, false);
      rowCount++;
    }

    int res = NDBT_OK;
    switch (queryRes)
    {
    case 1: // Scan complete
      g_err << "Query did not fail as it should have. Query returned " 
            << rowCount << " rows out of " << totalRowCount << endl;
      res = NDBT_FAILED;
      break;
    
    case -1: // Error
      {
        const int errCode = trans->getNdbError().code;
        if (errCode == 6000)
        {
          if (rowCount==0)
          {
            g_info << "Got expected error 6000. Query returned 0 rows out of " 
                   << totalRowCount  << endl;
          }
          else
          {
            g_err << "Got expected error 6000. Query returned " 
                  << rowCount << " rows out of " << totalRowCount 
                  << ". Exepected 0 rows."<< endl;
            res = NDBT_FAILED;
          }
        }
        else
        {
          g_err << "Got unexpected error " << errCode << ". Query returned " 
                << rowCount << " rows out of " << totalRowCount  << endl;
          res = NDBT_FAILED;
        }
      }
      break;
    
    default:
      require(false);
    }

    ndb->closeTransaction(trans);
    dictionary->dropTable(tabName);

    return res;
  }
} // namespace TupErr

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

/**
 * populateFragment0 - load a table with rows until fragment 0 contains a
 * given number of rows.
 */
static int
populateFragment0(Ndb* ndb, const NdbDictionary::Table* tab, Uint32 rows, Uint32 dbaccBuckets = 0)
{
  NdbRestarter restarter;
  require(restarter.insertError2InAllNodes(3004, dbaccBuckets) == 0);

  HugoTransactions hugoTrans(*tab);

  const NdbRecord* const record = tab->getDefaultRecord();
  require(record != NULL);

  NdbScanOperation::ScanOptions scanOptions;
  scanOptions.optionsPresent = scanOptions.SO_PARALLEL |
                               scanOptions.SO_BATCH |
                               scanOptions.SO_GETVALUE;
  scanOptions.parallel = 1;
  scanOptions.batch = 1;

  NdbOperation::GetValueSpec extraCols[2];

  Uint32 fragment;
  extraCols[0].column = NdbDictionary::Column::FRAGMENT;
  extraCols[0].appStorage = &fragment;
  extraCols[0].recAttr = NULL;

  Uint64 row_count = 0;
  extraCols[1].column = NdbDictionary::Column::ROW_COUNT;
  extraCols[1].appStorage = &row_count;
  extraCols[1].recAttr = NULL;

  scanOptions.extraGetValues = &extraCols[0];
  scanOptions.numExtraGetValues = 2;

  int start_row = 0;
  while (row_count < rows)
  {
    const int missing_rows = rows - row_count;
    hugoTrans.loadTableStartFrom(ndb, start_row, missing_rows);
    start_row += missing_rows;

    NdbTransaction* trans;
    NdbScanOperation* scanOp;
    trans = ndb->startTransaction();
    require(trans != NULL);
    scanOp = trans->scanTable(record,
                              NdbOperation::LM_Read,
                              NULL,
                              &scanOptions,
                              sizeof(scanOptions));
    require(scanOp != NULL);
    require(trans->execute(Commit) == 0);
    const char* resRow;
    int queryRes = scanOp->nextResult(&resRow,
                                      true,
                                      false);
    require(queryRes == 0);
    require(fragment == 0);

    scanOp->close();
    trans->close();
  }
  return 0;
}

/**
 * sizeFragment0DbaccHashTable - triggers Dbacc to change the hash table size
 * of fragment 0 to have given number of buckets.  The error insert (3004)
 * used can have effect on any tables fragment 0.  The resizing is triggered
 * on table given by the NdbRecord argument by deleting and re-insert a given
 * row.  That row must exist and be unlocked.
 */
static int
sizeFragment0DbaccHashTable(Ndb* ndb,
                            const NdbRecord* record,
                            const char* row,
                            Uint32 bucketCount)
{
  NdbRestarter restarter;

  // Set wanted bucket count for fragment 0
  require(restarter.insertError2InAllNodes(3004, bucketCount) == 0);

  NdbTransaction* trans = ndb->startTransaction();
  require(NULL != trans->deleteTuple(record, row, record));
  require(NULL != trans->insertTuple(record, row, record, row));
  require(0 == trans->execute(Commit));
  trans->close();
  sleep(1);

  return 0;
}

/**
 * runScanDuringShrinkAndExpandBack - test case demonstrating
 * Bug#22926938 ACC TABLE SCAN MAY SCAN SAME ROW TWICE
 *
 * 1. Start with table with just below 2^n buckets in fragment 0.
 * 2. Start scan and read a few rows.
 * 3. Shrink table, due to 1) top buckets will be merged to just below middle
 *    buckets, and shrink will not be hindered by the scan near bottom of
 *    table.
 * 4. Scan beyond middle buckets.
 * 5. Expand table back to original size.  The top buckets will now contain
 *    scanned elements.  But before bug fix top buckets was marked as
 *    unscanned.
 * 6. Complete scan on fragment 0.  Before bug fix some rows was scanned twice.
 */
static int
runScanDuringShrinkAndExpandBack(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* const ndb = GETNDB(step);
  const NdbRecord* const record = pTab->getDefaultRecord();
  require(record != NULL);
  NdbScanOperation::ScanOptions scanOptions;
  scanOptions.optionsPresent = scanOptions.SO_PARALLEL |
                               scanOptions.SO_BATCH |
                               scanOptions.SO_GETVALUE;
  scanOptions.parallel = 1;
  scanOptions.batch = 1;

  Uint32 fragment;
  NdbOperation::GetValueSpec extraCols[1];
  extraCols[0].column = NdbDictionary::Column::FRAGMENT;
  extraCols[0].appStorage = &fragment;
  extraCols[0].recAttr = NULL;

  scanOptions.extraGetValues = &extraCols[0];
  scanOptions.numExtraGetValues = 1;

  const size_t rowlen = NdbDictionary::getRecordRowLength(record);
  char* firstRow = new char[rowlen];

  const Uint32 high_bucket = 100; // top = 100, maxp = 63, p = 37
  const Uint32 low_bucket = 70;   // top = 70, maxp = 63, p = 7
  const Uint32 fragment_rows = 1000;

  // 1. Start with table with just below 2^n buckets in fragment 0.
  require(0 == populateFragment0(ndb, pTab, fragment_rows, high_bucket));

  // 2. Start scan and read a few rows.

  // Scan one row to delete later, and a second
  NdbTransaction* trans;
  NdbScanOperation* scanOp;

  trans = ndb->startTransaction();
  require(trans != NULL);
  scanOp = trans->scanTable(record,
                            NdbOperation::LM_Read,
                            NULL,
                            &scanOptions,
                            sizeof(scanOptions));
  require(scanOp != NULL);
  require(trans->execute(Commit) == 0);

  const char* anyRow;

  Uint32 scanned_rows = 0;
  int queryRes = scanOp->nextResult(&anyRow,
                                    true,
                                    false);
  require(queryRes == 0);
  memcpy(firstRow, anyRow, rowlen);
  scanned_rows ++;

  queryRes = scanOp->nextResult(&anyRow,
                                true,
                                false);
  require(queryRes == 0);
  scanned_rows ++;

  // 3. Shrink table.
  sizeFragment0DbaccHashTable(ndb, record, firstRow, low_bucket);

  // 4. Scan beyond middle buckets.
  while (scanned_rows < fragment_rows / 2)
  {
    queryRes = scanOp->nextResult(&anyRow, true, false);
    require(queryRes == 0);
    scanned_rows ++;
  }

  // 5. Expand table back to original size.
  sizeFragment0DbaccHashTable(ndb, record, firstRow, high_bucket);

  // 6. Complete scan on fragment 0.
  for(;;)
  {
    queryRes = scanOp->nextResult(&anyRow, true, false);
    require(queryRes == 0);
    if (fragment != 0)
    {
      break;
    }
    scanned_rows ++;
  }
  g_err << "Scanned " << scanned_rows << " rows." << endl;

  delete[] firstRow;
  scanOp->close();
  trans->close();

  if (scanned_rows < fragment_rows ||
      scanned_rows > fragment_rows + 2)
  {
    /**
     * Fragment 0 only have fragment_rows rows.
     * First row was deleted and re-inserted twice.
     * So it could legally been seen three times.
     * If scanned more than fragment_rows + 2 rows it is definitely an error.
     */
    return NDBT_FAILED;
  }

  /**
   * Reset error insert.
   */
  NdbRestarter restarter;
  require(restarter.insertErrorInAllNodes(0) == 0);

  return NDBT_OK;
}

/**
 * runScanDuringExpandAndShrinkBack - test case demonstrating
 * Bug#22926938 ACC TABLE SCAN MAY SCAN SAME ROW TWICE
 *
 * 1. Start with table with just above 2^n buckets in fragment 0.
 * 2. Start scan and read about half of the rows in fragment 0.
 * 3. Expand table, due to 1) the scanned buckets in bottom of table are
 *    splitted to top buckets.  And since scan is at about middle of the
 *    table it will not hinder expansion.
 * 4. Shrink table back to original size.  The scanned top buckets will now
 *    be merged back to bottom of table.  But before bug fix top buckets was
 *    marked as unscanned before merge.
 * 5. Complete scan on fragment 0.  Before bug fix some rows was scanned twice.
 */
static int
runScanDuringExpandAndShrinkBack(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* const ndb = GETNDB(step);
  const NdbRecord* const record = pTab->getDefaultRecord();
  require(record != NULL);
  NdbScanOperation::ScanOptions scanOptions;
  scanOptions.optionsPresent = scanOptions.SO_PARALLEL |
                               scanOptions.SO_BATCH |
                               scanOptions.SO_GETVALUE;
  scanOptions.parallel = 1;
  scanOptions.batch = 1;

  Uint32 fragment;
  NdbOperation::GetValueSpec extraCols[1];
  extraCols[0].column = NdbDictionary::Column::FRAGMENT;
  extraCols[0].appStorage = &fragment;
  extraCols[0].recAttr = NULL;

  scanOptions.extraGetValues = &extraCols[0];
  scanOptions.numExtraGetValues = 1;

  const size_t rowlen = NdbDictionary::getRecordRowLength(record);
  char* firstRow = new char[rowlen];

  const Uint32 low_bucket = 129;   // top = 129, maxp = 127, p = 2
  const Uint32 high_bucket = 150;  // top = 150, maxp = 127, p = 23
  const Uint32 fragment_rows = 1000;

  // 1. Start with table with just above 2^n buckets in fragment 0.
  require(0 == populateFragment0(ndb, pTab, fragment_rows, low_bucket));
  sleep(1);

  // 2. Start scan and read about half of the rows in fragment 0.

  // Scan one row to delete later, and a second
  NdbTransaction* trans;
  NdbScanOperation* scanOp;

  trans = ndb->startTransaction();
  require(trans != NULL);
  scanOp = trans->scanTable(record,
                            NdbOperation::LM_Read,
                            NULL,
                            &scanOptions,
                            sizeof(scanOptions));
  require(scanOp != NULL);
  require(trans->execute(Commit) == 0);

  const char* anyRow;

  Uint32 scanned_rows = 0;
  int queryRes = scanOp->nextResult(&anyRow,
                                    true,
                                    false);
  require(queryRes == 0);
  memcpy(firstRow, anyRow, rowlen);
  scanned_rows ++;

  while (scanned_rows < fragment_rows / 2)
  {
    queryRes = scanOp->nextResult(&anyRow, true, false);
    require(queryRes == 0);
    scanned_rows++;
  }

  // 3. Expand table.
  sizeFragment0DbaccHashTable(ndb, record, firstRow, high_bucket);

  // 4. Shrink table back to original size.
  sizeFragment0DbaccHashTable(ndb, record, firstRow, low_bucket);

  // 5. Complete scan on fragment 0.
  for(;;)
  {
    queryRes = scanOp->nextResult(&anyRow, true, false);
    require(queryRes == 0);
    if (fragment != 0)
    {
      break;
    }
    scanned_rows ++;
  }
  g_info << "Scanned " << scanned_rows << " rows." << endl;

  delete[] firstRow;
  scanOp->close();
  trans->close();

  if (scanned_rows < fragment_rows ||
      scanned_rows > fragment_rows + 2)
  {
    /**
     * Fragment 0 only have fragment_rows rows.
     * First row was deleted and re-inserted twice.
     * So it could legally been seen three times.
     * If scanned more than fragment_rows + 2 rows it is definitely an error.
     */
    return NDBT_FAILED;
  }

  /**
   * Reset error insert.
   */
  NdbRestarter restarter;
  require(restarter.insertErrorInAllNodes(0) == 0);

  return NDBT_OK;
}


int
runScanOperation(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb * pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  NdbTransaction* pTrans = pNdb->startTransaction();
  if (pTrans == NULL)
  {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
  if (pOp->readTuples(NdbOperation::LM_CommittedRead) != 0)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  if (pTrans->execute(NdbTransaction::NoCommit) != 0)
  {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  const int acceptError = ctx->getProperty("AcceptError");
  if (pOp->nextResult(true) < 0)
  {
    NDB_ERR(pOp->getNdbError());
    const NdbError err = pOp->getNdbError();
    if (err.code != acceptError)
    {
      ndbout << "Expected error: " << acceptError << endl;
      return NDBT_FAILED;
    }
  }

  pOp->close();
  pTrans->close();
  return NDBT_OK;
}


int
runScanUsingMultipleNdbObjects(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbScanOperation* pOp = NULL;
  NdbTransaction* pTrans = NULL;
  const char* tab_name = ctx->getTab()->getName();
  Ndb_cluster_connection* pCC = &ctx->m_cluster_connection;

  int numOfNdbObjects = 1000;
  Vector<Ndb*> ndbList;
  for (int i = 0; i < numOfNdbObjects; i++)
  {
    Ndb * pNdb = new Ndb(pCC, "TEST_DB");
    ndbList.push_back(pNdb);
    if (pNdb->init() != 0 &&
        pNdb->waitUntilReady(30) != 0)
    {
      NDB_ERR(pNdb->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
      NDB_ERR(pNdb->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }

    pOp = pTrans->getNdbScanOperation(tab_name);
    if (pOp == NULL)
    {
      NDB_ERR(pTrans->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }
    if (pOp->readTuples(NdbOperation::LM_Exclusive) != 0)
    {
      NDB_ERR(pTrans->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }

    if (pTrans->execute(NdbTransaction::NoCommit) != 0)
    {
      NDB_ERR(pTrans->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }

    if (pOp->nextResult(true) < 0)
    {
      NDB_ERR(pOp->getNdbError());
      result = NDBT_FAILED;
      goto cleanup;
    }

    pOp->close();
    pOp = NULL;
    pTrans->close();
    pTrans = NULL;
  }

  cleanup:
  if (pOp != NULL)
  {
    pOp->close();
  }
  if (pTrans != NULL)
  {
    pTrans->close();
  }
  for (uint i = 0; i < ndbList.size(); i++)
  {
    delete ndbList[i];
  }
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
         "Scan and insert error 8081."\
         "Check scanError() return from 'sendDihGetNodesLab'"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 8081);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}
TESTCASE("ScanReadError8115",
         "Scan and insert error 8115."\
         "Check scanError() return from 'sendFragScansLab'"){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("ErrorCode", 8115);
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
TESTCASE("ScanDihError7240",
         "Check that any error from DIH->TC "
         "is correctly returned by TC"){
  TC_PROPERTY("ErrorCode", 7240);
  TC_PROPERTY("AcceptError", 311);
  INITIALIZER(runLoadTable);
  INITIALIZER(runInsertError); //Set 'ErrorCode'
  STEP(runScanOperation);
  FINALIZER(runInsertError);   //Reset ErrorCode
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
TESTCASE("Bug54945", "Need --skip-ndb-optimized-node-selection")
{
  STEP(runBug54945);
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
TESTCASE("TupCheckSumError", ""){
  INITIALIZER(TupErr::createDataBase);
  INITIALIZER(TupErr::doCheckSumQuery);
}
TESTCASE("InterpretNok6000", ""){
  INITIALIZER(TupErr::createDataBase);
  INITIALIZER(TupErr::doInterpretNok6000Query);
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
TESTCASE("Bug16402744",
         "Test scan behaviour with multiple SCAN_FRAGREQ possibly "
         "delayed/incomplete due to a CONTINUEB(ZSEND_FRAG_SCANS) break.")
{
  INITIALIZER(runLoadTable);
  TC_PROPERTY("Parallelism", 240);
  TC_PROPERTY("ErrorCode", 8097);
  STEP(runScanReadError);
  FINALIZER(runClearTable);
}

TESTCASE("ScanDuringShrinkAndExpandBack",
         "Verify that dbacc scan do not scan rows twice if table shrinks and then "
         "expands back.  See bug#22926938.")
{
  STEP(runScanDuringShrinkAndExpandBack);
}

TESTCASE("ScanDuringExpandAndShrinkBack",
         "Verify that dbacc scan do not scan rows twice if table expands and then "
         "shrinks back.  See bug#22926938.")
{
  STEP(runScanDuringExpandAndShrinkBack);
}

TESTCASE("ScanUsingMultipleNdbObjects",
         "Run scan operations in a loop creating a new Ndb"
         "object for every run.")
{
  INITIALIZER(runLoadTable);
  STEP(runScanUsingMultipleNdbObjects);
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
