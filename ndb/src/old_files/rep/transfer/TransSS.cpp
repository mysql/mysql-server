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


#include "ConfigRetriever.hpp"

#include <NdbApiSignal.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/RepImpl.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/GrepImpl.hpp>

#include <SimpleProperties.hpp>
#include <rep/rep_version.hpp>

#include "TransSS.hpp"

//#define DEBUG_REP_GET_GCI_CONF

/*****************************************************************************
 * Constructor / Destructor / Init
 *****************************************************************************/
TransSS::TransSS(GCIContainer * gciContainer, RepState * repState) 
{
  m_repSender = new ExtSender();
  if (!m_repSender) REPABORT("Could not allocate new ExtSender");
  m_gciContainer = gciContainer;
  m_repState = repState;
}

TransSS::~TransSS() 
{
  delete m_repSender;
}

void
TransSS::init(const char * connectString) 
{
  abort();
#ifdef NOT_FUNCTIONAL
  m_signalExecThread = NdbThread_Create(signalExecThread_C,
					(void **)this,
					32768,
					"TransSS_Service",
					NDB_THREAD_PRIO_LOW);
  ConfigRetriever configRetriever;
  configRetriever.setConnectString(connectString);
  
  Properties* config = configRetriever.getConfig("REP", REP_VERSION_ID);
  if (config == 0) {
    ndbout << "Configuration error: ";
    const char* erString = configRetriever.getErrorString();
    if (erString == 0) {
      erString = "No error specified!";
    }
    ndbout << erString << endl;
    exit(-1);
  }
  Properties * extConfig;

  /**
   * @todo Hardcoded standby system name
   */
  if (!config->getCopy("EXTERNAL SYSTEM_External", &extConfig)) {
    ndbout << "External System \"External\" not found in configuration. "
	   << "Check config.ini." << endl;
    config->print();
    exit(-1);
  }
  m_ownNodeId = configRetriever.getOwnNodeId();
  extConfig->put("LocalNodeId", m_ownNodeId);
  extConfig->put("LocalNodeType", "REP");
  Uint32 noOfConnections;
  extConfig->get("NoOfConnections", &noOfConnections);
  /*  if (noOfConnections != 1) {
    ndbout << "TransSS: There are " << noOfConnections << " connections "
	   << "defined in configuration" 
	   << endl
	   << "       There should be exactly one!" << endl;
    exit(-1);
    }*/
  
  /******************************
   * Set node id of external REP
   ******************************/
  const Properties * connection;
  const char * extSystem;
 
  Uint32 extRepNodeId, tmpOwnNodeId;
  
  for(Uint32 i=0; i < noOfConnections; i++) {
    extConfig->get("Connection", i, &connection);
    if(connection == 0) REPABORT("Connection not found");

    if(connection->get("System1", &extSystem)) {
      connection->get("NodeId1", &extRepNodeId);
      connection->get("NodeId2", &tmpOwnNodeId);
    } else {
      connection->get("System2", &extSystem);
      connection->get("NodeId1", &tmpOwnNodeId);
      connection->get("NodeId2", &extRepNodeId);
    }
    if(m_ownNodeId == tmpOwnNodeId)
      break;
  }

  if(extRepNodeId==0) REPABORT("External replication server not found");
  if(extSystem==0) REPABORT("External system not found");

  m_transporterFacade = new TransporterFacade();
  if (!m_transporterFacade->init(extConfig)) 
  {
    ndbout << "TransSS: Failed to initialize transporter facade" << endl;
    exit(-1);
  } 
  
  m_ownBlockNo = m_transporterFacade->open(this, execSignal, execNodeStatus);
  assert(m_ownBlockNo > 0);
  m_ownRef = numberToRef(m_ownBlockNo, m_ownNodeId);
  assert(m_ownNodeId == m_transporterFacade->ownId());
  
  ndbout_c("Phase 2 (TransSS): Connection %d to external REP node %d opened",
	   m_ownBlockNo, extRepNodeId);
  
  m_repSender->setNodeId(extRepNodeId);
  m_repSender->setOwnRef(m_ownRef);
  m_repSender->setTransporterFacade(m_transporterFacade);
#endif
}

