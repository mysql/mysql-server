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


#include <ndb_global.h>
#include <kernel_types.h>

#include "NdbDictionaryImpl.hpp"
#include "API.hpp"
#include <NdbOut.hpp>
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include <signaldata/CreateEvnt.hpp>
#include <signaldata/SumaImpl.hpp>
#include <SimpleProperties.hpp>
#include <Bitmask.hpp>
#include <AttributeHeader.hpp>
#include <AttributeList.hpp>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <UtilBuffer.hpp>
#include <NdbDictionary.hpp>
#include <Ndb.hpp>
#include "NdbImpl.hpp"
#include "DictCache.hpp"
#include <portlib/NdbMem.h>
#include <NdbRecAttr.hpp>
#include <NdbEventOperation.hpp>
#include "NdbEventOperationImpl.hpp"

/*
 * Class NdbEventOperationImpl
 *
 *
 */

//#define EVENT_DEBUG


NdbEventOperationImpl::NdbEventOperationImpl(NdbEventOperation &N,
					     Ndb *theNdb, 
					     const char* eventName, 
					     const int bufferLength) 
  : NdbEventOperation(*this), m_ndb(theNdb),
    m_state(ERROR), m_bufferL(bufferLength)
{

  m_eventId = 0;
  theFirstRecAttrs[0] = NULL;
  theCurrentRecAttrs[0] = NULL;
  theFirstRecAttrs[1] = NULL;
  theCurrentRecAttrs[1] = NULL;
  sdata = NULL;
  ptr[0].p = NULL;
  ptr[1].p = NULL;
  ptr[2].p = NULL;

  // we should lookup id in Dictionary, TODO
  // also make sure we only have one listener on each event

  if (!m_ndb) { ndbout_c("m_ndb=NULL"); return; }

  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) { ndbout_c("getDictionary=NULL"); return; }

  const NdbDictionary::Event *myEvnt = myDict->getEvent(eventName);
  if (!myEvnt) { ndbout_c("getEvent()=NULL"); return; }

  m_eventImpl = &myEvnt->m_impl;
  if (!m_eventImpl) { ndbout_c("m_impl=NULL"); return; }

  m_bufferHandle = m_ndb->getGlobalEventBufferHandle();
  if (m_bufferHandle->m_bufferL > 0) 
    m_bufferL =m_bufferHandle->m_bufferL;
  else
    m_bufferHandle->m_bufferL = m_bufferL;

  m_state = CREATED;
}

NdbEventOperationImpl::~NdbEventOperationImpl()
{
  int i;
  if (sdata) NdbMem_Free(sdata);
  for (i=0 ; i<3; i++) {
    if (ptr[i].p) NdbMem_Free(ptr[i].p);
  }
  for (i=0 ; i<2; i++) {
    NdbRecAttr *p = theFirstRecAttrs[i];
    while (p) {
      NdbRecAttr *p_next = p->next();
      m_ndb->releaseRecAttr(p);
      p = p_next;
    }
  }
  if (m_state == NdbEventOperation::EXECUTING) {
    stop();
    // m_bufferHandle->dropSubscribeEvent(m_bufferId);
    ; // We should send stop signal here
  }
}

NdbEventOperation::State
NdbEventOperationImpl::getState()
{
  return m_state;
}

NdbRecAttr*
NdbEventOperationImpl::getValue(const char *colName, char *aValue, int n)
{
  if (m_state != NdbEventOperation::CREATED) {
    ndbout_c("NdbEventOperationImpl::getValue may only be called between instantiation and execute()");
    return NULL;
  }

  NdbColumnImpl *tAttrInfo = m_eventImpl->m_tableImpl->getColumn(colName);

  if (tAttrInfo == NULL) {
    ndbout_c("NdbEventOperationImpl::getValue attribute %s not found",colName);
    return NULL;
  }

  return NdbEventOperationImpl::getValue(tAttrInfo, aValue, n);
}

