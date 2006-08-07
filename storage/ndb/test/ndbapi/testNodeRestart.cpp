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
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <Bitmask.hpp>

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(GETNDB(step)) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsertUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTableUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (utilTrans.clearTable(GETNDB(step),  records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  NdbOperation::LockMode lm = 
    (NdbOperation::LockMode)ctx->getProperty("ReadLockMode", 
					     (Uint32)NdbOperation::LM_Read);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    if (hugoTrans.pkReadRecords(GETNDB(step), rows, batch, lm) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    if (hugoTrans.pkUpdateRecords(GETNDB(step), rows, batch) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkReadPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i++ << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    int row = (records - rows) ? rand() % (records - rows) : 0;
    
    int j,k;
    for(j = 0; j<rows; j += batch)
    {
      k = batch;
      if(j+k > rows)
	k = rows - j;
      
      if(hugoOps.startTransaction(pNdb) != 0)
	goto err;
      
      if(hugoOps.pkReadRecord(pNdb, row+j, k, NdbOperation::LM_Exclusive) != 0)
	goto err;

      if(hugoOps.execute_NoCommit(pNdb) != 0)
	goto err;

      if(hugoOps.pkUpdateRecord(pNdb, row+j, k, rand()) != 0)
	goto err;

      if(hugoOps.execute_Commit(pNdb) != 0)
	goto err;

      if(hugoOps.closeTransaction(pNdb) != 0)
	return NDBT_FAILED;
    }
    
    continue;
err:
    NdbConnection* pCon = hugoOps.getTransaction();
    if(pCon == 0)
      continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError){
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;    
  }
  return NDBT_OK;
}

int runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int abort = ctx->getProperty("AbortProb", (Uint32)0);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, abort, 
				    parallelism) == NDBT_FAILED){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runScanReadVerify(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, 64) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int sync_threads = ctx->getProperty("SyncThreads", (unsigned)0);
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }
  
  loops *= restarter.getNumDbNodes();
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId, false, false, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    

    if(restarter.waitClusterStarted(60) != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    ctx->sync_up_and_wait("PauseThreads", sync_threads);

    lastId++;
    i++;
  }

  ctx->stopTest();
  
  return result;
}

int runCheckAllNodesStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if(restarter.waitClusterStarted(1) != 0){
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}



int runRestarts(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NDBT_TestCase* pCase = ctx->getCase();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    
    if(restarts.executeRestart(pCase->getName(), timeout) != 0){
      g_err << "Failed to executeRestart(" <<pCase->getName() <<")" << endl;
      result = NDBT_FAILED;
      break;
    }    
    i++;
  }
  ctx->stopTest();
  return result;
}

int runDirtyRead(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
    
  int i = 0;
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    g_info << i << ": ";

    int id = i % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    restarter.insertErrorInNode(nodeId, 5041);
    restarter.insertErrorInAllNodes(8048 + (i & 1));
    
    for(int j = 0; j<records; j++){
      if(hugoOps.startTransaction(pNdb) != 0)
	return NDBT_FAILED;
      
      if(hugoOps.pkReadRecord(pNdb, j, 1, NdbOperation::LM_CommittedRead) != 0)
	goto err;
      
      int res;
      if((res = hugoOps.execute_Commit(pNdb)) == 4119)
	goto done;
      
      if(res != 0)
	goto err;
      
      if(hugoOps.closeTransaction(pNdb) != 0)
	return NDBT_FAILED;
    }
done:
    if(hugoOps.closeTransaction(pNdb) != 0)
      return NDBT_FAILED;
    
    i++;
    restarter.waitClusterStarted(60) ;
  }
  return result;
err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int runLateCommit(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  int i = 0;
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    g_info << i << ": ";

    if(hugoOps.startTransaction(pNdb) != 0)
      return NDBT_FAILED;
      
    if(hugoOps.pkUpdateRecord(pNdb, 1, 128) != 0)
      return NDBT_FAILED;

    if(hugoOps.execute_NoCommit(pNdb) != 0)
      return NDBT_FAILED;

    Uint32 transNode= hugoOps.getTransaction()->getConnectedNodeId();
    int id = i % restarter.getNumDbNodes();
    int nodeId;
    while((nodeId = restarter.getDbNodeId(id)) == transNode)
      id = (id + 1) % restarter.getNumDbNodes();

    ndbout << "Restart node " << nodeId << endl; 
    
    restarter.restartOneDbNode(nodeId,
			     /** initial */ false, 
			     /** nostart */ true,
			     /** abort   */ true);
    
    restarter.waitNodesNoStart(&nodeId, 1);
    
    int res;
    if(i & 1)
      res= hugoOps.execute_Commit(pNdb);
    else
      res= hugoOps.execute_Rollback(pNdb);
    
    ndbout_c("res= %d", res);
    
    hugoOps.closeTransaction(pNdb);
    
    restarter.startNodes(&nodeId, 1);
    restarter.waitNodesStarted(&nodeId, 1);
    
    if(i & 1)
    {
      if(res != 286)
	return NDBT_FAILED;
    }
    else
    {
      if(res != 0)
	return NDBT_FAILED;
    }
    i++;
  }
  
  return NDBT_OK;
}

