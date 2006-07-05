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

#include "Suma.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>
#include <SimpleProperties.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/ListTables.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/GetTableId.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/GCPSave.hpp>
#include <GrepError.hpp>

#include <DebuggerNames.hpp>

//#define HANDOVER_DEBUG
//#define NODEFAIL_DEBUG
//#define NODEFAIL_DEBUG2
//#define DEBUG_SUMA_SEQUENCE
//#define EVENT_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG2
#if 0
#undef DBUG_ENTER
#undef DBUG_PRINT
#undef DBUG_RETURN
#undef DBUG_VOID_RETURN

#define DBUG_ENTER(a) {ndbout_c("%s:%d >%s", __FILE__, __LINE__, a);}
#define DBUG_PRINT(a,b) {ndbout << __FILE__ << ":" << __LINE__ << " " << a << ": "; ndbout_c b ;}
#define DBUG_RETURN(a) { ndbout_c("%s:%d <", __FILE__, __LINE__); return(a); }
#define DBUG_VOID_RETURN { ndbout_c("%s:%d <", __FILE__, __LINE__); return; }
#endif

/**
 * @todo:
 * SUMA crashes if an index is created at the same time as
 * global replication. Very easy to reproduce using testIndex.
 * Note: This only happens occasionally, but is quite easy to reprod.
 */

Uint32 g_subPtrI = RNIL;
static const Uint32 SUMA_SEQUENCE = 0xBABEBABE;


/**************************************************************
 *
 * Start of suma
 *
 */

#define PRINT_ONLY 0
static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

void 
Suma::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);

  // SumaParticipant
  Uint32 noTables;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES,  
			    &noTables);

  /**
   * @todo: fix pool sizes
   */
  c_tablePool_.setSize(noTables);
  c_tables.setSize(noTables);
  
  c_subscriptions.setSize(20); //10
  c_subscriberPool.setSize(64);
  
  c_subscriptionPool.setSize(64); //2
  c_syncPool.setSize(20); //2
  c_dataBufferPool.setSize(128);
  
  {
    SLList<SyncRecord> tmp(c_syncPool);
    Ptr<SyncRecord> ptr;
    while(tmp.seize(ptr))
      new (ptr.p) SyncRecord(* this, c_dataBufferPool);
    tmp.release();
  }

  // Suma
  c_nodePool.setSize(MAX_NDB_NODES);
  c_masterNodeId = getOwnNodeId();

  c_nodeGroup = c_noNodesInGroup = c_idInNodeGroup = 0;
  for (int i = 0; i < MAX_REPLICAS; i++) {
    c_nodesInGroup[i]   = 0;
  }

  c_subCoordinatorPool.setSize(10);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Suma::execSTTOR(Signal* signal) {
  jamEntry();                            

  DBUG_ENTER("Suma::execSTTOR");
  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  DBUG_PRINT("info",("startphase = %u, typeOfStart = %u", startphase, typeOfStart));

  if(startphase == 3){
    jam();
    g_TypeOfStart = typeOfStart;
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);

#if 0

    /**
     * Debug
     */

    
    SubscriptionPtr subPtr;
    Ptr<SyncRecord> syncPtr;
    ndbrequire(c_subscriptions.seize(subPtr));
    ndbrequire(c_syncPool.seize(syncPtr));
    

    ndbout_c("Suma: subPtr.i = %d syncPtr.i = %d", subPtr.i, syncPtr.i);

    subPtr.p->m_syncPtrI = syncPtr.i;
    subPtr.p->m_subscriptionType = SubCreateReq::DatabaseSnapshot;
    syncPtr.p->m_subscriptionPtrI = subPtr.i;
    syncPtr.p->ptrI = syncPtr.i;
    g_subPtrI = subPtr.i;
    //    sendSTTORRY(signal);
#endif    
    DBUG_VOID_RETURN;
  }

  if(startphase == 7) {
    if(g_TypeOfStart == NodeState::ST_INITIAL_START &&
       c_masterNodeId == getOwnNodeId()) {
      jam();
      createSequence(signal);
      DBUG_VOID_RETURN;
    }//if
  }//if
  

  sendSTTORRY(signal);
  
  DBUG_VOID_RETURN;
}

void
Suma::createSequence(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::createSequence");

  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = SUMA_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
  // execUTIL_SEQUENCE_CONF will call createSequenceReply()
  DBUG_VOID_RETURN;
}

void
Suma::createSequenceReply(Signal* signal,
			  UtilSequenceConf * conf,
			  UtilSequenceRef * ref)
{
  jam();

  if (ref != NULL)
    ndbrequire(false);

  sendSTTORRY(signal);
}

void
Suma::execREAD_NODESCONF(Signal* signal){
  jamEntry();
  ReadNodesConf * const conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();
  c_preparingNodes.clear();

  Uint32 count = 0;
  for(Uint32 i = 0; i < MAX_NDB_NODES; i++){
    if(NodeBitmask::get(conf->allNodes, i)){
      jam();
      
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seize(node));
      
      node.p->nodeId = i;
      if(NodeBitmask::get(conf->inactiveNodes, i)){
	jam();
	node.p->alive = 0;
      } else {
	jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }
    } else
      jam();
  }
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);

  sendSTTORRY(signal);
}

void
Suma::sendSTTORRY(Signal* signal){
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 5;
  signal->theData[6] = 7;
  signal->theData[7] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 8, JBB);
}

void
Suma::execNDB_STTOR(Signal* signal) 
{
  jamEntry();                            
}

void
Suma::execCONTINUEB(Signal* signal){
  jamEntry();
}

