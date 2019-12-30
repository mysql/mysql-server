/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __SAFE_COUNTER_HPP
#define __SAFE_COUNTER_HPP

/*************************************************************
 *
 * SafeCounter "automates" three way to node-fais safe protocols
 * for "slave" failures.  This is done by registing "fake" signals
 * to be sent in case of nodefailure.
 *
 * init<SignalClass>(..., GSN, senderData);
 *
 * It is implemented so that one can replace SignalCounter with
 * SafeCounter (SignalCounter should probably go away with time)
 * methods:
 * clearWaitingFor(nodeId);
 * done();
 * etc.
 *
 * If included in a new block method
 * SafeCounterManager::execNODE_FAILREP must included in
 * <block>::execNODE_FAILREP
 *
 * the SignalClass must have senderRef, senderData and errorCode
 * and also ErrorCode::NF_FakeErrorREF, implemented
 *
 * SafeCounter consists of 3 parts:
 * SafeCounterManager which keeps track of active "counters"
 * SafeCounterHandle to store "i-value" in your "op record"
 * SafeCounter as a temporary variable only to use on the stack
 * for operation
 *
 */

#include <NodeBitmask.hpp>
#include "IntrusiveList.hpp"
#include "VMSignal.hpp"

#define JAM_FILE_ID 286


class SimulatedBlock;

/**
 *
 */
class SafeCounterManager {
  friend class SafeCounter;
  friend class SafeCounterHandle;
  friend class SimulatedBlock;
public:
  SafeCounterManager(class SimulatedBlock &);
  
  bool setSize(Uint32 maxNoOfActiveMutexes, bool exit_on_error = true);
  Uint32 getSize() const ;
  Uint32 getNoOfFree() const;

  void execNODE_FAILREP(Signal*, const NdbNodeBitmask &failed_nodes); 
  void printNODE_FAILREP(); 

#ifdef ERROR_INSERT
  void setFakeEmpty(bool val);
#endif

private:
  struct ActiveCounter { /** sizeof = 7words = 28bytes */ 
  public:
    Uint32 m_senderData;
    NdbNodeBitmask m_nodes;
    struct SignalDesc {
    public:
      Uint16 m_gsn; 
      Uint16 m_block;
      Uint8 m_senderRefOffset;
      Uint8 m_senderDataOffset;
      Uint8 m_errorCodeOffset;
      Uint8 m_nodeFailErrorCode;
    } m_signalDesc;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };

  typedef Ptr<ActiveCounter> ActiveCounterPtr;
  typedef ArrayPool<ActiveCounter> ActiveCounter_pool;
  typedef DLList<ActiveCounter_pool> ActiveCounter_list;
  
  bool seize(ActiveCounterPtr& ptr);
  void release(ActiveCounterPtr& ptr);
  void getPtr(ActiveCounterPtr& ptr, Uint32 ptrI) const;

  SimulatedBlock & m_block;
  ActiveCounter_pool m_counterPool;
  ActiveCounter_list m_activeCounters;
#ifdef ERROR_INSERT
  bool m_fakeEmpty;
#endif

  BlockReference reference() const;
  [[noreturn]] void progError(int line,
                              int err_code,
                              const char* extra = 0,
                              const char* check="");
};


class SafeCounterHandle {
  friend class SafeCounter;
public:
  SafeCounterHandle();

  /**
   * Return if done (no nodes set in bitmask)
   */
  bool clearWaitingFor(SafeCounterManager& mgr, Uint32 nodeId);
  
  bool done() const;
  
private:
  Uint32 m_activeCounterPtrI;
};

class SafeCounter {
  friend class SafeCounterManager;
public:
  SafeCounter(SafeCounterManager&, SafeCounterHandle&);
  
  template<typename SignalClass>
    bool init(Uint16 block, Uint16 GSN, Uint32 senderData);
  
  template<typename SignalClass>
    bool init(NodeReceiverGroup rg, Uint16 GSN, Uint32 senderData);

  template<typename SignalClass>
    bool init(NodeReceiverGroup rg, Uint32 senderData);
  
  ~SafeCounter();
  
  void clearWaitingFor();
  
  /**
   * When sending to different node
   */
  void setWaitingFor(Uint32 nodeId);
  bool clearWaitingFor(Uint32 nodeId);
  bool forceClearWaitingFor(Uint32 nodeId);
  
  bool isWaitingFor(Uint32 nodeId) const;
  bool done() const;

  const char * getText() const; /* ? needed for, some portability issues */

  SafeCounter& operator=(const NdbNodeBitmask&);
  SafeCounter& operator=(const NodeReceiverGroup&);
private:
  Uint32 m_count;
  NdbNodeBitmask m_nodes;
  
