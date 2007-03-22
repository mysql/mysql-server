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

#include <NdbRestarter.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <mgmapi_debug.h>
#include <NDBT_Output.hpp>
#include <random.h>
#include <kernel/ndb_limits.h>
#include <ndb_version.h>

#define MGMERR(h) \
  ndbout << "latest_error="<<ndb_mgm_get_latest_error(h) \
	 << ", line="<<ndb_mgm_get_latest_error_line(h) \
         << ", mesg="<<ndb_mgm_get_latest_error_msg(h) \
         << ", desc="<<ndb_mgm_get_latest_error_desc(h) \
	 << endl;


NdbRestarter::NdbRestarter(const char* _addr): 
  connected(false),
  handle(NULL),
  m_config(0)
{
  if (_addr == NULL){
    addr.assign("");
  } else {
    addr.assign(_addr);
  }
}

NdbRestarter::~NdbRestarter(){
  disconnect();
}

int NdbRestarter::getDbNodeId(int _i){
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  for(size_t i = 0; i < ndbNodes.size(); i++){     
    if (i == (unsigned)_i){
      return ndbNodes[i].node_id;
    }
  }
  return -1;
}


int
NdbRestarter::restartOneDbNode(int _nodeId,
			       bool inital,
			       bool nostart,
			       bool abort){
  if (!isConnected())
    return -1;

  int ret = 0;
  
  if ((ret = ndb_mgm_restart2(handle, 1, &_nodeId,
			      inital, nostart, abort)) <= 0) {
    /**
     * ndb_mgm_restart2 returned error, one reason could
     * be that the node have not stopped fast enough!
     * Check status of the node to see if it's on the 
     * way down. If that's the case ignore the error
     */ 

    if (getStatus() != 0)
      return -1;

    g_info << "ndb_mgm_restart2 returned with error, checking node state" << endl;

    for(size_t i = 0; i < ndbNodes.size(); i++){
      if(ndbNodes[i].node_id == _nodeId){
	g_info <<_nodeId<<": status="<<ndbNodes[i].node_status<<endl;
	/* Node found check state */
	switch(ndbNodes[i].node_status){
	case NDB_MGM_NODE_STATUS_RESTARTING:
	case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
	  return 0;
	default:
	  break;
	}
      }
    }
    
    MGMERR(handle);
    g_err  << "Could not stop node with id = "<< _nodeId << endl;
    return -1;
  }

  return 0;
}

int
NdbRestarter::getMasterNodeId(){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  int min = 0;
  int node = -1;
  for(size_t i = 0; i < ndbNodes.size(); i++){
    if(min == 0 || ndbNodes[i].dynamic_id < min){
      min = ndbNodes[i].dynamic_id;
      node = ndbNodes[i].node_id;
    }
  }

  return node;
}

int
NdbRestarter::getNodeGroup(int nodeId){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  for(size_t i = 0; i < ndbNodes.size(); i++)
  {
    if(ndbNodes[i].node_id == nodeId)
    {
      return ndbNodes[i].node_group;
    }
  }
  
  return -1;
}

int
NdbRestarter::getNextMasterNodeId(int nodeId){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  size_t i;
  for(i = 0; i < ndbNodes.size(); i++)
  {
    if(ndbNodes[i].node_id == nodeId)
    {
      break;
    }
  }
  assert(i < ndbNodes.size());
  if (i == ndbNodes.size())
    return -1;

  int dynid = ndbNodes[i].dynamic_id;
  int minid = dynid;
  for (i = 0; i<ndbNodes.size(); i++)
    if (ndbNodes[i].dynamic_id > minid)
      minid = ndbNodes[i].dynamic_id;
  
  for (i = 0; i<ndbNodes.size(); i++)
    if (ndbNodes[i].dynamic_id > dynid && 
	ndbNodes[i].dynamic_id < minid)
    {
      minid = ndbNodes[i].dynamic_id;
    }
  
  if (minid != ~0)
  {
    for (i = 0; i<ndbNodes.size(); i++)
      if (ndbNodes[i].dynamic_id == minid)
	return ndbNodes[i].node_id;
  }
  
  return getMasterNodeId();
}

int
NdbRestarter::getRandomNotMasterNodeId(int rand){
  int master = getMasterNodeId();
  if(master == -1)
    return -1;

  Uint32 counter = 0;
  rand = rand % ndbNodes.size();
  while(counter++ < ndbNodes.size() && ndbNodes[rand].node_id == master)
    rand = (rand + 1) % ndbNodes.size();
  
  if(ndbNodes[rand].node_id != master)
    return ndbNodes[rand].node_id;
  return -1;
}

