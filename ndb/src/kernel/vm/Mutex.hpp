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

#ifndef BLOCK_MUTEX_HPP
#define BLOCK_MUTEX_HPP

#include "Callback.hpp"
#include "SimulatedBlock.hpp"

class Mutex;

class MutexManager {
  friend class Mutex;
  friend class SimulatedBlock;
  friend class DbUtil;
public:
  MutexManager(class SimulatedBlock &);
  
  bool setSize(Uint32 maxNoOfActiveMutexes);
  Uint32 getSize() const ; // Get maxNoOfActiveMutexes

private:
  /**
   * core interface
   */
  struct ActiveMutex {
    Uint32 m_gsn; // state
    Uint32 m_mutexId;
    Uint32 m_mutexKey;
    Callback m_callback;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<ActiveMutex> ActiveMutexPtr;
  
  bool seize(ActiveMutexPtr& ptr);
  void release(Uint32 activeMutexPtrI);
  
  void getPtr(ActiveMutexPtr& ptr);
  
  void create(Signal*, ActiveMutexPtr&);
  void destroy(Signal*, ActiveMutexPtr&);
  void lock(Signal*, ActiveMutexPtr&);
  void trylock(Signal*, ActiveMutexPtr&);
  void unlock(Signal*, ActiveMutexPtr&);
  
private:
  void execUTIL_CREATE_LOCK_REF(Signal* signal);
  void execUTIL_CREATE_LOCK_CONF(Signal* signal);
  void execUTIL_DESTORY_LOCK_REF(Signal* signal);
  void execUTIL_DESTORY_LOCK_CONF(Signal* signal);
  void execUTIL_LOCK_REF(Signal* signal);
  void execUTIL_LOCK_CONF(Signal* signal);
  void execUTIL_UNLOCK_REF(Signal* signal);
  void execUTIL_UNLOCK_CONF(Signal* signal);

  SimulatedBlock & m_block;
  ArrayPool<ActiveMutex> m_mutexPool;
  DLList<ActiveMutex> m_activeMutexes;

  BlockReference reference() const;
  void progError(int line, int err_code, const char* extra = 0);
};


/**
 * MutexHandle - A "reference" to a mutex
 *             - Should be used together with Mutex
 */
class MutexHandle {
  friend class Mutex;
public:
  MutexHandle(Uint32 id);
  
  bool isNull() const;
  void release(MutexManager & mgr);

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
  void release(MutexManager & mgr);

private:
  Uint32 m_activeMutexPtrI;
};

/**
 * A mutex - Used together with a MutexHandle to be put on the stack
 */
class Mutex {
public:
  Mutex(Signal*, MutexManager & mgr, MutexHandle &);
  
  template<Uint32 MutexId>
  Mutex(Signal*, MutexManager & mgr, MutexHandle2<MutexId> &);
  
  ~Mutex();

  void release();
  bool isNull() const ;
  
  bool lock(Callback & callback);
  bool trylock(Callback & callback);
  void unlock(Callback & callback);
  void unlock(); // Ignore callback
  
  bool create(Callback & callback);
  bool destroy(Callback & callback);

private:
  Signal* m_signal;
  MutexManager & m_mgr;
  const Uint32 m_mutexId;
  Uint32 & m_srcPtrI;
  MutexManager::ActiveMutexPtr m_ptr;
  
public:
  static void release(MutexManager&, Uint32 activePtrI, Uint32 mutexId);
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
MutexHandle::release(MutexManager & mgr){
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
MutexHandle2<MutexId>::release(MutexManager & mgr){
  if(!isNull()){
    Mutex::release(mgr, m_activeMutexPtrI, MutexId);
    m_activeMutexPtrI = RNIL;
  }
}


inline
Mutex::Mutex(Signal* signal, MutexManager & mgr, MutexHandle & mh)
  : m_signal(signal),
    m_mgr(mgr),
    m_mutexId(mh.m_mutexId),
    m_srcPtrI(mh.m_activeMutexPtrI){

  m_ptr.i = m_srcPtrI;

}

template<Uint32 MutexId>
inline
Mutex::Mutex(Signal* signal, MutexManager & mgr, MutexHandle2<MutexId> & mh)
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
Mutex::lock(Callback & callback){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.lock(m_signal, m_ptr);
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
Mutex::trylock(Callback & callback){
  if(m_ptr.isNull()){
    if(m_mgr.seize(m_ptr)){
      m_ptr.p->m_mutexId = m_mutexId;
      m_ptr.p->m_callback = callback;
      m_mgr.lock(m_signal, m_ptr);
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
Mutex::unlock(Callback & callback){
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
Mutex::create(Callback & callback){
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
Mutex::destroy(Callback & callback){
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