void
SumaParticipant::execCONTINUEB(Signal* signal) 
{
  jamEntry();
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/

void
Suma::execSIGNAL_DROPPED_REP(Signal* signal){
  jamEntry();
  ndbrequire(false);
}

/********************************************************************
 *
 * Dump state
 *
 */

static unsigned
count_subscribers(const DLList<SumaParticipant::Subscriber> &subs)
{
  unsigned n= 0;
  SumaParticipant::SubscriberPtr i_subbPtr;
  subs.first(i_subbPtr);
  while(!i_subbPtr.isNull()){
    n++;
    subs.next(i_subbPtr);
  }
  return n;
}

void
Suma::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();

  Uint32 tCase = signal->theData[0];
  if(tCase >= 8000 && tCase <= 8003){
    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, g_subPtrI);
    
    Ptr<SyncRecord> syncPtr;
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
    
    if(tCase == 8000){
      syncPtr.p->startMeta(signal);
    }
    
    if(tCase == 8001){
      syncPtr.p->startScan(signal);
    }

    if(tCase == 8003){
      subPtr.p->m_subscriptionType = SubCreateReq::SingleTableScan;
      LocalDataBuffer<15> attrs(c_dataBufferPool, syncPtr.p->m_attributeList);
      Uint32 tab = 0;
      Uint32 att[] = { 0, 1, 1 };
      syncPtr.p->m_tableList.append(&tab, 1);
      attrs.append(att, 3);
    }
  }

  if(tCase == 8004){
    infoEvent("Suma: c_subscriberPool  size: %d free: %d",
	      c_subscriberPool.getSize(),
	      c_subscriberPool.getNoOfFree());

    infoEvent("Suma: c_tablePool  size: %d free: %d",
	      c_tablePool_.getSize(),
	      c_tablePool_.getNoOfFree());

    infoEvent("Suma: c_subscriptionPool  size: %d free: %d",
	      c_subscriptionPool.getSize(),
	      c_subscriptionPool.getNoOfFree());

    infoEvent("Suma: c_syncPool  size: %d free: %d",
	      c_syncPool.getSize(),
	      c_syncPool.getNoOfFree());

    infoEvent("Suma: c_dataBufferPool  size: %d free: %d",
	      c_dataBufferPool.getSize(),
	      c_dataBufferPool.getNoOfFree());

    infoEvent("Suma: c_metaSubscribers count: %d",
	      count_subscribers(c_metaSubscribers));
    infoEvent("Suma: c_dataSubscribers count: %d",
	      count_subscribers(c_dataSubscribers));
    infoEvent("Suma: c_prepDataSubscribers count: %d",
	      count_subscribers(c_prepDataSubscribers));
    infoEvent("Suma: c_removeDataSubscribers count: %d",
	      count_subscribers(c_removeDataSubscribers));
  }
}

/********************************************************************
 *
 * Convert a table name (db+schema+tablename) to tableId
 *
 */

#if 0
void
SumaParticipant::convertNameToId(SubscriptionPtr subPtr, Signal * signal)
{
  jam();
  if(subPtr.p->m_currentTable < subPtr.p->m_maxTables) {
    jam();

    GetTableIdReq * req = (GetTableIdReq *)signal->getDataPtrSend();
    char * tableName = subPtr.p->m_tableNames[subPtr.p->m_currentTable];
    const Uint32 strLen = strlen(tableName) + 1; // NULL Terminated
    req->senderRef  = reference();
    req->senderData = subPtr.i;
    req->len        = strLen;

    LinearSectionPtr ptr[1];
    ptr[0].p  = (Uint32*)tableName;
    ptr[0].sz = strLen;

    sendSignal(DBDICT_REF,
	       GSN_GET_TABLEID_REQ, 
	       signal, 
	       GetTableIdReq::SignalLength,
	       JBB,
	       ptr,
	       1);
  } else {
    jam();
    sendSubCreateConf(signal, subPtr.p->m_subscriberRef, subPtr);
  }
}
#endif


void 
SumaParticipant::addTableId(Uint32 tableId,
			    SubscriptionPtr subPtr, SyncRecord *psyncRec)
{
#ifdef NODEFAIL_DEBUG
  ndbout_c("SumaParticipant::addTableId(%u,%u,%u), current_table=%u",
	   tableId, subPtr.i, psyncRec, subPtr.p->m_currentTable);
#endif
  subPtr.p->m_tables[tableId] = 1;
  subPtr.p->m_currentTable++;
  if(psyncRec != NULL)
    psyncRec->m_tableList.append(&tableId, 1);  
}

#if 0
void 
SumaParticipant::execGET_TABLEID_CONF(Signal * signal)
{
  jamEntry();

  GetTableIdConf* conf = (GetTableIdConf *)signal->getDataPtr();
  Uint32 tableId = conf->tableId;
  //Uint32 schemaVersion = conf->schemaVersion;  
  Uint32 senderData = conf->senderData;

  SubscriptionPtr subPtr;
  Ptr<SyncRecord> syncPtr;

  c_subscriptions.getPtr(subPtr, senderData);
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);  

  /*
   * add to m_tableList
   */
  addTableId(tableId, subPtr, syncPtr.p);

  convertNameToId(subPtr, signal);
}

void 
SumaParticipant::execGET_TABLEID_REF(Signal * signal)
{
  jamEntry();
  GetTableIdRef const * ref = (GetTableIdRef *)signal->getDataPtr();
  Uint32 senderData         = ref->senderData;
  //  Uint32 err                = ref->err;
  
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, senderData);
  Uint32 subData = subPtr.p->m_subscriberData;
  SubCreateRef * reff = (SubCreateRef*)ref;
  /**
   * @todo: map ref->err to GrepError.
   */
  reff->err = GrepError::SELECTED_TABLE_NOT_FOUND;
  reff->subscriberData = subData;
  sendSignal(subPtr.p->m_subscriberRef,
	     GSN_SUB_CREATE_REF, 
	     signal, 
	     SubCreateRef::SignalLength,
	     JBB);
}
#endif


/*************************************************************
 *
 * Creation of subscription id's
 *
 ************************************************************/

