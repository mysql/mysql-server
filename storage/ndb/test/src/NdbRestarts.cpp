/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbRestarts.hpp>
#include <NDBT.hpp>
#include <string.h>
#include <NdbSleep.h>
#include <kernel/ndb_limits.h>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbEnv.h>
#include <NDBT_Test.hpp>

#define F_ARGS NDBT_Context* ctx, NdbRestarter& _restarter, const NdbRestarts::NdbRestart* _restart

int restartRandomNodeGraceful(F_ARGS);
int restartRandomNodeAbort(F_ARGS);
int restartRandomNodeError(F_ARGS);
int restartRandomNodeInitial(F_ARGS);
int restartNFDuringNR(F_ARGS);
int restartMasterNodeError(F_ARGS);
int twoNodeFailure(F_ARGS);
int fiftyPercentFail(F_ARGS);
int twoMasterNodeFailure(F_ARGS);
int restartAllNodesGracfeul(F_ARGS);
int restartAllNodesAbort(F_ARGS);
int restartAllNodesError9999(F_ARGS);
int fiftyPercentStopAndWait(F_ARGS);
int restartNodeDuringLCP(F_ARGS);
int stopOnError(F_ARGS);
int getRandomNodeId(NdbRestarter& _restarter);

/**
 * Define list of restarts
 *  - name of restart
 *  - function perfoming the restart
 *  - required number of nodes
 *  - ...
 *  - arg1, used depending of restart
 *  - arg2, used depending of restart
 */

const NdbRestarts::NdbRestart NdbRestarts::m_restarts[] = {
  /*********************************************************
   *
   *  NODE RESTARTS with 1 node restarted
   *
   *********************************************************/
  /** 
   * Restart a randomly selected node
   * with graceful shutdown
   */ 
  NdbRestart("RestartRandomNode", 
	     NODE_RESTART,
	     restartRandomNodeGraceful,
	     2),
  /** 
   * Restart a randomly selected node
   * with immediate(abort) shutdown
   */ 
  NdbRestart("RestartRandomNodeAbort", 
	     NODE_RESTART,
	     restartRandomNodeAbort,
	     2),
  /** 
   * Restart a randomly selected node
   * with  error insert
   *
   */ 
  NdbRestart("RestartRandomNodeError",
	     NODE_RESTART,
	     restartRandomNodeError,
	     2),
  /**
   * Restart the master node
   * with  error insert
   */ 
  NdbRestart("RestartMasterNodeError",
	     NODE_RESTART,
	     restartMasterNodeError,
	     2),
  /**
   * Restart a randomly selected node without fileystem
   *
   */ 
  NdbRestart("RestartRandomNodeInitial",
	     NODE_RESTART,
	     restartRandomNodeInitial,
	     2),
  /**
   * Restart a randomly selected node and then 
   * crash it while restarting
   *
   */    
  NdbRestart("RestartNFDuringNR",
	     NODE_RESTART,
	     restartNFDuringNR,
	     2),   

  /**
   * Set StopOnError and crash the node by sending
   * SYSTEM_ERROR to it
   *
   */    
  NdbRestart("StopOnError",
	     NODE_RESTART,
	     stopOnError,
	     1),  

  /*********************************************************
   *
   *  MULTIPLE NODE RESTARTS with more than 1 node
   *
   *********************************************************/
  /**
   * 2 nodes restart, select nodes to restart randomly and restart 
   * with a small random delay between restarts
   */ 
  NdbRestart("TwoNodeFailure",
	     MULTIPLE_NODE_RESTART,
	     twoNodeFailure,
	     4),
  /**
   * 2 nodes restart, select master nodes and restart with 
   * a small random delay between restarts 
   */ 
  
  NdbRestart("TwoMasterNodeFailure",
	     MULTIPLE_NODE_RESTART,
	     twoMasterNodeFailure,
	     4),

  NdbRestart("FiftyPercentFail",
	     MULTIPLE_NODE_RESTART,
	     fiftyPercentFail,
	     2),

  /*********************************************************
   *
   *  SYSTEM RESTARTS
   *
   *********************************************************/
  /**
   * Restart all nodes with graceful shutdown
   *
   */ 
  
  NdbRestart("RestartAllNodes",
	     SYSTEM_RESTART,
	     restartAllNodesGracfeul,
	     1),
  /**
   * Restart all nodes immediately without
   * graful shutdown
   */ 
  NdbRestart("RestartAllNodesAbort",
	     SYSTEM_RESTART,
	     restartAllNodesAbort,
	     1),
  /**
   * Restart all nodes with error insert 9999
   * TODO! We can later add more errors like 9998, 9997 etc.
   */ 
  NdbRestart("RestartAllNodesError9999",
	     SYSTEM_RESTART,
	     restartAllNodesError9999,
	     1),
  /**
   * Stop 50% of all nodes with error insert 9999
   * Wait for a random number of minutes
   * Stop the rest of the nodes and then start all again
   */ 
  NdbRestart("FiftyPercentStopAndWait",
	     SYSTEM_RESTART,
	     fiftyPercentStopAndWait,
	     2),
  /** 
   * Restart a master node during LCP with error inserts.
   */ 
  NdbRestart("RestartNodeDuringLCP", 
	     NODE_RESTART,
	     restartNodeDuringLCP,
	     2),
};