int
NdbRestarter::getRandomNodeOtherNodeGroup(int nodeId, int rand){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  int node_group = -1;
  for(size_t i = 0; i < ndbNodes.size(); i++){
    if(ndbNodes[i].node_id == nodeId){
      node_group = ndbNodes[i].node_group;
      break;
    }
  }
  if(node_group == -1){
    return -1;
  }

  Uint32 counter = 0;
  rand = rand % ndbNodes.size();
  while(counter++ < ndbNodes.size() && ndbNodes[rand].node_group == node_group)
    rand = (rand + 1) % ndbNodes.size();
  
  if(ndbNodes[rand].node_group != node_group)
    return ndbNodes[rand].node_id;

  return -1;
}

int
NdbRestarter::getRandomNodeSameNodeGroup(int nodeId, int rand){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  int node_group = -1;
  for(size_t i = 0; i < ndbNodes.size(); i++){
    if(ndbNodes[i].node_id == nodeId){
      node_group = ndbNodes[i].node_group;
      break;
    }
  }
  if(node_group == -1){
    return -1;
  }

  Uint32 counter = 0;
  rand = rand % ndbNodes.size();
  while(counter++ < ndbNodes.size() && 
	(ndbNodes[rand].node_id == nodeId || 
	 ndbNodes[rand].node_group != node_group))
    rand = (rand + 1) % ndbNodes.size();
  
  if(ndbNodes[rand].node_group == node_group &&
     ndbNodes[rand].node_id != nodeId)
    return ndbNodes[rand].node_id;
  
  return -1;
}

int 
NdbRestarter::waitClusterStarted(unsigned int _timeout){
  return waitClusterState(NDB_MGM_NODE_STATUS_STARTED, _timeout);
}

int 
NdbRestarter::waitClusterStartPhase(int _startphase, unsigned int _timeout){
  return waitClusterState(NDB_MGM_NODE_STATUS_STARTING, _timeout, _startphase);
}

int 
NdbRestarter::waitClusterSingleUser(unsigned int _timeout){
  return waitClusterState(NDB_MGM_NODE_STATUS_SINGLEUSER, _timeout);
}

int 
NdbRestarter::waitClusterNoStart(unsigned int _timeout){
  return waitClusterState(NDB_MGM_NODE_STATUS_NOT_STARTED, _timeout);
}

int 
NdbRestarter::waitClusterState(ndb_mgm_node_status _status,
			       unsigned int _timeout,
			       int _startphase){

  int nodes[MAX_NDB_NODES];
  int numNodes = 0;

  if (getStatus() != 0)
    return -1;
  
  // Collect all nodes into nodes
  for (size_t i = 0; i < ndbNodes.size(); i++){
    nodes[i] = ndbNodes[i].node_id;
    numNodes++;
  }

  return waitNodesState(nodes, numNodes, _status, _timeout, _startphase);
}
 

int 
NdbRestarter::waitNodesState(const int * _nodes, int _num_nodes,
			     ndb_mgm_node_status _status,
			     unsigned int _timeout,
			     int _startphase){
  
  if (!isConnected()){
    g_err << "!isConnected"<<endl;
    return -1;
  }

  unsigned int attempts = 0;
  unsigned int resetAttempts = 0;
  const unsigned int MAX_RESET_ATTEMPTS = 10;
  bool allInState = false;    
  while (allInState == false){
    if (_timeout > 0 && attempts > _timeout){
      /**
       * Timeout has expired waiting for the nodes to enter
       * the state we want
       */
      bool waitMore = false;
      /** 
       * Make special check if we are waiting for 
       * cluster to become started
       */
      if(_status == NDB_MGM_NODE_STATUS_STARTED){
	waitMore = true;
	/**
	 * First check if any node is not starting
	 * then it's no idea to wait anymore
	 */
	for (size_t n = 0; n < ndbNodes.size(); n++){
	  if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED &&
	      ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTING)
	    waitMore = false;

	}
      } 

      if (!waitMore || resetAttempts > MAX_RESET_ATTEMPTS){
	g_err << "waitNodeState("
	      << ndb_mgm_get_node_status_string(_status)
	      <<", "<<_startphase<<")"
	      << " timeout after " << attempts <<" attemps" << endl;
	return -1;
      } 

      g_err << "waitNodeState("
	    << ndb_mgm_get_node_status_string(_status)
	    <<", "<<_startphase<<")"
	    << " resetting number of attempts "
	    << resetAttempts << endl;
      attempts = 0;
      resetAttempts++;
      
    }

    allInState = true;
    if (getStatus() != 0){
      g_err << "getStatus != 0" << endl;
      return -1;
    }

    // ndbout << "waitNodeState; _num_nodes = " << _num_nodes << endl;
    // for (int i = 0; i < _num_nodes; i++)
    //   ndbout << " node["<<i<<"] =" <<_nodes[i] << endl;

    for (int i = 0; i < _num_nodes; i++){
      ndb_mgm_node_state* ndbNode = NULL;
      for (size_t n = 0; n < ndbNodes.size(); n++){
	if (ndbNodes[n].node_id == _nodes[i])
	  ndbNode = &ndbNodes[n];
      }

      if(ndbNode == NULL){
	allInState = false;
	continue;
      }

      g_info << "State node " << ndbNode->node_id << " "
	     << ndb_mgm_get_node_status_string(ndbNode->node_status);
      if (ndbNode->node_status == NDB_MGM_NODE_STATUS_STARTING)
        g_info<< ", start_phase=" << ndbNode->start_phase;
      g_info << endl;

      assert(ndbNode != NULL);

      if(_status == NDB_MGM_NODE_STATUS_STARTING && 
	 ((ndbNode->node_status == NDB_MGM_NODE_STATUS_STARTING && 
	   ndbNode->start_phase >= _startphase) ||
	  (ndbNode->node_status == NDB_MGM_NODE_STATUS_STARTED)))
	continue;

      if (_status == NDB_MGM_NODE_STATUS_STARTING){
	g_info << "status = "  
	       << ndb_mgm_get_node_status_string(ndbNode->node_status)
	       <<", start_phase="<<ndbNode->start_phase<<endl;
	if (ndbNode->node_status !=  _status) {
	  if (ndbNode->node_status < _status)
	    allInState = false;
	  else 
	    g_info << "node_status(" << ndbNode->node_status
		   <<") != _status("<<_status<<")"<<endl;
	} else if (ndbNode->start_phase < _startphase)
	  allInState = false;
      } else {
	if (ndbNode->node_status !=  _status) 
	  allInState = false;
      }
    }
    g_info << "Waiting for cluster enter state" 
	    << ndb_mgm_get_node_status_string(_status)<< endl;
    NdbSleep_SecSleep(1);
    attempts++;
  }
  return 0;
}

