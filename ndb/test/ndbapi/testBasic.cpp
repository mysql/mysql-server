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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>

#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()


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
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), records, batchSize) != NDBT_OK){
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int runPkDirtyRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int i = 0;
  bool dirty = true;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), records, 
				batchSize, dirty) != NDBT_OK){
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

    ndbout << i <<": Sleeping for " << sleepTime << " ms" << endl;
    NdbSleep_MilliSleep(sleepTime);

    // Dont care about result of these ops
    hugoOps.pkReadRecord(pNdb, 1, true);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction
    // Multiple operations
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, TryCommit) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction, TryCommit
    // Several operations in one transaction
    // The insert is OK
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, true) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 4, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Commit transaction, CommitAsMuchAsPossible
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, true) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 4, true) == 0);
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
  }while(false);

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
    CHECK(hugoOps.execute_Commit(pNdb, CommitAsMuchAsPossible) == 630);
  }while(false);

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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, false) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // No commit transaction, readTupleExcluive
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // No commit transaction, rollback
    // Multiple operations
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 2, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 3, true) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 4, true) == 0);
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
      CHECK(hugoOps.pkReadRecord(pNdb, i, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);

    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is not deleted
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is back to original value
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 1, false, numRecords) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK); // Update value 0
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Update  record 5
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkUpdateRecord(pNdb, 1, numRecords, 5) == 0);// Updates value 5
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
  
    // Check record is updated
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true, numRecords) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(5) == NDBT_OK); // Updates value 5
    CHECK(hugoOps.execute_Rollback(pNdb) == 0);

    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    // Check record is back to original value
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 1, true, numRecords) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, false, 10) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.verifyUpdatesValue(0) == NDBT_OK);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);
    
    Uint32 updatesValue = 0;
    Uint32 j;
    for(Uint32 i = 0; i<1; i++){
      // Read  record 5 - 10
      CHECK(hugoOps.startTransaction(pNdb) == 0);  
      CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
      
      for(j = 0; j<10; j++){
	// Update  record 5 - 10
	updatesValue++;
	CHECK(hugoOps.pkUpdateRecord(pNdb, 5, 10, updatesValue) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

	CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	CHECK(hugoOps.verifyUpdatesValue(updatesValue) == 0);
      }      
      
      for(j = 0; j<10; j++){
	// Delete record 5 - 10 times
	CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

#if 0
	// Check records are deleted
	CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
#endif

	updatesValue++;
	CHECK(hugoOps.pkInsertRecord(pNdb, 5, 10, updatesValue) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	
	CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
	CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
	CHECK(hugoOps.verifyUpdatesValue(updatesValue) == 0);
      }

      CHECK(hugoOps.pkDeleteRecord(pNdb, 5, 10) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

      // Check records are deleted
      CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 626);
      CHECK(hugoOps.execute_Rollback(pNdb) == 0);
      
      CHECK(hugoOps.closeTransaction(pNdb) == 0);
    }
    
    // Check records are not deleted
    // after rollback
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
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
    CHECK(hugoOps.pkReadRecord(pNdb, 5, true, 10) == 0);
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
    for(int i = 0; i<OPS_TOTAL; i += OPS_PER_TRANS){
      for(int j = 0; j<OPS_PER_TRANS; j++){
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
  
  for(int loop = 0; loop<LOOPS; loop++){
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    for(int i = 0; i<OPS_TOTAL-1; i ++){
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


NDBT_TESTSUITE(testBasic);
TESTCASE("PkInsert", 
	 "Verify that we can insert and delete from this table using PK"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runInsert);
  VERIFIER(runVerifyInsert);
}
TESTCASE("PkRead", 
	   "Verify that we can insert, read and delete from this table using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("PkDirtyRead", 
	 "Verify that we can insert, dirty read and delete from this table using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkDirtyRead);
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
TESTCASE("ReadConsistency",
	 "Check that a read within a transaction returns the " \
	 "same result no matter"){
  STEP(runInsertOne);
  STEP(runReadOne);
  FINALIZER(runClearTable2);
}
TESTCASE("PkInsertTwice", 
	 "Verify that we can't insert an already inserted record."
	 "Error should be returned" ){
  INITIALIZER(runLoadTable);
  STEP(runInsertTwice);
  FINALIZER(runClearTable);
}
TESTCASE("Fill", 
	 "Verify what happens when we fill the db" ){
  INITIALIZER(runFillTable);
  INITIALIZER(runPkRead);
  FINALIZER(runClearTable2);
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
TESTCASE("MassiveTransaction",
         "Test very large insert transaction"){
  INITIALIZER(runLoadTable2);
  FINALIZER(runClearTable2);
}
NDBT_TESTSUITE_END(testBasic);

int main(int argc, const char** argv){
  ndb_init();
  return testBasic.execute(argc, argv);
}