void 
Suma::execCREATE_SUBID_REQ(Signal* signal) 
{
  jamEntry();

  CRASH_INSERTION(13001);

  CreateSubscriptionIdReq const * req =
    (CreateSubscriptionIdReq*)signal->getDataPtr();
  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr)){
    jam();
    sendSubIdRef(signal, GrepError::SUBSCRIPTION_ID_NOMEM);
    return;
  }

  subbPtr.p->m_subscriberRef  = signal->getSendersBlockRef(); 
  subbPtr.p->m_senderData     = req->senderData;
  subbPtr.p->m_subscriberData = subbPtr.i;

  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
   
  utilReq->senderData  = subbPtr.p->m_subscriberData;
  utilReq->sequenceId  = SUMA_SEQUENCE;
  utilReq->requestType = UtilSequenceReq::NextVal;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Suma::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  DBUG_ENTER("Suma::execUTIL_SEQUENCE_CONF");
  CRASH_INSERTION(13002);

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  if(conf->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, conf, NULL);
    DBUG_VOID_RETURN;
  }

  Uint64 subId;
  memcpy(&subId,conf->sequenceValue,8);
  Uint32 subData = conf->senderData;

  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,subData);
  

  CreateSubscriptionIdConf * subconf = (CreateSubscriptionIdConf*)conf;
  subconf->subscriptionId = (Uint32)subId;
  subconf->subscriptionKey =(getOwnNodeId() << 16) | (Uint32)(subId & 0xFFFF);
  subconf->subscriberData = subbPtr.p->m_senderData;
  
  sendSignal(subbPtr.p->m_subscriberRef, GSN_CREATE_SUBID_CONF, signal,
	     CreateSubscriptionIdConf::SignalLength, JBB);

  c_subscriberPool.release(subbPtr);

  DBUG_VOID_RETURN;
}

void
Suma::execUTIL_SEQUENCE_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execUTIL_SEQUENCE_REF");
  UtilSequenceRef * ref = (UtilSequenceRef*)signal->getDataPtr();

  if(ref->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, NULL, ref);
    DBUG_VOID_RETURN;
  }

  Uint32 subData = ref->senderData;

  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,subData);
  sendSubIdRef(signal, GrepError::SEQUENCE_ERROR);
  c_subscriberPool.release(subbPtr);
  DBUG_VOID_RETURN;
}//execUTIL_SEQUENCE_REF()


void
SumaParticipant::sendSubIdRef(Signal* signal, Uint32 errCode){
  jam();
  CreateSubscriptionIdRef  * ref = 
    (CreateSubscriptionIdRef *)signal->getDataPtrSend();

  ref->err = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_CREATE_SUBID_REF,
	     signal, 
	     CreateSubscriptionIdRef::SignalLength,
	     JBB);
  
  releaseSections(signal);  
  return;
}

/**********************************************************
 * Suma participant interface
 *
 * Creation of subscriptions
 */

void
SumaParticipant::execSUB_CREATE_REQ(Signal* signal) {
#ifdef NODEFAIL_DEBUG
  ndbout_c("SumaParticipant::execSUB_CREATE_REQ");
#endif
  jamEntry();                            

  CRASH_INSERTION(13003);

  const SubCreateReq req = *(SubCreateReq*)signal->getDataPtr();    
  
  const Uint32 subId   = req.subscriptionId;
  const Uint32 subKey  = req.subscriptionKey;
  const Uint32 subRef  = req.subscriberRef;
  const Uint32 subData = req.subscriberData;
  const Uint32 type    = req.subscriptionType & SubCreateReq::RemoveFlags;
  const Uint32 flags   = req.subscriptionType & SubCreateReq::GetFlags;
  const bool addTableFlag = (flags & SubCreateReq::AddTableFlag) != 0;
  const bool restartFlag  = (flags & SubCreateReq::RestartFlag)  != 0;

  const Uint32 sender = signal->getSendersBlockRef();

  Subscription key;
  key.m_subscriptionId  = subId;
  key.m_subscriptionKey = subKey;

  SubscriptionPtr subPtr;
  Ptr<SyncRecord> syncPtr;
  
  if (addTableFlag) {
    ndbrequire(restartFlag);  //TODO remove this

    if(!c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::SUBSCRIPTION_NOT_FOUND);
      return;
    }
    jam();
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  } else {
    // Check that id/key is unique
    if(c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::SUBSCRIPTION_ID_NOT_UNIQUE);
      return;
    }
    if(!c_subscriptions.seize(subPtr)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::NOSPACE_IN_POOL);
      return;
    }
    if(!c_syncPool.seize(syncPtr)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::NOSPACE_IN_POOL);
      return;
    }
    jam();
    subPtr.p->m_subscriberRef    = subRef;
    subPtr.p->m_subscriberData   = subData;
    subPtr.p->m_subscriptionId   = subId;
    subPtr.p->m_subscriptionKey  = subKey;
    subPtr.p->m_subscriptionType = type;
  
    /**
     * ok to memset? Support on all compilers
     * @todo find out if memset is supported by all compilers
     */
    memset(subPtr.p->m_tables,0,MAX_TABLES);
    subPtr.p->m_maxTables    = 0;
    subPtr.p->m_currentTable = 0;
    subPtr.p->m_syncPtrI   = syncPtr.i;
    subPtr.p->m_markRemove = false;
    subPtr.p->m_nSubscribers = 0;

    c_subscriptions.add(subPtr);

    syncPtr.p->m_subscriptionPtrI = subPtr.i;
    syncPtr.p->m_doSendSyncData   = true;
    syncPtr.p->ptrI               = syncPtr.i;
    syncPtr.p->m_locked           = false;
    syncPtr.p->m_error            = false;
  }

  if (restartFlag || 
      type == SubCreateReq::TableEvent) {

    syncPtr.p->m_doSendSyncData = false;

    ndbrequire(type != SubCreateReq::SingleTableScan);
    jam();

    if (subPtr.p->m_tables[req.tableId] != 0) {
      ndbrequire(false); //TODO remove
      jam();
      sendSubCreateRef(signal, req, GrepError::SELECTED_TABLE_ALREADY_ADDED);
      return;
    }
    if (addTableFlag) {
      ndbrequire(type != SubCreateReq::TableEvent);
      jam();
    }
    subPtr.p->m_maxTables++;
    addTableId(req.tableId, subPtr, syncPtr.p);
  } else {
    switch(type){
    case SubCreateReq::SingleTableScan:
      {
	jam();
	syncPtr.p->m_tableList.append(&req.tableId, 1);
	if(signal->getNoOfSections() > 0){
	  SegmentedSectionPtr ptr;
	  signal->getSection(ptr, SubCreateReq::ATTRIBUTE_LIST);
	  LocalDataBuffer<15> attrBuf(c_dataBufferPool,syncPtr.p->m_attributeList);
	  append(attrBuf, ptr, getSectionSegmentPool());
	}
      }
    break;
#if 0
    case SubCreateReq::SelectiveTableSnapshot:
      /**
       * Tables specified by the user that does not exist
       * in the database are just ignored. No error message
       * is given, nor does the db nodes crash
       * @todo: Memory is not release here (used tableBuf)
       */
      {
	if(signal->getNoOfSections() == 0 ){
	  jam();
	  sendSubCreateRef(signal, req, GrepError::WRONG_NO_OF_SECTIONS);
	  return;
	}

	jam();      
	SegmentedSectionPtr ptr;
	signal->getSection(ptr,0);// SubCreateReq::TABLE_LIST);
	SimplePropertiesSectionReader r0(ptr, getSectionSegmentPool());
	Uint32 i=0;
	char table[MAX_TAB_NAME_SIZE];
	r0.reset();
	r0.first();
	while(true){
	  if ((r0.getValueType() != SimpleProperties::StringValue) ||
	      (r0.getValueLen() <= 0)) {
	    releaseSections(signal);
	    ndbrequire(false);
	  }
	  r0.getString(table);
	  strcpy(subPtr.p->m_tableNames[i],table);
	  i++;
	  if(!r0.next())
	    break;
	}
	releaseSections(signal);
	subPtr.p->m_maxTables    = i;
	subPtr.p->m_currentTable = 0;
	releaseSections(signal);
	convertNameToId(subPtr, signal);
	return;
      }
    break;
#endif
    case SubCreateReq::DatabaseSnapshot:
      {
	jam();
      }
    break;
    default:
      ndbrequire(false);
    }
  }

  sendSubCreateConf(signal, sender, subPtr);

  return;
}