  SafeCounterManager & m_mgr;
  SafeCounterManager::ActiveCounterPtr m_ptr;
  
  Uint32 & m_activeCounterPtrI;
};

inline
SafeCounterHandle::SafeCounterHandle(){
  m_activeCounterPtrI = RNIL;
}

inline
bool
SafeCounterHandle::done() const {
  return m_activeCounterPtrI == RNIL;
}

inline
SafeCounter::SafeCounter(SafeCounterManager& mgr, SafeCounterHandle& handle)
  : m_mgr(mgr),
    m_activeCounterPtrI(handle.m_activeCounterPtrI)
{
  m_ptr.i = handle.m_activeCounterPtrI;
  if (m_ptr.i == RNIL) {
    m_nodes.clear();
    m_count = 0;
  } else {
    m_mgr.getPtr(m_ptr, m_ptr.i);
    m_nodes = m_ptr.p->m_nodes;
    m_count = m_nodes.count();
  }
}

template<typename Ref>
inline
bool
SafeCounter::init(Uint16 block, Uint16 GSN, Uint32 senderData){
  
  SafeCounterManager::ActiveCounter::SignalDesc signalDesc;
  signalDesc.m_gsn = GSN;
  signalDesc.m_block = block;
  signalDesc.m_errorCodeOffset = offsetof(Ref, errorCode) >> 2;
  signalDesc.m_senderRefOffset = offsetof(Ref, senderRef) >> 2;
  signalDesc.m_senderDataOffset = offsetof(Ref, senderData) >> 2;
  signalDesc.m_nodeFailErrorCode = Ref::NF_FakeErrorREF;
  assert(((Uint32)Ref::NF_FakeErrorREF) < 256);
  
  if(m_ptr.i == RNIL){
    SafeCounterManager::ActiveCounterPtr ptr;
    if(m_mgr.seize(ptr)){
      ptr.p->m_senderData = senderData;
      ptr.p->m_signalDesc = signalDesc;
      m_ptr = ptr;
      return true;
    }
    return false;
  }

  if(m_count == 0){
    m_ptr.p->m_senderData = senderData;
    m_ptr.p->m_signalDesc = signalDesc;
    return true;
  } 

  ErrorReporter::handleAssert("SafeCounter::init twice", __FILE__, __LINE__);  
  return false;
}

template<typename Ref>
inline
bool
SafeCounter::init(NodeReceiverGroup rg, Uint16 GSN, Uint32 senderData){
  
  if (init<Ref>(rg.m_block, GSN, senderData))
  {
    m_nodes = rg.m_nodes;
    m_count = m_nodes.count();

    if (unlikely(m_count == 0))
    {
      ErrorReporter::handleAssert("SafeCounter::empty node list",
                                  __FILE__, __LINE__);
    }
    return true;
  }
  return false;
}

template<typename Ref>
inline
bool
SafeCounter::init(NodeReceiverGroup rg, Uint32 senderData)
{
  if (init<Ref>(rg.m_block, Ref::GSN, senderData))
  {
    m_nodes = rg.m_nodes;
    m_count = m_nodes.count();

    if (unlikely(m_count == 0))
    {
      ErrorReporter::handleAssert("SafeCounter::empty node list",
                                  __FILE__, __LINE__);
    }
    return true;
  }
  return false;
}

inline
void 
SafeCounter::setWaitingFor(Uint32 nodeId) {
  if(!m_nodes.get(nodeId)){
    m_nodes.set(nodeId);
    m_count++;
    return;
  }
  ErrorReporter::handleAssert("SafeCounter::set", __FILE__, __LINE__);
}

inline
bool
SafeCounter::isWaitingFor(Uint32 nodeId) const {
  return m_nodes.get(nodeId);
}

inline
bool
SafeCounter::done() const {
  return m_count == 0;
}

inline
bool
SafeCounter::clearWaitingFor(Uint32 nodeId) {
  if(m_count > 0 && m_nodes.get(nodeId)){
    m_count--;
    m_nodes.clear(nodeId);
    return (m_count == 0);
  }
  ErrorReporter::handleAssert("SafeCounter::clear", __FILE__, __LINE__);
  return false;
}

inline
void
SafeCounter::clearWaitingFor(){
  m_count = 0;
  m_nodes.clear();
}

inline
bool
SafeCounter::forceClearWaitingFor(Uint32 nodeId){
  if(isWaitingFor(nodeId)){
    return clearWaitingFor(nodeId);
  }
  return (m_count == 0);
}


#undef JAM_FILE_ID

#endif