int runBug15587(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  
  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[2] = { DumpStateOrd::LqhErrorInsert5042, 0 };
  dump[1] = tableId;

  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl; 
  
  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  
  if (restarter.dumpStateOneNode(nodeId, val2, 2))
    return NDBT_FAILED;

  if (restarter.dumpStateOneNode(nodeId, dump, 2))
    return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;

  restarter.waitNodesStartPhase(&nodeId, 1, 3);
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.dumpStateOneNode(nodeId, val2, 1))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
}

int runBug15632(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  
  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl; 
  
  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.insertErrorInNode(nodeId, 7165))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.insertErrorInNode(nodeId, 7171))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
}

int runBug15685(NDBT_Context* ctx, NDBT_Step* step){

  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), 10) != 0){
    return NDBT_FAILED;
  }

  if(hugoOps.startTransaction(pNdb) != 0)
    goto err;
  
  if(hugoOps.pkUpdateRecord(pNdb, 0, 1, rand()) != 0)
    goto err;

  if(hugoOps.execute_NoCommit(pNdb) != 0)
    goto err;

  if (restarter.insertErrorInAllNodes(5100))
    return NDBT_FAILED;
  
  hugoOps.execute_Rollback(pNdb);

  if (restarter.waitClusterStarted() != 0)
    goto err;

  if (restarter.insertErrorInAllNodes(0))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
  
err:
  ctx->stopTest();
  return NDBT_FAILED;
}

int 
runBug16772(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int aliveNodeId = restarter.getRandomNotMasterNodeId(rand());
  int deadNodeId = aliveNodeId;
  while (deadNodeId == aliveNodeId)
    deadNodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
  
  if (restarter.insertErrorInNode(aliveNodeId, 930))
    return NDBT_FAILED;

  if (restarter.restartOneDbNode(deadNodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&deadNodeId, 1))
    return NDBT_FAILED;

  if (restarter.startNodes(&deadNodeId, 1))
    return NDBT_FAILED;

  // It should now be hanging since we throw away NDB_FAILCONF
  int ret = restarter.waitNodesStartPhase(&deadNodeId, 1, 3, 10);
  // So this should fail...i.e it should not reach startphase 3

  // Now send a NDB_FAILCONF for deadNo
  int dump[] = { 7020, 323, 252, 0 };
  dump[3] = deadNodeId;
  if (restarter.dumpStateOneNode(aliveNodeId, dump, 4))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&deadNodeId, 1))
    return NDBT_FAILED;

  return ret ? NDBT_OK : NDBT_FAILED;
}

int 
runBug18414(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  int loop = 0;
  do 
  {
    if(hugoOps.startTransaction(pNdb) != 0)
      goto err;
    
    if(hugoOps.pkUpdateRecord(pNdb, 0, 128, rand()) != 0)
      goto err;
    
    if(hugoOps.execute_NoCommit(pNdb) != 0)
      goto err;

    int node1 = hugoOps.getTransaction()->getConnectedNodeId();
    int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());
    
    if (node1 == -1 || node2 == -1)
      break;
    
    if (loop & 1)
    {
      if (restarter.insertErrorInNode(node1, 8050))
	goto err;
    }
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (restarter.dumpStateOneNode(node2, val2, 2))
      goto err;
    
    if (restarter.insertErrorInNode(node2, 5003))
      goto err;
    
    int res= hugoOps.execute_Rollback(pNdb);
  
    if (restarter.waitNodesNoStart(&node2, 1) != 0)
      goto err;
    
    if (restarter.insertErrorInAllNodes(0))
      goto err;
    
    if (restarter.startNodes(&node2, 1) != 0)
      goto err;
    
    if (restarter.waitClusterStarted() != 0)
      goto err;
    
    if (hugoTrans.scanUpdateRecords(pNdb, 128) != 0)
      goto err;

    hugoOps.closeTransaction(pNdb);
    
  } while(++loop < 5);
  
  return NDBT_OK;
  