const int NdbRestarts::m_NoOfRestarts = sizeof(m_restarts) / sizeof(NdbRestart);


const NdbRestarts::NdbErrorInsert NdbRestarts::m_errors[] = {
  NdbErrorInsert("Error9999", 9999)
};

const int NdbRestarts::m_NoOfErrors = sizeof(m_errors) / sizeof(NdbErrorInsert);

NdbRestarts::NdbRestart::NdbRestart(const char* _name,
				    NdbRestartType _type,
				    restartFunc* _func,
				    int _requiredNodes,
				    int _arg1){
  m_name = _name;
  m_type = _type;
  m_restartFunc = _func;
  m_numRequiredNodes = _requiredNodes;
  //  m_arg1 = arg1;
}


int NdbRestarts::getNumRestarts(){
  return m_NoOfRestarts;
}

const NdbRestarts::NdbRestart* NdbRestarts::getRestart(int _num){
  if (_num >= m_NoOfRestarts)
    return NULL;

  return &m_restarts[_num];
}

const NdbRestarts::NdbRestart* NdbRestarts::getRestart(const char* _name){
  for(int i = 0; i < m_NoOfRestarts; i++){
    if (strcmp(m_restarts[i].m_name, _name) == 0){
      return &m_restarts[i];
    }
  }
  g_err << "The restart \""<< _name << "\" not found in NdbRestarts" << endl;
  return NULL;
}


int NdbRestarts::executeRestart(NDBT_Context* ctx,
                                const NdbRestarts::NdbRestart* _restart,
				unsigned int _timeout){
  // Check that there are enough nodes in the cluster
  // for this test
  NdbRestarter restarter;
  if (_restart->m_numRequiredNodes > restarter.getNumDbNodes()){
    g_err << "This test requires " << _restart->m_numRequiredNodes << " nodes "
	  << "there are only "<< restarter.getNumDbNodes() <<" nodes in cluster" 
	  << endl;
    return NDBT_OK;
  }
  if (restarter.waitClusterStarted(120) != 0){
    // If cluster is not started when we shall peform restart
    // the restart can not be executed and the test fails
    return NDBT_FAILED;
  }
  
  int res = _restart->m_restartFunc(ctx, restarter, _restart);

  // Sleep a little waiting for nodes to react to command
  NdbSleep_SecSleep(2);

  if  (_timeout == 0){
    // If timeout == 0 wait for ever
    while(restarter.waitClusterStarted(60) != 0)
      g_err << "Cluster is not started after restart. Waiting 60s more..." 
	    << endl;
  } else {
    if (restarter.waitClusterStarted(_timeout) != 0){
      g_err<<"Cluster failed to start" << endl;
      res = NDBT_FAILED; 
    }
  }

  return res;
} 

