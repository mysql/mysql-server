/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "Bank.hpp"
#include <NdbSleep.h>
#include <time.h>
#include <UtilTransactions.hpp>
#include "util/require.h"

Bank::Bank(Ndb_cluster_connection &con, bool _init, const char *dbase)
    : m_ndb(&con, dbase),
      m_maxAccount(-1),
      m_initialized(false),
      m_skip_create(false) {
  if (_init) init();
}

int Bank::init() {
  if (m_initialized == true) return NDBT_OK;

  myRandom48Init((long)NdbTick_CurrentMillisecond());

  m_ndb.init();
  if (m_ndb.waitUntilReady(30) != 0) {
    ndbout << "Ndb not ready" << endl;
    return NDBT_FAILED;
  }

  if (getNumAccounts() != NDBT_OK) return NDBT_FAILED;

  m_initialized = true;
  return NDBT_OK;
}

int Bank::performTransactions(int maxSleepBetweenTrans, int yield) {
  int transactions = 0;

  while (performTransaction() == NDBT_OK) {
    transactions++;

    if (maxSleepBetweenTrans > 0) {
      int val = myRandom48(maxSleepBetweenTrans);
      NdbSleep_MilliSleep(val);
    }

    if ((transactions % 100) == 0) g_info << transactions << endl;

    if (yield != 0 && transactions >= yield) return NDBT_OK;
  }

  return NDBT_FAILED;
}

int Bank::performTransaction() {
  int result = NDBT_OK;

  if (m_maxAccount <= 0) {
    g_err << "No accounts in bank" << endl;
    return NDBT_FAILED;
  }

  int fromAccount = myRandom48(m_maxAccount);
  int toAccount = myRandom48(m_maxAccount);

  if (fromAccount == toAccount) {
    // Increase toAccount with 1
    toAccount = (toAccount + 1) % m_maxAccount;
  }

  int maxAmount = getMaxAmount();

  int amount = myRandom48(maxAmount);

retry_transaction:
  int res = performTransaction(fromAccount, toAccount, amount);
  if (res != NDBT_OK) {
    switch (res) {
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
        NdbSleep_MilliSleep(50);
        goto retry_transaction;
        break;
      default:
        g_info << "performTransaction returned " << res << endl;
        break;
    }
  }
  return result;
}

