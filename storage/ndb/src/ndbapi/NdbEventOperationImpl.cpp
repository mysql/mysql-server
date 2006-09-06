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
#include <NdbBlob.hpp>
#include <NdbEventOperation.hpp>
#include "NdbEventOperationImpl.hpp"
#include <signaldata/AlterTable.hpp>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

static Gci_container_pod g_empty_gci_container;
static const Uint32 ACTIVE_GCI_DIRECTORY_SIZE = 4;
static const Uint32 ACTIVE_GCI_MASK = ACTIVE_GCI_DIRECTORY_SIZE - 1;

#ifdef VM_TRACE
static void
print_std(const SubTableData * sdata, LinearSectionPtr ptr[3])
{
  printf("addr=%p gci=%d op=%d\n", (void*)sdata, sdata->gci, 
	 SubTableData::getOperation(sdata->requestInfo));
  for (int i = 0; i <= 2; i++) {
    printf("sec=%d addr=%p sz=%d\n", i, (void*)ptr[i].p, ptr[i].sz);
    for (int j = 0; j < ptr[i].sz; j++)
      printf("%08x ", ptr[i].p[j]);
    printf("\n");
  }
}
#endif

// EventBufData

Uint32
EventBufData::get_blob_part_no() const
{
  assert(ptr[0].sz > 2);
  Uint32 pos = AttributeHeader(ptr[0].p[0]).getDataSize() +
              AttributeHeader(ptr[0].p[1]).getDataSize();
  Uint32 no = ptr[1].p[pos];
  return no;
}

void
EventBufData::add_part_size(Uint32 & full_count, Uint32 & full_sz) const
{
  Uint32 tmp_count = 0;
  Uint32 tmp_sz = 0;
  const EventBufData* data2 = m_next_blob;
  while (data2 != 0) {
    tmp_count++;
    tmp_sz += data2->sz;
    const EventBufData* data3 = data2->m_next;
    while (data3 != 0) {
      tmp_count++;
      tmp_sz += data3->sz;
      data3 = data3->m_next;
    }
    data2 = data2->m_next_blob;
  }
  full_count += tmp_count;
  full_sz += tmp_sz;
}

/*
 * Class NdbEventOperationImpl
 *
 *
 */

// todo handle several ndb objects
// todo free allocated data when closing NdbEventBuffer

NdbEventOperationImpl::NdbEventOperationImpl(NdbEventOperation &f,
					     Ndb *theNdb, 
					     const char* eventName) :
  NdbEventOperation(*this),
  m_facade(&f),
  m_ndb(theNdb),
  m_state(EO_ERROR),
  m_oid(~(Uint32)0)
{
  DBUG_ENTER("NdbEventOperationImpl::NdbEventOperationImpl");

  assert(m_ndb != NULL);
  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  assert(myDict != NULL);

  const NdbDictionary::Event *myEvnt = myDict->getEvent(eventName);
  if (!myEvnt)
  {
    m_error.code= myDict->getNdbError().code;
    DBUG_VOID_RETURN;
  }

  init(myEvnt->m_impl);
  DBUG_VOID_RETURN;
}

NdbEventOperationImpl::NdbEventOperationImpl(Ndb *theNdb,
                                             NdbEventImpl& evnt) :
  NdbEventOperation(*this),
  m_facade(this),
  m_ndb(theNdb),
  m_state(EO_ERROR),
  m_oid(~(Uint32)0)
{
  DBUG_ENTER("NdbEventOperationImpl::NdbEventOperationImpl [evnt]");
  init(evnt);
  DBUG_VOID_RETURN;
}

void
NdbEventOperationImpl::init(NdbEventImpl& evnt)
{
  DBUG_ENTER("NdbEventOperationImpl::init");

  m_magic_number = 0;
  mi_type = 0;
  m_change_mask = 0;
#ifdef VM_TRACE
  m_data_done_count = 0;
  m_data_count = 0;
#endif
  m_next = 0;
  m_prev = 0;

  m_eventId = 0;
  theFirstPkAttrs[0] = NULL;
  theCurrentPkAttrs[0] = NULL;
  theFirstPkAttrs[1] = NULL;
  theCurrentPkAttrs[1] = NULL;
  theFirstDataAttrs[0] = NULL;
  theCurrentDataAttrs[0] = NULL;
  theFirstDataAttrs[1] = NULL;
  theCurrentDataAttrs[1] = NULL;

  theBlobList = NULL;
  theBlobOpList = NULL;
  theMainOp = NULL;

  m_data_item= NULL;
  m_eventImpl = NULL;

  m_custom_data= 0;
  m_has_error= 1;

  // we should lookup id in Dictionary, TODO
  // also make sure we only have one listener on each event

  m_eventImpl = &evnt;

  m_eventId = m_eventImpl->m_eventId;

  m_oid= m_ndb->theImpl->theNdbObjectIdMap.map(this);

  m_state= EO_CREATED;

  m_node_bit_mask.clear();
#ifdef ndb_event_stores_merge_events_flag
  m_mergeEvents = m_eventImpl->m_mergeEvents;
#else
  m_mergeEvents = false;
#endif
  m_ref_count = 0;
  DBUG_PRINT("info", ("m_ref_count = 0 for op: %p", this));

  m_has_error= 0;

  DBUG_PRINT("exit",("this: 0x%x oid: %u", this, m_oid));
  DBUG_VOID_RETURN;
}