int NdbRestarts::executeRestart(NDBT_Context* ctx,
                                int _num,
				unsigned int _timeout){
  const NdbRestarts::NdbRestart* r = getRestart(_num);
  if (r == NULL)
    return NDBT_FAILED;

  int res = executeRestart(ctx, r, _timeout);
  return res;
}

int NdbRestarts::executeRestart(NDBT_Context* ctx,
                                const char* _name,
				unsigned int _timeout){
  const NdbRestarts::NdbRestart* r = getRestart(_name);
  if (r == NULL)
    return NDBT_FAILED;

  int res = executeRestart(ctx, r, _timeout);
  return res;
}

void NdbRestarts::listRestarts(NdbRestartType _type){
  for(int i = 0; i < m_NoOfRestarts; i++){
    if (m_restarts[i].m_type == _type)
      ndbout << " " << m_restarts[i].m_name << ", min " 
	     << m_restarts[i].m_numRequiredNodes 
	     << " nodes"<< endl;
  }  
}

void NdbRestarts::listRestarts(){
  ndbout << "NODE RESTARTS" << endl;
  listRestarts(NODE_RESTART);
  ndbout << "MULTIPLE NODE RESTARTS" << endl;
  listRestarts(MULTIPLE_NODE_RESTART);
  ndbout << "SYSTEM RESTARTS" << endl;
  listRestarts(SYSTEM_RESTART);  
}

NdbRestarts::NdbErrorInsert::NdbErrorInsert(const char* _name,
					    int _errorNo){
  
  m_name = _name;
  m_errorNo = _errorNo;
}

int NdbRestarts::getNumErrorInserts(){
  return m_NoOfErrors;
}

const NdbRestarts::NdbErrorInsert* NdbRestarts::getError(int _num){
  if (_num >= m_NoOfErrors)
    return NULL;
  return &m_errors[_num];
}

const NdbRestarts::NdbErrorInsert* NdbRestarts::getRandomError(){
  int randomId = myRandom48(m_NoOfErrors);
  return &m_errors[randomId];
}



/**
 *
 * IMPLEMENTATION OF THE DIFFERENT RESTARTS
 * Each function should perform it's action
 * and the returned NDBT_OK or NDBT_FAILED
 *
 */


#define CHECK(b, m) { int _xx = b; if (!(_xx)) { \
  ndbout << "ERR: "<< m \
           << "   " << "File: " << __FILE__ \
           << " (Line: " << __LINE__ << ")" << "- " << _xx << endl; \
  return NDBT_FAILED; } }



int restartRandomNodeGraceful(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);
  
  g_info << _restart->m_name << ": node = "<<nodeId << endl;

  CHECK(_restarter.restartOneDbNode(nodeId) == 0, 
	"Could not restart node "<<nodeId);

  return NDBT_OK;
}

int restartRandomNodeAbort(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);
  
  g_info << _restart->m_name << ": node = "<<nodeId << endl;

  CHECK(_restarter.restartOneDbNode(nodeId, false, false, true) == 0, 
	"Could not restart node "<<nodeId);

  return NDBT_OK;
}

int restartRandomNodeError(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);
  
  ndbout << _restart->m_name << ": node = "<<nodeId << endl;

  CHECK(_restarter.insertErrorInNode(nodeId, 9999) == 0, 
	"Could not restart node "<<nodeId);

  return NDBT_OK;
}

int restartMasterNodeError(F_ARGS){

  int nodeId = _restarter.getDbNodeId(0);
  
  g_info << _restart->m_name << ": node = "<<nodeId << endl;

  CHECK(_restarter.insertErrorInNode(nodeId, 39999) == 0, 
	"Could not restart node "<<nodeId);

  return NDBT_OK;
}

