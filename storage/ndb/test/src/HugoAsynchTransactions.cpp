/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <NdbSleep.h>
#include <HugoAsynchTransactions.hpp>
#include <random.h>


HugoAsynchTransactions::HugoAsynchTransactions(const NdbDictionary::Table& _t)
  : HugoTransactions(_t),
    transactionsCompleted(0),
    transInfo(NULL),
    theNdb(NULL),
    totalLoops(0),
    recordsPerLoop(0),
    operationType(NO_READ),
    execType(Commit),
    nextUnProcessedRecord(0),
    loopNum(0),
    totalCompletedRecords(0),
    maxUsedRetries(0),
    finished(false),
    testResult(NDBT_OK)
{
}

HugoAsynchTransactions::~HugoAsynchTransactions(){
  deallocTransactions();
}

int
HugoAsynchTransactions::loadTableAsynch(Ndb* pNdb, 
				  int records,
				  int batch,
				  int trans,
				  int operations){

  int result = executeAsynchOperation(pNdb, records, batch, trans, operations, 
                                       NO_INSERT);
  g_info << (unsigned int)transactionsCompleted * operations 
	 << "|- inserted..." << endl;

  return result;
} 

int 
HugoAsynchTransactions::pkDelRecordsAsynch(Ndb* pNdb, 
				     int records,
				     int batch,
				     int trans,
				     int operations) {
  
  g_info << "|- Deleting records asynchronous..." << endl;

  int result =  executeAsynchOperation(pNdb, records, batch, trans, 
                                        operations, 
                                        NO_DELETE);
  g_info << "|- " << (unsigned int)transactionsCompleted * operations 
	 << " deleted..." << endl;

  return result;  
}

int 
HugoAsynchTransactions::pkReadRecordsAsynch(Ndb* pNdb, 
				      int records,
				      int batch,
				      int trans,
				      int operations) {

  g_info << "|- Reading records asynchronous..." << endl;

  allocRows(trans*operations);
  int result = executeAsynchOperation(pNdb, records, batch, trans, operations, 
                                       NO_READ);

  g_info << "|- " << (unsigned int)transactionsCompleted * operations 
	 << " read..."
	 << endl;

  deallocRows();

  return result;
}

int 
HugoAsynchTransactions::pkUpdateRecordsAsynch(Ndb* pNdb, 
					int records,
					int batch,
					int trans,
					int operations) {

  g_info << "|- Updating records asynchronous..." << endl;

  allocRows(trans*operations);
  int result = executeAsynchOperation(pNdb, records, batch, trans, operations,
                                       NO_UPDATE);
  
  g_info << "|- " << (unsigned int)transactionsCompleted * operations 
	 << " read..."
	 << endl;

  deallocRows();

  return result;
}


void 
HugoAsynchTransactions::allocTransactions(int trans, int maxOpsPerTrans) {
  if (transInfo != NULL) {
    deallocTransactions(); 
  }
  transInfo = new TransactionInfo[trans];
  
  /* Initialise transaction info array */
  TransactionInfo init;
  init.hugoP= this;
  init.transaction= NULL;
  init.startRecordId= 0;
  init.numRecords= 0;
  init.resultRowStartIndex= 0;
  init.retries= 0;
  init.opType= NO_READ;

  for (int i=0; i < trans; i++)
  {
    transInfo[i]= init;
    transInfo[i].resultRowStartIndex= (i * maxOpsPerTrans);
  };
}

void 
HugoAsynchTransactions::deallocTransactions() {
  if (transInfo != NULL){
    delete[] transInfo;
  }
  transInfo = NULL;
}

