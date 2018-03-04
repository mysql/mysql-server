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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <Bitmask.hpp>
#include <random.h>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbConfig.hpp>
#include <BlockNumbers.h>
#include <NdbHost.h>

#define CHK1(b) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__; \
    result = NDBT_FAILED; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    result = NDBT_FAILED; \
    break; \
  }

/**
 * TODO 
 *  dirtyWrite, write, dirtyUpdate
 *  delete should be visible to same transaction
 *  
 */
int runLoadTable2(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records, 512, false, 0, true) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsert(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  // Insert records, dont allow any 
  // errors(except temporary) while inserting
  if (hugoTrans.loadTable(GETNDB(step), records, 1, false) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsertTwice(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  // Insert records, expect primary key violation 630
  if (hugoTrans.loadTable(GETNDB(step), records, 1, false) != 630){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runVerifyInsert(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecords(GETNDB(step),  records, 1, false) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsertUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecords(GETNDB(step),  records, batchSize) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runPkDelete(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.pkDelRecords(GETNDB(step),  records) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    // Load table, don't allow any primary key violations
    if (hugoTrans.loadTable(GETNDB(step), records, 512, false) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }  
  g_info << endl;
  return NDBT_OK;
}


int runPkRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int lm = ctx->getProperty("LockMode", NdbOperation::LM_Read);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), records, batchSize,
                                (NdbOperation::LockMode)lm) != NDBT_OK){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int runPkReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), records, batchSize) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
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

int runPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped()) {
    g_info << i << ": ";
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, batchSize) != 0){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int runLocker(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  
  if (hugoTrans.lockRecords(GETNDB(step), records, 10, 500) != 0){
    result = NDBT_FAILED;
  }
  ctx->stopTest();
  
  return result;
}

int
runInsertOne(NDBT_Context* ctx, NDBT_Step* step){
  
  if(ctx->getProperty("InsertCommitted", (Uint32)0) != 0){
    abort();
  }

  while(ctx->getProperty("Read1Performed", (Uint32)0) == 0){
    NdbSleep_MilliSleep(20);
  }
  
  HugoTransactions hugoTrans(*ctx->getTab());
  
  if (hugoTrans.loadTable(GETNDB(step), 1, 1) != 0){
    return NDBT_FAILED;
  }

  ctx->setProperty("InsertCommitted", 1);

  NdbSleep_SecSleep(2);

  return NDBT_OK;
}

/**
 * Insert error 5083 in DBLQH that puts operation into LOG_QUEUED state and puts
 * the operation in the REDO log queue. After a while it will be timed out and
 * the abort code will handle it, the runLoadTableFail method will then abort and
 * be assumed to be ok, so as long as the node doesn't crash we're passing the
 * test case and also the operation should be aborted.
 *
 * If this test case is run on 7.2 it will simply be the same as Fill since we
 * don't queue REDO log operations but rather abort it immediately. So it will
 * pass as well but won't test the desired functionality.
 */
int runLoadTableFail(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = 16;
  int res;
  HugoTransactions hugoTrans(*ctx->getTab());
  res = hugoTrans.loadTable(GETNDB(step), records, 64, true, 0, true, true);
  if (res == 266 || /* Timeout when REDO logging queueing active (or not) */
      res == 0)     /* No error when not REDO logging active and no error insert */
  {
    ndbout << "res = " << res << endl;
    return NDBT_OK;
  }
  /* All other error variants are errors in this case */
  return NDBT_FAILED;
}

int
insertError5083(NDBT_Context* ctx, NDBT_Step* step)
{
   NdbRestarter restarter;
   restarter.insertErrorInAllNodes(5083);
   return 0;
}

int
clearError5083(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  restarter.insertErrorInAllNodes(0);
  return 0;
}

static
int
readOneNoCommit(Ndb* pNdb, NdbConnection* pTrans, 
		const NdbDictionary::Table* tab,NDBT_ResultRow * row){
  int a;
  NdbOperation * pOp = pTrans->getNdbOperation(tab->getName());
  if (pOp == NULL){
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  HugoTransactions tmp(*tab);

  int check = pOp->readTuple();
  if( check == -1 ) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  // Define primary keys
  for(a = 0; a<tab->getNoOfColumns(); a++){
    if (tab->getColumn(a)->getPrimaryKey() == true){
      if(tmp.equalForAttr(pOp, a, 0) != 0){
	NDB_ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }
  }
  
  // Define attributes to read  
  for(a = 0; a<tab->getNoOfColumns(); a++){
    if((row->attributeStore(a) = 
	pOp->getValue(tab->getColumn(a)->getName())) == 0) {
      NDB_ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
  }

  check = pTrans->execute(NoCommit);     
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError(); 
    NDB_ERR(err);
    return err.code;
  }
  return NDBT_OK;
}

int
runReadOne(NDBT_Context* ctx, NDBT_Step* step){

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* tab = ctx->getTab();
  NDBT_ResultRow row1(*tab);
  NDBT_ResultRow row2(*tab);  

  if(ctx->getProperty("Read1Performed", (Uint32)0) != 0){
    abort();
  }

  if(ctx->getProperty("InsertCommitted", (Uint32)0) != 0){
    abort();
  }
  
  NdbConnection * pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    abort();
  }    

  // Read a record with NoCommit
  // Since the record isn't inserted yet it wil return 626
  const int res1 = readOneNoCommit(pNdb, pTrans, tab, &row1);
  g_info << "|- res1 = " << res1 << endl;

  ctx->setProperty("Read1Performed", 1);
  
  while(ctx->getProperty("InsertCommitted", (Uint32)0) == 0 && 
	!ctx->isTestStopped()){
    g_info << "|- Waiting for insert" << endl;
    NdbSleep_MilliSleep(20);
  }
  
  if(ctx->isTestStopped()){
    abort();
  }

  // Now the record should have been inserted
  // Read it once again in the same transaction
  // Should also reutrn 626 if reads are consistent

  // NOTE! Currently it's not possible to start a new operation
  // on a transaction that has returned an error code
  // This is wat fail in this test
  // MASV 20030624
  const int res2 = readOneNoCommit(pNdb, pTrans, tab, &row2);

  pTrans->execute(Commit);
  pNdb->closeTransaction(pTrans);
  g_info << "|- res2 = " << res2 << endl;

  if (res2 == 626 && res1 == res2)    
    return NDBT_OK;
  else
    return NDBT_FAILED;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){
  int batch = 512; //4096;
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(GETNDB(step), batch ) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable2(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step), records, 240) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; }

#define CHECK2(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; }

int runNoCommitSleep(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  int sleepTime = 100; // ms
  for (int i = 2; i < 8; i++){

    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

    ndbout << i <<": Sleeping for " << sleepTime << " ms" << endl;
    NdbSleep_MilliSleep(sleepTime);

    // Dont care about result of these ops
    hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive);
    hugoOps.closeTransaction(pNdb);

    sleepTime = sleepTime *i;
  }

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCommit626(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction
    // Multiple operations
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 626);
  }while(false);

  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runCommit630(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 630);
  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCommit_TryCommit626(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction, TryCommit
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, TryCommit) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction, TryCommit
    // Several operations in one transaction
    // The insert is OK
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 4, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, TryCommit) == 626);
  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCommit_TryCommit630(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  do{
    // Commit transaction, TryCommit
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, TryCommit) == 630);
  }while(false);
  
  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runCommit_CommitAsMuchAsPossible626(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction, CommitAsMuchAsPossible
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction, CommitAsMuchAsPossible
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  } while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCommit_CommitAsMuchAsPossible630(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Commit transaction, CommitAsMuchAsPossible
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.pkDeleteRecord(pNdb, 2) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 630);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 626);
  } while(false);

  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runNoCommit626(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // No commit transaction, readTuple
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // No commit transaction, readTupleExcluive
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
  }while(false);

  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runNoCommit630(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // No commit transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 630);
  }while(false);

  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runNoCommitRollback626(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // No commit transaction, rollback
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // No commit transaction, rollback
    // Multiple operations
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 4, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
  }while(false);

  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int runNoCommitRollback630(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // No commit transaction, rollback
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 630);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}


int runNoCommitAndClose(NDBT_Context* ctx, NDBT_Step* step){
  int i, result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Read 
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for (i = 0; i < 10; i++)
      CHECK(hugoOps.pkReadRecord(pNdb, i, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  
    // Update
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for (i = 0; i < 10; i++)
      CHECK(hugoOps.pkUpdateRecord(pNdb, i) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  
    // Delete
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for (i = 0; i < 10; i++)
      CHECK(hugoOps.pkDeleteRecord(pNdb, i) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Try to insert, record should already exist
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for (i = 0; i < 10; i++)
      CHECK(hugoOps.pkInsertRecord(pNdb, i) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 630);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}



int runCheckRollbackDelete(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{

    // Read value and save it for later
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.saveCopyOfRecord() == NDBT_OK);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Delete record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkDeleteRecord(pNdb, 5) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

    // Check record is deleted
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);

    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is not deleted
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is back to original value
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.compareRecordToCopy() == NDBT_OK);


  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCheckRollbackUpdate(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  int numRecords = 5;
  do{
    
    // Read value and save it for later
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 1, numRecords) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK); // Update value 0
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Update  record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkUpdateRecord(pNdb, 1, numRecords, 5) == 0);// Updates value 5
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
  
    // Check record is updated
    CHECK(hugoOps.pkReadRecord(pNdb, 1, numRecords, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(5) == NDBT_OK); // Updates value 5
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);

    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is back to original value
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 1, numRecords, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK); // Updates value 0

  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCheckRollbackDeleteMultiple(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Read value and save it for later
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 10) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
    
    Uint32 updatesValue = 0;
    Uint32 j;
    for(Uint32 i = 0; i<1; i++){
      // Read  record 5 - 10
      CHECK(hugoOps.startTransaction(pNdb) == 0);  
      CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      for(j = 0; j<10; j++){
	// Update  record 5 - 10
	updatesValue++;
	CHECK(hugoOps.pkUpdateRecord(pNdb, 5, 10, updatesValue) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

	CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	CHECK(hugoOps.verifyUpdatesValue(updatesValue) == 0);
      }      
      
      for(j = 0; j<10; j++){
	// Delete record 5 - 10 times
	CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

#if 0
	// Check records are deleted
	CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
#endif

	updatesValue++;
	CHECK(hugoOps.pkInsertRecord(pNdb, 5, 10, updatesValue) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	
	CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	CHECK(hugoOps.verifyUpdatesValue(updatesValue) == 0);
      }

      CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

      // Check records are deleted
      CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
      CHECK(hugoOps.execute_Rollback(pNdb) == 0);
      
      CHECK(hugoOps.closeTransaction(pNdb) == 0);
    }
    
    // Check records are not deleted
    // after rollback
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK);
    
  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}


int runCheckImplicitRollbackDelete(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Read  record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
    
    // Update  record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkUpdateRecord(pNdb, 5) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  
    // Delete record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkDeleteRecord(pNdb, 5) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is not deleted
    // Close transaction should have rollbacked
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 1, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runCheckCommitDelete(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Read  10 records
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
  
    // Update 10 records
    CHECK(hugoOps.pkUpdateRecord(pNdb, 5, 10) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
  
    // Delete 10 records
    CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record's are deleted
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 626);

  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runRollbackNothing(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  do{
    // Delete record 5 - 15
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
    // Rollback 
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check records are not deleted
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, 10, NdbOperation::LM_Exclusive) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);  

    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);

  }while(false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runMassiveRollback(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  const int records = 4 * restarter.getNumDbNodes();

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  const Uint32 OPS_PER_TRANS = 256;
  const Uint32 OPS_TOTAL = 4096;

  for(int row = 0; row < records; row++){
    int res;
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for(Uint32 i = 0; i<OPS_TOTAL; i += OPS_PER_TRANS){
      for(Uint32 j = 0; j<OPS_PER_TRANS; j++){
	CHECK(hugoOps.pkUpdateRecord(pNdb, row, 1, i) == 0);
      }
      g_info << "Performed " << (i+OPS_PER_TRANS) << " updates on row: " << row
	     << endl;
      if(result != NDBT_OK){
	break;
      }
      res = hugoOps.execute_NoCommit(pNdb);
      if(res != 0){
	NdbError err = pNdb->getNdbError(res);
	CHECK(err.classification == NdbError::TimeoutExpired);
	break;
      }
    }
    if(result != NDBT_OK){
      break;
    }
    g_info << "executeRollback" << endl;
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  }
  
  hugoOps.closeTransaction(pNdb);
  return result;
}

int
runMassiveRollback2(NDBT_Context* ctx, NDBT_Step* step){

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), 1) != 0){
    return NDBT_FAILED;
  }

  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  const Uint32 OPS_TOTAL = 4096;
  const Uint32 LOOPS = 10;
  
  for(Uint32 loop = 0; loop<LOOPS; loop++){
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for(Uint32 i = 0; i<OPS_TOTAL-1; i ++){
      if((i & 1) == 0){
	CHECK(hugoOps.pkUpdateRecord(pNdb, 0, 1, loop) == 0);
      } else {
	CHECK(hugoOps.pkUpdateRecord(pNdb, 1, 1, loop) == 0);
      }
    }
    CHECK(hugoOps.execute_Commit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  }
  
  hugoOps.closeTransaction(pNdb);
  return result;
}

