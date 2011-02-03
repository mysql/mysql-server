/*
   Copyright (C) 2003-2007 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef BLOCK_MUTEX_HPP
#define BLOCK_MUTEX_HPP

#include "Callback.hpp"
#include "SimulatedBlock.hpp"
#include <signaldata/UtilLock.hpp>

class Mutex;

/**
 * MutexHandle - A "reference" to a mutex
 *             - Should be used together with Mutex
 */
class MutexHandle {
  friend class Mutex;
public:
  MutexHandle(Uint32 id);
  
  bool isNull() const;
  void release(SimulatedBlock::MutexManager & mgr);

private:
  const Uint32 m_mutexId;
  Uint32 m_activeMutexPtrI;
};

/**
 * MutexHandle2 - A template-based "reference" to a mutex
 */
template<Uint32 MutexId>
class MutexHandle2 {
  friend class Mutex;
public:
  MutexHandle2();
  
  bool isNull() const;
  void release(SimulatedBlock::MutexManager & mgr);

  Uint32 getHandle() const;
  void setHandle(Uint32 handle);
  void clear(); // disassociate handle from activemutexptr

private:
  Uint32 m_activeMutexPtrI;
};

/**
 * A mutex - Used together with a MutexHandle to be put on the stack
 */
class Mutex {
public:
  Mutex(Signal*, SimulatedBlock::MutexManager & mgr, MutexHandle &);
  
  template<Uint32 MutexId>
  Mutex(Signal*, SimulatedBlock::MutexManager & mgr, MutexHandle2<MutexId> &);
  
  ~Mutex();

  void release();
  bool isNull() const ;
  
  bool lock(SimulatedBlock::Callback & callback, bool exclusive = true, bool notify = false);
  bool trylock(SimulatedBlock::Callback & callback, bool exclusive = true);
  void unlock(SimulatedBlock::Callback & callback);
  void unlock(); // Ignore callback

  bool create(SimulatedBlock::Callback & callback);
  bool destroy(SimulatedBlock::Callback & callback);

private:
  Signal* m_signal;
  SimulatedBlock::MutexManager & m_mgr;
  const Uint32 m_mutexId;
  Uint32 & m_srcPtrI;
  SimulatedBlock::MutexManager::ActiveMutexPtr m_ptr;
  
public:
  static void release(SimulatedBlock::MutexManager&, 
		      Uint32 activePtrI, Uint32 mutexId);
};

inline
MutexHandle::MutexHandle(Uint32 id) : m_mutexId(id) { 
  m_activeMutexPtrI = RNIL;
}

inline
bool
MutexHandle::isNull() const {
  return m_activeMutexPtrI == RNIL;
}

inline
void 
MutexHandle::release(SimulatedBlock::MutexManager & mgr){
  if(!isNull()){
    Mutex::release(mgr, m_activeMutexPtrI, m_mutexId);
    m_activeMutexPtrI = RNIL;
  }
}

template<Uint32 MutexId>
inline
MutexHandle2<MutexId>::MutexHandle2() { 
  m_activeMutexPtrI = RNIL;
}
  
template<Uint32 MutexId>
inline
bool 
MutexHandle2<MutexId>::isNull() const {
  return m_activeMutexPtrI == RNIL;
}


template<Uint32 MutexId>
inline
void 
MutexHandle2<MutexId>::release(SimulatedBlock::MutexManager & mgr){
  if(!isNull()){
    Mutex::release(mgr, m_activeMutexPtrI, MutexId);
    m_activeMutexPtrI = RNIL;
  }
}

template<Uint32 MutexId>
inline
Uint32
MutexHandle2<MutexId>::getHandle() const
{
  return m_activeMutexPtrI;
}

template<Uint32 MutexId>
inline
void
MutexHandle2<MutexId>::clear()
{
  m_activeMutexPtrI = RNIL;
}

template<Uint32 MutexId>
inline
void
MutexHandle2<MutexId>::setHandle(Uint32 val)
{
  if (m_activeMutexPtrI == RNIL)
  {
    m_activeMutexPtrI = val;
    return;
  }
  ErrorReporter::handleAssert("Mutex::setHandle mutex alreay inuse", 
			      __FILE__, __LINE__);
}

inline
Mutex::Mutex(Signal* signal, SimulatedBlock::MutexManager & mgr, 
	     MutexHandle & mh)
  : m_signal(signal),
    m_mgr(mgr),
    m_mutexId(mh.m_mutexId),
    m_srcPtrI(mh.m_activeMutexPtrI){

  m_ptr.i = m_srcPtrI;

}

template<Uint32 MutexId>
inline
Mutex::Mutex(Signal* signal, SimulatedBlock::MutexManager & mgr, 
	     MutexHandle2<MutexId> & mh)
  : m_signal(signal),
    m_mgr(mgr),
    m_mutexId(MutexId),
    m_srcPtrI(mh.m_activeMutexPtrI){
  
  m_ptr.i = m_srcPtrI;

}

inline
Mutex::~Mutex(){
  m_srcPtrI = m_ptr.i;
}

inline
void
Mutex::release(){
  if(!m_ptr.isNull()){
    Mutex::release(m_mgr, m_ptr.i, m_mutexId);
    m_ptr.setNull();
  }
}

inline
bool
Mutex::isNull() const {
  return m_ptr.isNull();
}

inline
bool
Mutex::lock(SimulatedBlock::Callback & callback, bool exclusive, bool notify){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.lock(m_signal, m_ptr, 
                 ((exclusive == false) ? UtilLockReq::SharedLock : 0) |
                 ((notify == true) ? UtilLockReq::Notify : 0));
      return true;
    }
    return false;
  }
  ErrorReporter::handleAssert("Mutex::lock mutex alreay inuse", 
			      __FILE__, __LINE__);
  return false;
}

inline
bool
Mutex::trylock(SimulatedBlock::Callback & callback, bool exclusive){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.lock(m_signal, m_ptr, 
                 UtilLockReq::TryLock |
                 ((exclusive == false) ? UtilLockReq::SharedLock : 0));
      return true;
    }
    return false;
  }
  ErrorReporter::handleAssert("Mutex::trylock mutex alreay inuse", 
			      __FILE__, __LINE__);
  return false;
}

inline
void
Mutex::unlock(SimulatedBlock::Callback & callback){
  if(!m_ptr.isNull()){
    m_mgr.getPtr(m_ptr);
    if(m_ptr.p->m_mutexId == m_mutexId){
      m_ptr.p->m_callback = callback;
      m_mgr.unlock(m_signal, m_ptr);
      return;
    }
  }
  ErrorReporter::handleAssert("Mutex::unlock invalid mutex", 
			      __FILE__, __LINE__);
}

inline
bool
Mutex::create(SimulatedBlock::Callback & callback){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.create(m_signal, m_ptr);
      return true;
    }
    return false;
  }
  ErrorReporter::handleAssert("Mutex::create mutex alreay inuse", 
			      __FILE__, __LINE__);
  return false;
}

inline
bool
Mutex::destroy(SimulatedBlock::Callback & callback){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.destroy(m_signal, m_ptr);
      return true;
    }
    return false;
  }
  ErrorReporter::handleAssert("Mutex::destroy mutex alreay inuse", 
			      __FILE__, __LINE__);
  return false;
}


#endif