void
SumaParticipant::sendSubCreateConf(Signal* signal, Uint32 sender,
				   SubscriptionPtr subPtr)
{
  SubCreateConf * const conf = (SubCreateConf*)signal->getDataPtrSend();      
  conf->subscriptionId       = subPtr.p->m_subscriptionId;
  conf->subscriptionKey      = subPtr.p->m_subscriptionKey;
  conf->subscriberData       = subPtr.p->m_subscriberData;
  sendSignal(sender, GSN_SUB_CREATE_CONF, signal,
	     SubCreateConf::SignalLength, JBB);
}

void
SumaParticipant::sendSubCreateRef(Signal* signal, const SubCreateReq& req, Uint32 errCode){
  jam();
  SubCreateRef * ref = (SubCreateRef *)signal->getDataPtrSend();
  ref->subscriberRef  = reference();
  ref->subscriberData = req.subscriberData;
  ref->err = errCode;
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_CREATE_REF, signal, 
	     SubCreateRef::SignalLength, JBB);
  return;
}

/**********************************************************
 *
 * Setting upp trigger for subscription
 *
 */

void 
SumaParticipant::execSUB_SYNC_REQ(Signal* signal) {
  jamEntry();

  CRASH_INSERTION(13004);
#ifdef EVENT_PH3_DEBUG
  ndbout_c("SumaParticipant::execSUB_SYNC_REQ");
#endif

  SubSyncReq * const req = (SubSyncReq*)signal->getDataPtr();

  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = req->subscriptionId;
  key.m_subscriptionKey = req->subscriptionKey;
  
  if(!c_subscriptions.find(subPtr, key)){
    jam();
    sendSubSyncRef(signal, GrepError::SUBSCRIPTION_ID_NOT_FOUND);
    return;
  }

  /**
   * @todo Tomas, do you really need to do this?
   */
  if(subPtr.p->m_subscriptionType == SubCreateReq::TableEvent) {
    jam();
    subPtr.p->m_subscriberData = req->subscriberData;
  }

  bool ok = false;
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;
  
  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  switch(part){
  case SubscriptionData::MetaData:
    ok = true;
    jam();

    syncPtr.p->startMeta(signal);
    break;
  case SubscriptionData::TableData: {
    ok = true;
    jam();
    syncPtr.p->startScan(signal);
    break;
  }
  }
  ndbrequire(ok);
}

void
SumaParticipant::sendSubSyncRef(Signal* signal, Uint32 errCode){
  jam();
  SubSyncRef  * ref = 
    (SubSyncRef *)signal->getDataPtrSend();
  ref->err = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_SYNC_REF, 
	     signal, 
	     SubSyncRef::SignalLength,
	     JBB);
	     
  releaseSections(signal);  
  return;
}

/**********************************************************
 * Dict interface
 */

void
SumaParticipant::execGET_TABINFOREF(Signal* signal){
  jamEntry();
  GetTabInfoRef* const ref = (GetTabInfoRef*)signal->getDataPtr();
  SyncRecord* tmp = c_syncPool.getPtr(ref->senderData);
  tmp->runGET_TABINFOREF(signal);
}

void
SumaParticipant::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13006);

  if(!assembleFragments(signal)){
    return;
  }
  
  GetTabInfoConf* conf = (GetTabInfoConf*)signal->getDataPtr();
  
  Uint32 tableId = conf->tableId;
  Uint32 senderData = conf->senderData;

  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  ndbrequire(parseTable(signal, conf, tableId, tmp));
  tmp->runGET_TABINFO_CONF(signal);
}