/*****************************************************************************
 * Signal Queue Executor
 *****************************************************************************/

class SigMatch 
{
public:
  int gsn;
  void (TransSS::* function)(NdbApiSignal *signal);
  
  SigMatch() { gsn = 0; function = NULL; };

  SigMatch(int _gsn, void (TransSS::* _function)(NdbApiSignal *signal)) {
    gsn = _gsn;
    function = _function;
  };
  
  bool check(NdbApiSignal *signal) {
    if(signal->readSignalNumber() == gsn)
      return true;
    return false;
  };
};

extern "C"
void *
signalExecThread_C(void *r) 
{
  TransSS *transss = (TransSS*)r;

  transss->signalExecThreadRun();
  NdbThread_Exit(0);
  /* NOTREACHED */
  return 0;
}

void
TransSS::signalExecThreadRun() 
{
  Vector<SigMatch> sl;
  /**
   * Signals to be forwarded to TransPS 
   */
  sl.push_back(SigMatch(GSN_REP_GET_GCI_REQ, 
			&TransSS::sendSignalRep));
  sl.push_back(SigMatch(GSN_REP_GET_GCIBUFFER_REQ, 
			&TransSS::sendSignalRep));
  /**
   * Signals to be executed
   */
  sl.push_back(SigMatch(GSN_REP_GCIBUFFER_ACC_REP, 
			&TransSS::execREP_GCIBUFFER_ACC_REP));
  sl.push_back(SigMatch(GSN_REP_DISCONNECT_REP, 
			&TransSS::execREP_DISCONNECT_REP));
  sl.push_back(SigMatch(GSN_GREP_SUB_REMOVE_CONF, 
			&TransSS::execGREP_SUB_REMOVE_CONF));
  sl.push_back(SigMatch(GSN_REP_GET_GCIBUFFER_CONF, 
			&TransSS::execREP_GET_GCIBUFFER_CONF));

  sl.push_back(SigMatch(GSN_REP_CLEAR_PS_GCIBUFFER_CONF, 
			&TransSS::execREP_CLEAR_PS_GCIBUFFER_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_SYNC_CONF, 
			&TransSS::execGREP_SUB_SYNC_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_SYNC_REF, 
			&TransSS::execGREP_SUB_SYNC_REF));
  sl.push_back(SigMatch(GSN_REP_GET_GCIBUFFER_REF, 
			&TransSS::execREP_GET_GCIBUFFER_REF));

  /**
   * Signals to be executed : Subscriptions
   */
  sl.push_back(SigMatch(GSN_GREP_CREATE_SUBID_CONF,
			&TransSS::execGREP_CREATE_SUBID_CONF));
  sl.push_back(SigMatch(GSN_GREP_CREATE_SUBID_REF,
			&TransSS::execGREP_CREATE_SUBID_REF));
  sl.push_back(SigMatch(GSN_GREP_SUB_CREATE_CONF, 
			&TransSS::execGREP_SUB_CREATE_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_CREATE_REF, 
			&TransSS::execGREP_SUB_CREATE_REF));
  sl.push_back(SigMatch(GSN_GREP_SUB_START_CONF, 
			&TransSS::execGREP_SUB_START_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_START_REF,
			&TransSS::execGREP_SUB_START_REF));

  /**
   * Signals to be executed and forwarded
   */
  sl.push_back(SigMatch(GSN_REP_GET_GCI_CONF, 
			&TransSS::execREP_GET_GCI_CONF));

  /**
   * Signals to be forwarded
   */
  sl.push_back(SigMatch(GSN_GREP_SUB_REMOVE_REF, 
			&TransSS::execGREP_SUB_REMOVE_REF));
  sl.push_back(SigMatch(GSN_REP_CLEAR_PS_GCIBUFFER_REF,
			&TransSS::execREP_CLEAR_PS_GCIBUFFER_REF));
  sl.push_back(SigMatch(GSN_REP_GET_GCI_REF, 
			&TransSS::execREP_GET_GCI_REF));
		      
  while(1) {
    SigMatch *handler = NULL;
    NdbApiSignal *signal = NULL;
    if(m_signalRecvQueue.waitFor(sl, handler, signal, DEFAULT_TIMEOUT)) 
    {
#if 0
      ndbout_c("TransSS: Removed signal from queue (GSN: %d, QSize: %d)",
	       signal->readSignalNumber(), m_signalRecvQueue.size());
#endif
      if(handler->function != 0) 
      {
	(this->*handler->function)(signal);
	delete signal;
	signal = 0;
      } else {
	REPABORT("Illegal handler for signal");
      }
    } 
  }
}