int Bank::performTransaction(int fromAccountId, int toAccountId, int amount) {
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

int Bank::performTransactionImpl1(int fromAccountId, int toAccountId,
                                  int amount) {
  int check;

  // Ok, all clear to do the transaction
  Uint64 transId;
  int result = NDBT_OK;
  if ((result = getNextTransactionId(transId)) != NDBT_OK) {
    return result;
  }

  NdbConnection *pTrans = m_ndb.startTransaction();

  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  Uint64 currTime;
  if (prepareGetCurrTimeOp(pTrans, currTime) != NDBT_OK) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Check balance on from account
   */
  NdbOperation *pOp = pTrans->getNdbOperation("ACCOUNT");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->readTupleExclusive();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_ID", fromAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *balanceFromRec = pOp->getValue("BALANCE");
  if (balanceFromRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *fromAccountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if (fromAccountTypeRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Read balance on to account
   */
  NdbOperation *pOp6 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp6 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp6->readTupleExclusive();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp6->equal("ACCOUNT_ID", toAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *balanceToRec = pOp6->getValue("BALANCE");
  if (balanceToRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *toAccountTypeRec = pOp6->getValue("ACCOUNT_TYPE");
  if (toAccountTypeRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  Uint32 balanceFrom = balanceFromRec->u_32_value();
  //  ndbout << "balanceFrom: " << balanceFrom << endl;

  if (((Int64)balanceFrom - amount) < 0) {
    m_ndb.closeTransaction(pTrans);
    // ndbout << "Not enough funds" << endl;
    return NOT_ENOUGH_FUNDS;
  }

  Uint32 fromAccountType = fromAccountTypeRec->u_32_value();

  Uint32 balanceTo = balanceToRec->u_32_value();
  //  ndbout << "balanceTo: " << balanceTo << endl;
  Uint32 toAccountType = toAccountTypeRec->u_32_value();

  /**
   * Update balance on from account
   */
  NdbOperation *pOp2 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp2 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp2->updateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp2->equal("ACCOUNT_ID", fromAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp2->setValue("BALANCE", balanceFrom - amount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Update balance on to account
   */
  NdbOperation *pOp3 = pTrans->getNdbOperation("ACCOUNT");
  if (pOp3 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp3->updateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp3->equal("ACCOUNT_ID", toAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp3->setValue("BALANCE", balanceTo + amount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Insert withdrawal transaction
   */
  NdbOperation *pOp4 = pTrans->getNdbOperation("TRANSACTION");
  if (pOp4 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->insertTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->equal("TRANSACTION_ID", transId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->equal("ACCOUNT", fromAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("ACCOUNT_TYPE", fromAccountType);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("OTHER_ACCOUNT", toAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("TRANSACTION_TYPE", WithDrawal);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("TIME", currTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp4->setValue("AMOUNT", amount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  /**
   * Insert deposit transaction
   */
  NdbOperation *pOp5 = pTrans->getNdbOperation("TRANSACTION");
  if (pOp5 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->insertTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->equal("TRANSACTION_ID", transId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->equal("ACCOUNT", toAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("ACCOUNT_TYPE", toAccountType);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("OTHER_ACCOUNT", fromAccountId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("TRANSACTION_TYPE", Deposit);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("TIME", currTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp5->setValue("AMOUNT", amount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pTrans);
  return NDBT_OK;
}

int Bank::performMakeGLs(int yield) {
  int result;

  int counter, maxCounter;
  int yieldCounter = 0;

  while (1) {
    // Counters to keep track of how many
    // GLs should be made before performing a validation
    counter = 0;
    maxCounter = 50 + myRandom48(100);

    /**
     * Validate GLs and Transactions for previous days.
     * Temporary 'validate' errors are ignored as they
     * will be retried in next round anyway.
     *
     */
    result = performValidateGLs();
    if (result != NDBT_OK) {
      if (result == VERIFICATION_FAILED) {
        g_err << "performValidateGLs verification failed" << endl;
        return NDBT_FAILED;
      } else if (result != NDBT_TEMPORARY) {
        g_err << "performValidateGLs failed: " << result << endl;
        return NDBT_FAILED;
      }
      g_info << "performValidateGLs skipped after temporary failure" << endl;
    }

    result = performValidatePurged();
    if (result != NDBT_OK) {
      if (result == VERIFICATION_FAILED) {
        g_err << "performValidatePurged verification failed" << endl;
        return NDBT_FAILED;
      } else if (result != NDBT_TEMPORARY) {
        g_err << "performValidatePurged failed: " << result << endl;
        return NDBT_FAILED;
      }
      g_info << "performValidatePurged skipped after temporary failure" << endl;
    }

    while (1) {
      yieldCounter++;
      if (yield != 0 && yieldCounter >= yield) return NDBT_OK;

      /**
       * Find last GL time.
       * ( GL record with highest time value)
       */
      Uint64 lastGLTime;
      if (findLastGL(lastGLTime) != NDBT_OK) {
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
      if (getCurrTime(currTime) != NDBT_OK) {
        g_info << "getCurrTime failed" << endl;
        // Break out of inner while loop
        break;
      }
      if (lastGLTime < currTime) {
        counter++;
        if (performMakeGL(lastGLTime) != NDBT_OK) {
          g_info << "performMakeGL failed" << endl;
          // Break out of inner while loop
          break;
        }

        if (counter > maxCounter) {
          // Break out of inner while loop and
          // validatePreviousGLs
          g_info << "counter(" << counter << ") > maxCounter(" << maxCounter
                 << ")" << endl;
          break;
        }

      } else {
        ;  // ndbout << "It's not time to make GL yet" << endl;

        // ndbout << "Sleeping 1 second" << endl;
        NdbSleep_SecSleep(1);
      }

      Uint32 age = 3;
      if (purgeOldGLTransactions(currTime, age) != NDBT_OK) {
        g_info << "purgeOldGLTransactions failed" << endl;
        // Break out of inner while loop
        break;
      }
    }
  }

  return NDBT_FAILED;
}

int Bank::performValidateAllGLs() {
  int result;

  while (1) {
    /**
     * Validate GLs and Transactions for previous days
     * Set age so that ALL GL's are validated.
     *
     * If we encounter a temporary failure, verification
     * is retried. Other errors are thrown as real errors.
     */
    int age = 100000;
    result = performValidateGLs(age);
    if (result != NDBT_OK) {
      if (result == NDBT_TEMPORARY) {
        g_info << "performValidateGLs, retry after temporary failure" << endl;
        continue;
      }
      if (result == VERIFICATION_FAILED)
        g_err << "performValidateGLs verification failed" << endl;
      else
        g_err << "performValidateGLs failed: " << result << endl;
      return NDBT_FAILED;
    }

    /**
     *
     *
     */
    result = performValidatePurged();
    if (result != NDBT_OK) {
      if (result == NDBT_TEMPORARY) {
        g_info << "performValidatePurged, retry after temporary failure"
               << endl;
        continue;
      }
      if (result == VERIFICATION_FAILED)
        g_err << "performValidatePurged verification failed" << endl;
      else
        g_err << "performValidatePurged failed: " << result << endl;
      return NDBT_FAILED;
    }
    return NDBT_OK;
  }

  return NDBT_FAILED;
}

int Bank::findLastGL(Uint64 &lastTime) {
  int check;
  /**
   * SELECT MAX(time) FROM GL
   */
  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuples()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *timeRec = pOp->getValue("TIME");
  if (timeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  eof = pOp->nextResult();
  lastTime = 0;

  while (eof == 0) {
    rows++;
    Uint64 t = timeRec->u_32_value();

    if (t > lastTime) lastTime = t;

    eof = pOp->nextResult();
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);

  return NDBT_OK;
}

int Bank::performMakeGL(Uint64 time) {
  g_info << "performMakeGL: " << time << endl;
  /**
   *  Create one GL record for each account type.
   *  All in the same transaction
   */
  // Start transaction
  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }
  for (int i = 0; i < getNumAccountTypes(); i++) {
    if (performMakeGLForAccountType(pTrans, time, i) != NDBT_OK) {
      g_err << "performMakeGLForAccountType returned NDBT_FAILED" << endl;
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  }
  // Execute transaction
  if (pTrans->execute(Commit, AbortOnError) == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }
  m_ndb.closeTransaction(pTrans);

  return NDBT_OK;
}

int Bank::performMakeGLForAccountType(NdbConnection *pTrans, Uint64 glTime,
                                      Uint32 accountTypeId) {
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
  NdbOperation *pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->insertTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("TIME", glTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("BALANCE", balance);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("DEPOSIT_COUNT", depositCount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("DEPOSIT_SUM", depositSum);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("WITHDRAWAL_COUNT", withdrawalCount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("WITHDRAWAL_SUM", withdrawalSum);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->setValue("PURGED", purged);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pOp->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  // Read previous GL record to get old balance
  NdbOperation *pOp2 = pTrans->getNdbOperation("GL");
  if (pOp2 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp2->readTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp2->equal("TIME", glTime - 1);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp2->equal("ACCOUNT_TYPE", accountTypeId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  NdbRecAttr *oldBalanceRec = pOp2->getValue("BALANCE");
  if (oldBalanceRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pOp2->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  Uint32 oldBalance = oldBalanceRec->u_32_value();
  //  ndbout << "oldBalance = "<<oldBalance<<endl;
  balance = oldBalance;
  // Start a scan transaction to search
  // for TRANSACTION records with TIME = time
  // and ACCOUNT_TYPE = accountTypeId
  // Build sum of all found transactions

  if (sumTransactionsForGL(glTime, accountTypeId, balance, withdrawalCount,
                           withdrawalSum, depositSum, depositCount,
                           countTransactions, pTrans) != NDBT_OK) {
    return NDBT_FAILED;
  }
  //  ndbout << "sumTransactionsForGL completed" << endl;
  //  ndbout << "balance="<<balance<<endl
  //	 << "withdrawalCount="<<withdrawalCount<<endl
  //	 << "withdrawalSum="<<withdrawalSum<<endl
  //	 << "depositCount="<<depositCount<<endl
  //	 << "depositSum="<<depositSum<<endl;

  NdbOperation *pOp3 = pTrans->getNdbOperation("GL");
  if (pOp3 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->updateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->equal("TIME", glTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->equal("ACCOUNT_TYPE", accountTypeId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("BALANCE", balance);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("DEPOSIT_COUNT", depositCount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("DEPOSIT_SUM", depositSum);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("WITHDRAWAL_COUNT", withdrawalCount);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("WITHDRAWAL_SUM", withdrawalSum);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp3->setValue("PURGED", purged);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Execute transaction
  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int Bank::sumTransactionsForGL(const Uint64 glTime, const Uint32 accountType,
                               Uint32 &balance, Uint32 &withdrawalCount,
                               Uint32 &withdrawalSum, Uint32 &depositSum,
                               Uint32 &depositCount, Uint32 &transactionsCount,
                               NdbConnection *pTrans) {
  int check;

  //  g_info << "sumTransactionsForGL: " << glTime << ", " << accountType <<
  //  endl;

  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("TRANSACTION");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuplesExclusive()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if (accountTypeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *timeRec = pOp->getValue("TIME");
  if (timeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *transTypeRec = pOp->getValue("TRANSACTION_TYPE");
  if (transTypeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *amountRec = pOp->getValue("AMOUNT");
  if (amountRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  int rowsFound = 0;
  eof = pOp->nextResult();

  while (eof == 0) {
    rows++;
    Uint32 a = accountTypeRec->u_32_value();
    Uint64 t = timeRec->u_64_value();

    if (a == accountType && t == glTime) {
      rowsFound++;
      // One record found
      int transType = transTypeRec->u_32_value();
      int amount = amountRec->u_32_value();
      if (transType == WithDrawal) {
        withdrawalCount++;
        withdrawalSum += amount;
        balance -= amount;
      } else {
        require(transType == Deposit);
        depositCount++;
        depositSum += amount;
        balance += amount;
      }
    }

    eof = pOp->nextResult();

    if ((rows % 100) == 0) {
      // "refresh" owner transaction every 100th row
      if (pTrans->refresh() == -1) {
        const NdbError err = pTrans->getNdbError();
        if (err.status == NdbError::TemporaryError) {
          NDB_ERR(err);
          return NDBT_TEMPORARY;
        }
        NDB_ERR(err)
        return NDBT_FAILED;
      }
    }
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);
  //  ndbout << rows << " TRANSACTIONS have been read" << endl;
  transactionsCount = rowsFound;

  return NDBT_OK;
}

int Bank::performValidateGLs(Uint64 age) {
  Uint64 currTime;
  if (getCurrTime(currTime) != NDBT_OK) {
    return NDBT_FAILED;
  }
  Uint64 glTime = currTime - 1;
  while ((glTime > 0) && ((glTime + age) >= currTime)) {
    int result = performValidateGL(glTime);
    if (result != NDBT_OK) {
      g_err << "performValidateGL failed: " << result << endl;
      return result;
    }

    glTime--;
  }

  return NDBT_OK;
}

int Bank::performValidateGL(Uint64 glTime) {
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
  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuples()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if (accountTypeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *timeRec = pOp->getValue("TIME");
  if (timeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *purgedRec = pOp->getValue("PURGED");
  if (purgedRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *balanceRec = pOp->getValue("BALANCE");
  if (balanceRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *depositSumRec = pOp->getValue("DEPOSIT_SUM");
  if (depositSumRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *depositCountRec = pOp->getValue("DEPOSIT_COUNT");
  if (depositCountRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *withdrawalSumRec = pOp->getValue("WITHDRAWAL_SUM");
  if (withdrawalSumRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
  NdbRecAttr *withdrawalCountRec = pOp->getValue("WITHDRAWAL_COUNT");
  if (withdrawalCountRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  int countGlRecords = 0;
  int result = NDBT_OK;
  eof = pOp->nextResult();

  while (eof == 0) {
    rows++;
    Uint64 t = timeRec->u_64_value();

    if (t == glTime) {
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
      if (purged == 0) {
        // If purged == 0, then the TRANSACTION table should be checked
        // to see that there are:
        // + DEPOSIT_COUNT deposit transactions with account_type ==
        // ACCOUNT_TYPE
        //   and TIME == glTime. The sum of these transactions should be
        //   DEPOSIT_SUM
        // + WITHDRAWAL_COUNT withdrawal transactions with account_type ==
        //   ACCOUNT_TYPE and TIME == glTime. The sum of these transactions
        //   should be WITHDRAWAL_SUM
        // + BALANCE should be equal to the sum of all transactions plus
        //   the balance of the previous GL record
        if (sumTransactionsForGL(t, a, balance, withdrawalCount, withdrawalSum,
                                 depositSum, depositCount, countTransactions,
                                 pScanTrans) != NDBT_OK) {
          result = NDBT_FAILED;
        } else {
          Uint32 prevBalance = 0;
          if (getBalanceForGL(t - 1, a, prevBalance) != NDBT_OK) {
            result = NDBT_FAILED;
          } else if (((prevBalance + balance) != b) ||
                     (wsum != withdrawalSum) || (wcount != withdrawalCount) ||
                     (dsum != depositSum) || (dcount != depositCount)) {
            g_err << "performValidateGL, sums and counts failed" << endl
                  << "balance   :   " << balance + prevBalance << "!=" << b
                  << endl
                  << "with sum  :   " << withdrawalSum << "!=" << wsum << endl
                  << "with count:   " << withdrawalCount << "!=" << wcount
                  << endl
                  << "dep sum   :   " << depositSum << "!=" << dsum << endl
                  << "dep count :   " << depositCount << "!=" << dcount << endl;
            result = VERIFICATION_FAILED;
          }
        }

      } else {
        require(purged == 1);
        // If purged == 1 then there should be NO transactions with
        // TIME == glTime and ACCOUNT_TYPE == account_type

        if (sumTransactionsForGL(t, a, balance, withdrawalCount, withdrawalSum,
                                 depositSum, depositCount, countTransactions,
                                 pScanTrans) != NDBT_OK) {
          result = NDBT_FAILED;
        } else {
          if (countTransactions != 0) {
            g_err << "performValidateGL, countTransactions("
                  << countTransactions << ") != 0" << endl;
            result = VERIFICATION_FAILED;
          }
        }
      }
    }
    eof = pOp->nextResult();
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);

  // - There should be zero or NoAccountTypes GL records for each glTime
  if ((countGlRecords != 0) && (countGlRecords != getNumAccountTypes())) {
    g_err << "performValidateGL: " << endl
          << "countGlRecords = " << countGlRecords << endl;
    result = VERIFICATION_FAILED;
  }

  return result;
}

int Bank::getBalanceForGL(const Uint64 glTime, const Uint32 accountTypeId,
                          Uint32 &balance) {
  int check;

  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbOperation *pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->readTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("TIME", glTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  NdbRecAttr *balanceRec = pOp->getValue("BALANCE");
  if (balanceRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pTrans);

  balance = balanceRec->u_32_value();

  return NDBT_OK;
}

int Bank::getOldestPurgedGL(const Uint32 accountType, Uint64 &oldest) {
  int check;
  /**
   * SELECT MAX(time) FROM GL WHERE account_type = @accountType and purged=1
   */
  NdbConnection *pScanTrans = 0;
  do {
    pScanTrans = m_ndb.startTransaction();
    if (pScanTrans == NULL) {
      const NdbError err = m_ndb.getNdbError();
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        return NDBT_TEMPORARY;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("GL");
    if (pOp == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    if (pOp->readTuples()) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
    if (accountTypeRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *timeRec = pOp->getValue("TIME");
    if (timeRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *purgedRec = pOp->getValue("PURGED");
    if (purgedRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    check = pScanTrans->execute(NoCommit, AbortOnError);
    if (check == -1) {
      const NdbError err = pScanTrans->getNdbError();
      m_ndb.closeTransaction(pScanTrans);
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    eof = pOp->nextResult();
    oldest = 0;

    while (eof == 0) {
      rows++;
      Uint32 a = accountTypeRec->u_32_value();
      Uint32 p = purgedRec->u_32_value();

      if (a == accountType && p == 1) {
        // One record found
        Uint64 t = timeRec->u_64_value();
        if (t > oldest) oldest = t;
      }
      eof = pOp->nextResult();
    }
    if (eof == -1) {
      const NdbError err = pScanTrans->getNdbError();
      m_ndb.closeTransaction(pScanTrans);

      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }
    break;
  } while (true);

  m_ndb.closeTransaction(pScanTrans);

  return NDBT_OK;
}

int Bank::getOldestNotPurgedGL(Uint64 &oldest, Uint32 &accountTypeId,
                               bool &found) {
  int check;
  /**
   * SELECT time, accountTypeId FROM GL
   * WHERE purged=0 order by time asc
   */
  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuples()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if (accountTypeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *timeRec = pOp->getValue("TIME");
  if (timeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *purgedRec = pOp->getValue("PURGED");
  if (purgedRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  eof = pOp->nextResult();
  oldest = (Uint64)-1;
  found = false;

  while (eof == 0) {
    rows++;
    Uint32 p = purgedRec->u_32_value();
    if (p == 0) {
      found = true;
      // One record found
      Uint32 a = accountTypeRec->u_32_value();
      Uint64 t = timeRec->u_64_value();
      if (t < oldest) {
        oldest = t;
        accountTypeId = a;
      }
    }
    eof = pOp->nextResult();
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);

  return NDBT_OK;
}

int Bank::checkNoTransactionsOlderThan(const Uint32 accountType,
                                       const Uint64 oldest) {
  /**
   * SELECT COUNT(transaction_id) FROM TRANSACTION
   * WHERE account_type = @accountType and time <= @oldest
   *
   */

  int loop = 0;
  int found = 0;
  NdbConnection *pScanTrans = 0;
  do {
    int check;
    loop++;
    pScanTrans = m_ndb.startTransaction();
    if (pScanTrans == NULL) {
      const NdbError err = m_ndb.getNdbError();
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        return NDBT_TEMPORARY;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("TRANSACTION");
    if (pOp == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    if (pOp->readTuples()) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
    if (accountTypeRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *timeRec = pOp->getValue("TIME");
    if (timeRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    NdbRecAttr *transactionIdRec = pOp->getValue("TRANSACTION_ID");
    if (transactionIdRec == NULL) {
      NDB_ERR(pScanTrans->getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    check = pScanTrans->execute(NoCommit, AbortOnError);
    if (check == -1) {
      const NdbError err = pScanTrans->getNdbError();
      m_ndb.closeTransaction(pScanTrans);

      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    found = 0;
    eof = pOp->nextResult();

    while (eof == 0) {
      rows++;
      Uint32 a = accountTypeRec->u_32_value();
      Uint32 t = timeRec->u_32_value();

      if (a == accountType && t <= oldest) {
        // One record found
        Uint64 ti = transactionIdRec->u_64_value();
        g_err << "checkNoTransactionsOlderThan found one record" << endl
              << "  t = " << t << endl
              << "  a = " << a << endl
              << "  ti = " << ti << endl;
        found++;
      }
      eof = pOp->nextResult();
    }
    if (eof == -1) {
      const NdbError err = pScanTrans->getNdbError();
      m_ndb.closeTransaction(pScanTrans);

      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }

      NDB_ERR(err);
      return NDBT_FAILED;
    }

    break;
  } while (true);

  m_ndb.closeTransaction(pScanTrans);

  if (found == 0)
    return NDBT_OK;
  else
    return VERIFICATION_FAILED;
}

int Bank::performValidatePurged() {
  /**
   * Make sure there are no TRANSACTIONS older than the oldest
   * purged GL record
   *
   */

  for (int i = 0; i < getNumAccountTypes(); i++) {
    Uint64 oldestGlTime;
    int result = getOldestPurgedGL(i, oldestGlTime);
    if (result != NDBT_OK) {
      g_err << "getOldestPurgedGL failed" << endl;
      return result;
    }
    result = checkNoTransactionsOlderThan(i, oldestGlTime);
    if (result != NDBT_OK) {
      g_err << "checkNoTransactionsOlderThan failed" << endl;
      return result;
    }
  }

  return NDBT_OK;
}

int Bank::purgeOldGLTransactions(Uint64 currTime, Uint32 age) {
  /**
   * For each GL record that are older than age and have purged == 0
   *  - delete all TRANSACTIONS belonging to the GL and set purged = 1
   *
   *
   */
  bool found;
  int count = 0;

  while (1) {
    count++;
    if (count > 100) return NDBT_OK;

    // Search for the oldest GL record with purged == 0
    Uint64 oldestGlTime;
    Uint32 accountTypeId;
    if (getOldestNotPurgedGL(oldestGlTime, accountTypeId, found) != NDBT_OK) {
      g_err << "getOldestNotPurgedGL failed" << endl;
      return NDBT_FAILED;
    }

    if (found == false) {
      // ndbout << "not found" << endl;
      return NDBT_OK;
    }

    //      ndbout << "purgeOldGLTransactions" << endl
    //      	    << "  oldestGlTime = " << oldestGlTime << endl
    //      	    << "  currTime = " << currTime << endl
    // 	    << "  age = " << age << endl;
    // Check if this GL is old enough to be purged
    if ((currTime < age) || (oldestGlTime > (currTime - age))) {
      //       ndbout << "is not old enough" << endl;
      return NDBT_OK;
    }

    if (purgeTransactions(oldestGlTime, accountTypeId) != NDBT_OK) {
      g_err << "purgeTransactions failed" << endl;
      return NDBT_FAILED;
    }
  }
  g_err << "abnormal return" << endl;
  return NDBT_FAILED;
}

int Bank::purgeTransactions(const Uint64 glTime, const Uint32 accountTypeId) {
  int check;
  g_info << "purgeTransactions: " << glTime << ", " << accountTypeId << endl;
  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  // Start by updating the GL record with purged = 1, use NoCommit
  NdbOperation *pOp = pTrans->getNdbOperation("GL");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->updateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("TIME", glTime);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  check = pOp->equal("ACCOUNT_TYPE", accountTypeId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  Uint32 purged = 1;
  check = pOp->setValue("PURGED", purged);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Execute transaction
  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  // Find all transactions and take over them for delete

  if (findTransactionsToPurge(glTime, accountTypeId, pTrans) != NDBT_OK) {
    g_err << "findTransactionToPurge failed" << endl;
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pTrans);
  return NDBT_OK;
}

int Bank::findTransactionsToPurge(const Uint64 glTime, const Uint32 accountType,
                                  NdbConnection *pTrans) {
  int check;

  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("TRANSACTION");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuplesExclusive()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *timeRec = pOp->getValue("TIME");
  if (timeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if (accountTypeRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  int rowsFound = 0;
  eof = pOp->nextResult();

  while (eof == 0) {
    rows++;
    Uint64 t = timeRec->u_64_value();
    Uint32 a = accountTypeRec->u_32_value();

    if (a == accountType && t == glTime) {
      rowsFound++;
      // One record found
      check = pOp->deleteCurrentTuple(pTrans);
      if (check == -1) {
        NDB_ERR(m_ndb.getNdbError());
        m_ndb.closeTransaction(pScanTrans);
        return NDBT_FAILED;
      }

      // Execute transaction
      check = pTrans->execute(NoCommit, AbortOnError);
      if (check == -1) {
        const NdbError err = pTrans->getNdbError();
        m_ndb.closeTransaction(pScanTrans);
        if (err.status == NdbError::TemporaryError) {
          NDB_ERR(err);
          return NDBT_TEMPORARY;
        }
        NDB_ERR(err);
        return NDBT_FAILED;
      }
    }
    eof = pOp->nextResult();
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);
  //  ndbout << rowsFound << " TRANSACTIONS have been deleted" << endl;

  return NDBT_OK;
}

int Bank::performIncreaseTime(int maxSleepBetweenDays, int yield) {
  int yieldCounter = 0;

  while (1) {
    Uint64 currTime;
    const int res = incCurrTime(currTime);
    if (res == NDBT_FAILED) break;
    if (res == NDBT_TEMPORARY)  // Retry
    {
      NdbSleep_MilliSleep(50);
      continue;
    }

    g_info << "Current time is " << currTime << endl;
    if (maxSleepBetweenDays > 0) {
      int val = myRandom48(maxSleepBetweenDays);
      NdbSleep_SecSleep(val);
    }

    yieldCounter++;
    if (yield != 0 && yieldCounter >= yield) return NDBT_OK;
  }
  return NDBT_FAILED;
}

int Bank::readSystemValue(SystemValueId sysValId, Uint64 &value) {
  int check;
  NdbConnection *pTrans = 0;
  while (true) {
    pTrans = m_ndb.startTransaction();
    if (pTrans == NULL) {
      const NdbError err = m_ndb.getNdbError();
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    int result;
    if ((result = prepareReadSystemValueOp(pTrans, sysValId, value)) !=
        NDBT_OK) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return result;
    }

    check = pTrans->execute(Commit, AbortOnError);
    if (check == -1) {
      const NdbError err = pTrans->getNdbError();
      m_ndb.closeTransaction(pTrans);
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    break;
  }

  m_ndb.closeTransaction(pTrans);
  return NDBT_OK;
}

int Bank::prepareReadSystemValueOp(NdbConnection *pTrans,
                                   SystemValueId sysValId, Uint64 &value) {
  int check;

  NdbOperation *pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    return NDBT_FAILED;
  }

  check = pOp->readTuple();
  if (check == -1) {
    return NDBT_FAILED;
  }

  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    return NDBT_FAILED;
  }

  NdbRecAttr *valueRec = pOp->getValue("VALUE", (char *)&value);
  if (valueRec == NULL) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int Bank::writeSystemValue(SystemValueId sysValId, Uint64 value) {
  int check;

  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbOperation *pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->insertTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->setValue("VALUE", value);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pTrans);
  return NDBT_OK;
}

int Bank::getNextTransactionId(Uint64 &value) {
  return increaseSystemValue2(LastTransactionId, value);
}

int Bank::incCurrTime(Uint64 &value) {
  return increaseSystemValue(CurrentTime, value);
}

int Bank::increaseSystemValue(SystemValueId sysValId, Uint64 &value) {
  /**
   * Increase value with one and return
   * updated value
   *
   */

  DBUG_ENTER("Bank::increaseSystemValue");

  int check;

  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      DBUG_RETURN(NDBT_TEMPORARY);
    }
    NDB_ERR(err);
    DBUG_RETURN(NDBT_FAILED);
  }

  NdbOperation *pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp->readTupleExclusive();
  //  check = pOp->readTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  NdbRecAttr *valueRec = pOp->getValue("VALUE");
  if (valueRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      DBUG_RETURN(NDBT_TEMPORARY);
    }
    NDB_ERR(err);
    DBUG_RETURN(NDBT_FAILED);
  }

  value = valueRec->u_64_value();
  value++;

  NdbOperation *pOp2 = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp2 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp2->updateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp2->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp2->setValue("VALUE", value);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      DBUG_RETURN(NDBT_TEMPORARY);
    }
    NDB_ERR(err);
    DBUG_RETURN(NDBT_FAILED);
  }

  NdbOperation *pOp3 = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp3 == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp3->readTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pOp3->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  // Read new value
  NdbRecAttr *valueNewRec = pOp3->getValue("VALUE");
  if (valueNewRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      DBUG_RETURN(NDBT_TEMPORARY);
    }
    NDB_ERR(err);
    DBUG_RETURN(NDBT_FAILED);
  }

  // Check that value updated equals the value we read after the update
  if (valueNewRec->u_64_value() != value) {
    printf("value actual=%lld\n", valueNewRec->u_64_value());
    printf("value expected=%lld actual=%lld\n", value,
           valueNewRec->u_64_value());

    DBUG_PRINT("info", ("value expected=%ld actual=%ld", (long)value,
                        (long)valueNewRec->u_64_value()));
    g_err << "getNextTransactionId: value was not updated" << endl;
    m_ndb.closeTransaction(pTrans);
    DBUG_RETURN(NDBT_FAILED);
  }

  m_ndb.closeTransaction(pTrans);

  DBUG_RETURN(0);
}

int Bank::increaseSystemValue2(SystemValueId sysValId, Uint64 &value) {
  /**
   * Increase value with one and return
   * updated value
   * A more optimized version using interpreted update!
   *
   */

  int check;

  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbOperation *pOp = pTrans->getNdbOperation("SYSTEM_VALUES");
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->interpretedUpdateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pOp->equal("SYSTEM_VALUES_ID", sysValId);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  Uint32 valToIncWith = 1;
  check = pOp->incValue("VALUE", valToIncWith);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *valueRec = pOp->getValue("VALUE");
  if (valueRec == NULL) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit, AbortOnError);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  value = valueRec->u_64_value();

  m_ndb.closeTransaction(pTrans);

  return 0;
}

int Bank::getCurrTime(Uint64 &time) {
  return readSystemValue(CurrentTime, time);
}

int Bank::prepareGetCurrTimeOp(NdbConnection *pTrans, Uint64 &time) {
  return prepareReadSystemValueOp(pTrans, CurrentTime, time);
}

int Bank::performSumAccounts(int maxSleepBetweenSums, int yield) {
  int yieldCounter = 0;

  while (1) {
    Uint32 sumAccounts = 0;
    Uint32 numAccounts = 0;
    const int result = getSumAccounts(sumAccounts, numAccounts);
    if (result != NDBT_OK) {
      if (result == NDBT_TEMPORARY) {
        g_info << "getSumAccounts, retry after temporary failure" << endl;
        continue;
      }
      g_err << "getSumAccounts FAILED" << endl;
      return NDBT_FAILED;
    } else {
      g_info << "num=" << numAccounts << ", sum=" << sumAccounts << endl;

      if (sumAccounts != (10000000 + (10000 * (numAccounts - 1)))) {
        g_err << "performSumAccounts  FAILED" << endl
              << "   sumAccounts=" << sumAccounts << endl
              << "   expected   =" << (10000000 + (10000 * (numAccounts - 1)))
              << endl
              << "   numAccounts=" << numAccounts << endl;
        return NDBT_FAILED;
      }

      if (maxSleepBetweenSums > 0) {
        int val = myRandom48(maxSleepBetweenSums);
        NdbSleep_MilliSleep(val);
      }
    }

    yieldCounter++;
    if (yield != 0 && yieldCounter >= yield) return NDBT_OK;
  }
  return NDBT_FAILED;
}

int Bank::getSumAccounts(Uint32 &sumAccounts, Uint32 &numAccounts) {
  // SELECT SUM(balance) FROM ACCOUNT

  int check;
  NdbConnection *pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pScanTrans->getNdbScanOperation("ACCOUNT");
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuplesExclusive()) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr *balanceRec = pOp->getValue("BALANCE");
  if (balanceRec == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if (check == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  NdbConnection *pTrans = m_ndb.startTransaction();
  if (pTrans == NULL) {
    const NdbError err = m_ndb.getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  int eof;
  eof = pOp->nextResult();

  while (eof == 0) {
    Uint32 b = balanceRec->u_32_value();

    sumAccounts += b;
    numAccounts++;

    //    ndbout << numAccounts << ": balance =" << b
    //	   << ", sum="<< sumAccounts << endl;

    // Take over the operation so that the lock is kept in db
    NdbOperation *pLockOp = pOp->updateCurrentTuple(pTrans);
    if (pLockOp == NULL) {
      NDB_ERR(m_ndb.getNdbError());
      m_ndb.closeTransaction(pScanTrans);
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    Uint32 illegalBalance = 99;
    check = pLockOp->setValue("BALANCE", illegalBalance);
    if (check == -1) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      m_ndb.closeTransaction(pScanTrans);
      return NDBT_FAILED;
    }

    // Execute transaction
    check = pTrans->execute(NoCommit, AbortOnError);
    if (check == -1) {
      const NdbError err = pTrans->getNdbError();
      m_ndb.closeTransaction(pScanTrans);
      m_ndb.closeTransaction(pTrans);
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        return NDBT_TEMPORARY;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    eof = pOp->nextResult();
  }
  if (eof == -1) {
    const NdbError err = pScanTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  // TODO Forget about rolling back, just close pTrans!!

  // Rollback transaction
  check = pTrans->execute(Rollback);
  if (check == -1) {
    const NdbError err = pTrans->getNdbError();
    m_ndb.closeTransaction(pScanTrans);
    m_ndb.closeTransaction(pTrans);
    if (err.status == NdbError::TemporaryError) {
      NDB_ERR(err);
      return NDBT_TEMPORARY;
    }
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  m_ndb.closeTransaction(pScanTrans);
  m_ndb.closeTransaction(pTrans);

  return NDBT_OK;
}