bool
SumaParticipant::parseTable(Signal* signal, GetTabInfoConf* conf, Uint32 tableId,
			    SyncRecord* syncPtr_p){

  SegmentedSectionPtr ptr;
  signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);
  
  SimplePropertiesSectionReader it(ptr, getSectionSegmentPool());
  
  SimpleProperties::UnpackStatus s;
  DictTabInfo::Table tableDesc; tableDesc.init();
  s = SimpleProperties::unpack(it, &tableDesc, 
			       DictTabInfo::TableMapping, 
			       DictTabInfo::TableMappingSize, 
			       true, true);
  
  ndbrequire(s == SimpleProperties::Break);

  TablePtr tabPtr;
  c_tables.find(tabPtr, tableId);
  
  if(!tabPtr.isNull() &&
     tabPtr.p->m_schemaVersion != tableDesc.TableVersion){
    jam();

    tabPtr.p->release(* this);

    // oops wrong schema version in stored tabledesc
    // we need to find all subscriptions with old table desc
    // and all subscribers to this
    // hopefully none
    c_tables.release(tabPtr);
    tabPtr.setNull();
    DLHashTable<SumaParticipant::Subscription>::Iterator i_subPtr;
    c_subscriptions.first(i_subPtr);
    SubscriptionPtr subPtr;
    for(;!i_subPtr.isNull();c_subscriptions.next(i_subPtr)){
      jam();
      c_subscriptions.getPtr(subPtr, i_subPtr.curr.i);
      SyncRecord* tmp = c_syncPool.getPtr(subPtr.p->m_syncPtrI);
      if (tmp == syncPtr_p) {
	jam();
	continue;
      }
      if (subPtr.p->m_tables[tableId]) {
	jam();
	subPtr.p->m_tables[tableId] = 0; // remove this old table reference
	TableList::DataBufferIterator it;
	for(tmp->m_tableList.first(it);!it.isNull();tmp->m_tableList.next(it)) {
	  jam();
	  if (*it.data == tableId){
	    jam();
	    Uint32 *pdata = it.data;
	    tmp->m_tableList.next(it);
	    for(;!it.isNull();tmp->m_tableList.next(it)) {
	      jam();
	      *pdata = *it.data;
	      pdata = it.data;
	    }
	    *pdata = RNIL; // todo remove this last item...
	    break;
	  }
	}
      }
    }
  }

  if (tabPtr.isNull()) {
    jam();
    /**
     * Uninitialized table record
     */
    ndbrequire(c_tables.seize(tabPtr));
    new (tabPtr.p) Table;
    tabPtr.p->m_schemaVersion = RNIL;
    tabPtr.p->m_tableId = tableId;
    tabPtr.p->m_hasTriggerDefined[0] = 0;
    tabPtr.p->m_hasTriggerDefined[1] = 0;
    tabPtr.p->m_hasTriggerDefined[2] = 0;
    tabPtr.p->m_triggerIds[0] = ILLEGAL_TRIGGER_ID;
    tabPtr.p->m_triggerIds[1] = ILLEGAL_TRIGGER_ID;
    tabPtr.p->m_triggerIds[2] = ILLEGAL_TRIGGER_ID;
#if 0
    ndbout_c("Get tab info conf %d", tableId);
#endif
    c_tables.add(tabPtr);
  }

  if(tabPtr.p->m_attributes.getSize() != 0){
    jam();
    return true;
  }

  /**
   * Initialize table object
   */
  Uint32 noAttribs = tableDesc.NoOfAttributes;
  Uint32 notFixed = (tableDesc.NoOfNullable+tableDesc.NoOfVariable);
  tabPtr.p->m_schemaVersion = tableDesc.TableVersion;
  
  // The attribute buffer
  LocalDataBuffer<15> attrBuf(c_dataBufferPool, tabPtr.p->m_attributes);
  
  // Temporary buffer
  DataBuffer<15> theRest(c_dataBufferPool);

  if(!attrBuf.seize(noAttribs)){
    ndbrequire(false);
    return false;
  }
  
  if(!theRest.seize(notFixed)){
    ndbrequire(false);
    return false;
  }
  
  DataBuffer<15>::DataBufferIterator attrIt; // Fixed not nullable
  DataBuffer<15>::DataBufferIterator restIt; // variable + nullable
  attrBuf.first(attrIt);
  theRest.first(restIt);
  
  for(Uint32 i = 0; i < noAttribs; i++) {
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    s = SimpleProperties::unpack(it, &attrDesc, 
				 DictTabInfo::AttributeMapping, 
				 DictTabInfo::AttributeMappingSize, 
				 true, true);
    ndbrequire(s == SimpleProperties::Break);

    if (!attrDesc.AttributeNullableFlag 
	/* && !attrDesc.AttributeVariableFlag */) {
      jam();
      * attrIt.data = attrDesc.AttributeId;
      attrBuf.next(attrIt);
    } else {
      jam();
      * restIt.data = attrDesc.AttributeId;
      theRest.next(restIt);
    }
    
    // Move to next attribute
    it.next();
  }

  /**
   * Put the rest in end of attrBuf
   */
  theRest.first(restIt);
  for(; !restIt.isNull(); theRest.next(restIt)){
    * attrIt.data = * restIt.data;
    attrBuf.next(attrIt);
  }

  theRest.release();
  
  return true;
}

void
SumaParticipant::execDI_FCOUNTCONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13007);

  const Uint32 senderData = signal->theData[3];
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDI_FCOUNTCONF(signal);
}

void 
SumaParticipant::execDIGETPRIMCONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13008);

  const Uint32 senderData = signal->theData[1];
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDIGETPRIMCONF(signal);
}

/*************************************************************************
 *
 *
 */

void
SumaParticipant::SyncRecord::startMeta(Signal* signal){
  jam();
  m_currentTable = 0;
  nextMeta(signal);
}

/**
 * m_tableList only contains UserTables
 */
void
SumaParticipant::SyncRecord::nextMeta(Signal* signal){
  jam();
  
  TableList::DataBufferIterator it;
  if(!m_tableList.position(it, m_currentTable)){
    completeMeta(signal);
    return;
  }

  GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
  req->senderRef = suma.reference();
  req->senderData = ptrI;
  req->requestType = 
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = * it.data;

#if 0
  ndbout_c("GET_TABINFOREQ id %d", req->tableId);
#endif
  suma.sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
		  GetTabInfoReq::SignalLength, JBB);
}

void
SumaParticipant::SyncRecord::runGET_TABINFOREF(Signal* signal)
{
  jam();

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);

  Uint32 type = subPtr.p->m_subscriptionType;

  bool do_continue = false;
  switch (type) {
  case SubCreateReq::TableEvent:
    jam();
    break;
  case SubCreateReq::DatabaseSnapshot:
    jam();
    do_continue = true;
    break;
  case SubCreateReq::SelectiveTableSnapshot:
    jam();
    do_continue = true;
    break;
  case SubCreateReq::SingleTableScan:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  if (! do_continue) {
    m_error = true;
    completeMeta(signal);
    return;
  }

  m_currentTable++;
  nextMeta(signal);
  return;

  // now we need to clean-up
}


