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

#ifndef NDBT_RESTARTER_HPP
#define NDBT_RESTARTER_HPP

#include <mgmapi.h>
#include "mgmcommon/NdbMgm.hpp"
#include <Vector.hpp>
#include <BaseString.hpp>

#define NDBT_NO_NODE_GROUP_ID (int(-256))

class NdbRestarter {
public:
  NdbRestarter(const char* _addr = 0, class Ndb_cluster_connection * con = 0);
  ~NdbRestarter();

  int getDbNodeId(int _i);

  enum RestartFlags {
    NRRF_INITIAL = 0x1,
    NRRF_NOSTART = 0x2,
    NRRF_ABORT   = 0x4,
    NRRF_FORCE   = 0x8
  };

  int restartOneDbNode(int _nodeId, 
		       bool initial = false, 
		       bool nostart = false, 
		       bool abort = false,
                       bool force = false,
                       bool captureError = false);

  int restartOneDbNode2(int _nodeId, Uint32 flags,
                        bool captureError = false){
    return restartOneDbNode(_nodeId,
                            flags & NRRF_INITIAL,
                            flags & NRRF_NOSTART,
                            flags & NRRF_ABORT,
                            flags & NRRF_FORCE,
                            captureError);
  }

  int restartAll(bool initial = false, 
		 bool nostart = false, 
		 bool abort = false,
                 bool force = false);
  
  int restartAll2(Uint32 flags){
    return restartAll(flags & NRRF_INITIAL,
                      flags & NRRF_NOSTART,
                      flags & NRRF_ABORT,
                      flags & NRRF_FORCE);
  }

  int restartAll3(bool initial = false,
       bool nostart = false,
       bool abort = false,
       bool force = false);

  int restartNodes(int * nodes,
                   int num_nodes,
                   Uint32 flags,
                   bool captureError = false);
  
  int startAll();
  int startNodes(const int * _nodes, int _num_nodes);
  int waitConnected(unsigned int _timeout = 120);
  int waitClusterStarted(unsigned int _timeout = 120);
  int waitClusterSingleUser(unsigned int _timeout = 120);
  int waitClusterStartPhase(int _startphase, unsigned int _timeout = 120);
  int waitClusterNoStart(unsigned int _timeout = 120);  
  int waitNodesStarted(const int * _nodes, int _num_nodes,
		       unsigned int _timeout = 120);
  int waitNodesStartPhase(const int * _nodes, int _num_nodes, 
			  int _startphase, unsigned int _timeout = 120);
  int waitNodesNoStart(const int * _nodes, int _num_nodes,
		       unsigned int _timeout = 120); 

  bool checkClusterState(const int * deadnodes, int num_nodes);

  int checkClusterAlive(const int * deadnodes, int num_nodes);

  int getNumDbNodes();
  int insertErrorInNode(int _nodeId, int error);
  int insertErrorInNodes(const int * _nodes, int _num_nodes, int error);
  int insertErrorInAllNodes(int error);

  int insertError2InNode(int _nodeId, int error, int extra);
  int insertError2InNodes(const int * _nodes, int _num_nodes, int error, int extra);
  int insertError2InAllNodes(int error, int extra);

  int enterSingleUserMode(int _nodeId);
  int exitSingleUserMode();

  int dumpStateOneNode(int _nodeId, const int * _args, int _num_args);
  int dumpStateAllNodes(const int * _args, int _num_args);

  int getMasterNodeId();
  int getNextMasterNodeId(int nodeId);
  int getNodeGroup(int nodeId);
  int getNodeGroups(Vector<int>& node_groups,
                    int * max_alive_replicas_ptr = nullptr);
  int getNumNodeGroups();
  int getNumReplicas();
  int getMaxConcurrentNodeFailures();
  int getMaxFailedNodes();
  int getRandomNodeSameNodeGroup(int nodeId, int randomNumber);
  int getRandomNodeOtherNodeGroup(int nodeId, int randomNumber);
  int getRandomNodePreferOtherNodeGroup(int nodeId, int randomNumber);
  int getRandomNotMasterNodeId(int randomNumber);

  int getMasterNodeVersion(int& version);
  int getNodeTypeVersionRange(ndb_mgm_node_type type, int& minVer, int& maxVer);
  
  int getNodeStatus(int nodeId); // return NDB_MGM_NODE_STATUS_*

  /**
   * return 2 vectors with nodeId's (partitions)
   *   so that each partition can survive
   */
  Vector<Vector<int> > splitNodes();

  NdbMgmHandle handle;  

  enum NodeSelector
  {
    NS_RANDOM     = 0, // Any node
    NS_MASTER     = 1, // Master node
    NS_NON_MASTER = 2
  };

  int getNode(NodeSelector);

  void setReconnect(bool);

  int rollingRestart(Uint32 flags = 0);

  int getNodeConnectCount(int nodeId);
protected:

  int waitClusterState(ndb_mgm_node_status _status,
		       unsigned int _timeout,
		       int _startphase = -1);  

  int waitNodesState(const int * _nodes, int _num_nodes,
		     ndb_mgm_node_status _status,
		     unsigned int _timeout,
		     int _startphase = -1);

  bool isConnected();
  int connect();
  void disconnect();
  int getStatus();

  Vector<ndb_mgm_node_state> mgmNodes;
  Vector<ndb_mgm_node_state> apiNodes;
  
  bool connected;
  BaseString addr;
  ndb_mgm::config_ptr m_config;
  bool m_reconnect;
protected:
  const ndb_mgm_configuration * getConfig();

  class Ndb_cluster_connection * m_cluster_connection;
  int wait_until_ready(const int * nodes = 0, int cnt = 0, int timeout = 60);
public:
  Vector<ndb_mgm_node_state> ndbNodes;
};

#endif