void 
TransSS::sendSignalRep(NdbApiSignal * s) 
{
  m_repSender->sendSignal(s);
}

void 
TransSS::execNodeStatus(void* obj, Uint16 nodeId, 
			bool alive, bool nfCompleted)
{
  TransSS * thisObj = (TransSS*)obj;

  if (alive) {
    thisObj->m_repState->eventNodeConnected(nodeId);

  } else if (!nfCompleted) {
    thisObj->m_repState->eventNodeDisconnected(nodeId);

  } else if (nfCompleted) {
    thisObj->m_repState->eventNodeConnectable(nodeId);

  } else {
    REPABORT("Illegal state for execNodeStatus");
  }
}

void
TransSS::execSignal(void* executorObj, NdbApiSignal* signal, 
		    class LinearSectionPtr ptr[3])
{
  TransSS * executor = (TransSS *) executorObj;

  const Uint32 gsn = signal->readSignalNumber();
  const Uint32 len = signal->getLength();

  NdbApiSignal * s = new NdbApiSignal(executor->m_ownRef);
  switch (gsn) {
  case GSN_REP_GET_GCI_REQ:
  case GSN_REP_GET_GCIBUFFER_REQ:
  case GSN_REP_GET_GCIBUFFER_CONF:
  case GSN_GREP_SUB_REMOVE_CONF:
  case GSN_REP_DISCONNECT_REP:
  case GSN_REP_GCIBUFFER_ACC_REP:
    s->set(0, PSREPBLOCKNO, gsn, len);    
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);    
    break;
  case GSN_GREP_CREATE_SUBID_CONF:
  case GSN_GREP_SUB_CREATE_CONF:
  case GSN_GREP_SUB_START_CONF:
  case GSN_GREP_SUB_SYNC_CONF:
  case GSN_REP_GET_GCI_CONF:
  case GSN_REP_CLEAR_PS_GCIBUFFER_CONF:
  case GSN_GREP_CREATE_SUBID_REF:
  case GSN_GREP_SUB_CREATE_REF:
  case GSN_GREP_SUB_START_REF:
  case GSN_GREP_SUB_SYNC_REF:
  case GSN_GREP_SUB_REMOVE_REF:
  case GSN_REP_GET_GCI_REF:
  case GSN_REP_GET_GCIBUFFER_REF:
  case GSN_REP_CLEAR_PS_GCIBUFFER_REF:
    s->set(0, GREP, gsn, len);    
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);    
    break;
  case GSN_REP_DATA_PAGE:
    executor->execREP_DATA_PAGE(signal, ptr);
    delete s;  s = 0;
    break;
  default:
    REPABORT1("Illegal signal received in execSignal %d", gsn);
  }

#if 0
  ndbout_c("TransSS: Inserted signal into queue (GSN: %d, Len: %d)",
	   signal->readSignalNumber(), len);
#endif
}

/*****************************************************************************
 * Signal Executors
 *****************************************************************************/