void
SumaParticipant::SyncRecord::runGET_TABINFO_CONF(Signal* signal){
  jam();
  
  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //  const Uint32 gci = conf->gci;
  const Uint32 tableId = conf->tableId;
  TableList::DataBufferIterator it;
  
  ndbrequire(m_tableList.position(it, m_currentTable));
  ndbrequire(* it.data == tableId);
  
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
  SegmentedSectionPtr ptr;
  signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);

  bool okToSend = m_doSendSyncData;

  /*
   * If it is a selectivetablesnapshot and the table is not part of the 
   * subscription, then do not send anything, just continue.
   * If it is a tablevent, don't send regardless since the APIs are not
   * interested in meta data.
   */
  if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot)
    if(!subPtr.p->m_tables[tableId])
      okToSend = false;

  if(okToSend) {
    if(refToNode(subPtr.p->m_subscriberRef) == 0){
      jam();
      suma.EXECUTE_DIRECT(refToBlock(subPtr.p->m_subscriberRef),
			  GSN_SUB_META_DATA,
			  signal, 
			  SubMetaData::SignalLength); 
      jamEntry();
      suma.releaseSections(signal);
    } else {
      jam();
      suma.sendSignal(subPtr.p->m_subscriberRef, 
		      GSN_SUB_META_DATA,
		      signal, 
		      SubMetaData::SignalLength, JBB);
    }
  }
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, tabPtr.p->m_fragments);
  if(fragBuf.getSize() == 0){
    /**
     * We need to gather fragment info
     */
    jam();
    signal->theData[0] = RNIL;
    signal->theData[1] = tableId;
    signal->theData[2] = ptrI;
    suma.sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);    
    return;
  }
  
  m_currentTable++;
  nextMeta(signal);
}

void 
SumaParticipant::SyncRecord::runDI_FCOUNTCONF(Signal* signal){
  jam();

  const Uint32 userPtr = signal->theData[0];
  const Uint32 fragCount = signal->theData[1];
  const Uint32 tableId = signal->theData[2];

  ndbrequire(userPtr == RNIL && signal->length() == 5);

  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);  
  ndbrequire(fragBuf.getSize() == 0);
  
  m_currentFragment = fragCount;
  signal->theData[0] = RNIL;
  signal->theData[1] = ptrI;
  signal->theData[2] = tableId;
  signal->theData[3] = 0; // Frag no
  suma.sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
}

void
SumaParticipant::SyncRecord::runDIGETPRIMCONF(Signal* signal){
  jam();

  const Uint32 userPtr = signal->theData[0];
  //const Uint32 senderData = signal->theData[1];
  const Uint32 nodeCount = signal->theData[6];
  const Uint32 tableId = signal->theData[7];
  const Uint32 fragNo = signal->theData[8];
  
  ndbrequire(userPtr == RNIL && signal->length() == 9);
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);  

  /**
   * Add primary node for fragment to list
   */
  FragmentDescriptor fd;
  fd.m_fragDesc.m_nodeId = signal->theData[2];
  fd.m_fragDesc.m_fragmentNo = fragNo;
  signal->theData[2] = fd.m_dummy;
  fragBuf.append(&signal->theData[2], 1);
  
  const Uint32 nextFrag = fragNo + 1;
  if(nextFrag == m_currentFragment){
    /**
     * Complete frag info for table
     */
    m_currentTable++;
    nextMeta(signal);
    return;
  }
  signal->theData[0] = RNIL;
  signal->theData[1] = ptrI;
  signal->theData[2] = tableId;
  signal->theData[3] = nextFrag; // Frag no
  suma.sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
}

void
SumaParticipant::SyncRecord::completeMeta(Signal* signal){
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (meta)");
#else
 
  suma.releaseSections(signal);

  if (m_error) {
    SubSyncRef * const ref = (SubSyncRef*)signal->getDataPtrSend();
    ref->subscriptionId = subPtr.p->m_subscriptionId;
    ref->subscriptionKey = subPtr.p->m_subscriptionKey;
    ref->part = SubscriptionData::MetaData;
    ref->subscriberData = subPtr.p->m_subscriberData;
    ref->errorCode = SubSyncRef::Undefined;
    suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_REF, signal,
		    SubSyncRef::SignalLength, JBB);
  } else {
    SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
    conf->subscriptionId = subPtr.p->m_subscriptionId;
    conf->subscriptionKey = subPtr.p->m_subscriptionKey;
    conf->part = SubscriptionData::MetaData;
    conf->subscriberData = subPtr.p->m_subscriberData;
    suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONF, signal,
		    SubSyncConf::SignalLength, JBB);
  }
#endif
}

/**********************************************************
 *
 * Scan interface
 *
 */

void
SumaParticipant::SyncRecord::startScan(Signal* signal){
  jam();
  
  /**
   * Get fraginfo
   */
  m_currentTable = 0;
  m_currentFragment = 0;
  
  nextScan(signal);
}

bool
SumaParticipant::SyncRecord::getNextFragment(TablePtr * tab, 
					     FragmentDescriptor * fd){
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  TableList::DataBufferIterator tabIt;
  DataBuffer<15>::DataBufferIterator fragIt;
  
  m_tableList.position(tabIt, m_currentTable);
  for(; !tabIt.curr.isNull(); m_tableList.next(tabIt), m_currentTable++){
    TablePtr tabPtr;
    ndbrequire(suma.c_tables.find(tabPtr, * tabIt.data));
    if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot) 
      {
	if(!subPtr.p->m_tables[tabPtr.p->m_tableId]) {
	  *tab = tabPtr;
	  return true;
	}
      }
    LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);
    
    fragBuf.position(fragIt, m_currentFragment);
    for(; !fragIt.curr.isNull(); fragBuf.next(fragIt), m_currentFragment++){
      FragmentDescriptor tmp;
      tmp.m_dummy = * fragIt.data;
      if(tmp.m_fragDesc.m_nodeId == suma.getOwnNodeId()){
	* fd = tmp;
	* tab = tabPtr;
	return true;
      }
    }
    m_currentFragment = 0;
  }
  return false;
}