int
runMassiveRollback3(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  const Uint32 BATCH = 10;
  const Uint32 OPS_TOTAL = 50;
  const Uint32 LOOPS = 100;
  
  for(Uint32 loop = 0; loop<LOOPS; loop++)
  {
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    bool ok = true;
    for (Uint32 i = 0; i<OPS_TOTAL; i+= BATCH)
    {
      CHECK(hugoOps.pkInsertRecord(pNdb, i, BATCH, 0) == 0);
      if (hugoOps.execute_NoCommit(pNdb) != 0)
      {
	ok = false;
	break;
      }
    }
    hugoOps.execute_Rollback(pNdb);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  }
  
  hugoOps.closeTransaction(pNdb);
  return result;
}

int
runMassiveRollback4(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  const Uint32 BATCH = 10;
  const Uint32 OPS_TOTAL = 20;
  const Uint32 LOOPS = 100;
  
  for(Uint32 loop = 0; loop<LOOPS; loop++)
  {
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    bool ok = true;
    for (Uint32 i = 0; i<OPS_TOTAL; i+= BATCH)
    {
      CHECK(hugoOps.pkInsertRecord(pNdb, i, BATCH, 0) == 0);
      CHECK(hugoOps.pkDeleteRecord(pNdb, i, BATCH) == 0);
      if (hugoOps.execute_NoCommit(pNdb) != 0)
      {
	ok = false;
	break;
      }
    }
    hugoOps.execute_Rollback(pNdb);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  }
  
  hugoOps.closeTransaction(pNdb);
  return result;
}

/**
 * TUP errors
 */
struct TupError 
{
  enum Bits {
    TE_VARSIZE  = 0x1,
    TE_MULTI_OP = 0x2,
    TE_DISK     = 0x4,
    TE_REPLICA  = 0x8,
    TE_OI       = 0x10, // Ordered index
    TE_UI       = 0x20  // Unique hash index
  };
  int op;
  int error;
  int bits;
};

static
TupError 
f_tup_errors[] = 
{
  { NdbOperation::InsertRequest, 4014, 0 },       // Out of undo buffer
  { NdbOperation::InsertRequest, 4015, TupError::TE_DISK }, // Out of log space
  { NdbOperation::InsertRequest, 4016, 0 },       // AI Inconsistency
  { NdbOperation::InsertRequest, 4017, 0 },       // Out of memory
  { NdbOperation::InsertRequest, 4018, 0 },       // Null check error
  { NdbOperation::InsertRequest, 4019, TupError::TE_REPLICA }, //Alloc rowid error
  { NdbOperation::InsertRequest, 4020, TupError::TE_MULTI_OP }, // Size change error
  { NdbOperation::InsertRequest, 4021, TupError::TE_DISK },    // Out of disk space
  { NdbOperation::InsertRequest, 4022, TupError::TE_OI },  // Tux add error first
  { NdbOperation::InsertRequest, 4023, TupError::TE_OI },  // Tux add error last
  { NdbOperation::InsertRequest, 4030, TupError::TE_UI },
  { NdbOperation::UpdateRequest, 4030, TupError::TE_UI },  // UI trig error
  { -1, 0, 0 }
};

static
int
compare(unsigned block,
        struct ndb_mgm_events * time0,
        struct ndb_mgm_events * time1)
{
  int diff = 0;
  for (int i = 0; i < time0->no_of_events; i++)
  {
    if (time0->events[i].MemoryUsage.block != block)
      continue;

    unsigned node = time0->events[i].source_nodeid;

    for (int j = 0; j < time1->no_of_events; j++)
    {
      if (time1->events[j].MemoryUsage.block != block)
        continue;

      if (time1->events[j].source_nodeid != node)
        continue;

      diff +=
        time0->events[i].MemoryUsage.pages_used -
        time1->events[j].MemoryUsage.pages_used;
    }
  }
  return diff;
}

int
runTupErrors(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  HugoTransactions hugoTrans(*ctx->getTab());
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const NdbDictionary::Table * tab = ctx->getTab();
  int i;
  int bits = TupError::TE_MULTI_OP;
  for(i = 0; i<tab->getNoOfColumns(); i++)
  {
    if (tab->getColumn(i)->getArrayType() != NdbDictionary::Column::ArrayTypeFixed)
      bits |= TupError::TE_VARSIZE;
    if (tab->getColumn(i)->getStorageType()!= NdbDictionary::Column::StorageTypeMemory)
      bits |= TupError::TE_DISK;
  }

  if (restarter.getNumDbNodes() >= 2)
  {
    bits |= TupError::TE_REPLICA;
  }

  NdbDictionary::Dictionary::List l;
  pNdb->getDictionary()->listIndexes(l, tab->getName());
  for (i = 0; i<(int)l.count; i++)
  {
    if (DictTabInfo::isOrderedIndex(l.elements[i].type))
      bits |= TupError::TE_OI;
    if (DictTabInfo::isUniqueIndex(l.elements[i].type))
      bits |= TupError::TE_UI;
  }

  /**
   * Insert
   */
  for(i = 0; f_tup_errors[i].op != -1; i++)
  {
    if (f_tup_errors[i].op != NdbOperation::InsertRequest)
    {
      continue;
    }

    if ((f_tup_errors[i].bits & bits) != f_tup_errors[i].bits)
    {
      g_err << "Skipping " << f_tup_errors[i].error
            << " - req bits: " << hex << f_tup_errors[i].bits
            << " bits: " << hex << bits << endl;
      continue;
    }
    
    g_err << "Testing error insert: " << f_tup_errors[i].error << endl;
    restarter.insertErrorInAllNodes(f_tup_errors[i].error);

    struct ndb_mgm_events * before =
      ndb_mgm_dump_events(restarter.handle, NDB_LE_MemoryUsage, 0, 0);
    if (before == 0)
    {
      ndbout_c("ERROR: failed to fetch report!");
      return NDBT_FAILED;;
    }

    if (f_tup_errors[i].bits & TupError::TE_MULTI_OP)
    {
      
    }
    else
    {
      hugoTrans.loadTable(pNdb, 5);
    }
    restarter.insertErrorInAllNodes(0);
    if (hugoTrans.clearTable(pNdb, 5) != 0)
    {
      return NDBT_FAILED;
    }      

    struct ndb_mgm_events * after =
      ndb_mgm_dump_events(restarter.handle, NDB_LE_MemoryUsage, 0, 0);
    if (after == 0)
    {
      ndbout_c("ERROR: failed to fetch report!");
      return NDBT_FAILED;;
    }

    /**
     * check memory leak
     */
    if (compare(DBTUP, before, after) != 0)
    {
      ndbout_c("memleak detected!!");
      return NDBT_FAILED;;
    }
    free(before);
    free(after);
  }

  /**
   * update
   */
  struct ndb_mgm_events * before =
    ndb_mgm_dump_events(restarter.handle, NDB_LE_MemoryUsage, 0, 0);
  hugoTrans.loadTable(pNdb, 5);
  for(i = 0; f_tup_errors[i].op != -1; i++)
  {
    if (f_tup_errors[i].op != NdbOperation::UpdateRequest)
    {
      continue;
    }

    if ((f_tup_errors[i].bits & bits) != f_tup_errors[i].bits)
    {
      g_err << "Skipping " << f_tup_errors[i].error
            << " - req bits: " << hex << f_tup_errors[i].bits
            << " bits: " << hex << bits << endl;
      continue;
    }

    g_err << "Testing error insert: " << f_tup_errors[i].error << endl;
    restarter.insertErrorInAllNodes(f_tup_errors[i].error);
    if (f_tup_errors[i].bits & TupError::TE_MULTI_OP)
    {

    }
    else
    {
      hugoTrans.scanUpdateRecords(pNdb, 5);
    }
    restarter.insertErrorInAllNodes(0);
    if (hugoTrans.scanUpdateRecords(pNdb, 5) != 0)
    {
      return NDBT_FAILED;
    }
  }
  if (hugoTrans.clearTable(pNdb) != 0)
  {
    return NDBT_FAILED;
  }

  struct ndb_mgm_events * after =
    ndb_mgm_dump_events(restarter.handle, NDB_LE_MemoryUsage, 0, 0);

  int diff = compare(DBTUP, before, after);
  free(before);
  free(after);

  if (diff != 0)
  {
    ndbout_c("memleak detected!!");
    return NDBT_FAILED;;
  }

  return NDBT_OK;
}

