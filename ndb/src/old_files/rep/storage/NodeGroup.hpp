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

#ifndef NODE_GROUP_HPP
#define NODE_GROUP_HPP

#include "NodeConnectInfo.hpp"
#include <Vector.hpp>
#include <ndb_types.h>

#include <rep/rep_version.hpp>

/**
 * @class NodeGroup
 * @brief Contains info about all nodes belonging to one node group
 */
class NodeGroup {
public:
  NodeGroup(Uint32 nodeGrp);
  ~NodeGroup();
  /**
   * Add node to node group
   * @param	nodeId     Node id of node to add
   * @param	connected  Status of this node (true==connected)
   */
  void		addNode(Uint32 nodeId, bool connected);

  /**
   * get first connected node in this node group
   * @returns	nodeId, 0 if there is no connected node...
   */
  Uint32	getFirstConnectedNode();

  /**
   * get the primary node id
   * @returns	nodeId, the primary node id
   */
  Uint32	getPrimaryNode() {return m_primaryNode;};


  /**
   * sets a node in this nodegroup as the primary node
   */
  void	        setPrimaryNode(Uint32 nodeId) {m_primaryNode=nodeId;};


  /**
   * get the node group
   * @returns	the nodegroup number (m_nodeGrp)
   */
  Uint32	getNodeGrp();

  /**
   * set the connection status for a particular node
   * @param	nodeId - the nodeId to set the connect status on
   * @param	connected - the status of this node (true==connected)
   */
  void		setNodeConnectStatus(Uint32 nodeId, bool connected);

  /**
   * Get the connection status for a particular node
   * @param	nodeId - the nodeId to check the connect status on
   * @returns	true if node is connected, otherwise false
   */
  bool		isConnected(Uint32 nodeId);

  /**
   * gives the status of this nodegroup.
   * @returns	true if atleast one node in the node group is connected
   */
  bool		connectedNodeGrp();

  /** 
   * @returns   true if ALL nodes are connected
   */
  bool		fullyConnected();

  /**
   * 
   * @returns	true if node exists in nodegroup
   */
  bool		exists(Uint32 nodeId);

  Vector <NodeConnectInfo *> * getNodeConnectList();
  
private:
  /**
   * Sort list (bubble sort)
   */
  void sort();
  Uint32                     m_primaryNode;
  Uint32	             m_nodeGrp;
  Vector<NodeConnectInfo *>  m_nodeConnectList;
};

#endif