err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;    
}

int 
runBug18612(NDBT_Context* ctx, NDBT_Step* step){

  // Assume two replicas
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Uint32 cnt = restarter.getNumDbNodes();

  for(int loop = 0; loop < ctx->getNumLoops(); loop++)
  {
    int partition0[256];
    int partition1[256];
    bzero(partition0, sizeof(partition0));
    bzero(partition1, sizeof(partition1));
    Bitmask<4> nodesmask;
    
    Uint32 node1 = restarter.getDbNodeId(rand()%cnt);
    for (Uint32 i = 0; i<cnt/2; i++)
    {
      do { 
	int tmp = restarter.getRandomNodeOtherNodeGroup(node1, rand());
	if (tmp == -1)
	{
	  ctx->stopTest();
	  return NDBT_OK;
	}
	node1 = tmp;
      } while(nodesmask.get(node1));
      
      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());
      
      ndbout_c("nodes %d %d", node1, partition1[i]);
      
      assert(!nodesmask.get(node1));
      assert(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    } 
    
    ndbout_c("done");

    int dump[255];
    dump[0] = DumpStateOrd::NdbcntrStopNodes;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);
    
    Uint32 master = restarter.getMasterNodeId();
    
    if (restarter.dumpStateOneNode(master, dump, 1+cnt/2))
      return NDBT_FAILED;
    
    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    
    if (restarter.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInAllNodes(932))
      return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1+cnt/2))
	return NDBT_FAILED;
    
    if (restarter.startNodes(partition0, cnt/2))
      return NDBT_FAILED;
    
    if (restarter.waitNodesStartPhase(partition0, cnt/2, 2))
      return NDBT_FAILED;
    
    dump[0] = 9001;
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateAllNodes(dump, 2))
	return NDBT_FAILED;

    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;
    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.restartOneDbNode(partition0[i], true, true, true))
	return NDBT_FAILED;
    
    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int 
