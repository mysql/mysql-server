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

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

static Gci_container g_empty_gci_container;
static const Uint32 ACTIVE_GCI_DIRECTORY_SIZE = 4;
static const Uint32 ACTIVE_GCI_MASK = ACTIVE_GCI_DIRECTORY_SIZE - 1;

/*
 * Class NdbEventOperationImpl
 *
 *
 */

//#define EVENT_DEBUG

// todo handle several ndb objects
// todo free allocated data when closing NdbEventBuffer

NdbEventOperationImpl::NdbEventOperationImpl(NdbEventOperation &N,
					     Ndb *theNdb, 
					     const char* eventName) 
  : NdbEventOperation(*this), m_facade(&N), m_magic_number(0),
    m_ndb(theNdb), m_state(EO_ERROR), mi_type(0), m_oid(~(Uint32)0),
#ifdef VM_TRACE
    m_data_done_count(0), m_data_count(0),
#endif
    m_next(0), m_prev(0)
{
  DBUG_ENTER("NdbEventOperationImpl::NdbEventOperationImpl");
  m_eventId = 0;
  theFirstPkAttrs[0] = NULL;
  theCurrentPkAttrs[0] = NULL;
  theFirstPkAttrs[1] = NULL;
  theCurrentPkAttrs[1] = NULL;
  theFirstDataAttrs[0] = NULL;
  theCurrentDataAttrs[0] = NULL;
  theFirstDataAttrs[1] = NULL;
  theCurrentDataAttrs[1] = NULL;
  m_data_item= NULL;
  m_eventImpl = NULL;

  m_custom_data= 0;
  m_has_error= 1;

  // we should lookup id in Dictionary, TODO
  // also make sure we only have one listener on each event

  if (!m_ndb) abort();

  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) { m_error.code= m_ndb->getNdbError().code; DBUG_VOID_RETURN; }

  const NdbDictionary::Event *myEvnt = myDict->getEvent(eventName);
  if (!myEvnt) { m_error.code= myDict->getNdbError().code; DBUG_VOID_RETURN; }

  m_eventImpl = &myEvnt->m_impl;

  m_eventId = m_eventImpl->m_eventId;

  m_oid= m_ndb->theImpl->theNdbObjectIdMap.map(this);

  m_state= EO_CREATED;

  m_separateEvents = false;

  m_has_error= 0;

  DBUG_PRINT("exit",("this: 0x%x oid: %u", this, m_oid));
  DBUG_VOID_RETURN;
}

NdbEventOperationImpl::~NdbEventOperationImpl()
{
  DBUG_ENTER("NdbEventOperationImpl::~NdbEventOperationImpl");
  m_magic_number= 0;

  stop();
  // m_bufferHandle->dropSubscribeEvent(m_bufferId);
  ; // ToDo? We should send stop signal here
  
  m_ndb->theImpl->theNdbObjectIdMap.unmap(m_oid, this);
  DBUG_PRINT("exit",("this: 0x%x/0x%x oid: %u", this, m_facade, m_oid));

  if (m_eventImpl)
  {
    delete m_eventImpl->m_facade;
    m_eventImpl= 0;
  }

  DBUG_VOID_RETURN;
}

NdbEventOperation::State
NdbEventOperationImpl::getState()
{
  return m_state;
}

NdbRecAttr*
NdbEventOperationImpl::getValue(const char *colName, char *aValue, int n)
{
  DBUG_ENTER("NdbEventOperationImpl::getValue");
  if (m_state != EO_CREATED) {
    ndbout_c("NdbEventOperationImpl::getValue may only be called between "
	     "instantiation and execute()");
    DBUG_RETURN(NULL);
  }

  NdbColumnImpl *tAttrInfo = m_eventImpl->m_tableImpl->getColumn(colName);

  if (tAttrInfo == NULL) {
    ndbout_c("NdbEventOperationImpl::getValue attribute %s not found",colName);
    DBUG_RETURN(NULL);
  }

  DBUG_RETURN(NdbEventOperationImpl::getValue(tAttrInfo, aValue, n));
}

NdbRecAttr*
NdbEventOperationImpl::getValue(const NdbColumnImpl *tAttrInfo, char *aValue, int n)
{
  DBUG_ENTER("NdbEventOperationImpl::getValue");
  // Insert Attribute Id into ATTRINFO part. 

  NdbRecAttr **theFirstAttr;
  NdbRecAttr **theCurrentAttr;

  if (tAttrInfo->getPrimaryKey())
  {
    theFirstAttr = &theFirstPkAttrs[n];
    theCurrentAttr = &theCurrentPkAttrs[n];
  }
  else
  {
    theFirstAttr = &theFirstDataAttrs[n];
    theCurrentAttr = &theCurrentDataAttrs[n];
  }

  /************************************************************************
   *	Get a Receive Attribute object and link it into the operation object.
   ************************************************************************/
  NdbRecAttr *tAttr = m_ndb->getRecAttr();
  if (tAttr == NULL) { 
    exit(-1);
    //setErrorCodeAbort(4000);
    DBUG_RETURN(NULL);
  }

  /**********************************************************************
   * Now set the attribute identity and the pointer to the data in 
   * the RecAttr object
   * Also set attribute size, array size and attribute type
   ********************************************************************/
  if (tAttr->setup(tAttrInfo, aValue)) {
    //setErrorCodeAbort(4000);
    m_ndb->releaseRecAttr(tAttr);
    exit(-1);
    DBUG_RETURN(NULL);
  }
  //theErrorLine++;

  tAttr->setUNDEFINED();
  
  // We want to keep the list sorted to make data insertion easier later

  if (*theFirstAttr == NULL) {
    *theFirstAttr = tAttr;
    *theCurrentAttr = tAttr;
    tAttr->next(NULL);
  } else {
    Uint32 tAttrId = tAttrInfo->m_attrId;
    if (tAttrId > (*theCurrentAttr)->attrId()) { // right order
      (*theCurrentAttr)->next(tAttr);
      tAttr->next(NULL);
      *theCurrentAttr = tAttr;
    } else if ((*theFirstAttr)->next() == NULL ||    // only one in list
	       (*theFirstAttr)->attrId() > tAttrId) {// or first 
      tAttr->next(*theFirstAttr);
      *theFirstAttr = tAttr;
    } else { // at least 2 in list and not first and not last
      NdbRecAttr *p = *theFirstAttr;
      NdbRecAttr *p_next = p->next();
      while (tAttrId > p_next->attrId()) {
	p = p_next;
	p_next = p->next();
      }
      if (tAttrId == p_next->attrId()) { // Using same attribute twice
	tAttr->release(); // do I need to do this?
	m_ndb->releaseRecAttr(tAttr);
	exit(-1);
	DBUG_RETURN(NULL);
      }
      // this is it, between p and p_next
      p->next(tAttr);
      tAttr->next(p_next);
    }
  }
  DBUG_RETURN(tAttr);
}

