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


#include "SimulatedBlock.hpp"
#include "Mutex.hpp"
#include <signaldata/UtilLock.hpp>

#define JAM_FILE_ID 293


SimulatedBlock::MutexManager::MutexManager(class SimulatedBlock & block) 
  : m_block(block),
    m_activeMutexes(m_mutexPool) {
}
  
bool
SimulatedBlock::MutexManager::setSize(Uint32 maxNoOfActiveMutexes){
  return m_mutexPool.setSize(maxNoOfActiveMutexes);
}

Uint32
SimulatedBlock::MutexManager::getSize() const {
  return m_mutexPool.getSize();
}

bool
SimulatedBlock::MutexManager::seize(ActiveMutexPtr& ptr){
  return m_activeMutexes.seizeFirst(ptr);
}

void
SimulatedBlock::MutexManager::release(Uint32 activeMutexPtrI){
  m_activeMutexes.release(activeMutexPtrI);
}

void
SimulatedBlock::MutexManager::getPtr(ActiveMutexPtr& ptr) const
{
  m_activeMutexes.getPtr(ptr);
}

BlockReference
SimulatedBlock::MutexManager::reference() const { 
  return m_block.reference(); 
}

void
SimulatedBlock::MutexManager::progError(int line, 
					int err_code, 
					const char* extra,
					const char* check)
{
  m_block.progError(line, err_code, extra, check);
}

void
SimulatedBlock::MutexManager::create(Signal* signal, ActiveMutexPtr& ptr){
  
  UtilCreateLockReq * req = (UtilCreateLockReq*)signal->getDataPtrSend();
  req->senderData = ptr.i;
  req->senderRef = m_block.reference();
  req->lockId = ptr.p->m_mutexId;
  req->lockType = UtilCreateLockReq::Mutex;

  m_block.sendSignal(DBUTIL_REF, 
		     GSN_UTIL_CREATE_LOCK_REQ, 
		     signal,
		     UtilCreateLockReq::SignalLength,
		     JBB);

  ptr.p->m_gsn = GSN_UTIL_CREATE_LOCK_REQ;
}

void
SimulatedBlock::MutexManager::execUTIL_CREATE_LOCK_REF(Signal* signal){

  UtilCreateLockRef * ref = (UtilCreateLockRef*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, ref->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_CREATE_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == ref->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, ref->errorCode);
}

void 
SimulatedBlock::MutexManager::execUTIL_CREATE_LOCK_CONF(Signal* signal){

  UtilCreateLockConf * conf = (UtilCreateLockConf*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, conf->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_CREATE_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == conf->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, 0);
}


void
SimulatedBlock::MutexManager::destroy(Signal* signal, ActiveMutexPtr& ptr){

  UtilDestroyLockReq * req = (UtilDestroyLockReq*)signal->getDataPtrSend();
  req->senderData = ptr.i;
  req->senderRef = m_block.reference();
  req->lockId = ptr.p->m_mutexId;

  m_block.sendSignal(DBUTIL_REF, 
		     GSN_UTIL_DESTROY_LOCK_REQ, 
		     signal,
		     UtilDestroyLockReq::SignalLength,
		     JBB);

  ptr.p->m_gsn = GSN_UTIL_DESTROY_LOCK_REQ;
}

void
SimulatedBlock::MutexManager::execUTIL_DESTORY_LOCK_REF(Signal* signal){
  UtilDestroyLockRef * ref = (UtilDestroyLockRef*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, ref->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_DESTROY_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == ref->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, ref->errorCode);
}

void
SimulatedBlock::MutexManager::execUTIL_DESTORY_LOCK_CONF(Signal* signal){
  UtilDestroyLockConf * conf = (UtilDestroyLockConf*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, conf->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_DESTROY_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == conf->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, 0);
}


