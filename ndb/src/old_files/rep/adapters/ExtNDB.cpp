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

#include "ExtNDB.hpp"
#include "ConfigRetriever.hpp"
#include <NdbSleep.h>

#include <NdbApiSignal.hpp>

#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>
#include <AttributeHeader.hpp>
#include <rep/rep_version.hpp>
#include <ndb_limits.h>

/*****************************************************************************
 * Constructor / Destructor / Init
 *****************************************************************************/
ExtNDB::ExtNDB(GCIContainerPS * gciContainer, ExtAPI * extAPI)
{
  m_grepSender = new ExtSender();
  if (!m_grepSender) REPABORT("Could not allocate object");
  m_gciContainerPS = gciContainer;

  m_nodeGroupInfo = new NodeGroupInfo();
  m_gciContainerPS->setNodeGroupInfo(m_nodeGroupInfo);
  
  m_doneSetGrepSender = false;
  m_subId = 0;
  m_subKey = 0;
  m_firstGCI = 0;
  m_dataLogStarted = false;

  m_extAPI = extAPI;
  if (!m_extAPI) REPABORT("Could not allocate object");
}

ExtNDB::~ExtNDB()
{
  delete m_grepSender;
  delete m_nodeGroupInfo;
}    

void 
ExtNDB::signalErrorHandler(NdbApiSignal  * signal, Uint32 nodeId) 
{
  //const Uint32 gsn = signal->readSignalNumber();
  //const Uint32 len = signal->getLength();
  RLOG(("Send signal failed. Signal %p", signal));
}

bool
ExtNDB::init(const char * connectString) 
{
  m_signalExecThread = NdbThread_Create(signalExecThread_C,
					(void **)this,
					32768,
					"ExtNDB_Service",
					NDB_THREAD_PRIO_LOW);

#if 0
  /**
   * I don't see that this does anything
   *
   * Jonas 13/2-04
   */
  ConfigRetriever cr; cr.setConnectString(connectString);

  ndb_mgm_configuration * config = cr.getConfig(NDB_VERSION, NODE_TYPE_REP);
  if (config == 0) {
    ndbout << "ExtNDB: Configuration error: ";
    const char* erString = cr.getErrorString();
    if (erString == 0) {
      erString = "No error specified!";
    }
    ndbout << erString << endl;
    return false;
  }
  NdbAutoPtr autoPtr(config);
  m_ownNodeId = r.getOwnNodeId();
  
  /**
   * Check which GREPs to connect to (in configuration)
   * 
   * @note SYSTEM LIMITATION: Only connects to one GREP
   */
  Uint32 noOfConnections=0;
  NodeId grepNodeId=0;
  const Properties * connection;

  config->get("NoOfConnections", &noOfConnections);
  for (Uint32 i=0; i<noOfConnections; i++) {
    Uint32 nodeId1, nodeId2;
    config->get("Connection", i, &connection);
    connection->get("NodeId1", &nodeId1);
    connection->get("NodeId2", &nodeId2);
    if (!connection->contains("System1") &&
	!connection->contains("System2") &&
	(nodeId1 == m_ownNodeId || nodeId2 == m_ownNodeId)) {
      /**
       * Found connection 
       */
      if (nodeId1 == m_ownNodeId) {
	grepNodeId = nodeId2;
      } else {
	grepNodeId = nodeId1;
      }
    }
  }
#endif

  m_transporterFacade = TransporterFacade::instance();
  
  assert(m_transporterFacade != 0);
  
  m_ownBlockNo = m_transporterFacade->open(this, execSignal, execNodeStatus);
  assert(m_ownBlockNo > 0);
  m_ownRef = numberToRef(m_ownBlockNo, m_ownNodeId);
  ndbout_c("EXTNDB blockno %d ownref %d ", m_ownBlockNo, m_ownRef);
  assert(m_ownNodeId == m_transporterFacade->ownId());
  
  m_grepSender->setOwnRef(m_ownRef);
  m_grepSender->setTransporterFacade(m_transporterFacade);

  if(!m_grepSender->connected(50000)){
    ndbout_c("ExtNDB: Failed to connect to DB nodes!");
    ndbout_c("ExtNDB: Tried to create transporter as (node %d, block %d).",
	     m_ownNodeId, m_ownBlockNo);
    ndbout_c("ExtNDB: Check that DB nodes are started.");
    return false; 
  }
  ndbout_c("Phase 3 (ExtNDB): Connection %d to NDB Cluster opened (Extractor)",
	   m_ownBlockNo);
  
  for (Uint32 i=1; i<MAX_NDB_NODES; i++) {
    if (m_transporterFacade->getIsDbNode(i) && 
	m_transporterFacade->getIsNodeSendable(i)) 
      {
	Uint32 nodeGrp = m_transporterFacade->getNodeGrp(i);
	m_nodeGroupInfo->addNodeToNodeGrp(i, true, nodeGrp);
	Uint32 nodeId = m_nodeGroupInfo->getFirstConnectedNode(nodeGrp);
	m_grepSender->setNodeId(nodeId);
	if(m_nodeGroupInfo->getPrimaryNode(nodeGrp) == 0) {
	  m_nodeGroupInfo->setPrimaryNode(nodeGrp, nodeId);
	}
	m_doneSetGrepSender = true;
#if 0
	RLOG(("Added node %d to node group %d", i, nodeGrp));
#endif
    }
  }

  return true;
}