void
SumaParticipant::SyncRecord::nextScan(Signal* signal){
  jam();
  TablePtr tabPtr;
  FragmentDescriptor fd;
  SubscriptionPtr subPtr;
  if(!getNextFragment(&tabPtr, &fd)){
    jam();
    completeScan(signal);
    return;
  }
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
 
  if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    if(!subPtr.p->m_tables[tabPtr.p->m_tableId]) {
      /*
       * table is not part of the subscription. Check next table
       */
      m_currentTable++;
      nextScan(signal);
      return;
    }
  }

  DataBuffer<15>::Head head = m_attributeList;
  if(head.getSize() == 0){
    head = tabPtr.p->m_attributes;
  }
  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, head);
  
  ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
  const Uint32 parallelism = 16;
  const Uint32 attrLen = 5 + attrBuf.getSize();

  req->senderData = m_subscriptionPtrI;
  req->resultRef = suma.reference();
  req->tableId = tabPtr.p->m_tableId;
  req->requestInfo = 0;
  req->savePointId = 0;
  ScanFragReq::setLockMode(req->requestInfo, 0);
  ScanFragReq::setHoldLockFlag(req->requestInfo, 1);
  ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
  ScanFragReq::setAttrLen(req->requestInfo, attrLen);
  req->fragmentNoKeyLen = fd.m_fragDesc.m_fragmentNo;
  req->schemaVersion = tabPtr.p->m_schemaVersion;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  req->clientOpPtr = (ptrI << 16);
  req->batch_size_rows= 16;
  req->batch_size_bytes= 0;
  suma.sendSignal(DBLQH_REF, GSN_SCAN_FRAGREQ, signal, 
		  ScanFragReq::SignalLength, JBB);
  
  signal->theData[0] = ptrI;
  signal->theData[1] = 0;
  signal->theData[2] = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  
  // Return all
  signal->theData[3] = attrBuf.getSize();
  signal->theData[4] = 0;
  signal->theData[5] = 0;
  signal->theData[6] = 0;
  signal->theData[7] = 0;
  
  Uint32 dataPos = 8;
  DataBuffer<15>::DataBufferIterator it;
  for(attrBuf.first(it); !it.curr.isNull(); attrBuf.next(it)){
    AttributeHeader::init(&signal->theData[dataPos++], * it.data, 0);
    if(dataPos == 25){
      suma.sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, 25, JBB);
	dataPos = 3;
    }
  }
  if(dataPos != 3){
    suma.sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, dataPos, JBB);
  }
  
  m_currentTableId = tabPtr.p->m_tableId;
  m_currentNoOfAttributes = attrBuf.getSize();        
}


void
SumaParticipant::execSCAN_FRAGREF(Signal* signal){
  jamEntry();

//  ScanFragRef * const ref = (ScanFragRef*)signal->getDataPtr();
  ndbrequire(false);
}

void
SumaParticipant::execSCAN_FRAGCONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13011);

  ScanFragConf * const conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 completed = conf->fragmentCompleted;
  const Uint32 senderData = conf->senderData;
  const Uint32 completedOps = conf->completedOps;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, senderData);
  
  if(completed != 2){
    jam();
    
#if PRINT_ONLY
    SubSyncContinueConf * const conf = 
      (SubSyncContinueConf*)signal->getDataPtrSend();  
    conf->subscriptionId = subPtr.p->m_subscriptionId;
    conf->subscriptionKey = subPtr.p->m_subscriptionKey;
    execSUB_SYNC_CONTINUE_CONF(signal);
#else
    SubSyncContinueReq * const req = (SubSyncContinueReq*)signal->getDataPtrSend();
    req->subscriberData = subPtr.p->m_subscriberData;
    req->noOfRowsSent = completedOps;
    sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONTINUE_REQ, signal,
	       SubSyncContinueReq::SignalLength, JBB);
#endif
    return;
  }

  ndbrequire(completedOps == 0);
  
  SyncRecord* tmp = c_syncPool.getPtr(subPtr.p->m_syncPtrI);
  
  tmp->m_currentFragment++;
  tmp->nextScan(signal);
}

void
SumaParticipant::execSUB_SYNC_CONTINUE_CONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13012);

  SubSyncContinueConf * const conf = 
    (SubSyncContinueConf*)signal->getDataPtr();  
  
  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = conf->subscriptionId;
  key.m_subscriptionKey = conf->subscriptionKey;
  
  ndbrequire(c_subscriptions.find(subPtr, key));

  ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
  req->senderData = subPtr.i;
  req->closeFlag = 0;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (getOwnNodeId() << 8);
  req->batch_size_rows = 16;
  req->batch_size_bytes = 0;
  sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	     ScanFragNextReq::SignalLength, JBB);
}

void
SumaParticipant::SyncRecord::completeScan(Signal* signal){
  jam();
  //  m_tableList.release();

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (data)");
#else
  SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
  conf->subscriptionId = subPtr.p->m_subscriptionId;
  conf->subscriptionKey = subPtr.p->m_subscriptionKey;
  conf->part = SubscriptionData::TableData;
  conf->subscriberData = subPtr.p->m_subscriberData;
  suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONF, signal,
		  SubSyncConf::SignalLength, JBB);
#endif
}

void
SumaParticipant::execSCAN_HBREP(Signal* signal){
  jamEntry();
#if 0
  ndbout << "execSCAN_HBREP" << endl << hex;
  for(int i = 0; i<signal->length(); i++){
    ndbout << signal->theData[i] << " ";
    if(((i + 1) % 8) == 0)
      ndbout << endl << hex;
  }
  ndbout << endl;
#endif
}

/**********************************************************
 * Scan data interface
 *
 * Assumption: one execTRANSID_AI contains all attr info
 *
 */

#define SUMA_BUF_SZ1 MAX_KEY_SIZE_IN_WORDS + MAX_TUPLE_SIZE_IN_WORDS
#define SUMA_BUF_SZ MAX_ATTRIBUTES_IN_TABLE + SUMA_BUF_SZ1

static Uint32 f_bufferLock = 0;
static Uint32 f_buffer[SUMA_BUF_SZ];
static Uint32 f_trigBufferSize = 0;
static Uint32 b_bufferLock = 0;
static Uint32 b_buffer[SUMA_BUF_SZ];
static Uint32 b_trigBufferSize = 0;

