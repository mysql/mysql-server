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

#ifndef VMSignal_H
#define VMSignal_H

#include <ndb_global.h>
#include <ndb_limits.h>
#include <kernel_types.h>

#include <ErrorReporter.hpp>
#include <NodeBitmask.hpp>

#include <RefConvert.hpp>
#include <TransporterDefinitions.hpp>
#include <SignalCounter.hpp>

#define JAM_FILE_ID 314


extern void getSections(Uint32 secCount, SegmentedSectionPtr ptr[3]);

struct SectionHandle
{
  SectionHandle (class SimulatedBlock*);
  SectionHandle (class SimulatedBlock*, Uint32 ptrI);
  SectionHandle (class SimulatedBlock*, class Signal*);
  ~SectionHandle ();

  Uint32 m_cnt;
  SegmentedSectionPtr m_ptr[3];

  [[nodiscard]] bool getSection(SegmentedSectionPtr & ptr, Uint32 sectionNo);
  void clear() { m_cnt = 0;}

  SimulatedBlock* m_block;
};

/**
 * Struct used when sending to multiple blocks
 */
struct NodeReceiverGroup {
  NodeReceiverGroup();
  NodeReceiverGroup(Uint32 blockRef);
  NodeReceiverGroup(Uint32 blockNo, const NodeBitmask &);
  NodeReceiverGroup(Uint32 blockNo, const NdbNodeBitmask &);
  NodeReceiverGroup(Uint32 blockNo, const class SignalCounter &);
  
  NodeReceiverGroup& operator=(BlockReference ref);
  
  Uint32 m_block;
  NodeBitmask m_nodes;
};

template <unsigned T> struct SignalT
{
  Uint32 m_sectionPtrI[3];
  SignalHeader header;
  union {
    Uint32 theData[T];
    Uint64 dummyAlign;
  };

  Uint32 getLength() const { return header.theLength; }
  Uint32 getTrace() const { return header.theTrace; }
  Uint32* getDataPtrSend() { return &theData[0]; }
  Uint32 getNoOfSections() const { return header.m_noOfSections; }
};

typedef SignalT<25> Signal25;

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
  Uint32 getSignalId() const;

  const Uint32* getDataPtr() const ;
  Uint32* getDataPtrSend() ;
  
  void setTrace(Uint32);

  Uint32 getNoOfSections() const;

  /**
   * Old deprecated methods...
   */
  Uint32 length() const { return getLength();}
  BlockReference senderBlockRef() const { return getSendersBlockRef();}

  void setLength(Uint32);
  
public:

  Uint32 m_sectionPtrI[3];
  SignalHeader header; // 28 bytes
  union {
    Uint32 theData[8192];  // 8192 32-bit words -> 32K Bytes
    Uint64 dummyAlign;
  };
  /**
   * A counter used to count extra signals executed as direct signals to ensure we use
   * proper means for how often to send and flush.
   */
  Uint32 m_extra_signals;
  void garbage_register();
};

template<Uint32 len>
class SaveSignal 
{
  Uint32 m_copy[len];
  Signal * m_signal;

public:
  SaveSignal(Signal* signal) {
    save(signal);
  }

  void save(Signal* signal) {
    m_signal = signal;
    for (Uint32 i = 0; i<len; i++)
      m_copy[i] = m_signal->theData[i];
  }

  void clear() { m_signal = 0;}

  void restore() {
    for (Uint32 i = 0; i<len; i++)
      m_signal->theData[i] = m_copy[i];
  }

  ~SaveSignal() {
    if (m_signal)
      restore();
    clear();
  }
};

inline
Uint32
Signal::getLength() const {
  return header.theLength;
}

inline
Uint32
Signal::getSignalId() const
{
  return header.theSignalId;
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
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockNo, 
				     const NodeBitmask & nodes)
{
  m_block = blockNo;
  m_nodes = nodes;
}

inline
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockNo, 
				     const NdbNodeBitmask & nodes)
{
  m_block = blockNo;
  m_nodes = nodes;
}

inline
NodeReceiverGroup::NodeReceiverGroup(Uint32 blockNo, 
				     const SignalCounter & nodes){
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

inline
SectionHandle::SectionHandle(SimulatedBlock* b)
  : m_cnt(0), 
    m_block(b)
{
}

inline
SectionHandle::SectionHandle(SimulatedBlock* b, Signal* s)
  : m_cnt(s->header.m_noOfSections),
    m_block(b)
{
  Uint32 * ptr = s->m_sectionPtrI;
  Uint32 ptr0 = * ptr++;
  Uint32 ptr1 = * ptr++;
  Uint32 ptr2 = * ptr++;

  m_ptr[0].i = ptr0;
  m_ptr[1].i = ptr1;
  m_ptr[2].i = ptr2;

  getSections(m_cnt, m_ptr);

  s->header.m_noOfSections = 0;
}

inline
SectionHandle::SectionHandle(SimulatedBlock* b, Uint32 ptr)
  : m_cnt(1),
    m_block(b)
{
  m_ptr[0].i = ptr;
  getSections(1, m_ptr);
}

inline
bool
SectionHandle::getSection(SegmentedSectionPtr& ptr, Uint32 no)
{
  if (likely(no < m_cnt))
  {
    ptr = m_ptr[no];
    return true;
  }

  return false;
}

inline
SectionHandle::~SectionHandle()
{
  if (unlikely(m_cnt))
  {
    ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
                               "Unhandled sections(handle) after execute",
                               "");
  }
}


#undef JAM_FILE_ID

#endif