/*****************************************************************************
 * Signal Queue Executor
 *****************************************************************************/

class SigMatch 
{
public:
  int gsn;
  void (ExtNDB::* function)(NdbApiSignal *signal);

  SigMatch() { gsn = 0; function = NULL; };

  SigMatch(int _gsn, void (ExtNDB::* _function)(NdbApiSignal *signal)) {
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
void *signalExecThread_C(void *r) 
{
  ExtNDB *grepps = (ExtNDB*)r;

  grepps->signalExecThreadRun();

  NdbThread_Exit(0);
  /* NOTREACHED */
  return 0;
}


void
ExtNDB::signalExecThreadRun() 
{
  Vector<SigMatch> sl;

  /**
   * Signals to be executed
   */
  sl.push_back(SigMatch(GSN_SUB_GCP_COMPLETE_REP, 
			&ExtNDB::execSUB_GCP_COMPLETE_REP));
  
  /**
   * Is also forwarded to SSCoord
   */
  sl.push_back(SigMatch(GSN_GREP_SUB_START_CONF,
			&ExtNDB::execGREP_SUB_START_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_CREATE_CONF,
			&ExtNDB::execGREP_SUB_CREATE_CONF));
  sl.push_back(SigMatch(GSN_GREP_SUB_REMOVE_CONF, 
			&ExtNDB::execGREP_SUB_REMOVE_CONF));
  /**
   * Signals to be forwarded 
   */  
  sl.push_back(SigMatch(GSN_GREP_CREATE_SUBID_CONF, 
			&ExtNDB::execGREP_CREATE_SUBID_CONF));

  sl.push_back(SigMatch(GSN_GREP_SUB_SYNC_CONF, &ExtNDB::sendSignalRep));

  sl.push_back(SigMatch(GSN_GREP_SUB_REMOVE_REF, &ExtNDB::sendSignalRep));
  sl.push_back(SigMatch(GSN_GREP_SUB_SYNC_REF, &ExtNDB::sendSignalRep));
  sl.push_back(SigMatch(GSN_GREP_CREATE_SUBID_REF, &ExtNDB::sendSignalRep));

  sl.push_back(SigMatch(GSN_GREP_SUB_START_REF, &ExtNDB::sendSignalRep));
  sl.push_back(SigMatch(GSN_GREP_SUB_CREATE_REF, &ExtNDB::sendSignalRep));


  while(1) {
    SigMatch *handler = NULL;
    NdbApiSignal *signal = NULL;

    if(m_signalRecvQueue.waitFor(sl, handler, signal, DEFAULT_TIMEOUT)) {
#if 0
      RLOG(("Removed signal from queue (GSN: %d, QSize: %d)",
	    signal->readSignalNumber(), m_signalRecvQueue.size()));
#endif
      if(handler->function != 0) {
	(this->*handler->function)(signal);
	delete signal;  signal = 0;
      } else {
	REPABORT("Illegal handler for signal");
      }
    }
  }
}

void
ExtNDB::sendSignalRep(NdbApiSignal * s)
{
  if(m_repSender->sendSignal(s) == -1)
  {
    signalErrorHandler(s, 0);
  }
}

void
ExtNDB::execSignal(void* executorObj, NdbApiSignal* signal, 
		   class LinearSectionPtr ptr[3])
{
  ExtNDB * executor = (ExtNDB*)executorObj;
     
  const Uint32 gsn = signal->readSignalNumber();
  const Uint32 len = signal->getLength();

  NdbApiSignal * s = new NdbApiSignal(executor->m_ownRef);
  switch(gsn){
  case GSN_SUB_GCP_COMPLETE_REP:
  case GSN_GREP_CREATE_SUBID_CONF:
  case GSN_GREP_SUB_CREATE_CONF:
  case GSN_GREP_SUB_START_CONF:
  case GSN_GREP_SUB_SYNC_CONF:
  case GSN_GREP_SUB_REMOVE_CONF:
  case GSN_GREP_CREATE_SUBID_REF:
  case GSN_GREP_SUB_CREATE_REF:
  case GSN_GREP_SUB_START_REF:
  case GSN_GREP_SUB_SYNC_REF:
  case GSN_GREP_SUB_REMOVE_REF:
    s->set(0, SSREPBLOCKNO, gsn, len);
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);    
    break;
  case GSN_SUB_TABLE_DATA:
    executor->execSUB_TABLE_DATA(signal, ptr);
    delete s;  s=0;
    break;
  case GSN_SUB_META_DATA:
    executor->execSUB_META_DATA(signal, ptr);
    delete s;  s=0;
    break;
  default:
    REPABORT1("Illegal signal received in execSignal", gsn);
  }
  s=0;
#if 0
  ndbout_c("ExtNDB: Inserted signal into queue (GSN: %d, Len: %d)",
	   signal->readSignalNumber(), len);
#endif
}

void 
ExtNDB::execNodeStatus(void* obj, Uint16 nodeId, bool alive, bool nfCompleted)
{
  ExtNDB * thisObj = (ExtNDB*)obj;

  RLOG(("Changed node status (Id %d, Alive %d, nfCompleted %d)",
	nodeId, alive, nfCompleted));
  
  if(alive) {
    /**
     *  Connected
     */
    Uint32 nodeGrp = thisObj->m_transporterFacade->getNodeGrp(nodeId);
    RLOG(("DB node %d of node group %d connected", nodeId, nodeGrp));
  
    thisObj->m_nodeGroupInfo->addNodeToNodeGrp(nodeId, true, nodeGrp);
    Uint32 firstNode = thisObj->m_nodeGroupInfo->getPrimaryNode(nodeGrp);
      
    if(firstNode == 0)
      thisObj->m_nodeGroupInfo->setPrimaryNode(nodeGrp, nodeId);

    if (!thisObj->m_doneSetGrepSender) {
      thisObj->m_grepSender->setNodeId(firstNode);
      thisObj->m_doneSetGrepSender = true;
    }

    RLOG(("Connect: First connected node in nodegroup: %d", 
	  thisObj->m_nodeGroupInfo->getPrimaryNode(nodeGrp)));

  } else if (!nfCompleted) {
    
    /**
     *  Set node as "disconnected" in m_nodeGroupInfo until 
     *  node comes up again.
     */
    Uint32 nodeGrp = thisObj->m_transporterFacade->getNodeGrp(nodeId);
    RLOG(("DB node %d of node group %d disconnected", 
	  nodeId, nodeGrp));
    thisObj->m_nodeGroupInfo->setConnectStatus(nodeId, false);
    /**
     * The node that crashed was also the primary node, the we must change
     * primary node 
     */
    if(nodeId == thisObj->m_nodeGroupInfo->getPrimaryNode(nodeGrp)) {
      Uint32 node = thisObj->m_nodeGroupInfo->getFirstConnectedNode(nodeGrp);
      if(node > 0) {
	thisObj->m_grepSender->setNodeId(node);
	thisObj->m_nodeGroupInfo->setPrimaryNode(nodeGrp, node);
      }
      else {
	thisObj->sendDisconnectRep(nodeGrp);
      }
    }
    RLOG(("Disconnect: First connected node in nodegroup: %d", 
	  thisObj->m_nodeGroupInfo->getPrimaryNode(nodeGrp)));

  } else if(nfCompleted) {
  } else {
    REPABORT("Function execNodeStatus with wrong parameters");
  }
}

/*****************************************************************************
 * Signal Receivers for LOG and SCAN
 *****************************************************************************/

/**
 * Receive datalog/datascan from GREP/SUMA
 */
void
ExtNDB::execSUB_TABLE_DATA(NdbApiSignal * signal, LinearSectionPtr ptr[3])
{
  SubTableData * const data = (SubTableData*)signal->getDataPtr();
  Uint32 tableId            = data->tableId;
  Uint32 operation          = data->operation;
  Uint32 gci                = data->gci;
  Uint32 nodeId             = refToNode(signal->theSendersBlockRef);

  if((SubTableData::LogType)data->logType == SubTableData::SCAN) 
  {
    Uint32 nodeGrp =  m_nodeGroupInfo->findNodeGroup(nodeId);

    NodeGroupInfo::iterator * it;  
    it = new NodeGroupInfo::iterator(nodeGrp, m_nodeGroupInfo);
    for(NodeConnectInfo * nci=it->first(); it->exists();nci=it->next()) {
      m_gciContainerPS->insertLogRecord(nci->nodeId, tableId, 
					operation, ptr, gci);	
    }
    delete it;  it = 0;
  } else {
    m_gciContainerPS->insertLogRecord(nodeId, tableId, operation, ptr, gci);   
  }
}

/**
 * Receive metalog/metascan from GREP/SUMA
 */
void
ExtNDB::execSUB_META_DATA(NdbApiSignal * signal, LinearSectionPtr ptr[3]) 
{
  Uint32 nodeId = refToNode(signal->theSendersBlockRef);
  SubMetaData * const data = (SubMetaData*)signal->getDataPtr();
  Uint32 tableId           = data->tableId;
  Uint32 gci               = data->gci;

  Uint32 nodeGrp = m_nodeGroupInfo->findNodeGroup(nodeId);

  NodeGroupInfo::iterator * it;  
  it = new NodeGroupInfo::iterator(nodeGrp, m_nodeGroupInfo);
  for(NodeConnectInfo * nci=it->first(); it->exists();nci=it->next()) {
    m_gciContainerPS->insertMetaRecord(nci->nodeId, tableId, ptr, gci);
    RLOG(("Received meta record in %d[%d]", nci->nodeId, gci));
  }

  delete it;  it = 0;    
}


/*****************************************************************************
 * Signal Receivers (Signals that are actually just forwarded to SS REP)
 *****************************************************************************/

void 
ExtNDB::execGREP_CREATE_SUBID_CONF(NdbApiSignal * signal) 
{
  CreateSubscriptionIdConf const * conf = 
    (CreateSubscriptionIdConf *)signal->getDataPtr();
  Uint32 subId  = conf->subscriptionId;
  Uint32 subKey = conf->subscriptionKey;
  ndbout_c("GREP_CREATE_SUBID_CONF m_extAPI=%p\n", m_extAPI);
  m_extAPI->eventSubscriptionIdCreated(subId, subKey);
}

/*****************************************************************************
 * Signal Receivers 
 *****************************************************************************/

/**
 * Receive information about completed GCI from GREP/SUMA
 *
 * GCI completed, i.e. no more unsent log records exists in SUMA
 * @todo use node id to identify buffers?
 */
void
ExtNDB::execSUB_GCP_COMPLETE_REP(NdbApiSignal * signal) 
{
  SubGcpCompleteRep * const rep = (SubGcpCompleteRep*)signal->getDataPtr();
  const Uint32 gci              = rep->gci;
  Uint32 nodeId                 = refToNode(rep->senderRef);

  RLOG(("Epoch %d completed at node %d", gci, nodeId));
  m_gciContainerPS->setCompleted(gci, nodeId);

  if(m_firstGCI == gci && !m_dataLogStarted) {
    sendGREP_SUB_START_CONF(signal, m_firstGCI);
    m_dataLogStarted = true;
  }
}

/**
 * Send info that scan is competed to SS REP
 *
 * @todo  Use node id to identify buffers?
 */
void 
ExtNDB::sendGREP_SUB_START_CONF(NdbApiSignal * signal, Uint32 gci)
{
  RLOG(("Datalog started (Epoch %d)", gci));
  GrepSubStartConf * conf = (GrepSubStartConf *)signal->getDataPtrSend();  
  conf->firstGCI                = gci;
  conf->subscriptionId          = m_subId;
  conf->subscriptionKey         = m_subKey;
  conf->part                    = SubscriptionData::TableData;
  signal->m_noOfSections = 0;
  signal->set(0, SSREPBLOCKNO, GSN_GREP_SUB_START_CONF,
	      GrepSubStartConf::SignalLength);  
  sendSignalRep(signal);
}

/**
 * Scan is completed... says SUMA/GREP
 *
 * @todo  Use node id to identify buffers?
 */
void 
ExtNDB::execGREP_SUB_START_CONF(NdbApiSignal * signal)
{
  GrepSubStartConf * const conf = (GrepSubStartConf *)signal->getDataPtr();  
  Uint32 part                   = conf->part;
  //Uint32 nodeId                 = refToNode(conf->senderRef);
  m_firstGCI                    = conf->firstGCI;

  if (part == SubscriptionData::TableData) {
    RLOG(("Datalog started (Epoch %d)", m_firstGCI));
    return;
  } 
  RLOG(("Metalog started (Epoch %d)", m_firstGCI));

  signal->set(0, SSREPBLOCKNO, GSN_GREP_SUB_START_CONF,
	      GrepSubStartConf::SignalLength);  
  sendSignalRep(signal);
}

/**
 * Receive no of node groups that PS has and pass signal on to SS
 */
void 
ExtNDB::execGREP_SUB_CREATE_CONF(NdbApiSignal * signal) 
{
  GrepSubCreateConf * conf = (GrepSubCreateConf *)signal->getDataPtrSend();  
  m_subId                  = conf->subscriptionId;
  m_subKey                 = conf->subscriptionKey;

  conf->noOfNodeGroups  = m_nodeGroupInfo->getNoOfNodeGroups();
  sendSignalRep(signal);
}

/**
 * Receive conf that subscription has been remove in GREP/SUMA
 *
 * Pass signal on to TransPS
 */
void 
ExtNDB::execGREP_SUB_REMOVE_CONF(NdbApiSignal * signal) 
{  
  m_gciContainerPS->reset();
  sendSignalRep(signal);
}

/**
 * If all PS nodes has disconnected, then remove all epochs 
 * for this subscription.
 */
void
ExtNDB::sendDisconnectRep(Uint32 nodeId) 
{
  NdbApiSignal * signal = new NdbApiSignal(m_ownRef);
  signal->set(0, SSREPBLOCKNO, GSN_REP_DISCONNECT_REP,
	      RepDisconnectRep::SignalLength);
  RepDisconnectRep * rep = (RepDisconnectRep*) signal->getDataPtrSend();
  rep->nodeId = nodeId;
  rep->subId  = m_subId;
  rep->subKey = m_subKey;
  sendSignalRep(signal);
}