void
TransSS::execREP_DATA_PAGE(NdbApiSignal * signal, LinearSectionPtr ptr[3])
{
  RepDataPage * const page = (RepDataPage*)signal->getDataPtr();
  m_gciContainer->insertPage(page->gci, page->nodeGrp,
			     (char*)(ptr[0].p), 4 * ptr[0].sz); 
}

/**
 * Recd from TransPS
 */
void
TransSS::execREP_GCIBUFFER_ACC_REP(NdbApiSignal * signal) 
{
  RepGciBufferAccRep * const  rep = 
    (RepGciBufferAccRep * )signal->getDataPtr();

  Uint32 gci              = rep->gci;
  Uint32 nodeGrp          = rep->nodeGrp;
  Uint32 totalSize        = rep->totalSentBytes;
  GCIBuffer * buffer      = m_gciContainer->getGCIBuffer(gci, nodeGrp);
  Uint32 getReceivedBytes = 0;
  if (buffer != 0) 
    getReceivedBytes = buffer->getReceivedBytes();

  RLOG(("TransSS: Received %d:[%d] (%d of %d bytes)",
	nodeGrp, gci, getReceivedBytes, totalSize));

  if(getReceivedBytes != totalSize) {
    REPABORT("Did not receive correct number of bytes");
  } 
}

/**
 *  Received from primary system
 */
void
TransSS::execREP_GET_GCIBUFFER_CONF(NdbApiSignal * signal) 
{
  RepGetGciBufferConf * conf = (RepGetGciBufferConf*)signal->getDataPtr();
  conf->senderRef = m_ownRef;
  Uint32 first = conf->firstSSGCI;
  Uint32 last  = conf->lastSSGCI;
  for(Uint32 i = first; i <= last; i++) {
    m_gciContainer->setCompleted(i, conf->nodeGrp);
  }

  /**
   * Buffers @ PS
   */
  Interval ps(conf->firstPSGCI, conf->lastPSGCI);
  m_repState->add(Channel::PS, conf->nodeGrp, ps);

  /**
   * Buffers @ SS
   */
  Uint32 ssfirst, sslast;
  m_gciContainer->getAvailableGCIBuffers(conf->nodeGrp, &ssfirst, &sslast);
  Interval ss(ssfirst, sslast);
  m_repState->clear(Channel::SS, conf->nodeGrp, universeInterval);
  m_repState->add(Channel::SS, conf->nodeGrp, ss);
  m_repState->clear(Channel::SSReq, conf->nodeGrp, ss);

  RLOG(("Transfered epochs (PS:%d[%d-%d], SS:%d[%d-%d])",
	conf->nodeGrp, conf->firstPSGCI, conf->lastPSGCI,
	conf->nodeGrp, conf->firstSSGCI, conf->lastSSGCI));
}

/**
 *  Received from primary system
 */
void
TransSS::execGREP_SUB_REMOVE_CONF(NdbApiSignal * signal) 
{
  GrepSubRemoveConf * conf = (GrepSubRemoveConf* )signal->getDataPtr();
  Uint32 subId  = conf->subscriptionId;
  Uint32 subKey = conf->subscriptionKey;
  
  /**
   * @todo Fix this sending
   */
#if 0
  signal->theData[0] = EventReport::GrepSubscriptionInfo;
  signal->theData[1] = GrepEvent::GrepSS_SubRemoveConf;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 4, JBB);
#endif

  m_repState->eventSubscriptionDeleted(subId, subKey);
  RLOG(("Subscription deleted (SubId: %d, SubKey: %d)", subId, subKey));
}

void
TransSS::execGREP_SUB_REMOVE_REF(NdbApiSignal * signal) 
{
  GrepSubRemoveRef * ref = (GrepSubRemoveRef* )signal->getDataPtr();
  Uint32 subId  = ref->subscriptionId;
  Uint32 subKey = ref->subscriptionKey;

  /** @todo: Add repevent for this */
  RLOG(("TransSS: Warning: Grep sub remove ref (SubId: %d, SubKey: %d)", 
	subId, subKey));
}