NdbRecAttr*
NdbEventOperationImpl::getValue(const NdbColumnImpl *tAttrInfo, char *aValue, int n)
{
  // Insert Attribute Id into ATTRINFO part. 
  NdbRecAttr *&theFirstRecAttr = theFirstRecAttrs[n];
  NdbRecAttr *&theCurrentRecAttr = theCurrentRecAttrs[n];
      
  /************************************************************************
   *	Get a Receive Attribute object and link it into the operation object.
   ************************************************************************/
  NdbRecAttr *tRecAttr = m_ndb->getRecAttr();
  if (tRecAttr == NULL) { 
    exit(-1);
    //setErrorCodeAbort(4000);
    return NULL;
  }

  /**********************************************************************
   * Now set the attribute identity and the pointer to the data in 
   * the RecAttr object
   * Also set attribute size, array size and attribute type
   ********************************************************************/
  if (tRecAttr->setup(tAttrInfo, aValue)) {
    //setErrorCodeAbort(4000);
    m_ndb->releaseRecAttr(tRecAttr);
    exit(-1);
    return NULL;
  }
  //theErrorLine++;

  tRecAttr->setNULL();
  
  // We want to keep the list sorted to make data insertion easier later
  if (theFirstRecAttr == NULL) {
    theFirstRecAttr = tRecAttr;
    theCurrentRecAttr = tRecAttr;
    tRecAttr->next(NULL);
  } else {
    Uint32 tAttrId = tAttrInfo->m_attrId;
    if (tAttrId > theCurrentRecAttr->attrId()) { // right order
      theCurrentRecAttr->next(tRecAttr);
      tRecAttr->next(NULL);
      theCurrentRecAttr = tRecAttr;
    } else if (theFirstRecAttr->next() == NULL ||    // only one in list
	       theFirstRecAttr->attrId() > tAttrId) {// or first 
      tRecAttr->next(theFirstRecAttr);
      theFirstRecAttr = tRecAttr;
    } else { // at least 2 in list and not first and not last
      NdbRecAttr *p = theFirstRecAttr;
      NdbRecAttr *p_next = p->next();
      while (tAttrId > p_next->attrId()) {
	p = p_next;
	p_next = p->next();
      }
      if (tAttrId == p_next->attrId()) { // Using same attribute twice
	tRecAttr->release(); // do I need to do this?
	m_ndb->releaseRecAttr(tRecAttr);
	exit(-1);
	return NULL;
      }
      // this is it, between p and p_next
      p->next(tRecAttr);
      tRecAttr->next(p_next);
    }
  }

  return tRecAttr;
}

int
NdbEventOperationImpl::execute()
{
  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) {
    ndbout_c("NdbEventOperation::execute(): getDictionary=NULL");
    return 0;
  }

  if (theFirstRecAttrs[0] == NULL) { // defaults to get all
    
  }

  NdbDictionaryImpl & myDictImpl = NdbDictionaryImpl::getImpl(*myDict);


  int hasSubscriber;
  m_bufferId = 
    m_bufferHandle->prepareAddSubscribeEvent(m_eventImpl->m_eventId,
					     hasSubscriber /* return value */);

  m_eventImpl->m_bufferId = m_bufferId;

  int r = -1;
  if (m_bufferId >= 0) {
    // now we check if there's already a subscriber

    if (hasSubscriber == 0) { // only excute if there's no other subscribers 
      r = myDictImpl.executeSubscribeEvent(*m_eventImpl);
    } else {
      r = 0;
    }
    if (r) {
      //Error
      m_bufferHandle->unprepareAddSubscribeEvent(m_bufferId);
      m_state = NdbEventOperation::ERROR;
    } else {
      m_bufferHandle->addSubscribeEvent(m_bufferId, this);
      m_state = NdbEventOperation::EXECUTING;
    }
  } else {
    //Error
    m_state = NdbEventOperation::ERROR;
  }
  return r;
}

int
NdbEventOperationImpl::stop()
{
  if (m_state != NdbEventOperation::EXECUTING)
    return -1;

  //  ndbout_c("NdbEventOperation::stopping()");

  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) {
    ndbout_c("NdbEventOperation::stop(): getDictionary=NULL");
    return 0;
  }

  NdbDictionaryImpl & myDictImpl = NdbDictionaryImpl::getImpl(*myDict);

  int hasSubscriber;
  int ret = 
    m_bufferHandle->prepareDropSubscribeEvent(m_bufferId,
					      hasSubscriber /* return value */);

  if (ret < 0) {
    ndbout_c("prepareDropSubscribeEvent failed");
    return -1;
  }
  //  m_eventImpl->m_bufferId = m_bufferId;

  int r = -1;

  if (hasSubscriber == 0) { // only excute if there's no other subscribers
    r = myDictImpl.stopSubscribeEvent(*m_eventImpl);
#ifdef EVENT_DEBUG
    ndbout_c("NdbEventOperation::stopping() done");
#endif
  } else
    r = 0;

  if (r) {
    //Error
    m_bufferHandle->unprepareDropSubscribeEvent(m_bufferId);
    m_state = NdbEventOperation::ERROR;
  } else {
#ifdef EVENT_DEBUG
    ndbout_c("NdbEventOperation::dropping()");
#endif
    m_bufferHandle->dropSubscribeEvent(m_bufferId);
    m_state = NdbEventOperation::CREATED;
  }


  return r;
}

bool
NdbEventOperationImpl::isConsistent()
{
  return sdata->isGCIConsistent();
}

Uint32
NdbEventOperationImpl::getGCI()
{
  return sdata->gci;
}

Uint32
NdbEventOperationImpl::getLatestGCI()
{
  return NdbGlobalEventBufferHandle::getLatestGCI();
}