int NdbRestarter::waitNodesStarted(const int * _nodes, int _num_nodes,
		     unsigned int _timeout){
  return waitNodesState(_nodes, _num_nodes, 
			NDB_MGM_NODE_STATUS_STARTED, _timeout);  
}

int NdbRestarter::waitNodesStartPhase(const int * _nodes, int _num_nodes, 
			int _startphase, unsigned int _timeout){
  return waitNodesState(_nodes, _num_nodes, 
			  NDB_MGM_NODE_STATUS_STARTING, _timeout,
			  _startphase);  
}

int NdbRestarter::waitNodesNoStart(const int * _nodes, int _num_nodes,
		     unsigned int _timeout){
  return waitNodesState(_nodes, _num_nodes, 
			  NDB_MGM_NODE_STATUS_NOT_STARTED, _timeout);  
}

bool 
NdbRestarter::isConnected(){
  if (connected == true)
    return true;
  return connect() == 0;
}

int 
NdbRestarter::connect(){
  disconnect();
  handle = ndb_mgm_create_handle();   
  if (handle == NULL){
    g_err << "handle == NULL" << endl;
    return -1;
  }
  g_info << "Connecting to mgmsrv at " << addr.c_str() << endl;
  if (ndb_mgm_set_connectstring(handle,addr.c_str()))
  {
    MGMERR(handle);
    g_err  << "Connection to " << addr.c_str() << " failed" << endl;
    return -1;
  }

  if (ndb_mgm_connect(handle, 0, 0, 0) == -1)
  {
    MGMERR(handle);
    g_err  << "Connection to " << addr.c_str() << " failed" << endl;
    return -1;
  }

  connected = true;
  return 0;
}

void 
NdbRestarter::disconnect(){
  if (handle != NULL){
    ndb_mgm_disconnect(handle);
    ndb_mgm_destroy_handle(&handle);
  }
  connected = false;
}

int 
NdbRestarter::getStatus(){
  int retries = 0;
  struct ndb_mgm_cluster_state * status;
  struct ndb_mgm_node_state * node;
  
  ndbNodes.clear();
  mgmNodes.clear();
  apiNodes.clear();

  if (!isConnected())
    return -1;
  
  while(retries < 10){
    status = ndb_mgm_get_status(handle);
    if (status == NULL){
      ndbout << "status==NULL, retries="<<retries<<endl;
      MGMERR(handle);
      retries++;
      continue;
    }
    for (int i = 0; i < status->no_of_nodes; i++){
      node = &status->node_states[i];      
      switch(node->node_type){
      case NDB_MGM_NODE_TYPE_NDB:
	ndbNodes.push_back(*node);
	break;
      case NDB_MGM_NODE_TYPE_MGM:
	mgmNodes.push_back(*node);
	break;
      case NDB_MGM_NODE_TYPE_API:
	apiNodes.push_back(*node);
	break;
      default:
	if(node->node_status == NDB_MGM_NODE_STATUS_UNKNOWN ||
	   node->node_status == NDB_MGM_NODE_STATUS_NO_CONTACT){
	  retries++;
	  ndbNodes.clear();
	  mgmNodes.clear();
	  apiNodes.clear();
	  free(status); 
	  status = NULL;
	  i = status->no_of_nodes;

	  ndbout << "kalle"<< endl;
	  break;
	}
	abort();
	break;
      }
    }
    if(status == 0){
      ndbout << "status == 0" << endl;
      continue;
    }
    free(status);
    return 0;
  }
   
  g_err  << "getStatus failed" << endl;
  return -1;
}