int
HugoAsynchTransactions::getNextWorkTask(int* startRecordId, int* numRecords)
{
  /* Get a start record id and # of records for the next work task
   * We return a range of up to maxOpsPerTrans records
   * If there are no unprocessed records remaining, we return -1
   */
  if (nextUnProcessedRecord == recordsPerLoop)
  {
    /* If we've completed all loops then stop.  Otherwise, loop around */
    if ((loopNum + 1) == totalLoops)
      return -1; // All work has been dispatched
    else
    {
      loopNum++;
      nextUnProcessedRecord= 0;
    }
  }

  int availableRecords= recordsPerLoop- nextUnProcessedRecord;
  int recordsInTask= (availableRecords < maxOpsPerTrans)?
    availableRecords : maxOpsPerTrans;

  *startRecordId= nextUnProcessedRecord;
  *numRecords= recordsInTask;
  
  nextUnProcessedRecord+= recordsInTask;

  return 0;
}

int
HugoAsynchTransactions::defineUpdateOpsForTask(TransactionInfo* tInfo)
{
  int check= 0;
  int a= 0;
  
  NdbTransaction* trans= tInfo->transaction;

  if (trans == NULL) {
    return -1;
  }	

  for (int recordId= tInfo->startRecordId; 
       recordId < (tInfo->startRecordId + tInfo->numRecords); 
       recordId++)
  {
    NdbOperation* pOp= trans->getNdbOperation(tab.getName());
    if (pOp == NULL) { 
      NDB_ERR(trans->getNdbError());
      trans->close();
      return -1;
    }
    
    /* We assume that row values have already been read. */
    int updateVal= calc.getUpdatesValue(rows[recordId]) + 1;
    
    check= pOp->updateTuple();
    if (equalForRow(pOp, recordId) != 0)
    {
      NDB_ERR(trans->getNdbError());
      trans->close();
      return -1;
    }
    // Update the record
    for (a = 0; a < tab.getNoOfColumns(); a++) {
      if (tab.getColumn(a)->getPrimaryKey() == false) {
        if (setValueForAttr(pOp, a, recordId, updateVal) != 0) {
          NDB_ERR(trans->getNdbError());
          trans->close();
          return -1;
        }
      }
    }
  } // For recordId

  return 0;
}

int
HugoAsynchTransactions::defineTransactionForTask(TransactionInfo* tInfo,
                                                 ExecType taskExecType)
{
  int check= 0;
  int a= 0;
  NdbTransaction* trans= theNdb->startTransaction();
  
  if (trans == NULL) {
    NDB_ERR(theNdb->getNdbError());
    return -1;
  }	

  for (int recordId= tInfo->startRecordId; 
       recordId < (tInfo->startRecordId + tInfo->numRecords); 
       recordId++)
  {
    NdbOperation* pOp= trans->getNdbOperation(tab.getName());
    if (pOp == NULL) { 
      NDB_ERR(trans->getNdbError());
      theNdb->closeTransaction(trans);
      return -1;
    }
    
    switch (tInfo->opType) {
    case NO_INSERT: 
      // Insert
      check = pOp->insertTuple();
      if (check == -1) { 
        NDB_ERR(trans->getNdbError());
        theNdb->closeTransaction(trans);
        return -1;
      }
      
      // Set a calculated value for each attribute in this table	 
      for (a = 0; a < tab.getNoOfColumns(); a++) {
        if (setValueForAttr(pOp, a, recordId, 0 ) != 0) {	  
          NDB_ERR(trans->getNdbError());
          theNdb->closeTransaction(trans);	  
          return -1;
        }
      } // For each attribute
      break;
    case NO_UPDATE:
    {
      g_err << "Attempt to define update transaction" << endl;
      return -1;
    }
    case NO_READ:
      // Define primary keys
      check = pOp->readTuple();
      if (equalForRow(pOp, recordId) != 0)
      {
        NDB_ERR(trans->getNdbError());
        theNdb->closeTransaction(trans);
        return -1;
      }	    
      // Define attributes to read  
      for (a = 0; a < tab.getNoOfColumns(); a++) {
        if ((rows[recordId]->attributeStore(a) = 
             pOp->getValue(tab.getColumn(a)->getName())) == 0) {
          NDB_ERR(trans->getNdbError());
          theNdb->closeTransaction(trans);
          return -1;
        }
      }	    	  
      break;
    case NO_DELETE:
      // Delete
      check = pOp->deleteTuple();
      if (check == -1) { 
        NDB_ERR(trans->getNdbError());
        theNdb->closeTransaction(trans);
        return -1;
      }
      
      // Define primary keys
      if (equalForRow(pOp, recordId) != 0)
      {
        NDB_ERR(trans->getNdbError());
        theNdb->closeTransaction(trans);
        return -1;
      }    
      break;
    default:
      // Should not happen...
      theNdb->closeTransaction(trans);
      return -1;
    }
  } // For recordId

  tInfo->transaction= trans;

  /* Now send it */
  tInfo->transaction->executeAsynch(taskExecType,
                                    &callbackFunc,
                                    tInfo);

  return 0;
}

