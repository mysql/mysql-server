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

#include "NodeGroup.hpp"
#include <NdbOut.hpp>

//#define NODE_GROUP_DEBUG

NodeGroup::NodeGroup(Uint32 nodeGrp) {
  m_nodeGrp = nodeGrp;
  m_primaryNode = 0;
}

NodeGroup::~NodeGroup() {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++) {
    delete m_nodeConnectList[i];
    m_nodeConnectList.erase(i);
  }
}

void 
NodeGroup::addNode(Uint32 nodeId, bool connected) {
#ifdef NODE_GROUP_DEBUG
  ndbout_c("NodeGroup: addNode(nodeId=%d, connected=%d), nodegrp=%d",
	   nodeId, connected, m_nodeGrp);
#endif

  /**
   *  If node already in node group, then do nothing except
   *  setting the connect statusflag for the node (in case it
   *  has changed).
   */
  for(Uint32 i=0; i < m_nodeConnectList.size(); i++) 
    if(m_nodeConnectList[i]->nodeId == nodeId) {
      m_nodeConnectList[i]->connected = connected;
      return;
    }

  /**
   *  If node not already in node group, then add node
   */
  m_nodeConnectList.push_back(new NodeConnectInfo(nodeId, connected));
  sort();

#ifdef NODE_GROUP_DEBUG
  for(Uint32 i=0; i < m_nodeConnectList.size(); i++) 
    ndbout_c("NodeGroup: NodeId=%d", m_nodeConnectList[i]->nodeId);
#endif
}

/**
 * crappy sort
 */
void NodeGroup::sort() {
  NodeConnectInfo * tmp;
  if(m_nodeConnectList.size()<2)
    return;
  for(Uint32 i=0; i < m_nodeConnectList.size()-1; i++) {
    for(Uint32 j=m_nodeConnectList.size()-1;j>i+1; j--) {
      if(m_nodeConnectList[j]->nodeId < m_nodeConnectList[j-1]->nodeId) {
	tmp=m_nodeConnectList[j];
	m_nodeConnectList[j]=m_nodeConnectList[j-1];
	m_nodeConnectList[j-1]=tmp;
      }
    }
  }
}

Uint32	
NodeGroup::getFirstConnectedNode() {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++){
    if(m_nodeConnectList[i]->connected) 
      return m_nodeConnectList[i]->nodeId;
  }
  return 0;
}

Uint32 
NodeGroup::getNodeGrp() {
  return m_nodeGrp;
}

Vector <NodeConnectInfo *> *
NodeGroup::getNodeConnectList(){
  return &m_nodeConnectList;
}

void
NodeGroup::setNodeConnectStatus(Uint32 nodeId, bool connected) {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++){
    if(m_nodeConnectList[i]->nodeId==nodeId) {
      m_nodeConnectList[i]->connected=connected;
      break;
    }
  }
} 

bool
NodeGroup::isConnected(Uint32 nodeId) {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++){
    if(m_nodeConnectList[i]->nodeId == nodeId) {
      return m_nodeConnectList[i]->connected;
    }
  }
  REPABORT1("Check for non-existing node to be connected", nodeId);
}


bool
NodeGroup::fullyConnected() {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++){
    if(!(m_nodeConnectList[i]->connected)) 
      return false;
  }
  return true;
}

bool	
NodeGroup::connectedNodeGrp() {
  for(Uint32 i=0; i<m_nodeConnectList.size(); i++){
    if(m_nodeConnectList[i]->connected) {
      return true;
    }
  }
  return false;
}


bool
NodeGroup::exists(Uint32 nodeId) {
  for(Uint32 i=0;i<m_nodeConnectList.size();i++) {
    if(m_nodeConnectList[i]->nodeId==nodeId)
      return true;
  }  
  return false;
}