void
SumaParticipant::execTRANSID_AI(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13015);
  TransIdAI * const data = (TransIdAI*)signal->getDataPtr();
  const Uint32 opPtrI = data->connectPtr;
  const Uint32 length = signal->length() - 3;

  if(f_bufferLock == 0){
    f_bufferLock = opPtrI;
  } else {
    ndbrequire(f_bufferLock == opPtrI);
  }
  
  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, (opPtrI >> 16));
  
  Uint32 sum = 0;
  Uint32 * dst = f_buffer + MAX_ATTRIBUTES_IN_TABLE;
  Uint32 * headers = f_buffer;
  const Uint32 * src = &data->attrData[0];
  const Uint32 * const end = &src[length];
  
  const Uint32 attribs = syncPtr.p->m_currentNoOfAttributes;
  for(Uint32 i = 0; i<attribs; i++){
    Uint32 tmp = * src++;
    * headers++ = tmp;
    Uint32 len = AttributeHeader::getDataSize(tmp);
    
    memcpy(dst, src, 4 * len);
    dst += len;
    src += len;
    sum += len;
  }
  
  ndbrequire(src == end);

  /**
   * Send data to subscriber
   */
  LinearSectionPtr ptr[3];
  ptr[0].p = f_buffer;
  ptr[0].sz = attribs;
  
  ptr[1].p = f_buffer + MAX_ATTRIBUTES_IN_TABLE;
  ptr[1].sz = sum;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, syncPtr.p->m_subscriptionPtrI);
  
  /**
   * Initialize signal
   */  
  SubTableData * sdata = (SubTableData*)signal->getDataPtrSend();
  Uint32 ref = subPtr.p->m_subscriberRef;
  sdata->tableId = syncPtr.p->m_currentTableId;
  sdata->senderData = subPtr.p->m_subscriberData;
  sdata->operation = 3; // Scan
  sdata->gci = 1; // Undefined
#if PRINT_ONLY
  ndbout_c("GSN_SUB_TABLE_DATA (scan) #attr: %d len: %d", attribs, sum);
#else
  sendSignal(ref,
	     GSN_SUB_TABLE_DATA,
	     signal, 
	     SubTableData::SignalLength, JBB,
	     ptr, 2);
#endif
  
  /**
   * Reset f_bufferLock
   */
  f_bufferLock = 0;
}

/**************************************************************
 *
 * Removing subscription
 *
 */

void
SumaParticipant::execSUB_REMOVE_REQ(Signal* signal) {
  jamEntry();

  Uint32 senderRef = signal->getSendersBlockRef();

  CRASH_INSERTION(13021);

  const SubRemoveReq req = *(SubRemoveReq*)signal->getDataPtr();
  SubscriptionPtr subPtr;
  Subscription key;
  key.m_subscriptionId  = req.subscriptionId;
  key.m_subscriptionKey = req.subscriptionKey;
  
  if(!c_subscriptions.find(subPtr, key)) {
    jam();
    sendSubRemoveRef(signal, req, (Uint32) GrepError::SUBSCRIPTION_ID_NOT_FOUND);
    return;
  }
  
  int count = 0;
  {
    jam();
    SubscriberPtr i_subbPtr;
    c_metaSubscribers.first(i_subbPtr);
    while(!i_subbPtr.isNull()){
      jam();
      SubscriberPtr subbPtr = i_subbPtr;
      c_metaSubscribers.next(i_subbPtr);
      if( subbPtr.p->m_subPtrI == subPtr.i ){
	jam();
	c_metaSubscribers.release(subbPtr);
      }
    }
  }

  subPtr.p->m_senderRef  = senderRef;
  subPtr.p->m_senderData = req.senderData;

  completeSubRemoveReq(signal, subPtr);
}

void
SumaParticipant::completeSubRemoveReq(Signal* signal, SubscriptionPtr subPtr) {
  Uint32 subscriptionId  = subPtr.p->m_subscriptionId;
  Uint32 subscriptionKey = subPtr.p->m_subscriptionKey;
  Uint32 senderRef       = subPtr.p->m_senderRef;
  Uint32 senderData      = subPtr.p->m_senderData;

  {
    Ptr<SyncRecord> syncPtr;
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
	
    syncPtr.p->release();
    c_syncPool.release(syncPtr);
  }

  //  if (subPtr.p->m_subscriptionType != SubCreateReq::TableEvent) {
  //    jam();
  //    senderRef = subPtr.p->m_subscriberRef;
  //  }
  c_subscriptions.release(subPtr);

  /**
   * I was the last subscription to be remove so clear c_tables
   */
#if 0
  ndbout_c("c_subscriptionPool.getSize() %d c_subscriptionPool.getNoOfFree()%d",
	   c_subscriptionPool.getSize(),c_subscriptionPool.getNoOfFree());
#endif

  if(c_subscriptionPool.getSize() == c_subscriptionPool.getNoOfFree()) {
    jam();
#if 0
    ndbout_c("SUB_REMOVE_REQ:Clearing c_tables");
#endif
    KeyTable<Table>::Iterator it;
    for(c_tables.first(it); !it.isNull(); ){
      
      it.curr.p->release(* this);
      
      TablePtr tabPtr = it.curr;
      
      c_tables.next(it);
      c_tables.release(tabPtr);
    }
  }
  
  SubRemoveConf * const conf = (SubRemoveConf*)signal->getDataPtrSend();
  conf->senderRef            = reference();
  conf->senderData           = senderData;
  conf->subscriptionId       = subscriptionId;
  conf->subscriptionKey      = subscriptionKey;

  sendSignal(senderRef, GSN_SUB_REMOVE_CONF, signal,
	     SubRemoveConf::SignalLength, JBB);
}

void
SumaParticipant::sendSubRemoveRef(Signal* signal, const SubRemoveReq& req,
				  Uint32 errCode, bool temporary){
  jam();
  SubRemoveRef  * ref = (SubRemoveRef *)signal->getDataPtrSend();
  ref->senderRef  = reference();
  ref->subscriptionId = req.subscriptionId;
  ref->subscriptionKey = req.subscriptionKey;
  ref->senderData = req.senderData;
  ref->err = errCode;
  if (temporary)
    ref->setTemporary();
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_REMOVE_REF, 
	     signal, SubRemoveRef::SignalLength, JBB);
  return;
}

void
SumaParticipant::Table::release(SumaParticipant & suma){
  jam();

  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, m_attributes);
  attrBuf.release();

  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, m_fragments);
  fragBuf.release();
}

void
SumaParticipant::SyncRecord::release(){
  jam();
  m_tableList.release();

  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, m_attributeList);
  attrBuf.release();  
}

template void append(DataBuffer<11>&,SegmentedSectionPtr,SectionSegmentPool&);