int
HugoAsynchTransactions::beginNewTask(TransactionInfo* tInfo)
{
  tInfo->transaction= NULL;
  tInfo->startRecordId= 0;
  tInfo->numRecords= 0;
  tInfo->retries= 0;
  
  /* Adjust for update special case */
  NDB_OPERATION realOpType= operationType;
  ExecType realExecType= execType;
  if (operationType == NO_UPDATE)
  {
    realOpType= NO_READ;
    realExecType= NoCommit;
  }
  tInfo->opType= realOpType;

  if (getNextWorkTask(&tInfo->startRecordId,
                      &tInfo->numRecords) == 0)
  {
    /* Have a task to do */
    if (defineTransactionForTask(tInfo, realExecType) != 0)
    {
      g_err << "Error defining new transaction" << endl;
      return -1;
    }

    return 0;
  }
  else
  {
    /* No more work to do */
    return 1;
  }
}

void 
HugoAsynchTransactions::callbackFunc(int result,
                                     NdbConnection* trans,
                                     void* anObject) {
  /* Execute callback method on passed object */
  HugoAsynchTransactions::TransactionInfo* tranInfo=
    (HugoAsynchTransactions::TransactionInfo*) anObject;

  tranInfo->hugoP->callback(result, trans, tranInfo);
}


