/*
 Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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


#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 


#include "Bank.hpp"

const char* _database = "BANK";

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection, _database);
  int overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection, _database);
  int wait = 30; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performIncreaseTime(wait, yield);
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection, _database);
  int wait = 10; // Max ms between each transaction
  int yield = 100; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performTransactions(wait, yield);
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection, _database);
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
  Bank bank(ctx->m_cluster_connection, _database);
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
  Bank bank(ctx->m_cluster_connection, _database);
  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runBankController(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int l = 0;
  int result = NDBT_OK;

  while (l < loops && result != NDBT_FAILED){

    if (pNdb->waitUntilReady() != 0){
      result = NDBT_FAILED;
      continue;
    }

    // Sleep for a while
    NdbSleep_SecSleep(records);
    
    l++;
  }

  if (pNdb->waitUntilReady() != 0){
    result = NDBT_FAILED;
  }

  ctx->stopTest();

  return result;
}

NDBT_TESTSUITE(testBank);
TESTCASE("Bank", 
	 "Run the bank\n"){
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEP(runBankTransactions);
  STEP(runBankGL);
  // TODO  STEP(runBankSum);
  STEP(runBankController);
  FINALIZER(runDropBank);

}
NDBT_TESTSUITE_END(testBank)

int main(int argc, const char** argv){
  ndb_init();
  // Tables should not be auto created
  NDBT_TESTSUITE_INSTANCE(testBank);
  testBank.setCreateTable(false);

  return testBank.execute(argc, argv);
}