int
NdbEventOperationImpl::execute()
{
  DBUG_ENTER("NdbEventOperationImpl::execute");
  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) {
    m_error.code= m_ndb->getNdbError().code;
    DBUG_RETURN(-1);
  }

  if (theFirstPkAttrs[0] == NULL && 
      theFirstDataAttrs[0] == NULL) { // defaults to get all
    
  }

  m_ndb->theEventBuffer->add_drop_lock();
  m_magic_number= NDB_EVENT_OP_MAGIC_NUMBER;
  m_state= EO_EXECUTING;
  mi_type= m_eventImpl->mi_type;
  m_ndb->theEventBuffer->add_op();
  int r= NdbDictionaryImpl::getImpl(*myDict).executeSubscribeEvent(*this);
  if (r == 0) {
    m_ndb->theEventBuffer->add_drop_unlock();
    DBUG_RETURN(0);
  }
  //Error
  m_state= EO_ERROR;
  mi_type= 0;
  m_magic_number= 0;
  m_error.code= myDict->getNdbError().code;
  m_ndb->theEventBuffer->remove_op();
  m_ndb->theEventBuffer->add_drop_unlock();
  DBUG_RETURN(r);
}

int
NdbEventOperationImpl::stop()
{
  DBUG_ENTER("NdbEventOperationImpl::stop");
  int i;

  for (i=0 ; i<2; i++) {
    NdbRecAttr *p = theFirstPkAttrs[i];
    while (p) {
      NdbRecAttr *p_next = p->next();
      m_ndb->releaseRecAttr(p);
      p = p_next;
    }
    theFirstPkAttrs[i]= 0;
  }
  for (i=0 ; i<2; i++) {
    NdbRecAttr *p = theFirstDataAttrs[i];
    while (p) {
      NdbRecAttr *p_next = p->next();
      m_ndb->releaseRecAttr(p);
      p = p_next;
    }
    theFirstDataAttrs[i]= 0;
  }

  if (m_state != EO_EXECUTING)
  {
    DBUG_RETURN(-1);
  }

  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) {
    m_error.code= m_ndb->getNdbError().code;
    DBUG_RETURN(-1);
  }

  m_ndb->theEventBuffer->add_drop_lock();
  int r= NdbDictionaryImpl::getImpl(*myDict).stopSubscribeEvent(*this);
  m_ndb->theEventBuffer->remove_op();
  m_state= EO_DROPPED;
  mi_type= 0;
  if (r == 0) {
    m_ndb->theEventBuffer->add_drop_unlock();
    DBUG_RETURN(0);
  }
  //Error
  m_error.code= NdbDictionaryImpl::getImpl(*myDict).m_error.code;
  m_state= EO_ERROR;
  m_ndb->theEventBuffer->add_drop_unlock();
  DBUG_RETURN(r);
}

Uint64
NdbEventOperationImpl::getGCI()
{
  return m_data_item->sdata->gci;
}

Uint64
NdbEventOperationImpl::getLatestGCI()
{
  return m_ndb->theEventBuffer->getLatestGCI();
}

