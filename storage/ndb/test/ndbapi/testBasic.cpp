/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <Bitmask.hpp>
#include <random.h>
#include <signaldata/DumpStateOrd.hpp>

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

static
int
readOneNoCommit(Ndb* pNdb, NdbConnection* pTrans, 
		const NdbDictionary::Table* tab,NDBT_ResultRow * row){
  int a;
  NdbOperation * pOp = pTrans->getNdbOperation(tab->getName());
  if (pOp == NULL){
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  HugoTransactions tmp(*tab);

  int check = pOp->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  // Define primary keys
  for(a = 0; a<tab->getNoOfColumns(); a++){
    if (tab->getColumn(a)->getPrimaryKey() == true){
      if(tmp.equalForAttr(pOp, a, 0) != 0){
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }
  }
  
  // Define attributes to read  
  for(a = 0; a<tab->getNoOfColumns(); a++){
    if((row->attributeStore(a) = 
	pOp->getValue(tab->getColumn(a)->getName())) == 0) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
  }

  check = pTrans->execute(NoCommit);     
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError(); 
    ERR(err);
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
  { NdbOperation::InsertRequest, 4022, TupError::TE_OI },
  { NdbOperation::InsertRequest, 4023, TupError::TE_OI },
  { NdbOperation::UpdateRequest, 4030, TupError::TE_UI },
  { -1, 0, 0 }
};

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
  }

  /**
   * update
   */
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
  NdbDictionary::Dictionary * dict = pNdb->getDictionary();

  HugoOperations ops(*ctx->getTab());
  
  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();
  
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
  const int rows = ctx->getNumRecords();
  
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
	ERR(pTrans->getNdbError());
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
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }
    if (pTrans->execute(Commit) != 0)
    {
      ERR(pTrans->getNdbError());
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
  NdbDictionary::Dictionary * dict = pNdb->getDictionary();
  
  HugoOperations ops(*ctx->getTab());

  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();
  
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
      assert(BitmaskImpl::count(sz, rowmask)== rowcnt);

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
      assert(BitmaskImpl::count(sz, rowmask)== rowcnt);

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
      assert(BitmaskImpl::count(sz, rowmask)== rowcnt);

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
      assert(BitmaskImpl::count(sz, rowmask)== rowcnt);
      assert(rowcnt == 0);

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
          ERR(err);
          NdbSleep_MilliSleep(50);
          retryAttempt++;
          lockHandles.clear();
          check(hugoOps.closeTransaction(ndb) == 0,
                hugoOps);
          check(hugoOps.startTransaction(ndb) == 0, (*ndb));
          continue;
        }
        ERR(err);
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
    }
    ndb_mgm_destroy_logevent_handle(&handle);
  }

  for (int i = 0; i<5; i++)
  {
    int val1 = DumpStateOrd::DihMaxTimeBetweenLCP;
    int val2 = 7099; // Force start

    restarter.dumpStateAllNodes(&val1, 1);
    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    restarter.dumpStateAllNodes(val, 2);

    restarter.insertErrorInAllNodes(932); // prevent arbit shutdown

    HugoTransactions hugoTrans(*pTab);
    hugoTrans.loadTable(pNdb, 20);

    restarter.dumpStateAllNodes(&val2, 1);

    NdbSleep_SecSleep(15);
    hugoTrans.clearTable(pNdb);

    hugoTransCopy.pkReadRecords(pNdb, rows);

    HugoOperations hugoOps(*pTab);
    hugoOps.startTransaction(pNdb);
    hugoOps.pkInsertRecord(pNdb, 1);
    hugoOps.execute_NoCommit(pNdb);

    restarter.insertErrorInAllNodes(5056);
    restarter.dumpStateAllNodes(&val2, 1);
    restarter.waitClusterNoStart();
    int vall = 11009;
    restarter.dumpStateAllNodes(&vall, 1);
    restarter.startAll();
    restarter.waitClusterStarted();
  }

  pDict->dropTable(copy.getName());

  // remove 25-page pgman
  restarter.restartAll(false, true, true);
  restarter.waitClusterNoStart();
  restarter.startAll();
  restarter.waitClusterStarted();
  return NDBT_OK;
}

int
runBug54944(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table * pTab = ctx->getTab();
  NdbRestarter res;

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

    res.insertErrorInAllNodes(8087);

    HugoTransactions hugoTrans(*pTab);
    hugoTrans.loadTableStartFrom(pNdb, 50000, 100);

    hugoOps.execute_Rollback(pNdb);
    hugoTrans.clearTable(pNdb);

    res.insertErrorInAllNodes(0);
  }
  return NDBT_OK;
}

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
TESTCASE("Bug54986", "")
{
  INITIALIZER(runBug54986);
}
TESTCASE("Bug54944", "")
{
  INITIALIZER(runBug54944);
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



