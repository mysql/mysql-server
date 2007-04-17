/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
#include <NdbMixRestarter.hpp>

bool disk = false;

#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting, disk, 10) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

/**
 *
 * SR 0 - normal
 * SR 1 - shutdown in progress
 * SR 2 - restart in progress
 */
int 
runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 5; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 
  
  ctx->incProperty(NMR_SR_THREADS);
  while (!ctx->isTestStopped()) 
  {
    Bank bank(ctx->m_cluster_connection);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
    {
      if(bank.performIncreaseTime(wait, yield) == NDBT_FAILED)
	break;
    }
    
    ndbout_c("runBankTimer is stopped");
    ctx->incProperty(NMR_SR_THREADS_STOPPED);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  int wait = 0; // Max ms between each transaction
  int yield = 1; // Loops before bank returns 

  ctx->incProperty(NMR_SR_THREADS);
  while (!ctx->isTestStopped()) 
  {
    Bank bank(ctx->m_cluster_connection);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
      if(bank.performTransactions(0, 1) == NDBT_FAILED)
	break;
    
    ndbout_c("runBankTransactions is stopped");
    ctx->incProperty(NMR_SR_THREADS_STOPPED);
    if(ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  int yield = 1; // Loops before bank returns 
  int result = NDBT_OK;
  
  ctx->incProperty(NMR_SR_THREADS);
  while (ctx->isTestStopped() == false) 
  {
    Bank bank(ctx->m_cluster_connection);
    while(!ctx->isTestStopped() && 
          ctx->getProperty(NMR_SR) <= NdbMixRestarter::SR_STOPPING)
      if (bank.performMakeGLs(yield) != NDBT_OK)
      {
	if(ctx->getProperty(NMR_SR) != NdbMixRestarter::SR_RUNNING)
	  break;
	ndbout << "bank.performMakeGLs FAILED" << endl;
        abort();
	return NDBT_FAILED;
      }
    
    ndbout_c("runBankGL is stopped");
    ctx->incProperty(NMR_SR_THREADS_STOPPED);
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
      abort();
      return NDBT_FAILED;
    }
    
    if (bank.performValidateAllGLs() != 0)
    {
      ndbout << "bank.performValidateAllGLs FAILED" << endl;
      abort();
      return NDBT_FAILED;
    }
    
    ctx->incProperty(NMR_SR_VALIDATE_THREADS_DONE);
    
    if (ctx->getPropertyWait(NMR_SR, NdbMixRestarter::SR_RUNNING))
      break;
  }
  
  return NDBT_OK;
}

#if 0
int runBankSum(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int wait = 2000; // Max ms between each sum of accounts
  int yield = 1; // Loops before bank returns 
  int result = NDBT_OK;

  while (ctx->isTestStopped() == false) 
  {
    if (bank.performSumAccounts(wait, yield) != NDBT_OK){
      ndbout << "bank.performSumAccounts FAILED" << endl;
      result = NDBT_FAILED;
    }
  }
  return result ;
}
#endif


int
runMixRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbMixRestarter res;
  int runtime = ctx->getNumLoops();
  int sleeptime = ctx->getNumRecords();
  Uint32 mask = ctx->getProperty("Type", ~(Uint32)0);
  res.setRestartTypeMask(mask);

  if (res.runPeriod(ctx, step, runtime, sleeptime))
  {
    abort();
    return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

int 
runDropBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
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
  STEP(runBankSrValidator);
  STEP(runMixRestart);
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
  STEP(runMixRestart);
  FINALIZER(runDropBank);
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
  STEP(runMixRestart);
  STEP(runBankSrValidator);
  FINALIZER(runDropBank);
}
NDBT_TESTSUITE_END(testSRBank);

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
  return testSRBank.execute(argc, argv);
}

template class Vector<ndb_mgm_node_state*>;