int
NdbEventOperationImpl::receive_event()
{
  DBUG_ENTER("NdbEventOperationImpl::receive_event");

  Uint32 operation= (Uint32)m_data_item->sdata->operation;
  DBUG_PRINT("info",("sdata->operation %u",operation));

  if (unlikely(operation >= NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT))
  {
    DBUG_RETURN(1);
  }

  // now move the data into the RecAttrs
    
  int is_update= operation == NdbDictionary::Event::_TE_UPDATE;

  Uint32 *aAttrPtr = m_data_item->ptr[0].p;
  Uint32 *aAttrEndPtr = aAttrPtr + m_data_item->ptr[0].sz;
  Uint32 *aDataPtr = m_data_item->ptr[1].p;

  DBUG_DUMP("after",(char*)m_data_item->ptr[1].p, m_data_item->ptr[1].sz*4);
  DBUG_DUMP("before",(char*)m_data_item->ptr[2].p, m_data_item->ptr[2].sz*4);

  // copy data into the RecAttr's
  // we assume that the respective attribute lists are sorted

  // first the pk's
  {
    NdbRecAttr *tAttr= theFirstPkAttrs[0];
    NdbRecAttr *tAttr1= theFirstPkAttrs[1];
    while(tAttr)
    {
      assert(aAttrPtr < aAttrEndPtr);
      unsigned tDataSz= AttributeHeader(*aAttrPtr).getByteSize();
      assert(tAttr->attrId() ==
	     AttributeHeader(*aAttrPtr).getAttributeId());
      receive_data(tAttr, aDataPtr, tDataSz);
      if (is_update)
	receive_data(tAttr1, aDataPtr, tDataSz);
      else
        tAttr1->setUNDEFINED(); // do not leave unspecified
      tAttr1= tAttr1->next();
      // next
      aAttrPtr++;
      aDataPtr+= (tDataSz + 3) >> 2;
      tAttr= tAttr->next();
    }
  }
  
  NdbRecAttr *tWorkingRecAttr = theFirstDataAttrs[0];
  
  Uint32 tRecAttrId;
  Uint32 tAttrId;
  Uint32 tDataSz;
  int hasSomeData=0;
  while ((aAttrPtr < aAttrEndPtr) && (tWorkingRecAttr != NULL)) {
    tRecAttrId = tWorkingRecAttr->attrId();
    tAttrId = AttributeHeader(*aAttrPtr).getAttributeId();
    tDataSz = AttributeHeader(*aAttrPtr).getByteSize();
    
    while (tAttrId > tRecAttrId) {
      DBUG_PRINT("info",("undef [%u] %u 0x%x [%u] 0x%x",
			 tAttrId, tDataSz, *aDataPtr, tRecAttrId, aDataPtr));
      tWorkingRecAttr->setUNDEFINED();
      tWorkingRecAttr = tWorkingRecAttr->next();
      if (tWorkingRecAttr == NULL)
	break;
      tRecAttrId = tWorkingRecAttr->attrId();
    }
    if (tWorkingRecAttr == NULL)
      break;
    
    if (tAttrId == tRecAttrId) {
      hasSomeData++;
      
      DBUG_PRINT("info",("set [%u] %u 0x%x [%u] 0x%x",
			 tAttrId, tDataSz, *aDataPtr, tRecAttrId, aDataPtr));
      
      receive_data(tWorkingRecAttr, aDataPtr, tDataSz);
      tWorkingRecAttr = tWorkingRecAttr->next();
    }
    aAttrPtr++;
    aDataPtr += (tDataSz + 3) >> 2;
  }
    
  while (tWorkingRecAttr != NULL) {
    tRecAttrId = tWorkingRecAttr->attrId();
    //printf("set undefined [%u] %u %u [%u]\n",
    //       tAttrId, tDataSz, *aDataPtr, tRecAttrId);
    tWorkingRecAttr->setUNDEFINED();
    tWorkingRecAttr = tWorkingRecAttr->next();
  }
  
  tWorkingRecAttr = theFirstDataAttrs[1];
  aDataPtr = m_data_item->ptr[2].p;
  Uint32 *aDataEndPtr = aDataPtr + m_data_item->ptr[2].sz;
  while ((aDataPtr < aDataEndPtr) && (tWorkingRecAttr != NULL)) {
    tRecAttrId = tWorkingRecAttr->attrId();
    tAttrId = AttributeHeader(*aDataPtr).getAttributeId();
    tDataSz = AttributeHeader(*aDataPtr).getByteSize();
    aDataPtr++;
    while (tAttrId > tRecAttrId) {
      tWorkingRecAttr->setUNDEFINED();
      tWorkingRecAttr = tWorkingRecAttr->next();
      if (tWorkingRecAttr == NULL)
	break;
      tRecAttrId = tWorkingRecAttr->attrId();
    }
    if (tWorkingRecAttr == NULL)
      break;
    if (tAttrId == tRecAttrId) {
      assert(!m_eventImpl->m_tableImpl->getColumn(tRecAttrId)->getPrimaryKey());
      hasSomeData++;
      
      receive_data(tWorkingRecAttr, aDataPtr, tDataSz);
      tWorkingRecAttr = tWorkingRecAttr->next();
    }
    aDataPtr += (tDataSz + 3) >> 2;
  }
  while (tWorkingRecAttr != NULL) {
    tWorkingRecAttr->setUNDEFINED();
    tWorkingRecAttr = tWorkingRecAttr->next();
  }
  
  if (hasSomeData || !is_update)
  {
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

NdbDictionary::Event::TableEvent 
NdbEventOperationImpl::getEventType()
{
  return (NdbDictionary::Event::TableEvent)
    (1 << (unsigned)m_data_item->sdata->operation);
}



void
NdbEventOperationImpl::print()
{
  int i;
  ndbout << "EventId " << m_eventId << "\n";

  for (i = 0; i < 2; i++) {
    NdbRecAttr *p = theFirstPkAttrs[i];
    ndbout << " %u " << i;
    while (p) {
      ndbout << " : " << p->attrId() << " = " << *p;
      p = p->next();
    }
    ndbout << "\n";
  }
  for (i = 0; i < 2; i++) {
    NdbRecAttr *p = theFirstDataAttrs[i];
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
  Uint32 *aAttrPtr = m_data_item->ptr[0].p;
  Uint32 *aAttrEndPtr = aAttrPtr + m_data_item->ptr[0].sz;
  Uint32 *aDataPtr = m_data_item->ptr[1].p;

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

/*
 * Class NdbEventBuffer
 * Each Ndb object has a Object.
 */

// ToDo ref count this so it get's destroyed
NdbMutex *NdbEventBuffer::p_add_drop_mutex= 0;

NdbEventBuffer::NdbEventBuffer(Ndb *ndb) :
  m_system_nodes(ndb->theImpl->theNoOfDBnodes),
  m_ndb(ndb),
  m_latestGCI(0),
  m_total_alloc(0),
  m_free_thresh(10),
  m_min_free_thresh(10),
  m_max_free_thresh(100),
  m_gci_slip_thresh(3),
  m_dropped_ev_op(0),
  m_active_op_count(0)
{
#ifdef VM_TRACE
  m_latest_command= "NdbEventBuffer::NdbEventBuffer";
#endif

  if ((p_cond = NdbCondition_Create()) ==  NULL) {
    ndbout_c("NdbEventHandle: NdbCondition_Create() failed");
    exit(-1);
  }
  m_mutex= ndb->theImpl->theWaiter.m_mutex;
  lock();
  if (p_add_drop_mutex == 0)
  {
    if ((p_add_drop_mutex = NdbMutex_Create()) == NULL) {
      ndbout_c("NdbEventBuffer: NdbMutex_Create() failed");
      exit(-1);
    }
  }
  unlock();

  // ToDo set event buffer size
  // pre allocate event data array
  m_sz= 0;
#ifdef VM_TRACE
  m_free_data_count= 0;
#endif
  m_free_data= 0;
  m_free_data_sz= 0;

  // initialize lists
  bzero(&g_empty_gci_container, sizeof(Gci_container));
  init_gci_containers();
}

NdbEventBuffer::~NdbEventBuffer()
{
  // todo lock?  what if receive thread writes here?
  for (unsigned j= 0; j < m_allocated_data.size(); j++)
  {
    unsigned sz= m_allocated_data[j]->sz;
    EventBufData *data= m_allocated_data[j]->data;
    EventBufData *end_data= data+sz;
    for (; data < end_data; data++)
    {
      if (data->sdata)
	NdbMem_Free(data->sdata);
    }
    NdbMem_Free((char*)m_allocated_data[j]);
  }

  NdbCondition_Destroy(p_cond);

  lock();
  if (p_add_drop_mutex)
  {
    NdbMutex_Destroy(p_add_drop_mutex);
    p_add_drop_mutex = 0;
  }
  unlock();
}

void
NdbEventBuffer::add_op()
{
  if(m_active_op_count == 0)
  {
    init_gci_containers();
  }
  m_active_op_count++;
}

void
NdbEventBuffer::remove_op()
{
  m_active_op_count--;
}

void
NdbEventBuffer::init_gci_containers()
{
  bzero(&m_complete_data, sizeof(m_complete_data));
  m_latest_complete_GCI = m_latestGCI = 0;
  m_active_gci.clear();
  m_active_gci.fill(2 * ACTIVE_GCI_DIRECTORY_SIZE - 1, g_empty_gci_container);
}

int NdbEventBuffer::expand(unsigned sz)
{
  unsigned alloc_size=
    sizeof(EventBufData_chunk) +(sz-1)*sizeof(EventBufData);
  EventBufData_chunk *chunk_data=
    (EventBufData_chunk *)NdbMem_Allocate(alloc_size);

  chunk_data->sz= sz;
  m_allocated_data.push_back(chunk_data);

  EventBufData *data= chunk_data->data;
  EventBufData *end_data= data+sz;
  EventBufData *last_data= m_free_data;

  bzero((void*)data, sz*sizeof(EventBufData));
  for (; data < end_data; data++)
  {
    data->m_next= last_data;
    last_data= data;
  }
  m_free_data= last_data;

  m_sz+= sz;
#ifdef VM_TRACE
  m_free_data_count+= sz;
#endif
  return 0;
}

int
NdbEventBuffer::pollEvents(int aMillisecondNumber, Uint64 *latestGCI)
{
  int ret= 1;
#ifdef VM_TRACE
  const char *m_latest_command_save= m_latest_command;
  m_latest_command= "NdbEventBuffer::pollEvents";
#endif

  NdbMutex_Lock(m_mutex);
  NdbEventOperationImpl *ev_op= move_data();
  if (unlikely(ev_op == 0 && aMillisecondNumber))
  {
    NdbCondition_WaitTimeout(p_cond, m_mutex, aMillisecondNumber);
    ev_op= move_data();
    if (unlikely(ev_op == 0))
      ret= 0;
  }
  if (latestGCI)
    *latestGCI= m_latestGCI;
#ifdef VM_TRACE
  if (ev_op)
  {
    // m_mutex is locked
    // update event ops data counters
    ev_op->m_data_count-= ev_op->m_data_done_count;
    ev_op->m_data_done_count= 0;
  }
  m_latest_command= m_latest_command_save;
#endif
  NdbMutex_Unlock(m_mutex); // we have moved the data
  return ret;
}

#ifdef VM_TRACE
static void
print_std(const char* tag, const SubTableData * sdata, LinearSectionPtr ptr[3])
{
  printf("%s\n", tag);
  printf("addr=%p gci=%d op=%d\n", (void*)sdata, sdata->gci, sdata->operation);
  for (int i = 0; i <= 2; i++) {
    printf("sec=%d addr=%p sz=%d\n", i, (void*)ptr[i].p, ptr[i].sz);
    for (int j = 0; j < ptr[i].sz; j++)
      printf("%08x ", ptr[i].p[j]);
    printf("\n");
  }
}
#endif

NdbEventOperation *
NdbEventBuffer::nextEvent()
{
  DBUG_ENTER("NdbEventBuffer::nextEvent");
#ifdef VM_TRACE
  const char *m_latest_command_save= m_latest_command;
#endif

  if (m_used_data.m_count > 1024)
  {
#ifdef VM_TRACE
    m_latest_command= "NdbEventBuffer::nextEvent (lock)";
#endif
    NdbMutex_Lock(m_mutex);
    // return m_used_data to m_free_data
    free_list(m_used_data);

    NdbMutex_Unlock(m_mutex);
  }
#ifdef VM_TRACE
  m_latest_command= "NdbEventBuffer::nextEvent";
#endif

  EventBufData *data;
  while ((data= m_available_data.m_head))
  {
    NdbEventOperationImpl *op= data->m_event_op;

    // set NdbEventOperation data
    op->m_data_item= data;

    // remove item from m_available_data
    m_available_data.remove_first();

    // add it to used list
    m_used_data.append(data);

#ifdef VM_TRACE
    op->m_data_done_count++;
#endif

    // NUL event is not returned
    if (data->sdata->operation == NdbDictionary::Event::_TE_NUL)
      continue;

    int r= op->receive_event();
    if (r > 0)
    {
      if (op->m_state == NdbEventOperation::EO_EXECUTING)
      {
#ifdef VM_TRACE
	m_latest_command= m_latest_command_save;
#endif
	DBUG_RETURN(op->m_facade);
      }
      // the next event belonged to an event op that is no
      // longer valid, skip to next
      continue;
    }
#ifdef VM_TRACE
    m_latest_command= m_latest_command_save;
#endif
  }
  m_error.code= 0;
#ifdef VM_TRACE
  m_latest_command= m_latest_command_save;
#endif
  DBUG_RETURN(0);
}

void
NdbEventBuffer::lock()
{
  NdbMutex_Lock(m_mutex);
}
void
NdbEventBuffer::unlock()
{
  NdbMutex_Unlock(m_mutex);
}
void
NdbEventBuffer::add_drop_lock()
{
  NdbMutex_Lock(p_add_drop_mutex);
}
void
NdbEventBuffer::add_drop_unlock()
{
  NdbMutex_Unlock(p_add_drop_mutex);
}

static
NdbOut&
operator<<(NdbOut& out, const Gci_container& gci)
{
  out << "[ GCI: " << gci.m_gci
      << "  state: " << hex << gci.m_state 
      << "  head: " << hex << gci.m_data.m_head
      << "  tail: " << hex << gci.m_data.m_tail
#ifdef VM_TRACE
      << "  cnt: " << dec << gci.m_data.m_count
#endif
      << " gcp: " << dec << gci.m_gcp_complete_rep_count 
      << "]";
  return out;
}

static
Gci_container*
find_bucket_chained(Vector<Gci_container> * active, Uint64 gci)
{
  Uint32 pos = (gci & ACTIVE_GCI_MASK);
  Gci_container *bucket= active->getBase() + pos;

  if(gci > bucket->m_gci)
  {
    Gci_container* move;
    Uint32 move_pos = pos + ACTIVE_GCI_DIRECTORY_SIZE;
    do 
    {
      active->fill(move_pos, g_empty_gci_container);
      bucket = active->getBase() + pos; // Needs to recomputed after fill
      move = active->getBase() + move_pos;
      if(move->m_gcp_complete_rep_count == 0)
      {
	memcpy(move, bucket, sizeof(Gci_container));
	bzero(bucket, sizeof(Gci_container));
	bucket->m_gci = gci;
	bucket->m_gcp_complete_rep_count = ~(Uint32)0;
	return bucket;
      }
      move_pos += ACTIVE_GCI_DIRECTORY_SIZE;
    } while(true);
  }
  else /** gci < bucket->m_gci */
  {
    Uint32 size = active->size() - ACTIVE_GCI_DIRECTORY_SIZE;
    do 
    {
      pos += ACTIVE_GCI_DIRECTORY_SIZE;
      bucket += ACTIVE_GCI_DIRECTORY_SIZE;
      
      if(bucket->m_gci == gci)
	return bucket;
      
    } while(pos < size);
    
    return 0;
  }
}

inline
Gci_container*
find_bucket(Vector<Gci_container> * active, Uint64 gci)
{
  Uint32 pos = (gci & ACTIVE_GCI_MASK);
  Gci_container *bucket= active->getBase() + pos;
  if(likely(gci == bucket->m_gci))
    return bucket;

  return find_bucket_chained(active,gci);
}

void
NdbEventBuffer::execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep * const rep)
{
  if (unlikely(m_active_op_count == 0))
  {
    return;
  }
  
  DBUG_ENTER("NdbEventBuffer::execSUB_GCP_COMPLETE_REP");

  const Uint64 gci= rep->gci;
  const Uint32 cnt= rep->gcp_complete_rep_count;

  Gci_container *bucket = find_bucket(&m_active_gci, gci);

  if (unlikely(bucket == 0))
  {
    /**
     * Already completed GCI...
     *   Possible in case of resend during NF handling
     */
    ndbout << "bucket == 0, gci:" << gci
	   << " complete: " << m_complete_data << endl;
    for(Uint32 i = 0; i<m_active_gci.size(); i++)
    {
      ndbout << i << " - " << m_active_gci[i] << endl;
    }
    DBUG_VOID_RETURN;
  }

  Uint32 old_cnt = bucket->m_gcp_complete_rep_count;
  if(unlikely(old_cnt == ~(Uint32)0))
  {
    old_cnt = m_system_nodes;
  }
  
  assert(old_cnt >= cnt);
  bucket->m_gcp_complete_rep_count = old_cnt - cnt;

  if(old_cnt == cnt)
  {
    if(likely(gci == m_latestGCI + 1 || m_latestGCI == 0))
    {
      m_latestGCI = m_complete_data.m_gci = gci; // before reportStatus
      if(!bucket->m_data.is_empty())
      {
#ifdef VM_TRACE
	assert(bucket->m_data.m_count);
#endif
	m_complete_data.m_data.append(bucket->m_data);
      }
      reportStatus();
      bzero(bucket, sizeof(Gci_container));
      bucket->m_gci = gci + ACTIVE_GCI_DIRECTORY_SIZE;
      bucket->m_gcp_complete_rep_count = m_system_nodes;
      if(unlikely(m_latest_complete_GCI > gci))
      {
	complete_outof_order_gcis();
      }

      // signal that somethings happened

      NdbCondition_Signal(p_cond);
    }
    else
    {
      /** out of order something */
      ndbout_c("out of order bucket: %d gci: %lld m_latestGCI: %lld", 
	       bucket-m_active_gci.getBase(), gci, m_latestGCI);
      bucket->m_state = Gci_container::GC_COMPLETE;
      bucket->m_gcp_complete_rep_count = 1; // Prevent from being reused
      m_latest_complete_GCI = gci;
    }
  }
  
  DBUG_VOID_RETURN;
}

void
NdbEventBuffer::complete_outof_order_gcis()
{
  Uint64 start_gci = m_latestGCI + 1;
  Uint64 stop_gci = m_latest_complete_GCI;
  
  const Uint32 size = m_active_gci.size();
  Gci_container* array= m_active_gci.getBase();
  
  ndbout_c("complete_outof_order_gcis");
  for(Uint32 i = 0; i<size; i++)
  {
    ndbout << i << " - " << array[i] << endl;
  }
  
  for(; start_gci <= stop_gci; start_gci++)
  {
    /**
     * Find gci
     */
    Uint32 i;
    Gci_container* bucket= 0;
    for(i = 0; i<size; i++)
    {
      Gci_container* tmp = array + i;
      if(tmp->m_gci == start_gci && tmp->m_state == Gci_container::GC_COMPLETE)
      {
	bucket= tmp;
	break;
      }
    }
    if(bucket == 0)
    {
      break;
    }

    printf("complete_outof_order_gcis - completing %lld", start_gci);
    if(!bucket->m_data.is_empty())
    {
#ifdef VM_TRACE
      assert(bucket->m_data.m_count);
#endif
      m_complete_data.m_data.append(bucket->m_data);
#ifdef VM_TRACE
      ndbout_c(" moved %lld rows -> %lld", bucket->m_data.m_count,
	       m_complete_data.m_data.m_count);
#else
      ndbout_c("");
#endif
    }
    bzero(bucket, sizeof(Gci_container));
    if(i < ACTIVE_GCI_DIRECTORY_SIZE)
    {
      bucket->m_gci = start_gci + ACTIVE_GCI_DIRECTORY_SIZE;
      bucket->m_gcp_complete_rep_count = m_system_nodes;
    }
    
    m_latestGCI = m_complete_data.m_gci = start_gci;
  }
  
  ndbout_c("complete_outof_order_gcis: m_latestGCI: %lld", m_latestGCI);
}

void
NdbEventBuffer::report_node_failure(Uint32 node_id)
{
  DBUG_ENTER("NdbEventBuffer::report_node_failure");
  SubTableData data;
  LinearSectionPtr ptr[3];
  bzero(&data, sizeof(data));
  bzero(ptr, sizeof(ptr));

  data.tableId = ~0;
  data.operation = NdbDictionary::Event::_TE_NODE_FAILURE;
  data.req_nodeid = (Uint8)node_id;
  data.ndbd_nodeid = (Uint8)node_id;
  data.logType = SubTableData::LOG;
  /**
   * Insert this event for each operation
   */
  NdbEventOperation* op= 0;
  while((op = m_ndb->getEventOperation(op)))
  {
    NdbEventOperationImpl* impl= &op->m_impl;
    data.senderData = impl->m_oid;
    insertDataL(impl, &data, ptr); 
  }
  DBUG_VOID_RETURN;
}

void
NdbEventBuffer::completeClusterFailed()
{
  DBUG_ENTER("NdbEventBuffer::completeClusterFailed");

  SubTableData data;
  LinearSectionPtr ptr[3];
  bzero(&data, sizeof(data));
  bzero(ptr, sizeof(ptr));

  data.tableId = ~0;
  data.operation = NdbDictionary::Event::_TE_CLUSTER_FAILURE;
  data.logType = SubTableData::LOG;

  /**
   * Find min not completed GCI
   */
  Uint32 sz= m_active_gci.size();
  Uint64 gci= ~0;
  Gci_container* bucket = 0;
  Gci_container* array = m_active_gci.getBase();
  for(Uint32 i = 0; i<sz; i++)
  {
    if(array[i].m_gcp_complete_rep_count && array[i].m_gci < gci)
    {
      bucket= array + i;
      gci = bucket->m_gci;
    }
  }

  if(bucket == 0)
  {
    /**
     * Did not find any not completed GCI's
     *   lets fake one...
     */
    gci = m_latestGCI + 1;
    bucket = array + ( gci & ACTIVE_GCI_MASK );
    bucket->m_gcp_complete_rep_count = 1;
  }
  
  const Uint32 cnt= bucket->m_gcp_complete_rep_count = 1; 

  /**
   * Release all GCI's
   */
  for(Uint32 i = 0; i<sz; i++)
  {
    Gci_container* tmp = array + i;
    if(!tmp->m_data.is_empty())
    {
      free_list(tmp->m_data);
#if 0
      m_free_data_count++;
      EventBufData* loop= tmp->m_head;
      while(loop != tmp->m_tail)
      {
	m_free_data_count++;
	loop = loop->m_next;
      }
#endif
    }
    bzero(tmp, sizeof(Gci_container));
  }
  
  bucket->m_gci = gci;
  bucket->m_gcp_complete_rep_count = cnt;
  
  data.gci = gci;
  
  /**
   * Insert this event for each operation
   */
  NdbEventOperation* op= 0;
  while((op = m_ndb->getEventOperation(op)))
  {
    NdbEventOperationImpl* impl= &op->m_impl;
    data.senderData = impl->m_oid;
    insertDataL(impl, &data, ptr); 
  }
  
  /**
   * And finally complete this GCI
   */
  SubGcpCompleteRep rep;
  rep.gci= gci;
  rep.gcp_complete_rep_count= cnt;
  execSUB_GCP_COMPLETE_REP(&rep);

  DBUG_VOID_RETURN;
}

Uint64
NdbEventBuffer::getLatestGCI()
{
  return m_latestGCI;
}

int
NdbEventBuffer::insertDataL(NdbEventOperationImpl *op,
			    const SubTableData * const sdata, 
			    LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbEventBuffer::insertDataL");

  Uint64 gci= sdata->gci;

  if ( likely((Uint32)op->mi_type & 1 << (Uint32)sdata->operation) )
  {
    Gci_container* bucket= find_bucket(&m_active_gci, gci);
      
    DBUG_PRINT("info", ("data insertion in eventId %d", op->m_eventId));
    DBUG_PRINT("info", ("gci=%d tab=%d op=%d node=%d",
                        sdata->gci, sdata->tableId, sdata->operation,
                        sdata->req_nodeid));

    if (unlikely(bucket == 0))
    {
      /**
       * Already completed GCI...
       *   Possible in case of resend during NF handling
       */
      DBUG_RETURN(0);
    }

    bool use_hash =
      ! op->m_separateEvents &&
      sdata->operation < NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT;

    // find position in bucket hash table
    EventBufData* data = 0;
    EventBufData_hash::Pos hpos;
    if (use_hash)
    {
      bucket->m_data_hash.search(hpos, op, ptr);
      data = hpos.data;
    }

    if (data == 0)
    {
      // allocate new result buffer
      data = alloc_data();
      if (unlikely(data == 0))
      {
        op->m_has_error = 2;
        DBUG_RETURN(-1);
      }

      if (unlikely(copy_data(sdata, ptr, data)))
      {
        op->m_has_error = 3;
        DBUG_RETURN(-1);
      }
      // add it to list and hash table
      bucket->m_data.append(data);
      if (use_hash)
      {
        bucket->m_data_hash.append(hpos, data);
      }
#ifdef VM_TRACE
      op->m_data_count++;
#endif
    }
    else
    {
      // event with same op, PK found, merge into old buffer
      if (unlikely(merge_data(sdata, ptr, data)))
      {
        op->m_has_error = 3;
        DBUG_RETURN(-1);
      }
    }
    data->m_event_op = op;
    if (use_hash)
    {
      data->m_pkhash = hpos.pkhash;
    }
    DBUG_RETURN(0);
  }

#ifdef VM_TRACE
  if ((Uint32)op->m_eventImpl->mi_type & 1 << (Uint32)sdata->operation)
  {
    // XXX never reached
    DBUG_PRINT("info",("Data arrived before ready eventId", op->m_eventId));
    DBUG_RETURN(0);
  }
  else {
    DBUG_PRINT("info",("skipped"));
    DBUG_RETURN(0);
  }
#else
  return 0;
#endif
}

// allocate EventBufData
EventBufData*
NdbEventBuffer::alloc_data()
{
  DBUG_ENTER("alloc_data");
  EventBufData* data = m_free_data;

  if (unlikely(data == 0))
  {
#ifdef VM_TRACE
    assert(m_free_data_count == 0);
    assert(m_free_data_sz == 0);
#endif
    expand(4000);
    reportStatus();

    data = m_free_data;
    if (unlikely(data == 0))
    {
#ifdef VM_TRACE
      printf("m_latest_command: %s\n", m_latest_command);
      printf("no free data, m_latestGCI %lld\n",
             m_latestGCI);
      printf("m_free_data_count %d\n", m_free_data_count);
      printf("m_available_data_count %d first gci %d last gci %d\n",
             m_available_data.m_count,
             m_available_data.m_head ? m_available_data.m_head->sdata->gci : 0,
             m_available_data.m_tail ? m_available_data.m_tail->sdata->gci : 0);
      printf("m_used_data_count %d\n", m_used_data.m_count);
#endif
      DBUG_RETURN(0); // TODO handle this, overrun, or, skip?
    }
  }

  // remove data from free list
  m_free_data = data->m_next;
  data->m_next = 0;
#ifdef VM_TRACE
  m_free_data_count--;
  assert(m_free_data_sz >= data->sz);
#endif
  m_free_data_sz -= data->sz;
  DBUG_RETURN(data);
}

// allocate initial or bigger memory area in EventBufData
// takes sizes from given ptr and sets up data->ptr
int
NdbEventBuffer::alloc_mem(EventBufData* data, LinearSectionPtr ptr[3])
{
  const Uint32 min_alloc_size = 128;

  Uint32 sz4 = (sizeof(SubTableData) + 3) >> 2;
  Uint32 alloc_size = (sz4 + ptr[0].sz + ptr[1].sz + ptr[2].sz) << 2;
  if (alloc_size < min_alloc_size)
    alloc_size = min_alloc_size;

  if (data->sz < alloc_size)
  {
    NdbMem_Free((char*)data->memory);
    assert(m_total_alloc >= data->sz);
    m_total_alloc -= data->sz;
    data->memory = 0;
    data->sz = 0;

    data->memory = (Uint32*)NdbMem_Allocate(alloc_size);
    if (data->memory == 0)
      return -1;
    data->sz = alloc_size;
    m_total_alloc += data->sz;
  }

  Uint32* memptr = data->memory;
  memptr += sz4;
  int i;
  for (i = 0; i <= 2; i++)
  {
    data->ptr[i].p = memptr;
    data->ptr[i].sz = ptr[i].sz;
    memptr += ptr[i].sz;
  }

  return 0;
}

int 
NdbEventBuffer::copy_data(const SubTableData * const sdata,
                          LinearSectionPtr ptr[3],
                          EventBufData* data)
{
  DBUG_ENTER("NdbEventBuffer::copy_data");

  if (alloc_mem(data, ptr) != 0)
    DBUG_RETURN(-1);
  memcpy(data->sdata, sdata, sizeof(SubTableData));
  int i;
  for (i = 0; i <= 2; i++)
    memcpy(data->ptr[i].p, ptr[i].p, ptr[i].sz << 2);
  DBUG_RETURN(0);
}

static struct Ev_t {
  enum {
    INS = NdbDictionary::Event::_TE_INSERT,
    DEL = NdbDictionary::Event::_TE_DELETE,
    UPD = NdbDictionary::Event::_TE_UPDATE,
    NUL = NdbDictionary::Event::_TE_NUL,
    ERR = 255
  };
  int t1, t2, t3;
} ev_t[] = {
  { Ev_t::INS, Ev_t::INS, Ev_t::ERR },
  { Ev_t::INS, Ev_t::DEL, Ev_t::NUL }, //ok
  { Ev_t::INS, Ev_t::UPD, Ev_t::INS }, //ok
  { Ev_t::DEL, Ev_t::INS, Ev_t::UPD }, //ok
  { Ev_t::DEL, Ev_t::DEL, Ev_t::ERR },
  { Ev_t::DEL, Ev_t::UPD, Ev_t::ERR },
  { Ev_t::UPD, Ev_t::INS, Ev_t::ERR },
  { Ev_t::UPD, Ev_t::DEL, Ev_t::DEL }, //ok
  { Ev_t::UPD, Ev_t::UPD, Ev_t::UPD }  //ok
};

/*
 *   | INS            | DEL              | UPD
 * 0 | pk ah + all ah | pk ah            | pk ah + new ah 
 * 1 | pk ad + all ad | old pk ad        | new pk ad + new ad 
 * 2 | empty          | old non-pk ah+ad | old ah+ad
 */

static AttributeHeader
copy_head(Uint32& i1, Uint32* p1, Uint32& i2, const Uint32* p2,
          Uint32 flags)
{
  AttributeHeader ah(p2[i2]);
  bool do_copy = (flags & 1);
  if (do_copy)
    p1[i1] = p2[i2];
  i1++;
  i2++;
  return ah;
}

static void
copy_attr(AttributeHeader ah,
          Uint32& j1, Uint32* p1, Uint32& j2, const Uint32* p2,
          Uint32 flags)
{
  bool do_copy = (flags & 1);
  bool with_head = (flags & 2);
  Uint32 n = with_head + ah.getDataSize();
  if (do_copy)
  {
    Uint32 k;
    for (k = 0; k < n; k++)
      p1[j1++] = p2[j2++];
  }
  else
  {
    j1 += n;
    j2 += n;
  }
}

int 
NdbEventBuffer::merge_data(const SubTableData * const sdata,
                           LinearSectionPtr ptr2[3],
                           EventBufData* data)
{
  DBUG_ENTER("NdbEventBuffer::merge_data");

  Uint32 nkey = data->m_event_op->m_eventImpl->m_tableImpl->m_noOfKeys;

  int t1 = data->sdata->operation;
  int t2 = sdata->operation;
  if (t1 == Ev_t::NUL)
    DBUG_RETURN(copy_data(sdata, ptr2, data));

  Ev_t* tp = 0;
  int i;
  for (i = 0; i < sizeof(ev_t)/sizeof(ev_t[0]); i++) {
    if (ev_t[i].t1 == t1 && ev_t[i].t2 == t2) {
      tp = &ev_t[i];
      break;
    }
  }
  assert(tp != 0 && tp->t3 != Ev_t::ERR);

  // save old data
  EventBufData olddata = *data;
  data->memory = 0;
  data->sz = 0;

  // compose ptr1 o ptr2 = ptr
  LinearSectionPtr (&ptr1) [3] = olddata.ptr;
  LinearSectionPtr (&ptr) [3] = data->ptr;

  // loop twice where first loop only sets sizes
  int loop;
  for (loop = 0; loop <= 1; loop++)
  {
    if (loop == 1)
    {
      if (alloc_mem(data, ptr) != 0)
        DBUG_RETURN(-1);
      *data->sdata = *sdata;
      data->sdata->operation = tp->t3;
    }

    ptr[0].sz = ptr[1].sz = ptr[3].sz = 0;

    // copy pk from new version
    {
      AttributeHeader ah;
      Uint32 i = 0;
      Uint32 j = 0;
      Uint32 i2 = 0;
      Uint32 j2 = 0;
      while (i < nkey)
      {
        ah = copy_head(i, ptr[0].p, i2, ptr2[0].p, loop);
        copy_attr(ah, j, ptr[1].p, j2, ptr2[1].p, loop);
      }
      ptr[0].sz = i;
      ptr[1].sz = j;
    }

    // merge after values, new version overrides
    if (tp->t3 != Ev_t::DEL)
    {
      AttributeHeader ah;
      Uint32 i = ptr[0].sz;
      Uint32 j = ptr[1].sz;
      Uint32 i1 = 0;
      Uint32 j1 = 0;
      Uint32 i2 = nkey;
      Uint32 j2 = ptr[1].sz;
      while (i1 < nkey)
      {
        j1 += AttributeHeader(ptr1[0].p[i1++]).getDataSize();
      }
      while (1)
      {
        bool b1 = (i1 < ptr1[0].sz);
        bool b2 = (i2 < ptr2[0].sz);
        if (b1 && b2)
        {
          Uint32 id1 = AttributeHeader(ptr1[0].p[i1]).getAttributeId();
          Uint32 id2 = AttributeHeader(ptr2[0].p[i2]).getAttributeId();
          if (id1 < id2)
            b2 = false;
          else if (id1 > id2)
            b1 = false;
          else
          {
            j1 += AttributeHeader(ptr1[0].p[i1++]).getDataSize();
            b1 = false;
          }
        }
        if (b1)
        {
          ah = copy_head(i, ptr[0].p, i1, ptr1[0].p, loop);
          copy_attr(ah, j, ptr[1].p, j1, ptr1[1].p, loop);
        }
        else if (b2)
        {
          ah = copy_head(i, ptr[0].p, i2, ptr2[0].p, loop);
          copy_attr(ah, j, ptr[1].p, j2, ptr2[1].p, loop);
        }
        else
          break;
      }
      ptr[0].sz = i;
      ptr[1].sz = j;
    }

    // merge before values, old version overrides
    if (tp->t3 != Ev_t::INS)
    {
      AttributeHeader ah;
      Uint32 k = 0;
      Uint32 k1 = 0;
      Uint32 k2 = 0;
      while (1)
      {
        bool b1 = (k1 < ptr1[2].sz);
        bool b2 = (k2 < ptr2[2].sz);
        if (b1 && b2)
        {
          Uint32 id1 = AttributeHeader(ptr1[2].p[k1]).getAttributeId();
          Uint32 id2 = AttributeHeader(ptr2[2].p[k2]).getAttributeId();
          if (id1 < id2)
            b2 = false;
          else if (id1 > id2)
            b1 = false;
          else
          {
            k2 += 1 + AttributeHeader(ptr2[2].p[k2]).getDataSize();
            b2 = false;
          }
        }
        if (b1)
        {
          ah = AttributeHeader(ptr1[2].p[k1]);
          copy_attr(ah, k, ptr[2].p, k1, ptr1[2].p, loop | 2);
        }
        else if (b2)
        {
          ah = AttributeHeader(ptr2[2].p[k2]);
          copy_attr(ah, k, ptr[2].p, k2, ptr2[2].p, loop | 2);
        }
        else
          break;
      }
      ptr[2].sz = k;
    }
  }

  // free old data
  NdbMem_Free((char*)olddata.memory);

  DBUG_RETURN(0);
}

NdbEventOperationImpl *
NdbEventBuffer::move_data()
{
  // handle received data
  if (!m_complete_data.m_data.is_empty())
  {
    // move this list to last in m_available_data
    m_available_data.append(m_complete_data.m_data);

    bzero(&m_complete_data, sizeof(m_complete_data));
  }

  // handle used data
  if (!m_used_data.is_empty())
  {
    // return m_used_data to m_free_data
    free_list(m_used_data);
  }
  if (!m_available_data.is_empty())
  {
    DBUG_ENTER("NdbEventBuffer::move_data");
#ifdef VM_TRACE
    DBUG_PRINT("exit",("m_available_data_count %u", m_available_data.m_count));
#endif
    DBUG_RETURN(m_available_data.m_head->m_event_op);
  }
  return 0;
}

void
NdbEventBuffer::free_list(EventBufData_list &list)
{
  // return list to m_free_data
  list.m_tail->m_next= m_free_data;
  m_free_data= list.m_head;
#ifdef VM_TRACE
  m_free_data_count+= list.m_count;
#endif
  m_free_data_sz+= list.m_sz;

  // list returned to m_free_data
  new (&list) EventBufData_list;
}

NdbEventOperation*
NdbEventBuffer::createEventOperation(const char* eventName,
				     NdbError &theError)
{
  DBUG_ENTER("NdbEventBuffer::createEventOperation");
  NdbEventOperation* tOp= new NdbEventOperation(m_ndb, eventName);
  if (tOp == 0)
  {
    theError.code= 4000;
    DBUG_RETURN(NULL);
  }
  if (tOp->getState() != NdbEventOperation::EO_CREATED) {
    theError.code= tOp->getNdbError().code;
    delete tOp;
    DBUG_RETURN(NULL);
  }
  DBUG_RETURN(tOp);
}

void
NdbEventBuffer::dropEventOperation(NdbEventOperation* tOp)
{
  NdbEventOperationImpl* op= getEventOperationImpl(tOp);

  op->stop();

  op->m_next= m_dropped_ev_op;
  op->m_prev= 0;
  if (m_dropped_ev_op)
    m_dropped_ev_op->m_prev= op;
  m_dropped_ev_op= op;

  // ToDo, take care of these to be deleted at the
  // appropriate time, after we are sure that there
  // are _no_ more events coming

  //  delete tOp;
}

void
NdbEventBuffer::reportStatus()
{
  EventBufData *apply_buf= m_available_data.m_head;
  Uint64 apply_gci, latest_gci= m_latestGCI;
  if (apply_buf == 0)
    apply_buf= m_complete_data.m_data.m_head;
  if (apply_buf)
    apply_gci= apply_buf->sdata->gci;
  else
    apply_gci= latest_gci;

  if (100*m_free_data_sz < m_min_free_thresh*m_total_alloc &&
      m_total_alloc > 1024*1024)
  {
    /* report less free buffer than m_free_thresh,
       next report when more free than 2 * m_free_thresh
    */
    m_min_free_thresh= 0;
    m_max_free_thresh= 2 * m_free_thresh;
    goto send_report;
  }
  
  if (100*m_free_data_sz > m_max_free_thresh*m_total_alloc &&
      m_total_alloc > 1024*1024)
  {
    /* report more free than 2 * m_free_thresh
       next report when less free than m_free_thresh
    */
    m_min_free_thresh= m_free_thresh;
    m_max_free_thresh= 100;
    goto send_report;
  }
  if (latest_gci-apply_gci >=  m_gci_slip_thresh)
  {
    goto send_report;
  }
  return;

send_report:
  Uint32 data[8];
  data[0]= NDB_LE_EventBufferStatus;
  data[1]= m_total_alloc-m_free_data_sz;
  data[2]= m_total_alloc;
  data[3]= 0;
  data[4]= apply_gci & ~(Uint32)0;
  data[5]= apply_gci >> 32;
  data[6]= latest_gci & ~(Uint32)0;
  data[7]= latest_gci >> 32;
  m_ndb->theImpl->send_event_report(data,8);
#ifdef VM_TRACE
  assert(m_total_alloc >= m_free_data_sz);
#endif
}

// hash table routines

// could optimize the all-fixed case
Uint32
EventBufData_hash::getpkhash(NdbEventOperationImpl* op, LinearSectionPtr ptr[3])
{
  const NdbTableImpl* tab = op->m_eventImpl->m_tableImpl;

  // in all cases ptr[0] = pk ah.. ptr[1] = pk ad..
  // for pk update (to equivalent pk) post/pre values give same hash
  Uint32 nkey = tab->m_noOfKeys;
  assert(nkey != 0 && nkey <= ptr[0].sz);
  const Uint32* hptr = ptr[0].p;
  const uchar* dptr = (uchar*)ptr[1].p;

  // hash registers
  ulong nr1 = 0;
  ulong nr2 = 0;
  while (nkey-- != 0)
  {
    AttributeHeader ah(*hptr++);
    Uint32 bytesize = ah.getByteSize();
    assert(dptr + bytesize <= (uchar*)(ptr[1].p + ptr[1].sz));

    Uint32 i = ah.getAttributeId();
    const NdbColumnImpl* col = tab->getColumn(i);
    assert(col != 0);

    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(col->m_type, dptr, bytesize, lb, len);
    assert(ok);

    CHARSET_INFO* cs = col->m_cs ? col->m_cs : &my_charset_bin;
    (*cs->coll->hash_sort)(cs, dptr + lb, len, &nr1, &nr2);
    dptr += bytesize;
  }
  return nr1;
}

// this is seldom invoked
bool
EventBufData_hash::getpkequal(NdbEventOperationImpl* op, LinearSectionPtr ptr1[3], LinearSectionPtr ptr2[3])
{
  const NdbTableImpl* tab = op->m_eventImpl->m_tableImpl;

  Uint32 nkey = tab->m_noOfKeys;
  assert(nkey != 0 && nkey <= ptr1[0].sz && nkey <= ptr2[0].sz);
  const Uint32* hptr1 = ptr1[0].p;
  const Uint32* hptr2 = ptr2[0].p;
  const uchar* dptr1 = (uchar*)ptr1[1].p;
  const uchar* dptr2 = (uchar*)ptr2[1].p;

  while (nkey-- != 0)
  {
    AttributeHeader ah1(*hptr1++);
    AttributeHeader ah2(*hptr2++);
    // sizes can differ on update of varchar endspace
    Uint32 bytesize1 = ah1.getByteSize();
    Uint32 bytesize2 = ah1.getByteSize();
    assert(dptr1 + bytesize1 <= (uchar*)(ptr1[1].p + ptr1[1].sz));
    assert(dptr2 + bytesize2 <= (uchar*)(ptr2[1].p + ptr2[1].sz));

    assert(ah1.getAttributeId() == ah2.getAttributeId());
    Uint32 i = ah1.getAttributeId();
    const NdbColumnImpl* col = tab->getColumn(i);
    assert(col != 0);

    Uint32 lb1, len1;
    bool ok1 = NdbSqlUtil::get_var_length(col->m_type, dptr1, bytesize1, lb1, len1);
    Uint32 lb2, len2;
    bool ok2 = NdbSqlUtil::get_var_length(col->m_type, dptr2, bytesize2, lb2, len2);
    assert(ok1 && ok2 && lb1 == lb2);

    CHARSET_INFO* cs = col->m_cs ? col->m_cs : &my_charset_bin;
    int res = (cs->coll->strnncollsp)(cs, dptr1 + lb1, len1, dptr2 + lb2, len2, false);
    if (res != 0)
      return false;
    dptr1 += bytesize1;
    dptr2 += bytesize2;
  }
  return true;
}

void
EventBufData_hash::search(Pos& hpos, NdbEventOperationImpl* op, LinearSectionPtr ptr[3])
{
  Uint32 pkhash = getpkhash(op, ptr);
  Uint32 index = (op->m_oid ^ pkhash) % GCI_EVENT_HASH_SIZE;
  EventBufData* data = m_hash[index];
  while (data != 0)
  {
    if (data->m_event_op == op &&
        data->m_pkhash == pkhash &&
        getpkequal(op, data->ptr, ptr))
      break;
    data = data->m_next_hash;
  }
  hpos.index = index;
  hpos.data = data;
  hpos.pkhash = pkhash;
}

template class Vector<Gci_container>;
template class Vector<NdbEventBuffer::EventBufData_chunk*>;
