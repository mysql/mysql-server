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

#ifndef NODE_GROUPINFO_HPP
#define NODE_GROUPINFO_HPP

#include <Vector.hpp>
#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
//#include <NdbSleep.h>

#include "NodeGroup.hpp"
#include <rep/rep_version.hpp>

/**
 * @class NodeGroupInfo
 * @brief Contains info about all node groups and their connectivity status
 */
class NodeGroupInfo {
public:
  NodeGroupInfo();
  ~NodeGroupInfo();
  
  /**
   * Add a node to a nodegroup together with the status of the node
   * @param	nodeId - the nodeId to add
   * @param	connected - true/false
   * @param	nodeGrp	- the nodegroup to add this node to
   */
  void	       addNodeToNodeGrp(Uint32 nodeId, bool connected, Uint32 nodeGrp);

  /**
   * Get the nodegroup that a node belongs to.
   * @param	nodeId - the nodeId to check wich nodegroup it belongs to
   * @return	the nodegroup
   */
  Uint32	findNodeGroup(Uint32 nodeId);

  /**
   * Get the first connected node in a node group
   * @param	nodegroup - the node group to get the node in.
   * @return	nodeId, 0 if there is no connected node in the nodegroup
   */
  Uint32	getFirstConnectedNode(Uint32 nodeGrp);


  /**
   * sets a nodeId in a nodeGroup as the primary node. If the 
   * primary node fails, then a new node in the node group is chosen
   * @param	nodegroup - the node group to get the node in.
   * @param	nodeId, 0 if there is no connected node in the nodegroup
   */
  void	        setPrimaryNode(Uint32 nodeGrp, Uint32 nodeId);
  
  /**
   * gets the nodeId in the nodegroup of the primary node.
   * @param	nodegroup - the node group to get the node in.
   * @return	nodeId, 0 if there is no connected node in the nodegroup
   */
  Uint32        getPrimaryNode(Uint32 nodeGrp);


  /**
   * Checks if at least one node in the nodegroup is connected.
   * @param	nodeGrp - the nodegrp to check 
   * @return	true if >0 nodes are connected in the nodegroup
   */
  bool		connectedNodeGrp(Uint32 nodeGrp);

  /**
   * Checks if a node is connected or not
   * @param	nodeId - the nodeId to check connectivity
   * @return	true if node is connected
   */
  bool		isConnected(Uint32 nodeId);

  /**
   * Set if a node is connected or not
   * @param	nodeId - the nodeId to set the connect flag fory
   * @param	connected - true if connect false if disconnect
   */
  void		setConnectStatus(Uint32 nodeId, bool connected);

  /**
   * Check if all nodes are connected in all nodegroups
   * @return	return true if ALL nodes are connected in ALL nodeGroups
   */
  bool		fullyConnected();

  /**
   * Get the number of nodegroups
   * @return	the number of nodegroups.
   */
  Uint32	getNoOfNodeGroups() { return m_nodeGroupList.size();};

  /**
   * @class iterator
   * The iterator class iterates over a nodegroup, returning nodeIds
   * in that node group.
   *   
   * @code  
   *    NodeGroupInfo::iterator * it;  
   *    for(Uint32 i=0;i < m_nodeGroupInfo->getNoOfNodeGroups();i++) {
   *       it = new NodeGroupInfo::iterator(i,m_nodeGroupInfo);
   *       for(NodeConnectInfo * nci=it->first(); it->exists();nci=it->next())
   *           ndbout_c("Iterating: %d", nci->nodeId);
   *       
   *    }
   * @end code
   */
  class iterator {
  public:
    iterator(Uint32 nodeGrp, NodeGroupInfo * ngi);
    NodeConnectInfo * first();  ///< @return nodeConnectInfo* if exists.
				///< (NULL if no more nodes exists)
    NodeConnectInfo * next();  ///< @return nodeConnectInfo* if exists.
				///< (NULL if no more nodes exists)
    bool exists();      ///< @return true if another nodeId exists (for next())
  private:
    Uint32             m_iterator;
    const Vector<NodeConnectInfo *> * m_nodeList;
  };
  friend class NodeGroupInfo::iterator;
  
private:
  bool		existsNodeGroup(Uint32 nodeGrp, Uint32 * pos);
  
  Vector<NodeGroup *>	m_nodeGroupList;
};

#endif