int restartRandomNodeInitial(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);
  
  g_info << _restart->m_name << ": node = "<<nodeId << endl;

  CHECK(_restarter.restartOneDbNode(nodeId, true) == 0,
	"Could not restart node "<<nodeId);

  return NDBT_OK;
}

int twoNodeFailure(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int n[2];
  n[0] = _restarter.getDbNodeId(randomId);  
  n[1] = _restarter.getRandomNodeOtherNodeGroup(n[0], rand());
  g_info << _restart->m_name << ": node = "<< n[0] << endl;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  CHECK(_restarter.dumpStateOneNode(n[0], val2, 2) == 0,
        "Failed to dump");
  CHECK(_restarter.dumpStateOneNode(n[1], val2, 2) == 0,
        "Failed to dump");
  
  CHECK(_restarter.insertErrorInNode(n[0], 9999) == 0,
	"Could not restart node "<< n[0]);

    // Create random value, max 3 secs
  int max = 3000;
  int ms = (myRandom48(max)) + 1;   
  g_info << "Waiting for " << ms << "(" << max 
	 << ") ms " << endl;
  NdbSleep_MilliSleep(ms);

  g_info << _restart->m_name << ": node = "<< n[1] << endl;
  CHECK(_restarter.insertErrorInNode(n[1], 9999) == 0,
	"Could not restart node "<< n[1]);

  CHECK(_restarter.waitNodesNoStart(n, 2) == 0,
        "Failed to wait nostart");

  _restarter.startNodes(n, 2);
  
  return NDBT_OK;
}

int twoMasterNodeFailure(F_ARGS){

  int n[2];
  n[0] = _restarter.getMasterNodeId();  
  n[1] = n[0];
  do {
    n[1] = _restarter.getNextMasterNodeId(n[1]);
  } while(_restarter.getNodeGroup(n[0]) == _restarter.getNodeGroup(n[1]));
  
  g_info << _restart->m_name << ": ";
  g_info << "node0 = "<< n[0] << "(" << _restarter.getNodeGroup(n[0]) << ") ";
  g_info << "node1 = "<< n[1] << "(" << _restarter.getNodeGroup(n[1]) << ") ";
  g_info << endl;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  CHECK(_restarter.dumpStateOneNode(n[0], val2, 2) == 0,
        "Failed to dump");
  CHECK(_restarter.dumpStateOneNode(n[1], val2, 2) == 0,
        "Failed to dump");
  
  CHECK(_restarter.insertErrorInNode(n[0], 9999) == 0,
	"Could not restart node "<< n[0]);
  
  // Create random value, max 3 secs
  int max = 3000;
  int ms = (myRandom48(max)) + 1;   
  g_info << "Waiting for " << ms << "(" << max 
	 << ") ms " << endl;
  NdbSleep_MilliSleep(ms);
  
  g_info << _restart->m_name << ": node = "<< n[1] << endl;
  
  CHECK(_restarter.insertErrorInNode(n[1], 9999) == 0,
	"Could not restart node "<< n[1]);
  
  CHECK(_restarter.waitNodesNoStart(n, 2) == 0,
        "Failed to wait nostart");

  _restarter.startNodes(n, 2);
  
  return NDBT_OK;
}

int get50PercentOfNodes(NdbRestarter& restarter, 
			int * _nodes){
  // For now simply return all nodes with even node id
  // TODO Check nodegroup and return one node from each 

  int num50Percent = restarter.getNumDbNodes() / 2;
  assert(num50Percent <= MAX_NDB_NODES);

  // Calculate which nodes to stop, select all even nodes
  for (int i = 0; i < num50Percent; i++){
    _nodes[i] = restarter.getDbNodeId(i*2);
  }
  return num50Percent;
}

