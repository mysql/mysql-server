/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>
#include <kernel_types.h>

#include "API.hpp"
#include <NdbOut.hpp>

#include <signaldata/CreateEvnt.hpp>
#include <signaldata/SumaImpl.hpp>
#include <SimpleProperties.hpp>
#include <Bitmask.hpp>
#include <AttributeHeader.hpp>
#include <AttributeList.hpp>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <UtilBuffer.hpp>
#include <portlib/NdbMem.h>
#include <signaldata/AlterTable.hpp>
#include "ndb_internal.hpp"

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#define TOTAL_BUCKETS_INIT (1U << 15)
static Gci_container_pod g_empty_gci_container;

static const MonotonicEpoch NULL_EPOCH(Uint32(0), Uint64(0));
static const MonotonicEpoch MAX_EPOCH(~Uint32(0),~Uint64(0));

#if defined(VM_TRACE) && defined(NOT_USED)
static void
print_std(const SubTableData * sdata, LinearSectionPtr ptr[3])
{
  printf("addr=%p gci{hi/lo}hi=%u/%u op=%d\n", (void*)sdata,
         sdata->gci_hi, sdata->gci_lo,
	 SubTableData::getOperation(sdata->requestInfo));
  for (int i = 0; i <= 2; i++) {
    printf("sec=%d addr=%p sz=%d\n", i, (void*)ptr[i].p, ptr[i].sz);
    for (int j = 0; (uint) j < ptr[i].sz; j++)
      printf("%08x ", ptr[i].p[j]);
    printf("\n");
  }
}
#endif

// EventBufData

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
  m_oid(~(Uint32)0),
  m_stop_gci(),
  m_allow_empty_update(false)
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
  m_oid(~(Uint32)0),
  m_stop_gci(),
  m_allow_empty_update(false)
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
  theBlobVersion = 0;

  m_data_item= NULL;
  m_eventImpl = NULL;

  m_custom_data= 0;
  m_has_error= 1;

  // we should lookup id in Dictionary, TODO
  // also make sure we only have one listener on each event

  m_eventImpl = &evnt;

  m_eventId = m_eventImpl->m_eventId;

  m_oid= m_ndb->theImpl->mapRecipient(this);

  m_state= EO_CREATED;

#ifdef ndb_event_stores_merge_events_flag
  m_mergeEvents = m_eventImpl->m_mergeEvents;
#else
  m_mergeEvents = false;
#endif
  m_ref_count = 0;
  DBUG_PRINT("info", ("m_ref_count = 0 for op: 0x%lx", (long) this));

  m_has_error= 0;

  DBUG_PRINT("exit",("this: 0x%lx  oid: %u", (long) this, m_oid));
  DBUG_VOID_RETURN;
}

NdbEventOperationImpl::~NdbEventOperationImpl()
{
  DBUG_ENTER("NdbEventOperationImpl::~NdbEventOperationImpl");
  m_magic_number= 0;

#ifndef NDEBUG
  m_state = (NdbEventOperation::State)0xDead;
#endif

  if (m_oid == ~(Uint32)0)
    DBUG_VOID_RETURN;

  stop();
  
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

  m_ndb->theImpl->unmapRecipient(m_oid, this);
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
        m_ndb->theEventBuffer->createEventOperationImpl(*blobEvnt, m_error);
      if (tBlobOp == NULL)
        DBUG_RETURN(NULL);

      // pointer to main table op
      tBlobOp->theMainOp = this;
      tBlobOp->m_mergeEvents = m_mergeEvents;
      tBlobOp->theBlobVersion = tAttrInfo->m_blobVersion;

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

Uint32
NdbEventOperationImpl::get_blob_part_no(bool hasDist)
{
  assert(theBlobVersion == 1 || theBlobVersion == 2);
  assert(theMainOp != NULL);
  const NdbTableImpl* mainTable = theMainOp->m_eventImpl->m_tableImpl;
  assert(m_data_item != NULL);
  LinearSectionPtr (&ptr)[3] = m_data_item->ptr;

  uint pos = 0; // PK and possibly DIST to skip

  if (unlikely(theBlobVersion == 1)) {
    pos += AttributeHeader(ptr[0].p[0]).getDataSize();
    assert(hasDist);
    pos += AttributeHeader(ptr[0].p[1]).getDataSize();
  } else {
    uint n = mainTable->m_noOfKeys;
    uint i;
    for (i = 0; i < n; i++) {
      pos += AttributeHeader(ptr[0].p[i]).getDataSize();
    }
    if (hasDist)
      pos += AttributeHeader(ptr[0].p[n]).getDataSize();
  }

  assert(pos < ptr[1].sz);
  Uint32 no = ptr[1].p[pos];
  return no;
}

int
NdbEventOperationImpl::readBlobParts(char* buf, NdbBlob* blob,
                                     Uint32 part, Uint32 count, Uint16* lenLoc)
{
  DBUG_ENTER_EVENT("NdbEventOperationImpl::readBlobParts");
  DBUG_PRINT_EVENT("info", ("part=%u count=%u post/pre=%d",
                      part, count, blob->theEventBlobVersion));

  NdbEventOperationImpl* blob_op = blob->theBlobEventOp;
  const bool hasDist = (blob->theStripeSize != 0);

  DBUG_PRINT_EVENT("info", ("m_data_item=%p", m_data_item));
  assert(m_data_item != NULL);

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
  Uint32 noutside = 0;
  EventBufData* data = head;
  // XXX optimize using part no ordering
  while (data != NULL)
  {
    /*
     * Hack part no directly out of buffer since it is not returned
     * in pre data (PK buglet).  For part data use receive_event().
     * This means extra copy. XXX fix
     */
    blob_op->m_data_item = data;
    int r = blob_op->receive_event();
    require(r > 0);
    // XXX should be: no = blob->theBlobEventPartValue
    Uint32 no = blob_op->get_blob_part_no(hasDist);

    DBUG_PRINT_EVENT("info", ("part_data=%p part no=%u part", data, no));

    if (part <= no && no < part + count)
    {
      DBUG_PRINT_EVENT("info", ("part within read range"));

      const char* src = blob->theBlobEventDataBuf.data;
      Uint32 sz = 0;
      if (blob->theFixedDataFlag) {
        sz = blob->thePartSize;
      } else {
        const uchar* p = (const uchar*)blob->theBlobEventDataBuf.data;
        sz = p[0] + (p[1] << 8);
        src += 2;
      }
      memcpy(buf + (no - part) * sz, src, sz);
      nparts++;
      if (lenLoc != NULL) {
        assert(count == 1);
        *lenLoc = sz;
      } else {
        assert(sz == blob->thePartSize);
      }
    }
    else
    {
      DBUG_PRINT_EVENT("info", ("part outside read range"));
      noutside++;
    }
    data = data->m_next;
  }
  if (unlikely(nparts != count))
  {
    ndbout_c("nparts: %u count: %u noutside: %u", nparts, count, noutside);
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

  bool schemaTrans = false;
  if (m_ndb->theEventBuffer->m_prevent_nodegroup_change)
  {
    /*
     * Since total count of sub data streams (Suma buckets)
     * are initially set when the first subscription are setup,
     * a dummy schema transaction are used to stop add or drop
     * node to occur for first subscription.  Otherwise count may
     * change before we are in a state to detect that correctly.
     * This should not be needed since the handling of
     * SUB_GCP_COMPLETE_REP in recevier thread(s) should handle
     * this, but until sure this behaviour is kept.
     */
    int res = NdbDictionaryImpl::getImpl(* myDict).beginSchemaTrans(false);
    if (res != 0)
    {
      switch(myDict->getNdbError().code){
      case 711:
      case 763:
        // ignore;
        break;
      default:
        m_error.code= myDict->getNdbError().code;
        DBUG_RETURN(-1);
      }
    }
    else
    {
      schemaTrans = true;
    }
  }

  if (theFirstPkAttrs[0] == NULL && 
      theFirstDataAttrs[0] == NULL) { // defaults to get all
  }

  m_magic_number= NDB_EVENT_OP_MAGIC_NUMBER;
  m_state= EO_EXECUTING;
  mi_type= m_eventImpl->mi_type;
  // add kernel reference
  // removed on TE_STOP, TE_CLUSTER_FAILURE, or error below
  m_ref_count++;
  m_stop_gci= MAX_EPOCH;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p", m_ref_count, this));
  int r= NdbDictionaryImpl::getImpl(*myDict).executeSubscribeEvent(*this);
  if (r == 0) 
  {
    m_ndb->theEventBuffer->m_prevent_nodegroup_change = false;
    if (schemaTrans)
    {
      schemaTrans = false;
      myDict->endSchemaTrans(1);
    }

    if (theMainOp == NULL) {
      DBUG_PRINT("info", ("execute blob ops"));
      NdbEventOperationImpl* blob_op = theBlobOpList;
      while (blob_op != NULL) {
        r = blob_op->execute_nolock();
        if (r != 0) {
          // since main op is running and possibly some blob ops as well
          // we can't just reset the main op.  Instead return with error,
          // main op (and blob ops) will be cleaned up when user calls
          // dropEventOperation
          m_error.code= myDict->getNdbError().code;
          DBUG_RETURN(r);
        }
        blob_op = blob_op->m_next;
      }
    }
    if (r == 0)
    {
      DBUG_RETURN(0);
    }
  }
  // Error
  // remove kernel reference
  // added above
  m_ref_count--;
  m_stop_gci = NULL_EPOCH;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p", m_ref_count, this));
  m_state= EO_ERROR;
  mi_type= 0;
  m_magic_number= 0;
  m_error.code= myDict->getNdbError().code;

  if (schemaTrans)
  {
    schemaTrans = false;
    myDict->endSchemaTrans(1);
  }

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
  /**
   * Note, that there is a deadlock risk both in the call to
   * stopSubscribeEvent and the NdbMutex_Lock below, both using
   * the trp_client lock, which could already be taken if this
   * function is called from NdbEventOperationImpl destructor
   * invoked in deleteUsedEventOperations via nextEvents*() and
   * pollEvents*().
   */
  Uint64 stop_gci = 0;
  const int r= NdbDictionaryImpl::getImpl(*myDict).stopSubscribeEvent(*this,stop_gci);
  /**
   * remove_op decrements the active event operation counter.
   * This enables later cleanup of obsolete receiver threads data.
   * To guarantee that this is only called once per event
   * operation unsubscription it is called here in client thread.
   */
  NdbMutex_Lock(m_ndb->theEventBuffer->m_mutex);
  m_ndb->theEventBuffer->remove_op();
  NdbMutex_Unlock(m_ndb->theEventBuffer->m_mutex);
  m_state= EO_DROPPED;
  mi_type= 0;
  if (r == 0) {
    if (stop_gci == 0)
    {
      // response from old kernel
      stop_gci= m_ndb->theEventBuffer->m_highest_sub_gcp_complete_GCI;
      if (stop_gci)
      {
        // calculate a "safe" gci in the future to remove event op.
        stop_gci += (Uint64(3) << 32);
      }
      else
      {
        // set highest value to ensure that operation does not get dropped
        // too early. Note '-1' as ~Uint64(0) indicates active event
        stop_gci = ~Uint64(0)-1;
      }
    }
    NdbMutex_Lock(m_ndb->theEventBuffer->m_mutex);
    if (m_stop_gci == MAX_EPOCH) //A CLUSTER_FAILURE could happen inbetween
    {
      m_stop_gci = MonotonicEpoch(m_ndb->theEventBuffer->m_epoch_generation, stop_gci);
    }
    NdbMutex_Unlock(m_ndb->theEventBuffer->m_mutex);
    m_ndb->theEventBuffer->add_drop_unlock();
    DBUG_RETURN(0);
  }
  //Error
  m_error.code= NdbDictionaryImpl::getImpl(*myDict).m_error.code;
  m_state= EO_ERROR;
  m_ndb->theEventBuffer->add_drop_unlock();
  DBUG_RETURN(r);
}

bool NdbEventOperationImpl::tableNameChanged() const
{
  return (bool)AlterTableReq::getNameFlag(m_change_mask);
}

bool NdbEventOperationImpl::tableFrmChanged() const
{
  return (bool)AlterTableReq::getFrmFlag(m_change_mask);
}

bool NdbEventOperationImpl::tableFragmentationChanged() const
{
  return (bool)AlterTableReq::getFragDataFlag(m_change_mask);
}

bool NdbEventOperationImpl::tableRangeListChanged() const
{
  return (bool)AlterTableReq::getRangeListFlag(m_change_mask);
}

Uint64
NdbEventOperationImpl::getGCI() const
{
  return m_data_item->getGCI();
}

Uint64
EventBufData::getGCI() const
{
  const Uint32 gci_hi = sdata->gci_hi;
  const Uint32 gci_lo = sdata->gci_lo;
  return gci_lo | (Uint64(gci_hi) << 32);
}

bool
NdbEventOperationImpl::isErrorEpoch(NdbDictionary::Event::TableEvent *error_type)
{
  const NdbDictionary::Event::TableEvent type = getEventType2();
  // Error types are defined from TE_INCONSISTENT
  if (type >= NdbDictionary::Event::TE_INCONSISTENT)
  {
    if (error_type)
      *error_type = type;
    return true;
  }
  return false;
}

bool
NdbEventOperationImpl::isEmptyEpoch()
{
  const Uint32 type = getEventType2();
  if (type == NdbDictionary::Event::TE_EMPTY)
    return true;
  return false;
}

Uint32
NdbEventOperationImpl::getAnyValue() const
{
  return m_data_item->sdata->anyValue;
}

Uint64
NdbEventOperationImpl::getLatestGCI()
{
  return m_ndb->theEventBuffer->getLatestGCI();
}

Uint64
NdbEventOperationImpl::getTransId() const
{
  /* Return 64 bit composite */
  Uint32 transId1 = m_data_item->sdata->transId1;
  Uint32 transId2 = m_data_item->sdata->transId2;
  return Uint64(transId1) << 32 | transId2;
}

bool
NdbEventOperationImpl::execSUB_TABLE_DATA(const NdbApiSignal * signal,
                                          const LinearSectionPtr ptr[3])
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
    m_ndb->theImpl->incClientStat(Ndb::NonDataEventsRecvdCount, 1);
    if (operation == NdbDictionary::Event::_TE_ALTER)
    {
      // Parse the new table definition and
      // create a table object
      NdbDictInterface::Tx tx_unused;
      NdbError error;
      int warn;
      NdbDictInterface dif(tx_unused, error, warn);
      NdbTableImpl *at;
      m_change_mask = m_data_item->sdata->changeMask;
      error.code = dif.parseTableInfo(&at,
                                      (Uint32*)m_buffer.get_data(), 
                                      m_buffer.length() / 4, 
                                      true);
      m_buffer.clear();
      if (unlikely(error.code))
      {
        DBUG_PRINT("info", ("Failed to parse DictTabInfo error %u", 
                                  error.code));
        ndbout_c("Failed to parse DictTabInfo error %u", error.code);
        DBUG_RETURN(1);
      }
      at->buildColumnHash();
      
      NdbTableImpl *tmp_table_impl= m_eventImpl->m_tableImpl;
      m_eventImpl->m_tableImpl = at;
      
      DBUG_PRINT("info", ("switching table impl 0x%lx -> 0x%lx",
                          (long) tmp_table_impl, (long) at));
      
      // change the rec attrs to refer to the new table object
      int i;
      for (i = 0; i < 2; i++)
      {
        NdbRecAttr *p = theFirstPkAttrs[i];
        while (p)
        {
          int no = p->getColumn()->getColumnNo();
          NdbColumnImpl *tAttrInfo = at->getColumn(no);
          DBUG_PRINT("info", ("rec_attr: 0x%lx  "
                              "switching column impl 0x%lx -> 0x%lx",
                              (long) p, (long) p->m_column, (long) tAttrInfo));
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
          DBUG_PRINT("info", ("rec_attr: 0x%lx  "
                              "switching column impl 0x%lx -> 0x%lx",
                              (long) p, (long) p->m_column, (long) tAttrInfo));
          p->m_column = tAttrInfo;
          p = p->next();
        }
      }
      // change the blobHandle's to refer to the new table object.
      NdbBlob *p = theBlobList;
      while (p)
      {
        int no = p->getColumn()->getColumnNo();
        NdbColumnImpl *tAttrInfo = at->getColumn(no);
        DBUG_PRINT("info", ("blob_handle: 0x%lx  "
                            "switching column impl 0x%lx -> 0x%lx",
                            (long) p, (long) p->theColumn, (long) tAttrInfo));
        p->theColumn = tAttrInfo;
        p = p->next();
      }
      if (tmp_table_impl) 
        delete tmp_table_impl;
    }
    DBUG_RETURN(1);
  }

  DBUG_ENTER_EVENT("NdbEventOperationImpl::receive_event");
  DBUG_PRINT_EVENT("info",("sdata->operation %u  this: %p", operation, this));
  // now move the data into the RecAttrs
  m_ndb->theImpl->incClientStat(Ndb::DataEventsRecvdCount, 1);

  int is_insert= operation == NdbDictionary::Event::_TE_INSERT;

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
      if (!is_insert)
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
  int hasSomeData= (operation != NdbDictionary::Event::_TE_UPDATE) ||
    m_allow_empty_update;
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
      hasSomeData=1;
      
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
      hasSomeData=1;
      
      receive_data(tWorkingRecAttr, aDataPtr, tDataSz);
      tWorkingRecAttr = tWorkingRecAttr->next();
    }
    aDataPtr += (tDataSz + 3) >> 2;
  }
  while (tWorkingRecAttr != NULL) {
    tWorkingRecAttr->setUNDEFINED();
    tWorkingRecAttr = tWorkingRecAttr->next();
  }
  
  if (hasSomeData)
  {
    DBUG_RETURN_EVENT(1);
  }

  DBUG_RETURN_EVENT(0);
}

