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

#include "GCIContainerPS.hpp"
#include <NdbOut.hpp>
#include <NdbMem.h>
#include <new>

GCIContainerPS::GCIContainerPS(Uint32 maxNoOfNodeGrps) 
{
  m_container = new GCIContainer(maxNoOfNodeGrps);
  if (!m_container) REPABORT("Could not allocate new GCIContainer");
}

GCIContainerPS::~GCIContainerPS() 
{
  delete m_container;
}

void
GCIContainerPS::setNodeGroupInfo(NodeGroupInfo * info) 
{
  m_nodeGroupInfo=info;
};

void 
GCIContainerPS::createGCIBuffer(Uint32 gci, Uint32 id) 
{
  m_container->createGCIBuffer(gci, id);
}

void 
GCIContainerPS::getAvailableGCIBuffers(Uint32 id /*nodegrp */, 
				       Uint32 * first, Uint32 * last) {

  Uint32 nodeId = m_nodeGroupInfo->getPrimaryNode(id);
  if(!nodeId) {
    *first = 1;
    *last  = 0;
    return;
  }

  /** 
   *@todo do smart stuff with this!
   */
  m_container->getAvailableGCIBuffers(nodeId, first, last);
	   
}
  
void 
GCIContainerPS::destroyGCIBuffersBeforeGCI(Uint32 gci) 
{
  //for each node in every nodeGrp do:
  NodeGroupInfo::iterator * it;  
  for(Uint32 i=0; i<m_nodeGroupInfo->getNoOfNodeGroups(); i++) {
    it = new NodeGroupInfo::iterator(i, m_nodeGroupInfo);
    for(NodeConnectInfo * nci=it->first(); it->exists();nci=it->next()) {
      m_container->destroyGCIBuffersBeforeGCI(gci, nci->nodeId);
    }
    delete it;
  }
}

void
GCIContainerPS::insertLogRecord(Uint32 id, Uint32 tableId, Uint32 operation,
				class LinearSectionPtr ptr[3], Uint32 gci) 
{
  m_container->insertLogRecord(id, tableId, operation, ptr, gci);
}

void
GCIContainerPS::insertMetaRecord(Uint32 id, Uint32 tableId, 
				 class LinearSectionPtr ptr[3], Uint32 gci) 
{
  m_container->insertMetaRecord(id, tableId, ptr, gci);
}

void
GCIContainerPS::setCompleted(Uint32 gci, Uint32 id) 
{
  m_container->setCompleted(gci, id);
}

GCIBuffer *
GCIContainerPS::getGCIBuffer(Uint32 gci, Uint32 id) 
{
  return m_container->getGCIBuffer(gci, id);
}

/**
 * @todo: fix return value
 */
bool
GCIContainerPS::destroyGCIBuffer(Uint32 gci, Uint32 id) 
{
  //for each node in  nodeGrp id  do:
  NodeGroupInfo::iterator * it;  
  it = new NodeGroupInfo::iterator(id, m_nodeGroupInfo);
  for(NodeConnectInfo * nci=it->first(); it->exists();nci=it->next()) 
  {
    if(!m_container->destroyGCIBuffer(gci, nci->nodeId)) 
    {
      delete it;
      return false;
    }
  }
  delete it;
  return true;
}

bool
GCIContainerPS::reset() 
{
  return m_container->reset();
}