/**
 *  Received from primary system
 */
void
TransSS::execREP_GET_GCI_CONF(NdbApiSignal * signal) 
{
  RepGetGciConf * conf = (RepGetGciConf*)signal->getDataPtr();
  Uint32 nodeGrp = conf->nodeGrp;
  Interval i(conf->firstPSGCI, conf->lastPSGCI);
  m_repState->add(Channel::PS, nodeGrp, i);

  Uint32 first, last;
  m_gciContainer->getAvailableGCIBuffers(nodeGrp, &first, &last);
  Interval j(first, last);
  m_repState->clear(Channel::SS, nodeGrp, universeInterval);
  m_repState->add(Channel::SS, nodeGrp, j);

#ifdef DEBUG_REP_GET_GCI_CONF
  RLOG(("TransSS: Requestor info received "
	 "(PS: %d:[%d-%d], SS: %d:[%d-%d])",
	 conf->nodeGrp, conf->firstPSGCI, conf->lastPSGCI,
	 conf->nodeGrp, conf->firstSSGCI, conf->lastSSGCI));
#endif
}

void
TransSS::execREP_GET_GCI_REF(NdbApiSignal * signal) 
{
  RepGetGciRef * ref = (RepGetGciRef*)signal->getDataPtr();
  Uint32 nodeGrp = ref->nodeGrp;

  RLOG(("WARNING! Requestor info request failed (Nodegrp: %d)", nodeGrp));
}

/**
 * Recd from GrepPS
 * This signal means that a DB node has disconnected.
 * @todo Do we need to know that a DB node disconnected?
 *
 * A node has disconnected (REP or PS DB)
 * @todo let the requestor respond to this event 
 * in a proper way.
 */
void
TransSS::execREP_DISCONNECT_REP(NdbApiSignal * signal) 
{
  RepDisconnectRep * const rep = 
    (RepDisconnectRep*)signal->getDataPtr();
  
  //Uint32 nodeId      = rep->nodeId;
  Uint32 nodeType    = rep->nodeType;

  if((RepDisconnectRep::NodeType)nodeType == RepDisconnectRep::REP)
  {
    m_repState->disable();
  }
}

/**
 *  The buffer is now deleted on REP PS.  We can now clear it from PS.
 */
void
TransSS::execREP_CLEAR_PS_GCIBUFFER_CONF(NdbApiSignal * signal)
{
  RepClearPSGciBufferConf * const conf = 
    (RepClearPSGciBufferConf*)signal->getDataPtr();
  Uint32 firstGCI    = conf->firstGCI;
  Uint32 lastGCI     = conf->lastGCI;
  Uint32 nodeGrp     = conf->nodeGrp;
  Interval i(firstGCI, lastGCI);
  m_repState->clear(Channel::PS, nodeGrp, i);
  m_repState->clear(Channel::DelReq, nodeGrp, i);
 
  RLOG(("Deleted PS:%d:[%d-%d]", nodeGrp, firstGCI, lastGCI));
}

/**
 * Something went wrong when deleting buffer on REP PS
 */
void
TransSS::execREP_CLEAR_PS_GCIBUFFER_REF(NdbApiSignal * signal) 
{
  RepClearPSGciBufferRef * const ref = 
    (RepClearPSGciBufferRef*)signal->getDataPtr();
  Uint32 firstGCI    = ref->firstGCI;
  Uint32 lastGCI     = ref->lastGCI;
  Uint32 nodeGrp     = ref->nodeGrp;
  
  RLOG(("WARNING! Could not delete PS:%d:[%d-%d]", nodeGrp, firstGCI, lastGCI));
}

/*****************************************************************************
 * Signal Executors : SCAN
 *****************************************************************************/

/**
 * Scan has started on PS side... (says PS REP)
 */