int NdbRestarter::getNumDbNodes(){
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  return ndbNodes.size();
}

int NdbRestarter::restartAll(bool initial,
			     bool nostart,
			     bool abort){
  
  if (!isConnected())
    return -1;

  if (ndb_mgm_restart2(handle, 0, NULL, initial, 1, abort) == -1) {
    MGMERR(handle);
    g_err  << "Could not restart(stop) all nodes " << endl;
    // return -1; Continue anyway - Magnus
  }
  
  if (waitClusterNoStart(60) != 0){
    g_err << "Cluster didnt enter STATUS_NOT_STARTED within 60s" << endl;
    return -1;
  }
  
  if(nostart){
    g_debug << "restartAll: nostart == true" << endl;
    return 0;
  }

  if (ndb_mgm_start(handle, 0, NULL) == -1) {
    MGMERR(handle);
    g_err  << "Could not restart(start) all nodes " << endl;
    return -1;
  }
  
  return 0;
}

int NdbRestarter::startAll(){
  if (!isConnected())
    return -1;

  if (ndb_mgm_start(handle, 0, NULL) == -1) {
    MGMERR(handle);
    g_err  << "Could not start all nodes " << endl;
    return -1;
  }
  
  return 0;
  
}

int NdbRestarter::startNodes(const int * nodes, int num_nodes){
  if (!isConnected())
    return -1;
  
  if (ndb_mgm_start(handle, num_nodes, nodes) != num_nodes) {
    MGMERR(handle);
    g_err  << "Could not start all nodes " << endl;
    return -1;
  }
  
  return 0;
}

int NdbRestarter::insertErrorInNode(int _nodeId, int _error){
  if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  if (ndb_mgm_insert_error(handle, _nodeId, _error, &reply) == -1){
    MGMERR(handle);
    g_err << "Could not insert error in node with id = "<< _nodeId << endl;
  }
  if(reply.return_code != 0){
    g_err << "Error: " << reply.message << endl;
  }
  return 0;
}

int NdbRestarter::insertErrorInAllNodes(int _error){
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int result = 0;
 
  for(size_t i = 0; i < ndbNodes.size(); i++){     
    g_debug << "inserting error in node " << ndbNodes[i].node_id << endl;
    if (insertErrorInNode(ndbNodes[i].node_id, _error) == -1)
      result = -1;
  }
  return result;

}



int NdbRestarter::dumpStateOneNode(int _nodeId, const int * _args, int _num_args){
 if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  if (ndb_mgm_dump_state(handle, _nodeId, _args, _num_args, &reply) == -1){
    MGMERR(handle);
    g_err << "Could not dump state in node with id = "<< _nodeId << endl;
  }

  if(reply.return_code != 0){
    g_err << "Error: " << reply.message << endl;
  }
  return reply.return_code;  
}

int NdbRestarter::dumpStateAllNodes(const int * _args, int _num_args){
 if (!isConnected())
    return -1;

 if (getStatus() != 0)
   return -1;

 int result = 0;
 
 for(size_t i = 0; i < ndbNodes.size(); i++){     
   g_debug << "dumping state in node " << ndbNodes[i].node_id << endl;
   if (dumpStateOneNode(ndbNodes[i].node_id, _args, _num_args) == -1)
     result = -1;
 }
 return result;

}


int NdbRestarter::enterSingleUserMode(int _nodeId){
  if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  if (ndb_mgm_enter_single_user(handle, _nodeId, &reply) == -1){
    MGMERR(handle);
    g_err << "Could not enter single user mode api node = "<< _nodeId << endl;
  }
  
  if(reply.return_code != 0){
    g_err << "Error: " << reply.message << endl;
  }
  
  return reply.return_code;  
}


int NdbRestarter::exitSingleUserMode(){
  if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  if (ndb_mgm_exit_single_user(handle, &reply) == -1){
    MGMERR(handle);
    g_err << "Could not exit single user mode " << endl;
  }

  if(reply.return_code != 0){
    g_err << "Error: " << reply.message << endl;
  }
  return reply.return_code;  
}

ndb_mgm_configuration*
NdbRestarter::getConfig(){
  if(m_config) return m_config;

  if (!isConnected())
    return 0;
  m_config = ndb_mgm_get_configuration(handle, 0);
  return m_config;
}

template class Vector<ndb_mgm_node_state>;
