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
#include <NdbSleep.h>

#include <NdbApiSignal.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>
#include <GrepError.hpp>
#include <SimpleProperties.hpp>

#include "TransPS.hpp"
#include <rep/storage/NodeGroupInfo.hpp>

/*****************************************************************************
 * Constructor / Destructor / Init
 *****************************************************************************/
TransPS::TransPS(GCIContainerPS* gciContainer) 
{
  m_repSender = new ExtSender();
  m_gciContainerPS = gciContainer;
}

TransPS::~TransPS() 
{
  delete m_repSender;
}

void
TransPS::init(TransporterFacade * tf, const char * connectString) 
{
  abort();
#ifdef NOT_FUNCTIONAL
  m_signalExecThread = NdbThread_Create(signalExecThread_C,
					(void **)this,
					32768,
					"TransPS_Service",
					NDB_THREAD_PRIO_LOW);

  ConfigRetriever configRetriever;
  //  configRetriever.setConnectString(connectString);
  Properties* config = configRetriever.getConfig("REP", REP_VERSION_ID);
  if (config == 0) {
    ndbout << "TransPS: Configuration error: ";
    const char* erString = configRetriever.getErrorString();
    if (erString == 0) {
      erString = "No error specified!";
    }
    ndbout << erString << endl;
    exit(-1);
  }

  Properties * extConfig;
  /**
   * @todo Hardcoded primary system name
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
    ndbout << "TransPS: There are " << noOfConnections << " connections "
	   << "defined in configuration" 
	   << endl
	   << "       There should be exactly one!" << endl;
    exit(-1);
  }
  */
  /******************************
   * Set node id of external REP
   ******************************/
  const Properties * connection;
  const char * extSystem;
  Uint32 extRepNodeId, tmpOwnNodeId;
  
  for(Uint32 i=0; i < noOfConnections; i++) {
    extConfig->get("Connection", i, &connection);
    if(connection == 0) REPABORT("No connection found");

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

  m_ownBlockNo = tf->open(this, execSignal, execNodeStatus);
  assert(m_ownBlockNo > 0);

  m_ownRef = numberToRef(m_ownBlockNo, m_ownNodeId);
  assert(m_ownNodeId == tf->ownId());

  ndbout_c("Phase 4 (TransPS): Connection %d to external REP node %d opened",
	   m_ownBlockNo, extRepNodeId);

  m_repSender->setNodeId(extRepNodeId);
  m_repSender->setOwnRef(m_ownRef);
  m_repSender->setTransporterFacade(tf);
#endif
}

/*****************************************************************************
 * Signal Queue Executor
 *****************************************************************************/

class SigMatch 
{
public:
  int gsn;
  void (TransPS::* function)(NdbApiSignal *signal);

  SigMatch() { gsn = 0; function = NULL; };

  SigMatch(int _gsn, void (TransPS::* _function)(NdbApiSignal *signal)) { 
    gsn = _gsn;
    function = _function;
  };
  
  bool check(NdbApiSignal *signal) {
    if(signal->readSignalNumber() == gsn) return true;
    return false;
  };
};

extern "C"
void *
signalExecThread_C(void *r) 
{
  TransPS *repps = (TransPS*)r;

  repps->signalExecThreadRun();

  NdbThread_Exit(0);
  /* NOTREACHED */
  return 0;
}

void
TransPS::signalExecThreadRun() 
{
  Vector<SigMatch> sl;

  /**
   * Signals executed here
   */
  sl.push_back(SigMatch(GSN_REP_GET_GCI_REQ, 
			&TransPS::execREP_GET_GCI_REQ));
  sl.push_back(SigMatch(GSN_REP_GET_GCIBUFFER_REQ,
			&TransPS::execREP_GET_GCIBUFFER_REQ));
  sl.push_back(SigMatch(GSN_REP_CLEAR_PS_GCIBUFFER_REQ,
			&TransPS::execREP_CLEAR_PS_GCIBUFFER_REQ));

  /** 
   * Signals to be forwarded to GREP::PSCoord
   */
  sl.push_back(SigMatch(GSN_GREP_SUB_CREATE_REQ, &TransPS::sendSignalGrep));
  
  /** 
   * Signals to be forwarded to GREP::PSCoord
   */
  sl.push_back(SigMatch(GSN_GREP_CREATE_SUBID_REQ, &TransPS::sendSignalGrep));
  sl.push_back(SigMatch(GSN_GREP_SUB_START_REQ, &TransPS::sendSignalGrep));
  sl.push_back(SigMatch(GSN_GREP_SUB_SYNC_REQ, &TransPS::sendSignalGrep));
  sl.push_back(SigMatch(GSN_GREP_SUB_REMOVE_REQ, &TransPS::sendSignalGrep));

  while(1) {
    SigMatch *handler = NULL;
    NdbApiSignal *signal = NULL;
    if(m_signalRecvQueue.waitFor(sl, handler, signal, DEFAULT_TIMEOUT)) {
#if 0
      ndbout_c("TransPS: Removed signal from queue (GSN: %d, QSize: %d)",
	       signal->readSignalNumber(), m_signalRecvQueue.size());
#endif
      if(handler->function != 0) {
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
TransPS::sendSignalRep(NdbApiSignal * s)
{
  m_repSender->sendSignal(s);
}

void
TransPS::sendSignalGrep(NdbApiSignal * s) 
{
  m_grepSender->sendSignal(s);
}

void
TransPS::sendFragmentedSignalRep(NdbApiSignal * s, 
				 LinearSectionPtr ptr[3], 
				 Uint32 sections)
{
  m_repSender->sendFragmentedSignal(s, ptr, sections);
}

void
TransPS::sendFragmentedSignalGrep(NdbApiSignal * s, 
				  LinearSectionPtr ptr[3], 
				  Uint32 sections)
{
  m_grepSender->sendFragmentedSignal(s, ptr, sections);
}


void 
TransPS::execNodeStatus(void* obj, Uint16 nodeId, bool alive, bool nfCompleted)
{
//  TransPS * thisObj = (TransPS*)obj;
  
  RLOG(("Node changed state (NodeId %d, Alive %d, nfCompleted %d)",
	nodeId, alive, nfCompleted));
  
  if(!alive && !nfCompleted) { }
  
  if(!alive && nfCompleted) { }
}

void
TransPS::execSignal(void* executeObj, NdbApiSignal* signal, 
		  class LinearSectionPtr ptr[3]){

  TransPS * executor = (TransPS *) executeObj;

  const Uint32 gsn = signal->readSignalNumber();
  const Uint32 len = signal->getLength();
  
  NdbApiSignal * s = new NdbApiSignal(executor->m_ownRef);
  switch(gsn){
  case GSN_REP_GET_GCI_REQ:
  case GSN_REP_GET_GCIBUFFER_REQ:
  case GSN_REP_CLEAR_PS_GCIBUFFER_REQ:
    s->set(0, SSREPBLOCKNO, gsn, len);
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);
    break;
  case GSN_GREP_SUB_CREATE_REQ:    
    {
      if(signal->m_noOfSections > 0) {
	memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
	s->set(0, GREP, gsn,
	       len);
	executor->sendFragmentedSignalGrep(s,ptr,1);
	delete s;
      } else {
	s->set(0, GREP, gsn, len);
	memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
	executor->m_signalRecvQueue.receive(s);
      }
    }
    break;
  case GSN_GREP_SUB_START_REQ:
  case GSN_GREP_SUB_SYNC_REQ:
  case GSN_GREP_SUB_REMOVE_REQ:
  case GSN_GREP_CREATE_SUBID_REQ:
    s->set(0, GREP, gsn, len);
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);
    break;
  default:
    REPABORT1("Illegal signal received in execSignal", gsn);
  }
#if 0
  ndbout_c("TransPS: Inserted signal into queue (GSN: %d, Len: %d)",
	   signal->readSignalNumber(), len);
#endif
}

/*****************************************************************************
 * Signal Receivers 
 *****************************************************************************/

void
TransPS::execREP_GET_GCIBUFFER_REQ(NdbApiSignal* signal) 
{
  RepGetGciBufferReq * req = (RepGetGciBufferReq*)signal->getDataPtr();
  Uint32 firstGCI = req->firstGCI;
  Uint32 lastGCI  = req->lastGCI;
  Uint32 nodeGrp  = req->nodeGrp;
  
  RLOG(("Received request for %d:[%d-%d]", nodeGrp, firstGCI, lastGCI));
  
  NodeGroupInfo * tmp = m_gciContainerPS->getNodeGroupInfo();
  Uint32 nodeId = tmp->getPrimaryNode(nodeGrp);
  /**
   * If there is no connected node in the nodegroup -> abort.
   * @todo: Handle error when a nodegroup is "dead"
   */
  if(!nodeId) {
    RLOG(("There are no connected nodes in node group %d", nodeGrp));
    sendREP_GET_GCIBUFFER_REF(signal, firstGCI, lastGCI, nodeGrp,
			      GrepError::REP_NO_CONNECTED_NODES);
    return;
  }

  transferPages(firstGCI, lastGCI, nodeId, nodeGrp, signal);
 
  /**
   * Done tfxing pages, sending GCIBuffer conf.
   */
  Uint32 first, last;
  m_gciContainerPS->getAvailableGCIBuffers(nodeGrp, &first, &last);  

  RepGetGciBufferConf * conf = (RepGetGciBufferConf*)req;
  conf->senderRef  = m_ownRef;
  conf->firstPSGCI = first;    // Buffers found on REP PS (piggy-back info)
  conf->lastPSGCI  = last;
  conf->firstSSGCI = firstGCI; // Now been transferred to REP SS
  conf->lastSSGCI  = lastGCI;
  conf->nodeGrp    = nodeGrp;
  signal->set(0, SSREPBLOCKNO, GSN_REP_GET_GCIBUFFER_CONF, 
	      RepGetGciBufferConf::SignalLength);
  sendSignalRep(signal);

  RLOG(("Sent %d:[%d-%d] (Stored PS:%d:[%d-%d])",
	nodeGrp, firstGCI, lastGCI, nodeGrp, first, last));
}

void 
TransPS::transferPages(Uint32 firstGCI, Uint32 lastGCI, 
		       Uint32 nodeId, Uint32 nodeGrp, 
		       NdbApiSignal * signal) 
{
  /**
   *  Transfer pages in GCI Buffer to SS
   *  When buffer is sent, send accounting information.
   */
  RepDataPage * pageData = (RepDataPage*)signal->getDataPtr();
  LinearSectionPtr ptr[1];
  GCIPage * page;
  for(Uint32 i=firstGCI; i<=lastGCI; i++) {
    Uint32 totalSizeSent = 0;
    GCIBuffer * buffer = m_gciContainerPS->getGCIBuffer(i, nodeId);

    if(buffer != 0) {   
      GCIBuffer::iterator it(buffer);
      /**
       *  Send all pages to SS
       */
      for (page = it.first(); page != 0; page = it.next()) {
	ptr[0].p = page->getStoragePtr();
	ptr[0].sz = page->getStorageWordSize();
	totalSizeSent += ptr[0].sz;
	pageData->gci     = i;
	pageData->nodeGrp = nodeGrp;
	signal->set(0, SSREPBLOCKNO, GSN_REP_DATA_PAGE, 
		    RepDataPage::SignalLength);
	sendFragmentedSignalRep(signal, ptr, 1);
      }
      
      /**
       *  Send accounting information to SS
       */ 
      RepGciBufferAccRep * rep = (RepGciBufferAccRep *)pageData;
      rep->gci = i;
      rep->nodeGrp = nodeGrp;
      rep->totalSentBytes = (4 * totalSizeSent); //words to bytes
      signal->set(0, SSREPBLOCKNO, GSN_REP_GCIBUFFER_ACC_REP, 
		  RepGciBufferAccRep::SignalLength);
      sendSignalRep(signal);
      
      RLOG(("Sending %d:[%d] (%d bytes) to external REP (nodeId %d)", 
	    nodeGrp, i, 4*totalSizeSent, nodeId));
    }
  }
  page = 0;
}

void 
TransPS::execREP_GET_GCI_REQ(NdbApiSignal* signal) 
{
  RepGetGciReq * req = (RepGetGciReq*)signal->getDataPtr();
  Uint32 nodeGrp = req->nodeGrp;

  Uint32 first, last;
  m_gciContainerPS->getAvailableGCIBuffers(nodeGrp, &first, &last);
  
  RepGetGciConf * conf = (RepGetGciConf*) req;
  conf->firstPSGCI = first;
  conf->lastPSGCI  = last;
  conf->senderRef  = m_ownRef;
  conf->nodeGrp    = nodeGrp;
  signal->set(0, SSREPBLOCKNO, GSN_REP_GET_GCI_CONF, 
	      RepGetGciConf::SignalLength);
  sendSignalRep(signal); 
}

/**
 * REP_CLEAR_PS_GCIBUFFER_REQ
 * destroy the GCI buffer in the GCI Container
 *  and send a CONF to Grep::SSCoord
 */
void 
TransPS::execREP_CLEAR_PS_GCIBUFFER_REQ(NdbApiSignal * signal) 
{
  RepClearPSGciBufferReq * const req = 
    (RepClearPSGciBufferReq*)signal->getDataPtr();
  Uint32 firstGCI     = req->firstGCI;
  Uint32 lastGCI      = req->lastGCI;
  Uint32 nodeGrp = req->nodeGrp;

  assert(firstGCI >= 0 && lastGCI > 0);
  if(firstGCI<0 && lastGCI <= 0) 
  {
    RLOG(("WARNING! Illegal delete request ignored"));
    sendREP_CLEAR_PS_GCIBUFFER_REF(signal, firstGCI, lastGCI,
				   0, nodeGrp,
				   GrepError::REP_DELETE_NEGATIVE_EPOCH);
  }

  if(firstGCI==0 && lastGCI==(Uint32)0xFFFF) {
    m_gciContainerPS->getAvailableGCIBuffers(nodeGrp, &firstGCI, &lastGCI);
    RLOG(("Deleting PS:[%d-%d]", firstGCI, lastGCI));
  }

  if(firstGCI == 0) {
    Uint32 f, l;
    m_gciContainerPS->getAvailableGCIBuffers(nodeGrp, &f, &l);

    RLOG(("Deleting PS:[%d-%d]", f, l));
    
    if(f>firstGCI)
      firstGCI = f;
  }
  
  /**
   * Delete buffer
   * Abort if we try to destroy a buffer that does not exist
   * Deleting buffer from every node in the nodegroup
   */
  for(Uint32 i=firstGCI; i<=lastGCI; i++) {    
    if(!m_gciContainerPS->destroyGCIBuffer(i, nodeGrp)) {
      sendREP_CLEAR_PS_GCIBUFFER_REF(signal, firstGCI, lastGCI, i, nodeGrp,
				     GrepError::REP_DELETE_NONEXISTING_EPOCH);
      return;
    }
    
    RLOG(("Deleted PS:%d:[%d]", nodeGrp, i));
  }  

  /**
   * Send reply to Grep::SSCoord
   */
  RepClearPSGciBufferConf * conf = (RepClearPSGciBufferConf*)req;
  conf->firstGCI = firstGCI;
  conf->lastGCI  = lastGCI;
  conf->nodeGrp  = nodeGrp;
  signal->set(0, SSREPBLOCKNO, GSN_REP_CLEAR_PS_GCIBUFFER_CONF, 
	      RepClearPSGciBufferConf::SignalLength);   
  sendSignalRep(signal);
}

/*****************************************************************************
 * Signal Senders
 *****************************************************************************/

void 
TransPS::sendREP_GET_GCI_REF(NdbApiSignal* signal,
			     Uint32 nodeGrp,
			     Uint32 firstPSGCI, Uint32 lastPSGCI,
			     GrepError::Code err)
{ 
  RepGetGciRef * ref = (RepGetGciRef *)signal->getDataPtrSend();
  ref->firstPSGCI          = firstPSGCI;
  ref->lastPSGCI           = lastPSGCI;
  ref->firstSSGCI          = 0;
  ref->lastSSGCI           = 0;
  ref->nodeGrp             = nodeGrp;
  ref->err                 = err;
  signal->set(0, SSREPBLOCKNO, GSN_REP_GET_GCI_REF, 
	      RepGetGciRef::SignalLength);   
  sendSignalRep(signal);
}

void 
TransPS::sendREP_CLEAR_PS_GCIBUFFER_REF(NdbApiSignal* signal, 
					Uint32 firstGCI, Uint32 lastGCI, 
					Uint32 currentGCI,
					Uint32 nodeGrp,
					GrepError::Code err)
{
  RepClearPSGciBufferRef * ref = 
    (RepClearPSGciBufferRef *)signal->getDataPtrSend();
  ref->firstGCI            = firstGCI;
  ref->lastGCI             = lastGCI;
  ref->currentGCI          = currentGCI; 
  ref->nodeGrp             = nodeGrp;
  ref->err                 = err;
  signal->set(0, SSREPBLOCKNO, GSN_REP_CLEAR_PS_GCIBUFFER_REF, 
	      RepClearPSGciBufferRef::SignalLength);   
  sendSignalRep(signal);
}

void
TransPS::sendREP_GET_GCIBUFFER_REF(NdbApiSignal* signal,
				   Uint32 firstGCI, Uint32 lastGCI,
				   Uint32 nodeGrp,
				   GrepError::Code err)
{
  RepGetGciBufferRef * ref = 
    (RepGetGciBufferRef *)signal->getDataPtrSend();
  ref->firstPSGCI          = firstGCI;
  ref->lastPSGCI           = lastGCI;
  ref->firstSSGCI          = 0;
  ref->lastSSGCI           = 0;
  ref->nodeGrp             = nodeGrp;
  ref->err                 = err;
  signal->set(0, SSREPBLOCKNO, GSN_REP_GET_GCIBUFFER_REF, 
	      RepGetGciBufferRef::SignalLength);   
  sendSignalRep(signal);
}