int
runInsertError(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  HugoOperations hugoOp1(*ctx->getTab());
  HugoOperations hugoOp2(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  NdbRestarter restarter;
  restarter.insertErrorInAllNodes(4017);
  const Uint32 LOOPS = 10;
  for (Uint32 i = 0; i<LOOPS; i++)
  {
    CHECK(hugoOp1.startTransaction(pNdb) == 0);  
    CHECK(hugoOp1.pkInsertRecord(pNdb, 1) == 0);
    
    CHECK(hugoOp2.startTransaction(pNdb) == 0);
    CHECK(hugoOp2.pkReadRecord(pNdb, 1, 1) == 0);
    
    CHECK(hugoOp1.execute_async_prepare(pNdb, NdbTransaction::Commit) == 0);
    CHECK(hugoOp2.execute_async_prepare(pNdb, NdbTransaction::Commit) == 0);
    hugoOp1.wait_async(pNdb);
    hugoOp2.wait_async(pNdb);
    CHECK(hugoOp1.closeTransaction(pNdb) == 0);
    CHECK(hugoOp2.closeTransaction(pNdb) == 0);
  }
  
  restarter.insertErrorInAllNodes(0);
  
  return result;
}

int
runInsertError2(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOp1(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  NdbRestarter restarter;
  restarter.insertErrorInAllNodes(4017);
  
  const Uint32 LOOPS = 1;
  for (Uint32 i = 0; i<LOOPS; i++)
  {
    CHECK(hugoOp1.startTransaction(pNdb) == 0);  
    CHECK(hugoOp1.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOp1.pkDeleteRecord(pNdb, 1) == 0);
    
    hugoOp1.execute_NoCommit(pNdb);
    CHECK(hugoOp1.closeTransaction(pNdb) == 0);
  }
  
  restarter.insertErrorInAllNodes(0);
  return NDBT_OK;
}
  
int
runBug25090(NDBT_Context* ctx, NDBT_Step* step){
  
  Ndb* pNdb = GETNDB(step);
  //NdbDictionary::Dictionary * dict = pNdb->getDictionary();

  HugoOperations ops(*ctx->getTab());
  
  int loops = ctx->getNumLoops();
  //const int rows = ctx->getNumRecords();
  
  while (loops--)
  {
    ops.startTransaction(pNdb);
    ops.pkReadRecord(pNdb, 1, 1);
    ops.execute_Commit(pNdb, AO_IgnoreError);
    sleep(10);
    ops.closeTransaction(pNdb);
  }
  
  return NDBT_OK;
}

int
runDeleteRead(NDBT_Context* ctx, NDBT_Step* step){
  
  Ndb* pNdb = GETNDB(step);

  const NdbDictionary::Table* tab = ctx->getTab();
  NDBT_ResultRow row(*ctx->getTab());
  HugoTransactions tmp(*ctx->getTab());

  int a;
  int loops = ctx->getNumLoops();
  //const int rows = ctx->getNumRecords();
  
  while (loops--)
  {
    NdbTransaction* pTrans = pNdb->startTransaction();
    NdbOperation* pOp = pTrans->getNdbOperation(tab->getName());
    pOp->deleteTuple();
    tmp.equalForRow(pOp, loops);
    
    // Define attributes to read  
    for(a = 0; a<tab->getNoOfColumns(); a++)
    {
      if((row.attributeStore(a) = pOp->getValue(tab->getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }

    pTrans->execute(Commit);
    pTrans->close();

    pTrans = pNdb->startTransaction();    
    pOp = pTrans->getNdbOperation(tab->getName());
    pOp->insertTuple();
    tmp.setValues(pOp, loops, 0);

    pOp = pTrans->getNdbOperation(tab->getName());
    pOp->deleteTuple();
    tmp.equalForRow(pOp, loops);
    for(a = 0; a<tab->getNoOfColumns(); a++)
    {
      if((row.attributeStore(a) = pOp->getValue(tab->getColumn(a)->getName())) == 0) 
      {
	NDB_ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }
    if (pTrans->execute(Commit) != 0)
    {
      NDB_ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }

    pTrans->close();    
  }
  
  return NDBT_OK;
}

int
runBug27756(NDBT_Context* ctx, NDBT_Step* step)
{
  
  Ndb* pNdb = GETNDB(step);
  //NdbDictionary::Dictionary * dict = pNdb->getDictionary();
  
  HugoOperations ops(*ctx->getTab());

  int loops = ctx->getNumLoops();
  //const int rows = ctx->getNumRecords();
  
  Vector<Uint64> copies;
  while (loops--)
  {
    ops.startTransaction(pNdb);
    ops.pkInsertRecord(pNdb, 1, 1);
    ops.execute_NoCommit(pNdb);
    
    NdbTransaction* pTrans = ops.getTransaction();
    NdbOperation* op = pTrans->getNdbOperation(ctx->getTab()->getName());
    op->interpretedUpdateTuple();
    ops.equalForRow(op, 1);
    NdbRecAttr* attr = op->getValue(NdbDictionary::Column::COPY_ROWID);
    ops.execute_NoCommit(pNdb);
    
    copies.push_back(attr->u_64_value());
    ndbout_c("copy at: %llx", copies.back());
    ops.execute_NoCommit(pNdb);
    
    ops.pkDeleteRecord(pNdb, 1, 1);
    ops.execute_NoCommit(pNdb);
    
    if (loops & 1)
    {
      ops.execute_Rollback(pNdb);
      ops.closeTransaction(pNdb);
    }
    else
    {
      ops.execute_Commit(pNdb);
      ops.closeTransaction(pNdb);
      ops.clearTable(pNdb, 100);
    }
  }
  
  for (Uint32 i = 0; i<copies.size(); i++)
    if (copies[i] != copies.back())
    {
      ndbout_c("Memleak detected");
      return NDBT_FAILED;
    }
  
  return NDBT_OK;
}

int
runBug28073(NDBT_Context *ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const NdbDictionary::Table *table= ctx->getTab();
  HugoOperations hugoOp1(*table);
  HugoOperations hugoOp2(*table);
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  bool inserted= false;

  while (loops--)
  {
    if (!inserted)
    {
      CHECK(hugoOp1.startTransaction(pNdb) == 0);
      CHECK(hugoOp1.pkInsertRecord(pNdb, 1, 1) == 0);
      CHECK(hugoOp1.execute_Commit(pNdb) == 0);
      CHECK(hugoOp1.closeTransaction(pNdb) == 0);
      inserted= 1;
    }

    // Use TC hint to hit the same node in both transactions.
    Uint32 key_val= 0;
    const char *key= (const char *)(&key_val);
    CHECK(hugoOp1.startTransaction(pNdb, table, key, 4) == 0);
    CHECK(hugoOp2.startTransaction(pNdb, table, key, 4) == 0);

    // First take 2*read lock on the tuple in transaction 1.
    for (Uint32 i= 0; i < 2; i++)
    {
      CHECK(hugoOp1.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read) == 0);
      CHECK(hugoOp1.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read) == 0);
    }
    CHECK(hugoOp1.execute_NoCommit(pNdb) == 0);

    // Now send ops in two transactions, one batch.
    // First 2*read in transaction 2.
    for (Uint32 i= 0; i < 2; i++)
    {
      CHECK(hugoOp2.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read) == 0);
      CHECK(hugoOp2.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read) == 0);
    }
    CHECK(hugoOp2.execute_async_prepare(pNdb, NdbTransaction::NoCommit) == 0);

    // Second op an update in transaction 1.
    CHECK(hugoOp1.pkUpdateRecord(pNdb, 1, 1) == 0);
    CHECK(hugoOp1.execute_async_prepare(pNdb, NdbTransaction::Commit) == 0);

    // Transaction 1 will now hang waiting on transaction 2 to commit before it
    // can upgrade its read lock to a write lock.
    // With the bug, we get a node failure due to watchdog timeout here.
    CHECK(hugoOp2.wait_async(pNdb) == 0);

    // Now commit transaction 2, we should see transaction 1 finish with the
    // update.
    CHECK(hugoOp2.execute_async_prepare(pNdb, NdbTransaction::Commit) == 0);
    CHECK(hugoOp2.wait_async(pNdb) == 0);
    // No error check, as transaction 1 may have terminated already.
    hugoOp1.wait_async(pNdb);

    CHECK(hugoOp1.closeTransaction(pNdb) == 0);
    CHECK(hugoOp2.closeTransaction(pNdb) == 0);
  }

  return result;
}

int
runBug20535(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table * tab = ctx->getTab();

  bool hasDefault = false;
  for (Uint32 i = 0; i<(Uint32)tab->getNoOfColumns(); i++)
  {
    if (tab->getColumn(i)->getNullable() ||
        tab->getColumn(i)->getDefaultValue())
    {
      hasDefault = true;
      break;
    }
  }
  
  if (!hasDefault)
    return NDBT_OK;

  HugoTransactions hugoTrans(* tab);
  hugoTrans.loadTable(pNdb, 1);

  NdbTransaction* pTrans = pNdb->startTransaction();
  NdbOperation* pOp = pTrans->getNdbOperation(tab->getName());
  pOp->deleteTuple();
  hugoTrans.equalForRow(pOp, 0);
  if (pTrans->execute(NoCommit) != 0)
    return NDBT_FAILED;

  pOp = pTrans->getNdbOperation(tab->getName());
  pOp->insertTuple();
  hugoTrans.equalForRow(pOp, 0);
  for (Uint32 i = 0; i<(Uint32)tab->getNoOfColumns(); i++)
  {
    if (!tab->getColumn(i)->getPrimaryKey() &&
        !tab->getColumn(i)->getNullable() &&
        !tab->getColumn(i)->getDefaultValue())
    {
      hugoTrans.setValueForAttr(pOp, i, 0, 1);
    }
  }
  
  if (pTrans->execute(Commit) != 0)
    return NDBT_FAILED;
  
  pTrans->close();

  pTrans = pNdb->startTransaction();
  pOp = pTrans->getNdbOperation(tab->getName());
  pOp->readTuple();
  hugoTrans.equalForRow(pOp, 0);
  Vector<NdbRecAttr*> values;
  for (Uint32 i = 0; i<(Uint32)tab->getNoOfColumns(); i++)
  {
    if (!tab->getColumn(i)->getPrimaryKey() &&
        (tab->getColumn(i)->getNullable() ||
         tab->getColumn(i)->getDefaultValue()))
    {
      values.push_back(pOp->getValue(i));
    }
  }
  
  if (pTrans->execute(Commit) != 0)
    return NDBT_FAILED;

  bool defaultOk = true;
  for (unsigned int i = 0; i<values.size(); i++)
  {
    const NdbRecAttr* recAttr = values[i];
    const NdbDictionary::Column* col = recAttr->getColumn();
    unsigned int defaultLen = 0;
    const char* def = (const char*) col->getDefaultValue(&defaultLen);
      
    if (def)
    {
      /* Column has a native default, check that it was set */

      if (!recAttr->isNULL())
      {
        if (memcmp(def, recAttr->aRef(), defaultLen) != 0)
        {
          defaultOk = false;
          ndbout_c("column %s does not have correct default value",
                   recAttr->getColumn()->getName());
        }
      }
      else
      {
        defaultOk = false;
        ndbout_c("column %s is null, should have default value",
                 recAttr->getColumn()->getName());
      }
    }
    else
    {
      /* Column has Null as its default */      
      if (!recAttr->isNULL())
      {
        defaultOk = false;
        ndbout_c("column %s is not NULL", recAttr->getColumn()->getName());
      }
    }
  }
  
  pTrans->close();  
  
  if (defaultOk)
    return NDBT_OK;
  else
    return NDBT_FAILED;
}


int
runDDInsertFailUpdateBatch(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbRestarter restarter;
  
  const NdbDictionary::Table * tab = ctx->getTab();
  
  int errCode = 0;
  int expectedError = 0;
  {
    bool tabHasDD = false;
    for(int i = 0; i<tab->getNoOfColumns(); i++)
    {
      tabHasDD |= (tab->getColumn(i)->getStorageType() == 
                   NdbDictionary::Column::StorageTypeDisk);
    }
    
    if (tabHasDD)
    {
      errCode = 4021;
      expectedError = 1601;
    }
    else
    {
      NdbDictionary::Dictionary::List l;
      pNdb->getDictionary()->listIndexes(l, tab->getName());
      for (Uint32 i = 0; i<l.count; i++)
      {
        if (DictTabInfo::isOrderedIndex(l.elements[i].type))
        {
          errCode = 4023;
          expectedError = 9999;
          break;
        }
      }
    }

    if (errCode == 0)
    {
      ndbout_c("Table %s has no disk attributes or ordered indexes, skipping",
               tab->getName());
      return NDBT_OK;
    }
  }

  HugoOperations hugoOps(*ctx->getTab());

  int result = NDBT_OK;
  
  for (Uint32 loop = 0; loop < 100; loop ++)
  {
    restarter.insertErrorInAllNodes(errCode);
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    
    /* Create batch with insert op (which will fail due to disk allocation issue)
     * followed by update op on same pk
     * Transaction will abort due to insert failure, and reason should be
     * disk space exhaustion, not any issue with the update.
     */
    CHECK(hugoOps.pkInsertRecord(pNdb, loop, 1, 0) == 0);
    
    /* Add up to 16 updates after the insert */
    Uint32 numUpdates = 1 + (loop % 15);
    for (Uint32 updateCnt = 0; updateCnt < numUpdates; updateCnt++)
      CHECK(hugoOps.pkUpdateRecord(pNdb, loop, 1, 1+updateCnt) == 0);
    
    CHECK(hugoOps.execute_Commit(pNdb) != 0); /* Expect failure */
    
    NdbError err= hugoOps.getTransaction()->getNdbError();
    
    CHECK(err.code == expectedError);
    
    hugoOps.closeTransaction(pNdb);
  }  

  restarter.insertErrorInAllNodes(0);
  
  return result;
}

// Bug34348

#define chk1(b) \
  if (!(b)) { g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ << endl; result = NDBT_FAILED; continue; }
#define chk2(b, e) \
  if (!(b)) { g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ << ": " << e << endl; result = NDBT_FAILED; continue; }

const char* tabname_bug34348 = "TBug34348";

int
runBug34348insert(NDBT_Context* ctx, NDBT_Step* step,
                  HugoOperations& ops, int i, bool* rangeFull)
{
  const int rangeFullError = 633;
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  while (result == NDBT_OK)
  {
    int code = 0;
    chk2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    chk2(ops.pkInsertRecord(pNdb, i, 1) == 0, ops.getNdbError());
    chk2(ops.execute_Commit(pNdb) == 0 || (code = ops.getNdbError().code) == rangeFullError, ops.getNdbError());
    ops.closeTransaction(pNdb);
    *rangeFull = (code == rangeFullError);
    break;
  }
  return result;
}

int
runBug34348delete(NDBT_Context* ctx, NDBT_Step* step,
                  HugoOperations& ops, int i)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  while (result == NDBT_OK)
  {
    chk2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    chk2(ops.pkDeleteRecord(pNdb, i, 1) == 0, ops.getNdbError());
    chk2(ops.execute_Commit(pNdb) == 0, ops.getNdbError());
    ops.closeTransaction(pNdb);
    break;
  }
  return result;
}

int
runBug34348(NDBT_Context* ctx, NDBT_Step* step)
{
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  NdbRestarter restarter;
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();
  const int errInsDBACC = 3002;
  const int errInsCLEAR = 0;
  Uint32* rowmask = 0;

  while (result == NDBT_OK)
  {
    chk1(restarter.insertErrorInAllNodes(errInsDBACC) == 0);
    ndbout << "error insert " << errInsDBACC << " done" << endl;

    const NdbDictionary::Table* pTab = 0;
    while (result == NDBT_OK)
    {
      (void)pDict->dropTable(tabname_bug34348);
      NdbDictionary::Table tab(tabname_bug34348);
      {
        NdbDictionary::Column col("a");
        col.setType(NdbDictionary::Column::Unsigned);
        col.setPrimaryKey(true);
        tab.addColumn(col);
      }
      {
        NdbDictionary::Column col("b");
        col.setType(NdbDictionary::Column::Unsigned);
        col.setNullable(false);
        tab.addColumn(col);
      }
      chk2(pDict->createTable(tab) == 0, pDict->getNdbError());
      chk2((pTab = pDict->getTable(tabname_bug34348)) != 0, pDict->getNdbError());
      break;
    }

    HugoOperations ops(*pTab);
    ops.setQuiet();

    int rowmaxprev = 0;
    int loop = 0;
    while (result == NDBT_OK && loop < loops)
    {
      ndbout << "loop:" << loop << endl;
      int rowcnt = 0;

      // fill up
      while (result == NDBT_OK)
      {
        bool rangeFull;
        chk1(runBug34348insert(ctx, step, ops, rowcnt, &rangeFull) == NDBT_OK);
        if (rangeFull)
        {
          // 360449 (1 fragment)
          ndbout << "dir range full at " << rowcnt << endl;
          break;
        }
        rowcnt++;
      }
      chk1(result == NDBT_OK);
      const int rowmax = rowcnt;

      if (loop == 0)
        rowmaxprev = rowmax;
      else
        chk2(rowmaxprev == rowmax, "rowmaxprev:" << rowmaxprev << " rowmax:" << rowmax);

      const int sz = (rowmax + 31) / 32;
      delete [] rowmask;
      rowmask = new Uint32 [sz];
      BitmaskImpl::clear(sz, rowmask);
      {
        int i;
        for (i = 0; i < rowmax; i++)
          BitmaskImpl::set(sz, rowmask, i);
      }

      // random delete until insert succeeds
      while (result == NDBT_OK)
      {
        int i = myRandom48(rowmax);
        if (!BitmaskImpl::get(sz, rowmask, i))
          continue;
        chk1(runBug34348delete(ctx, step, ops, i) == NDBT_OK);
        BitmaskImpl::clear(sz, rowmask, i);
        rowcnt--;
        bool rangeFull;
        chk1(runBug34348insert(ctx, step, ops, rowmax, &rangeFull) == NDBT_OK);
        if (!rangeFull)
        {
          chk1(runBug34348delete(ctx, step, ops, rowmax) == NDBT_OK);
          // 344063 (1 fragment)
          ndbout << "dir range released at " << rowcnt << endl;
          break;
        }
      }
      chk1(result == NDBT_OK);
      require(BitmaskImpl::count(sz, rowmask)== (Uint32)rowcnt);

      // delete about 1/2 remaining
      while (result == NDBT_OK)
      {
        int i;
        for (i = 0; result == NDBT_OK && i < rowmax; i++)
        {
          if (!BitmaskImpl::get(sz, rowmask, i))
            continue;
          if (myRandom48(100) < 50)
            continue;
          chk1(runBug34348delete(ctx, step, ops, i) == NDBT_OK);
          BitmaskImpl::clear(sz, rowmask, i);
          rowcnt--;
        }
        ndbout << "deleted down to " << rowcnt << endl;
        break;
      }
      chk1(result == NDBT_OK);
      require(BitmaskImpl::count(sz, rowmask)== (Uint32)rowcnt);

      // insert until full again
      while (result == NDBT_OK)
      {
        int i;
        for (i = 0; result == NDBT_OK && i < rowmax; i++)
        {
          if (BitmaskImpl::get(sz, rowmask, i))
            continue;
          bool rangeFull;
          chk1(runBug34348insert(ctx, step, ops, i, &rangeFull) == NDBT_OK);
          // assume all can be inserted back
          chk2(!rangeFull, "dir range full too early at " << rowcnt);
          BitmaskImpl::set(sz, rowmask, i);
          rowcnt++;
        }
        chk1(result == NDBT_OK);
        ndbout << "inserted all back to " << rowcnt << endl;
        break;
      }
      chk1(result == NDBT_OK);
      require(BitmaskImpl::count(sz, rowmask)== (Uint32)rowcnt);

      // delete all
      while (result == NDBT_OK)
      {
        int i;
        for (i = 0; result == NDBT_OK && i < rowmax; i++)
        {
          if (!BitmaskImpl::get(sz, rowmask, i))
            continue;
          chk1(runBug34348delete(ctx, step, ops, i) == NDBT_OK);
          BitmaskImpl::clear(sz, rowmask, i);
          rowcnt--;
        }
        ndbout << "deleted all" << endl;
        break;
      }
      chk1(result == NDBT_OK);
      require(BitmaskImpl::count(sz, rowmask)== (Uint32)rowcnt);
      require(rowcnt == 0);

      loop++;
    }

    chk2(pDict->dropTable(tabname_bug34348) == 0, pDict->getNdbError());

    chk1(restarter.insertErrorInAllNodes(errInsCLEAR) == 0);
    ndbout << "error insert clear done" << endl;
    break;
  }

  if (result != NDBT_OK && restarter.insertErrorInAllNodes(errInsCLEAR) != 0)
    g_err << "error insert clear failed" << endl;

  delete [] rowmask;
  rowmask = 0;
  return result;
}

#define check(b, e) \
  if (!(b)) { g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ << ": " << e.getNdbError() << endl; return NDBT_FAILED; }

int runUnlocker(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("Batchsize", 1);
  int doubleUnlock = ctx->getProperty("DoubleUnlock", (Uint32)0);
  int lm = ctx->getProperty("LockMode", NdbOperation::LM_Read);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* ndb = GETNDB(step);

  g_err << "Unlocker : ";
  g_err << "Loops = " << loops << " Records = " << records << " Batchsize = " 
        << batchSize << endl;

  while(i++ < loops) 
  {
    g_err << i << " ";

    check(hugoOps.startTransaction(ndb) == 0, (*ndb));

    const int maxRetries = 10;
    int retryAttempt = 0;
    int r = records;
    Vector<const NdbLockHandle*> lockHandles;

    while(r > 0)
    {
      int batchContents = MIN(r, batchSize);

      check(hugoOps.pkReadRecordLockHandle(ndb,
                                           lockHandles,
                                           records - r,
                                           batchContents,
                                           (NdbOperation::LockMode)lm) == 0, 
            hugoOps);

      r-= batchContents;

      if (hugoOps.execute_NoCommit(ndb) != 0)
      {
        NdbError err = hugoOps.getNdbError();
        if ((err.status == NdbError::TemporaryError) &&
            retryAttempt < maxRetries){
          NDB_ERR(err);
          NdbSleep_MilliSleep(50);
          retryAttempt++;
          lockHandles.clear();
          check(hugoOps.closeTransaction(ndb) == 0,
                hugoOps);
          check(hugoOps.startTransaction(ndb) == 0, (*ndb));
          continue;
        }
        NDB_ERR(err);
        return NDBT_FAILED;
      }

      check(hugoOps.pkUnlockRecord(ndb,
                                  lockHandles) == 0, 
            hugoOps);
      
      check(hugoOps.execute_NoCommit(ndb) == 0, 
            hugoOps);

      if (doubleUnlock)
      {
        NdbOperation::AbortOption ao;
        switch(rand() % 2)
        {
        case 0:
          ao = NdbOperation::AbortOnError;
          break;
        case 1:
        default:
          ao = NdbOperation::AO_IgnoreError;
          break;
        }

        g_err << "Double unlock, abort option is "
              << ao << endl;

        /* Failure scenario */
        check(hugoOps.pkUnlockRecord(ndb,
                                     lockHandles,
                                     0,    // offset
                                     ~(0), // NumRecords
                                     ao) == 0, 
              hugoOps);
        
        check(hugoOps.execute_NoCommit(ndb,
                                       DefaultAbortOption) != 0, 
              hugoOps);

        /* 417 = Bad operation reference */
        check(hugoOps.getNdbError().code == 417,
              hugoOps);

        
        if (ao == NdbOperation::AbortOnError)
        {
          /* Restart transaction and continue with next loop iteration */
          r = 0;
          lockHandles.clear();
          check(hugoOps.closeTransaction(ndb) == 0,
                hugoOps);
          check(hugoOps.startTransaction(ndb) == 0, (*ndb));
          
          continue;
        }
        /* Otherwise, IgnoreError, so let's attempt to
         * continue
         */
      }
      
      check(hugoOps.releaseLockHandles(ndb,
                                       lockHandles) == 0,
            hugoOps);
      
      lockHandles.clear();
    }
    
    switch(rand() % 3)
    {
    case 0:
      check(hugoOps.execute_Commit(ndb) == 0,
            hugoOps);
      break;
    case 1:
      check(hugoOps.execute_Rollback(ndb) == 0,
            hugoOps);
      break;
    default:
      /* Do nothing, just close */
      break;
    }
    
    check(hugoOps.closeTransaction(ndb) == 0,
          hugoOps);

  }

  g_err << endl;

  return NDBT_OK;
}

template class Vector<NdbRecAttr*>;

int
runBug54986(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const NdbDictionary::Table * pTab = ctx->getTab();
  NdbDictionary::Table copy = *pTab;
  int result = NDBT_OK;

  BaseString name;
  name.assfmt("%s_COPY", copy.getName());
  copy.setName(name.c_str());
  pDict->createTable(copy);
  const NdbDictionary::Table * copyTab = pDict->getTable(copy.getName());

  HugoTransactions hugoTrans(*pTab);
  hugoTrans.loadTable(pNdb, 20);
  hugoTrans.clearTable(pNdb);

  const Uint32 rows = 5000;

  HugoTransactions hugoTransCopy(*copyTab);
  hugoTransCopy.loadTable(pNdb, rows);

  ndbout << "Waiting for 3 LCPs" << endl;
  {
    restarter.getNumDbNodes(); // connect
    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
    NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);
    for (Uint32 i = 0; i<3; i++)
    {
      int dump[] = { DumpStateOrd::DihStartLcpImmediately };

      struct ndb_logevent event;

      restarter.dumpStateAllNodes(dump, 1);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointStarted);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointCompleted);

      ndbout << "LCP" << i << endl;
    }
    ndb_mgm_destroy_logevent_handle(&handle);
  }

  for (int i = 0; i<5; i++)
  {
    ndbout  << "loop: " << i << endl;
    int val1 = DumpStateOrd::DihMaxTimeBetweenLCP;
    int val2 = DumpStateOrd::DihStartLcpImmediately; // Force start

    ndbout << " ... dumpState set 'MaxTimeBetweenLCP'" << endl;
    CHK1(restarter.dumpStateAllNodes(&val1, 1) == 0);

    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    ndbout << " ... dumpState set 'RestartOnErrorInsert = NoStart'" << endl;
    CHK1(restarter.dumpStateAllNodes(val, 2) == 0);

    ndbout << " ... insert error 932" << endl;
    CHK1(restarter.insertErrorInAllNodes(932) ==0); // prevent arbit shutdown

    HugoTransactions hugoTrans(*pTab);
    ndbout << " ... loadTable" << endl;
    CHK1(hugoTrans.loadTable(pNdb, 20) == 0);

    ndbout << " ... dumpState set 'StartLcpImmediately'" << endl;
    CHK1(restarter.dumpStateAllNodes(&val2, 1) == 0);

    ndbout << " ... sleep for 15 sec" << endl;
    NdbSleep_SecSleep(15);
    ndbout << " ... clearTable" << endl;
    CHK1(hugoTrans.clearTable(pNdb) == 0);

    ndbout << " ... Hugo txn" << endl;
    CHK1(hugoTransCopy.pkReadRecords(pNdb, rows) == 0);

    HugoOperations hugoOps(*pTab);
    CHK1(hugoOps.startTransaction(pNdb) == 0);
    CHK1(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHK1(hugoOps.execute_NoCommit(pNdb) == 0);

    ndbout << " ... insert error 5056 (Crash on LCP_COMPLETE_REP)" << endl;
    CHK1(restarter.insertErrorInAllNodes(5056) == 0);

    ndbout << " ... dumpState set 'StartLcpImmediately'" << endl;
    CHK1(restarter.dumpStateAllNodes(&val2, 1) == 0);

    ndbout << " ... waitClusterNoStart" << endl;
    CHK1(restarter.waitClusterNoStart() == 0);
    int vall = 11009;
    CHK1(restarter.dumpStateAllNodes(&vall, 1) == 0);
    CHK1(restarter.startAll() == 0);
    CHK1(restarter.waitClusterStarted() == 0);
    CHK1(pNdb->waitUntilReady() == 0);
    CHK1(hugoOps.closeTransaction(pNdb) == 0);
  }

  pDict->dropTable(copy.getName());

  // remove 25-page pgman
  restarter.restartAll(false, true, true);
  restarter.waitClusterNoStart();
  restarter.startAll();
  restarter.waitClusterStarted();
  pNdb->waitUntilReady();
  return result;
}

