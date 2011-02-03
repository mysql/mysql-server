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

#include <NdbRestarter.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <mgmapi_debug.h>
#include <NDBT_Output.hpp>
#include <random.h>
#include <kernel/ndb_limits.h>
#include <ndb_version.h>
#include <NodeBitmask.hpp>

#define MGMERR(h) \
  ndbout << "latest_error="<<ndb_mgm_get_latest_error(h) \
	 << ", line="<<ndb_mgm_get_latest_error_line(h) \
         << ", mesg="<<ndb_mgm_get_latest_error_msg(h) \
         << ", desc="<<ndb_mgm_get_latest_error_desc(h) \
	 << endl;


NdbRestarter::NdbRestarter(const char* _addr): 
  handle(NULL),
  connected(false),
  m_config(0),
  m_reconnect(false)
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
			       bool abort,
                               bool force)
{
  return restartNodes(&_nodeId, 1,
                      (inital ? NRRF_INITIAL : 0) |
                      (nostart ? NRRF_NOSTART : 0) |
                      (abort ? NRRF_ABORT : 0) |
                      (force ? NRRF_FORCE : 0));
}

int
NdbRestarter::restartNodes(int * nodes, int cnt,
                           Uint32 flags)
{
  if (!isConnected())
    return -1;

  int ret = 0;
  int unused;
  if ((ret = ndb_mgm_restart4(handle, cnt, nodes,
                              (flags & NRRF_INITIAL),
                              (flags & NRRF_NOSTART),
                              (flags & NRRF_ABORT),
                              (flags & NRRF_FORCE),
                              &unused)) <= 0)
  {
    /**
     * ndb_mgm_restart4 returned error, one reason could
     * be that the node have not stopped fast enough!
     * Check status of the node to see if it's on the 
     * way down. If that's the case ignore the error
     */ 

    if (getStatus() != 0)
      return -1;

    g_info << "ndb_mgm_restart4 returned with error, checking node state"
           << endl;

    for (int j = 0; j<cnt; j++)
    {
      int _nodeId = nodes[j];
      for(size_t i = 0; i < ndbNodes.size(); i++)
      {
        if(ndbNodes[i].node_id == _nodeId)
        {
          g_info <<_nodeId<<": status="<<ndbNodes[i].node_status<<endl;
          /* Node found check state */
          switch(ndbNodes[i].node_status){
          case NDB_MGM_NODE_STATUS_RESTARTING:
          case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
            break;
          default:
            MGMERR(handle);
            g_err  << "Could not stop node with id = "<< _nodeId << endl;
            return -1;
          }
        }
      }
    }
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


// Wait until connected to ndb_mgmd
int
NdbRestarter::waitConnected(unsigned int _timeout){
  _timeout*= 10;
  while (isConnected() && getStatus() != 0){
    if (_timeout-- == 0){
      ndbout << "NdbRestarter::waitConnected failed" << endl;
      return -1;
    }
    NdbSleep_MilliSleep(100);
  }
  return 0;
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

  if (getStatus() != 0){
    g_err << "waitClusterStat: getStatus != 0" << endl;
    return -1;
  }
  
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
	g_err << "waitNodesState("
	      << ndb_mgm_get_node_status_string(_status)
	      <<", "<<_startphase<<")"
	      << " timeout after " << attempts <<" attemps" << endl;
	return -1;
      } 

      g_err << "waitNodesState("
	    << ndb_mgm_get_node_status_string(_status)
	    <<", "<<_startphase<<")"
	    << " resetting number of attempts "
	    << resetAttempts << endl;
      attempts = 0;
      resetAttempts++;
      
    }

    allInState = true;
    if (getStatus() != 0){
      g_err << "waitNodesState: getStatus != 0" << endl;
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
      if (m_reconnect){
        if (connect() == 0){
          g_err << "Reconnected..." << endl;
          continue;
        }
        const int err = ndb_mgm_get_latest_error(handle);
        if (err == NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET){
          g_err << "Could not connect to socket, sleep and retry" << endl;
          retries= 0;
          NdbSleep_SecSleep(1);
          continue;
        }
      }
      const int err = ndb_mgm_get_latest_error(handle);
      ndbout << "status==NULL, retries="<<retries<< " err=" << err << endl;
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
			     bool abort,
                             bool force)
{
  if (!isConnected())
    return -1;

  int unused;
  if (ndb_mgm_restart4(handle, 0, NULL, initial, 1, abort,
                       force, &unused) == -1) {
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

int
NdbRestarter::getNode(NodeSelector type)
{
  switch(type){
  case NS_RANDOM:
    return getDbNodeId(rand() % getNumDbNodes());
  case NS_MASTER:
    return getMasterNodeId();
  case NS_NON_MASTER:
    return getRandomNotMasterNodeId(rand());
  default:
    abort();
  }
  return -1;
}


void
NdbRestarter::setReconnect(bool val){
  m_reconnect= val;
}

int
NdbRestarter::checkClusterAlive(const int * deadnodes, int num_nodes)
{
  if (getStatus() != 0)
    return -1;
  
  NdbNodeBitmask mask;
  for (int i = 0; i<num_nodes; i++)
    mask.set(deadnodes[i]);
  
  for (size_t n = 0; n < ndbNodes.size(); n++)
  {
    if (mask.get(ndbNodes[n].node_id))
      continue;

    if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED)
      return ndbNodes[n].node_id;
  }
  
  return 0;
}

int
NdbRestarter::rollingRestart(Uint32 flags)
{
  if (getStatus() != 0)
    return -1;
  
  NdbNodeBitmask ng_mask;
  NdbNodeBitmask restart_nodes;
  Vector<int> nodes;
  for(size_t i = 0; i < ndbNodes.size(); i++)
  { 
    if (ng_mask.get(ndbNodes[i].node_group) == false)
    {
      ng_mask.set(ndbNodes[i].node_group);
      nodes.push_back(ndbNodes[i].node_id);
      restart_nodes.set(ndbNodes[i].node_id);
    }
  }

loop:  
  if (ndb_mgm_restart2(handle, nodes.size(), nodes.getBase(),
                       (flags & NRRF_INITIAL) != 0, 
                       (flags & NRRF_NOSTART) != 0,
                       (flags & NRRF_ABORT) != 0 || true) <= 0)
  {
    return -1;
  }
  
  if (waitNodesNoStart(nodes.getBase(), nodes.size()))
    return -1;

  if (startNodes(nodes.getBase(), nodes.size()))
    return -1;

  if (waitClusterStarted())
    return -1;

  nodes.clear();
  for (Uint32 i = 0; i<ndbNodes.size(); i++)
  {
    if (restart_nodes.get(ndbNodes[i].node_id) == false)
    {
      nodes.push_back(ndbNodes[i].node_id);
      restart_nodes.set(ndbNodes[i].node_id);
    }
  }
  if (nodes.size())
    goto loop;
  
  return 0;
}

int
NdbRestarter::getMasterNodeVersion(int& version)
{
  int masterNodeId = getMasterNodeId();
  if (masterNodeId != -1)
  {
    for(size_t i = 0; i < ndbNodes.size(); i++)
    {
      if (ndbNodes[i].node_id == masterNodeId)
      {
        version =  ndbNodes[i].version;
        return 0;
      }
    }
  }

  g_err << "Could not find node info for master node id "
        << masterNodeId << endl;
  return -1;
}

int
NdbRestarter::getNodeTypeVersionRange(ndb_mgm_node_type type,
                                      int& minVer,
                                      int& maxVer)
{
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  Vector<ndb_mgm_node_state>* nodeVec = NULL;

  switch (type)
  {
  case NDB_MGM_NODE_TYPE_API:
    nodeVec = &apiNodes;
    break;
  case NDB_MGM_NODE_TYPE_NDB:
    nodeVec = &ndbNodes;
    break;
  case NDB_MGM_NODE_TYPE_MGM:
    nodeVec = &mgmNodes;
    break;
  default:
    g_err << "Bad node type : " << type << endl;
    return -1;
  }

  if (nodeVec->size() == 0)
  {
    g_err << "No nodes of type " << type << " online" << endl;
    return -1;
  }

  minVer = 0;
  maxVer = 0;
  
  for(size_t i = 0; i < nodeVec->size(); i++)
  {
    int nodeVer = (*nodeVec)[i].version;
    if ((minVer == 0) ||
        (nodeVer < minVer))
      minVer = nodeVer;
    
    if (nodeVer > maxVer)
      maxVer = nodeVer;
  }

  return 0;
}

int
NdbRestarter::getNodeStatus(int nodeid)
{
  if (getStatus() != 0)
    return -1;

  for (size_t n = 0; n < ndbNodes.size(); n++)
  {
    if (ndbNodes[n].node_id == nodeid)
      return ndbNodes[n].node_status;
  }
  return -1;
}

template class Vector<ndb_mgm_node_state>;
