/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>
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
#include <ndb_cluster_connection.hpp>
#include <ndb_rand.h>

#define MGMERR(h) \
  ndbout << "latest_error="<<ndb_mgm_get_latest_error(h) \
	 << ", line="<<ndb_mgm_get_latest_error_line(h) \
         << ", mesg="<<ndb_mgm_get_latest_error_msg(h) \
         << ", desc="<<ndb_mgm_get_latest_error_desc(h) \
	 << endl;


NdbRestarter::NdbRestarter(const char* _addr, Ndb_cluster_connection * con):
  handle(NULL),
  connected(false),
  m_reconnect(false),
  m_cluster_connection(con)
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

  for(unsigned i = 0; i < ndbNodes.size(); i++){     
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
                              bool force,
                              bool captureError)
{
  return restartNodes(&_nodeId, 1,
                      (inital ? NRRF_INITIAL : 0) |
                      (nostart ? NRRF_NOSTART : 0) |
                      (abort ? NRRF_ABORT : 0) |
                      (force ? NRRF_FORCE : 0),
                      captureError);
}

int
NdbRestarter::restartNodes(int * nodes, int cnt,
                           Uint32 flags, bool captureError)
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
     * way down. If that's the case ignore the error.
     *
     * Bug #11757421 is a special case where the
     * error code and description is required in
     * the test case. The call to getStatus()
     * overwrites the error and is thus avoided
     * by adding an option to capture the error.
     */ 

    if (!captureError && getStatus() != 0)
      return -1;

    g_info << "ndb_mgm_restart4 returned with error, checking node state"
           << endl;

    for (int j = 0; j<cnt; j++)
    {
      int _nodeId = nodes[j];
      for(unsigned i = 0; i < ndbNodes.size(); i++)
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

  if ((flags & NRRF_NOSTART) == 0)
  {
    wait_until_ready(nodes, cnt);
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
  for(unsigned i = 0; i < ndbNodes.size(); i++){
    if(min == 0 || ndbNodes[i].dynamic_id < min){
      min = ndbNodes[i].dynamic_id;
      node = ndbNodes[i].node_id;
    }
  }

  return node;
}

int NdbRestarter::getNodeGroup(int nodeId) {
  if (!isConnected()) {
    g_err << "getNodeGroup failed: Not connected to ndb_mgmd!!" << endl;
    return -1;
  }

  if (getStatus() != 0) {
    g_err << "getNodeGroup failed: Failed to get status!!" << endl;
    return -1;
  }

  ndbout << "Node ids from ndb_mgm:- " << endl;
  for (unsigned i = 0; i < ndbNodes.size(); i++) {
    ndbout << "ndbNodes[" << i << "].node_id = " << ndbNodes[i].node_id << endl;
    if (ndbNodes[i].node_id == nodeId) {
      return ndbNodes[i].node_group;
    }
  }
  g_err << "getNodeGroup failed: Node with id " << nodeId
         << " not found in mgm!!" << endl;
  return -1;
}

/* getNodeGroups()
   Both parameters are OUT params.
   Returns -1 on error, or the number of configured node groups on success.
*/
int
NdbRestarter::getNodeGroups(Vector<int>& node_groups, int * max_alive_replicas_ptr)
{
  if (!isConnected())
  {
    g_err << "getNodeGroup failed: Not connected to ndb_mgmd!!" << endl;
    return -1;
  }

  if (getStatus() != 0)
  {
    g_err << "getNodeGroup failed: Failed to get status!!" << endl;
    return -1;
  }

  int n_groups = 0;
  Vector<int> node_group_replicas;
  for (unsigned i = 0; i < ndbNodes.size(); i++)
  {
    const unsigned node_group = ndbNodes[i].node_group;
    if (node_group == RNIL)
    {
      // Data node without node group
      continue;
    }
    require(node_group < RNIL);

    // Grow vector if needed.
    int zero_replicas = 0;
    node_group_replicas.fill(node_group + 1, zero_replicas);

    // If not seen node group before, add it.
    if (node_group_replicas[node_group] == 0)
    {
      node_groups.push_back(node_group);
      n_groups++;
    }

    node_group_replicas[node_group]++;
  }

  if (max_alive_replicas_ptr != nullptr)
  {
    int max_alive_replicas = 0;
    for (unsigned i = 0; i < node_group_replicas.size(); i++)
    {
      const int ng_replicas = node_group_replicas[i];
      if (max_alive_replicas < ng_replicas)
      {
        max_alive_replicas = ng_replicas;
      }
    }
    *max_alive_replicas_ptr = max_alive_replicas;
  }
  return n_groups;
}

int NdbRestarter::getNumNodeGroups() {
  Vector<int> node_group_list;
  return getNodeGroups(node_group_list);
}

int NdbRestarter::getNumReplicas() {
  Vector<int> node_group_list;
  int replicas;
  (void) getNodeGroups(node_group_list, &replicas);
  return replicas;
}

/* Calculate the number of data nodes that can fail at the same time,
   which is half the total number of data nodes (rounded down) if
   there are two or more replicas of the data.
*/
int NdbRestarter::getMaxConcurrentNodeFailures() {
  return (getNumReplicas() < 2) ? 0 : getNumDbNodes() / 2;
}

/* Calculate the total number of data nodes that can eventually fail.
   In each replica set, one node must remain running.
*/
int NdbRestarter::getMaxFailedNodes() {
  Vector<int> node_group_list;
  int replicas;
  int ngroups = getNodeGroups(node_group_list, &replicas);
  return (replicas-1) * ngroups;
}

int
NdbRestarter::getNextMasterNodeId(int nodeId){
  if (!isConnected())
    return -1;
  
  if (getStatus() != 0)
    return -1;
  
  unsigned i;
  for(i = 0; i < ndbNodes.size(); i++)
  {
    if(ndbNodes[i].node_id == nodeId)
    {
      break;
    }
  }
  require(i < ndbNodes.size());
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

  // find nodegroup corresponding to nodeId
  for (unsigned i = 0; i < ndbNodes.size(); i++)
  {
    if (ndbNodes[i].node_id == nodeId &&
        ndbNodes[i].node_group <= MAX_NDB_NODE_GROUPS)
    {
      node_group = ndbNodes[i].node_group;
      break;
    }
  }
  if(node_group == -1){
    return -1;
  }

  Uint32 counter = 0;
  rand = rand % ndbNodes.size();

  // find random node not belonging to node_group
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
  // find nodegroup corresponding to nodeId
  for(unsigned i = 0; i < ndbNodes.size(); i++){
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

  // find random node which is not nodeId, belonging to node_group
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
NdbRestarter::getRandomNodePreferOtherNodeGroup(int nodeId, int rand) {
  int n = getRandomNodeOtherNodeGroup(nodeId, rand);
  if(n == -1) n = getRandomNodeSameNodeGroup(nodeId, rand);
  return n;
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
  int res = waitClusterState(NDB_MGM_NODE_STATUS_STARTED, _timeout);
  if (res == 0)
  {
    wait_until_ready();
  }
  return res;
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
  for (unsigned i = 0; i < ndbNodes.size(); i++){
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
	for (unsigned n = 0; n < ndbNodes.size(); n++){
	  if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED &&
	      ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTING)
	  {
            // Found one not starting node, don't wait anymore
	    waitMore = false;
            break;
          }

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

    for (int i = 0; i < _num_nodes; i++)
    {
      // Find node with given node id
      ndb_mgm_node_state* ndbNode = NULL;
      for (unsigned n = 0; n < ndbNodes.size(); n++)
      {
        if (ndbNodes[n].node_id == _nodes[i])
        {
          ndbNode = &ndbNodes[n];
          break;
        }
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

      require(ndbNode != NULL);

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
  int res = waitNodesState(_nodes, _num_nodes,
                           NDB_MGM_NODE_STATUS_STARTED, _timeout);
  if (res == 0)
  {
    wait_until_ready(_nodes, _num_nodes);
  }

  return res;
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
  g_info << "Connecting to management server at " << addr.c_str() << endl;
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


int NdbRestarter::restartAll3(bool initial,
           bool nostart,
           bool abort,
                             bool force)
{
  /*
   * This function has been added since restartAll
   * and restartAll2 both include handling various
   * cases of restart failure. Some cases such as
   * Bug #11757421 require the handling of failures
   * to be done in the test itself as the error
   * returned is of interest.
   */

  if (!isConnected())
      return -1;

  int unused;
  if (ndb_mgm_restart4(handle, 0, NULL, initial, 1, abort,
                       force, &unused) <= 0)
  {
    MGMERR(handle);
    g_err  << "Could not stop nodes" << endl;
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

int NdbRestarter::insertErrorInNodes(const int * _nodes, int _num_nodes, int _error)
{
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int result = 0;

  for(int i = 0; i < _num_nodes ; i++)
  {
    g_debug << "inserting error in node " << _nodes[i] << endl;
    if (insertErrorInNode(_nodes[i], _error) == -1)
      result = -1;
  }
  return result;
}

int NdbRestarter::insertErrorInAllNodes(int _error){
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int result = 0;
 
  for(unsigned i = 0; i < ndbNodes.size(); i++){     
    g_debug << "inserting error in node " << ndbNodes[i].node_id << endl;
    if (insertErrorInNode(ndbNodes[i].node_id, _error) == -1)
      result = -1;
  }
  return result;

}

int
NdbRestarter::insertError2InNode(int _nodeId, int _error, int extra){
  if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  if (ndb_mgm_insert_error2(handle, _nodeId, _error, extra, &reply) == -1){
    MGMERR(handle);
    g_err << "Could not insert error in node with id = "<< _nodeId << endl;
  }
  if(reply.return_code != 0){
    g_err << "Error: " << reply.message << endl;
  }
  return 0;
}

int NdbRestarter::insertError2InNodes(const int * _nodes, int _num_nodes, int _error, int extra)
{
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int result = 0;

  for(int i = 0; i < _num_nodes ; i++)
  {
    g_debug << "inserting error in node " << _nodes[i] << endl;
    if (insertError2InNode(_nodes[i], _error, extra) == -1)
      result = -1;
  }
  return result;
}

int NdbRestarter::insertError2InAllNodes(int _error, int extra){
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int result = 0;

  for(unsigned i = 0; i < ndbNodes.size(); i++){
    g_debug << "inserting error in node " << ndbNodes[i].node_id << endl;
    if (insertError2InNode(ndbNodes[i].node_id, _error, extra) == -1)
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
 
 for(unsigned i = 0; i < ndbNodes.size(); i++){     
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

/*
   Fetch configuration from ndb_mgmd unless config has already been fetched
   (and thus cached earlier). Return pointer to configuration.
*/
const ndb_mgm_configuration* NdbRestarter::getConfig(){
  if(m_config) return m_config.get();

  if (!isConnected())
    return 0;
  m_config.reset(ndb_mgm_get_configuration(handle, 0));
  return m_config.get();
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

bool
in_node_list(const int *dead_nodes, int num_dead_nodes, int nodeId)
{
  for (int i = 0; i < num_dead_nodes; i++)
  {
    if (dead_nodes[i] == nodeId)
      return true;
  }
  return false;
}

bool
NdbRestarter::checkClusterState(const int *dead_nodes, int num_dead_nodes)
{
  if (getStatus() != 0)
    return false;

  for (unsigned n = 0; n < ndbNodes.size(); n++)
  {
    if (in_node_list(dead_nodes, num_dead_nodes, ndbNodes[n].node_id))
    {
      if (ndbNodes[n].node_status == NDB_MGM_NODE_STATUS_STARTED)
      {
        ndbout_c("Node %d started, expected dead", ndbNodes[n].node_id);
        return false;
      }
    }
    else
    {
      if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED)
      {
        ndbout_c("Node %d dead, expected started", ndbNodes[n].node_id);
        return false;
      }
    }
  }
  return true;
}

int
NdbRestarter::checkClusterAlive(const int * deadnodes, int num_nodes)
{
  if (getStatus() != 0)
    return -1;
  
  NdbNodeBitmask mask;
  for (int i = 0; i<num_nodes; i++)
    mask.set(deadnodes[i]);
  
  for (unsigned n = 0; n < ndbNodes.size(); n++)
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
  for(unsigned i = 0; i < ndbNodes.size(); i++)
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
    for(unsigned i = 0; i < ndbNodes.size(); i++)
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
  
  for(unsigned i = 0; i < nodeVec->size(); i++)
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

  for (unsigned n = 0; n < ndbNodes.size(); n++)
  {
    if (ndbNodes[n].node_id == nodeid)
      return ndbNodes[n].node_status;
  }
  return -1;
}


static uint
urandom(uint m)
{
  require(m != 0);
  uint n = (uint)ndb_rand();
  return n % m;
}

Vector<Vector<int> >
NdbRestarter::splitNodes()
{
  // Vector of parts. Each part has the NodeIds of nodes belonging to it.
  Vector<Vector<int>> parts;

  // Vector of node group masks
  Vector<Bitmask<255>>ngMasks;

  for (int i = 0; i < getNumDbNodes(); i++)
  {
    int nodeId = getDbNodeId(i);
    int ng = getNodeGroup(nodeId);
    unsigned numOfNGKnown = ngMasks.size();
    unsigned j = 0;
    for (j = 0; j < numOfNGKnown; j++)
    {
      if (ngMasks[j].get(ng) == false)
      {
        // parts[j] doesn't have node belonging to ng, add to parts[j]
        parts[j].push_back(nodeId);

        // set ng in ngMasks[j] so we know it's there in parts[j]
        ngMasks[j].set(ng);
        break;
      }
    }
    if (j == numOfNGKnown)
    {
      /**
       * It's the first node we're looping through, or;
       * there's already one node in each part that has a nodeId belonging to
       * ng. So, create new part and new ng bitmask
       */

      Vector<int> newPart;
      Bitmask<255> newNGMask;

      // add the nodeId to a new part
      newPart.push_back(nodeId);
      parts.push(newPart, urandom(numOfNGKnown + 1));

      // set ng in a new bitmask, add to ngMasks
      newNGMask.set(ng);
      ngMasks.push_back(newNGMask);
    }
  }

  g_debug << "Number of parts: " << parts.size() << endl;
  g_debug << "Number of masks: " << ngMasks.size() << endl;
  return parts;
}

int
NdbRestarter::wait_until_ready(const int * nodes, int cnt, int timeout)
{
  if (m_cluster_connection == 0)
  {
    // no cluster connection, skip wait
    return 0;
  }

  Vector<int> allNodes;
  if (cnt == 0)
  {
    if (!isConnected())
      return -1;

    if (getStatus() != 0)
      return -1;

    for(unsigned i = 0; i < ndbNodes.size(); i++)
    {
      allNodes.push_back(ndbNodes[i].node_id);
    }
    cnt = (int)allNodes.size();
    nodes = allNodes.getBase();
  }

  return m_cluster_connection->wait_until_ready(nodes, cnt, timeout);
}

int
NdbRestarter::getNodeConnectCount(int nodeId)
{
  if (getStatus() != 0)
    return -1;

  for (unsigned n = 0; n < ndbNodes.size(); n++)
  {
    if (ndbNodes[n].node_id == nodeId)
      return ndbNodes[n].connect_count;
  }
  return -1;
}

template class Vector<ndb_mgm_node_state>;
template class Vector<Vector<int> >;