int
runBug54944(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table * pTab = ctx->getTab();
  NdbRestarter res;
  int databuffer = ctx->getProperty("DATABUFFER");

  for (Uint32 i = 0; i<5; i++)
  {
    Uint32 rows = 5000 + i * 2000;
    HugoOperations hugoOps(*pTab);
    hugoOps.startTransaction(pNdb);

    for (Uint32 r = 0; r < rows; r++)
    {
      for (Uint32 b = 0; b<100; b++, r++)
      {
        hugoOps.pkInsertRecord(pNdb, r);
      }
      hugoOps.execute_NoCommit(pNdb);
    }

    if (!databuffer)
      res.insertErrorInAllNodes(8087);
    else
      res.insertErrorInAllNodes(8096);

    HugoTransactions hugoTrans(*pTab);
    hugoTrans.loadTableStartFrom(pNdb, 50000, 100);

    hugoOps.execute_Rollback(pNdb);
    hugoTrans.clearTable(pNdb);

    res.insertErrorInAllNodes(0);
  }
  return NDBT_OK;
}

int
runBug59496_scan(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table * pTab = ctx->getTab();
  NdbRestarter res;
  int rowcount = ctx->getProperty("CHECK_ROWCOUNT", Uint32(0));
  int records = ctx->getNumRecords();
  if (rowcount == 0)
    records = 0;

  HugoTransactions hugoTrans(*pTab);
  while (!ctx->isTestStopped())
  {
    if (hugoTrans.scanReadRecords(pNdb,
                                  records, 0, 0,
                                  NdbOperation::LM_CommittedRead,
                                  (int)NdbScanOperation::SF_TupScan) != NDBT_OK)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
runBug59496_case1(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();

  HugoOperations hugoOps(*ctx->getTab());
  for (int i = 0; i < loops; i++)
  {
    hugoOps.startTransaction(pNdb);
    hugoOps.pkInsertRecord(pNdb, 0, records, 0);
    hugoOps.execute_NoCommit(pNdb);
    hugoOps.pkUpdateRecord(pNdb, 0, records, rand());
    hugoOps.execute_NoCommit(pNdb);
    hugoOps.pkUpdateRecord(pNdb, 0, records, rand());
    hugoOps.execute_NoCommit(pNdb);
    res.insertErrorInAllNodes(8089);
    hugoOps.execute_Commit(pNdb);
    res.insertErrorInAllNodes(0);
    hugoOps.closeTransaction(pNdb);
    hugoOps.clearTable(pNdb);
  }
  ctx->stopTest();
  return NDBT_OK;
}

int
runBug59496_case2(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();

  HugoOperations hugoOps(*ctx->getTab());
  for (int i = 0; i < loops; i++)
  {
    hugoOps.startTransaction(pNdb);
    hugoOps.pkDeleteRecord(pNdb, 0, records);
    hugoOps.execute_NoCommit(pNdb);
    hugoOps.pkInsertRecord(pNdb, 0, records, 0);
    hugoOps.execute_NoCommit(pNdb);

    res.insertErrorInAllNodes(8089);
    hugoOps.execute_Rollback(pNdb);
    res.insertErrorInAllNodes(0);

    hugoOps.closeTransaction(pNdb);
  }
  ctx->stopTest();
  return NDBT_OK;
}

#define CHK_RET_FAILED(x) if (!(x)) { ndbout_c("Failed on line: %u", __LINE__); return NDBT_FAILED; }

int
runTest899(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  const int rows = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();
  const int batch = ctx->getProperty("Batch", Uint32(50));
  const int until_stopped = ctx->getProperty("UntilStopped");

  const NdbRecord * pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 * pRow = new Uint8[len];

  int count_ok = 0;
  int count_failed = 0;
  int count_899 = 0;
  for (int i = 0; i < loops || (until_stopped && !ctx->isTestStopped()); i++)
  {
    ndbout_c("loop: %d",i);
    int result = 0;
    for (int rowNo = 0; rowNo < rows;)
    {
      NdbTransaction* pTrans = pNdb->startTransaction();
      CHK_RET_FAILED(pTrans != 0);

      for (int b = 0; rowNo < rows && b < batch; rowNo++, b++)
      {
        bzero(pRow, len);

        HugoCalculator calc(* pTab);

        NdbOperation::OperationOptions opts;
        bzero(&opts, sizeof(opts));

        const NdbOperation* pOp = 0;
        switch(i % 2){
        case 0:
          calc.setValues(pRow, pRowRecord, rowNo, rand());
          pOp = pTrans->writeTuple(pRowRecord, (char*)pRow,
                                   pRowRecord, (char*)pRow,
                                   0,
                                   &opts,
                                   sizeof(opts));
          result = pTrans->execute(NoCommit);
          break;
        case 1:
          calc.setValues(pRow, pRowRecord, rowNo, rand());
          pOp = pTrans->deleteTuple(pRowRecord, (char*)pRow,
                                    pRowRecord, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          result = pTrans->execute(NoCommit, AO_IgnoreError);
          break;
        }

        CHK_RET_FAILED(pOp != 0);

        if (result != 0)
        {
          goto found_error;
        }
      }
      result = pTrans->execute(Commit);

      if (result != 0)
      {
    found_error:
        count_failed++;
        NdbError err = pTrans->getNdbError();
        if (! (err.status == NdbError::TemporaryError ||
               err.classification == NdbError::NoDataFound ||
               err.classification == NdbError::ConstraintViolation))
        {
          ndbout << err << endl;
        }
        CHK_RET_FAILED(err.status == NdbError::TemporaryError ||
                       err.classification == NdbError::NoDataFound ||
                       err.classification == NdbError::ConstraintViolation);
        if (err.code == 899)
        {
          count_899++;
          ndbout << err << endl;
        }
      }
      else
      {
        count_ok++;
      }
      pTrans->close();
    }
  }

  ndbout_c("count_ok: %d count_failed: %d (899: %d)",
           count_ok, count_failed, count_899);
  delete [] pRow;

  return count_899 == 0 ? NDBT_OK : NDBT_FAILED;
}

int
runInit899(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  restarter.dumpStateAllNodes(&val, 1);

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table * pTab2 = pNdb->getDictionary()->
    getTable(pTab->getName());

  int tableId = pTab2->getObjectId();
  int val2[] = { DumpStateOrd::BackupErrorInsert, 10042, tableId };

  for (int i = 0; i < restarter.getNumDbNodes(); i++)
  {
    if (i & 1)
    {
      int nodeId = restarter.getDbNodeId(i);
      ndbout_c("Setting slow LCP of table %d on node %d",
               tableId, nodeId);
      restarter.dumpStateOneNode(nodeId, val2, 3);
    }
  }

  return NDBT_OK;
}

int
runEnd899(NDBT_Context* ctx, NDBT_Step* step)
{
  // reset LCP speed
  NdbRestarter restarter;
  int val[] = { DumpStateOrd::DihMinTimeBetweenLCP, 0 };
  restarter.dumpStateAllNodes(val, 2);

  restarter.insertErrorInAllNodes(0);
  return NDBT_OK;
}


int initSubscription(NDBT_Context* ctx, NDBT_Step* step){
  /* Subscribe to events on the table, and put access
   * to the subscription somewhere handy
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table& tab = *ctx->getTab();
  bool merge_events = false;
  bool report = false;

  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << "Dictionary not found "
	  << pNdb->getNdbError().code << " "
	  << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  myDict->dropEvent(eventName);

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(tab.getName());
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL);
  for(int a = 0; a < tab.getNoOfColumns(); a++){
    myEvent.addEventColumn(a);
  }
  myEvent.mergeEvents(merge_events);

  if (report)
    myEvent.setReport(NdbDictionary::Event::ER_SUBSCRIBE);

  int res = myDict->createEvent(myEvent); // Add event to database

  if (res == 0)
    myEvent.print();
  else if (myDict->getNdbError().classification ==
	   NdbError::SchemaObjectExists)
  {
    g_info << "Event creation failed event exists\n";
    res = myDict->dropEvent(eventName);
    if (res) {
      g_err << "Failed to drop event: "
	    << myDict->getNdbError().code << " : "
	    << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
    // try again
    res = myDict->createEvent(myEvent); // Add event to database
    if (res) {
      g_err << "Failed to create event (1): "
	    << myDict->getNdbError().code << " : "
	    << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    g_err << "Failed to create event (2): "
	  << myDict->getNdbError().code << " : "
	  << myDict->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int removeSubscription(NDBT_Context* ctx, NDBT_Step* step){
  /* Remove subscription created above */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table& tab = *ctx->getTab();

  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << "Dictionary not found "
	  << pNdb->getNdbError().code << " "
	  << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  myDict->dropEvent(eventName);

  return NDBT_OK;
}

int runVerifyRowCount(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* ndb = GETNDB(step);

  /* Check that number of results returned by a normal scan
   * and per-fragment rowcount sum are equal
   */
  Uint32 rowCountSum = 0;
  Uint32 rowScanCount = 0;

  int result = NDBT_OK;
  do
  {
    NdbTransaction* trans = ndb->startTransaction();
    CHECK(trans != NULL);

    NdbScanOperation* scan = trans->getNdbScanOperation(ctx->getTab());
    CHECK(scan != NULL);

    CHECK(scan->readTuples(NdbScanOperation::LM_CommittedRead) == 0);

    NdbInterpretedCode code;

    CHECK(code.interpret_exit_last_row() == 0);
    CHECK(code.finalise() == 0);

    NdbRecAttr* rowCountRA = scan->getValue(NdbDictionary::Column::ROW_COUNT);
    CHECK(rowCountRA != NULL);
    CHECK(scan->setInterpretedCode(&code) == 0);

    CHECK(trans->execute(NoCommit) == 0);

    while (scan->nextResult() == 0)
      rowCountSum+= rowCountRA->u_32_value();

    trans->close();

    trans = ndb->startTransaction();
    CHECK(trans != NULL);

    scan = trans->getNdbScanOperation(ctx->getTab());
    CHECK(scan != NULL);

    CHECK(scan->readTuples(NdbScanOperation::LM_CommittedRead) == 0);

    rowCountRA = scan->getValue(NdbDictionary::Column::ROW_COUNT);
    CHECK(rowCountRA != NULL);

    CHECK(trans->execute(NoCommit) == 0);

    while (scan->nextResult() == 0)
      rowScanCount++;

    trans->close();
  }
  while(0);

  if (result == NDBT_OK)
  {
    ndbout_c("Sum of fragment row counts : %u  Number rows scanned : %u",
             rowCountSum,
             rowScanCount);

    if (rowCountSum != rowScanCount)
    {
      ndbout_c("MISMATCH");
      result = NDBT_FAILED;
    }
  }

  return result;
}

enum ApiEventType { Insert, Update, Delete };

template class Vector<ApiEventType>;

struct EventInfo
{
  ApiEventType type;
  int id;
  Uint64 gci;
};
template class Vector<EventInfo>;

int collectEvents(Ndb* ndb,
                  HugoCalculator& calc,
                  const NdbDictionary::Table& tab,
                  Vector<EventInfo>& receivedEvents,
                  int idCol,
                  int updateCol,
                  Vector<NdbRecAttr*>* beforeAttrs,
                  Vector<NdbRecAttr*>* afterAttrs)
{
  int MaxTimeouts = 5;
  int MaxEmptyPollsAfterData = 10;
  bool some_event_data_received = false;
  while (true)
  {
    NdbEventOperation* pOp = NULL;

    int res = ndb->pollEvents(1000);
    if (res <= 0)
    {
      if (--MaxTimeouts == 0)
        break;
    }
    else
    {
      assert(res == 1);
      pOp = ndb->nextEvent();
      if (!pOp)
      {
        /* pollEvents returning 1 and nextEvent returning 0 means
         * empty epochs found in the event queue. After we receive
         * some event data, we wait some empty epoch poll rounds
         * to make sure that no more event data arrives.
         */
        if (some_event_data_received && --MaxEmptyPollsAfterData == 0)
          break;
      }
    }

    {
      while (pOp)
      {
        bool isDelete = (pOp->getEventType() == NdbDictionary::Event::TE_DELETE);
        Vector<NdbRecAttr*>* whichVersion =
          isDelete?
          beforeAttrs :
          afterAttrs;
        int id = (*whichVersion)[idCol]->u_32_value();
        Uint64 gci = pOp->getGCI();
        Uint32 anyValue = pOp->getAnyValue();
        Uint32 scenario = ((anyValue >> 24) & 0xff) -1;
        Uint32 optype = ((anyValue >> 16) & 0xff);
        Uint32 recNum = (anyValue & 0xffff);

        g_err << "# " << receivedEvents.size()
              << " GCI : " << (gci >> 32)
              << "/"
              << (gci & 0xffffffff)
              << " id : "
              << id
              << " scenario : " << scenario
              << " optype : " << optype
              << " record : " << recNum
              << "  ";

        /* Check event has self-consistent data */
        int updatesValue = (*whichVersion)[updateCol]->u_32_value();

        if ((*whichVersion)[updateCol]->isNULL() ||
            (*whichVersion)[idCol]->isNULL())
        {
          g_err << "Null update/id cols : REFRESH of !EXISTS  ";
        }

        g_err << "(Updates val = " << updatesValue << ")";

        for (int i=0; i < (int) whichVersion->size(); i++)
        {
          /* Check PK columns and also other columns for non-delete */
          if (!isDelete ||
              tab.getColumn(i)->getPrimaryKey())
          {
            NdbRecAttr* ra = (*whichVersion)[i];
            if (calc.verifyRecAttr(recNum, updatesValue, ra) != 0)
            {
              g_err << "Verify failed on recNum : " << recNum << " with updates value "
                    << updatesValue << " for column " << ra->getColumn()->getAttrId()
                    << endl;
              return NDBT_FAILED;
            }
          }
        }

        EventInfo ei;

        switch (pOp->getEventType())
        {
        case NdbDictionary::Event::TE_INSERT:
          g_err << " Insert event" << endl;
          ei.type = Insert;
          break;
        case NdbDictionary::Event::TE_DELETE:
          ei.type = Delete;
          g_err << " Delete event" << endl;
          break;
        case NdbDictionary::Event::TE_UPDATE:
          ei.type = Update;
          g_err << " Update event" << endl;
          break;
        default:
          g_err << " Event type : " << pOp->getEventType() << endl;
          abort();
          break;
        }

        ei.id = recNum;
        ei.gci = gci;

        receivedEvents.push_back(ei);
        some_event_data_received = true;
        pOp = ndb->nextEvent();
      }
    }
  }
  return NDBT_OK;
}

int verifyEvents(const Vector<EventInfo>& receivedEvents,
                 const Vector<ApiEventType>& expectedEvents,
                 int records)
{
  /* Now verify received events against expected
   * This is messy as events occurring in the same epoch are unordered
   * except via id, so we use id-duplicates to determine which event
   * sequence we're looking at.
   */
  g_err << "Received total of " << receivedEvents.size() << " events" << endl;
  Vector<Uint32> keys;
  Vector<Uint64> gcis;
  Uint32 z = 0;
  Uint64 z2 = 0;
  keys.fill(records, z);
  gcis.fill(records, z2);
  Uint64 currGci = 0;

  for (Uint32 e=0; e < receivedEvents.size(); e++)
  {
    EventInfo ei = receivedEvents[e];

    if (ei.gci != currGci)
    {
      if (ei.gci < currGci)
        abort();

      /* Epoch boundary */
      /* At this point, all id counts must be equal */
      for (int i=0; i < records; i++)
      {
        if (keys[i] != keys[0])
        {
          g_err << "Count for id " << i
                << " is " << keys[i]
                << " but should be " << keys[0] << endl;
          return NDBT_OK;
        }
      }

      currGci = ei.gci;
    }

    Uint32 eventIndex = keys[ei.id];
    keys[ei.id]++;

    ApiEventType et = expectedEvents[eventIndex];

    if (ei.type != et)
    {
      g_err << "Expected event of type " << et
            << " but found " << ei.type
            << " at expectedEvent " << eventIndex
            << " and event num " << e << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runRefreshTuple(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  Ndb* ndb = GETNDB(step);

  /* Now attempt to create EventOperation */
  NdbEventOperation* pOp;
  const NdbDictionary::Table& tab = *ctx->getTab();

  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());

  pOp = ndb->createEventOperation(eventName);
  if (pOp == NULL)
  {
    g_err << "Failed to create event operation\n";
    return NDBT_FAILED;
  }

  HugoCalculator calc(tab);
  Vector<NdbRecAttr*> eventAfterRecAttr;
  Vector<NdbRecAttr*> eventBeforeRecAttr;
  int updateCol = -1;
  int idCol = -1;

  /* Now request all attributes */
  for (int a = 0; a < tab.getNoOfColumns(); a++)
  {
    eventAfterRecAttr.push_back(pOp->getValue(tab.getColumn(a)->getName()));
    eventBeforeRecAttr.push_back(pOp->getPreValue(tab.getColumn(a)->getName()));
    if (calc.isIdCol(a))
      idCol = a;
    if (calc.isUpdateCol(a))
      updateCol = a;
  }

  /* Now execute the event */
  if (pOp->execute())
  {
    g_err << "Event operation execution failed : " << pOp->getNdbError() << endl;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*ctx->getTab());
  int scenario = 0;

  Vector<ApiEventType> expectedEvents;

  for (scenario = 0; scenario < 2; scenario++)
  {
    g_err << "Scenario = " << scenario
          << " ( Refresh "
          << ((scenario == 0)? "before":"after")
          << " operations )" << endl;
    int optype = 0;
    bool done = false;
    int expectedError = 0;
    do
    {
      check(hugoOps.startTransaction(ndb) == 0, hugoOps);

      if (scenario == 0)
      {
        g_err << "Refresh before operations" << endl;
        int anyValue =
          ((1) << 8) |
          optype;
        check(hugoOps.pkRefreshRecord(ndb, 0, records, anyValue) == 0, hugoOps);
      }

      switch(optype)
      {
      case 0:
      {
        /* Refresh with no data present */
        g_err << "  Do nothing" << endl;
        expectedError = 0; /* Single refresh should always be fine */
        expectedEvents.push_back(Delete);
        break;
      }
      case 1:
      {
        /* [Refresh] Insert [Refresh] */
        g_err << "  Insert" << endl;
        check(hugoOps.pkInsertRecord(ndb, 0, records, 1) == 0, hugoOps);
        if (scenario == 0)
        {
          /* Tuple already existed error when we insert after refresh */
          expectedError = 630;
          expectedEvents.push_back(Delete);
        }
        else
        {
          expectedError = 0;
          expectedEvents.push_back(Insert);
        }
        /* Tuple already existed error when we insert after refresh */
        break;
      }
      case 2:
      {
        /* Refresh */
        g_err << "  Refresh" << endl;
        if (scenario == 0)
        {
          expectedEvents.push_back(Delete);
        }
        else
        {
          expectedEvents.push_back(Insert);
        }
        expectedError = 0;
        break;
      }
      case 3:
      {
        /* [Refresh] Update [Refresh] */
        g_err << "  Update" << endl;
        check(hugoOps.pkUpdateRecord(ndb, 0, records, 3) == 0, hugoOps);
        if (scenario == 0)
        {
          expectedError = 920;
          expectedEvents.push_back(Delete);
        }
        else
        {
          expectedError = 0;
          expectedEvents.push_back(Insert);
        }
        break;
      }
      case 4:
      {
        /* [Refresh] Delete [Refresh] */
        g_err << "  [Refresh] Delete [Refresh]" << endl;
        if (scenario == 0)
        {
          expectedError = 920;
          expectedEvents.push_back(Delete);
        }
        else
        {
          expectedError = 0;
          expectedEvents.push_back(Delete);
        }
        check(hugoOps.pkDeleteRecord(ndb, 0, records) == 0, hugoOps);
        break;
      }
      case 5:
      {
        g_err << "  Refresh" << endl;
        expectedError = 0;
        expectedEvents.push_back(Delete);
        /* Refresh with no data present */
        break;
      }
      case 6:
      {
        g_err << "  Double refresh" << endl;
        int anyValue =
          ((2) << 8) |
          optype;
        check(hugoOps.pkRefreshRecord(ndb, 0, records, anyValue) == 0, hugoOps);
        expectedError = 920; /* Row operation defined after refreshTuple() */
        expectedEvents.push_back(Delete);
      }
      default:
        done = true;
        break;
      }

      if (scenario == 1)
      {
        g_err << "Refresh after operations" << endl;
        int anyValue =
          ((4) << 8) |
          optype;
        check(hugoOps.pkRefreshRecord(ndb, 0, records, anyValue) == 0, hugoOps);
      }

      int rc = hugoOps.execute_Commit(ndb, AO_IgnoreError);
      check(rc == expectedError, hugoOps);

      check(hugoOps.closeTransaction(ndb) == 0, hugoOps);

      optype++;


      /* Now check fragment counts vs findable row counts */
      if (runVerifyRowCount(ctx, step) != NDBT_OK)
        return NDBT_FAILED;

    } while (!done);
  } // for scenario...

  /* Now check fragment counts vs findable row counts */
  if (runVerifyRowCount(ctx, step) != NDBT_OK)
    return NDBT_FAILED;

  /* Now let's dump and check the events */
  g_err << "Expecting the following sequence..." << endl;
  for (Uint32 i=0; i < expectedEvents.size(); i++)
  {
    g_err << i << ".  ";
    switch(expectedEvents[i])
    {
    case Insert:
      g_err << "Insert" << endl;
      break;
    case Update:
      g_err << "Update" << endl;
      break;
    case Delete:
      g_err << "Delete" << endl;
      break;
    default:
      abort();
    }
  }

  Vector<EventInfo> receivedEvents;

  int rc = collectEvents(ndb, calc, tab, receivedEvents, idCol, updateCol,
                         &eventBeforeRecAttr,
                         &eventAfterRecAttr);
  if (rc == NDBT_OK)
  {
    rc = verifyEvents(receivedEvents,
                      expectedEvents,
                      records);
  }

  if (ndb->dropEventOperation(pOp) != 0)
  {
    g_err << "Drop Event Operation failed : " << ndb->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return rc;
};

// Regression test for bug #14208924
static int
runLeakApiConnectObjects(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  /**
   * This error insert inc ombination with bug #14208924 will 
   * cause TC to leak ApiConnectRecord objects.
   */
  restarter.insertErrorInAllNodes(8094);

  Ndb* const ndb = GETNDB(step);
  Uint32 maxTrans = 0;
  NdbConfig conf;
  require(conf.getProperty(conf.getMasterNodeId(),
                                 NODE_TYPE_DB,
                                 CFG_DB_NO_TRANSACTIONS,
                                 &maxTrans));
  require(maxTrans > 0);

  HugoOperations hugoOps(*ctx->getTab());
  // One ApiConnectRecord object is leaked for each iteration.
  for (uint i = 0; i < maxTrans+1; i++)
  {
    require(hugoOps.startTransaction(ndb) == 0);
    require(hugoOps.pkInsertRecord(ndb, i) == 0);
    NdbTransaction* const trans = hugoOps.getTransaction();
    /**
     * The error insert causes trans->execute(Commit) to fail with error code
     * 286 even if the bug is fixed. Therefore, we ignore this error code.
     */
    if (trans->execute(Commit) != 0 && 
        trans->getNdbError().code != 286)
    {
      g_err << "trans->execute() gave unexpected error : " 
            << trans->getNdbError() << endl;
      restarter.insertErrorInAllNodes(0);
      return NDBT_FAILED;
    }
    require(hugoOps.closeTransaction(ndb) == 0);
  }
  restarter.insertErrorInAllNodes(0);

  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(ndb) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

enum PreRefreshOps
{
  PR_NONE,
  PR_INSERT,
  PR_INSERTDELETE,
  PR_DELETE
};

struct RefreshScenario
{
  const char*   name;
  bool          preExist;
  PreRefreshOps preRefreshOps;
};

static RefreshScenario refreshTests[] = {
  { "No row, No pre-ops",        false, PR_NONE         },
  { "No row, Insert pre-op",     false, PR_INSERT       },
  { "No row, Insert-Del pre-op", false, PR_INSERTDELETE },
  { "Row exists, No pre-ops",    true,  PR_NONE         },
  { "Row exists, Delete pre-op", true,  PR_DELETE       }
};

enum OpTypes
{
  OP_READ_C,
  OP_READ_S,
  OP_READ_E,
  OP_INSERT,
  OP_UPDATE,
  OP_WRITE,
  OP_DELETE,
  OP_LAST
};

const char* opTypeNames[] =
{
  "READ_C",
  "READ_S",
  "READ_E",
  "INSERT",
  "UPDATE",
  "WRITE",
  "DELETE"
};


int
runRefreshLocking(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Check that refresh in various situations has the
   * locks we expect it to
   * Scenario combinations :
   *   Now row pre-existing | Row pre-existing
   *   Trans1 : Refresh | Insert-Refresh | Insert-Delete-Refresh
   *            Delete-Refresh
   *   Trans2 : Read [Committed|Shared|Exclusive] | Insert | Update
   *            Write | Delete
   *
   * Expectations : Read committed  always non-blocking
   *                Read committed sees pre-existing row
   *                All other trans2 operations deadlock
   */

  Ndb* ndb = GETNDB(step);
  Uint32 numScenarios = sizeof(refreshTests) / sizeof(refreshTests[0]);
  HugoTransactions hugoTrans(*ctx->getTab());

  for (Uint32 s = 0; s < numScenarios; s++)
  {
    RefreshScenario& scenario = refreshTests[s];

    if (scenario.preExist)
    {
      /* Create pre-existing tuple */
      if (hugoTrans.loadTable(ndb, 1) != 0)
      {
        g_err << "Pre-exist failed : " << hugoTrans.getNdbError() << endl;
        return NDBT_FAILED;
      }
    }

    if (hugoTrans.startTransaction(ndb) != 0)
    {
      g_err << "Start trans failed : " << hugoTrans.getNdbError() << endl;
      return NDBT_FAILED;
    }

    g_err << "Scenario : " << scenario.name << endl;

    /* Do pre-refresh ops */
    switch (scenario.preRefreshOps)
    {
    case PR_NONE:
      break;
    case PR_INSERT:
    case PR_INSERTDELETE:
      if (hugoTrans.pkInsertRecord(ndb, 0) != 0)
      {
        g_err << "Pre insert failed : " << hugoTrans.getNdbError() << endl;
        return NDBT_FAILED;
      }

      if (scenario.preRefreshOps == PR_INSERT)
        break;
    case PR_DELETE:
      if (hugoTrans.pkDeleteRecord(ndb, 0) != 0)
      {
        g_err << "Pre delete failed : " << hugoTrans.getNdbError() << endl;
        return NDBT_FAILED;
      }
      break;
    }

    /* Then refresh */
    if (hugoTrans.pkRefreshRecord(ndb, 0) != 0)
    {
      g_err << "Refresh failed : " << hugoTrans.getNdbError() << endl;
      return NDBT_FAILED;
    }

    /* Now execute */
    if (hugoTrans.execute_NoCommit(ndb) != 0)
    {
      g_err << "Execute failed : " << hugoTrans.getNdbError() << endl;
      return NDBT_FAILED;
    }

    {
      /* Now try ops from another transaction */
      HugoOperations hugoOps(*ctx->getTab());
      Uint32 ot = OP_READ_C;

      while (ot < OP_LAST)
      {
        if (hugoOps.startTransaction(ndb) != 0)
        {
          g_err << "Start trans2 failed : " << hugoOps.getNdbError() << endl;
          return NDBT_FAILED;
        }

        g_err << "Operation type : " << opTypeNames[ot] << endl;
        int res = 0;
        switch (ot)
        {
        case OP_READ_C:
          res = hugoOps.pkReadRecord(ndb,0,1,NdbOperation::LM_CommittedRead);
          break;
        case OP_READ_S:
          res = hugoOps.pkReadRecord(ndb,0,1,NdbOperation::LM_Read);
          break;
        case OP_READ_E:
          res = hugoOps.pkReadRecord(ndb,0,1,NdbOperation::LM_Exclusive);
          break;
        case OP_INSERT:
          res = hugoOps.pkInsertRecord(ndb, 0);
          break;
        case OP_UPDATE:
          res = hugoOps.pkUpdateRecord(ndb, 0);
          break;
        case OP_WRITE:
          res = hugoOps.pkWriteRecord(ndb, 0);
          break;
        case OP_DELETE:
          res = hugoOps.pkDeleteRecord(ndb, 0);
          break;
        case OP_LAST:
          abort();
        }

        hugoOps.execute_Commit(ndb);

        if ((ot == OP_READ_C) && (scenario.preExist))
        {
          if (hugoOps.getNdbError().code == 0)
          {
            g_err << "Read committed succeeded" << endl;
          }
          else
          {
            g_err << "UNEXPECTED : Read committed failed. " << hugoOps.getNdbError() << endl;
            return NDBT_FAILED;
          }
        }
        else
        {
          if (hugoOps.getNdbError().code == 0)
          {
            g_err << opTypeNames[ot] << " succeeded, should not have" << endl;
            return NDBT_FAILED;
          }
        }

        hugoOps.closeTransaction(ndb);

        ot = ot + 1;
      }

    }

    /* Close refresh transaction */
    hugoTrans.closeTransaction(ndb);

    if (scenario.preExist)
    {
      /* Cleanup pre-existing before next iteration */
      if (hugoTrans.pkDelRecords(ndb, 0) != 0)
      {
        g_err << "Delete pre existing failed : " << hugoTrans.getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}

int
runBugXXX_init(NDBT_Context* ctx, NDBT_Step* step)
{
  return NDBT_OK;
}

int
runBugXXX_trans(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  while (!ctx->isTestStopped())
  {
    runLoadTable(ctx, step);
    ctx->getPropertyWait("CREATE_INDEX", 1);
    ctx->setProperty("CREATE_INDEX", Uint32(0));
    res.insertErrorInAllNodes(8105); // randomly abort trigger ops with 218
    runClearTable2(ctx, step);
    res.insertErrorInAllNodes(0);
  }

  return NDBT_OK;
}

int
runBugXXX_createIndex(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  const int loops = ctx->getNumLoops();

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  BaseString name;
  name.assfmt("%s_PK_IDX", pTab->getName());
  NdbDictionary::Index pIdx(name.c_str());
  pIdx.setTable(pTab->getName());
  pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c < pTab->getNoOfColumns(); c++)
  {
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey())
    {
      pIdx.addIndexColumn(col->getName());
    }
  }
  pIdx.setStoredIndex(false);

  for (int i = 0; i < loops; i++)
  {
    res.insertErrorInAllNodes(18000);
    ctx->setProperty("CREATE_INDEX", 1);
    pNdb->getDictionary()->createIndex(pIdx);
    pNdb->getDictionary()->dropIndex(name.c_str(), pTab->getName());
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runDeleteNdbInFlight(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  Ndb_cluster_connection & nc = pNdb->get_ndb_cluster_connection();
  const NdbDictionary::Table* tab = ctx->getTab();
  BaseString name;
  name.assfmt("%s", tab->getName());
  int result = NDBT_OK;
  int rows = 1000;

  /**
   * We start by filling the table with 1000 rows.
   * Next we start a transaction that inserts 1000 rows
   * without committing it.
   * Next we delete the Ndb object that sends off a
   * TCRELEASEREQ signal that should ensure the
   * transaction is aborted, we will not receive
   * any info on this since we disconnected.
   *
   * Next we perform 1000 inserts and start off
   * executing those in prepare phase. Next we
   * delete Ndb object that sends off TCRELEASEREQ
   * signal.
   *
   * After receiving TCRELEASECONF we can still
   * receive many more TCKEYCONF's and TRANSID_AI's
   * from the LDM threads and TC threads.
   * This is ok, getting rid of TRANSID_AI's from
   * LDM threads is more or less impossible since
   * these are no longer controlled by TC. Getting
   * rid of TCKEYCONF's is possible, but dangerous,
   * so we put the responsibility on the NDB API to
   * filter out those old signals by looking at the
   * Transaction id.
   *
   * Finally we test a scan that gets closed down
   * in the middle of execution by a TCRELEASEREQ.
   *
   * This test also massages the code for API node
   * fail handling that probably wasn't 100% covered
   * before this test was written.
   *
   * Given that ongoing transactions was stopped by
   * deleting Ndb object we have to set Transaction
   * to NULL in HugOperations to avoid it closing
   * the transaction.
   */
  HugoOperations *h_op = new HugoOperations(*tab);
  h_op->startTransaction(pNdb);
  h_op->pkInsertRecord(pNdb, 0, rows);
  h_op->execute_Commit(pNdb);
  h_op->closeTransaction(pNdb);
  delete h_op;

  ndbout_c("Test1");
  Ndb *newNdb1 = new Ndb(&nc, "TEST_DB");
  newNdb1->init(1024);
  const NdbDictionary::Table *tab1 =
    newNdb1->getDictionary()->getTable(name.c_str());
  HugoOperations *h_op1 = new HugoOperations(*tab1);
  h_op1->startTransaction(newNdb1);
  h_op1->pkInsertRecord(newNdb1, rows, rows);
  h_op1->execute_NoCommit(newNdb1);
  delete newNdb1;

  ndbout_c("Test2");
  Ndb *newNdb2 = new Ndb(&nc, "TEST_DB");
  newNdb2->init(1024);
  const NdbDictionary::Table *tab2 =
    newNdb2->getDictionary()->getTable(name.c_str());
  HugoOperations *h_op2 = new HugoOperations(*tab2);
  h_op2->startTransaction(newNdb2);
  h_op2->pkInsertRecord(newNdb2, rows, 2 * rows);
  h_op2->execute_async(newNdb2, NdbTransaction::Commit);
  delete newNdb2;

  ndbout_c("Test3");
  Ndb *newNdb3 = new Ndb(&nc, "TEST_DB");
  newNdb3->init(1024);
  const NdbDictionary::Table *tab3 =
    newNdb3->getDictionary()->getTable(name.c_str());
  HugoOperations *h_op3 = new HugoOperations(*tab3);
  h_op3->startTransaction(newNdb3);
  h_op3->scanReadRecords(newNdb3, NdbScanOperation::LM_Exclusive, rows);
  h_op3->execute_NoCommit(newNdb1);
  delete newNdb3;

  h_op1->setTransaction(NULL, true);
  h_op2->setTransaction(NULL, true);
  h_op3->setTransaction(NULL, true);
  delete h_op1;
  delete h_op2;
  delete h_op3;

  return result;
}

int
runBug16834333(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int records = ctx->getNumRecords();
  NdbRestarter restarter;
  int result = NDBT_OK;

  do
  {
    /*
     * Drop the pre-created table before initial restart to avoid invalid
     * dict cache.  One symptom would be running the test twice and getting
     * abort() in final dict cache release due to non-existent version.
     * Also use a copy of the pre-created table struct to avoid accessing
     * invalid memory.
     */
    const NdbDictionary::Table tab(* ctx->getTab());
    CHK2(pDic->dropTable(tab.getName()) == 0, pDic->getNdbError());

    ndbout_c("restart initial");
    restarter.restartAll(true, /* initial */
                         true, /* nostart */
                         true  /* abort */ );

    ndbout_c("wait nostart");
    restarter.waitClusterNoStart();
    ndbout_c("startAll");
    restarter.startAll();
    ndbout_c("wait started");
    restarter.waitClusterStarted();
    CHK_NDB_READY(pNdb);

    ndbout_c("create tab");
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    const NdbDictionary::Table* pTab = pDic->getTable(tab.getName());
    CHK2(pTab != 0, pDic->getNdbError());

    ndbout_c("load table");
    HugoTransactions trans(* pTab);
    CHK2(trans.loadTable(pNdb, records) == 0, trans.getNdbError());

    int codes[] = { 5080, 5081 };
    for (int i = 0, j = 0; i < restarter.getNumDbNodes(); i++, j++)
    {
      int code = codes[j % NDB_ARRAY_SIZE(codes)];
      int nodeId = restarter.getDbNodeId(i);
      ndbout_c("error %d node: %d", code, nodeId);
      restarter.insertErrorInNode(nodeId, code);
    }

    ndbout_c("running big trans");
    HugoOperations ops(* pTab);
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    CHK2(ops.pkReadRecord(0, 16384) == 0, ops.getNdbError());
    if (ops.execute_Commit(pNdb, AO_IgnoreError) != 0)
    {
      // XXX should this occur if AO_IgnoreError ?
      CHK2(ops.getNdbError().code == 1223, ops.getNdbError());
      g_info << ops.getNdbError() << endl;
    }
    ops.closeTransaction(pNdb);
  }
  while (0);

  restarter.insertErrorInAllNodes(0);
  return result;
}

// bug#19031389

int
runAccCommitOrder(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  require(records > 0);
  const int opsteps = ctx->getProperty("OPSTEPS");
  int result = NDBT_OK;

  for (int loop = 0; loop < loops; loop++)
  {
    g_info << "loop " << loop << endl;

    {
      g_info << "load table" << endl;
      HugoTransactions trans(*pTab);
      CHK2(trans.loadTable(pNdb, records) == 0, trans.getNdbError());
    }

    g_info << "start op steps" << endl;
    require(ctx->getProperty("RUNNING", (Uint32)opsteps) == 0);
    ctx->setProperty("RUN", (Uint32)1);

    if (ctx->getPropertyWait("RUNNING", (Uint32)opsteps))
      break;
    g_info << "all op steps running" << endl;

    int mssleep = 10 + ndb_rand() % records;
    if (mssleep > 1000)
      mssleep = 1000;
    NdbSleep_MilliSleep(mssleep);

    g_info << "stop op steps" << endl;
    require(ctx->getProperty("RUNNING", (Uint32)0) == (Uint32)opsteps);
    ctx->setProperty("RUN", (Uint32)0);

    if (ctx->getPropertyWait("RUNNING", (Uint32)0))
      break;
    g_info << "all op steps stopped" << endl;

    {
      g_info << "clear table" << endl;
      UtilTransactions trans(*pTab);
      CHK1(trans.clearTable(pNdb, records) == 0);
    }
  }

  g_info << "stop test" << endl;
  ctx->stopTest();
  return result;
}

int
runAccCommitOrderOps(NDBT_Context* ctx, NDBT_Step* step)
{
  const int stepNo = step->getStepNo();
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  unsigned seed = (unsigned)(NdbHost_GetProcessId() ^ stepNo);
  ndb_srand(seed);

  int loop = 0;
  while (!ctx->isTestStopped())
  {
    if (ctx->getPropertyWait("RUN", (Uint32)1))
      break;
    g_info << "step " << stepNo << ": loop " << loop << endl;

    ctx->incProperty("RUNNING");
    g_info << "step " << stepNo << ": running" << endl;

    int opscount = 0;
    int n = 0; // steps should hit about same records
    while (ctx->getProperty("RUN", (Uint32)0) == (Uint32)1)
    {
      HugoOperations ops(*pTab);
      ops.setQuiet();
      CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());

      const int numreads = 2 + ndb_rand_r(&seed) % 3;
      for (int i = 0; i < numreads; i++)
      {
        NdbOperation::LockMode lm = NdbOperation::LM_Read;
        CHK2(ops.pkReadRecord(pNdb, n, 1, lm) == 0, ops.getNdbError());
        opscount++;
      }
      CHK1(result == NDBT_OK);

      CHK2(ops.pkDeleteRecord(pNdb, n, 1) == 0, ops.getNdbError());

      CHK2(ops.execute_Commit(pNdb) == 0 ||
           ops.getNdbError().code == 626 ||
           (
             ops.getNdbError().status == NdbError::TemporaryError &&
             ops.getNdbError().classification != NdbError::NodeRecoveryError
           ),
           ops.getNdbError());
      ops.closeTransaction(pNdb);
      n = (n + 1) % records;
    }
    CHK1(result == NDBT_OK);
    g_info << "step " << stepNo << ": ops count " << opscount << endl;

    ctx->decProperty("RUNNING");
    g_info << "step " << stepNo << ": stopped" << endl;

    loop++;
  }

  ctx->stopTest();
  return result;
}

/**
 * TESTCASE DeleteNdbWhilePoll: Delete an Ndb object while it(trp_clnt)
 * is in poll q.
 * runInsertOneTuple : inserts one tuple in table.
 * runLockTuple : A thread runs transaction 1 (Txn1) which locks the
 * tuple with exclusive lock and signals another thread running
 * transaction 2 (Txn2).
 * runReadLockedTuple : Txn2 issues an exclusive read of the tuple
 * locked by Txn1 (and waits for lock), and signals the delete thread.
 * deleteNdbWhileWaiting : deletes the ndb object used by Txn2.
 * Tx1 commits and Tx2 commits and provokes consequences from deleted Ndb.
 *
 * The most probable consequence, is :
 * TransporterFacade.cpp:1674: require((clnt->m_poll.m_locked == true)) failed
 */
// Candidate for deleteNdbWhileWaiting().
static Ndb* ndbToDelete = NULL;

int
runInsertOneTuple(NDBT_Context *ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const NdbDictionary::Table *table= ctx->getTab();
  HugoOperations hugoOp1(*table);
  Ndb* pNdb = GETNDB(step);

  CHECK2(hugoOp1.startTransaction(pNdb) == 0);
  CHECK2(hugoOp1.pkInsertRecord(pNdb, 1, 1) == 0);
  CHECK2(hugoOp1.execute_Commit(pNdb) == 0);
  CHECK2(hugoOp1.closeTransaction(pNdb) == 0);

  g_info << "Rec inserted, ndb " << pNdb <<endl <<endl;
  return result;
}

int
runLockTuple(NDBT_Context *ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const NdbDictionary::Table *table= ctx->getTab();
  HugoOperations hugoOp1(*table);
  Ndb* pNdb = GETNDB(step);

  CHECK2(hugoOp1.startTransaction(pNdb) == 0);
  // read the inserted tuple (Txn1).
  CHECK2(hugoOp1.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);

  CHECK2(hugoOp1.execute_NoCommit(pNdb) == 0);

  g_info << "Txn1 readlocked tuple, ndb "<<pNdb << endl;

  // Flag Txn2 to read (which will be blocked due to Txn1's read lock).
  ctx->setProperty("Txn1-LockedTuple", 1);

  // Wait until ndb of Txn2 is deleted by deleteNdbWhileWaiting().
  while(ctx->getProperty("NdbDeleted", (Uint32)0) == 0 &&
	!ctx->isTestStopped()){
    NdbSleep_MilliSleep(20);
  }

  // Now commit Txn1.
  /* Intention is when this commits, Txn1's trp_clnt will relinquish the
   * poll rights it had to trp_clnt to Txn2, which is deleted.
   * However this is not determisnistic. Sometimes, the poll right
   * is owned by Txn2's trp_clnt, causing test to assert-fail in do_poll.
   */
  g_info << "Txn1 commits, ndb " << pNdb << endl;

  if (!ctx->isTestStopped())
    CHECK2(hugoOp1.execute_Commit(pNdb) == 0);
  g_info << "Txn1 commited, ndb " << pNdb << endl;

  if (!ctx->isTestStopped())
    CHECK2(hugoOp1.closeTransaction(pNdb) == 0);
  return result;
}

// Read the tuple locked by Txn1, see runLockTuple().
int
runReadLockedTuple(NDBT_Context *ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const NdbDictionary::Table *table= ctx->getTab();
  HugoOperations hugoOp2(*table);
  Ndb* pNdb = GETNDB(step);

  // Wait until the tuple is locked by Txn1
  while(ctx->getProperty("Txn1-LockedTuple", (Uint32)0) == 0 &&
	!ctx->isTestStopped()){
    NdbSleep_MilliSleep(20);
  }

  CHECK2(hugoOp2.startTransaction(pNdb) == 0);
  // Txn2 reads the locked tuple.
  CHECK2(hugoOp2.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive) == 0);

  // Flag deleteNdbWhileWaiting() to delete my ndb object
  ndbToDelete = pNdb; // candidate for deleteNdbWhileWaiting()
  ctx->setProperty("Txn2-SendCommit", 1);

  // Now commit Txn2
  g_info << "Txn2 commits, ndb " << pNdb
	<< ", Ndb to delete " << ndbToDelete << endl << endl;

  CHECK2(hugoOp2.execute_Commit(pNdb) == 0);

  CHECK2(hugoOp2.closeTransaction(pNdb) == 0);

  ctx->stopTest();
  return result;
}

// Delete ndb of Txn2.
int deleteNdbWhileWaiting(NDBT_Context* ctx, NDBT_Step* step)
{
  // Wait until Txn2 sends the read of the locked tuple.
  while(ctx->getProperty("Txn2-SendCommit", (Uint32)0) == 0 &&
    !ctx->isTestStopped()){
    g_info << "|- Waiting for read" << endl;
    NdbSleep_MilliSleep(20);
  }

  // Delete ndb of Txn2 while it is waiting in the poll queue.
  g_info << "deleteNdbWhileWaiting deletes ndb " << ndbToDelete << endl << endl;
  delete ndbToDelete;

  // Signal Txn1 to commit
  ctx->setProperty("NdbDeleted", 1);
  return NDBT_OK;
}
/******* end TESTCASE DeleteNdbWhilePoll*******/


NDBT_TESTSUITE(testBasic);
TESTCASE("PkInsert", 
	 "Verify that we can insert and delete from this table using PK"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runInsert);
  VERIFIER(runVerifyInsert);
}
TESTCASE("PkRead", 
	   "Verify that we can insert, read and delete from this table using PK"){
  TC_PROPERTY("LockMode", NdbOperation::LM_Read);
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("PkDirtyRead", 
	 "Verify that we can insert, dirty read and delete from this table using PK"){
  TC_PROPERTY("LockMode", NdbOperation::LM_Dirty);
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("PkSimpleRead", 
	 "Verify that we can insert, simple read and delete from this table using PK"){
  TC_PROPERTY("LockMode", NdbOperation::LM_SimpleRead);
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("PkUpdate", 
	   "Verify that we can insert, update and delete from this table using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("PkDelete", 
	 "Verify that we can delete from this table using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkDelete);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateAndRead", 
	 "Verify that we can read and update at the same time"){
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  STEP(runPkRead);
  STEP(runPkRead);
  STEP(runPkUpdate);  
  STEP(runPkUpdate);
  STEP(runPkUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadAndLocker", 
	 "Verify that we can read although there are "\
	 " a number of 1 second locks in the table"){
  INITIALIZER(runLoadTable);
  STEP(runPkReadUntilStopped);
  STEP(runLocker);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadAndLocker2", 
	 "Verify that we can read and update although there are "\
	 " a number of 1 second locks in the table"){
  INITIALIZER(runLoadTable);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runLocker);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadUpdateAndLocker", 
	 "Verify that we can read and update although there are "\
	 " a number of 1 second locks in the table"){
  INITIALIZER(runLoadTable);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runLocker);
  FINALIZER(runClearTable);
}
TESTCASE("ReadWithLocksAndInserts", 
	 "TR457: This test is added to verify that an insert of a records "\
	 "that is already in the database does not delete the record"){  
  INITIALIZER(runLoadTable);
  STEP(runPkReadUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runLocker);
  STEP(runInsertUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkInsertTwice", 
	 "Verify that we can't insert an already inserted record."
	 "Error should be returned" ){
  INITIALIZER(runLoadTable);
  STEP(runInsertTwice);
  FINALIZER(runClearTable);
}
TESTCASE("NoCommitSleep", 
	 "Verify what happens when a NoCommit transaction is aborted by "
	 "NDB because the application is sleeping" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runNoCommitSleep);
  FINALIZER(runClearTable2);
}
TESTCASE("Commit626", 
	 "Verify what happens when a Commit transaction is aborted by "
	 "NDB because the record does no exist" ){
  INITIALIZER(runClearTable2);
  INITIALIZER(runCommit626);
  FINALIZER(runClearTable2);
}
TESTCASE("CommitTry626", 
	 "Verify what happens when a Commit(TryCommit) \n"
	 "transaction is aborted by "
	 "NDB because the record does no exist" ){
  INITIALIZER(runClearTable2);
  INITIALIZER(runCommit_TryCommit626);
  FINALIZER(runClearTable2);
}
TESTCASE("CommitAsMuch626", 
	 "Verify what happens when a Commit(CommitAsMuchAsPossible) \n"
	 "transaction is aborted by\n"
	 "NDB because the record does no exist" ){
  INITIALIZER(runClearTable2);
  INITIALIZER(runCommit_CommitAsMuchAsPossible626);
  FINALIZER(runClearTable2);
}
TESTCASE("NoCommit626", 
	 "Verify what happens when a NoCommit transaction is aborted by "
	 "NDB because the record does no exist" ){
  INITIALIZER(runClearTable2);
  INITIALIZER(runNoCommit626);
  FINALIZER(runClearTable2);
}
TESTCASE("NoCommitRollback626", 
	 "Verify what happens when a NoCommit transaction is aborted by "
	 "NDB because the record does no exist and then we try to rollback\n"
	 "the transaction" ){
  INITIALIZER(runClearTable2);
  INITIALIZER(runNoCommitRollback626);
  FINALIZER(runClearTable2);
}
TESTCASE("Commit630", 
	 "Verify what happens when a Commit transaction is aborted by "
	 "NDB because the record already exist" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCommit630);
  FINALIZER(runClearTable2);
}
TESTCASE("CommitTry630", 
	 "Verify what happens when a Commit(TryCommit) \n"
	 "transaction is aborted by "
	 "NDB because the record already exist" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCommit_TryCommit630);
  FINALIZER(runClearTable2);
}
TESTCASE("CommitAsMuch630", 
	 "Verify what happens when a Commit(CommitAsMuchAsPossible) \n"
	 "transaction is aborted by\n"
	 "NDB because the record already exist" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCommit_CommitAsMuchAsPossible630);
  FINALIZER(runClearTable2);
}
TESTCASE("NoCommit630", 
	 "Verify what happens when a NoCommit transaction is aborted by "
	 "NDB because the record already exist" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runNoCommit630);
  FINALIZER(runClearTable2);
}
TESTCASE("NoCommitRollback630", 
	 "Verify what happens when a NoCommit transaction is aborted by "
	 "NDB because the record already exist and then we try to rollback\n"
	 "the transaction" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runNoCommitRollback630);
  FINALIZER(runClearTable2);
}
TESTCASE("NoCommitAndClose", 
	 "Verify what happens when a NoCommit transaction is closed "
	 "without rolling back the transaction " ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runNoCommitAndClose);
  FINALIZER(runClearTable2);
}
TESTCASE("RollbackDelete", 
	 "Test rollback of a no committed delete"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCheckRollbackDelete);
  FINALIZER(runClearTable2);
}
TESTCASE("RollbackUpdate", 
	 "Test rollback of a no committed update"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCheckRollbackUpdate);
  FINALIZER(runClearTable2);
}
TESTCASE("RollbackDeleteMultiple", 
	 "Test rollback of 10 non committed delete"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCheckRollbackDeleteMultiple);
  FINALIZER(runClearTable2);
}
TESTCASE("ImplicitRollbackDelete", 
	 "Test close transaction after a no commited delete\n"
	 "this would give an implicit rollback of the delete\n"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCheckImplicitRollbackDelete);
  FINALIZER(runClearTable2);
}
TESTCASE("CommitDelete", 
	 "Test close transaction after a no commited delete\n"
	 "this would give an implicit rollback of the delete\n"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runCheckCommitDelete);
  FINALIZER(runClearTable2);
}
TESTCASE("RollbackNothing", 
	 "Test rollback of nothing"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runRollbackNothing);
  FINALIZER(runClearTable2);
}
TESTCASE("MassiveRollback", 
	 "Test rollback of 4096 operations"){
  INITIALIZER(runClearTable2);
  INITIALIZER(runMassiveRollback);
  FINALIZER(runClearTable2);
}
TESTCASE("MassiveRollback2", 
	 "Test rollback of 4096 operations"){
  INITIALIZER(runClearTable2);
  INITIALIZER(runMassiveRollback2);
  FINALIZER(runClearTable2);
}
TESTCASE("MassiveRollback3", 
	 "Test rollback of 4096 operations"){
  INITIALIZER(runClearTable2);
  STEP(runMassiveRollback3);
  STEP(runMassiveRollback3);
  FINALIZER(runClearTable2);
}
TESTCASE("MassiveRollback4", 
	 "Test rollback of 4096 operations"){
  INITIALIZER(runClearTable2);
  STEP(runMassiveRollback4);
  STEP(runMassiveRollback4);
  FINALIZER(runClearTable2);
}
TESTCASE("MassiveTransaction",
         "Test very large insert transaction"){
  INITIALIZER(runLoadTable2);
  FINALIZER(runClearTable2);
}
TESTCASE("TupError", 
	 "Verify what happens when we fill the db" ){
  INITIALIZER(runTupErrors);
}
TESTCASE("InsertError", "" ){
  INITIALIZER(runInsertError);
}
TESTCASE("InsertError2", "" ){
  INITIALIZER(runInsertError2);
}
TESTCASE("Fill", 
	 "Verify what happens when we fill the db" ){
  STEP(runFillTable);
}
TESTCASE("Bug25090", 
	 "Verify what happens when we fill the db" ){
  STEP(runBug25090);
}
TESTCASE("DeleteRead", 
	 "Verify Delete+Read" ){
  INITIALIZER(runLoadTable);
  INITIALIZER(runDeleteRead);
  FINALIZER(runClearTable2);
}
TESTCASE("Bug27756", 
	 "Verify what happens when we fill the db" ){
  STEP(runBug27756);
}
TESTCASE("Bug28073", 
	 "Infinite loop in lock queue" ){
  STEP(runBug28073);
}
TESTCASE("Bug20535", 
	 "Verify what happens when we fill the db" ){
  STEP(runBug20535);
}
TESTCASE("DDInsertFailUpdateBatch",
         "Verify DD insert failure effect on other ops in batch on same PK"){
  STEP(runDDInsertFailUpdateBatch);
}