void
HugoAsynchTransactions::callback(int result, 
                                 NdbConnection* trans,
                                 TransactionInfo* tInfo)
{
  if (finished)
    return; // No point continuing here

  // Paranoia
  if (trans != tInfo->transaction)
  {
    g_err << "Transactions not same in callback!" << endl;
    finished= true;
    testResult= NDBT_FAILED;
    return;
  }

  NdbError transErr= trans->getNdbError();

  if (transErr.code == 0)
  {
    /* This transaction executed successfully, perform post-execution 
     * steps
     */
    switch (tInfo->opType)
    {
    case NO_READ:
      // Verify the data!
      for (int recordId = tInfo->startRecordId; 
           recordId < (tInfo->startRecordId + tInfo->numRecords); 
           recordId++) 
      {
        if (calc.verifyRowValues(rows[recordId]) != 0) {
          g_info << "|- Verify failed..." << endl;
          // Close all transactions
          finished= true;
          testResult= NDBT_FAILED;
          return;
        }
      }
      
      if (operationType == NO_UPDATE)
      {
        /* Read part of update completed, now define the update...*/
        if (defineUpdateOpsForTask(tInfo) == 0)
        {
          tInfo->opType= NO_UPDATE;
          tInfo->transaction->executeAsynch(Commit,
                                            &callbackFunc,
                                            tInfo);
        }
        else
        {
          g_err << "Error defining update operations in callback" << endl;
          finished= true;
          testResult= NDBT_FAILED;
        }
        
        /* return to polling loop awaiting completion of updates...*/
        return;
      }
      
      break;
    case NO_UPDATE:
    case NO_INSERT:
    case NO_DELETE:
      break;
    }
    
    /* Task completed successfully
     * Now close the transaction, and start next task, if there is one 
     */
    trans ->close();
    transactionsCompleted ++;
    totalCompletedRecords+= tInfo->numRecords;
    
    if (beginNewTask(tInfo) < 0)
    {
      finished= true;
      g_err << "Error begin new task" << endl;
      testResult= NDBT_FAILED;
    }
  }
  else
  {
    /* We have had some sort of issue with this transaction ... */
    g_err << "Callback got error on task : " 
          << tInfo->startRecordId << " to "
          << tInfo->startRecordId + tInfo->numRecords << "  "
          << transErr.code << ":" 
          << transErr.message 
          << ". Task type : " << tInfo->opType <<  endl;
    
    switch(transErr.status) {
    case NdbError::TemporaryError:
      
      if (tInfo->retries < 10) // Support up to 10 retries
      {
        /* Retry original request */
        tInfo->retries++;
        tInfo->transaction->close();
        
        if (tInfo->retries > maxUsedRetries)
          maxUsedRetries= tInfo->retries;
        
        /* Exponential backoff - note that this also delays callback
         * handling for other outstanding transactions so in effect
         * serialises processing
         */
        int multiplier= 1 << tInfo->retries;
        int base= 200; // millis
        int backoffMillis= multiplier*base + myRandom48(base);
        
        g_err << "  Error is temporary, retrying in "
              << backoffMillis << " millis.  Retry number " 
              << tInfo->retries << endl;
        NdbSleep_MilliSleep(backoffMillis);
        
        /* If we failed somewhere in an update operation, redo from the start
         * (including reads)
         */
        tInfo->opType= operationType;
        ExecType taskExecType= execType;
        if (operationType == NO_UPDATE)
        {
          tInfo->opType= NO_READ;
          taskExecType= NoCommit;
        }
        
        /* Define a new transction to perform the original task */
        if (defineTransactionForTask(tInfo, taskExecType) != 0)
        {
          g_err << "Error defining retry transaction in callback" << endl;
          finished= true;
          testResult= NDBT_FAILED;
        }
        
        break;
      }

      g_err << "Too many retries (" << tInfo->retries 
            << ") failing." << endl;
      [[fallthrough]];

    default:
      /* Non temporary error */
      NDB_ERR(transErr);
      g_err << "Status= " << transErr.status << " Failing test" << endl;
      testResult= NDBT_FAILED;
      finished= true;
      break;
    };
  } // Successful execution
} // callbackFunc

int 
HugoAsynchTransactions::executeAsynchOperation(Ndb* pNdb,		      
                                               int records,
                                               int batch,
                                               int trans,
                                               int operations,
                                               NDB_OPERATION theOperation,
                                               ExecType theType) {
  
  /* We want to process 'records' records using at most 'trans' transactions,
   * each with at most 'operations' operations.
   * This is done 'batch' times.
   * This procedure sets up the control state, and starts the first 'trans'
   * transactions
   * After that the execution completion callback code handles operation
   * results, and initiating new transactions or retrying failed transactions
   * as necessary.
   * If there is a failure, the finished bool is set, which is detected in the
   * polling loop below.
   * If all of the requested records have been read, this is detected in the
   * loop below
   * Note that Update operations are a special case, comprising a read, executed
   * with NoCommit, followed by an Update executed with Commit.
   */

  theNdb= pNdb;
  totalLoops= batch;
  loopNum= 0;
  recordsPerLoop= records;
  maxOpsPerTrans= operations;
  operationType= theOperation;
  execType= theType;
  nextUnProcessedRecord= 0;
  totalCompletedRecords= 0;
  maxUsedRetries= 0;
  finished= false;
  testResult= NDBT_OK;

  allocTransactions(trans, maxOpsPerTrans);

  /* Start by defining all transactions */
  int nextUndefinedTrans= 0;
  while ((nextUndefinedTrans < trans) &&
         (beginNewTask(&transInfo[nextUndefinedTrans++]) == 0))
  { /* Empty */ };
  
  /* Poll for results, the transaction callback will handle results
   * and initiate new operations as necessary, setting finished to
   * true if there's a problem.
   */
  while (!finished)
  {
    pNdb->pollNdb(3000,0);
    
    if (totalCompletedRecords == (records * totalLoops))
      finished = true;
  };

  deallocTransactions();
  theNdb= NULL;

  return testResult;
}