int
NdbEventOperationImpl::next(int *pOverrun)
{
  int nr = 10000; // a high value
  int tmpOverrun = 0;
  int *ptmpOverrun;
  if (pOverrun) {
    ptmpOverrun = &tmpOverrun;
  } else
    ptmpOverrun = NULL;

  while (nr > 0) {
    int r=NdbGlobalEventBufferHandle::getDataL(m_bufferId, sdata,
					       ptr, pOverrun);
    if (pOverrun) {
      tmpOverrun += *pOverrun;
      *pOverrun = tmpOverrun;
    }

    if (r <= 0) return r; // no data

    if (r < nr) r = nr; else nr--; // we don't want to be stuck here forever
  
#ifdef EVENT_DEBUG
    ndbout_c("!!!!!!!sdata->operation %u", (Uint32)sdata->operation);
#endif

    // now move the data into the RecAttrs
    if ((theFirstRecAttrs[0] == NULL) && 
	(theFirstRecAttrs[1] == NULL)) return r;
    // no copying since no RecAttr's


    Uint32 *aAttrPtr = ptr[0].p;
    Uint32 *aAttrEndPtr = aAttrPtr + ptr[0].sz;
    Uint32 *aDataPtr = ptr[1].p;

#ifdef EVENT_DEBUG
    printf("after values sz=%u\n", ptr[1].sz);
    for(int i=0; i < ptr[1].sz; i++)
      printf ("H'%.8X ",ptr[1].p[i]);
    printf("\n");
    printf("before values sz=%u\n", ptr[2].sz);
    for(int i=0; i < ptr[2].sz; i++)
      printf ("H'%.8X ",ptr[2].p[i]);
    printf("\n");
#endif

    NdbRecAttr *tWorkingRecAttr = theFirstRecAttrs[0];

    // copy data into the RecAttr's
    // we assume that the respective attribute lists are sorted

    Uint32 tRecAttrId;
    Uint32 tAttrId;
    Uint32 tDataSz;
    int hasSomeData=0;
    while ((aAttrPtr < aAttrEndPtr) && (tWorkingRecAttr != NULL)) {
      tRecAttrId = tWorkingRecAttr->attrId();
      tAttrId = AttributeHeader(*aAttrPtr).getAttributeId();
      tDataSz = AttributeHeader(*aAttrPtr).getDataSize();
      
      while (tAttrId > tRecAttrId) {
	//printf("[%u] %u %u [%u]\n", tAttrId, tDataSz, *aDataPtr, tRecAttrId);
	tWorkingRecAttr->setNULL();
	tWorkingRecAttr = tWorkingRecAttr->next();
	if (tWorkingRecAttr == NULL)
	  break;
	tRecAttrId = tWorkingRecAttr->attrId();
      }
      if (tWorkingRecAttr == NULL)
	break;
      
      //printf("[%u] %u %u [%u]\n", tAttrId, tDataSz, *aDataPtr, tRecAttrId);
      
      if (tAttrId == tRecAttrId) {
	if (!m_eventImpl->m_tableImpl->getColumn(tRecAttrId)->getPrimaryKey())
	  hasSomeData++;
	
	//printf("set!\n");
	
	tWorkingRecAttr->receive_data(aDataPtr, tDataSz);
	
	// move forward, data has already moved forward
	aAttrPtr++;
	aDataPtr += tDataSz;
	tWorkingRecAttr = tWorkingRecAttr->next();
      } else {
	// move only attr forward
	aAttrPtr++;
	aDataPtr += tDataSz;
      }
    }
    
    while (tWorkingRecAttr != NULL) {
      tRecAttrId = tWorkingRecAttr->attrId();
      //printf("set undefined [%u] %u %u [%u]\n", tAttrId, tDataSz, *aDataPtr, tRecAttrId);
      tWorkingRecAttr->setNULL();
      tWorkingRecAttr = tWorkingRecAttr->next();
    }
    
    tWorkingRecAttr = theFirstRecAttrs[1];
    aDataPtr = ptr[2].p;
    Uint32 *aDataEndPtr = aDataPtr + ptr[2].sz;
    while ((aDataPtr < aDataEndPtr) && (tWorkingRecAttr != NULL)) {
      tRecAttrId = tWorkingRecAttr->attrId();
      tAttrId = AttributeHeader(*aDataPtr).getAttributeId();
      tDataSz = AttributeHeader(*aDataPtr).getDataSize();
      aDataPtr++;
      while (tAttrId > tRecAttrId) {
	tWorkingRecAttr->setNULL();
	tWorkingRecAttr = tWorkingRecAttr->next();
	if (tWorkingRecAttr == NULL)
	  break;
	tRecAttrId = tWorkingRecAttr->attrId();
      }
      if (tWorkingRecAttr == NULL)
	break;
      if (tAttrId == tRecAttrId) {
	if (!m_eventImpl->m_tableImpl->getColumn(tRecAttrId)->getPrimaryKey())
	  hasSomeData++;
	
	tWorkingRecAttr->receive_data(aDataPtr, tDataSz);
	aDataPtr += tDataSz;
	// move forward, data+attr has already moved forward
	tWorkingRecAttr = tWorkingRecAttr->next();
      } else {
	// move only data+attr forward
	aDataPtr += tDataSz;
      }
    }
    while (tWorkingRecAttr != NULL) {
      tWorkingRecAttr->setNULL();
      tWorkingRecAttr = tWorkingRecAttr->next();
    }
    
    if (hasSomeData)
      return r;
  }
  return 0;
}

