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

#include "NodeGroupInfo.hpp"

NodeGroupInfo::NodeGroupInfo() 
{
}

NodeGroupInfo::~NodeGroupInfo() 
{
  for(Uint32 i=0; i<m_nodeGroupList.size(); i++) {
    delete m_nodeGroupList[i];
  }
  m_nodeGroupList.clear();
}


void
NodeGroupInfo::setPrimaryNode(Uint32 nodeGrp, Uint32 nodeId) {
  Uint32 pos;
  /**
   * Validation check to find out that the nodegroup really exists.
   * The list is not sorted, so the index of the nodegroup is returned
   * in pos.
   */
  if(existsNodeGroup(nodeGrp, &pos)) {
    m_nodeGroupList[pos]->setPrimaryNode(nodeId);
  } else {
    /**
     * could not find node group
     */
    RLOG(("Node group not found"));
    REPABORT("Node group not found");
  }
}

Uint32
NodeGroupInfo::getPrimaryNode(Uint32 nodeGrp) {
  Uint32 pos;
  /**
   * Validation check to find out that the nodegroup really exists.
   * The list is not sorted, so the index of the nodegroup is returned
   * in pos.
   */
  if(existsNodeGroup(nodeGrp, &pos)) {
    return m_nodeGroupList[pos]->getPrimaryNode();
  } else {
    /**
     * could not find node group
     */
    RLOG(("Node group not found"));
    REPABORT("Node group not found");
  }
}

void		
NodeGroupInfo::addNodeToNodeGrp(Uint32 nodeId, bool connected, Uint32 nodeGrp)
{
  Uint32 pos;
  if(existsNodeGroup(nodeGrp, &pos)) {
    /**
     *  NG exists -> just add the node
     */
    m_nodeGroupList[pos]->addNode(nodeId, connected);

  } else {
    /**
     *  NG do not exist -> create a new nodeGrp and add the node
     */
    m_nodeGroupList.push_back(new NodeGroup(nodeGrp));
    
    /**
     * paranoia
     */
    if(existsNodeGroup(nodeGrp, &pos)) {
      m_nodeGroupList[pos]->addNode(nodeId, connected);
    } else {
      REPABORT("");
    }
  }
}

Uint32
NodeGroupInfo::findNodeGroup(Uint32 nodeId) 
{
  /**
   *  Check for existance in each nodegroup
   */
  for(Uint32 i=0; i<m_nodeGroupList.size(); i++) {
    if(m_nodeGroupList[i]->exists(nodeId)) return i;
  }

  REPABORT1("No node group known for node", nodeId);
}

Uint32 
NodeGroupInfo::getFirstConnectedNode(Uint32 nodeGrp) 
{
  Uint32 pos;
  /**
   * Validation check to find out that the nodegroup really exists.
   * The list is not sorted, so the index of the nodegroup is returned
   * in pos.
   */
  if(existsNodeGroup(nodeGrp, &pos)) {
    return m_nodeGroupList[pos]->getFirstConnectedNode();
  } else {
    /**
     * could not find node group
     */
    REPABORT("");
  }
}

bool
NodeGroupInfo::connectedNodeGrp(Uint32 nodeGrp) 
{
  return m_nodeGroupList[nodeGrp]->connectedNodeGrp();
}

bool
NodeGroupInfo::isConnected(Uint32 nodeId) 
{
  Uint32 nodeGrp = findNodeGroup(nodeId);
  return m_nodeGroupList[nodeGrp]->isConnected(nodeId);
 
}

bool
NodeGroupInfo::fullyConnected() 
{
  for(Uint32 i=0; i<m_nodeGroupList.size(); i++) {
    if(!(m_nodeGroupList[i]->fullyConnected()))
      return false;
  }
  return true;
}


void
NodeGroupInfo::setConnectStatus(Uint32 nodeId, bool connected) 
{
  Uint32 nodeGrp = findNodeGroup(nodeId);
  m_nodeGroupList[nodeGrp]->setNodeConnectStatus(nodeId,connected);
}


bool
NodeGroupInfo::existsNodeGroup(Uint32 nodeGrp, Uint32 * pos)
{
  for(Uint32 i=0; i<m_nodeGroupList.size(); i++) {
    if(m_nodeGroupList[i]->getNodeGrp()==nodeGrp) {
      *pos=i;
      return true;
    }
  }
  return false;
}


/*****************************************************************************
 * Iterator
 *****************************************************************************/

NodeGroupInfo::iterator::iterator(Uint32 nodeGrp, NodeGroupInfo * ngi) 
{
  m_iterator = 0;
  for(Uint32 i=0; i < ngi->m_nodeGroupList.size(); i++) {
    if(ngi->m_nodeGroupList[i]->getNodeGrp()==nodeGrp) {
      m_nodeList = ngi->m_nodeGroupList[i]->getNodeConnectList();
      return;
    }
  }
  m_nodeList=0;
}

bool
NodeGroupInfo::iterator::exists() 
{
  if(m_nodeList==0) return 0;
  return (m_iterator < m_nodeList->size());
}

NodeConnectInfo *
NodeGroupInfo::iterator::first() 
{
  m_iterator=0;
  if(m_nodeList==0) return 0;
  if(m_nodeList->size() == 0) return 0;
  return (*m_nodeList)[m_iterator];
}

NodeConnectInfo *
NodeGroupInfo::iterator::next() 
{
  m_iterator++;
  if(m_nodeList==0) return 0;
  if(m_nodeList->size() == 0) return 0;
  if(m_iterator<m_nodeList->size())
    return (*m_nodeList)[m_iterator];
  else
    return 0;
}