int fiftyPercentFail(F_ARGS){


  int nodes[MAX_NDB_NODES];

  int numNodes = get50PercentOfNodes(_restarter, nodes);
  
  // Stop the nodes, with nostart and abort
  for (int i = 0; i < numNodes; i++){
    g_info << "Stopping node "<< nodes[i] << endl;
    int res = _restarter.restartOneDbNode(nodes[i], false, true, true);
    CHECK(res == 0, "Could not stop node: "<< nodes[i]);
  }

  CHECK(_restarter.waitNodesNoStart(nodes, numNodes) == 0, 
	"waitNodesNoStart");

  // Order all nodes to start 
  ndbout << "Starting all nodes" << endl;
  CHECK(_restarter.startAll() == 0,
	"Could not start all nodes");

  return NDBT_OK;
}


int restartAllNodesGracfeul(F_ARGS){

  g_info << _restart->m_name  << endl;

  // Restart graceful
  CHECK(_restarter.restartAll() == 0,
	"Could not restart all nodes");

  return NDBT_OK;

}

int restartAllNodesAbort(F_ARGS){
  
  g_info << _restart->m_name  << endl;

  // Restart abort
  CHECK(_restarter.restartAll(false, false, true) == 0,
	"Could not restart all nodes");

  return NDBT_OK;
}

int restartAllNodesError9999(F_ARGS){
  
  g_info << _restart->m_name <<  endl;

  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 } ;
  CHECK(_restarter.dumpStateAllNodes(val, 2) == 0,
        "failed to set RestartOnErrorInsert");

  CHECK(_restarter.insertErrorInAllNodes(932) == 0,
        "Failed to set error 932 (auto-restart on arbit error)");

  // Restart with error insert
  CHECK(_restarter.insertErrorInAllNodes(9999) == 0,
	"Could not restart all nodes ");

  CHECK(_restarter.waitClusterNoStart() == 0,
        "Failed to wait not started");

  _restarter.startAll();

  return NDBT_OK;
}

int fiftyPercentStopAndWait(F_ARGS){

  int nodes[MAX_NDB_NODES];
  int numNodes = get50PercentOfNodes(_restarter, nodes);

  // Stop the nodes, with nostart and abort
  for (int i = 0; i < numNodes; i++){
    g_info << "Stopping node "<<nodes[i] << endl;
    int res = _restarter.restartOneDbNode(nodes[i], false, true, true);
    CHECK(res == 0, "Could not stop node: "<< nodes[i]);
  }

  CHECK(_restarter.waitNodesNoStart(nodes, numNodes) == 0, 
	"waitNodesNoStart");

  // Create random value, max 120 secs
  int max = 120;
  int seconds = (myRandom48(max)) + 1;   
  g_info << "Waiting for " << seconds << "(" << max 
	 << ") secs " << endl;
  NdbSleep_SecSleep(seconds);  


  // Restart graceful
  CHECK(_restarter.restartAll() == 0,
	"Could not restart all nodes");

  g_info << _restart->m_name <<  endl;

  return NDBT_OK;
}

int
NFDuringNR_codes[] = {
  7121,
  5027,
  7172,
  6000,
  6001,
  7171,
  7130,
  7133,
  7138,
  7154,
  7144,
  5026,
  7139,
  7132,
  5045,

  7195, 7196,7197,7198,7199,
  

  //LCP
  8000,
  8001,
  5010,
  7022,
  7024,
  7016,
  7017,
  5002
};