NdbDictionary::Event::TableEvent 
NdbEventOperationImpl::getEventType()
{
  switch (sdata->operation) {
  case TriggerEvent::TE_INSERT:
    return NdbDictionary::Event::TE_INSERT;
  case TriggerEvent::TE_DELETE:
    return NdbDictionary::Event::TE_DELETE;
  case TriggerEvent::TE_UPDATE:
    return NdbDictionary::Event::TE_UPDATE;
  default:
    return NdbDictionary::Event::TE_ALL;
  }
}



void
NdbEventOperationImpl::print()
{
  ndbout << "EventId " << m_eventId << "\n";

  for (int i = 0; i < 2; i++) {
    NdbRecAttr *p = theFirstRecAttrs[i];
    ndbout << " %u " << i;
    while (p) {
      ndbout << " : " << p->attrId() << " = " << *p;
      p = p->next();
    }
    ndbout << "\n";
  }
}

void
NdbEventOperationImpl::printAll()
{
  Uint32 *aAttrPtr = ptr[0].p;
  Uint32 *aAttrEndPtr = aAttrPtr + ptr[0].sz;
  Uint32 *aDataPtr = ptr[1].p;

  //tRecAttr->setup(tAttrInfo, aValue)) {

  Uint32 tAttrId;
  Uint32 tDataSz;
  for (; aAttrPtr < aAttrEndPtr; ) {
    tAttrId = AttributeHeader(*aAttrPtr).getAttributeId();
    tDataSz = AttributeHeader(*aAttrPtr).getDataSize();

    aAttrPtr++;
    aDataPtr += tDataSz;
  }
}


int NdbEventOperationImpl::wait(void *p, int aMillisecondNumber)
{
  return ((NdbGlobalEventBufferHandle*)p)->wait(aMillisecondNumber);
}

/*
 * Global variable ndbGlobalEventBuffer
 * Class NdbGlobalEventBufferHandle
 * Class NdbGlobalEventBuffer
 *
 */

#define ADD_DROP_LOCK_GUARDR(TYPE, FN) \
{ \
  ndbGlobalEventBuffer->add_drop_lock(); \
  ndbGlobalEventBuffer->lock(); \
  TYPE r = ndbGlobalEventBuffer->FN; \
  ndbGlobalEventBuffer->unlock(); \
  if (r < 0) { \
    ndbGlobalEventBuffer->add_drop_unlock(); \
  } \
  return r;\
}
#define GUARDR(TYPE, FN) \
{ \
  ndbGlobalEventBuffer->lock(); \
  TYPE r = ndbGlobalEventBuffer->FN; \
  ndbGlobalEventBuffer->unlock(); \
  return r;\
}
#define GUARD(FN) \
{ \
  ndbGlobalEventBuffer->lock(); \
  ndbGlobalEventBuffer->FN; \
  ndbGlobalEventBuffer->unlock(); \
}
#define ADD_DROP_UNLOCK_GUARD(FN) \
{ \
  GUARD(FN); \
  ndbGlobalEventBuffer->add_drop_unlock(); \
}
#define GUARDBLOCK(BLOCK) \
{ \
  ndbGlobalEventBuffer->lock(); \
  BLOCK \
  ndbGlobalEventBuffer->unlock(); \
}

/*
 * Global variable ndbGlobalEventBuffer
 *
 */

extern NdbMutex * ndb_global_event_buffer_mutex;
static NdbGlobalEventBuffer *ndbGlobalEventBuffer=NULL;

/*
 * Class NdbGlobalEventBufferHandle
 * Each Ndb object has a Handle.  This Handle is used to access the
 * global NdbGlobalEventBuffer instance ndbGlobalEventBuffer
 */

NdbGlobalEventBufferHandle *
NdbGlobalEventBuffer_init(int n) 
{
  return new NdbGlobalEventBufferHandle(n);
  // return NdbGlobalEventBufferHandle::init(n);
}

void
NdbGlobalEventBuffer_drop(NdbGlobalEventBufferHandle *h) 
{
  delete h;
}

NdbGlobalEventBufferHandle::NdbGlobalEventBufferHandle
(int MAX_NUMBER_ACTIVE_EVENTS) : m_bufferL(0), m_nids(0)
{
  if ((p_cond = NdbCondition_Create()) ==  NULL) {
    ndbout_c("NdbGlobalEventBufferHandle: NdbCondition_Create() failed");
    exit(-1);
  }
  
  NdbMutex_Lock(ndb_global_event_buffer_mutex);
  if (ndbGlobalEventBuffer == NULL) {
    if (ndbGlobalEventBuffer == NULL) {
      ndbGlobalEventBuffer = new NdbGlobalEventBuffer();
      if (!ndbGlobalEventBuffer) {
	NdbMutex_Unlock(ndb_global_event_buffer_mutex);
	ndbout_c("NdbGlobalEventBufferHandle:: failed to allocate ndbGlobalEventBuffer");
	exit(-1);
      }
    }
  }
  NdbMutex_Unlock(ndb_global_event_buffer_mutex);

  GUARD(real_init(this,MAX_NUMBER_ACTIVE_EVENTS));
}