NdbDictionary::Event::TableEvent 
NdbEventOperationImpl::getEventType2()
{
  return (NdbDictionary::Event::TableEvent)
    (1U << SubTableData::getOperation(m_data_item->sdata->requestInfo));
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

EventBufferManager::EventBufferManager(const Ndb* const ndb) :
  m_ndb(ndb),
  m_pre_gap_epoch(0), // equivalent to setting state COMPLETELY_BUFFERING
  m_begin_gap_epoch(0),
  m_end_gap_epoch(0),
  m_max_buffered_epoch(0),
  m_max_received_epoch(0),
  m_free_percent(20),
  m_event_buffer_manager_state(EBM_COMPLETELY_BUFFERING)
{}

unsigned
EventBufferManager::get_eventbuffer_free_percent()
{
  return m_free_percent;
}

void
EventBufferManager::set_eventbuffer_free_percent(unsigned free)
{
  m_free_percent = free;
}

void
EventBufferManager::onBufferingEpoch(Uint64 received_epoch)
{
  if (m_max_buffered_epoch < received_epoch)
    m_max_buffered_epoch = received_epoch;
}

bool
EventBufferManager::onEventDataReceived(Uint32 memory_usage_percent,
                                        Uint64 received_epoch)
{
  bool report_status = false;

  if (isCompletelyBuffering())
  {
    if (memory_usage_percent >= 100)
    {
      // Transition COMPLETELY_BUFFERING -> PARTIALLY_DISCARDING.
      m_pre_gap_epoch = m_max_buffered_epoch;
      m_event_buffer_manager_state = EBM_PARTIALLY_DISCARDING;
      report_status = true;
    }
  }
  else if (isCompletelyDiscarding())
  {
    if (memory_usage_percent < 100 - m_free_percent)
    {
      // Transition COMPLETELY_DISCARDING -> PARTIALLY_BUFFERING
      m_end_gap_epoch = m_max_received_epoch;
      m_event_buffer_manager_state = EBM_PARTIALLY_BUFFERING;
      report_status = true;
    }
  }
  else if (isPartiallyBuffering())
  {
    if (memory_usage_percent >= 100)
    {
      // New gap is starting before the on-going gap ends.
      report_status = true;

      g_eventLogger->warning("Ndb 0x%x %s: Event Buffer: Ending gap epoch %u/%u (%llu) lacks event buffer memory. Overbuffering.",
              m_ndb->getReference(), m_ndb->getNdbObjectName(),
              Uint32(m_begin_gap_epoch >> 32), Uint32(m_begin_gap_epoch),
              m_begin_gap_epoch);
      g_eventLogger->warning("Check how many epochs the eventbuffer_free_percent memory can accommodate.\n");
      g_eventLogger->warning("Increase eventbuffer_free_percent, eventbuffer memory or both accordingly.\n");
    }
  }
  /**
   * else: transition from PARTIALLY_DISCARDING to COMPLETELY_DISCARDING
   * and PARTIALLY_BUFFERING to COMPLETELY_BUFFERING
   * will be handled in execSUB_GCP_COMPLETE()
   */

  // Any new epoch received after memory becomes available will be buffered
  if (m_max_received_epoch < received_epoch)
    m_max_received_epoch = received_epoch;

  return report_status;
}

bool
EventBufferManager::isEventDataToBeDiscarded(Uint64 received_epoch)
{
  DBUG_ENTER_EVENT("EventBufferManager::isEventDataToBeDiscarded");
  /* Discard event data received via SUB_TABLE_DATA during gap period,
   * m_pre_gap_epoch > 0 : gap will start at the next epoch
   * m_end_gap_epoch == 0 : gap has not ended
   * received_epoch <= m_end_gap_epoch : gap has ended at m_end_gap_epoch
   */
  if (m_pre_gap_epoch > 0 && received_epoch > m_pre_gap_epoch &&
      (m_end_gap_epoch == 0 || received_epoch <= m_end_gap_epoch ))
  {
    assert(isInDiscardingState());
    DBUG_PRINT_EVENT("info", ("Discarding SUB_TABLE_DATA for epoch %u/%u (%llu) > begin_gap epoch %u/%u (%llu)",
                              Uint32(received_epoch >> 32),
                              Uint32(received_epoch),
                              received_epoch,
                              Uint32(m_pre_gap_epoch >> 32),
                              Uint32(m_pre_gap_epoch),
                              m_pre_gap_epoch));
    if (m_end_gap_epoch > 0)
    {
      DBUG_PRINT_EVENT("info", (" and <= end_gap epoch %u/%u (%llu)"
                                Uint32(m_end_gap_epoch >> 32),
                                Uint32(m_end_gap_epoch),
                                m_end_gap_epoch));
    }
    DBUG_RETURN_EVENT(true);
  }
  DBUG_RETURN_EVENT(false);
}

bool
EventBufferManager::onEpochCompleted(Uint64 completed_epoch, bool& gap_begins)
{
  bool report_status = false;

  if (isPartiallyDiscarding() && completed_epoch > m_pre_gap_epoch)
  {
    /**
     * No on-going gap. This should be the first completed epoch after
     * a transition to PARTIALLY_DISCARDING (the first completed epoch
     * after m_pre_gap_epoch). Mark this as the beginning of a new gap.
     * Transition PARTIALLY_DISCARDING -> COMPLETELY_DISCARDING:
     */
    m_begin_gap_epoch = completed_epoch;
    m_event_buffer_manager_state = EBM_COMPLETELY_DISCARDING;
    gap_begins = true;
    report_status = true;
    g_eventLogger->warning("Ndb 0x%x %s: Event Buffer: New gap begins at epoch : %u/%u (%llu)",
                           m_ndb->getReference(), m_ndb->getNdbObjectName(),
                           (Uint32)(m_begin_gap_epoch >> 32),
                           (Uint32)m_begin_gap_epoch, m_begin_gap_epoch);
  }
  else if (isPartiallyBuffering() && completed_epoch > m_end_gap_epoch)
  {
    // The completed_epoch marks the first completely buffered post_gap epoch
    // Transition PARTIALLY_BUFFERNG -> COMPLETELY_BUFFERING
    g_eventLogger->warning("Ndb 0x%x %s: Event Buffer : Gap began at epoch : %u/%u (%llu) ends at epoch %u/%u (%llu)",
                           m_ndb->getReference(),
                           m_ndb->getNdbObjectName(),
                           (Uint32)(m_begin_gap_epoch >> 32),
                           (Uint32)m_begin_gap_epoch, m_begin_gap_epoch,
                           (Uint32)(completed_epoch >> 32),
                           (Uint32)completed_epoch, completed_epoch);
    m_pre_gap_epoch = 0;
    m_begin_gap_epoch = 0;
    m_end_gap_epoch = 0;
    m_event_buffer_manager_state = EBM_COMPLETELY_BUFFERING;
    report_status = true;
  }
  /**
   * else: transition from COMPLETELY_BUFFERING to PARTIALLY_DISCARDING
   * and COMPLETELY_DISCARDING to PARTIALLY_BUFFERING
   * are handled in insertDataL
   */
  return report_status;
}

bool
EventBufferManager::isGcpCompleteToBeDiscarded(Uint64 completed_epoch)
{
  DBUG_ENTER_EVENT("EventBufferManager::isGcpCompleteToBeDiscarded");
  /* Discard SUB_GCP_COMPLETE during gap period,
   * m_begin_gap_epoch > 0 : gap has started at m_begin_gap_epoch
   * m_end_gap_epoch == 0 : gap has not ended
   * received_epoch <= m_end_gap_epoch : gap has ended at m_end_gap_epoch
   */

  // for m_begin_gap_epoch < completed_epoch <= m_end_gap_epoch

  if (m_begin_gap_epoch > 0 && completed_epoch > m_begin_gap_epoch &&
      (m_end_gap_epoch == 0 || completed_epoch <= m_end_gap_epoch ))
  {
    assert(isInDiscardingState());
    DBUG_PRINT_EVENT("info", ("Discarding SUB_GCP_COMPLETE_REP for epoch %u/%u (%llu) > begin_gap epoch %u/%u (%llu)",
                              Uint32(completed_epoch >> 32),
                              Uint32(completed_epoch),
                              completed_epoch,
                              Uint32(m_begin_gap_epoch >> 32),
                              Uint32(m_begin_gap_epoch),
                              m_begin_gap_epoch));
    if (m_end_gap_epoch > 0)
    {
      DBUG_PRINT_EVENT("info", (" and <= end_gap epoch %u/%u (%llu)"
                                Uint32(m_end_gap_epoch >> 32),
                                Uint32(m_end_gap_epoch),
                                m_end_gap_epoch));
    }
    DBUG_RETURN_EVENT(true);
  }
  DBUG_RETURN_EVENT(false);
}

/*
 * Class NdbEventBuffer
 * Each Ndb object has a Object.
 */
NdbEventBuffer::NdbEventBuffer(Ndb *ndb) :
  m_total_buckets(TOTAL_BUCKETS_INIT), 
  m_min_gci_index(0),
  m_max_gci_index(0),
  m_known_gci(),
  m_active_gci(),
  m_ndb(ndb),
  m_epoch_generation(0),
  m_latestGCI(0), m_latest_complete_GCI(0),
  m_highest_sub_gcp_complete_GCI(0),
  m_latest_poll_GCI(),
  m_failure_detected(false),
  m_prevent_nodegroup_change(true),
  m_mutex(NULL),
  m_current_data(NULL),
  m_total_alloc(0),
  m_max_alloc(0),
  m_event_buffer_manager(ndb),
  m_free_thresh(0),
  m_min_free_thresh(0),
  m_max_free_thresh(0),
  m_gci_slip_thresh(0),
  m_dropped_ev_op(0),
  m_active_op_count(0)
{
#ifdef VM_TRACE
  m_latest_command= "NdbEventBuffer::NdbEventBuffer";
  m_flush_gci = 0;
#endif

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
  bzero(&m_complete_data, sizeof(m_complete_data));
  bzero(&g_empty_gci_container, sizeof(Gci_container));
  init_gci_containers();

  m_alive_node_bit_mask.clear();

  bzero(&m_sub_data_streams, sizeof(m_sub_data_streams));
}

NdbEventBuffer::~NdbEventBuffer()
{
  // client should not have any active subscriptions
  assert(m_active_op_count == 0);
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

  // Return EventBufData lists to free list in a nice way
  // before actual deallocation using m_allocated_data.
  free_list(m_complete_data.m_data);
  free_list(m_available_data);
  free_list(m_used_data);

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
}

unsigned
NdbEventBuffer::get_eventbuffer_free_percent()
{
  return m_event_buffer_manager.get_eventbuffer_free_percent();
}

void
NdbEventBuffer::set_eventbuffer_free_percent(unsigned free)
{
  m_event_buffer_manager.set_eventbuffer_free_percent(free);
}

void
NdbEventBuffer::add_op()
{
  /*
   * When m_active_op_count is zero, SUB_GCP_COMPLETE_REP is
   * ignored and no event data will reach application.
   * Positive values will enable event data to reach application.
   */
  m_active_op_count++;
}

void
NdbEventBuffer::remove_op()
{
  assert(m_active_op_count > 0);
  m_active_op_count--;
}

/**
 * Init the *receiver thread* part of the event buffers.
 *
 * NOTE:
 *  ::consume_all() is the propper way to empty the client
 *  side buffers.
 */
void
NdbEventBuffer::init_gci_containers()
{
  // Return EventBufData to free list and release GCI ops before clearing.
  for(unsigned i = 0; i < m_active_gci.size(); i++)
  {
    Gci_container& bucket = (Gci_container&)m_active_gci[i];
    free_list(bucket.m_data);
  }

  m_startup_hack = true;
  m_active_gci.clear();
  m_active_gci.fill(3, g_empty_gci_container);
  m_min_gci_index = m_max_gci_index = 1;
  Uint64 gci = 0;
  m_known_gci.clear();
  m_known_gci.fill(7, gci);
  // No 'out of order' epoch in the containers.
  m_latest_complete_GCI = 0;
}

/**
 * Discard all buffered events in the client thread.
 */
void NdbEventBuffer::consume_all()   //Need m_mutex locked
{
  m_current_data = NULL;

  // Drop all buffered event data and its gci_ops
  free_list(m_complete_data.m_data);
  free_list(m_available_data);
  free_list(m_used_data);

  /* Clean up deleted event_op which expired. In case we consumed
   * across a failure event the MonotonicEvent will handle
   * this by inclusion of the 'epoch generation'.
   */
  deleteUsedEventOperations(MonotonicEpoch(m_epoch_generation,m_latestGCI));
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
NdbEventBuffer::pollEvents(Uint64 *highestQueuedEpoch)
{
  int ret= 1;
#ifdef VM_TRACE
  const char *m_latest_command_save= m_latest_command;
  m_latest_command= "NdbEventBuffer::pollEvents";
#endif

  NdbMutex_Lock(m_mutex);
  EventBufData *ev_data= move_data();
  m_latest_poll_GCI= MonotonicEpoch(m_epoch_generation,m_latestGCI);
#ifdef VM_TRACE
  if (ev_data && ev_data->m_event_op)
  {
    NdbEventOperationImpl *ev_op= ev_data->m_event_op;
    // m_mutex is locked
    // update event ops data counters
    ev_op->m_data_count-= ev_op->m_data_done_count;
    ev_op->m_data_done_count= 0;
  }
  m_latest_command= m_latest_command_save;
#endif
  if (unlikely(ev_data == NULL))
  {
    ret= 0; // applicable for both aMillisecondNumber >= 0
    /*
      gci's consumed up until m_latest_poll_GCI, so we can free all
      dropped event operations stopped up until that gci
    */
    deleteUsedEventOperations(m_latest_poll_GCI);
  }
  NdbMutex_Unlock(m_mutex); // we have moved the data

  if (highestQueuedEpoch)
    *highestQueuedEpoch= m_latest_poll_GCI.getGCI();

  return ret;
}

int
NdbEventBuffer::flushIncompleteEvents(Uint64 gci)
{
  /**
   *  Find min complete gci
   */
  Uint64 * array = m_known_gci.getBase();
  Uint32 mask = m_known_gci.size() - 1;
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;

  g_eventLogger->info("Flushing incomplete GCI:s < %u/%u",
                      Uint32(gci >> 32), Uint32(gci));
  while (minpos != maxpos && array[minpos] < gci)
  {
    Gci_container* tmp = find_bucket(array[minpos]);
    assert(tmp);
    assert(maxpos == m_max_gci_index);
    free_list(tmp->m_data);
    bzero(tmp, sizeof(Gci_container));
    minpos = (minpos + 1) & mask;
  }

  m_min_gci_index = minpos;

#ifdef VM_TRACE
  m_flush_gci = gci;
#endif

  return 0;
}

void
NdbEventBuffer::free_consumed_event_data()
{
  if (m_used_data.m_count > 1024)
  {
#ifdef VM_TRACE
    m_latest_command= "NdbEventBuffer::free_consumed_event_data (lock)";
#endif
    NdbMutex_Lock(m_mutex);
    // return m_used_data to m_free_data
    free_list(m_used_data);

    NdbMutex_Unlock(m_mutex);
  }
}

bool
NdbEventBuffer::is_exceptional_epoch(EventBufData *data)
{
  Uint32 type = SubTableData::getOperation(data->sdata->requestInfo);

  if (type == NdbDictionary::Event::_TE_EMPTY ||
      type >= NdbDictionary::Event::_TE_INCONSISTENT)
  {
    if (type != NdbDictionary::Event::_TE_EMPTY)
    {
      DBUG_PRINT_EVENT("info",
                       ("detected excep. gci %u/%u (%u) 0x%x 0x%x %s",
                        Uint32(gci >> 32), Uint32(gci),
                        data->sdata->gci_lo|(Uint64(data->sdata->gci_hi) << 32),
                        type,
                        m_ndb->getReference(), m_ndb->getNdbObjectName()));
    }
    DBUG_RETURN_EVENT(true);
  }
  DBUG_RETURN_EVENT(false);
}

/**
 * Return the next EventData deliverable to the client.
 * Keeps the Gci_ops list in synch with the returned EventData
 * such that correct current Gci_ops is referred.
 * Gci_ops belonging to consumed EventData are deleted.
 */
EventBufData *
NdbEventBuffer::nextEventData(Uint32 & full_sz)
{
  EventBufData *data = m_available_data.m_head;
  if (data != NULL)
  {
    // Move first available item to used queue prior to processing
    Uint32 full_count;
    m_available_data.remove_first(full_count, full_sz);
    m_used_data.append_used_data(data, full_count, full_sz);

    // Sync Gci_ops with EventData-gci
    const Uint64 gci = data->getGCI();
    EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
    while (gci_ops && gci_ops->m_gci.getGCI() != gci)
    {
      gci_ops = m_available_data.delete_next_gci_ops();
    }

    // There is a requirement that all EventData should have a Gci_ops
    // with the same gci. Report if that fails.
    if (gci_ops == NULL)
    {
      // NOTE: ::nextEventData() keep data and Gci_ops on same GCI,
      //       iff there are any Gci_ops at all.
      ndbout << "nextEventData, no 'Gci_ops'"
             << " gci " << gci
             << " (" << Uint32(gci >> 32) << "/" << Uint32(gci) << ")";

      NdbEventOperationImpl *op= data->m_event_op;
      if (op != NULL)
        ndbout << " type " << hex << op->getEventType2();
      else
        ndbout << " exceptional type ";

      ndbout << " data's operation " << hex
             <<  SubTableData::getOperation(data->sdata->requestInfo)
             << " " << m_ndb->getNdbObjectName()
             << endl;
    }
    assert(gci_ops != NULL);

    // Should not return a GCI higher then last polled GCI,
    assert(gci_ops->m_gci <= m_latest_poll_GCI);
  }
  else
  {
    // No more EventData, delete remaining Gci_ops.
    while (m_available_data.first_gci_ops())
    {
      m_available_data.delete_next_gci_ops();
    }
  }

  m_current_data = data;
  return data;
}

NdbEventOperation *
NdbEventBuffer::nextEvent2()
{
  DBUG_ENTER_EVENT("NdbEventBuffer::nextEvent2");
#ifdef VM_TRACE
  const char *m_latest_command_save= m_latest_command;
#endif

  free_consumed_event_data();

#ifdef VM_TRACE
  m_latest_command= "NdbEventBuffer::nextEvent2";
#endif

  Uint32 data_size = 0;
  while (EventBufData *data= nextEventData(data_size))
  {
    m_ndb->theImpl->incClientStat(Ndb::EventBytesRecvdCount, data_size);

    NdbEventOperationImpl *op= data->m_event_op;
    assert(!(op && op->m_state == 0xDead)); //event_op destructed?

    /*
     * Exceptional events are not yet associated with an event operation,
     * Pick one, which one is not important, to tuck the ex-event onto.
     */
    assert((op == NULL) == (is_exceptional_epoch(data)));
    if (is_exceptional_epoch(data))
    {
      DBUG_PRINT_EVENT("info", ("detected inconsistent gci %u 0x%x %s",
                                data->getGCI(), m_ndb->getReference(),
                                m_ndb->getNdbObjectName()));

      // If all event operations are dropped, ignore exceptional-event
      op = m_ndb->theImpl->m_ev_op;
      if (op == NULL)
        continue;

      data->m_event_op = op;
      op->m_data_item = data;
      DBUG_RETURN_EVENT(op->m_facade);
    }

    DBUG_PRINT_EVENT("info", ("available data=%p op=%p 0x%x %s",
                              data, op, m_ndb->getReference(),
                              m_ndb->getNdbObjectName()));

    /*
     * If merge is on, blob part sub-events must not be seen on this level.
     * If merge is not on, there are no blob part sub-events.
     */
    assert(op->theMainOp == NULL);

    // set NdbEventOperation data
    op->m_data_item= data;

#ifdef VM_TRACE
    op->m_data_done_count++;
#endif

    if (op->m_state == NdbEventOperation::EO_EXECUTING)
    {
      int r= op->receive_event();
      if (r > 0)
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

         // to return TE_NUL it should be made into data event
         if (SubTableData::getOperation(data->sdata->requestInfo) ==
	   NdbDictionary::Event::_TE_NUL)
         {
           DBUG_PRINT_EVENT("info", ("skip _TE_NUL 0x%x %s",
                                     m_ndb->getReference(),
                                     m_ndb->getNdbObjectName()));
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

  // All available events and its gci_ops should have been consumed
  assert(m_available_data.first_gci_ops() == NULL);
  assert(m_available_data.is_empty());
  assert(m_current_data == NULL);

  /*
   * Event consumed up until m_latest_poll_GCI.
   * Free all dropped event operations stopped up until that gci
   */
  if (m_dropped_ev_op)
  {
    NdbMutex_Lock(m_mutex);
    deleteUsedEventOperations(m_latest_poll_GCI);
    NdbMutex_Unlock(m_mutex);
  }
  DBUG_RETURN_EVENT(0);
}

bool
NdbEventBuffer::isConsistent(Uint64& gci)
{
  DBUG_ENTER("NdbEventBuffer::isConsistent");
  EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
  while (gci_ops)
  {
    if (gci_ops->m_error == NdbDictionary::Event::_TE_INCONSISTENT)
    {
      gci = gci_ops->m_gci.getGCI();
      DBUG_RETURN(false);
    }
    gci_ops = gci_ops->m_next;
  }

  DBUG_RETURN(true);
}

bool
NdbEventBuffer::isConsistentGCI(Uint64 gci)
{
  DBUG_ENTER("NdbEventBuffer::isConsistentGCI");
  EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
  while (gci_ops)
  {
    if (gci_ops->m_gci.getGCI() == gci &&
        gci_ops->m_error == NdbDictionary::Event::_TE_INCONSISTENT)
      DBUG_RETURN(false);
    gci_ops = gci_ops->m_next;
  }

  DBUG_RETURN(true);
}

NdbEventOperationImpl*
NdbEventBuffer::getEpochEventOperations(Uint32* iter, Uint32* event_types, Uint32* cumulative_any_value)
{
  DBUG_ENTER("NdbEventBuffer::getEpochEventOperations");
  EventBufData_list::Gci_ops *gci_ops = m_available_data.first_gci_ops();
  if (*iter < gci_ops->m_gci_op_count)
  {
    EventBufData_list::Gci_op g = gci_ops->m_gci_op_list[(*iter)++];
    if (event_types != NULL)
      *event_types = g.event_types;
    if (cumulative_any_value != NULL)
      *cumulative_any_value = g.cumulative_any_value;
    DBUG_PRINT("info", ("gci: %u  g.op: 0x%lx  g.event_types: 0x%lx g.cumulative_any_value: 0x%lx 0x%x %s",
                        (unsigned)gci_ops->m_gci.getGCI(), (long) g.op,
                        (long) g.event_types, (long) g.cumulative_any_value,
                        m_ndb->getReference(), m_ndb->getNdbObjectName()));
    DBUG_RETURN(g.op);
  }
  DBUG_RETURN(NULL);
}

void
NdbEventBuffer::deleteUsedEventOperations(MonotonicEpoch last_consumed_gci)
{
  NdbEventOperationImpl *op= m_dropped_ev_op;
  while (op && op->m_stop_gci != NULL_EPOCH)
  {
    if (last_consumed_gci > op->m_stop_gci)
    {
      while (op)
      {
        NdbEventOperationImpl *next_op= op->m_next;
        op->m_stop_gci = NULL_EPOCH;
        op->m_ref_count--;
        if (op->m_ref_count == 0)
        {
          if (op->m_next)
            op->m_next->m_prev = op->m_prev;
          if (op->m_prev)
            op->m_prev->m_next = op->m_next;
          else
            m_dropped_ev_op = op->m_next;
          delete op->m_facade;
        }
        op = next_op;
      }
      break;
    }
    op = op->m_next;
  }
}

#ifdef VM_TRACE

NdbOut&
operator<<(NdbOut& out, const MonotonicEpoch& gci)
{
  out << (gci.getGCI() >> 32) << "/" << (gci.getGCI() & 0xFFFFFFFF);
  out << "(" << gci.m_seq << ")";
  return out;
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
#endif

void
NdbEventBuffer::resize_known_gci()
{
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;
  Uint32 mask = m_known_gci.size() - 1;

  Uint64 fill = 0;
  Uint32 newsize = 2 * (mask + 1);
  m_known_gci.fill(newsize - 1, fill);
  Uint64 * array = m_known_gci.getBase();

  if (0)
  {
    printf("before (%u): ", minpos);
    for (Uint32 i = minpos; i != maxpos; i = (i + 1) & mask)
      printf("%u/%u ",
             Uint32(array[i] >> 32),
             Uint32(array[i]));
    printf("\n");
  }

  Uint32 idx = mask + 1; // Store eveything in "new" part of buffer
  if (0) printf("swapping ");
  while (minpos != maxpos)
  {
    if (0) printf("%u-%u ", minpos, idx);
    Uint64 tmp = array[idx];
    array[idx] = array[minpos];
    array[minpos] = tmp;

    idx++;
    minpos = (minpos + 1) & mask; // NOTE old mask
  }
  if (0) printf("\n");

  minpos = m_min_gci_index = mask + 1;
  maxpos = m_max_gci_index = idx;
  assert(minpos < maxpos);

  if (0)
  {
    ndbout_c("resize_known_gci from %u to %u", (mask + 1), newsize);
    printf("after: ");
    for (Uint32 i = minpos; i < maxpos; i++)
    {
      printf("%u/%u ",
             Uint32(array[i] >> 32),
             Uint32(array[i]));
    }
    printf("\n");
  }

#ifdef VM_TRACE
  Uint64 gci = array[minpos];
  for (Uint32 i = minpos + 1; i<maxpos; i++)
  {
    assert(array[i] > gci);
    gci = array[i];
  }
#endif
}

#ifdef VM_TRACE
void
NdbEventBuffer::verify_known_gci(bool allowempty)
{
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;
  Uint32 mask = m_known_gci.size() - 1;

  Uint32 line;
#define MMASSERT(x) { if (!(x)) { line = __LINE__; goto fail; }}
  if (m_min_gci_index == m_max_gci_index)
  {
    MMASSERT(allowempty);
    for (Uint32 i = 0; i<m_active_gci.size(); i++)
      MMASSERT(((Gci_container*)(m_active_gci.getBase()+i))->m_gci == 0);
    return;
  }

  {
    Uint64 last = m_known_gci[minpos];
    MMASSERT(last > m_latestGCI);
    MMASSERT(find_bucket(last) != 0);
    MMASSERT(maxpos == m_max_gci_index);

    minpos = (minpos + 1) & mask;
    while (minpos != maxpos)
    {
      MMASSERT(m_known_gci[minpos] > last);
      last = m_known_gci[minpos];
      MMASSERT(find_bucket(last) != 0);
      MMASSERT(maxpos == m_max_gci_index);
      minpos = (minpos + 1) & mask;
    }
  }

  {
    Gci_container* bucktets = (Gci_container*)(m_active_gci.getBase());
    for (Uint32 i = 0; i<m_active_gci.size(); i++)
    {
      if (bucktets[i].m_gci)
      {
        bool found = false;
        for (Uint32 j = m_min_gci_index; j != m_max_gci_index;
             j = (j + 1) & mask)
        {
          if (m_known_gci[j] == bucktets[i].m_gci)
          {
            found = true;
            break;
          }
        }
        if (!found)
          ndbout_c("%u/%u not found",
                   Uint32(bucktets[i].m_gci >> 32),
                   Uint32(bucktets[i].m_gci));
        MMASSERT(found == true);
      }
    }
  }

  return;
fail:
  ndbout_c("assertion at %d", line);
  printf("known gci: ");
  for (Uint32 i = m_min_gci_index; i != m_max_gci_index; i = (i + 1) & mask)
  {
    printf("%u/%u ", Uint32(m_known_gci[i] >> 32), Uint32(m_known_gci[i]));
  }

  printf("\nContainers");
  for (Uint32 i = 0; i<m_active_gci.size(); i++)
    ndbout << m_active_gci[i] << endl;
  abort();
}
#endif

Gci_container*
NdbEventBuffer::find_bucket_chained(Uint64 gci)
{
  if (0)
    printf("find_bucket_chained(%u/%u) ", Uint32(gci >> 32), Uint32(gci));
  if (unlikely(gci <= m_latestGCI))
  {
    /**
     * an already complete GCI
     */
    if (0)
      ndbout_c("already complete (%u/%u)",
               Uint32(m_latestGCI >> 32),
               Uint32(m_latestGCI));
    return 0;
  }

  if (m_event_buffer_manager.isGcpCompleteToBeDiscarded(gci))
  {
    return 0; // gci belongs to a gap
  }

  if (unlikely(m_total_buckets == 0))
  {
    return 0;
  }

  Uint32 pos = Uint32(gci & ACTIVE_GCI_MASK);
  Uint32 size = m_active_gci.size();
  Gci_container *buckets = (Gci_container*)(m_active_gci.getBase());
  while (pos < size)
  {
    Uint64 cmp = (buckets + pos)->m_gci;
    if (cmp == gci)
    {
      if (0)
        ndbout_c("found pos: %u", pos);
      return buckets + pos;
    }

    if (cmp == 0)
    {
      if (0)
        ndbout_c("empty(%u) ", pos);
      Uint32 search = pos + ACTIVE_GCI_DIRECTORY_SIZE;
      while (search < size)
      {
        if ((buckets + search)->m_gci == gci)
        {
          memcpy(buckets + pos, buckets + search, sizeof(Gci_container));
          bzero(buckets + search, sizeof(Gci_container));
          if (0)
            printf("moved from %u to %u", search, pos);
          if (search == size - 1)
          {
            m_active_gci.erase(search);
            if (0)
              ndbout_c(" shrink");
          }
          else
          {
            if (0)
              printf("\n");
          }
          return buckets + pos;
        }
        search += ACTIVE_GCI_DIRECTORY_SIZE;
      }
      goto newbucket;
    }
    pos += ACTIVE_GCI_DIRECTORY_SIZE;
  }

  /**
   * This is a new bucket...likely close to start
   */
  if (0)
    ndbout_c("new (with expand) ");
  m_active_gci.fill(pos, g_empty_gci_container);
  buckets = (Gci_container*)(m_active_gci.getBase());
newbucket:
  Gci_container* bucket = buckets + pos;
  bucket->m_gci = gci;
  bucket->m_gcp_complete_rep_count = m_total_buckets;

  Uint32 mask = m_known_gci.size() - 1;
  Uint64 * array = m_known_gci.getBase();

  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;
  bool full = ((maxpos + 1) & mask) == minpos;
  if (unlikely(full))
  {
    resize_known_gci();
    minpos = m_min_gci_index;
    maxpos = m_max_gci_index;
    mask = m_known_gci.size() - 1;
    array = m_known_gci.getBase();
  }

  Uint32 maxindex = (maxpos - 1) & mask;
  Uint32 newmaxpos = (maxpos + 1) & mask;
  m_max_gci_index = newmaxpos;
  if (likely(minpos == maxpos || gci > array[maxindex]))
  {
    array[maxpos] = gci;
#ifdef VM_TRACE
    verify_known_gci(false);
#endif
    return bucket;
  }

  for (pos = minpos; pos != maxpos; pos = (pos + 1) & mask)
  {
    if (array[pos] > gci)
      break;
  }

  if (0)
    ndbout_c("insert %u/%u (max %u/%u) at pos %u (min: %u max: %u)",
             Uint32(gci >> 32),
             Uint32(gci),
             Uint32(array[maxindex] >> 32),
             Uint32(array[maxindex]),
             pos,
             m_min_gci_index, m_max_gci_index);

  assert(pos != maxpos);
  Uint64 oldgci;
  do {
    oldgci = array[pos];
    array[pos] = gci;
    gci = oldgci;
    pos = (pos + 1) & mask;
  } while (pos != maxpos);
  array[pos] = gci;

#ifdef VM_TRACE
  verify_known_gci(false);
#endif
  return bucket;
}

void
NdbEventBuffer::crash_on_invalid_SUB_GCP_COMPLETE_REP(const Gci_container* bucket,
				      const SubGcpCompleteRep * const rep,
                                      Uint32 replen,
                                      Uint32 remcnt,
                                      Uint32 repcnt) const
{
  ndbout_c("INVALID SUB_GCP_COMPLETE_REP");
  // SubGcpCompleteRep
  ndbout_c("signal length: %u", replen);
  ndbout_c("gci: %u/%u", rep->gci_hi, rep->gci_lo);
  ndbout_c("senderRef: x%x", rep->senderRef);
  ndbout_c("count: %u", rep->gcp_complete_rep_count);
  ndbout_c("flags: x%x", rep->flags);
  if (rep->flags & rep->ON_DISK) ndbout_c("\tON_DISK");
  if (rep->flags & rep->IN_MEMORY) ndbout_c("\tIN_MEMORY");
  if (rep->flags & rep->MISSING_DATA) ndbout_c("\tMISSING_DATA");
  if (rep->flags & rep->ADD_CNT) ndbout_c("\tADD_CNT %u", rep->flags>>16);
  if (rep->flags & rep->SUB_CNT) ndbout_c("\tSUB_CNT %u", rep->flags>>16);
  if (rep->flags & rep->SUB_DATA_STREAMS_IN_SIGNAL) 
  {
    ndbout_c("\tSUB_DATA_STREAMS_IN_SIGNAL");
    // Expected signal size with two stream id per word
    const Uint32 explen = rep->SignalLength + (rep->gcp_complete_rep_count + 1)/2;
    if (replen != explen)
    {
      ndbout_c("ERROR: Signal length %d words does not match expected %d! Corrupt signal?", replen, explen);
    }
    // Protect against corrupt signal length, max signal size is 25 words
    if (replen > 25) replen = 25;
    if (replen > rep->SignalLength)
    {
      const int words = replen - rep->SignalLength;
      for (int i=0; i < words; i++)
      {
        ndbout_c("\t\t%04x\t%04x", Uint32(rep->sub_data_streams[i]), Uint32(rep->sub_data_streams[i]>>16));
      }
    }
  }
  ndbout_c("remaining count: %u", remcnt);
  ndbout_c("report count (without duplicates): %u", repcnt);
  // Gci_container
  ndbout_c("bucket gci: %u/%u", Uint32(bucket->m_gci>>32), Uint32(bucket->m_gci));
  ndbout_c("bucket state: x%x", bucket->m_state);
  if (bucket->m_state & bucket->GC_COMPLETE) ndbout_c("\tGC_COMPLETE");
  if (bucket->m_state & bucket->GC_INCONSISTENT) ndbout_c("\tGC_INCONSISTENT");
  if (bucket->m_state & bucket->GC_CHANGE_CNT) ndbout_c("\tGC_CHANGE_CNT");
  if (bucket->m_state & bucket->GC_OUT_OF_MEMORY) ndbout_c("\tGC_OUT_OF_MEMORY");
  ndbout_c("bucket remain count: %u", bucket->m_gcp_complete_rep_count);
  ndbout_c("total buckets: %u", m_total_buckets);
  ndbout_c("startup hack: %u", m_startup_hack);
  for (int i=0; i < MAX_SUB_DATA_STREAMS; i++)
  {
    Uint16 id = m_sub_data_streams[i];
    if (id == 0) continue;
    ndbout_c("stream: idx %u, id %04x, counted %d", i, id, bucket->m_gcp_complete_rep_sub_data_streams.get(i));
  }
  abort();
}

void
NdbEventBuffer::complete_empty_bucket_using_exceptional_event(Uint64 gci,
                                                              Uint32 type)
{
  EventBufData *dummy_data= alloc_data();
  dummy_data->m_event_op = 0;

  /** Add gci and event type to the inconsistent epoch event data,
   * such that nextEvent handles it correctly and makes it visible
   * to the consumer, such that consumer will be able to handle it.
   */
  LinearSectionPtr ptr[3];
  for (int i = 0; i < 3; i++)
  {
    ptr[i].p = NULL;
    ptr[i].sz = 0;
  }
  alloc_mem(dummy_data, ptr, 0);

  SubTableData *sdata = (SubTableData*) dummy_data->memory;
  assert(dummy_data->memory);
  sdata->tableId = ~0;
  sdata->requestInfo = 0;
  sdata->gci_hi = Uint32(gci >> 32);
  sdata->gci_lo = Uint32(gci);
  SubTableData::setOperation(sdata->requestInfo, type);

  // NOTE:
  // We do not yet assign an m_event_op to the exceptional event:
  // Whatever event we assigned now, could later be dropped before
  // nextEvent() reads it. nextEvent() will later find a suitable op.

  // Add gci_ops for error epoch events to make the search for
  // inconsistent(Uint64& gci) to be effective (backward compatibility)
  {
    EventBufData_list dummy_event_list;
    dummy_event_list.append_used_data(dummy_data);
    dummy_event_list.m_is_not_multi_list = true;
    const MonotonicEpoch epoch(m_epoch_generation,gci);
    m_complete_data.m_data.append_list(&dummy_event_list, &epoch);
    assert(m_complete_data.m_data.m_gci_ops_list_tail != NULL);
    if (type >= NdbDictionary::Event::_TE_INCONSISTENT)
    {
      m_complete_data.m_data.m_gci_ops_list_tail->m_error = type;
    }
  }
}

void
NdbEventBuffer::discard_events_from_bucket(Gci_container* bucket)
{
  // Empty the gci_op(s) list of the epoch from the bucket
  DBUG_PRINT_EVENT("info", ("discard_events_from_bucket: deleting m_gci_op_list: %p",
                            bucket->m_data.m_gci_op_list));

  // Empty the event data from the bucket and return it to m_free_data
  // gci_op(s) list is deleted
  free_list(bucket->m_data);
}

void
NdbEventBuffer::complete_bucket(Gci_container* bucket)
{
  const Uint64 gci = bucket->m_gci;
  Gci_container* buckets = (Gci_container*)m_active_gci.getBase();

  if (0)
    ndbout_c("complete %u/%u pos: %u", Uint32(gci >> 32), Uint32(gci),
             Uint32(bucket - buckets));

#ifdef VM_TRACE
  verify_known_gci(false);
#endif

  bool bucket_empty = bucket->m_data.is_empty();
  Uint32 type = 0;
  if (bucket->m_state & Gci_container::GC_INCONSISTENT)
  {
    type = NdbDictionary::Event::_TE_INCONSISTENT;
  }
  else if (bucket->m_state & Gci_container::GC_OUT_OF_MEMORY)
  {
    type = NdbDictionary::Event::_TE_OUT_OF_MEMORY;
  }
  else if (bucket_empty)
  {
    assert(!bucket->m_data.m_is_not_multi_list);
    assert(bucket->m_data.first_gci_ops() == 0);
    type = NdbDictionary::Event::_TE_EMPTY;
  }

  if(!bucket_empty)
  {
#ifdef VM_TRACE
    assert(bucket->m_data.m_count);
#endif

    if (bucket->hasError())
    {
      /*
       * Bucket marked as possibly missing data, probably due to
       * kernel running out of event_buffer during node failure.
       * Discard the partially-received event data.
       */
      discard_events_from_bucket(bucket);
      bucket_empty = true;
    }
  }

  if (bucket_empty)
  {
    assert(type > 0);
    complete_empty_bucket_using_exceptional_event(gci, type);
  }
  else
  {
    // bucket is complete and consistent: add it to complete_data list
    const MonotonicEpoch epoch(m_epoch_generation,gci);
    m_complete_data.m_data.append_list(&bucket->m_data, &epoch);
  }

  Uint32 minpos = m_min_gci_index;
  Uint32 mask = m_known_gci.size() - 1;
  assert((mask & (mask + 1)) == 0);

  bzero(bucket, sizeof(Gci_container));

  m_min_gci_index = (minpos + 1) & mask;

#ifdef VM_TRACE
  verify_known_gci(true);
#endif
}

void
NdbEventBuffer::execSUB_START_CONF(const SubStartConf * const rep,
                                   Uint32 len)
{
  Uint32 buckets;
  if (len >= SubStartConf::SignalLength)
  {
    buckets = rep->bucketCount;
  }
  else
  {
    /*
     * Pre-7.0 kernel nodes do not return the number of buckets
     * Assume it's == theNoOfDBnodes as was the case in 6.3
     */
    buckets = m_ndb->theImpl->theNoOfDBnodes;
  }

  set_total_buckets(buckets);

  add_op();
}

void
NdbEventBuffer::execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep * const rep,
                                         Uint32 len, int complete_cluster_failure)
{
  Uint32 gci_hi = rep->gci_hi;
  Uint32 gci_lo = rep->gci_lo;

  if (unlikely(len < SubGcpCompleteRep::SignalLength))
  {
    gci_lo = 0;
  }

  const Uint64 gci= gci_lo | (Uint64(gci_hi) << 32);
  if (gci > m_highest_sub_gcp_complete_GCI)
    m_highest_sub_gcp_complete_GCI = gci;

  if (!complete_cluster_failure)
  {
    m_alive_node_bit_mask.set(refToNode(rep->senderRef));
    // Reset cluster failure marker
    m_failure_detected= false;

    if (unlikely(m_active_op_count == 0))
    {
      return;
    }
  }
  
  DBUG_ENTER_EVENT("NdbEventBuffer::execSUB_GCP_COMPLETE_REP");

  Uint32 cnt= rep->gcp_complete_rep_count;

  Gci_container *bucket = find_bucket(gci);

  if (0)
    ndbout_c("execSUB_GCP_COMPLETE_REP(%u/%u) cnt: %u from %x flags: 0x%x",
             Uint32(gci >> 32), Uint32(gci), cnt, rep->senderRef,
             rep->flags);

  if (unlikely(rep->flags & (SubGcpCompleteRep::ADD_CNT |
                             SubGcpCompleteRep::SUB_CNT)))
  {
    handle_change_nodegroup(rep);
  }

  if (unlikely(bucket == 0))
  {
    if (unlikely(gci <= m_latestGCI))
    {
    /**
     * Already completed GCI...
     *   Possible in case of resend during NF handling
     */
#ifdef VM_TRACE
    Uint64 minGCI = m_known_gci[m_min_gci_index];
    ndbout_c("bucket == 0, gci: %u/%u minGCI: %u/%u m_latestGCI: %u/%u",
             Uint32(gci >> 32), Uint32(gci),
             Uint32(minGCI >> 32), Uint32(minGCI),
             Uint32(m_latestGCI >> 32), Uint32(m_latestGCI));
    ndbout << " complete: " << m_complete_data << endl;
    for(Uint32 i = 0; i<m_active_gci.size(); i++)
    {
      if (((Gci_container*)(&m_active_gci[i]))->m_gci)
        ndbout << i << " - " << m_active_gci[i] << endl;
    }
#endif
    }
    else
    {
      DBUG_PRINT_EVENT("info", ("bucket == 0 due to an ongoing gap, completed epoch: %u/%u (%llu)",
                                Uint32(gci >> 32), Uint32(gci), gci));
    }
    DBUG_VOID_RETURN_EVENT;
  }

  if (rep->flags & SubGcpCompleteRep::SUB_DATA_STREAMS_IN_SIGNAL)
  {
    Uint32 already_counted = 0;
    for(Uint32 i = 0; i < cnt; i ++)
    {
      Uint16 sub_data_stream;
      if ((i & 1) == 0)
      {
        sub_data_stream = rep->sub_data_streams[i / 2] & 0xFFFF;
      }
      else
      {
        sub_data_stream = (rep->sub_data_streams[i / 2] >> 16);
      }
      Uint32 sub_data_stream_number = find_sub_data_stream_number(sub_data_stream);
      if (bucket->m_gcp_complete_rep_sub_data_streams.get(sub_data_stream_number))
      {
        // Received earlier. This must be a duplicate from the takeover node.
        already_counted ++;
      }
      else
      {
        bucket->m_gcp_complete_rep_sub_data_streams.set(sub_data_stream_number);
      }
    }
    assert(already_counted <= cnt);
    if (already_counted <= cnt)
    {
      cnt -= already_counted;
      if (cnt == 0)
      {
        // All sub data streams are already reported as completed for epoch
        // So data for all streams reported in this signal have been sent
        // twice but from two different nodes.  Ignore this duplicate report.
        DBUG_VOID_RETURN_EVENT;
      }
    }
  }

  if (rep->flags & SubGcpCompleteRep::MISSING_DATA)
  {
    bucket->m_state = Gci_container::GC_INCONSISTENT;
  }

  Uint32 old_cnt = bucket->m_gcp_complete_rep_count;
  if(unlikely(old_cnt == ~(Uint32)0))
  {
    old_cnt = m_total_buckets;
  }
  
  if (unlikely(! (old_cnt >= cnt)))
  {
    crash_on_invalid_SUB_GCP_COMPLETE_REP(bucket, rep, len, old_cnt, cnt);
  }
  bucket->m_gcp_complete_rep_count = old_cnt - cnt;
  
  if(old_cnt == cnt)
  {
    Uint64 minGCI = m_known_gci[m_min_gci_index];
    if(likely(minGCI == 0 || gci == minGCI))
    {
  do_complete:
      m_startup_hack = false;
      bool gapBegins = false;

      // if there is a gap, mark the gap boundary
      if (m_event_buffer_manager.onEpochCompleted(gci, gapBegins))
        reportStatus();

      // if a new gap begins, mark the bucket.
      if (gapBegins)
        bucket->m_state |= Gci_container::GC_OUT_OF_MEMORY;

      complete_bucket(bucket);
      m_latestGCI = m_complete_data.m_gci = gci; // before reportStatus
      reportStatus();
      
      if(unlikely(m_latest_complete_GCI > gci))
      {
	complete_outof_order_gcis();
      }
    }
    else
    {
      if (unlikely(m_startup_hack))
      {
        flushIncompleteEvents(gci);
        bucket = find_bucket(gci);
        assert(bucket);
        assert(bucket->m_gci == gci);
        goto do_complete;
      }
      /** out of order something */
      g_eventLogger->info("out of order bucket: %d gci: %u/%u minGCI: %u/%u m_latestGCI: %u/%u",
                          (int)(bucket-(Gci_container*)m_active_gci.getBase()),
                          Uint32(gci >> 32), Uint32(gci),
                          Uint32(minGCI >> 32), Uint32(minGCI),
                          Uint32(m_latestGCI >> 32), Uint32(m_latestGCI));
      bucket->m_state = Gci_container::GC_COMPLETE;
      if (gci > m_latest_complete_GCI)
        m_latest_complete_GCI = gci;
    }
  }
  
  DBUG_VOID_RETURN_EVENT;
}

void
NdbEventBuffer::complete_outof_order_gcis()
{
#ifdef VM_TRACE
  verify_known_gci(false);
#endif

  Uint64 * array = m_known_gci.getBase();
  Uint32 mask = m_known_gci.size() - 1;
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;
  Uint64 stop_gci = m_latest_complete_GCI;

  Uint64 start_gci = array[minpos];
  g_eventLogger->info("complete_outof_order_gcis from: %u/%u(%u) to: %u/%u(%u)",
                      Uint32(start_gci >> 32), Uint32(start_gci), minpos,
                      Uint32(stop_gci >> 32), Uint32(stop_gci), maxpos);

  assert(start_gci <= stop_gci);
  do
  {
    start_gci = array[minpos];
    Gci_container* bucket = find_bucket(start_gci);
    assert(bucket);
    assert(maxpos == m_max_gci_index);
    if (!(bucket->m_state & Gci_container::GC_COMPLETE)) // Not complete
    {
#ifdef VM_TRACE
      verify_known_gci(false);
#endif
      return;
    }

#ifdef VM_TRACE
    ndbout_c("complete_outof_order_gcis - completing %u/%u rows: %u",
             Uint32(start_gci >> 32), Uint32(start_gci), bucket->m_data.m_count);
#else
    ndbout_c("complete_outof_order_gcis - completing %u/%u",
             Uint32(start_gci >> 32), Uint32(start_gci));
#endif
    
    complete_bucket(bucket);
    m_latestGCI = m_complete_data.m_gci = start_gci;

#ifdef VM_TRACE
    verify_known_gci(true);
#endif
    minpos = (minpos + 1) & mask;
  } while (start_gci != stop_gci);
}

void
NdbEventBuffer::insert_event(NdbEventOperationImpl* impl,
                             SubTableData &data,
                             LinearSectionPtr *ptr,
                             Uint32 &oid_ref)
{
  DBUG_PRINT("info", ("gci{hi/lo}: %u/%u 0x%x %s",
                      data.gci_hi, data.gci_lo, m_ndb->getReference(),
                      m_ndb->getNdbObjectName()));
  do
  {
    if (impl->m_stop_gci == MAX_EPOCH)
    {
      oid_ref = impl->m_oid;
      insertDataL(impl, &data, SubTableData::SignalLength, ptr);
    }
    NdbEventOperationImpl* blob_op = impl->theBlobOpList;
    while (blob_op != NULL)
    {
      if (blob_op->m_stop_gci == MAX_EPOCH)
      {
        oid_ref = blob_op->m_oid;
        insertDataL(blob_op, &data, SubTableData::SignalLength, ptr);
      }
      blob_op = blob_op->m_next;
    }
  } while((impl = impl->m_next));
}

bool
NdbEventBuffer::find_max_known_gci(Uint64 * res) const
{
  const Uint64 * array = m_known_gci.getBase();
  Uint32 mask = m_known_gci.size() - 1;
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;

  if (minpos == maxpos)
    return false;

  if (res)
  {
    * res = array[(maxpos - 1) & mask];
  }

  return true;
}

void
NdbEventBuffer::handle_change_nodegroup(const SubGcpCompleteRep* rep)
{
  const Uint64 gci = (Uint64(rep->gci_hi) << 32) | rep->gci_lo;
  const Uint32 cnt = (rep->flags >> 16);
  const Uint64 *const array = m_known_gci.getBase();
  const Uint32 mask = m_known_gci.size() - 1;
  const Uint32 minpos = m_min_gci_index;
  const Uint32 maxpos = m_max_gci_index;

  if (rep->flags & SubGcpCompleteRep::ADD_CNT)
  {
    ndbout_c("handle_change_nodegroup(add, cnt=%u,gci=%u/%u)",
             cnt, Uint32(gci >> 32), Uint32(gci));

    Uint32 found = 0;
    Uint32 pos = minpos;
    for (; pos != maxpos; pos = (pos + 1) & mask)
    {
      if (array[pos] == gci)
      {
        Gci_container* tmp = find_bucket(array[pos]);
        if (tmp->m_state & Gci_container::GC_CHANGE_CNT)
        {
          found = 1;
          ndbout_c(" - gci %u/%u already marked complete",
                   Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
          break;
        }
        else
        {
          found = 2;
          ndbout_c(" - gci %u/%u marking (and increasing)",
                   Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
          tmp->m_state |= Gci_container::GC_CHANGE_CNT;
          tmp->m_gcp_complete_rep_count += cnt;
          break;
        }
      }
      else
      {
        ndbout_c(" - ignore %u/%u",
                 Uint32(array[pos] >> 32), Uint32(array[pos]));
      }
    }

    if (found == 0)
    {
      ndbout_c(" - NOT FOUND (total: %u cnt: %u)", m_total_buckets, cnt);
      return;
    }

    if (found == 1)
    {
      return; // Nothing todo
    }

    m_total_buckets += cnt;

    /* ADD_CNT make any out of order buckets incomplete */
    m_latest_complete_GCI = 0;

    /* Adjust expected 'complete_rep_count' for any buckets arrived OOO */
    pos = (pos + 1) & mask;
    for (; pos != maxpos; pos = (pos + 1) & mask)
    {
      assert(array[pos] > gci);
      Gci_container* tmp = find_bucket(array[pos]);
      assert((tmp->m_state & Gci_container::GC_CHANGE_CNT) == 0);
      tmp->m_gcp_complete_rep_count += cnt;
      tmp->m_state &= ~Gci_container::GC_COMPLETE; //If 'complete', undo it
      ndbout_c(" - increasing cnt on %u/%u by %u",
               Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci), cnt);
    }
  }
  else if (rep->flags & SubGcpCompleteRep::SUB_CNT)
  {
    ndbout_c("handle_change_nodegroup(sub, cnt=%u,gci=%u/%u)",
             cnt, Uint32(gci >> 32), Uint32(gci));

    Uint32 found = 0;
    Uint32 pos = minpos;
    for (; pos != maxpos; pos = (pos + 1) & mask)
    {
      if (array[pos] == gci)
      {
        Gci_container* tmp = find_bucket(array[pos]);
        if (tmp->m_state & Gci_container::GC_CHANGE_CNT)
        {
          found = 1;
          ndbout_c(" - gci %u/%u already marked complete",
                   Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
          break;
        }
        else
        {
          found = 2;
          ndbout_c(" - gci %u/%u marking",
                   Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
          tmp->m_state |= Gci_container::GC_CHANGE_CNT;
          break;
        }
      }
      else
      {
        ndbout_c(" - ignore %u/%u",
                 Uint32(array[pos] >> 32), Uint32(array[pos]));
      }
    }

    if (found == 0)
    {
      ndbout_c(" - NOT FOUND");
      return;
    }

    if (found == 1)
    {
      return; // Nothing todo
    }

    m_total_buckets -= cnt;

    /* Adjust expected 'complete_rep_count' for any buckets arrived out of order */
    pos = (pos + 1) & mask;
    for (; pos != maxpos; pos = (pos + 1) & mask)
    {
      assert(array[pos] > gci);
      Gci_container* tmp = find_bucket(array[pos]);
      assert((tmp->m_state & Gci_container::GC_CHANGE_CNT) == 0);
      assert((tmp->m_state & Gci_container::GC_COMPLETE) == 0);
      assert(tmp->m_gcp_complete_rep_count >= cnt);
      tmp->m_gcp_complete_rep_count -= cnt;
      ndbout_c(" - decreasing cnt on %u/%u by %u to: %u",
               Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci), 
               cnt,
               tmp->m_gcp_complete_rep_count);
      if (tmp->m_gcp_complete_rep_count == 0)
      {
        ndbout_c("   completed out of order %u/%u",
                 Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
        tmp->m_state |= Gci_container::GC_COMPLETE;
        if (array[pos] > m_latest_complete_GCI)
          m_latest_complete_GCI = array[pos];
      }
    }
  }
}

Uint16
NdbEventBuffer::find_sub_data_stream_number(Uint16 sub_data_stream)
{
  /*
   * The stream_index calculated will be the one returned unless
   * Suma have been changed to calculate stream identifiers in a
   * non compatible way.  In that case a linear search in the
   * fixed size hash table will resolve the correct index.
   */
  const Uint16 stream_index = (sub_data_stream % 256) + MAX_SUB_DATA_STREAMS_PER_GROUP * (sub_data_stream / 256 - 1);
  const Uint16 num0 = stream_index % NDB_ARRAY_SIZE(m_sub_data_streams);
  Uint32 num = num0;
  while (m_sub_data_streams[num] != sub_data_stream)
  {
    if (m_sub_data_streams[num] == 0)
    {
      m_sub_data_streams[num] = sub_data_stream;
      break;
    }
    num = (num + 1) % NDB_ARRAY_SIZE(m_sub_data_streams);
    require(num != num0);
  }
  return num;
}


/**
 * Initially we do not know the number of SUB_GCP_COMPLETE_REP 
 * to expect from the datanodes before the epoch can be considered
 * completed from all datanodes. Thus we init m_total_buckets
 * to a high initial value, and later use ::set_total_buckets()
 * to set the correct 'cnt' as recieved as part of SUB_START_CONF.
 *
 * As there is a possible race between SUB_START_CONF from SUMA and 
 * GSN_SUB_TABLE_DATA & SUB_GCP_COMPLETE_REP arriving from the
 * datanodes, we have to update any Gci_container's already
 * containing data, and possibly complete them if all 
 * SUB_GCP_COMPLETE_REP's had been received.
 */
void
NdbEventBuffer::set_total_buckets(Uint32 cnt)
{
  if (m_total_buckets == cnt)
    return;

  assert(m_total_buckets == TOTAL_BUCKETS_INIT);
  m_total_buckets = cnt;

  // The delta between initial 'unknown' and real #buckets
  const Uint32 delta = TOTAL_BUCKETS_INIT - cnt;

  const Uint64 * array = m_known_gci.getBase();
  const Uint32 mask = m_known_gci.size() - 1;
  const Uint32 minpos = m_min_gci_index;
  const Uint32 maxpos = m_max_gci_index;

  for (Uint32 pos = minpos; pos != maxpos; pos = (pos + 1) & mask)
  {
    const Uint64 gci = array[pos];
    Gci_container* tmp = find_bucket(gci);
    if (delta >= tmp->m_gcp_complete_rep_count)
    {
      if (0)
        ndbout_c("set_total_buckets(%u) complete %u/%u",
                 cnt, Uint32(tmp->m_gci >> 32), Uint32(tmp->m_gci));
      tmp->m_gcp_complete_rep_count = 0;
      complete_bucket(tmp);
      m_latestGCI = m_complete_data.m_gci = gci;
    }
    else
    {
      assert(tmp->m_gcp_complete_rep_count > delta);
      tmp->m_gcp_complete_rep_count -= delta;
    }
  }
}

void
NdbEventBuffer::report_node_failure_completed(Uint32 node_id)
{
  assert(node_id < 32 * m_alive_node_bit_mask.Size); // only data-nodes
  if (! (node_id < 32 * m_alive_node_bit_mask.Size))
    return;

  m_alive_node_bit_mask.clear(node_id);

  NdbEventOperation* op= m_ndb->getEventOperation(0);
  if (op == 0)
    return;

  DBUG_ENTER("NdbEventBuffer::report_node_failure_completed");
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
  data.flags = SubTableData::LOG;

  Uint64 gci = Uint64((m_latestGCI >> 32) + 1) << 32;
  find_max_known_gci(&gci);

  data.gci_hi = Uint32(gci >> 32);
  data.gci_lo = Uint32(gci);

  /**
   * Insert this event for each operation
   */
  // no need to lock()/unlock(), receive thread calls this
  insert_event(&op->m_impl, data, ptr, data.senderData);

  if (!m_alive_node_bit_mask.isclear())
    DBUG_VOID_RETURN;

  /*
   * Cluster failure
   */

  DBUG_PRINT("info", ("Cluster failure 0x%x %s", m_ndb->getReference(),
                      m_ndb->getNdbObjectName()));

  gci = Uint64((m_latestGCI >> 32) + 1) << 32;
  bool found = find_max_known_gci(&gci);

  Uint64 * array = m_known_gci.getBase();
  Uint32 mask = m_known_gci.size() - 1;
  Uint32 minpos = m_min_gci_index;
  Uint32 maxpos = m_max_gci_index;

  /**
   * Incompleted and/or 'out-of-order' Gci_containers should be cleared after
   * a failure. (Nothing more will ever arrive for whatever remaing there)
   * Temporary keep the last one, the failure-event will complete it.
   */
  while (minpos != maxpos && array[minpos] != gci)
  {
    Gci_container* tmp = find_bucket(array[minpos]);
    assert(tmp);
    assert(maxpos == m_max_gci_index);
    free_list(tmp->m_data);
    bzero(tmp, sizeof(Gci_container));

    minpos = (minpos + 1) & mask;
  }
  m_min_gci_index = minpos;
  m_latest_complete_GCI = 0; //Cleared any 'out of order' epoch

  if (found)
  {
    assert(((minpos + 1) & mask) == maxpos);
  }
  else
  {
    assert(minpos == maxpos);
  }

  /**
   * Inject new event
   */
  data.tableId = ~0;
  data.requestInfo = 0;
  SubTableData::setOperation(data.requestInfo,
			     NdbDictionary::Event::_TE_CLUSTER_FAILURE);

  /**
   * Insert this event for each operation
   */
  // no need to lock()/unlock(), receive thread calls this
  insert_event(&op->m_impl, data, ptr, data.senderData);

  /**
   * Mark that event buffer is containing a failure event
   */
  m_failure_detected= true;

#ifdef VM_TRACE
  m_flush_gci = 0;
#endif
  
  /**
   * And finally complete this GCI
   */
  Gci_container* tmp = find_bucket(gci);
  assert(tmp);
  if (found)
  {
    assert(m_max_gci_index == maxpos); // shouldnt have changed...
  }
  else
  {
    assert(m_max_gci_index == ((maxpos + 1) & mask));
  }
  Uint32 cnt = tmp->m_gcp_complete_rep_count;
  
  SubGcpCompleteRep rep;
  rep.gci_hi= (Uint32)(gci >> 32);
  rep.gci_lo= (Uint32)(gci & 0xFFFFFFFF);
  rep.gcp_complete_rep_count= cnt;
  rep.flags = 0;
  execSUB_GCP_COMPLETE_REP(&rep, SubGcpCompleteRep::SignalLength, 1);

  /**
   * We have now cleaned up all Gci_containers which were
   * incomplete at time of failure, assert that.
   * As the failure possible resets the GCI-sequence, we 
   * do the same to avoid false duplicate rejection.
   */
  //init_gci_containers(); //Known to already be empty
  assert(m_min_gci_index == m_max_gci_index);
  assert(m_latest_complete_GCI == 0);
  m_latestGCI = 0;

  m_epoch_generation++;
  DBUG_VOID_RETURN;
}

Uint64
NdbEventBuffer::getLatestGCI()
{
  /*
   * TODO: Fix data race with m_latestGCI.
   * m_latestGCI is changed by receiver thread, and getLatestGCI
   * is called from application thread.
   */
  return m_latestGCI;
}

Uint64
NdbEventBuffer::getHighestQueuedEpoch()
{
  return m_latest_poll_GCI.getGCI();
}

int
NdbEventBuffer::insertDataL(NdbEventOperationImpl *op,
			    const SubTableData * const sdata, 
                            Uint32 len,
			    LinearSectionPtr ptr[3])
{
  DBUG_ENTER_EVENT("NdbEventBuffer::insertDataL");
  const Uint32 ri = sdata->requestInfo;
  const Uint32 operation = SubTableData::getOperation(ri);
  Uint32 gci_hi = sdata->gci_hi;
  Uint32 gci_lo = sdata->gci_lo;

  if (unlikely(len < SubTableData::SignalLength))
  {
    gci_lo = 0;
  }

  Uint64 gci= gci_lo | (Uint64(gci_hi) << 32);
  const bool is_data_event = 
    operation < NdbDictionary::Event::_TE_FIRST_NON_DATA_EVENT;

  if (!is_data_event)
  {
    if (operation == NdbDictionary::Event::_TE_CLUSTER_FAILURE)
    {
      /*
        Mark event as stopping.  Subsequent dropEventOperation
        will add the event to the dropped list for delete
      */
      op->m_stop_gci = MonotonicEpoch(m_epoch_generation,gci);
    }
    else if (operation == NdbDictionary::Event::_TE_ACTIVE)
    {
      // internal event, do not relay to user
      DBUG_PRINT("info",
                 ("_TE_ACTIVE: m_ref_count: %u for op: %p id: %u 0x%x %s",
                  op->m_ref_count, op, SubTableData::getNdbdNodeId(ri),
                  m_ndb->getReference(), m_ndb->getNdbObjectName()));
      DBUG_RETURN_EVENT(0);
    }
    else if (operation == NdbDictionary::Event::_TE_STOP)
    {
      // internal event, do not relay to user
      DBUG_PRINT("info",
                 ("_TE_STOP: m_ref_count: %u for op: %p id: %u 0x%x %s",
                  op->m_ref_count, op, SubTableData::getNdbdNodeId(ri),
                  m_ndb->getReference(), m_ndb->getNdbObjectName()));
      DBUG_RETURN_EVENT(0);
    }
  }
  
  const Uint32 used_data_sz = (m_total_alloc - m_free_data_sz);
  const Uint32 memory_usage = (m_max_alloc == 0) ? 0 :
    (Uint32)((100 * (Uint64)used_data_sz) / m_max_alloc);

  if (m_event_buffer_manager.onEventDataReceived(memory_usage, gci))
    reportStatus(true/*force_report*/);

  if (m_event_buffer_manager.isEventDataToBeDiscarded(gci))
  {
    DBUG_RETURN_EVENT(0);
  }

  if ( likely((Uint32)op->mi_type & (1U << operation)))
  {
    Gci_container* bucket= find_bucket(gci);
    
    DBUG_PRINT_EVENT("info", ("data insertion in eventId %d 0x%x %s",
                              op->m_eventId, m_ndb->getReference(),
                              m_ndb->getNdbObjectName()));
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
      DBUG_PRINT_EVENT("info", ("ignore non-data event on blob table 0x%x %s",
                                m_ndb->getReference(), m_ndb->getNdbObjectName()));
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
      data = alloc_data();  // alloc_data crashes if allocation fails.

      m_event_buffer_manager.onBufferingEpoch(gci);

      // Initialize m_event_op, in case copy_data fails due to insufficient memory
      data->m_event_op = 0;
      if (unlikely(copy_data(sdata, len, ptr, data, NULL)))
      {
        crashMemAllocError("insertDataL : copy_data failed.");
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
          crashMemAllocError("insertDataL : get_main_data failed.");
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
      if (unlikely(merge_data(sdata, len, ptr, data, &bucket->m_data.m_sz)))
      {
        crashMemAllocError("insertDataL : merge_data failed.");
      }

      // merge is on so we do not report blob part events
      if (! is_blob_event) {
        // report actual operation and the composite
        // there is no way to "fix" the flags for a composite op
        // since the flags represent multiple ops on multiple PKs
        // XXX fix by doing merge at end of epoch (extra mem cost)
        {
          Uint32 any_value = sdata->anyValue;
          EventBufData_list::Gci_op g = { op, (1U << operation), any_value };
          bucket->m_data.add_gci_op(g);
        }
        {
          Uint32 any_value = data->sdata->anyValue;
          EventBufData_list::Gci_op 
	    g = { op, 
		  (1U << SubTableData::getOperation(data->sdata->requestInfo)), any_value};
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
  if ((Uint32)op->m_eventImpl->mi_type & (1U << operation))
  {
    DBUG_PRINT_EVENT("info",("Data arrived before ready eventId %d 0x%x %s",
                             op->m_eventId, m_ndb->getReference(),
                             m_ndb->getNdbObjectName()));
    DBUG_RETURN_EVENT(0);
  }
  else {
    DBUG_PRINT_EVENT("info",("skipped 0x%x %s", m_ndb->getReference(),
                             m_ndb->getNdbObjectName()));
    DBUG_RETURN_EVENT(0);
  }
#else
  DBUG_RETURN_EVENT(0);
#endif
}

void
NdbEventBuffer::crashMemAllocError(const char *error_text)
{
  g_eventLogger->error("Ndb Event Buffer 0x%x %s", m_ndb->getReference(),
	  m_ndb->getNdbObjectName());
  g_eventLogger->error("Ndb Event Buffer : %s", error_text);
  g_eventLogger->error("Ndb Event Buffer : Fatal error.");
  exit(-1);
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
      printf("m_latest_command: %s 0x%x %s\n",
             m_latest_command, m_ndb->getReference(), m_ndb->getNdbObjectName());
      printf("no free data, m_latestGCI %u/%u\n",
             (Uint32)(m_latestGCI << 32), (Uint32)m_latestGCI);
      printf("m_free_data_count %d\n", m_free_data_count);
      printf("m_available_data_count %d first gci{hi/lo} %u/%u last gci{hi/lo} %u/%u\n",
             m_available_data.m_count,
             m_available_data.m_head?m_available_data.m_head->sdata->gci_hi:0,
             m_available_data.m_head?m_available_data.m_head->sdata->gci_lo:0,
             m_available_data.m_tail?m_available_data.m_tail->sdata->gci_hi:0,
             m_available_data.m_tail?m_available_data.m_tail->sdata->gci_lo:0);
      printf("m_used_data_count %d\n", m_used_data.m_count);
#endif
      crashMemAllocError("alloc_data : Allocation of meta data failed.");
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
  DBUG_PRINT("info", ("ptr sz %u + %u + %u 0x%x %s",
                      ptr[0].sz, ptr[1].sz, ptr[2].sz, m_ndb->getReference(),
                      m_ndb->getNdbObjectName()));
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
    data->memory = 0;

    data->memory = (Uint32*)NdbMem_Allocate(alloc_size);
    if (data->memory == 0)
    {
      goto out_of_mem_err;
    }
    data->sz = alloc_size;
    m_total_alloc += add_sz;

    if (change_sz != NULL)
      *change_sz += add_sz;
  }
  {
  Uint32* memptr = data->memory;
  memptr += sz4;
  int i;
  for (i = 0; i <= 2; i++)
  {
    data->ptr[i].p = memptr;
    data->ptr[i].sz = ptr[i].sz;
    memptr += ptr[i].sz;
  }
  }
  DBUG_RETURN(0);

out_of_mem_err:
  // Dealloc succeeded, but alloc bigger size failed
  crashMemAllocError("Attempt to allocate memory from OS failed");
  DBUG_RETURN(-1);
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
NdbEventBuffer::copy_data(const SubTableData * const sdata, Uint32 len,
                          LinearSectionPtr ptr[3],
                          EventBufData* data,
                          Uint32 * change_sz)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::copy_data");

  if (alloc_mem(data, ptr, change_sz) != 0)
    DBUG_RETURN_EVENT(-1);
  memcpy(data->sdata, sdata, sizeof(SubTableData));

  if (unlikely(len < SubTableData::SignalLength))
  {
    data->sdata->gci_lo = 0;
  }
  if (len < SubTableData::SignalLengthWithTransId)
  {
    /* No TransId, set to uninit value */
    data->sdata->transId1 = ~Uint32(0);
    data->sdata->transId2 = ~Uint32(0);
  }

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
NdbEventBuffer::merge_data(const SubTableData * const sdata, Uint32 len,
                           LinearSectionPtr ptr2[3],
                           EventBufData* data,
                           Uint32 * change_sz)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::merge_data");

  /* TODO : Consider how/if to merge multiple events/key with different
   * transid
   * Same consideration probably applies to AnyValue!
   */

  Uint32 nkey = data->m_event_op->m_eventImpl->m_tableImpl->m_noOfKeys;

  int t1 = SubTableData::getOperation(data->sdata->requestInfo);
  int t2 = SubTableData::getOperation(sdata->requestInfo);
  if (t1 == Ev_t::enum_NUL)
    DBUG_RETURN_EVENT(copy_data(sdata, len, ptr2, data, change_sz));

  Ev_t* tp = 0;
  int i;
  for (i = 0; (uint) i < sizeof(ev_t)/sizeof(ev_t[0]); i++) {
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
        DBUG_PRINT("info", ("idempotent op %d*%d data differs in sec %d 0x%x %s",
                            tp->t1, tp->t2, i, m_ndb->getReference(),
                            m_ndb->getNdbObjectName()));
        assert(false);
        DBUG_RETURN_EVENT(-1);
      }
    }
    DBUG_PRINT("info", ("idempotent op %d*%d data ok 0x%x %s",
                        tp->t1, tp->t2, m_ndb->getReference(),
                        m_ndb->getNdbObjectName()));
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

int
NdbEventBuffer::get_main_data(Gci_container* bucket,
                              EventBufData_hash::Pos& hpos,
                              EventBufData* blob_data)
{
  DBUG_ENTER_EVENT("NdbEventBuffer::get_main_data");

  int blobVersion = blob_data->m_event_op->theBlobVersion;
  assert(blobVersion == 1 || blobVersion == 2);

  NdbEventOperationImpl* main_op = blob_data->m_event_op->theMainOp;
  assert(main_op != NULL);
  const NdbTableImpl* mainTable = main_op->m_eventImpl->m_tableImpl;

  // create LinearSectionPtr for main table key
  LinearSectionPtr ptr[3];

  Uint32 pk_ah[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
  Uint32* pk_data = blob_data->ptr[1].p;
  Uint32 pk_size = 0;

  if (unlikely(blobVersion == 1)) {
    /*
     * Blob PK attribute 0 is concatenated table PK null padded
     * to fixed maximum size.  The actual size and attributes of
     * table PK must be discovered.
     */
    Uint32 max_size = AttributeHeader(blob_data->ptr[0].p[0]).getDataSize();

    Uint32 sz = 0; // words parsed so far
    Uint32 n = 0;
    Uint32 i;
    for (i = 0; n < mainTable->m_noOfKeys; i++) {
      const NdbColumnImpl* c = mainTable->getColumn(i);
      assert(c != NULL);
      if (! c->m_pk)
        continue;

      Uint32 bytesize = c->m_attrSize * c->m_arraySize;
      Uint32 lb, len;
      require(sz < max_size);
      bool ok = NdbSqlUtil::get_var_length(c->m_type, &pk_data[sz],
                                           bytesize, lb, len);
      if (!ok)
      {
        DBUG_RETURN_EVENT(-1);
      }

      AttributeHeader ah(i, lb + len);
      pk_ah[n] = ah.m_value;
      sz += ah.getDataSize();
      n++;
    }
    assert(n == mainTable->m_noOfKeys);
    require(sz <= max_size);
    pk_size = sz;
  } else {
    /*
     * Blob PK starts with separate table PKs.  Total size must be
     * counted and blob attribute ids changed to table attribute ids.
     */
    Uint32 sz = 0; // count size
    Uint32 n = 0;
    Uint32 i;
    for (i = 0; n < mainTable->m_noOfKeys; i++) {
      const NdbColumnImpl* c = mainTable->getColumn(i);
      assert(c != NULL);
      if (! c->m_pk)
        continue;

      AttributeHeader ah(blob_data->ptr[0].p[n]);
      ah.setAttributeId(i);
      pk_ah[n] = ah.m_value;
      sz += ah.getDataSize();
      n++;
    }
    assert(n == mainTable->m_noOfKeys);
    pk_size = sz;
  }

  ptr[0].sz = mainTable->m_noOfKeys;
  ptr[0].p = pk_ah;
  ptr[1].sz = pk_size;
  ptr[1].p = pk_data;
  ptr[2].sz = 0;
  ptr[2].p = 0;

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
  if (copy_data(&sdata, SubTableData::SignalLength, ptr, main_data, NULL) != 0)
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
  DBUG_PRINT_EVENT("info", ("main_data=%p blob_data=%p 0x%x %s",
                            main_data, blob_data, m_ndb->getReference(),
                            m_ndb->getNdbObjectName()));
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

EventBufData *
NdbEventBuffer::move_data()
{
  // handle received data
  if (!m_complete_data.m_data.is_empty())
  {
    // move this list to last in m_available_data
    m_available_data.append_list(&m_complete_data.m_data, NULL);

    bzero(&m_complete_data, sizeof(m_complete_data));
  }

  // return m_used_data to m_free_data
  free_list(m_used_data);

  if (!m_available_data.is_empty())
  {
    DBUG_ENTER_EVENT("NdbEventBuffer::move_data");
#ifdef VM_TRACE
    DBUG_PRINT_EVENT("exit",("m_available_data_count %u 0x%x %s",
                             m_available_data.m_count,
                             m_ndb->getReference(), m_ndb->getNdbObjectName()));
#endif
    DBUG_RETURN_EVENT(m_available_data.m_head);
  }
  return 0;
}

void
NdbEventBuffer::free_list(EventBufData_list &list)
{
  if (list.m_head != NULL)
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
  list.delete_gci_ops(); // free gci ops
}

void EventBufData_list::append_list(EventBufData_list *list,
                                    const MonotonicEpoch *epoch)
{
#ifdef NDB_EVENT_VERIFY_SIZE
  NdbEventBuffer::verify_size(*list);
#endif
  move_gci_ops(list, epoch);

  if (m_tail)
    m_tail->m_next= list->m_head;
  else
    m_head= list->m_head;
  m_tail= list->m_tail;
  m_count+= list->m_count;
  m_sz+= list->m_sz;

  list->m_head = list->m_tail = NULL;
  list->m_count = list->m_sz = 0;
}

void
EventBufData_list::add_gci_op(Gci_op g)
{
  DBUG_ENTER_EVENT("EventBufData_list::add_gci_op");
  DBUG_PRINT_EVENT("info", ("p.op: %p  g.event_types: %x g.cumulative_any_value: %x", g.op, g.event_types, g.cumulative_any_value));
  assert(g.op != NULL && g.op->theMainOp == NULL); // as in nextEvent
  Uint32 i;
  for (i = 0; i < m_gci_op_count; i++) {
    if (m_gci_op_list[i].op == g.op)
      break;
  }
  if (i < m_gci_op_count) {
    m_gci_op_list[i].event_types |= g.event_types;
    m_gci_op_list[i].cumulative_any_value &= g.cumulative_any_value;
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
      else
        assert(old_list == 0);
      DBUG_PRINT_EVENT("info", ("this: %p  new m_gci_op_list: %p",
                                this, m_gci_op_list));
      m_gci_op_alloc = n;
    }
    assert(m_gci_op_count < m_gci_op_alloc);
#ifndef DBUG_OFF
    i = m_gci_op_count;
#endif
    m_gci_op_list[m_gci_op_count++] = g;
  }
  DBUG_PRINT_EVENT("exit", ("m_gci_op_list[%u].event_types: %x", i, m_gci_op_list[i].event_types));
  DBUG_VOID_RETURN_EVENT;
}

void
EventBufData_list::move_gci_ops(EventBufData_list *list,
                                const MonotonicEpoch *gci)
{
  DBUG_ENTER_EVENT("EventBufData_list::move_gci_ops");
  DBUG_PRINT_EVENT("info", ("this: %p  list: %p  gci: %u/%u",
                            this, list, (Uint32)(gci >> 32), (Uint32)gci));
  assert(!m_is_not_multi_list);
  if (!list->m_is_not_multi_list)
  {
    assert(gci == NULL);
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
    assert(gci != NULL);
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
    new_gci_ops->m_gci = *gci;
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

  if (m_ndb->theImpl->m_ev_op == NULL)
  {
    //Any buffered events should have been discarded
    //when we dropped last event op - Prior to this create:
    assert(m_available_data.is_empty());
    assert(m_available_data.first_gci_ops() == NULL);
  }

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
  // add user reference
  // removed in dropEventOperation
  getEventOperationImpl(tOp)->m_ref_count = 1;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p 0x%x %s",
                      getEventOperationImpl(tOp)->m_ref_count,
                      getEventOperationImpl(tOp), m_ndb->getReference(),
                      m_ndb->getNdbObjectName()));
  DBUG_RETURN(tOp);
}

NdbEventOperationImpl*
NdbEventBuffer::createEventOperationImpl(NdbEventImpl& evnt,
                                         NdbError &theError)
{
  DBUG_ENTER("NdbEventBuffer::createEventOperationImpl");
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
    MonotonicEpoch max_stop_gci = op->m_stop_gci;
    NdbEventOperationImpl* tBlobOp = op->theBlobOpList;
    while (tBlobOp != NULL)
    {
      tBlobOp->stop();
      MonotonicEpoch stop_gci = tBlobOp->m_stop_gci;
      if (stop_gci > max_stop_gci)
        max_stop_gci = stop_gci;
      tBlobOp = tBlobOp->m_next;
    }
    tBlobOp = op->theBlobOpList;
    while (tBlobOp != NULL)
    {
      tBlobOp->m_stop_gci = max_stop_gci;
      tBlobOp = tBlobOp->m_next;
    }
    op->m_stop_gci = max_stop_gci;
  }

  /**
   * Needs mutex lock as report_node_XXX accesses list...
   */
  NdbMutex_Lock(m_mutex);

  // release blob handles now, further access is user error
  if (op->theMainOp == NULL)
  {
    while (op->theBlobList != NULL)
    {
      NdbBlob* tBlob = op->theBlobList;
      op->theBlobList = tBlob->theNext;
      m_ndb->releaseNdbBlob(tBlob);
    }
  }

  if (op->m_next)
    op->m_next->m_prev= op->m_prev;
  if (op->m_prev)
    op->m_prev->m_next= op->m_next;
  else
    m_ndb->theImpl->m_ev_op= op->m_next;
  
  assert(m_ndb->theImpl->m_ev_op == 0 || m_ndb->theImpl->m_ev_op->m_prev == 0);
  
  DBUG_ASSERT(op->m_ref_count > 0);
  // remove user reference
  // added in createEventOperation
  // user error to use reference after this
  op->m_ref_count--;
  DBUG_PRINT("info", ("m_ref_count: %u for op: %p 0x%x %s",
                      op->m_ref_count, op, m_ndb->getReference(),
                      m_ndb->getNdbObjectName()));
  if (op->m_ref_count == 0)
  {
    DBUG_PRINT("info", ("deleting op: %p 0x%x %s",
                        op, m_ndb->getReference(), m_ndb->getNdbObjectName()));
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

  if (m_active_op_count == 0)
  {
    /**
     * Client dropped all event operations. Thus, all buffered, polled
     * and unpolled, (completed) events can now safely be discarded.
     */
    consume_all();

    /* Clean up obsolete receiver thread data. */
    init_gci_containers();
  }

  NdbMutex_Unlock(m_mutex);
  DBUG_VOID_RETURN;
}

void
NdbEventBuffer::reportStatus(bool force_report)
{
  EventBufData *apply_buf= m_available_data.m_head;
  Uint64 apply_gci, latest_gci= m_latestGCI;
  if (apply_buf == 0)
    apply_buf= m_complete_data.m_data.m_head;
  if (apply_buf && apply_buf->sdata)
  {
    Uint32 gci_hi = apply_buf->sdata->gci_hi;
    Uint32 gci_lo = apply_buf->sdata->gci_lo;
    apply_gci= gci_lo | (Uint64(gci_hi) << 32);
  }
  else
    apply_gci= latest_gci;

  if (force_report)
      goto send_report;

  if (m_free_thresh)
  {
    if (100*(Uint64)m_free_data_sz < m_min_free_thresh*(Uint64)m_total_alloc &&
        m_total_alloc > 1024*1024)
    {
      /* report less free buffer than m_free_thresh,
         next report when more free than 2 * m_free_thresh
      */
      m_min_free_thresh= 0;
      m_max_free_thresh= 2 * m_free_thresh;
      goto send_report;
    }
  
    if (100*(Uint64)m_free_data_sz > m_max_free_thresh*(Uint64)m_total_alloc &&
        m_total_alloc > 1024*1024)
    {
      /* report more free than 2 * m_free_thresh
         next report when less free than m_free_thresh
      */
      m_min_free_thresh= m_free_thresh;
      m_max_free_thresh= 100;
      goto send_report;
    }
  }
  if (m_gci_slip_thresh &&
      (latest_gci-apply_gci >= m_gci_slip_thresh))
  {
    goto send_report;
  }
  return;

send_report:
  Uint32 data[8];
  data[0]= NDB_LE_EventBufferStatus;
  data[1]= m_total_alloc-m_free_data_sz;
  data[2]= m_total_alloc;
  data[3]= m_max_alloc;
  data[4]= (Uint32)(apply_gci);
  data[5]= (Uint32)(apply_gci >> 32);
  data[6]= (Uint32)(latest_gci);
  data[7]= (Uint32)(latest_gci >> 32);
  Ndb_internal::send_event_report(true, m_ndb, data,8);
#ifdef VM_TRACE
  assert(m_total_alloc >= m_free_data_sz);
#endif
}

void
NdbEventBuffer::get_event_buffer_memory_usage(Ndb::EventBufferMemoryUsage& usage)
{
  // m_total_alloc  >= m_free_data_sz always.
  const Uint32 used_data_sz = (m_total_alloc - m_free_data_sz);

  usage.allocated_bytes = m_total_alloc;
  usage.used_bytes = used_data_sz;

  // If there's no configured max limit then
  // the percentage is a fraction of the total allocated.

  Uint32 ret = 0;
  // m_max_alloc == 0 ==> unlimited usage,
  if (m_max_alloc > 0)
    ret = (Uint32)((100 * (Uint64)used_data_sz) / m_max_alloc);
  else if (m_total_alloc > 0)
    ret = (Uint32)((100 * (Uint64)used_data_sz) / m_total_alloc);

  usage.usage_percent = ret;
}

#ifdef VM_TRACE
void
NdbEventBuffer::verify_size(const EventBufData* data, Uint32 count, Uint32 sz)
{
#if 0
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
#endif
}
void
NdbEventBuffer::verify_size(const EventBufData_list & list)
{
#if 0
  verify_size(list.m_head, list.m_count, list.m_sz);
#endif
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
    require(col != 0);

    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(col->m_type, dptr, bytesize, lb, len);
    require(ok);

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
    require(ok1 && ok2 && lb1 == lb2);

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