int restartNFDuringNR(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int i;
  const int sz = sizeof(NFDuringNR_codes)/sizeof(NFDuringNR_codes[0]);
  for(i = 0; i<sz; i++){
    int randomId = myRandom48(_restarter.getNumDbNodes());
    int nodeId = _restarter.getDbNodeId(randomId);
    int error = NFDuringNR_codes[i];
    
    g_err << _restart->m_name << ": node = " << nodeId 
	  << " error code = " << error << endl;
    
    CHECK(_restarter.restartOneDbNode(nodeId, false, true, true) == 0,
	  "Could not restart node "<< nodeId);
    
    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");
    
    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 } ;
    CHECK(_restarter.dumpStateOneNode(nodeId, val, 2) == 0,
	  "failed to set RestartOnErrorInsert");
    
    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");
   
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    NdbSleep_SecSleep(3);

    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
          "waitNodesNoStart failed");

    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");
    
    CHECK(_restarter.waitNodesStarted(&nodeId, 1) == 0,
	  "waitNodesStarted failed");
  }

  return NDBT_OK;
  
  if(_restarter.getNumDbNodes() < 4)
    return NDBT_OK;

  char buf[256];
  if(NdbEnv_GetEnv("USER", buf, 256) == 0 || strcmp(buf, "ejonore") != 0)
    return NDBT_OK;
  
  for(i = 0; i<sz && !ctx->isTestStopped(); i++){
    const int randomId = myRandom48(_restarter.getNumDbNodes());
    int nodeId = _restarter.getDbNodeId(randomId);
    const int error = NFDuringNR_codes[i];
    
    const int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");
    int crashNodeId = 0;
    do {
      int rand = myRandom48(1000);
      crashNodeId = _restarter.getRandomNodeOtherNodeGroup(nodeId, rand);
    } while(crashNodeId == masterNodeId);

    CHECK(crashNodeId > 0, "getMasterNodeId failed");

    g_info << _restart->m_name << " restarting node = " << nodeId 
	   << " error code = " << error 
	   << " crash node = " << crashNodeId << endl;
    
    CHECK(_restarter.restartOneDbNode(nodeId, false, true, true) == 0,
	  "Could not restart node "<< nodeId);
    
    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");
        
    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    CHECK(_restarter.dumpStateOneNode(crashNodeId, val, 2) == 0,
	  "failed to set RestartOnErrorInsert");
    
    CHECK(_restarter.insertErrorInNode(crashNodeId, error) == 0,
	  "failed to set error insert");
   
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    CHECK(_restarter.waitClusterStarted() == 0,
	  "waitClusterStarted failed");
  }

  return NDBT_OK;
}

int
NRDuringLCP_Master_codes[] = {
  7009, // Insert system error in master when local checkpoint is idle.
  7010, // Insert system error in master when local checkpoint is in the
        // state clcpStatus = CALCULATE_KEEP_GCI.
  7013, // Insert system error in master when local checkpoint is in the
        // state clcpStatus = COPY_GCI before sending COPY_GCIREQ.
  7014, // Insert system error in master when local checkpoint is in the
        // state clcpStatus = TC_CLOPSIZE before sending TC_CLOPSIZEREQ.
  7015, // Insert system error in master when local checkpoint is in the
        // state clcpStatus = START_LCP_ROUND before sending START_LCP_ROUND.
  7019, // Insert system error in master when local checkpoint is in the
        // state clcpStatus = IDLE before sending CONTINUEB(ZCHECK_TC_COUNTER).
  7075, // Master. Don't send any LCP_FRAG_ORD(last=true)
        // And crash when all have "not" been sent
  7021, // Crash in  master when receiving START_LCP_REQ
  7023, // Crash in  master when sending START_LCP_CONF
  7025, // Crash in  master when receiving LCP_FRAG_REP
  7026, // Crash in  master when changing state to LCP_TAB_COMPLETED 
  7027  // Crash in  master when changing state to LCP_TAB_SAVED
};

int
NRDuringLCP_NonMaster_codes[] = {
  7020, // Insert system error in local checkpoint participant at reception 
        // of COPY_GCIREQ.
  8000, // Crash particpant when receiving TCGETOPSIZEREQ
  8001, // Crash particpant when receiving TC_CLOPSIZEREQ
  5010, // Crash any when receiving LCP_FRAGORD
  7022, // Crash in !master when receiving START_LCP_REQ
  7024, // Crash in !master when sending START_LCP_CONF
  7016, // Crash in !master when receiving LCP_FRAG_REP
  7017, // Crash in !master when changing state to LCP_TAB_COMPLETED 
  7018  // Crash in !master when changing state to LCP_TAB_SAVED
};