NdbGlobalEventBufferHandle::~NdbGlobalEventBufferHandle()
{
  NdbCondition_Destroy(p_cond);

  ndbGlobalEventBuffer->lock();
  ndbGlobalEventBuffer->real_remove(this);
  ndbGlobalEventBuffer->unlock();

  NdbMutex_Lock(ndb_global_event_buffer_mutex);
  if (ndbGlobalEventBuffer->m_handlers.size() == 0) {
    delete ndbGlobalEventBuffer;
    ndbGlobalEventBuffer = NULL;
  }
  NdbMutex_Unlock(ndb_global_event_buffer_mutex);
}

void
NdbGlobalEventBufferHandle::addBufferId(int bufferId)
{
  if (m_nids >= NDB_MAX_ACTIVE_EVENTS) {
    ndbout_c("NdbGlobalEventBufferHandle::addBufferId error in paramerer setting");
    exit(-1);
  }
  m_bufferIds[m_nids] = bufferId;
  m_nids++;
}

void
NdbGlobalEventBufferHandle::dropBufferId(int bufferId)
{
  for (int i = 0; i < m_nids; i++)
    if (m_bufferIds[i] == bufferId) {
      m_nids--;
      for (; i < m_nids; i++)
	m_bufferIds[i] = m_bufferIds[i+1];
      return;
    }
  ndbout_c("NdbGlobalEventBufferHandle::dropBufferId %d does not exist",
	   bufferId);
  exit(-1);
}
/*
NdbGlobalEventBufferHandle *
NdbGlobalEventBufferHandle::init (int MAX_NUMBER_ACTIVE_EVENTS)
{
  return new NdbGlobalEventBufferHandle();
}
void
NdbGlobalEventBufferHandle::drop(NdbGlobalEventBufferHandle *handle)
{
  delete handle;
}
*/
int 
NdbGlobalEventBufferHandle::prepareAddSubscribeEvent(Uint32 eventId,
						     int& hasSubscriber)
{
  ADD_DROP_LOCK_GUARDR(int,real_prepareAddSubscribeEvent(this, eventId, hasSubscriber));
}
void
NdbGlobalEventBufferHandle::addSubscribeEvent
(int bufferId, NdbEventOperationImpl *ndbEventOperationImpl)
{
  ADD_DROP_UNLOCK_GUARD(real_addSubscribeEvent(bufferId, ndbEventOperationImpl));
}
void
NdbGlobalEventBufferHandle::unprepareAddSubscribeEvent(int bufferId)
{
  ADD_DROP_UNLOCK_GUARD(real_unprepareAddSubscribeEvent(bufferId));
}

int 
NdbGlobalEventBufferHandle::prepareDropSubscribeEvent(int bufferId,
						     int& hasSubscriber)
{
  ADD_DROP_LOCK_GUARDR(int,real_prepareDropSubscribeEvent(bufferId, hasSubscriber));
}

void
NdbGlobalEventBufferHandle::unprepareDropSubscribeEvent(int bufferId)
{
  ADD_DROP_UNLOCK_GUARD(real_unprepareDropSubscribeEvent(bufferId));
}

void 
NdbGlobalEventBufferHandle::dropSubscribeEvent(int bufferId)
{
  ADD_DROP_UNLOCK_GUARD(real_dropSubscribeEvent(bufferId));
}

int 
NdbGlobalEventBufferHandle::insertDataL(int bufferId,
					const SubTableData * const sdata,
					LinearSectionPtr ptr[3])
{
  GUARDR(int,real_insertDataL(bufferId,sdata,ptr));
}
 
void
NdbGlobalEventBufferHandle::latestGCI(int bufferId, Uint32 gci)
{
  GUARD(real_latestGCI(bufferId,gci));
}
 
Uint32
NdbGlobalEventBufferHandle::getLatestGCI()
{
  GUARDR(Uint32, real_getLatestGCI());
}
 
inline void
NdbGlobalEventBufferHandle::group_lock()
{
  ndbGlobalEventBuffer->group_lock();
}

inline void
NdbGlobalEventBufferHandle::group_unlock()
{
  ndbGlobalEventBuffer->group_unlock();
}

int
NdbGlobalEventBufferHandle::wait(int aMillisecondNumber)
{
  GUARDR(int, real_wait(this, aMillisecondNumber));
}

int NdbGlobalEventBufferHandle::getDataL(const int bufferId,
					 SubTableData * &sdata,
					 LinearSectionPtr ptr[3],
					 int *pOverrun)
{
  GUARDR(int,real_getDataL(bufferId,sdata,ptr,pOverrun));
}

/*
 * Class NdbGlobalEventBuffer
 *
 *
 */


