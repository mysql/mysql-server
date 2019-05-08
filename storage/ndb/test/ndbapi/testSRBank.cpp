/*
   Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbBackup.hpp>

#include "bank/Bank.hpp"
#include <NdbMixRestarter.hpp>

bool disk = false;

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting, disk, 10) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

/**
 *
 * SR_RUNNING  (0) - Normal, no failures are allowed.
 * SR_STOPPING (1) - Shutdown in progress, failures should
 *                   be expected/ignored, and operations retried.
 * SR_STOPPED  (2) - restart in progress, halt operations
 *                   until we are SR_RUNNING.
 */
int 
runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 5; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 
  
  while (!ctx->isTestStopped()) 
  {
    Bank bank(ctx->m_cluster_connection);
    ctx->incProperty(NMR_SR_THREADS_ACTIVE);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
    {
      if(bank.performIncreaseTime(wait, yield) == NDBT_FAILED)
      {
        ndbout << "performIncreaseTime FAILED" << endl;
        if (ctx->getProperty(NMR_SR) == NdbMixRestarter::SR_RUNNING)
          return NDBT_FAILED;
        else
          break;  // Possibly retry
      }
    }
    
    ndbout_c("runBankTimer is stopped");
    ctx->decProperty(NMR_SR_THREADS_ACTIVE);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 0; // Max ms between each transaction
  int yield = 1; // Loops before bank returns 

  while (!ctx->isTestStopped()) 
  {
    Bank bank(ctx->m_cluster_connection);
    ctx->incProperty(NMR_SR_THREADS_ACTIVE);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
    {
      if(bank.performTransactions(wait, yield) == NDBT_FAILED)
      {
        ndbout << "performTransactions FAILED" << endl;
        if (ctx->getProperty(NMR_SR) == NdbMixRestarter::SR_RUNNING)
          return NDBT_FAILED;
        else
          break;  // Possibly retry
      }
    }
    ndbout_c("runBankTransactions is stopped");
    ctx->decProperty(NMR_SR_THREADS_ACTIVE);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  int yield = 1; // Loops before bank returns 
  
  while (ctx->isTestStopped() == false) 
  {
    Bank bank(ctx->m_cluster_connection);
    ctx->incProperty(NMR_SR_THREADS_ACTIVE);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
    {
      if (bank.performMakeGLs(yield) == NDBT_FAILED)
      {
        ndbout << "bank.performMakeGLs FAILED" << endl;
        if (ctx->getProperty(NMR_SR) == NdbMixRestarter::SR_RUNNING)
          return NDBT_FAILED;
        else
          break;  // Possibly retry
      }
    }
    ndbout_c("runBankGL is stopped");
    ctx->decProperty(NMR_SR_THREADS_ACTIVE);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int 
runBankSrValidator(NDBT_Context* ctx, NDBT_Step* step)
{
  ctx->incProperty(NMR_SR_VALIDATE_THREADS);

  while(!ctx->isTestStopped())
  {
    if (ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_VALIDATING))
      break;
    
    int wait = 0;
    int yield = 1;
    Bank bank(ctx->m_cluster_connection);
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
    
    ndbout_c("runBankSrValidator is stopped");
    ctx->decProperty(NMR_SR_VALIDATE_THREADS_ACTIVE);
    
    if (ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  
  ctx->decProperty(NMR_SR_VALIDATE_THREADS);
  return NDBT_OK;
}

int runBankSum(NDBT_Context* ctx, NDBT_Step* step)
{
  int wait = 2000; // Max ms between each sum of accounts
  int yield = 1; // Loops before bank returns 

  while (!ctx->isTestStopped()) 
  {
    Bank bank(ctx->m_cluster_connection);
    ctx->incProperty(NMR_SR_THREADS_ACTIVE);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
    {
      if (bank.performSumAccounts(wait, yield) == NDBT_FAILED)
      {
        ndbout << "bank.performSumAccounts FAILED" << endl;
        if (ctx->getProperty(NMR_SR) == NdbMixRestarter::SR_RUNNING)
          return NDBT_FAILED;
        else
          break;  // Possibly retry
      }
    }
    ndbout_c("performSumAccounts is stopped");
    ctx->decProperty(NMR_SR_THREADS_ACTIVE);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int
runMixRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMixRestarter res;
  int runtime = ctx->getNumLoops();
  int sleeptime = ctx->getNumRecords();
  Uint32 mask = ctx->getProperty("Type", ~(Uint32)0);
  res.setRestartTypeMask(mask);

  if (res.runPeriod(ctx, step, runtime, sleeptime))
  {
    return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

/**
 * Verify Bank consisteny after load has been stopped.
 * Then, unconditionaly drop the Bank-DB
 */
int 
runVerifyAndDropBank(NDBT_Context* ctx, NDBT_Step* step)
{
  int wait = 0;
  int yield = 1;
  int result = NDBT_OK;
  Bank bank(ctx->m_cluster_connection);

  if (bank.performSumAccounts(wait, yield) == NDBT_FAILED)
  {
    ndbout << "runVerifyAndDropBank: bank.performSumAccounts FAILED" << endl;
    result = NDBT_FAILED;
  }
  if (bank.performValidateAllGLs() == NDBT_FAILED)
  {
    ndbout << "runVerifyAndDropBank: bank.performValidateAllGLs FAILED" << endl;
    result = NDBT_FAILED;
  }

  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return result;
}


NDBT_TESTSUITE(testSRBank);
TESTCASE("SR", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  TC_PROPERTY("Type", NdbMixRestarter::RTM_SR);
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEPS(runBankTransactions, 10);
  STEP(runBankGL);
  STEP(runBankSum);
  STEP(runBankSrValidator);
  STEP(runMixRestart);
  FINALIZER(runVerifyAndDropBank);
}
TESTCASE("NR", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  TC_PROPERTY("Type", NdbMixRestarter::RTM_NR);
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEPS(runBankTransactions, 10);
  STEP(runBankGL);
  STEP(runBankSum);
  STEP(runMixRestart);
  FINALIZER(runVerifyAndDropBank);
}
TESTCASE("Mix", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  TC_PROPERTY("Type", NdbMixRestarter::RTM_ALL);
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEPS(runBankTransactions, 10);
  STEP(runBankGL);
  STEP(runBankSum);
  STEP(runMixRestart);
  STEP(runBankSrValidator);
  FINALIZER(runVerifyAndDropBank);
}
NDBT_TESTSUITE_END(testSRBank)

int 
main(int argc, const char** argv){
  ndb_init();
  for (int i = 0; i<argc; i++)
  {
    if (strcmp(argv[i], "--disk") == 0)
    {
      argc--;
      disk = true;
      for (; i<argc; i++)
	argv[i] = argv[i+1];
      break;
    }
  } 
  NDBT_TESTSUITE_INSTANCE(testSRBank);
  return testSRBank.execute(argc, argv);
}

