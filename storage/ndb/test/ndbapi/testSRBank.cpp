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
    Bank bank(ctx->m_cluster_connection);
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
    Bank bank(ctx->m_cluster_connection);
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
    Bank bank(ctx->m_cluster_connection);
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
  Bank bank(ctx->m_cluster_connection);
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

static
int
restart_cluster(NDBT_Context* ctx, NDBT_Step* step, NdbRestarter& restarter)
{
  bool abort = true;
  int timeout = 180;
  int result = NDBT_OK;

  do 
  {
    ndbout << " -- Shutting down " << endl;
    ctx->setProperty("SR", 1);
    CHECK(restarter.restartAll(false, true, abort) == 0);
    ctx->setProperty("SR", 2);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);
    
    Uint32 cnt = ctx->getProperty("ThreadCount");
    Uint32 curr= ctx->getProperty("ThreadStopped");
    while(curr != cnt && !ctx->isTestStopped())
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
    }
    
    ndbout << " -- Validating complete " << endl;
  } while(0);
  ctx->setProperty("SR", (Uint32)0);
  ctx->broadcast();
  return result;
}

static
ndb_mgm_node_state*
select_node_to_stop(Vector<ndb_mgm_node_state>& nodes)
{
  Uint32 i, j;
  Vector<ndb_mgm_node_state*> alive_nodes;
  for(i = 0; i<nodes.size(); i++)
  {
    ndb_mgm_node_state* node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_STARTED)
      alive_nodes.push_back(node);
  }

  Vector<ndb_mgm_node_state*> victims;
  // Remove those with one in node group
  for(i = 0; i<alive_nodes.size(); i++)
  {
    int group = alive_nodes[i]->node_group;
    for(j = 0; j<alive_nodes.size(); j++) 
    {
      if (i != j && alive_nodes[j]->node_group == group)
      {
	victims.push_back(alive_nodes[i]);
	break;
      }
    }
  }

  if (victims.size())
  {
    int victim = rand() % victims.size();
    return victims[victim];
  }
  else
  {
    return 0;
  }
}

static
ndb_mgm_node_state*
select_node_to_start(Vector<ndb_mgm_node_state>& nodes)
{
  Uint32 i, j;
  Vector<ndb_mgm_node_state*> victims;
  for(i = 0; i<nodes.size(); i++)
  {
    ndb_mgm_node_state* node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_NOT_STARTED)
      victims.push_back(node);
  }

  if (victims.size())
  {
    int victim = rand() % victims.size();
    return victims[victim];
  }
  else
  {
    return 0;
  }
}

enum Action {
  AA_RestartCluster = 0x1,
  AA_RestartNode    = 0x2,
  AA_StopNode       = 0x4,
  AA_StartNode      = 0x8,
  AA_COUNT = 4
};

int
runMixRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int runtime = ctx->getNumLoops();
  int sleeptime = ctx->getNumRecords();
  NdbRestarter restarter;
  int timeout = 180;
  Uint32 type = ctx->getProperty("Type", ~(Uint32)0);
  
  restarter.waitClusterStarted();
  Vector<ndb_mgm_node_state> nodes;
  nodes = restarter.ndbNodes;
#if 0
  for (Uint32 i = 0; i<restarter.ndbNodes.size(); i++)
    nodes.push_back(restarter.ndbNodes[i]);
#endif  

  
  Uint32 now;
  const Uint32 stop = time(0)+ runtime;
  while(!ctx->isTestStopped() && ((now= time(0)) < stop) && result == NDBT_OK)
  {
    ndbout << " -- Sleep " << sleeptime << "s " << endl;
    int cnt = sleeptime;
    while (cnt-- && !ctx->isTestStopped())
      NdbSleep_SecSleep(1);
    if (ctx->isTestStopped())
      return NDBT_FAILED;
    
    ndb_mgm_node_state* node = 0;
    int action;
loop:
    while(((action = (1 << (rand() % AA_COUNT))) & type) == 0);
    switch(action){
    case AA_RestartCluster:
      if (restart_cluster(ctx, step, restarter))
	return NDBT_FAILED;
      for (Uint32 i = 0; i<nodes.size(); i++)
	nodes[i].node_status = NDB_MGM_NODE_STATUS_STARTED;
      break;
    case AA_RestartNode:
    case AA_StopNode:
    {
      if ((node = select_node_to_stop(nodes)) == 0)
	goto loop;
      
      if (action == AA_RestartNode)
	g_err << "Restarting " << node->node_id << endl;
      else
	g_err << "Stopping " << node->node_id << endl;

      if (restarter.restartOneDbNode(node->node_id, false, true, true))
	return NDBT_FAILED;
      
      if (restarter.waitNodesNoStart(&node->node_id, 1))
	return NDBT_FAILED;
      
      node->node_status = NDB_MGM_NODE_STATUS_NOT_STARTED;
      
      if (action == AA_StopNode)
	break;
      else
	goto start;
    }
    case AA_StartNode:
      if ((node = select_node_to_start(nodes)) == 0)
	goto loop;
  start:
      g_err << "Starting " << node->node_id << endl;
      if (restarter.startNodes(&node->node_id, 1))
	return NDBT_FAILED;
      if (restarter.waitNodesStarted(&node->node_id, 1))
	return NDBT_FAILED;
      
      node->node_status = NDB_MGM_NODE_STATUS_STARTED;      
      break;
    }
  }

  Vector<int> not_started;
  {
    ndb_mgm_node_state* node = 0;
    while((node = select_node_to_start(nodes)))
    {
      not_started.push_back(node->node_id);
      node->node_status = NDB_MGM_NODE_STATUS_STARTED;
    }
  }
  
  if (not_started.size())
  {
    g_err << "Starting stopped nodes " << endl;
    if (restarter.startNodes(not_started.getBase(), not_started.size()))
      return NDBT_FAILED;
    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runDropBank(NDBT_Context* ctx, NDBT_Step* step){
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
  TC_PROPERTY("Type", AA_RestartCluster);
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
}
TESTCASE("NR", 
	 " Test that a consistent bank is restored after graceful shutdown\n"
	 "1.  Create bank\n"
	 "2.  Start bank and let it run\n"
	 "3.  Restart ndb and verify consistency\n"
	 "4.  Drop bank\n")
{
  TC_PROPERTY("Type", AA_RestartNode | AA_StopNode | AA_StartNode);
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
  TC_PROPERTY("Type", ~0);
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