void
NdbGlobalEventBuffer::lock()
{
  if (!m_group_lock_flag)
    NdbMutex_Lock(ndb_global_event_buffer_mutex);
}
void
NdbGlobalEventBuffer::unlock()
{
  if (!m_group_lock_flag)
    NdbMutex_Unlock(ndb_global_event_buffer_mutex);
}
void
NdbGlobalEventBuffer::add_drop_lock()
{
  NdbMutex_Lock(p_add_drop_mutex);
}
void
NdbGlobalEventBuffer::add_drop_unlock()
{
  NdbMutex_Unlock(p_add_drop_mutex);
}
inline void
NdbGlobalEventBuffer::group_lock()
{
  lock();
  m_group_lock_flag = 1;
}

inline void
NdbGlobalEventBuffer::group_unlock()
{
  m_group_lock_flag = 0;
  unlock();
}

void
NdbGlobalEventBuffer::lockB(int bufferId)
{
  NdbMutex_Lock(m_buf[ID(bufferId)].p_buf_mutex);
}
void
NdbGlobalEventBuffer::unlockB(int bufferId)
{
  NdbMutex_Lock(m_buf[ID(bufferId)].p_buf_mutex);
}

// Private methods

NdbGlobalEventBuffer::NdbGlobalEventBuffer() : 
  m_handlers(),
  m_group_lock_flag(0),
  m_latestGCI(0),
  m_no(0) // must start at ZERO!
{
  if ((p_add_drop_mutex = NdbMutex_Create()) == NULL) {
    ndbout_c("NdbGlobalEventBuffer: NdbMutex_Create() failed");
    exit(-1);
  }
}

NdbGlobalEventBuffer::~NdbGlobalEventBuffer()
{
  NdbMutex_Destroy(p_add_drop_mutex);
  // NdbMem_Deallocate(m_eventBufferIdToEventId);
}
void
NdbGlobalEventBuffer::real_init (NdbGlobalEventBufferHandle *h, 
				 int MAX_NUMBER_ACTIVE_EVENTS)
{
  if (m_handlers.size() == 0) { // First init
    m_max = MAX_NUMBER_ACTIVE_EVENTS;
    m_buf = new BufItem[m_max];
      // (BufItem *)NdbMem_Allocate(m_max*sizeof(BufItem));

    for (int i=0; i<m_max; i++) {
      m_buf[i].gId = 0;
    }
  }
  // TODO make sure we don't hit roof
  //  m_handlers[m_nhandlers] = h;
  m_handlers.push_back(h);
  //  ndbout_c("NdbGlobalEventBuffer::real_init(), m_handles=%u %u", m_nhandlers, h);
}
void
NdbGlobalEventBuffer::real_remove(NdbGlobalEventBufferHandle *h)
{
  //  ndbout_c("NdbGlobalEventBuffer::real_init_remove(), m_handles=%u %u", m_nhandlers, h);
  for (Uint32 i=0 ; i < m_handlers.size(); i++) {
    //    ndbout_c("%u %u %u", i, m_handlers[i], h);
    if (m_handlers[i] == h) {
      m_handlers.erase(i);
      if (m_handlers.size() == 0) {
	//	ndbout_c("last to go");
	delete[] m_buf;
	m_buf = NULL;
	// NdbMem_Free((char*)m_buf);
      }
      return;
    }
  }
  ndbout_c("NdbGlobalEventBuffer::real_init_remove() non-existing handle");
  exit(-1);
}

int 
NdbGlobalEventBuffer::real_prepareAddSubscribeEvent
(NdbGlobalEventBufferHandle *aHandle, Uint32 eventId, int& hasSubscriber)
{
  int i;
  int bufferId = -1;

  //  add_drop_lock(); // only one thread can do add or drop at a time

  // Find place where eventId already set
  for (i=0; i<m_no; i++) {
    if (m_buf[i].gId == eventId) {
      bufferId = i;
      break;
    }
  }
  if (bufferId < 0) {
    // find space for new bufferId
    for (i=0; i<m_no; i++) {
      if (m_buf[i].gId == 0) {
	bufferId = i; // we found an empty spot
	break;
      }
    }
    if (bufferId < 0 &&
	m_no < m_max) {
      // room for more so get that
      bufferId=m_no;
      m_buf[m_no].gId = 0;
      m_no++;
    } else {
      ndbout_c("prepareAddSubscribeEvent: Can't accept more subscribers");
      //      add_drop_unlock();
      return -1;
    }
  }

  BufItem &b = m_buf[ID(bufferId)];

  if (b.gId == 0) { // first subscriber needs some initialization

    bufferId = NO_ID(0, bufferId);

    b.gId = eventId;

    if ((b.p_buf_mutex = NdbMutex_Create()) == NULL) {
      ndbout_c("NdbGlobalEventBuffer: NdbMutex_Create() failed");
      exit(-1);
    }

    b.subs = 0;
    b.f = 0;
    b.sz = 0;
    b.max_sz = aHandle->m_bufferL;
    b.data = 
      (BufItem::Data *)NdbMem_Allocate(b.max_sz*sizeof(BufItem::Data));
    for (int i = 0; i < b.max_sz; i++) {
      b.data[i].sdata = NULL;
      b.data[i].ptr[0].p = NULL;
      b.data[i].ptr[1].p = NULL;
      b.data[i].ptr[2].p = NULL;
    }
  } else {
#ifdef EVENT_DEBUG
    ndbout_c("NdbGlobalEventBuffer::prepareAddSubscribeEvent: TRYING handle one subscriber per event b.subs = %u", b.subs);
#endif

    int ni = -1;
    for(int i=0; i < b.subs;i++) {
      if (b.ps[i].theHandle == NULL) {
	ni = i;
	break;
      }
    }
    if (ni < 0) {
      if (b.subs < MAX_SUBSCRIBERS_PER_EVENT) {
	ni = b.subs;
      } else {
	ndbout_c("prepareAddSubscribeEvent: Can't accept more subscribers");
	//	add_drop_unlock();
	return -1;
      }
    }
    bufferId = NO_ID(ni, bufferId);
  }

  // initialize BufItem::Ps
  {
    int n = NO(bufferId);
    NdbGlobalEventBuffer::BufItem::Ps &e = b.ps[n];
    e.theHandle = aHandle;
    e.b=0;
    e.bufferempty = 1;
    e.overrun=0; // set to -1 to handle first insert
  }

  if (b.subs > 0)
    hasSubscriber = 1;
  else
    hasSubscriber = 0;

#ifdef EVENT_DEBUG
  ndbout_c("prepareAddSubscribeEvent: handed out bufferId %d for eventId %d",
	   bufferId, eventId);
#endif

  /* we now have a lock on the prepare so that no one can mess with this
   * unlock comes in unprepareAddSubscribeEvent or addSubscribeEvent
   */
  return bufferId;
}

