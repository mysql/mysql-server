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

#include "Bank.hpp"
#include <time.h>
#include <NdbSleep.h>
#include <UtilTransactions.hpp>

Bank::Bank():
  m_ndb("BANK"),
  m_maxAccount(-1),
  m_initialized(false)
{

}

int Bank::init(){
  if (m_initialized == true)
    return NDBT_OK;

  myRandom48Init(NdbTick_CurrentMillisecond());

  m_ndb.init();   
  while (m_ndb.waitUntilReady(10) != 0)
    ndbout << "Waiting for ndb to be ready" << endl;

  if (getNumAccounts() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int Bank::performTransactions(int maxSleepBetweenTrans, int yield){

  if (init() != NDBT_OK)
    return NDBT_FAILED;
  int transactions = 0;

  while(1){

    while(m_ndb.waitUntilReady(10) != 0)
      ndbout << "Waiting for ndb to be ready" << endl;

    while(performTransaction() != NDBT_FAILED){
      transactions++;

      if (maxSleepBetweenTrans > 0){
	int val = myRandom48(maxSleepBetweenTrans);
	NdbSleep_MilliSleep(val);      
      }

      if((transactions % 100) == 0)
	g_info << transactions  << endl;

      if (yield != 0 && transactions >= yield)
	return NDBT_OK;
    }
  }
  return NDBT_FAILED;

}

int Bank::performTransaction(){
  int result = NDBT_OK;

  if (m_maxAccount <= 0){
    g_err << "No accounts in bank" << endl;
    return NDBT_FAILED;
  }

  int fromAccount = myRandom48(m_maxAccount);
  int toAccount = myRandom48(m_maxAccount);
    
  if (fromAccount == toAccount){
    // Increase toAccount with 1
    toAccount = (toAccount+1)%m_maxAccount;
  }
    
  int maxAmount = getMaxAmount();
    
  int amount = myRandom48(maxAmount);

 retry_transaction:
  int res = performTransaction(fromAccount, toAccount, amount);
  if (res != 0){
    switch (res){
    case NDBT_FAILED:
      g_err << "performTransaction returned NDBT_FAILED" << endl
	    << "  fromAccount = " << fromAccount << endl
	    << "  toAccount = " << toAccount << endl
	    << "  amount = " << amount << endl;
      result = NDBT_FAILED;
      break;
    case NOT_ENOUGH_FUNDS:
      //   ndbout << "performTransaction returned NOT_ENOUGH_FUNDS" << endl;	
      break;
    case NDBT_TEMPORARY:
      g_err << "TEMPORARY_ERRROR retrying" << endl;
      goto retry_transaction;
      break;
    default:
      g_info << "performTransaction returned "<<res << endl;	
      break;
    }
  }
  return result;
}

/**
 * Perform a transaction in the bank. 
 * Ie. transfer money from one account to another.
 *
 * @param 
 * @return 0 if successful or an error code
 */
int Bank::performTransaction(int fromAccountId,
			     int toAccountId,
			     int amount ){
  /**
   * 1. Start transaction
   * 2. Check balance on from account, if there is
   *    not enough funds abort transaction
   * 3. Update ACCOUNT set balance = balance - amount on
   *    from account 
   * 4. Insert withdrawal in TRANSACTION
   * 5. Insert deposit in transaction
   * 6. Update ACCOUNT set balance = balance + amount on 
   *    to account
   * 7. Commit transaction
   */ 
  //  g_info << "performTransaction " << fromAccountId 
  //	 << ", "<<toAccountId<<", "<<amount << endl;

  // Call the first implementation of this trans
  // In the future we can have several different versions of this trans
  // and call them randomly 
  return performTransactionImpl1(fromAccountId, toAccountId, amount); 
}


int Bank::performTransactionImpl1(int fromAccountId,
				  int toAccountId,
				  int amount ){

  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if( pTrans == NULL ) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError){
      ERR(err);
      return NDBT_TEMPORARY;
    }
    ERR(err);
    return NDBT_FAILED;
  }
    
  /** 
   * Check balance on from account
   */
  NdbOperation* pOp = pTrans->getNdbOperation("ACCOUNT");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp->readTupleExclusive();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp->equal("ACCOUNT_ID", fromAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  NdbRecAttr* balanceFromRec = pOp->getValue("BALANCE");
  if( balanceFromRec ==NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  NdbRecAttr* fromAccountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( fromAccountTypeRec == NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);    
    if (err.status == NdbError::TemporaryError){
      ERR(err);
      return NDBT_TEMPORARY;
    }
    ERR(err);
    return NDBT_FAILED;
  }
    
  Uint32  balanceFrom = balanceFromRec->u_32_value();
  //  ndbout << "balanceFrom: " << balanceFrom << endl;

  if (((Int64)balanceFrom - amount) < 0){
    m_ndb.closeTransaction(pTrans);      
    //ndbout << "Not enough funds" << endl;      
    return NOT_ENOUGH_FUNDS;
  }

  Uint32 fromAccountType = fromAccountTypeRec->u_32_value();

  /** 
   * Read balance on to account
   */
  NdbOperation* pOp6 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp6 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp6->readTupleExclusive();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp6->equal("ACCOUNT_ID", toAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  NdbRecAttr* balanceToRec = pOp6->getValue("BALANCE");
  if( balanceToRec == NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* toAccountTypeRec = pOp6->getValue("ACCOUNT_TYPE");
  if( toAccountTypeRec == NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);    
    if (err.status == NdbError::TemporaryError){
      ERR(err);
      return NDBT_TEMPORARY;
    }
    ERR(err);
    return NDBT_FAILED;
  }
    
  Uint32  balanceTo = balanceToRec->u_32_value();
  //  ndbout << "balanceTo: " << balanceTo << endl;
  Uint32 toAccountType = toAccountTypeRec->u_32_value();

  // Ok, all clear to do the transaction
  Uint64 transId;
  if (getNextTransactionId(transId) != NDBT_OK){
    return NDBT_FAILED;
  }

  Uint64 currTime;
  if (getCurrTime(currTime) != NDBT_OK){
    return NDBT_FAILED;
  }

  /**
   * Update balance on from account
   */
  NdbOperation* pOp2 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp2 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp2->updateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp2->equal("ACCOUNT_ID", fromAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp2->setValue("BALANCE", balanceFrom - amount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Update balance on to account
   */
  NdbOperation* pOp3 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp3 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp3->updateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp3->equal("ACCOUNT_ID", toAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp3->setValue("BALANCE", balanceTo + amount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Insert withdrawal transaction
   */
  NdbOperation* pOp4 = pTrans->getNdbOperation("TRANSACTION");
  if (pOp4 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp4->insertTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp4->equal("TRANSACTION_ID", transId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->equal("ACCOUNT", fromAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("ACCOUNT_TYPE", fromAccountType);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("OTHER_ACCOUNT", toAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("TRANSACTION_TYPE", WithDrawal);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp4->setValue("TIME", currTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("AMOUNT", amount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Insert deposit transaction
   */
  NdbOperation* pOp5 = pTrans->getNdbOperation("TRANSACTION");
  if (pOp5 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp5->insertTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp5->equal("TRANSACTION_ID", transId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->equal("ACCOUNT", toAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("ACCOUNT_TYPE", toAccountType);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("OTHER_ACCOUNT", fromAccountId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("TRANSACTION_TYPE", Deposit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("TIME", currTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("AMOUNT", amount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit);
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);    
    if (err.status == NdbError::TemporaryError){
      ERR(err);
      return NDBT_TEMPORARY;
    }
    ERR(err);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;  
}


    

int Bank::performMakeGLs(int yield){
  int result;
  if (init() != NDBT_OK)
    return NDBT_FAILED;
  
  int counter, maxCounter;
  int yieldCounter = 0;

  while (1){
    // Counters to keep tracck of how many
    // GLs should be made before performing a validation
    counter = 0;
    maxCounter = 50 + myRandom48(100);
    
    while(m_ndb.waitUntilReady(10) != 0)
      ndbout << "Waiting for ndb to be ready" << endl;

    /** 
     * Validate GLs and Transactions for previous days
     *
     */
    result = performValidateGLs();
    if (result != NDBT_OK){
      if (result == VERIFICATION_FAILED){
	g_err << "performValidateGLs verification failed" << endl;
	return NDBT_FAILED;
      }
      g_info << "performValidateGLs failed" << endl;
      continue;
    }

    result = performValidatePurged();
    if (result != NDBT_OK){
      if (result == VERIFICATION_FAILED){
	g_err << "performValidatePurged verification failed" << endl;
	return NDBT_FAILED;
      }
      g_info << "performValidatePurged failed" << endl;
      continue;
    }

    while (1){

      yieldCounter++;
      if (yield != 0 && yieldCounter >= yield)
	return NDBT_OK;

      /**
       * Find last GL time.  
       * ( GL record with highest time value)
       */
      Uint64 lastGLTime;
      if (findLastGL(lastGLTime) != NDBT_OK){	
	g_info << "findLastGL failed" << endl;
	// Break out of inner while loop
	break;
      }
      
      lastGLTime++;
      
      /** 
       * If last GL time + 1 is smaller than current time
       * perform a GL for that time
       */
      Uint64 currTime;
      if (getCurrTime(currTime) != NDBT_OK){
	g_info << "getCurrTime failed" << endl;
	// Break out of inner while loop
	break;
      }      
      if (lastGLTime < currTime){
	counter++;
	if (performMakeGL(lastGLTime) != NDBT_OK){
	  g_info << "performMakeGL failed" << endl;
	// Break out of inner while loop
	  break;
	}
	
	if (counter > maxCounter){
	  // Break out of inner while loop and 
	  // validatePreviousGLs
	  g_info << "counter("<<counter<<") > maxCounter("<<maxCounter<<")" << endl;
	  break;
	}

      } else {
	;//ndbout << "It's not time to make GL yet" << endl;

	// ndbout << "Sleeping 1 second" << endl;
	NdbSleep_SecSleep(1);      

      }
      
      Uint32 age = 3;
      if (purgeOldGLTransactions(currTime, age) != NDBT_OK){
        g_info << "purgeOldGLTransactions failed" << endl;
	// Break out of inner while loop
	break;
      }     
            
    }
  }
    
  return NDBT_FAILED;
  
}

int Bank::performValidateAllGLs(){
  int result;
  if (init() != NDBT_OK)
    return NDBT_FAILED;
  
  while (1){
    
    while(m_ndb.waitUntilReady(10) != 0)
      ndbout << "Waiting for ndb to be ready" << endl;

    /** 
     * Validate GLs and Transactions for previous days
     * Set age so that ALL GL's are validated
     */
    int age = 100000;
    result = performValidateGLs(age);
    if (result != NDBT_OK){
      if (result == VERIFICATION_FAILED){
	g_err << "performValidateGLs verification failed" << endl;
	return NDBT_FAILED;
      }
      g_err << "performValidateGLs failed" << endl;
      return NDBT_FAILED;
    }

    /**
     * 
     *
     */
    result = performValidatePurged();
    if (result != NDBT_OK){
      if (result == VERIFICATION_FAILED){
	g_err << "performValidatePurged verification failed" << endl;
	return NDBT_FAILED;
      }
      g_err << "performValidatePurged failed" << endl;
      return NDBT_FAILED;
    }
    return NDBT_OK;
  }
    
  return NDBT_FAILED;
  
}

int Bank::findLastGL(Uint64 &lastTime){

 int check;  
  /**
   * SELECT MAX(time) FROM GL
   */
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("GL");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuples();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  eof = rs->nextResult();
  lastTime = 0;
    
  while(eof == 0){
    rows++;
    Uint64 t = timeRec->u_32_value();

    if (t > lastTime)
      lastTime = t;
    
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  
  return NDBT_OK;
}


int Bank::performMakeGL(int time){
  g_info << "performMakeGL: " << time << endl;
  /**
   *  Create one GL record for each account type.
   *  All in the same transaction
   */
  // Start transaction    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
  for (int i = 0; i < getNumAccountTypes(); i++){
  
    if (performMakeGLForAccountType(pTrans, time, i) != NDBT_OK){
      g_err << "performMakeGLForAccountType returned NDBT_FAILED"<<endl;
      m_ndb.closeTransaction(pTrans);      
      return NDBT_FAILED;
    }
  }
  // Execute transaction    
  if( pTrans->execute(Commit) == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }    
  m_ndb.closeTransaction(pTrans);      
  
  return NDBT_OK;
}

int Bank::performMakeGLForAccountType(NdbConnection* pTrans, 
				      Uint64 glTime,
				      Uint32 accountTypeId){
  int check;

  Uint32 balance = 0;
  Uint32 withdrawalCount = 0;
  Uint32 withdrawalSum = 0;
  Uint32 depositSum = 0;
  Uint32 depositCount = 0;
  Uint32 countTransactions = 0;
  Uint32 purged = 0;

  // Insert record in GL so that we know
  // that no one else is performing the same task
  // Set purged = 0 to indicate that TRANSACTION
  // records still exist
  NdbOperation* pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
    
  check = pOp->insertTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pOp->equal("TIME", glTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("BALANCE", balance);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("DEPOSIT_COUNT", depositCount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("DEPOSIT_SUM", depositSum);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("WITHDRAWAL_COUNT", withdrawalCount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("WITHDRAWAL_SUM", withdrawalSum);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("PURGED", purged);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pOp->getNdbError());
    return NDBT_FAILED;
  }

  // Read previous GL record to get old balance
  NdbOperation* pOp2 = pTrans->getNdbOperation("GL");
  if (pOp2 == NULL) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
    
  check = pOp2->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pOp2->equal("TIME", glTime-1);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp2->equal("ACCOUNT_TYPE", accountTypeId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  NdbRecAttr* oldBalanceRec = pOp2->getValue("BALANCE");
  if( oldBalanceRec == NULL ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pOp2->getNdbError());
    return NDBT_FAILED;
  }    

  Uint32 oldBalance = oldBalanceRec->u_32_value();
  //  ndbout << "oldBalance = "<<oldBalance<<endl;
  balance = oldBalance;
  // Start a scan transaction to search
  // for TRANSACTION records with TIME = time 
  // and ACCOUNT_TYPE = accountTypeId
  // Build sum of all found transactions
    
  if (sumTransactionsForGL(glTime, 
			   accountTypeId,
			   balance,
			   withdrawalCount,
			   withdrawalSum,
			   depositSum,
			   depositCount,
			   countTransactions,
			   pTrans) != NDBT_OK){
    return NDBT_FAILED;
  }
  //  ndbout << "sumTransactionsForGL completed" << endl;
  //  ndbout << "balance="<<balance<<endl
  //	 << "withdrawalCount="<<withdrawalCount<<endl
  //	 << "withdrawalSum="<<withdrawalSum<<endl
  //	 << "depositCount="<<depositCount<<endl
  //	 << "depositSum="<<depositSum<<endl;
      


  NdbOperation* pOp3 = pTrans->getNdbOperation("GL");
  if (pOp3 == NULL) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
    
  check = pOp3->updateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pOp3->equal("TIME", glTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->equal("ACCOUNT_TYPE", accountTypeId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("BALANCE", balance);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("DEPOSIT_COUNT", depositCount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("DEPOSIT_SUM", depositSum);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("WITHDRAWAL_COUNT", withdrawalCount);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("WITHDRAWAL_SUM", withdrawalSum);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("PURGED", purged);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Execute transaction    
  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }    

  return NDBT_OK;
}




int Bank::sumTransactionsForGL(const Uint64 glTime, 
			       const Uint32 accountType,
			       Uint32& balance,
			       Uint32& withdrawalCount,
			       Uint32& withdrawalSum,
			       Uint32& depositSum,
			       Uint32& depositCount,
			       Uint32& transactionsCount,
			       NdbConnection* pTrans){
  int check;

  //  g_info << "sumTransactionsForGL: " << glTime << ", " << accountType << endl;
    
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("TRANSACTION");
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuplesExclusive();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* transTypeRec = pOp->getValue("TRANSACTION_TYPE");
  if( transTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* amountRec = pOp->getValue("AMOUNT");
  if( amountRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  int rowsFound = 0;
  eof = rs->nextResult();
    
  while(eof == 0){
    rows++;
    Uint32 a = accountTypeRec->u_32_value();
    Uint64 t = timeRec->u_64_value();

    if (a == accountType && t == glTime){
      rowsFound++;
      // One record found
      int transType = transTypeRec->u_32_value();
      int amount = amountRec->u_32_value();
      if (transType == WithDrawal){
	withdrawalCount++;
	withdrawalSum += amount;
	balance -= amount;
      } else {	  
	assert(transType == Deposit);
	depositCount++;
	depositSum += amount;
	balance += amount;
      }
    }

    eof = rs->nextResult();

    if ((rows % 100) == 0){
      // "refresh" ownner transaction every 100th row
      if (pTrans->refresh() == -1) {
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    }

  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  //  ndbout << rows << " TRANSACTIONS have been read" << endl;
  transactionsCount = rowsFound;

  return NDBT_OK;

}

 int Bank::performValidateGLs(Uint64 age){

  Uint64 currTime;
  if (getCurrTime(currTime) != NDBT_OK){
    return NDBT_FAILED;
  }
  Uint64 glTime = currTime - 1;
  while((glTime > 0) && ((glTime + age) >= currTime)){
    
    int result = performValidateGL(glTime);
    if (result != NDBT_OK){
      g_err << "performValidateGL failed" << endl;
      return result;
    }
    
    glTime--;
  }
  
  return NDBT_OK;
 }

int Bank::performValidateGL(Uint64 glTime){
   
   ndbout << "performValidateGL: " << glTime << endl;
   /**
    * Rules: 
    * - There should be zero or NoAccountTypes GL records for each glTime
    * - If purged == 0, then the TRANSACTION table should be checked
    *   to see that there are:
    *   + DEPOSIT_COUNT deposit transactions with account_type == ACCOUNT_TYPE
    *     and TIME == glTime. The sum of these transactions should be 
    *     DEPOSIT_SUM
    *   + WITHDRAWAL_COUNT withdrawal transactions with account_type == 
    *     ACCOUNT_TYPE and TIME == glTime. The sum of these transactions 
    *     should be WITHDRAWAL_SUM
    *   + BALANCE should be equal to the sum of all transactions plus
    *     the balance of the previous GL record
    * - If purged == 1 then there should be NO transactions with TIME == glTime
    *   and ACCOUNT_TYPE == account_type
    *  
    */ 

   int check;  
   /**
    * SELECT * FROM GL WHERE account_type = @accountType and time = @time
    */
   NdbConnection* pScanTrans = m_ndb.startTransaction();
   if (pScanTrans == NULL) {
     ERR(m_ndb.getNdbError());
     return NDBT_FAILED;
   }
   
   NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("GL");	
   if (pOp == NULL) {
     ERR(pScanTrans->getNdbError());
     m_ndb.closeTransaction(pScanTrans);
     return NDBT_FAILED;
   }
   
   NdbResultSet * rs = pOp->readTuples();
   if( rs == 0 ) {
     ERR(pScanTrans->getNdbError());
     m_ndb.closeTransaction(pScanTrans);
     return NDBT_FAILED;
   }
   
   check = pOp->interpret_exit_ok();
   if( check == -1 ) {
     ERR(pScanTrans->getNdbError());
     m_ndb.closeTransaction(pScanTrans);
     return NDBT_FAILED;
   }
   
   NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
   if( accountTypeRec ==NULL ) {
     ERR(pScanTrans->getNdbError());
     m_ndb.closeTransaction(pScanTrans);
     return NDBT_FAILED;
   }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* purgedRec = pOp->getValue("PURGED");
  if( purgedRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* balanceRec = pOp->getValue("BALANCE");
  if( balanceRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* depositSumRec = pOp->getValue("DEPOSIT_SUM");
  if( depositSumRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* depositCountRec = pOp->getValue("DEPOSIT_COUNT");
  if( depositCountRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* withdrawalSumRec = pOp->getValue("WITHDRAWAL_SUM");
  if( withdrawalSumRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
  NdbRecAttr* withdrawalCountRec = pOp->getValue("WITHDRAWAL_COUNT");
  if( withdrawalCountRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  int countGlRecords = 0;
  int result = NDBT_OK;
  eof = rs->nextResult();
    
  while(eof == 0){
    rows++;
    Uint64 t = timeRec->u_64_value();

    if (t == glTime){
      countGlRecords++;
      Uint32 a = accountTypeRec->u_32_value();
      Uint32 purged = purgedRec->u_32_value();
      Uint32 wsum = withdrawalSumRec->u_32_value();
      Uint32 wcount = withdrawalCountRec->u_32_value();
      Uint32 dsum = depositSumRec->u_32_value();
      Uint32 dcount = depositCountRec->u_32_value();
      Uint32 b = balanceRec->u_32_value();

      Uint32 balance = 0; 
      Uint32 withdrawalSum = 0;
      Uint32 withdrawalCount = 0;
      Uint32 depositSum = 0;
      Uint32 depositCount = 0;
      Uint32 countTransactions = 0;
      if (purged == 0){	
	// If purged == 0, then the TRANSACTION table should be checked
	// to see that there are:
	// + DEPOSIT_COUNT deposit transactions with account_type == ACCOUNT_TYPE
	//   and TIME == glTime. The sum of these transactions should be 
	//   DEPOSIT_SUM
	// + WITHDRAWAL_COUNT withdrawal transactions with account_type == 
	//   ACCOUNT_TYPE and TIME == glTime. The sum of these transactions 
	//   should be WITHDRAWAL_SUM
	// + BALANCE should be equal to the sum of all transactions plus
	//   the balance of the previous GL record	
	if (sumTransactionsForGL(t, 
				 a,
				 balance,
				 withdrawalCount,
				 withdrawalSum,
				 depositSum,
				 depositCount,
				 countTransactions,
				 pScanTrans) != NDBT_OK){
	  result = NDBT_FAILED;	  
	} else {
	  Uint32 prevBalance = 0;
	  if (getBalanceForGL(t-1, a, prevBalance) != NDBT_OK){
	    result = NDBT_FAILED;
	  } else
	  if (((prevBalance + balance) != b) ||
	      (wsum != withdrawalSum) ||
	      (wcount != withdrawalCount) ||
	      (dsum != depositSum) ||
	      (dcount != depositCount)){
	    g_err << "performValidateGL, sums and counts failed" << endl
		  << "balance   :   " << balance+prevBalance << "!="<<b<<endl
		  << "with sum  :   " << withdrawalSum << "!="<<wsum<<endl
		  << "with count:   " << withdrawalCount << "!="<<wcount<<endl
		  << "dep sum   :   " << depositSum << "!="<<dsum<<endl
		  << "dep count :   " << depositCount << "!="<<dcount<<endl;
	    result = VERIFICATION_FAILED;
	  }
	    }	  

      } else {
	assert(purged == 1);
	// If purged == 1 then there should be NO transactions with 
	// TIME == glTime and ACCOUNT_TYPE == account_type
	
	if (sumTransactionsForGL(t, 
				 a,
				 balance,
				 withdrawalCount,
				 withdrawalSum,
				 depositSum,
				 depositCount,
				 countTransactions,
				 pScanTrans) != NDBT_OK){
	  result = NDBT_FAILED;	  
	} else {
	  if (countTransactions != 0){
	    g_err << "performValidateGL, countTransactions("<<countTransactions<<") != 0" << endl;
	    result = VERIFICATION_FAILED;
	  }
	}	 
      }

    }
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);

  // - There should be zero or NoAccountTypes GL records for each glTime
  if ((countGlRecords != 0) && (countGlRecords != getNumAccountTypes())){
    g_err << "performValidateGL: " << endl
	   << "countGlRecords = " << countGlRecords << endl;
    result = VERIFICATION_FAILED;
  }

  return result;

   
 }

int Bank::getBalanceForGL(const Uint64 glTime,
			  const Uint32 accountTypeId,
			  Uint32 &balance){
  int check;

  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
  
  NdbOperation* pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
    
  check = pOp->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pOp->equal("TIME", glTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  NdbRecAttr* balanceRec = pOp->getValue("BALANCE");
  if( balanceRec == NULL ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  } 

  m_ndb.closeTransaction(pTrans);

  balance = balanceRec->u_32_value();

  return NDBT_OK;
}



int Bank::getOldestPurgedGL(const Uint32 accountType,
			    Uint64 &oldest){
  int check;  
  /**
   * SELECT MAX(time) FROM GL WHERE account_type = @accountType and purged=1
   */
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("GL");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuples();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* purgedRec = pOp->getValue("PURGED");
  if( purgedRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  eof = rs->nextResult();
  oldest = 0;
    
  while(eof == 0){
    rows++;
    Uint32 a = accountTypeRec->u_32_value();
    Uint32 p = purgedRec->u_32_value();

    if (a == accountType && p == 1){
      // One record found
      Uint64 t = timeRec->u_64_value();
      if (t > oldest)
	oldest = t;
    }
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  
  return NDBT_OK;
}

int Bank::getOldestNotPurgedGL(Uint64 &oldest,
			       Uint32 &accountTypeId,
			       bool &found){
  int check;  
  /**
   * SELECT time, accountTypeId FROM GL 
   * WHERE purged=0 order by time asc
   */
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("GL");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuples();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* purgedRec = pOp->getValue("PURGED");
  if( purgedRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  eof = rs->nextResult();
  oldest = (Uint64)-1;
  found = false;
    
  while(eof == 0){
    rows++;
    Uint32 p = purgedRec->u_32_value();
    if (p == 0){
      found = true;
      // One record found
      Uint32 a = accountTypeRec->u_32_value();      
      Uint64 t = timeRec->u_64_value();
      if (t < oldest){
	oldest = t;
	accountTypeId = a;
      }
    }
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);

  return NDBT_OK;
}


int Bank::checkNoTransactionsOlderThan(const Uint32 accountType,
				       const Uint64 oldest){
  /**
   * SELECT COUNT(transaction_id) FROM TRANSACTION 
   * WHERE account_type = @accountType and time <= @oldest
   *
   */

  int check;  
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("TRANSACTION");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuples();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* transactionIdRec = pOp->getValue("TRANSACTION_ID");
  if( transactionIdRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);   
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  int found = 0;
  eof = rs->nextResult();
    
  while(eof == 0){
    rows++;
    Uint32 a = accountTypeRec->u_32_value();
    Uint32 t = timeRec->u_32_value();

    if (a == accountType && t <= oldest){
      // One record found
      Uint64 ti = transactionIdRec->u_64_value();
      g_err << "checkNoTransactionsOlderThan found one record" << endl
	    << "  t = " << t << endl
	    << "  a = " << a << endl
	    << "  ti = " << ti << endl;
      found++;
    }
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  
  if (found == 0)
    return NDBT_OK;
  else
    return VERIFICATION_FAILED;
}


 int Bank::performValidatePurged(){
   /**
    * Make sure there are no TRANSACTIONS older than the oldest 
    * purged GL record
    * 
    */

   for (int i = 0; i < getNumAccountTypes(); i++){
     ndbout << "performValidatePurged: " << i << endl;
     Uint64 oldestGlTime; 
     if (getOldestPurgedGL(i, oldestGlTime) != NDBT_OK){
       g_err << "getOldestPurgedGL failed" << endl;
       return NDBT_FAILED;    
     }
     int result = checkNoTransactionsOlderThan(i, oldestGlTime);
     if (result != NDBT_OK){
       g_err << "checkNoTransactionsOlderThan failed" << endl;
       return result;
     }
     
   }
   
   return NDBT_OK;
 }

 int Bank::purgeOldGLTransactions(Uint64 currTime, Uint32 age){
   /** 
    * For each GL record that are older than age and have purged == 0
    *  - delete all TRANSACTIONS belonging to the GL and set purged = 1
    *
    * 
    */
   bool found;
   int count = 0;

   while(1){
     count++;
     if (count > 100)
       return NDBT_OK;
     
     // Search for the oldest GL record with purged == 0
     Uint64 oldestGlTime;
     Uint32 accountTypeId;
     if (getOldestNotPurgedGL(oldestGlTime, accountTypeId, found) != NDBT_OK){
       g_err << "getOldestNotPurgedGL failed" << endl;
       return NDBT_FAILED;
     }


     if (found == false){
       // ndbout << "not found" << endl;
       return NDBT_OK;
     }

     
//      ndbout << "purgeOldGLTransactions" << endl
//      	    << "  oldestGlTime = " << oldestGlTime << endl
//      	    << "  currTime = " << currTime << endl
// 	    << "  age = " << age << endl;
     // Check if this GL is old enough to be purged
     if ((currTime < age) || (oldestGlTime > (currTime-age))){
       //       ndbout << "is not old enough" << endl;
       return NDBT_OK;
     }

     if (purgeTransactions(oldestGlTime, accountTypeId) != NDBT_OK){
       g_err << "purgeTransactions failed" << endl;
       return NDBT_FAILED;
     }
   }
   g_err << "abnormal return" << endl; 
   return NDBT_FAILED;
 }
 

int Bank::purgeTransactions(const Uint64 glTime, 
			    const Uint32 accountTypeId)
{
  int check;
  g_info << "purgeTransactions: " << glTime << ", "<<accountTypeId<<endl;
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }

  // Start by updating the GL record with purged = 1, use NoCommit
  NdbOperation* pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
    
  check = pOp->updateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }
      
  check = pOp->equal("TIME", glTime);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  Uint32 purged = 1;
  check = pOp->setValue("PURGED", purged);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Execute transaction    
  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }    

  // Find all transactions and take over them for delete

  if(findTransactionsToPurge(glTime,
			    accountTypeId,
			    pTrans) != NDBT_OK){
    g_err << "findTransactionToPurge failed" << endl;
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }



  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  } 

  m_ndb.closeTransaction(pTrans);
  return NDBT_OK;
}


int Bank::findTransactionsToPurge(const Uint64 glTime, 
				  const Uint32 accountType,
				  NdbConnection* pTrans){
  int check;
  
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("TRANSACTION");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuplesExclusive();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* timeRec = pOp->getValue("TIME");
  if( timeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);   
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  int rowsFound = 0;
  eof = rs->nextResult();
    
  while(eof == 0){
    rows++;
    Uint64 t = timeRec->u_64_value();
    Uint32 a = accountTypeRec->u_32_value();

    if (a == accountType && t == glTime){
      rowsFound++;
      // One record found
      check = rs->deleteTuple(pTrans);
      if (check == -1){
	ERR(m_ndb.getNdbError());
	m_ndb.closeTransaction(pScanTrans);
	return NDBT_FAILED;
      }
      
      // Execute transaction    
      check = pTrans->execute(NoCommit);
      if( check == -1 ) {
	ERR(pTrans->getNdbError());
	m_ndb.closeTransaction(pScanTrans);
	return NDBT_FAILED;
      }       
    }
    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  //  ndbout << rowsFound << " TRANSACTIONS have been deleted" << endl;

  return NDBT_OK;

}
 
 
 int Bank::performIncreaseTime(int maxSleepBetweenDays, int yield){
  if (init() != NDBT_OK)
    return NDBT_FAILED;

  int yieldCounter = 0;

   while(1){
     
     while(m_ndb.waitUntilReady(10) != 0)
      ndbout << "Waiting for ndb to be ready" << endl;
     
     while(1){
       
       Uint64 currTime;
       if (incCurrTime(currTime) != NDBT_OK)
	 break;

       g_info << "Current time is " << currTime << endl;
       if (maxSleepBetweenDays > 0){
	 int val = myRandom48(maxSleepBetweenDays);
	 NdbSleep_SecSleep(val);
       }

       yieldCounter++;
       if (yield != 0 && yieldCounter >= yield)
	 return NDBT_OK;
       
     }
   }
   return NDBT_FAILED;
 }



int Bank::readSystemValue(SystemValueId sysValId, Uint64 & value){

  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  NdbRecAttr* valueRec = pOp->getValue("VALUE");
  if( valueRec ==NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  value = valueRec->u_64_value();
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;

}

int Bank::writeSystemValue(SystemValueId sysValId, Uint64 value){

  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  check = pOp->insertTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
      
  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->setValue("VALUE", value);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;

}

int Bank::getNextTransactionId(Uint64 &value){
  return increaseSystemValue2(LastTransactionId, value);
}

int Bank::incCurrTime(Uint64 &value){
  return increaseSystemValue(CurrentTime, value);
}

  
int Bank::increaseSystemValue(SystemValueId sysValId, Uint64 &value){
  /**
   * Increase value with one and return
   * updated value
   *
   */

  DBUG_ENTER("Bank::increaseSystemValue");

  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    DBUG_RETURN(NDBT_FAILED);
  }
    
  NdbOperation* pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp->readTupleExclusive();
  //  check = pOp->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  NdbRecAttr* valueRec = pOp->getValue("VALUE");
  if( valueRec ==NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  value = valueRec->u_64_value();
  value++;
    
  NdbOperation* pOp2 = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp2 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp2->updateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp2->equal("SYSTEM_VALUES_ID", sysValId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp2->setValue("VALUE", value);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pTrans->execute(NoCommit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  NdbOperation* pOp3 = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp3 == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp3->readTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }
    
  check = pOp3->equal("SYSTEM_VALUES_ID", sysValId);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  // Read new value
  NdbRecAttr* valueNewRec = pOp3->getValue("VALUE");
  if( valueNewRec ==NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  // Check that value updated equals the value we read after the update
  if (valueNewRec->u_64_value() != value){

    printf("value actual=%lld\n", valueNewRec->u_64_value());
    printf("value expected=%lld actual=%lld\n", value, valueNewRec->u_64_value());

    DBUG_PRINT("info", ("value expected=%ld actual=%ld", value, valueNewRec->u_64_value()));
    g_err << "getNextTransactionId: value was not updated" << endl;
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  m_ndb.closeTransaction(pTrans);

  DBUG_RETURN(0);
}

int Bank::increaseSystemValue2(SystemValueId sysValId, Uint64 &value){
  /**
   * Increase value with one and return
   * updated value
   * A more optimized version using interpreted update!
   *
   */

  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpretedUpdateTuple();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->equal("SYSTEM_VALUES_ID", sysValId );
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  Uint32 valToIncWith = 1;
  check = pOp->incValue("VALUE", valToIncWith);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* valueRec = pOp->getValue("VALUE");
  if( valueRec == NULL ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  value = valueRec->u_64_value();
    
  m_ndb.closeTransaction(pTrans);

  return 0;

}



int Bank::getCurrTime(Uint64 &time){
  return readSystemValue(CurrentTime, time);
}


int Bank::performSumAccounts(int maxSleepBetweenSums, int yield){
  if (init() != NDBT_OK)
    return NDBT_FAILED;
  
  int yieldCounter = 0;

  while (1){

    while (m_ndb.waitUntilReady(10) != 0)
      ndbout << "Waiting for ndb to be ready" << endl;

    Uint32 sumAccounts = 0;
    Uint32 numAccounts = 0;
    if (getSumAccounts(sumAccounts, numAccounts) != NDBT_OK){
      g_err << "getSumAccounts FAILED" << endl;
    } else {
    
      g_info << "num="<<numAccounts<<", sum=" << sumAccounts << endl;
      
      if (sumAccounts != (10000000 + (10000*(numAccounts-1)))){
	g_err << "performSumAccounts  FAILED" << endl
	      << "   sumAccounts="<<sumAccounts<<endl
	      << "   expected   ="<<(10000000 + (10000*(numAccounts-1)))<<endl
	      << "   numAccounts="<<numAccounts<<endl;
	return NDBT_FAILED;
      } 
    
      if (maxSleepBetweenSums > 0){
	int val = myRandom48(maxSleepBetweenSums);
	NdbSleep_MilliSleep(val);      
      }
    }

    yieldCounter++;
    if (yield != 0 && yieldCounter >= yield)
      return NDBT_OK;
  }
  return NDBT_FAILED;
}


int Bank::getSumAccounts(Uint32 &sumAccounts, 
			 Uint32 &numAccounts){

  // SELECT SUM(balance) FROM ACCOUNT

  int check;    
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("ACCOUNT");	
  if (pOp == NULL) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbResultSet * rs = pOp->readTuplesExclusive();
  if( rs == 0 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* balanceRec = pOp->getValue("BALANCE");
  if( balanceRec ==NULL ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit);   
  if( check == -1 ) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    ERR(m_ndb.getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }   

  int eof;
  eof = rs->nextResult();
    
  while(eof == 0){
    Uint32 b = balanceRec->u_32_value();
    
    sumAccounts += b;
    numAccounts++;

    //    ndbout << numAccounts << ": balance =" << b 
    //	   << ", sum="<< sumAccounts << endl;

    // Take over the operation so that the lock is kept in db
    NdbOperation* pLockOp = rs->updateTuple(pTrans);
    if (pLockOp == NULL){
      ERR(m_ndb.getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    Uint32 illegalBalance = 99;
    check = pLockOp->setValue("BALANCE", illegalBalance);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }
    
    // Execute transaction    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }       

    eof = rs->nextResult();
  }
  if (eof == -1) {
    ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // TODO Forget about rolling back, just close pTrans!!

  // Rollback transaction    
  check = pTrans->execute(Rollback);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }       
    
  m_ndb.closeTransaction(pScanTrans);
  m_ndb.closeTransaction(pTrans);


  return NDBT_OK;

}