void
TransSS::execGREP_SUB_SYNC_CONF(NdbApiSignal* signal) 
{
  GrepSubSyncConf * const conf = (GrepSubSyncConf * ) signal->getDataPtr();
  Uint32 subId                 = conf->subscriptionId;
  Uint32 subKey                = conf->subscriptionKey;
  Interval epochs(conf->firstGCI, conf->lastGCI);
  SubscriptionData::Part part  = (SubscriptionData::Part) conf->part;
 
  switch(part) {
  case SubscriptionData::MetaData:
    RLOG(("Metascan completed. Subcription %d-%d, Epochs [%d-%d]",
	  subId, subKey, epochs.first(), epochs.last()));
    m_repState->eventMetaScanCompleted(signal, subId, subKey, epochs);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionInfo;
    signal->theData[1] = GrepEvent::GrepSS_SubSyncMetaConf;
    signal->theData[2] = subId;
    signal->theData[3] = subKey;
    signal->theData[4] = gci;
    sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 5, JBB);
#endif
    break;
  case SubscriptionData::TableData:
    RLOG(("Datascan completed. Subcription %d-%d, Epochs [%d-%d]",
	  subId, subKey, epochs.first(), epochs.last()));
    m_repState->eventDataScanCompleted(signal, subId, subKey, epochs);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionInfo;
    signal->theData[1] = GrepEvent::GrepSS_SubSyncDataConf;
    signal->theData[2] = subId;
    signal->theData[3] = subKey;
    signal->theData[4] = gci;
    sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 5, JBB);
#endif
    break;
  default:
    REPABORT3("Wrong subscription part", part, subId, subKey);
  }
}

void
TransSS::execGREP_SUB_SYNC_REF(NdbApiSignal* signal) 
{
  GrepSubSyncRef * const ref = (GrepSubSyncRef * ) signal->getDataPtr();
  Uint32 subId                    = ref->subscriptionId;
  Uint32 subKey                   = ref->subscriptionKey;
  SubscriptionData::Part part     = (SubscriptionData::Part) ref->part;
  GrepError::Code error           = (GrepError::Code) ref->err;
 
  switch(part) {
  case SubscriptionData::MetaData:
    m_repState->eventMetaScanFailed(subId, subKey, error);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionAlert;
    signal->theData[1] = GrepEvent::GrepSS_SubSyncMetaRef;
    signal->theData[2] = subId;
    signal->theData[3] = subKey;
    //    signal->theData[4] = gci;
    sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 5, JBB);
#endif
    break;
  case SubscriptionData::TableData:
    m_repState->eventDataScanFailed(subId, subKey, error);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionAlert;
    signal->theData[1] = GrepEvent::GrepSS_SubSyncDataRef;
    signal->theData[2] = subId;
    signal->theData[3] = subKey;
    //signal->theData[4] = m_lastScanGCI;
    sendSignal(CMVMI_REF,GSN_EVENT_REP,signal, 5, JBB);
#endif
    break;
  default:
    REPABORT3("Wrong subscription part", part, subId, subKey);
  }
}

/**
 * Something went wrong says REP PS
 */
void 
TransSS::execREP_GET_GCIBUFFER_REF(NdbApiSignal* signal) 
{
  RepGetGciBufferRef * const ref = (RepGetGciBufferRef*)signal->getDataPtr();
  /*
  Uint32 senderData       = ref->senderData;
  Uint32 senderRef        = ref->senderRef;
  Uint32 firstPSGCI       = ref->firstPSGCI;
  Uint32 lastPSGCI        = ref->lastPSGCI;
  Uint32 firstSSGCI       = ref->firstSSGCI;
  Uint32 lastSSGCI        = ref->lastSSGCI;
  Uint32 currentGCIBuffer = ref->currentGCIBuffer;
  Uint32 nodeGrp          = ref->nodeGrp;
  */
  GrepError::Code err     = ref->err;

  RLOG(("WARNING! Request to get buffer failed. Error %d:%s",
	err, GrepError::getErrorDesc(err)));
}