int restartNodeDuringLCP(F_ARGS) {
  int i;
  // Master
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  CHECK(_restarter.dumpStateAllNodes(&val, 1) == 0,
	"Failed to set LCP to min value"); // Set LCP to min val
  int sz = sizeof(NRDuringLCP_Master_codes)/
           sizeof(NRDuringLCP_Master_codes[0]);
  for(i = 0; i<sz; i++) {

    int error = NRDuringLCP_Master_codes[i];
    int masterNodeId = _restarter.getMasterNodeId();

    CHECK(masterNodeId > 0, "getMasterNodeId failed");

    ndbout << _restart->m_name << " restarting master node = " << masterNodeId 
	   << " error code = " << error << endl;

    {
      int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
      CHECK(_restarter.dumpStateAllNodes(val, 2) == 0,
	    "failed to set RestartOnErrorInsert");
    }

    CHECK(_restarter.insertErrorInNode(masterNodeId, error) == 0,
	  "failed to set error insert");

    CHECK(_restarter.waitNodesNoStart(&masterNodeId, 1, 300) == 0,
				      "failed to wait no start");

    CHECK(_restarter.startNodes(&masterNodeId, 1) == 0,
	  "failed to start node");

    CHECK(_restarter.waitClusterStarted(300) == 0,
	  "waitClusterStarted failed");

    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(_restarter.dumpStateOneNode(masterNodeId, &val, 1) == 0,
	    "failed to set error insert");
    }
  }

  // NON-Master
  sz = sizeof(NRDuringLCP_NonMaster_codes)/
       sizeof(NRDuringLCP_NonMaster_codes[0]);
  for(i = 0; i<sz; i++) {

    int error = NRDuringLCP_NonMaster_codes[i];
    int nodeId = getRandomNodeId(_restarter);
    int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");

    while (nodeId == masterNodeId) {
      nodeId = getRandomNodeId(_restarter);
    }

    ndbout << _restart->m_name << " restarting non-master node = " << nodeId
	   << " error code = " << error << endl;

    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    CHECK(_restarter.dumpStateAllNodes(val, 2) == 0,
	  "failed to set RestartOnErrorInsert");
    
    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");

    CHECK(_restarter.waitNodesNoStart(&nodeId, 1, 300) == 0,
				      "failed to wait no start");

    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    CHECK(_restarter.waitClusterStarted(300) == 0,
	  "waitClusterStarted failed");

    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(_restarter.dumpStateOneNode(nodeId, &val, 1) == 0,
	    "failed to set error insert");
    }
  }

  return NDBT_OK;
}

int stopOnError(F_ARGS){

  myRandom48Init((long)NdbTick_CurrentMillisecond());

  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);
  
  do {
    g_info << _restart->m_name << ": node = " << nodeId 
	   << endl;
    
    CHECK(_restarter.waitClusterStarted(300) == 0,
	  "waitClusterStarted failed");
    
    int val = DumpStateOrd::NdbcntrTestStopOnError;
    CHECK(_restarter.dumpStateOneNode(nodeId, &val, 1) == 0,
	  "failed to set NdbcntrTestStopOnError");
    
    NdbSleep_SecSleep(3);
    
    CHECK(_restarter.waitClusterStarted(300) == 0,
	  "waitClusterStarted failed");
  } while (false);
  
  return NDBT_OK;
}

int getRandomNodeId(NdbRestarter& _restarter) {
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int randomId = myRandom48(_restarter.getNumDbNodes());
  int nodeId = _restarter.getDbNodeId(randomId);

  return nodeId;
}