void
NdbGlobalEventBuffer::real_unprepareAddSubscribeEvent(int bufferId)
{
  BufItem &b = m_buf[ID(bufferId)];
  int n = NO(bufferId);

  b.ps[n].theHandle = NULL;

  // remove subscribers from the end,
  // we have to keep gaps since the position
  // has been handed out in bufferId
  for (int i = b.subs-1; i >= 0; i--)
    if (b.ps[i].theHandle == NULL)
      b.subs--;
    else
      break;

  if (b.subs == 0) {
#ifdef EVENT_DEBUG
    ndbout_c("unprepareAddSubscribeEvent: no more subscribers left on eventId %d", b.gId);
#endif
    b.gId = 0;  // We don't have any subscribers, reuse BufItem
    if (b.data) {
      NdbMem_Free((void *)b.data);
      b.data = NULL;
    }
    if (b.p_buf_mutex) {
      NdbMutex_Destroy(b.p_buf_mutex);
      b.p_buf_mutex = NULL;
    }
  }
  //  add_drop_unlock();
}

void
NdbGlobalEventBuffer::real_addSubscribeEvent(int bufferId, 
					     void *ndbEventOperation)
{
  BufItem &b = m_buf[ID(bufferId)];
  int n = NO(bufferId);

  b.subs++;
  b.ps[n].theHandle->addBufferId(bufferId);

  //  add_drop_unlock();
#ifdef EVENT_DEBUG
  ndbout_c("addSubscribeEvent:: added bufferId %d", bufferId);
#endif
}

void
NdbGlobalEventBuffer::real_unprepareDropSubscribeEvent(int bufferId)
{
  //  add_drop_unlock(); // only one thread can do add or drop at a time
}

int 
NdbGlobalEventBuffer::real_prepareDropSubscribeEvent(int bufferId,
						     int& hasSubscriber)
{
  //  add_drop_lock(); // only one thread can do add or drop at a time

  BufItem &b = m_buf[ID(bufferId)];

  int n = 0;
  for(int i=0; i < b.subs;i++) {
    if (b.ps[i].theHandle != NULL)
      n++;
  }

  if (n > 1)
    hasSubscriber = 1;
  else if (n == 1)
    hasSubscriber = 0;
  else
    return -1;

  return 0;
}

void
NdbGlobalEventBuffer::real_dropSubscribeEvent(int bufferId)
{
  //  add_drop_lock(); // only one thread can do add-drop at a time

  BufItem &b = m_buf[ID(bufferId)];
  int n = NO(bufferId);

  b.ps[n].overrun=0;
  b.ps[n].bufferempty=1;
  b.ps[n].b=0;
  b.ps[n].theHandle->dropBufferId(bufferId);

  real_unprepareAddSubscribeEvent(bufferId); // does add_drop_unlock();

#ifdef EVENT_DEBUG
  ndbout_c("dropSubscribeEvent:: dropped bufferId %d", bufferId);
#endif
}

void
NdbGlobalEventBuffer::real_latestGCI(int bufferId, Uint32 gci)
{
  if (gci > m_latestGCI)
    m_latestGCI = gci;
  else if ((m_latestGCI-gci) > 0xffff) // If NDB stays up :-)
    m_latestGCI = gci;
}

Uint32
NdbGlobalEventBuffer::real_getLatestGCI()
{
  return m_latestGCI;
}

