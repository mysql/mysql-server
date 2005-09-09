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
#include <NdbBackup.hpp>

#include "bank/Bank.hpp"

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  int overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting, 10) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

/**
 *
 * SR 0 - normal
 * SR 1 - shutdown in progress
 * SR 2 - restart in progress
 */
int runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 5; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 

  ctx->incProperty("ThreadCount");
  while (!ctx->isTestStopped()) 
  {
    Bank bank;
    while(!ctx->isTestStopped() && ctx->getProperty("SR") <= 1)
      if(bank.performIncreaseTime(wait, yield) == NDBT_FAILED)
	break;

    ndbout_c("runBankTimer is stopped");
    ctx->incProperty("ThreadStopped");
    if(ctx->getPropertyWait("SR", (Uint32)0))
      break;
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 0; // Max ms between each transaction
  int yield = 1; // Loops before bank returns 

  ctx->incProperty("ThreadCount");
  while (!ctx->isTestStopped()) 
  {
    Bank bank;
    while(!ctx->isTestStopped() && ctx->getProperty("SR") <= 1)
      if(bank.performTransactions(0, 1) == NDBT_FAILED)
	break;
    
    ndbout_c("runBankTransactions is stopped");
    ctx->incProperty("ThreadStopped");
    if(ctx->getPropertyWait("SR", (Uint32)0))
      break;
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  int yield = 1; // Loops before bank returns 
  int result = NDBT_OK;

  ctx->incProperty("ThreadCount");
  while (ctx->isTestStopped() == false) 
  {
    Bank bank;
    while(!ctx->isTestStopped() && ctx->getProperty("SR") <= 1)
      if (bank.performMakeGLs(yield) != NDBT_OK)
      {
	if(ctx->getProperty("SR") != 0)
	  break;
	ndbout << "bank.performMakeGLs FAILED" << endl;
	return NDBT_FAILED;
      }
    
    ndbout_c("runBankGL is stopped");
    ctx->incProperty("ThreadStopped");
    if(ctx->getPropertyWait("SR", (Uint32)0))
      break;
  }
  return NDBT_OK;
}

int runBankSum(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
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

#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int runSR(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int runtime = ctx->getNumLoops();
  int sleeptime = ctx->getNumRecords();
  NdbRestarter restarter;
  bool abort = true;
  int timeout = 180;

  Uint32 now;
  const Uint32 stop = time(0)+ runtime;
  while(!ctx->isTestStopped() && ((now= time(0)) < stop) && result == NDBT_OK)
  {
    ndbout << " -- Sleep " << sleeptime << "s " << endl;
    NdbSleep_SecSleep(sleeptime);
    ndbout << " -- Shutting down " << endl;
    ctx->setProperty("SR", 1);
    CHECK(restarter.restartAll(false, true, abort) == 0);
    ctx->setProperty("SR", 2);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);

    Uint32 cnt = ctx->getProperty("ThreadCount");
    Uint32 curr= ctx->getProperty("ThreadStopped");
    while(curr != cnt)
    {
      ndbout_c("%d %d", curr, cnt);
      NdbSleep_MilliSleep(100);
      curr= ctx->getProperty("ThreadStopped");
    }

    ctx->setProperty("ThreadStopped", (Uint32)0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    
    ndbout << " -- Validating starts " << endl;
    {
      int wait = 0;
      int yield = 1;
      Bank bank;
      if (bank.performSumAccounts(wait, yield) != 0)
      {
	ndbout << "bank.performSumAccounts FAILED" << endl;
	return NDBT_FAILED;
      }

      if (bank.performValidateAllGLs() != 0)
      {
	ndbout << "bank.performValidateAllGLs FAILED" << endl;
	return NDBT_FAILED;
      }
    }

    ndbout << " -- Validating complete " << endl;
    ctx->setProperty("SR", (Uint32)0);
    ctx->broadcast();
  }
  ctx->stopTest();
  return NDBT_OK;
}

int runDropBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank;
  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}


NDBT_TESTSUITE(testSRBank);
TESTCASE("Graceful", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankGL);
  STEP(runSR);
}
TESTCASE("Abort", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankGL);
  STEP(runSR);
  FINALIZER(runDropBank);
}
NDBT_TESTSUITE_END(testSRBank);

int main(int argc, const char** argv){
  ndb_init();
  return testSRBank.execute(argc, argv);
}


