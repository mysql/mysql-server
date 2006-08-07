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

#ifndef NDBT_RESTARTER_HPP
#define NDBT_RESTARTER_HPP

#include <mgmapi.h>
#include <Vector.hpp>
#include <BaseString.hpp>

class NdbRestarter {
public:
  NdbRestarter(const char* _addr = 0);
  ~NdbRestarter();

  int getDbNodeId(int _i);

  int restartOneDbNode(int _nodeId, 
		       bool initial = false, 
		       bool nostart = false, 
		       bool abort = false);

  int restartAll(bool initial = false, 
		 bool nostart = false, 
		 bool abort = false);
  
  int startAll();
  int startNodes(const int * _nodes, int _num_nodes);
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


  int getNumDbNodes();
  int insertErrorInNode(int _nodeId, int error);
  int insertErrorInAllNodes(int error);

  int enterSingleUserMode(int _nodeId);
  int exitSingleUserMode();

  int dumpStateOneNode(int _nodeId, const int * _args, int _num_args);
  int dumpStateAllNodes(const int * _args, int _num_args);

  int getMasterNodeId();
  int getRandomNodeSameNodeGroup(int nodeId, int randomNumber);
  int getRandomNodeOtherNodeGroup(int nodeId, int randomNumber);
  int getRandomNotMasterNodeId(int randomNumber);
  
  NdbMgmHandle handle;  
  
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
  ndb_mgm_configuration * m_config;
protected:
  ndb_mgm_configuration * getConfig();

public:  
  Vector<ndb_mgm_node_state> ndbNodes;
};

#endif