NdbEventOperationImpl::~NdbEventOperationImpl()
{
  DBUG_ENTER("NdbEventOperationImpl::~NdbEventOperationImpl");
  m_magic_number= 0;

  if (m_oid == ~(Uint32)0)
    DBUG_VOID_RETURN;

  stop();
  // m_bufferHandle->dropSubscribeEvent(m_bufferId);
  ; // ToDo? We should send stop signal here
  
  if (theMainOp == NULL)
  {
    NdbEventOperationImpl* tBlobOp = theBlobOpList;
    while (tBlobOp != NULL)
    {
      NdbEventOperationImpl *op = tBlobOp;
      tBlobOp = tBlobOp->m_next;
      delete op;
    }
  }

  m_ndb->theImpl->theNdbObjectIdMap.unmap(m_oid, this);
  DBUG_PRINT("exit",("this: %p/%p oid: %u main: %p",
             this, m_facade, m_oid, theMainOp));

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

NdbBlob*
NdbEventOperationImpl::getBlobHandle(const char *colName, int n)
{
  DBUG_ENTER("NdbEventOperationImpl::getBlobHandle (colName)");

  assert(m_mergeEvents);

  if (m_state != EO_CREATED) {
    ndbout_c("NdbEventOperationImpl::getBlobHandle may only be called between "
	     "instantiation and execute()");
    DBUG_RETURN(NULL);
  }

  NdbColumnImpl *tAttrInfo = m_eventImpl->m_tableImpl->getColumn(colName);

  if (tAttrInfo == NULL) {
    ndbout_c("NdbEventOperationImpl::getBlobHandle attribute %s not found",colName);
    DBUG_RETURN(NULL);
  }

  NdbBlob* bh = getBlobHandle(tAttrInfo, n);
  DBUG_RETURN(bh);
}

NdbBlob*
NdbEventOperationImpl::getBlobHandle(const NdbColumnImpl *tAttrInfo, int n)
{
  DBUG_ENTER("NdbEventOperationImpl::getBlobHandle");
  DBUG_PRINT("info", ("attr=%s post/pre=%d", tAttrInfo->m_name.c_str(), n));
  
  // as in NdbOperation, create only one instance
  NdbBlob* tBlob = theBlobList;
  NdbBlob* tLastBlob = NULL;
  while (tBlob != NULL) {
    if (tBlob->theColumn == tAttrInfo && tBlob->theEventBlobVersion == n)
      DBUG_RETURN(tBlob);
    tLastBlob = tBlob;
    tBlob = tBlob->theNext;
  }

  NdbEventOperationImpl* tBlobOp = NULL;

  const bool is_tinyblob = (tAttrInfo->getPartSize() == 0);
  assert(is_tinyblob == (tAttrInfo->m_blobTable == NULL));

  if (! is_tinyblob) {
    // blob event name
    char bename[MAX_TAB_NAME_SIZE];
    NdbBlob::getBlobEventName(bename, m_eventImpl, tAttrInfo);

    // find blob event op if any (it serves both post and pre handles)
    tBlobOp = theBlobOpList;
    NdbEventOperationImpl* tLastBlopOp = NULL;
    while (tBlobOp != NULL) {
      if (strcmp(tBlobOp->m_eventImpl->m_name.c_str(), bename) == 0) {
        break;
      }
      tLastBlopOp = tBlobOp;
      tBlobOp = tBlobOp->m_next;
    }

    DBUG_PRINT("info", ("%s blob event op for %s",
                        tBlobOp ? " reuse" : " create", bename));

    // create blob event op if not found
    if (tBlobOp == NULL) {
      // get blob event
      NdbDictionaryImpl& dict =
        NdbDictionaryImpl::getImpl(*m_ndb->getDictionary());
      NdbEventImpl* blobEvnt =
        dict.getBlobEvent(*this->m_eventImpl, tAttrInfo->m_column_no);
      if (blobEvnt == NULL) {
        m_error.code = dict.m_error.code;
        DBUG_RETURN(NULL);
      }

      // create blob event operation
      tBlobOp =
        m_ndb->theEventBuffer->createEventOperation(*blobEvnt, m_error);
      if (tBlobOp == NULL)
        DBUG_RETURN(NULL);

      // pointer to main table op
      tBlobOp->theMainOp = this;
      tBlobOp->m_mergeEvents = m_mergeEvents;

      // to hide blob op it is linked under main op, not under m_ndb
      if (tLastBlopOp == NULL)
        theBlobOpList = tBlobOp;
      else
        tLastBlopOp->m_next = tBlobOp;
      tBlobOp->m_next = NULL;
    }
  }

  tBlob = m_ndb->getNdbBlob();
  if (tBlob == NULL) {
    m_error.code = m_ndb->getNdbError().code;
    DBUG_RETURN(NULL);
  }

  // calls getValue on inline and blob part
  if (tBlob->atPrepare(this, tBlobOp, tAttrInfo, n) == -1) {
    m_error.code = tBlob->getNdbError().code;
    m_ndb->releaseNdbBlob(tBlob);
    DBUG_RETURN(NULL);
  }

  // add to list end
  if (tLastBlob == NULL)
    theBlobList = tBlob;
  else
    tLastBlob->theNext = tBlob;
  tBlob->theNext = NULL;
  DBUG_RETURN(tBlob);
}

int
NdbEventOperationImpl::readBlobParts(char* buf, NdbBlob* blob,
                                     Uint32 part, Uint32 count)
{
  DBUG_ENTER_EVENT("NdbEventOperationImpl::readBlobParts");
  DBUG_PRINT_EVENT("info", ("part=%u count=%u post/pre=%d",
                      part, count, blob->theEventBlobVersion));

  NdbEventOperationImpl* blob_op = blob->theBlobEventOp;

  EventBufData* main_data = m_data_item;
  DBUG_PRINT_EVENT("info", ("main_data=%p", main_data));
  assert(main_data != NULL);

  // search for blob parts list head
  EventBufData* head;
  assert(m_data_item != NULL);
  head = m_data_item->m_next_blob;
  while (head != NULL)
  {
    if (head->m_event_op == blob_op)
    {
      DBUG_PRINT_EVENT("info", ("found blob parts head %p", head));
      break;
    }
    head = head->m_next_blob;
  }

  Uint32 nparts = 0;
  EventBufData* data = head;
  // XXX optimize using part no ordering
  while (data != NULL)
  {
    /*
     * Hack part no directly out of buffer since it is not returned
     * in pre data (PK buglet).  For part data use receive_event().
     * This means extra copy.
     */
    blob_op->m_data_item = data;
    int r = blob_op->receive_event();
    assert(r > 0);
    Uint32 no = data->get_blob_part_no();
    Uint32 sz = blob->thePartSize;
    const char* src = blob->theBlobEventDataBuf.data;

    DBUG_PRINT_EVENT("info", ("part_data=%p part no=%u part sz=%u", data, no, sz));

    if (part <= no && no < part + count)
    {
      DBUG_PRINT_EVENT("info", ("part within read range"));
      memcpy(buf + (no - part) * sz, src, sz);
      nparts++;
    }
    else
    {
      DBUG_PRINT_EVENT("info", ("part outside read range"));
    }
    data = data->m_next;
  }
  assert(nparts == count);

  DBUG_RETURN_EVENT(0);
}

int
NdbEventOperationImpl::execute()
{
  DBUG_ENTER("NdbEventOperationImpl::execute");
  m_ndb->theEventBuffer->add_drop_lock();
  int r = execute_nolock();
  m_ndb->theEventBuffer->add_drop_unlock();
  DBUG_RETURN(r);
}

int
NdbEventOperationImpl::execute_nolock()
{
  DBUG_ENTER("NdbEventOperationImpl::execute_nolock");
  DBUG_PRINT("info", ("this=%p type=%s", this, !theMainOp ? "main" : "blob"));

  NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
  if (!myDict) {
    m_error.code= m_ndb->getNdbError().code;
    DBUG_RETURN(-1);
  }

  if (theFirstPkAttrs[0] == NULL && 
      theFirstDataAttrs[0] == NULL) { // defaults to get all
  }

  m_magic_number= NDB_EVENT_OP_MAGIC_NUMBER;
  m_state= EO_EXECUTING;
  mi_type= m_eventImpl->mi_type;
  m_ndb->theEventBuffer->add_op();
  int r= NdbDictionaryImpl::getImpl(*myDict).executeSubscribeEvent(*this);
  if (r == 0) {
    if (theMainOp == NULL) {
      DBUG_PRINT("info", ("execute blob ops"));
      NdbEventOperationImpl* blob_op = theBlobOpList;
      while (blob_op != NULL) {
        r = blob_op->execute_nolock();
        if (r != 0)
          break;
        blob_op = blob_op->m_next;
      }
    }
    if (r == 0)
    {
      m_ref_count++;
      DBUG_PRINT("info", ("m_ref_count: %u for op: %p", m_ref_count, this));
      DBUG_RETURN(0);
    }
  }
  //Error
  m_state= EO_ERROR;
  mi_type= 0;
  m_magic_number= 0;
  m_error.code= myDict->getNdbError().code;
  m_ndb->theEventBuffer->remove_op();
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

const bool NdbEventOperationImpl::tableNameChanged() const
{
  return (bool)AlterTableReq::getNameFlag(m_change_mask);
}

const bool NdbEventOperationImpl::tableFrmChanged() const
{
  return (bool)AlterTableReq::getFrmFlag(m_change_mask);
}

const bool NdbEventOperationImpl::tableFragmentationChanged() const
{
  return (bool)AlterTableReq::getFragDataFlag(m_change_mask);
}

const bool NdbEventOperationImpl::tableRangeListChanged() const
{
  return (bool)AlterTableReq::getRangeListFlag(m_change_mask);
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

bool
NdbEventOperationImpl::execSUB_TABLE_DATA(NdbApiSignal * signal, 
                                          LinearSectionPtr ptr[3])
{
  DBUG_ENTER("NdbEventOperationImpl::execSUB_TABLE_DATA");
  const SubTableData * const sdata=
    CAST_CONSTPTR(SubTableData, signal->getDataPtr());

  if(signal->isFirstFragment()){
    m_fragmentId = signal->getFragmentId();
    m_buffer.grow(4 * sdata->totalLen);
  } else {
    if(m_fragmentId != signal->getFragmentId()){
      abort();
    }
  }
  const Uint32 i = SubTableData::DICT_TAB_INFO;
  DBUG_PRINT("info", ("Accumulated %u bytes for fragment %u", 
                      4 * ptr[i].sz, m_fragmentId));
  m_buffer.append(ptr[i].p, 4 * ptr[i].sz);
  
  if(!signal->isLastFragment()){
    DBUG_RETURN(FALSE);
  }  
  
  DBUG_RETURN(TRUE);
}


int
NdbEventOperationImpl::receive_event()
{
  Uint32 operation= 
    SubTableData::getOperation(m_data_item->sdata->requestInfo);
  if (unlikely(operation >= NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT))
  {
    DBUG_ENTER("NdbEventOperationImpl::receive_event");
    DBUG_PRINT("info",("sdata->operation %u  this: %p", operation, this));
    if (operation == NdbDictionary::Event::_TE_ALTER)
    {
      // Parse the new table definition and
      // create a table object
      NdbDictionary::Dictionary *myDict = m_ndb->getDictionary();
      NdbDictionaryImpl *dict = & NdbDictionaryImpl::getImpl(*myDict);
      NdbError error;
      NdbDictInterface dif(error);
      NdbTableImpl *at;
      m_change_mask = m_data_item->sdata->changeMask;
      error.code = dif.parseTableInfo(&at,
                                      (Uint32*)m_buffer.get_data(), 
                                      m_buffer.length() / 4, 
                                      true);
      m_buffer.clear();
      if (unlikely(!at))
      {
        DBUG_PRINT("info", ("Failed to parse DictTabInfo error %u", 
                                  error.code));
        ndbout_c("Failed to parse DictTabInfo error %u", error.code);
        DBUG_RETURN(1);
      }
      at->buildColumnHash();
      
      NdbTableImpl *tmp_table_impl= m_eventImpl->m_tableImpl;
      m_eventImpl->m_tableImpl = at;
      
      DBUG_PRINT("info", ("switching table impl 0x%x -> 0x%x",
                          tmp_table_impl, at));
      
      // change the rec attrs to refer to the new table object
      int i;
      for (i = 0; i < 2; i++)
      {
        NdbRecAttr *p = theFirstPkAttrs[i];
        while (p)
        {
          int no = p->getColumn()->getColumnNo();
          NdbColumnImpl *tAttrInfo = at->getColumn(no);
          DBUG_PRINT("info", ("rec_attr: 0x%x  "
                              "switching column impl 0x%x -> 0x%x",
                              p, p->m_column, tAttrInfo));
          p->m_column = tAttrInfo;
          p = p->next();
        }
      }
      for (i = 0; i < 2; i++)
      {
        NdbRecAttr *p = theFirstDataAttrs[i];
        while (p)
        {
          int no = p->getColumn()->getColumnNo();
          NdbColumnImpl *tAttrInfo = at->getColumn(no);
          DBUG_PRINT("info", ("rec_attr: 0x%x  "
                              "switching column impl 0x%x -> 0x%x",
                              p, p->m_column, tAttrInfo));
          p->m_column = tAttrInfo;
          p = p->next();
        }
      }
      if (tmp_table_impl) 
        delete tmp_table_impl;
    }
    DBUG_RETURN(1);
  }

  DBUG_ENTER_EVENT("NdbEventOperationImpl::receive_event");
  DBUG_PRINT_EVENT("info",("sdata->operation %u  this: %p", operation, this));
  // now move the data into the RecAttrs
    
  int is_update= operation == NdbDictionary::Event::_TE_UPDATE;

  Uint32 *aAttrPtr = m_data_item->ptr[0].p;
  Uint32 *aAttrEndPtr = aAttrPtr + m_data_item->ptr[0].sz;
  Uint32 *aDataPtr = m_data_item->ptr[1].p;

  DBUG_DUMP_EVENT("after",(char*)m_data_item->ptr[1].p, m_data_item->ptr[1].sz*4);
  DBUG_DUMP_EVENT("before",(char*)m_data_item->ptr[2].p, m_data_item->ptr[2].sz*4);

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
      DBUG_PRINT_EVENT("info",("undef [%u] %u 0x%x [%u] 0x%x",
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
      
      DBUG_PRINT_EVENT("info",("set [%u] %u 0x%x [%u] 0x%x",
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
    DBUG_RETURN_EVENT(1);
  }

  DBUG_RETURN_EVENT(0);
}

NdbDictionary::Event::TableEvent 
NdbEventOperationImpl::getEventType()
{
  return (NdbDictionary::Event::TableEvent)
    (1 << SubTableData::getOperation(m_data_item->sdata->requestInfo));
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
  m_active_op_count(0),
  m_add_drop_mutex(0)
{
#ifdef VM_TRACE
  m_latest_command= "NdbEventBuffer::NdbEventBuffer";
  m_flush_gci = 0;
#endif

  if ((p_cond = NdbCondition_Create()) ==  NULL) {
    ndbout_c("NdbEventHandle: NdbCondition_Create() failed");
    exit(-1);
  }
  m_mutex= ndb->theImpl->theWaiter.m_mutex;
  // ToDo set event buffer size
  // pre allocate event data array
  m_sz= 0;
#ifdef VM_TRACE
  m_free_data_count= 0;
#endif
  m_free_data= 0;
  m_free_data_sz= 0;

  // get reference to mutex managed by current connection
  m_add_drop_mutex= 
    m_ndb->theImpl->m_ndb_cluster_connection.m_event_add_drop_mutex;

  // initialize lists
  bzero(&g_empty_gci_container, sizeof(Gci_container));
  init_gci_containers();
}

NdbEventBuffer::~NdbEventBuffer()
{
  // todo lock?  what if receive thread writes here?
  NdbEventOperationImpl* op= m_dropped_ev_op;  
  while ((op = m_dropped_ev_op))
  {
    m_dropped_ev_op = m_dropped_ev_op->m_next;
    delete op->m_facade;
  }

  unsigned j;
  Uint32 sz= m_active_gci.size();
  Gci_container* array = (Gci_container*)m_active_gci.getBase();
  for(j = 0; j < sz; j++)
  {
    array[j].~Gci_container();
  }

  for (j= 0; j < m_allocated_data.size(); j++)
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

int
NdbEventBuffer::flushIncompleteEvents(Uint64 gci)
{
  /**
   *  Find min complete gci
   */
  // called by user thread, so we need to lock the data
  lock();
  Uint32 i;
  Uint32 sz= m_active_gci.size();
  Gci_container* array = (Gci_container*)m_active_gci.getBase();
  for(i = 0; i < sz; i++)
  {
    Gci_container* tmp = array + i;
    if (tmp->m_gci && tmp->m_gci < gci)
    {
      // we have found an old not-completed gci, remove it
      ndbout_c("ndb: flushing incomplete epoch %lld (<%lld)", tmp->m_gci, gci);
      if(!tmp->m_data.is_empty())
      {
        free_list(tmp->m_data);
      }
      tmp->~Gci_container();
      bzero(tmp, sizeof(Gci_container));
    }
  }
#ifdef VM_TRACE
  m_flush_gci = gci;
#endif
  unlock();
  return 0;
}

NdbEventOperation *
NdbEventBuffer::nextEvent()
{
  DBUG_ENTER_EVENT("NdbEventBuffer::nextEvent");
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
    DBUG_PRINT_EVENT("info", ("available data=%p op=%p", data, op));

    /*
     * If merge is on, blob part sub-events must not be seen on this level.
     * If merge is not on, there are no blob part sub-events.
     */
    assert(op->theMainOp == NULL);

    // set NdbEventOperation data
    op->m_data_item= data;

    // remove item from m_available_data and return size
    Uint32 full_count, full_sz;
    m_available_data.remove_first(full_count, full_sz);

    // add it to used list
    m_used_data.append_used_data(data, full_count, full_sz);

#ifdef VM_TRACE
    op->m_data_done_count++;
#endif

    int r= op->receive_event();
    if (r > 0)
    {
      if (op->m_state == NdbEventOperation::EO_EXECUTING)
      {
#ifdef VM_TRACE
	m_latest_command= m_latest_command_save;
#endif
        NdbBlob* tBlob = op->theBlobList;
        while (tBlob != NULL)
        {
          (void)tBlob->atNextEvent();
          tBlob = tBlob->theNext;
        }
        EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
        while (gci_ops && op->getGCI() > gci_ops->m_gci)
        {
          deleteUsedEventOperations();
          gci_ops = m_available_data.next_gci_ops();
        }
        assert(gci_ops && (op->getGCI() == gci_ops->m_gci));
        // to return TE_NUL it should be made into data event
        if (SubTableData::getOperation(data->sdata->requestInfo) == 
	    NdbDictionary::Event::_TE_NUL)
        {
          DBUG_PRINT_EVENT("info", ("skip _TE_NUL"));
          continue;
        }
	DBUG_RETURN_EVENT(op->m_facade);
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

  // free all "per gci unique" collected operations
  EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
  while (gci_ops)
  {
    deleteUsedEventOperations();
    gci_ops = m_available_data.next_gci_ops();
  }
  DBUG_RETURN_EVENT(0);
}

NdbEventOperationImpl*
NdbEventBuffer::getGCIEventOperations(Uint32* iter, Uint32* event_types)
{
  DBUG_ENTER("NdbEventBuffer::getGCIEventOperations");
  EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
  if (*iter < gci_ops->m_gci_op_count)
  {
    EventBufData_list::Gci_op g = gci_ops->m_gci_op_list[(*iter)++];
    if (event_types != NULL)
      *event_types = g.event_types;
    DBUG_PRINT("info", ("gci: %d  g.op: %x  g.event_types: %x",
                        (unsigned)gci_ops->m_gci, g.op, g.event_types));
    DBUG_RETURN(g.op);
  }
  DBUG_RETURN(NULL);
}

void
NdbEventBuffer::deleteUsedEventOperations()
{
  Uint32 iter= 0;
  const NdbEventOperation *op_f;
  while ((op_f= getGCIEventOperations(&iter, NULL)) != NULL)
  {
    NdbEventOperationImpl *op = &op_f->m_impl;
    DBUG_ASSERT(op->m_ref_count > 0);
    op->m_ref_count--;
    DBUG_PRINT("info", ("m_ref_count: %u for op: %p", op->m_ref_count, op));
    if (op->m_ref_count == 0)
    {
      DBUG_PRINT("info", ("deleting op: %p", op));
      DBUG_ASSERT(op->m_node_bit_mask.isclear());
      if (op->m_next)
        op->m_next->m_prev = op->m_prev;
      if (op->m_prev)
        op->m_prev->m_next = op->m_next;
      else
        m_dropped_ev_op = op->m_next;
      delete op->m_facade;
    }
  }
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
NdbOut&
operator<<(NdbOut& out, const Gci_container_pod& gci)
{
  Gci_container* ptr = (Gci_container*)&gci;
  out << *ptr;
  return out;
}


static
Gci_container*
find_bucket_chained(Vector<Gci_container_pod> * active, Uint64 gci
#ifdef VM_TRACE
                    ,Uint64 flush_gci
#endif
                    )
{
  Uint32 pos = (gci & ACTIVE_GCI_MASK);
  Gci_container *bucket= ((Gci_container*)active->getBase()) + pos;

  if(gci > bucket->m_gci)
  {
    Gci_container* move;
    Uint32 move_pos = pos + ACTIVE_GCI_DIRECTORY_SIZE;
    do 
    {
      active->fill(move_pos, g_empty_gci_container);
      // Needs to recomputed after fill
      bucket = ((Gci_container*)active->getBase()) + pos; 
      move = ((Gci_container*)active->getBase()) + move_pos;
      if(move->m_gcp_complete_rep_count == 0)
      {
	memcpy(move, bucket, sizeof(Gci_container));
	bzero(bucket, sizeof(Gci_container));
	bucket->m_gci = gci;
	bucket->m_gcp_complete_rep_count = ~(Uint32)0;
#ifdef VM_TRACE
        if (gci < flush_gci)
        {
          ndbout_c("received old gci %llu < flush gci %llu", gci, flush_gci);
          assert(false);
        }
#endif
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
      {
#ifdef VM_TRACE
        if (gci < flush_gci)
        {
          ndbout_c("received old gci %llu < flush gci %llu", gci, flush_gci);
          assert(false);
        }
#endif
	return bucket;
      }
      
    } while(pos < size);
    
    return 0;
  }
}

inline
Gci_container*
find_bucket(Vector<Gci_container_pod> * active, Uint64 gci
#ifdef VM_TRACE
            ,Uint64 flush_gci
#endif
            )
{
  Uint32 pos = (gci & ACTIVE_GCI_MASK);
  Gci_container *bucket= ((Gci_container*)active->getBase()) + pos;
  if(likely(gci == bucket->m_gci))
    return bucket;

  return find_bucket_chained(active,gci
#ifdef VM_TRACE
                             , flush_gci
#endif
                             );
}

static
void
crash_on_invalid_SUB_GCP_COMPLETE_REP(const Gci_container* bucket,
				      const SubGcpCompleteRep * const rep,
				      Uint32 nodes)
{
  Uint32 old_cnt = bucket->m_gcp_complete_rep_count;
  
  ndbout_c("INVALID SUB_GCP_COMPLETE_REP");
  ndbout_c("gci: %d", rep->gci);
  ndbout_c("sender: %x", rep->senderRef);
  ndbout_c("count: %d", rep->gcp_complete_rep_count);
  ndbout_c("bucket count: %u", old_cnt);
  ndbout_c("nodes: %u", nodes);
  abort();
}

void
NdbEventBuffer::execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep * const rep)
{
  if (unlikely(m_active_op_count == 0))
  {
    return;
  }
  
  DBUG_ENTER_EVENT("NdbEventBuffer::execSUB_GCP_COMPLETE_REP");

  const Uint64 gci= rep->gci;
  const Uint32 cnt= rep->gcp_complete_rep_count;

  Gci_container *bucket = find_bucket(&m_active_gci, gci
#ifdef VM_TRACE
                                      , m_flush_gci
#endif
                                      );

  if (unlikely(bucket == 0))
  {
    /**
     * Already completed GCI...
     *   Possible in case of resend during NF handling
     */
#ifdef VM_TRACE
    ndbout << "bucket == 0, gci:" << gci
	   << " complete: " << m_complete_data << endl;
    for(Uint32 i = 0; i<m_active_gci.size(); i++)
    {
      ndbout << i << " - " << m_active_gci[i] << endl;
    }
#endif
    DBUG_VOID_RETURN_EVENT;
  }

  Uint32 old_cnt = bucket->m_gcp_complete_rep_count;
  if(unlikely(old_cnt == ~(Uint32)0))
  {
    old_cnt = m_system_nodes;
  }
  
  //assert(old_cnt >= cnt);
  if (unlikely(! (old_cnt >= cnt)))
  {
    crash_on_invalid_SUB_GCP_COMPLETE_REP(bucket, rep, m_system_nodes);
  }
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
	m_complete_data.m_data.append_list(&bucket->m_data, gci);
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
	       bucket-(Gci_container*)m_active_gci.getBase(), 
	       gci, m_latestGCI);
      bucket->m_state = Gci_container::GC_COMPLETE;
      bucket->m_gcp_complete_rep_count = 1; // Prevent from being reused
      m_latest_complete_GCI = gci;
    }
  }
  
  DBUG_VOID_RETURN_EVENT;
}

void
NdbEventBuffer::complete_outof_order_gcis()
{
  Uint64 start_gci = m_latestGCI + 1;
  Uint64 stop_gci = m_latest_complete_GCI;
  
  const Uint32 size = m_active_gci.size();
  Gci_container* array= (Gci_container*)m_active_gci.getBase();
  
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
      m_complete_data.m_data.append_list(&bucket->m_data, start_gci);
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
NdbEventBuffer::report_node_connected(Uint32 node_id)
{
  NdbEventOperation* op= m_ndb->getEventOperation(0);
  if (op == 0)
    return;

  DBUG_ENTER("NdbEventBuffer::report_node_connected");
  SubTableData data;
  LinearSectionPtr ptr[3];
  bzero(&data, sizeof(data));
  bzero(ptr, sizeof(ptr));

  data.tableId = ~0;
  data.requestInfo = 0;
  SubTableData::setOperation(data.requestInfo,
			     NdbDictionary::Event::_TE_ACTIVE);
  SubTableData::setReqNodeId(data.requestInfo, node_id);
  SubTableData::setNdbdNodeId(data.requestInfo, node_id);
  data.logType = SubTableData::LOG;
  data.gci = m_latestGCI + 1;
  /**
   * Insert this event for each operation
   */
  {
    // no need to lock()/unlock(), receive thread calls this
    NdbEventOperationImpl* impl = &op->m_impl;
    do if (!impl->m_node_bit_mask.isclear())
    {
      data.senderData = impl->m_oid;
      insertDataL(impl, &data, ptr);
    } while((impl = impl->m_next));
    for (impl = m_dropped_ev_op; impl; impl = impl->m_next)
      if (!impl->m_node_bit_mask.isclear())
      {
        data.senderData = impl->m_oid;
        insertDataL(impl, &data, ptr);
      }
  }
  DBUG_VOID_RETURN;
}

void
NdbEventBuffer::report_node_failure(Uint32 node_id)
{
  NdbEventOperation* op= m_ndb->getEventOperation(0);
  if (op == 0)
    return;

  DBUG_ENTER("NdbEventBuffer::report_node_failure");
  SubTableData data;
  LinearSectionPtr ptr[3];
  bzero(&data, sizeof(data));
  bzero(ptr, sizeof(ptr));

  data.tableId = ~0;
  data.requestInfo = 0;
  SubTableData::setOperation(data.requestInfo, 
			     NdbDictionary::Event::_TE_NODE_FAILURE);
  SubTableData::setReqNodeId(data.requestInfo, node_id);
  SubTableData::setNdbdNodeId(data.requestInfo, node_id);
  data.logType = SubTableData::LOG;
  data.gci = m_latestGCI + 1;
  /**
   * Insert this event for each operation
   */
  {
    // no need to lock()/unlock(), receive thread calls this
    NdbEventOperationImpl* impl = &op->m_impl;
    do if (!impl->m_node_bit_mask.isclear())
    {
      data.senderData = impl->m_oid;
      insertDataL(impl, &data, ptr);
    } while((impl = impl->m_next));
    for (impl = m_dropped_ev_op; impl; impl = impl->m_next)
      if (!impl->m_node_bit_mask.isclear())
      {
        data.senderData = impl->m_oid;
        insertDataL(impl, &data, ptr);
      }
  }
  DBUG_VOID_RETURN;
}

void
NdbEventBuffer::completeClusterFailed()
{
  NdbEventOperation* op= m_ndb->getEventOperation(0);
  if (op == 0)
    return;

  DBUG_ENTER("NdbEventBuffer::completeClusterFailed");
  SubTableData data;
  LinearSectionPtr ptr[3];
  bzero(&data, sizeof(data));
  bzero(ptr, sizeof(ptr));

  data.tableId = ~0;
  data.requestInfo = 0;
  SubTableData::setOperation(data.requestInfo,
			     NdbDictionary::Event::_TE_CLUSTER_FAILURE);
  data.logType = SubTableData::LOG;
  data.gci = m_latestGCI + 1;
  
#ifdef VM_TRACE
  m_flush_gci = 0;
#endif

  /**
   * Insert this event for each operation
   */
  {
    // no need to lock()/unlock(), receive thread calls this
    NdbEventOperationImpl* impl = &op->m_impl;
    do if (!impl->m_node_bit_mask.isclear())
    {
      data.senderData = impl->m_oid;
      insertDataL(impl, &data, ptr);
    } while((impl = impl->m_next));
    for (impl = m_dropped_ev_op; impl; impl = impl->m_next)
      if (!impl->m_node_bit_mask.isclear())
      {
        data.senderData = impl->m_oid;
        insertDataL(impl, &data, ptr);
      }
  }
  
  /**
   *  Release all GCI's with m_gci > gci
   */
  Uint32 i;
  Uint32 sz= m_active_gci.size();
  Uint64 gci= data.gci;
  Gci_container* bucket = 0;
  Gci_container* array = (Gci_container*)m_active_gci.getBase();
  for(i = 0; i < sz; i++)
  {
    Gci_container* tmp = array + i;
    if (tmp->m_gci > gci)
    {
      if(!tmp->m_data.is_empty())
      {
        free_list(tmp->m_data);
      }
      tmp->~Gci_container();
      bzero(tmp, sizeof(Gci_container));
    }
    else if (tmp->m_gcp_complete_rep_count)
    {
      if (tmp->m_gci == gci)
      {
        bucket= tmp;
        continue;
      }
      // we have found an old not-completed gci
      // something is wrong, assert in debug, but try so salvage
      // in release
      ndbout_c("out of order bucket detected at cluster disconnect, "
               "data.gci: %u.  tmp->m_gci: %u",
               (unsigned)data.gci, (unsigned)tmp->m_gci);
      assert(false);
      if(!tmp->m_data.is_empty())
      {
        free_list(tmp->m_data);
      }
      tmp->~Gci_container();
      bzero(tmp, sizeof(Gci_container));
    }
  }

  if (bucket == 0)
  {
    // no bucket to complete
    DBUG_VOID_RETURN;
  }

  const Uint32 cnt= bucket->m_gcp_complete_rep_count = 1; 
  bucket->m_gci = gci;
  bucket->m_gcp_complete_rep_count = cnt;
  
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
  DBUG_ENTER_EVENT("NdbEventBuffer::insertDataL");
  const Uint32 ri = sdata->requestInfo;
  const Uint32 operation = SubTableData::getOperation(ri);
  Uint64 gci= sdata->gci;
  const bool is_data_event = 
    operation < NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT;

  if (!is_data_event)
  {
    switch (operation)
    {
    case NdbDictionary::Event::_TE_NODE_FAILURE:
      op->m_node_bit_mask.clear(SubTableData::getNdbdNodeId(ri));
      break;
    case NdbDictionary::Event::_TE_ACTIVE:
      op->m_node_bit_mask.set(SubTableData::getNdbdNodeId(ri));
      // internal event, do not relay to user
      DBUG_RETURN_EVENT(0);
      break;
    case NdbDictionary::Event::_TE_CLUSTER_FAILURE:
      op->m_node_bit_mask.clear();
      DBUG_ASSERT(op->m_ref_count > 0);
      op->m_ref_count--;
      DBUG_PRINT("info", ("m_ref_count: %u for op: %p", op->m_ref_count, op));
      break;
    case NdbDictionary::Event::_TE_STOP:
      op->m_node_bit_mask.clear(SubTableData::getNdbdNodeId(ri));
      if (op->m_node_bit_mask.isclear())
      {
        DBUG_ASSERT(op->m_ref_count > 0);
        op->m_ref_count--;
        DBUG_PRINT("info", ("m_ref_count: %u for op: %p", op->m_ref_count, op));
      }
      break;
    default:
      break;
    }
  }
  
  if ( likely((Uint32)op->mi_type & (1 << operation)))
  {
    Gci_container* bucket= find_bucket(&m_active_gci, gci
#ifdef VM_TRACE
                                       , m_flush_gci
#endif
                                       );
    
    DBUG_PRINT_EVENT("info", ("data insertion in eventId %d", op->m_eventId));
    DBUG_PRINT_EVENT("info", ("gci=%d tab=%d op=%d node=%d",
                              sdata->gci, sdata->tableId, 
			      SubTableData::getOperation(sdata->requestInfo),
                              SubTableData::getReqNodeId(sdata->requestInfo)));

    if (unlikely(bucket == 0))
    {
      /**
       * Already completed GCI...
       *   Possible in case of resend during NF handling
       */
      DBUG_RETURN_EVENT(0);
    }
    
    const bool is_blob_event = (op->theMainOp != NULL);
    const bool use_hash =  op->m_mergeEvents && is_data_event;

    if (! is_data_event && is_blob_event)
    {
      // currently subscribed to but not used
      DBUG_PRINT_EVENT("info", ("ignore non-data event on blob table"));
      DBUG_RETURN_EVENT(0);
    }
    
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
        DBUG_RETURN_EVENT(-1);
      }
      if (unlikely(copy_data(sdata, ptr, data, NULL)))
      {
        op->m_has_error = 3;
        DBUG_RETURN_EVENT(-1);
      }
      data->m_event_op = op;
      if (! is_blob_event || ! is_data_event)
      {
        bucket->m_data.append_data(data);
      }
      else
      {
        // find or create main event for this blob event
        EventBufData_hash::Pos main_hpos;
        int ret = get_main_data(bucket, main_hpos, data);
        if (ret == -1)
        {
          op->m_has_error = 4;
          DBUG_RETURN_EVENT(-1);
        }
        EventBufData* main_data = main_hpos.data;
        if (ret != 0) // main event was created
        {
          main_data->m_event_op = op->theMainOp;
          bucket->m_data.append_data(main_data);
          if (use_hash)
          {
            main_data->m_pkhash = main_hpos.pkhash;
            bucket->m_data_hash.append(main_hpos, main_data);
          }
        }
        // link blob event under main event
        add_blob_data(bucket, main_data, data);
      }
      if (use_hash)
      {
        data->m_pkhash = hpos.pkhash;
        bucket->m_data_hash.append(hpos, data);
      }
#ifdef VM_TRACE
      op->m_data_count++;
#endif
    }
    else
    {
      // event with same op, PK found, merge into old buffer
      if (unlikely(merge_data(sdata, ptr, data, & bucket->m_data.m_sz)))
      {
        op->m_has_error = 3;
        DBUG_RETURN_EVENT(-1);
      }
      // merge is on so we do not report blob part events
      if (! is_blob_event) {
        // report actual operation and the composite
        // there is no way to "fix" the flags for a composite op
        // since the flags represent multiple ops on multiple PKs
        // XXX fix by doing merge at end of epoch (extra mem cost)
        {
          EventBufData_list::Gci_op g = { op, (1 << operation) };
          bucket->m_data.add_gci_op(g);
        }
        {
          EventBufData_list::Gci_op 
	    g = { op, 
		  (1 << SubTableData::getOperation(data->sdata->requestInfo))};
          bucket->m_data.add_gci_op(g);
        }
      }
    }
#ifdef NDB_EVENT_VERIFY_SIZE
    verify_size(bucket->m_data);
#endif
    DBUG_RETURN_EVENT(0);
  }
  
#ifdef VM_TRACE
  if ((Uint32)op->m_eventImpl->mi_type & (1 << operation))
  {
    DBUG_PRINT_EVENT("info",("Data arrived before ready eventId", op->m_eventId));
    DBUG_RETURN_EVENT(0);
  }
  else {
    DBUG_PRINT_EVENT("info",("skipped"));
    DBUG_RETURN_EVENT(0);
  }
#else
  DBUG_RETURN_EVENT(0);
#endif
}

// allocate EventBufData
EventBufData*
NdbEventBuffer::alloc_data()
{
  DBUG_ENTER_EVENT("alloc_data");
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
      DBUG_RETURN_EVENT(0); // TODO handle this, overrun, or, skip?
    }
  }

  // remove data from free list
  if (data->m_next_blob == 0)
    m_free_data = data->m_next;
  else {
    EventBufData* data2 = data->m_next_blob;
    if (data2->m_next == 0) {
      data->m_next_blob = data2->m_next_blob;
      data = data2;
    } else {
      EventBufData* data3 = data2->m_next;
      data2->m_next = data3->m_next;
      data = data3;
    }
  }
  data->m_next = 0;
  data->m_next_blob = 0;
#ifdef VM_TRACE
  m_free_data_count--;
  assert(m_free_data_sz >= data->sz);
#endif
  m_free_data_sz -= data->sz;
  DBUG_RETURN_EVENT(data);
}

// allocate initial or bigger memory area in EventBufData
// takes sizes from given ptr and sets up data->ptr
int
NdbEventBuffer::alloc_mem(EventBufData* data,
                          LinearSectionPtr ptr[3],
                          Uint32 * change_sz)
{
  DBUG_ENTER("NdbEventBuffer::alloc_mem");
  DBUG_PRINT("info", ("ptr sz %u + %u + %u", ptr[0].sz, ptr[1].sz, ptr[2].sz));
  const Uint32 min_alloc_size = 128;

  Uint32 sz4 = (sizeof(SubTableData) + 3) >> 2;
  Uint32 alloc_size = (sz4 + ptr[0].sz + ptr[1].sz + ptr[2].sz) << 2;
  if (alloc_size < min_alloc_size)
    alloc_size = min_alloc_size;

  if (data->sz < alloc_size)
  {
    Uint32 add_sz = alloc_size - data->sz;

    NdbMem_Free((char*)data->memory);
    assert(m_total_alloc >= data->sz);
    m_total_alloc -= data->sz;
    data->memory = 0;
    data->sz = 0;

    data->memory = (Uint32*)NdbMem_Allocate(alloc_size);
    if (data->memory == 0)
      DBUG_RETURN(-1);
    data->sz = alloc_size;
    m_total_alloc += data->sz;

    if (change_sz != NULL)
      *change_sz += add_sz;
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

  DBUG_RETURN(0);
}

void
NdbEventBuffer::dealloc_mem(EventBufData* data,
                            Uint32 * change_sz)
{
  NdbMem_Free((char*)data->memory);
  assert(m_total_alloc >= data->sz);
  m_total_alloc -= data->sz;
  if (change_sz != NULL) {
    assert(*change_sz >= data->sz);
    *change_sz -= data->sz;
  }
  data->memory = 0;
  data->sz = 0;
}

int 
NdbEventBuffer::copy_data(const SubTableData * const sdata,
                          LinearSectionPtr ptr[3],
                          EventBufData* data,
                          Uint32 * change_sz)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::copy_data");

  if (alloc_mem(data, ptr, change_sz) != 0)
    DBUG_RETURN_EVENT(-1);
  memcpy(data->sdata, sdata, sizeof(SubTableData));
  int i;
  for (i = 0; i <= 2; i++)
    memcpy(data->ptr[i].p, ptr[i].p, ptr[i].sz << 2);
  DBUG_RETURN_EVENT(0);
}

static struct Ev_t {
  enum {
    enum_INS = NdbDictionary::Event::_TE_INSERT,
    enum_DEL = NdbDictionary::Event::_TE_DELETE,
    enum_UPD = NdbDictionary::Event::_TE_UPDATE,
    enum_NUL = NdbDictionary::Event::_TE_NUL,
    enum_IDM = 254,     // idempotent op possibly allowed on NF
    enum_ERR = 255      // always impossible
  };
  int t1, t2, t3;
} ev_t[] = {
  { Ev_t::enum_INS, Ev_t::enum_INS, Ev_t::enum_IDM },
  { Ev_t::enum_INS, Ev_t::enum_DEL, Ev_t::enum_NUL }, //ok
  { Ev_t::enum_INS, Ev_t::enum_UPD, Ev_t::enum_INS }, //ok
  { Ev_t::enum_DEL, Ev_t::enum_INS, Ev_t::enum_UPD }, //ok
  { Ev_t::enum_DEL, Ev_t::enum_DEL, Ev_t::enum_IDM },
  { Ev_t::enum_DEL, Ev_t::enum_UPD, Ev_t::enum_ERR },
  { Ev_t::enum_UPD, Ev_t::enum_INS, Ev_t::enum_ERR },
  { Ev_t::enum_UPD, Ev_t::enum_DEL, Ev_t::enum_DEL }, //ok
  { Ev_t::enum_UPD, Ev_t::enum_UPD, Ev_t::enum_UPD }  //ok
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
      p1[j1 + k] = p2[j2 + k];
  }
  j1 += n;
  j2 += n;
}

int 
NdbEventBuffer::merge_data(const SubTableData * const sdata,
                           LinearSectionPtr ptr2[3],
                           EventBufData* data,
                           Uint32 * change_sz)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::merge_data");

  Uint32 nkey = data->m_event_op->m_eventImpl->m_tableImpl->m_noOfKeys;

  int t1 = SubTableData::getOperation(data->sdata->requestInfo);
  int t2 = SubTableData::getOperation(sdata->requestInfo);
  if (t1 == Ev_t::enum_NUL)
    DBUG_RETURN_EVENT(copy_data(sdata, ptr2, data, change_sz));

  Ev_t* tp = 0;
  int i;
  for (i = 0; i < sizeof(ev_t)/sizeof(ev_t[0]); i++) {
    if (ev_t[i].t1 == t1 && ev_t[i].t2 == t2) {
      tp = &ev_t[i];
      break;
    }
  }
  assert(tp != 0 && tp->t3 != Ev_t::enum_ERR);

  if (tp->t3 == Ev_t::enum_IDM) {
    LinearSectionPtr (&ptr1)[3] = data->ptr;

    /*
     * TODO
     * - can get data in INS ptr2[2] which is supposed to be empty
     * - can get extra data in DEL ptr2[2]
     * - why does DBUG_PRINT not work in this file ???
     *
     * replication + bug#19872 can ignore this since merge is on
     * only for tables with explicit PK and before data is not used
     */
    const int maxsec = 1; // ignore section 2

    int i;
    for (i = 0; i <= maxsec; i++) {
      if (ptr1[i].sz != ptr2[i].sz ||
          memcmp(ptr1[i].p, ptr2[i].p, ptr1[i].sz << 2) != 0) {
        DBUG_PRINT("info", ("idempotent op %d*%d data differs in sec %d",
                             tp->t1, tp->t2, i));
        assert(false);
        DBUG_RETURN_EVENT(-1);
      }
    }
    DBUG_PRINT("info", ("idempotent op %d*%d data ok", tp->t1, tp->t2));
    DBUG_RETURN_EVENT(0);
  }

  // TODO: use old data items, avoid malloc/free on each merge

  // save old data
  EventBufData olddata = *data;
  data->memory = 0;
  data->sz = 0;

  // compose ptr1 o ptr2 = ptr
  LinearSectionPtr (&ptr1)[3] = olddata.ptr;
  LinearSectionPtr (&ptr)[3] = data->ptr;

  // loop twice where first loop only sets sizes
  int loop;
  int result = 0;
  for (loop = 0; loop <= 1; loop++)
  {
    if (loop == 1)
    {
      if (alloc_mem(data, ptr, change_sz) != 0)
      {
        result = -1;
        goto end;
      }
      *data->sdata = *sdata;
      SubTableData::setOperation(data->sdata->requestInfo, tp->t3);
    }

    ptr[0].sz = ptr[1].sz = ptr[2].sz = 0;

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
    if (tp->t3 != Ev_t::enum_DEL)
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
    if (tp->t3 != Ev_t::enum_INS)
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

end:
  dealloc_mem(&olddata, change_sz);
  DBUG_RETURN_EVENT(result);
}
 
/*
 * Given blob part event, find main table event on inline part.  It
 * should exist (force in TUP) but may arrive later.  If so, create
 * NUL event on main table.  The real event replaces it later.
 */

// write attribute headers for concatened PK
static void
split_concatenated_pk(const NdbTableImpl* t, Uint32* ah_buffer,
                      const Uint32* pk_buffer, Uint32 pk_sz)
{
  Uint32 sz = 0; // words parsed so far
  Uint32 n;  // pk attr count
  Uint32 i;
  for (i = n = 0; i < t->m_columns.size() && n < t->m_noOfKeys; i++)
  {
    const NdbColumnImpl* c = t->getColumn(i);
    assert(c != NULL);
    if (! c->m_pk)
      continue;

    assert(sz < pk_sz);
    Uint32 bytesize = c->m_attrSize * c->m_arraySize;
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(c->m_type, &pk_buffer[sz], bytesize,
                                         lb, len);
    assert(ok);

    AttributeHeader ah(i, lb + len);
    ah_buffer[n++] = ah.m_value;
    sz += ah.getDataSize();
  }
  assert(n == t->m_noOfKeys && sz <= pk_sz);
}

int
NdbEventBuffer::get_main_data(Gci_container* bucket,
                              EventBufData_hash::Pos& hpos,
                              EventBufData* blob_data)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::get_main_data");

  NdbEventOperationImpl* main_op = blob_data->m_event_op->theMainOp;
  assert(main_op != NULL);
  const NdbTableImpl* mainTable = main_op->m_eventImpl->m_tableImpl;

  // create LinearSectionPtr for main table key
  LinearSectionPtr ptr[3];
  Uint32 ah_buffer[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
  ptr[0].sz = mainTable->m_noOfKeys;
  ptr[0].p = ah_buffer;
  ptr[1].sz = AttributeHeader(blob_data->ptr[0].p[0]).getDataSize();
  ptr[1].p = blob_data->ptr[1].p;
  ptr[2].sz = 0;
  ptr[2].p = 0;
  split_concatenated_pk(mainTable, ptr[0].p, ptr[1].p, ptr[1].sz);

  DBUG_DUMP_EVENT("ah", (char*)ptr[0].p, ptr[0].sz << 2);
  DBUG_DUMP_EVENT("pk", (char*)ptr[1].p, ptr[1].sz << 2);

  // search for main event buffer
  bucket->m_data_hash.search(hpos, main_op, ptr);
  if (hpos.data != NULL)
    DBUG_RETURN_EVENT(0);

  // not found, create a place-holder
  EventBufData* main_data = alloc_data();
  if (main_data == NULL)
    DBUG_RETURN_EVENT(-1);
  SubTableData sdata = *blob_data->sdata;
  sdata.tableId = main_op->m_eventImpl->m_tableImpl->m_id;
  SubTableData::setOperation(sdata.requestInfo, NdbDictionary::Event::_TE_NUL);
  if (copy_data(&sdata, ptr, main_data, NULL) != 0)
    DBUG_RETURN_EVENT(-1);
  hpos.data = main_data;

  DBUG_RETURN_EVENT(1);
}

void
NdbEventBuffer::add_blob_data(Gci_container* bucket,
                              EventBufData* main_data,
                              EventBufData* blob_data)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::add_blob_data");
  DBUG_PRINT_EVENT("info", ("main_data=%p blob_data=%p", main_data, blob_data));
  EventBufData* head;
  head = main_data->m_next_blob;
  while (head != NULL)
  {
    if (head->m_event_op == blob_data->m_event_op)
      break;
    head = head->m_next_blob;
  }
  if (head == NULL)
  {
    head = blob_data;
    head->m_next_blob = main_data->m_next_blob;
    main_data->m_next_blob = head;
  }
  else
  {
    blob_data->m_next = head->m_next;
    head->m_next = blob_data;
  }
  // adjust data list size
  bucket->m_data.m_count += 1;
  bucket->m_data.m_sz += blob_data->sz;
  DBUG_VOID_RETURN_EVENT;
}

NdbEventOperationImpl *
NdbEventBuffer::move_data()
{
  // handle received data
  if (!m_complete_data.m_data.is_empty())
  {
    // move this list to last in m_available_data
    m_available_data.append_list(&m_complete_data.m_data, 0);

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
    DBUG_ENTER_EVENT("NdbEventBuffer::move_data");
#ifdef VM_TRACE
    DBUG_PRINT_EVENT("exit",("m_available_data_count %u", m_available_data.m_count));
#endif
    DBUG_RETURN_EVENT(m_available_data.m_head->m_event_op);
  }
  return 0;
}

void
NdbEventBuffer::free_list(EventBufData_list &list)
{
#ifdef NDB_EVENT_VERIFY_SIZE
  verify_size(list);
#endif
  // return list to m_free_data
  list.m_tail->m_next= m_free_data;
  m_free_data= list.m_head;
#ifdef VM_TRACE
  m_free_data_count+= list.m_count;
#endif
  m_free_data_sz+= list.m_sz;

  list.m_head = list.m_tail = NULL;
  list.m_count = list.m_sz = 0;
}

void EventBufData_list::append_list(EventBufData_list *list, Uint64 gci)
{
#ifdef NDB_EVENT_VERIFY_SIZE
  NdbEventBuffer::verify_size(*list);
#endif
  move_gci_ops(list, gci);

  if (m_tail)
    m_tail->m_next= list->m_head;
  else
    m_head= list->m_head;
  m_tail= list->m_tail;
  m_count+= list->m_count;
  m_sz+= list->m_sz;
}

void
EventBufData_list::add_gci_op(Gci_op g)
{
  DBUG_ENTER_EVENT("EventBufData_list::add_gci_op");
  DBUG_PRINT_EVENT("info", ("p.op: %p  g.event_types: %x", g.op, g.event_types));
  assert(g.op != NULL && g.op->theMainOp == NULL); // as in nextEvent
  Uint32 i;
  for (i = 0; i < m_gci_op_count; i++) {
    if (m_gci_op_list[i].op == g.op)
      break;
  }
  if (i < m_gci_op_count) {
    m_gci_op_list[i].event_types |= g.event_types;
  } else {
    if (m_gci_op_count == m_gci_op_alloc) {
      Uint32 n = 1 + 2 * m_gci_op_alloc;
      Gci_op* old_list = m_gci_op_list;
      m_gci_op_list = new Gci_op [n];
      if (m_gci_op_alloc != 0) {
        Uint32 bytes = m_gci_op_alloc * sizeof(Gci_op);
        memcpy(m_gci_op_list, old_list, bytes);
        DBUG_PRINT_EVENT("info", ("this: %p  delete m_gci_op_list: %p",
                                  this, old_list));
        delete [] old_list;
      }
      DBUG_PRINT_EVENT("info", ("this: %p  new m_gci_op_list: %p",
                                this, m_gci_op_list));
      m_gci_op_alloc = n;
    }
    assert(m_gci_op_count < m_gci_op_alloc);
#ifndef DBUG_OFF
    i = m_gci_op_count;
#endif
    g.op->m_ref_count++;
    DBUG_PRINT("info", ("m_ref_count: %u for op: %p", g.op->m_ref_count, g.op));
    m_gci_op_list[m_gci_op_count++] = g;
  }
  DBUG_PRINT_EVENT("exit", ("m_gci_op_list[%u].event_types: %x", i, m_gci_op_list[i].event_types));
  DBUG_VOID_RETURN_EVENT;
}

void
EventBufData_list::move_gci_ops(EventBufData_list *list, Uint64 gci)
{
  DBUG_ENTER_EVENT("EventBufData_list::move_gci_ops");
  DBUG_PRINT_EVENT("info", ("this: %p  list: %p  gci: %llu",
                            this, list, gci));
  assert(!m_is_not_multi_list);
  if (!list->m_is_not_multi_list)
  {
    assert(gci == 0);
    if (m_gci_ops_list_tail)
      m_gci_ops_list_tail->m_next = list->m_gci_ops_list;
    else
    {
      m_gci_ops_list =  list->m_gci_ops_list;
    }
    m_gci_ops_list_tail = list->m_gci_ops_list_tail;
    goto end;
  }
  {
    Gci_ops *new_gci_ops = new Gci_ops;
    DBUG_PRINT_EVENT("info", ("this: %p  m_gci_op_list: %p",
                        new_gci_ops, list->m_gci_op_list));
    if (m_gci_ops_list_tail)
      m_gci_ops_list_tail->m_next = new_gci_ops;
    else
    {
      assert(m_gci_ops_list == 0);
      m_gci_ops_list = new_gci_ops;
    }
    m_gci_ops_list_tail = new_gci_ops;
    
    new_gci_ops->m_gci_op_list = list->m_gci_op_list;
    new_gci_ops->m_gci_op_count = list->m_gci_op_count;
    new_gci_ops->m_gci = gci;
    new_gci_ops->m_next = 0;
  }
end:
  list->m_gci_op_list = 0;
  list->m_gci_ops_list_tail = 0;
  list->m_gci_op_alloc = 0;
  DBUG_VOID_RETURN_EVENT;
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
  getEventOperationImpl(tOp)->m_ref_count = 1;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p",
                      getEventOperationImpl(tOp)->m_ref_count, getEventOperationImpl(tOp)));
  DBUG_RETURN(tOp);
}

NdbEventOperationImpl*
NdbEventBuffer::createEventOperation(NdbEventImpl& evnt,
				     NdbError &theError)
{
  DBUG_ENTER("NdbEventBuffer::createEventOperation [evnt]");
  NdbEventOperationImpl* tOp= new NdbEventOperationImpl(m_ndb, evnt);
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
  DBUG_ENTER("NdbEventBuffer::dropEventOperation");
  NdbEventOperationImpl* op= getEventOperationImpl(tOp);

  op->stop();
  // stop blob event ops
  if (op->theMainOp == NULL)
  {
    NdbEventOperationImpl* tBlobOp = op->theBlobOpList;
    while (tBlobOp != NULL)
    {
      tBlobOp->stop();
      tBlobOp = tBlobOp->m_next;
    }

    // release blob handles now, further access is user error
    while (op->theBlobList != NULL)
    {
      NdbBlob* tBlob = op->theBlobList;
      op->theBlobList = tBlob->theNext;
      m_ndb->releaseNdbBlob(tBlob);
    }
  }

  DBUG_ASSERT(op->m_ref_count > 0);
  op->m_ref_count--;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p", op->m_ref_count, op));
  if (op->m_ref_count == 0)
  {
    DBUG_PRINT("info", ("deleting op: %p", op));
    DBUG_ASSERT(op->m_node_bit_mask.isclear());
    delete op->m_facade;
  }
  else
  {
    op->m_next= m_dropped_ev_op;
    op->m_prev= 0;
    if (m_dropped_ev_op)
      m_dropped_ev_op->m_prev= op;
    m_dropped_ev_op= op;
  }
  DBUG_VOID_RETURN;
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

#ifdef VM_TRACE
void
NdbEventBuffer::verify_size(const EventBufData* data, Uint32 count, Uint32 sz)
{
  Uint32 tmp_count = 0;
  Uint32 tmp_sz = 0;
  while (data != 0) {
    Uint32 full_count, full_sz;
    data->get_full_size(full_count, full_sz);
    tmp_count += full_count;
    tmp_sz += full_sz;
    data = data->m_next;
  }
  assert(tmp_count == count);
  assert(tmp_sz == sz);
}
void
NdbEventBuffer::verify_size(const EventBufData_list & list)
{
  verify_size(list.m_head, list.m_count, list.m_sz);
}
#endif

// hash table routines

// could optimize the all-fixed case
Uint32
EventBufData_hash::getpkhash(NdbEventOperationImpl* op, LinearSectionPtr ptr[3])
{
  DBUG_ENTER_EVENT("EventBufData_hash::getpkhash");
  DBUG_DUMP_EVENT("ah", (char*)ptr[0].p, ptr[0].sz << 2);
  DBUG_DUMP_EVENT("pk", (char*)ptr[1].p, ptr[1].sz << 2);

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
    dptr += ((bytesize + 3) / 4) * 4;
  }
  DBUG_PRINT_EVENT("info", ("hash result=%08x", nr1));
  DBUG_RETURN_EVENT(nr1);
}

bool
EventBufData_hash::getpkequal(NdbEventOperationImpl* op, LinearSectionPtr ptr1[3], LinearSectionPtr ptr2[3])
{
  DBUG_ENTER_EVENT("EventBufData_hash::getpkequal");
  DBUG_DUMP_EVENT("ah1", (char*)ptr1[0].p, ptr1[0].sz << 2);
  DBUG_DUMP_EVENT("pk1", (char*)ptr1[1].p, ptr1[1].sz << 2);
  DBUG_DUMP_EVENT("ah2", (char*)ptr2[0].p, ptr2[0].sz << 2);
  DBUG_DUMP_EVENT("pk2", (char*)ptr2[1].p, ptr2[1].sz << 2);

  const NdbTableImpl* tab = op->m_eventImpl->m_tableImpl;

  Uint32 nkey = tab->m_noOfKeys;
  assert(nkey != 0 && nkey <= ptr1[0].sz && nkey <= ptr2[0].sz);
  const Uint32* hptr1 = ptr1[0].p;
  const Uint32* hptr2 = ptr2[0].p;
  const uchar* dptr1 = (uchar*)ptr1[1].p;
  const uchar* dptr2 = (uchar*)ptr2[1].p;

  bool equal = true;

  while (nkey-- != 0)
  {
    AttributeHeader ah1(*hptr1++);
    AttributeHeader ah2(*hptr2++);
    // sizes can differ on update of varchar endspace
    Uint32 bytesize1 = ah1.getByteSize();
    Uint32 bytesize2 = ah2.getByteSize();
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
    {
      equal = false;
      break;
    }
    dptr1 += ((bytesize1 + 3) / 4) * 4;
    dptr2 += ((bytesize2 + 3) / 4) * 4;
  }

  DBUG_PRINT_EVENT("info", ("equal=%s", equal ? "true" : "false"));
  DBUG_RETURN_EVENT(equal);
}

void
EventBufData_hash::search(Pos& hpos, NdbEventOperationImpl* op, LinearSectionPtr ptr[3])
{
  DBUG_ENTER_EVENT("EventBufData_hash::search");
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
  DBUG_PRINT_EVENT("info", ("search result=%p", data));
  DBUG_VOID_RETURN_EVENT;
}

template class Vector<Gci_container_pod>;
template class Vector<NdbEventBuffer::EventBufData_chunk*>;