TESTCASE("Bug34348",
         "Test fragment directory range full in ACC.\n"
         "NOTE: If interrupted, must clear error insert 3002 manually"){
  STEP(runBug34348);
}
TESTCASE("UnlockBatch",
         "Test that batched unlock operations work ok"){
  TC_PROPERTY("Batchsize", 33);
  INITIALIZER(runLoadTable);
  STEP(runUnlocker);
  FINALIZER(runClearTable);
}
TESTCASE("DoubleUnlock",
         "Test that batched unlock operations work ok"){
  TC_PROPERTY("DoubleUnlock", 1);
  INITIALIZER(runLoadTable);
  STEP(runUnlocker);
  FINALIZER(runClearTable);
}
TESTCASE("UnlockUpdateBatch",
         "Test Unlock mixed with Update"){
  TC_PROPERTY("Batchsize", 32);
  INITIALIZER(runLoadTable);
  STEP(runUnlocker);
  STEP(runUnlocker);
  STEP(runLocker);
  STEP(runPkUpdate);
  STEP(runPkUpdate);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("RefreshTuple",
         "Test refreshTuple() operation properties"){
  INITIALIZER(initSubscription);
  INITIALIZER(runRefreshTuple);
  FINALIZER(removeSubscription);
}
TESTCASE("Bug54986", "")
{
  INITIALIZER(runBug54986);
}
TESTCASE("Bug54944", "")
{
  TC_PROPERTY("DATABUFFER", (Uint32)0);
  INITIALIZER(runBug54944);
}
TESTCASE("Bug54944DATABUFFER", "")
{
  TC_PROPERTY("DATABUFFER", (Uint32)1);
  INITIALIZER(runBug54944);
}
TESTCASE("Bug59496_case1", "")
{
  STEP(runBug59496_case1);
  STEPS(runBug59496_scan, 10);
}
TESTCASE("Bug59496_case2", "")
{
  TC_PROPERTY("CHECK_ROWCOUNT", 1);
  INITIALIZER(runLoadTable);
  STEP(runBug59496_case2);
  STEPS(runBug59496_scan, 10);
}
TESTCASE("899", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(runInit899);
  STEP(runTest899);
  FINALIZER(runEnd899);
}
TESTCASE("LeakApiConnectObjects", "")
{
  INITIALIZER(runLeakApiConnectObjects);
}
TESTCASE("RefreshLocking",
         "Test Refresh locking properties")
{
  INITIALIZER(runRefreshLocking);
}
TESTCASE("BugXXX","")
{
  INITIALIZER(runBugXXX_init);
  STEP(runBugXXX_createIndex);
  STEP(runBugXXX_trans);
}
TESTCASE("Bug16834333","")
{
  INITIALIZER(runBug16834333);
}
TESTCASE("DeleteNdbInFlight","")
{
  INITIALIZER(runDeleteNdbInFlight);
}
TESTCASE("FillQueueREDOLog",
         "Verify that we can handle a REDO log queue situation")
{
  INITIALIZER(insertError5083);
  STEP(runLoadTableFail);
  FINALIZER(clearError5083);
}
TESTCASE("AccCommitOrder",
         "Bug19031389. MT kernel crash on deleted tuple in read*-delete.")
{
  TC_PROPERTY("OPSTEPS", (Uint32)2);
  TC_PROPERTY("RUN", (Uint32)0);
  TC_PROPERTY("RUNNING", (Uint32)0);
  STEP(runAccCommitOrder);
  STEPS(runAccCommitOrderOps, 2);
}
TESTCASE("DeleteNdbWhilePoll",
	 "Delete an Ndb while it(trp_clnt) is in poll queue. Will crash the test, and thus not to be run in a regular test" ){
  INITIALIZER(runInsertOneTuple);
  STEP(runLockTuple);
  STEP(runReadLockedTuple);
  STEP(deleteNdbWhileWaiting);
  FINALIZER(runClearTable2);
}
NDBT_TESTSUITE_END(testBasic);

#if 0
TESTCASE("ReadConsistency",
	 "Check that a read within a transaction returns the " \
	 "same result no matter"){
  STEP(runInsertOne);
  STEP(runReadOne);
  FINALIZER(runClearTable2);
}
TESTCASE("Fill", 
	 "Verify what happens when we fill the db" ){
  INITIALIZER(runFillTable);
  INITIALIZER(runPkRead);
  FINALIZER(runClearTable2);
}
#endif

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testBasic);
  return testBasic.execute(argc, argv);
}



