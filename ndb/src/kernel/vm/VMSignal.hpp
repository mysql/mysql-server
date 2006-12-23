/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef VMSignal_H
#define VMSignal_H

#include <ndb_global.h>
#include <ndb_limits.h>
#include <kernel_types.h>

#include <ErrorReporter.hpp>
#include <NodeBitmask.hpp>

#include <RefConvert.hpp>
#include <TransporterDefinitions.hpp>

/**
 * Struct used when sending to multiple blocks
 */
struct NodeReceiverGroup {
  NodeReceiverGroup();
  NodeReceiverGroup(Uint32 blockRef);
  NodeReceiverGroup(Uint32 blockNo, const NodeBitmask &);
  NodeReceiverGroup(Uint32 blockNo, const class SignalCounter &);
  
  NodeReceiverGroup& operator=(BlockReference ref);
  
  Uint32 m_block;
  NodeBitmask m_nodes;
};

template <unsigned T> struct SignalT
{
  SignalHeader header;
  SegmentedSectionPtr m_sectionPtr[3]; 
  union {
    Uint32 theData[T];
    Uint64 dummyAlign;
  };
};

/**
 * class used for passing argumentes to blocks
 */
class Signal {
  friend class SimulatedBlock;
  friend class APZJobBuffer;
  friend class FastScheduler;
public:
  Signal();
  
  Uint32 getLength() const;
  Uint32 getTrace() const;
  Uint32 getSendersBlockRef() const;

  const Uint32* getDataPtr() const ;
  Uint32* getDataPtrSend() ;
  
  void setTrace(Uint32);

  Uint32 getNoOfSections() const;
  bool getSection(SegmentedSectionPtr & ptr, Uint32 sectionNo);
  void setSection(SegmentedSectionPtr ptr, Uint32 sectionNo);

  /**
   * Old depricated methods...
   */
  Uint32 length() const { return getLength();}
  BlockReference senderBlockRef() const { return getSendersBlockRef();}

private:
  void setLength(Uint32);
  
public:
#define VMS_DATA_SIZE \
  (MAX_ATTRIBUTES_IN_TABLE + MAX_TUPLE_SIZE_IN_WORDS + MAX_KEY_SIZE_IN_WORDS)

#if VMS_DATA_SIZE > 8192
#error "VMSignal buffer is too small"
#endif
  
  SignalHeader header; // 28 bytes
  SegmentedSectionPtr m_sectionPtr[3]; 
  union {
    Uint32 theData[8192];  // 8192 32-bit words -> 32K Bytes
    Uint64 dummyAlign;
  };
  void garbage_register();
};

inline
Uint32
Signal::getLength() const {
  return header.theLength;
}

inline
Uint32
Signal::getTrace() const {
  return header.theTrace;
}

inline
Uint32
Signal::getSendersBlockRef() const {
  return header.theSendersBlockRef;
}

inline
const Uint32* 
Signal::getDataPtr() const { 
  return &theData[0];
}

inline
Uint32* 
Signal::getDataPtrSend() { 
  return &theData[0];
}

inline
void
Signal::setLength(Uint32 len){
  header.theLength = len;
}

inline
void
Signal::setTrace(Uint32 t){
  header.theTrace = t;
}

inline
Uint32 
Signal::getNoOfSections() const {
  return header.m_noOfSections;
}

inline
bool 
Signal::getSection(SegmentedSectionPtr & ptr, Uint32 section){
  if(section < header.m_noOfSections){
    ptr = m_sectionPtr[section];
    return true;
  }
  ptr.p = 0;
  return false;
}

inline
void
Signal::setSection(SegmentedSectionPtr ptr, Uint32 sectionNo){
  if(sectionNo != header.m_noOfSections || sectionNo > 2){
    abort();
  }
  m_sectionPtr[sectionNo] = ptr;
  header.m_noOfSections++;
}

inline
NodeReceiverGroup::NodeReceiverGroup() : m_block(0){
  m_nodes.clear();
}

inline
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockRef){
  m_nodes.clear();
  m_block = refToBlock(blockRef);
  m_nodes.set(refToNode(blockRef));
}

inline
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockNo, const NodeBitmask & nodes){
  m_block = blockNo;
  m_nodes = nodes;
}

#include "SignalCounter.hpp"

inline
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockNo, const SignalCounter & nodes){
  m_block = blockNo;
  m_nodes = nodes.m_nodes;
}

inline
NodeReceiverGroup& 
NodeReceiverGroup::operator=(BlockReference blockRef){
  m_nodes.clear();
  m_block = refToBlock(blockRef);
  m_nodes.set(refToNode(blockRef));
  return * this;
}

#endif
