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

#ifndef NDB_IMPL_HPP
#define NDB_IMPL_HPP

#include <Vector.hpp>
#include "ObjectMap.hpp"

/**
 * Private parts of the Ndb object (corresponding to Ndb.hpp in public API)
 */
class NdbImpl {
public:
  Vector<class NdbTableImpl *> m_invalidTables;

  void checkErrorCode(Uint32 i);
  void checkInvalidTable(class NdbDictionaryImpl * dict);
};

#include <Ndb.hpp>
#include <NdbError.hpp>
#include <NdbCondition.h>
#include <NdbReceiver.hpp>
#include <NdbOperation.hpp>

#include <NdbTick.h>

#ifdef VM_TRACE
#define TRACE_DEBUG(x) ndbout << x << endl;
#else
#define TRACE_DEBUG(x)
#endif

#define CHECK_STATUS_MACRO \
   {if (checkInitState() == -1) { theError.code = 4100; return -1;}}
#define CHECK_STATUS_MACRO_VOID \
   {if (checkInitState() == -1) { theError.code = 4100; return;}}
#define CHECK_STATUS_MACRO_ZERO \
   {if (checkInitState() == -1) { theError.code = 4100; return 0;}}
#define CHECK_STATUS_MACRO_NULL \
   {if (checkInitState() == -1) { theError.code = 4100; return NULL;}}

inline
void *
Ndb::int2void(Uint32 val){
  return theNdbObjectIdMap->getObject(val);
}

inline
NdbReceiver *
Ndb::void2rec(void* val){
  return (NdbReceiver*)val;
}

inline
NdbConnection *
Ndb::void2con(void* val){
  return (NdbConnection*)val;
}

inline
NdbOperation*
Ndb::void2rec_op(void* val){
  return (NdbOperation*)(void2rec(val)->getOwner());
}

inline
NdbIndexOperation*
Ndb::void2rec_iop(void* val){
  return (NdbIndexOperation*)(void2rec(val)->getOwner());
}

inline 
NdbConnection * 
NdbReceiver::getTransaction(){ 
  return ((NdbOperation*)m_owner)->theNdbCon;
}


inline
int
Ndb::checkInitState()
{
  theError.code = 0;

  if (theInitState != Initialised)
    return -1;
  return 0;
}

Uint32 convertEndian(Uint32 Data);

enum WaitSignalType { 
  NO_WAIT           = 0,
  WAIT_NODE_FAILURE = 1,  // Node failure during wait
  WAIT_TIMEOUT      = 2,  // Timeout during wait

  WAIT_TC_SEIZE     = 3,
  WAIT_TC_RELEASE   = 4,
  WAIT_NDB_TAMPER   = 5,
  WAIT_SCAN         = 6,

  // DICT stuff
  WAIT_GET_TAB_INFO_REQ = 11,
  WAIT_CREATE_TAB_REQ = 12,
  WAIT_DROP_TAB_REQ = 13,
  WAIT_ALTER_TAB_REQ = 14,
  WAIT_CREATE_INDX_REQ = 15,
  WAIT_DROP_INDX_REQ = 16,
  WAIT_LIST_TABLES_CONF = 17
};

enum LockMode { 
  Read, 
  Update,
  Insert,
  Delete 
};

#include <NdbOut.hpp>

inline
void
NdbWaiter::wait(int waitTime)
{
  const bool forever = (waitTime == -1);
  const NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + waitTime;
  while (1) {
    if (m_state == NO_WAIT || m_state == WAIT_NODE_FAILURE)
      break;
    if (forever) {
      NdbCondition_Wait(m_condition, (NdbMutex*)m_mutex);
    } else {
      if (waitTime <= 0) {
        m_state = WAIT_TIMEOUT;
        break;
      }
      NdbCondition_WaitTimeout(m_condition, (NdbMutex*)m_mutex, waitTime);
      waitTime = maxTime - NdbTick_CurrentMillisecond();
    }
  }
}

inline
void
NdbWaiter::nodeFail(Uint32 aNodeId){
  if (m_state != NO_WAIT && m_node == aNodeId){
    m_state = WAIT_NODE_FAILURE;
    NdbCondition_Signal(m_condition);
  }
}

inline
void 
NdbWaiter::signal(Uint32 state){
  m_state = state;
  NdbCondition_Signal(m_condition);
}

#endif
