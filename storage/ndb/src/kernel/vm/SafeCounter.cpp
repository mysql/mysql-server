/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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


#include "SimulatedBlock.hpp"
#include "SafeCounter.hpp"
#include <signaldata/NodeFailRep.hpp>

#define JAM_FILE_ID 266


SafeCounterManager::SafeCounterManager(class SimulatedBlock & block)
  : m_block(block),
    m_activeCounters(m_counterPool)
{}
  
bool
SafeCounterManager::setSize(Uint32 maxNoOfActiveMutexes, bool exit_on_error) {
  return m_counterPool.setSize(maxNoOfActiveMutexes, false, exit_on_error);
}

Uint32
SafeCounterManager::getSize() const {
  return m_counterPool.getSize();
}

Uint32
SafeCounterManager::getNoOfFree() const {
  return m_counterPool.getNoOfFree();
}

bool
SafeCounterManager::seize(ActiveCounterPtr& ptr){
  return m_activeCounters.seizeFirst(ptr);
}

void
SafeCounterManager::release(ActiveCounterPtr& ptr){
  m_activeCounters.release(ptr);
}

void
SafeCounterManager::getPtr(ActiveCounterPtr& ptr, Uint32 ptrI){
  m_activeCounters.getPtr(ptr, ptrI);
}


void
SafeCounterManager::printNODE_FAILREP(){
  ActiveCounterPtr ptr;

  NdbNodeBitmask nodes;
  nodes.clear();
  //  nodes.bitORC(nodes);

  for(m_activeCounters.first(ptr); !ptr.isNull(); m_activeCounters.next(ptr)){
    ActiveCounter::SignalDesc desc = ptr.p->m_signalDesc;
    ndbout_c("theData[desc.m_senderDataOffset=%u] = %u",
	     desc.m_senderDataOffset, ptr.p->m_senderData);
    ndbout_c("theData[desc.m_errorCodeOffset=%u] = %u",
	     desc.m_errorCodeOffset, desc.m_nodeFailErrorCode);
    Uint32 len = MAX(MAX(desc.m_senderDataOffset, desc.m_errorCodeOffset),
		     desc.m_senderRefOffset);
    
    NdbNodeBitmask overlapping = ptr.p->m_nodes;
    Uint32 i = 0;
    while((i = overlapping.find(i)) != NdbNodeBitmask::NotFound){
      ndbout_c("  theData[desc.m_senderRefOffset=%u] = %x",
	       desc.m_senderRefOffset, numberToRef(desc.m_block, i));
      ndbout_c("  sendSignal(%x,%u,signal,%u,JBB",
	       m_block.reference(), desc.m_gsn, len+1);
      i++;
    }
  }
}

void
SafeCounterManager::execNODE_FAILREP(Signal* signal){
  Uint32 * theData = signal->getDataPtrSend();
  ActiveCounterPtr ptr;
  NdbNodeBitmask nodes;
  nodes.assign(NdbNodeBitmask::Size, 
	       ((const NodeFailRep*)signal->getDataPtr())->theNodes);

  for(m_activeCounters.first(ptr); !ptr.isNull(); m_activeCounters.next(ptr)){
    if(nodes.overlaps(ptr.p->m_nodes)){
      ActiveCounter::SignalDesc desc = ptr.p->m_signalDesc;
      theData[desc.m_senderDataOffset] = ptr.p->m_senderData;
      theData[desc.m_errorCodeOffset] = desc.m_nodeFailErrorCode;
      Uint32 len = MAX(MAX(desc.m_senderDataOffset, desc.m_errorCodeOffset),
		       desc.m_senderRefOffset);
      
      NdbNodeBitmask overlapping = ptr.p->m_nodes;
      overlapping.bitAND(nodes);
      Uint32 i = 0;
      while((i = overlapping.find(i)) != NdbNodeBitmask::NotFound){
	theData[desc.m_senderRefOffset] = numberToRef(desc.m_block, i);
	m_block.sendSignal(m_block.reference(), desc.m_gsn, signal, len+1,JBB);
	i++;
      }
    }
  }
}

BlockReference
SafeCounterManager::reference() const {
  return m_block.reference();
}

void
SafeCounterManager::progError(int line, int err_code, const char* extra){
  m_block.progError(line, err_code, extra);
}

bool
SafeCounterHandle::clearWaitingFor(SafeCounterManager& mgr, Uint32 nodeId)
{
  SafeCounterManager::ActiveCounterPtr ptr;
  mgr.getPtr(ptr, m_activeCounterPtrI);
  ptr.p->m_nodes.clear(nodeId);
  
  if (ptr.p->m_nodes.isclear()){
    mgr.release(ptr);
    m_activeCounterPtrI = RNIL;
    return true;
  }
  return false;
}

SafeCounter::~SafeCounter(){
  bool clear = m_count == 0;
  bool isnull = m_ptr.i == RNIL;

  m_activeCounterPtrI = m_ptr.i;

  if(clear && isnull)
    return;

  if(clear && !isnull){
    m_mgr.release(m_ptr);
    m_activeCounterPtrI = RNIL;
    return;
  }

  /**
   * !clear && !isnull
   */
  if(!isnull){
    m_ptr.p->m_nodes = m_nodes;
    return;
  }

  ErrorReporter::handleAssert("~SafeCounter:: wo/ init", __FILE__, __LINE__);
}