void 
SimulatedBlock::MutexManager::lock(Signal* signal, 
                                   ActiveMutexPtr& ptr,
                                   Uint32 flags){

  UtilLockReq * req = (UtilLockReq*)signal->getDataPtrSend();
  req->senderData = ptr.i;
  req->senderRef = m_block.reference();
  req->lockId = ptr.p->m_mutexId;
  req->requestInfo = flags;
  req->extra = ptr.p->m_callback.m_callbackData;

  m_block.sendSignal(DBUTIL_REF, 
		     GSN_UTIL_LOCK_REQ, 
		     signal,
		     UtilLockReq::SignalLength,
		     JBB);
  
  ptr.p->m_gsn = GSN_UTIL_LOCK_REQ;
}

void
SimulatedBlock::MutexManager::execUTIL_LOCK_REF(Signal* signal){
  UtilLockRef * ref = (UtilLockRef*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, ref->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == ref->lockId);

  bool notify = ref->errorCode == UtilLockRef::InLockQueue;
  CallbackFunction fun = ptr.p->m_callback.m_callbackFunction; 

  if (!notify)
  {
    ptr.p->m_gsn = 0;
  }
  m_block.execute(signal, ptr.p->m_callback, ref->errorCode);
  
  if (notify)
  {
    // execute clears function so that same callback shouldn't be called twice
    ptr.p->m_callback.m_callbackFunction = fun;
  }
}

void
SimulatedBlock::MutexManager::execUTIL_LOCK_CONF(Signal* signal){
  UtilLockConf * conf = (UtilLockConf*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, conf->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_LOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == conf->lockId);
  
  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, 0);
}

void 
SimulatedBlock::MutexManager::unlock(Signal* signal, ActiveMutexPtr& ptr){
  UtilUnlockReq * req = (UtilUnlockReq*)signal->getDataPtrSend();
  req->senderData = ptr.i;
  req->senderRef = m_block.reference();
  req->lockId = ptr.p->m_mutexId;
  
  m_block.sendSignal(DBUTIL_REF, 
		     GSN_UTIL_UNLOCK_REQ, 
		     signal,
		     UtilUnlockReq::SignalLength,
		     JBB);
  
  ptr.p->m_gsn = GSN_UTIL_UNLOCK_REQ;
}

void
SimulatedBlock::MutexManager::execUTIL_UNLOCK_REF(Signal* signal){
  UtilUnlockRef * ref = (UtilUnlockRef*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, ref->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_UNLOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == ref->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, ref->errorCode);
}

void
SimulatedBlock::MutexManager::execUTIL_UNLOCK_CONF(Signal* signal){
  UtilUnlockConf * conf = (UtilUnlockConf*)signal->getDataPtr();
  ActiveMutexPtr ptr;
  m_activeMutexes.getPtr(ptr, conf->senderData);
  ndbrequire(ptr.p->m_gsn == GSN_UTIL_UNLOCK_REQ);
  ndbrequire(ptr.p->m_mutexId == conf->lockId);

  ptr.p->m_gsn = 0;
  m_block.execute(signal, ptr.p->m_callback, 0);
}

void
Mutex::release(SimulatedBlock::MutexManager& mgr, 
	       Uint32 activePtrI, Uint32 mutexId){
  SimulatedBlock::MutexManager::ActiveMutexPtr ptr;
  ptr.i = activePtrI;
  mgr.getPtr(ptr);
  if(ptr.p->m_gsn == 0 && ptr.p->m_mutexId == mutexId){
    mgr.release(activePtrI);
    return;
  }
  
  if(ptr.p->m_mutexId != mutexId)
    ErrorReporter::handleAssert("MutexHandle::release invalid handle", 
				__FILE__, __LINE__);
  ErrorReporter::handleAssert("MutexHandle::release of mutex inuse", 
			      __FILE__, __LINE__);
}

void
Mutex::unlock(){
  if(!m_ptr.isNull()){
    m_mgr.getPtr(m_ptr);
    if(m_ptr.p->m_mutexId == m_mutexId){
      SimulatedBlock::Callback c = 
	{ &SimulatedBlock::ignoreMutexUnlockCallback, m_ptr.i };
      m_ptr.p->m_callback = c;
      m_mgr.unlock(m_signal, m_ptr);
      m_ptr.setNull(); // Remove reference
    }
  }
}