runBug18612SR(NDBT_Context* ctx, NDBT_Step* step){

  // Assume two replicas
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Uint32 cnt = restarter.getNumDbNodes();

  for(int loop = 0; loop < ctx->getNumLoops(); loop++)
  {
    int partition0[256];
    int partition1[256];
    bzero(partition0, sizeof(partition0));
    bzero(partition1, sizeof(partition1));
    Bitmask<4> nodesmask;
    
    Uint32 node1 = restarter.getDbNodeId(rand()%cnt);
    for (Uint32 i = 0; i<cnt/2; i++)
    {
      do { 
	int tmp = restarter.getRandomNodeOtherNodeGroup(node1, rand());
	if (tmp == -1)
	  break;
	node1 = tmp;
      } while(nodesmask.get(node1));
      
      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());
      
      ndbout_c("nodes %d %d", node1, partition1[i]);
      
      assert(!nodesmask.get(node1));
      assert(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    } 
    
    ndbout_c("done");

    if (restarter.restartAll(false, true, false))
      return NDBT_FAILED;

    int dump[255];
    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    
    if (restarter.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInAllNodes(932))
      return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;
    
    if (restarter.waitClusterStartPhase(2))
      return NDBT_FAILED;
    
    dump[0] = 9001;
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateAllNodes(dump, 2))
	return NDBT_FAILED;

    if (restarter.waitClusterNoStart(30))
      if (restarter.waitNodesNoStart(partition0, cnt/2, 10))
	if (restarter.waitNodesNoStart(partition1, cnt/2, 10))
	  return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;
    
    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug20185(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const int masterNode = restarter.getMasterNodeId();

  int dump[] = { 7090, 20 } ;
  if (restarter.dumpStateAllNodes(dump, 2))
    return NDBT_FAILED;
  
  NdbSleep_MilliSleep(3000);
  
retry:
  if(hugoOps.startTransaction(pNdb) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.pkUpdateRecord(pNdb, 1, 1) != 0)
    return NDBT_FAILED;
  
  if (hugoOps.execute_NoCommit(pNdb) != 0)
    return NDBT_FAILED;
  
  const int node = hugoOps.getTransaction()->getConnectedNodeId();
  if (node != masterNode)
  {
    hugoOps.closeTransaction(pNdb);
    goto retry;
  } 
  
  int nodeId;
  do {
    nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
  } while (nodeId == node);
  
  if (restarter.insertErrorInAllNodes(7030))
    return NDBT_FAILED;
  
  if (restarter.insertErrorInNode(nodeId, 7031))
    return NDBT_FAILED;
  
  NdbSleep_MilliSleep(500);
  
  if (hugoOps.execute_Commit(pNdb) == 0)
    return NDBT_FAILED;

  NdbSleep_MilliSleep(3000);

  restarter.waitClusterStarted();
  
  if (restarter.dumpStateAllNodes(dump, 1))
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int 
runBug21271(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const int masterNode = restarter.getMasterNodeId();
  const int nodeId = restarter.getRandomNodeSameNodeGroup(masterNode, rand());

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  if (restarter.dumpStateOneNode(nodeId, val2, 2))
    return NDBT_FAILED;
  
  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[] = { DumpStateOrd::LqhErrorInsert5042, 0, 5044 };
  dump[1] = tableId;

  if (restarter.dumpStateOneNode(nodeId, dump, 3))
    return NDBT_FAILED;
  
  restarter.waitNodesNoStart(&nodeId, 1);
  ctx->stopTest();

  restarter.startNodes(&nodeId, 1);

  if (restarter.waitClusterStarted() != 0)
    return NDBT_FAILED;

  return NDBT_OK;
  return NDBT_OK;
}


NDBT_TESTSUITE(testNodeRestart);
TESTCASE("NoLoad", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "when there are no load on the system. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("PkRead", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadCommitted", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_CommittedRead);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkRead", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", -1);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadPkUpdate", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read and pk update while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkReadPkUpdate", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read and pk update while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", -1);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("ReadUpdateScan", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read, pk update and scan reads while restarting. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedReadUpdateScan", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read, pk update and scan reads while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", -1);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Terror", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform all kind of transactions while restarting. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("FullDb", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "when db is full. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runFillTable);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNode", 
	 "Test that we can execute the restart RestartRandomNode loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeError", 
	 "Test that we can execute the restart RestartRandomNodeError loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeInitial", 
	 "Test that we can execute the restart RestartRandomNodeInitial loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNFDuringNR", 
	 "Test that we can execute the restart RestartNFDuringNR loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartMasterNodeError", 
	 "Test that we can execute the restart RestartMasterNodeError loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}

TESTCASE("TwoNodeFailure", 
	 "Test that we can execute the restart TwoNodeFailure\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("TwoMasterNodeFailure", 
	 "Test that we can execute the restart TwoMasterNodeFailure\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentFail", 
	 "Test that we can execute the restart FiftyPercentFail\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodes", 
	 "Test that we can execute the restart RestartAllNodes\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesAbort", 
	 "Test that we can execute the restart RestartAllNodesAbort\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesError9999", 
	 "Test that we can execute the restart RestartAllNodesError9999\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentStopAndWait", 
	 "Test that we can execute the restart FiftyPercentStopAndWait\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){
  INITIALIZER(runCheckAllNodesStarted); 
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNodeDuringLCP", 
	 "Test that we can execute the restart RestartRandomNode loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("StopOnError", 
	 "Test StopOnError. A node that has StopOnError set to false "\
	 "should restart automatically when an error occurs"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("CommittedRead", 
	 "Test committed read"){ 
  INITIALIZER(runLoadTable);
  STEP(runDirtyRead);
  FINALIZER(runClearTable);
}
TESTCASE("LateCommit",
	 "Test commit after node failure"){
  INITIALIZER(runLoadTable);
  STEP(runLateCommit);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15587",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runBug15587);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15632",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runBug15632);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15685",
	 "Test bug with NF during abort"){
  STEP(runBug15685);
  FINALIZER(runClearTable);
}
TESTCASE("Bug16772",
	 "Test bug with restarting before NF handling is complete"){
  STEP(runBug16772);
}
TESTCASE("Bug18414",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runBug18414);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612",
	 "Test bug with partitioned clusters"){
  INITIALIZER(runLoadTable);
  STEP(runBug18612);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612SR",
	 "Test bug with partitioned clusters"){
  INITIALIZER(runLoadTable);
  STEP(runBug18612SR);
  FINALIZER(runClearTable);
}
TESTCASE("Bug20185",
	 ""){
  INITIALIZER(runLoadTable);
  STEP(runBug20185);
  FINALIZER(runClearTable);
}
TESTCASE("Bug21271",
	 ""){
  INITIALIZER(runLoadTable);
  STEP(runBug21271);
  STEP(runPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testNodeRestart);

int main(int argc, const char** argv){
  ndb_init();
#if 0
  // It might be interesting to have longer defaults for num
  // loops in this test
  // Just performing 100 node restarts would not be enough?
  // We can have initialisers in the NDBT_Testcase class like 
  // this...
  testNodeRestart.setDefaultLoops(1000);
#endif
  return testNodeRestart.execute(argc, argv);
}