int
NdbGlobalEventBuffer::real_insertDataL(int bufferId, 
				       const SubTableData * const sdata, 
				       LinearSectionPtr ptr[3])
{
  BufItem &b = m_buf[ID(bufferId)];
#ifdef EVENT_DEBUG
  int n = NO(bufferId);
#endif
  {
    if (b.subs) {
#ifdef EVENT_DEBUG
      ndbout_c("data insertion in buffer %d with eventId %d", bufferId, b.gId);
#endif
      // move front forward
      if (copy_data_alloc(sdata, ptr,
			  b.data[b.f].sdata, b.data[b.f].ptr))
	return -1;
      for (int i=0; i < b.subs; i++) {
	NdbGlobalEventBuffer::BufItem::Ps &e = b.ps[i];
	if (e.theHandle) { // active subscriber
	  if (b.f == e.b) { // next-to-read == written
	    if (e.bufferempty == 0) {
	      e.overrun++; // another item has been overwritten
	      e.b++; // move next-to-read next since old item was overwritten
	      if (e.b == b.max_sz) e.b = 0; // start from beginning
	    }
	  }
	  e.bufferempty = 0;
	  // signal subscriber that there's more to get
	  NdbCondition_Signal(e.theHandle->p_cond);
	}
      }
      b.f++; // move next-to-write
      if (b.f == b.max_sz) b.f = 0; // start from beginning
#ifdef EVENT_DEBUG
      ndbout_c("Front= %d Back = %d overun = %d", b.f,
	       b.ps[n].b, b.ps[n].overrun);
#endif
    } else {
#ifdef EVENT_DEBUG
      ndbout_c("Data arrived before ready eventId", b.gId);
#endif
    }
  }
  return 0;
}

int NdbGlobalEventBuffer::hasData(int bufferId) {
  BufItem &b = m_buf[ID(bufferId)];
  int n = NO(bufferId);
  NdbGlobalEventBuffer::BufItem::Ps &e = b.ps[n];

  if(e.bufferempty)
    return 0;

  if (b.f <= e.b)
    return b.max_sz-e.b + b.f;
  else
    return b.f-e.b;
}

int NdbGlobalEventBuffer::real_getDataL(const int bufferId,
					SubTableData * &sdata,
					LinearSectionPtr ptr[3],
					int *pOverrun)
{
  BufItem &b = m_buf[ID(bufferId)];
  int n = NO(bufferId);
  NdbGlobalEventBuffer::BufItem::Ps &e = b.ps[n];

  if (pOverrun) {
    *pOverrun = e.overrun;
    e.overrun = 0; // if pOverrun is returned to user reset e.overrun
  }

  if (e.bufferempty)
    return 0; // nothing to get

  if (copy_data_alloc(b.data[e.b].sdata, b.data[e.b].ptr,
		      sdata, ptr))
    return -1;

  e.b++; if (e.b == b.max_sz) e.b = 0; // move next-to-read forward

  if (b.f == e.b) // back has cought up with front
    e.bufferempty = 1;

#ifdef EVENT_DEBUG
  ndbout_c("getting data from buffer %d with eventId %d", bufferId, b.gId);
#endif

  return hasData(bufferId)+1;
}
int 
NdbGlobalEventBuffer::copy_data_alloc(const SubTableData * const f_sdata,
				      LinearSectionPtr f_ptr[3],
				      SubTableData * &t_sdata,
				      LinearSectionPtr t_ptr[3])
{
  if (t_sdata == NULL) {
    t_sdata = (SubTableData *)NdbMem_Allocate(sizeof(SubTableData));
  }
  memcpy(t_sdata,f_sdata,sizeof(SubTableData));
  for (int i = 0; i < 3; i++) {
    LinearSectionPtr & f_p = f_ptr[i];
    LinearSectionPtr & t_p = t_ptr[i];
    if (f_p.sz > 0) {
      if (t_p.p == NULL) {
	t_p.p = (Uint32 *)NdbMem_Allocate(sizeof(Uint32)*f_p.sz);
      } else if (t_p.sz != f_p.sz) {
	NdbMem_Free(t_p.p);
	t_p.p = (Uint32 *)NdbMem_Allocate(sizeof(Uint32)*f_p.sz);
      }
      memcpy(t_p.p, f_p.p, sizeof(Uint32)*f_p.sz);
    } else if (t_p.p != NULL) {
      NdbMem_Free(t_p.p);
      t_p.p = NULL;
    }
    t_p.sz = f_p.sz;
  }
  return 0;
}
int
NdbGlobalEventBuffer::real_wait(NdbGlobalEventBufferHandle *h,
				int aMillisecondNumber)
{
  // check if there are anything in any of the buffers
  int i;
  int n = 0;
  for (i = 0; i < h->m_nids; i++)
    n += hasData(h->m_bufferIds[i]);
  if (n) return n;

  int r = NdbCondition_WaitTimeout(h->p_cond, ndb_global_event_buffer_mutex,
				   aMillisecondNumber);
  if (r > 0)
    return -1;

  n = 0;
  for (i = 0; i < h->m_nids; i++)
    n += hasData(h->m_bufferIds[i]);
  return n;
}

template class Vector<NdbGlobalEventBufferHandle*>;
