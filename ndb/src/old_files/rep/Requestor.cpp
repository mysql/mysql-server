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

#include "Requestor.hpp"
#include "ConfigRetriever.hpp"

#include <NdbApiSignal.hpp>

#include <signaldata/RepImpl.hpp>
#include <signaldata/GrepImpl.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>

#include <AttributeHeader.hpp>
#include <rep/rep_version.hpp>

#define TIME_BETWEEN_EXECUTES_MS 250

/*
 * @todo The requestor still has a TF, but this is not used...
 *       (We will need a (set of) TF(s) for REP-REP 
 *       on the same system though....)
 */


/*****************************************************************************
 * Constructor / Destructor / Init
 *****************************************************************************/
Requestor::Requestor(GCIContainer * gciContainer, 
		     AppNDB * appNDB,
		     RepState * repState) 
{
  m_gciContainer = gciContainer;
  m_applier      = appNDB;
  m_repState     = repState;

  //m_grepSender = new ExtSender();
  //if (!m_grepSender) REPABORT("");

  m_repState->setSubscriptionRequests(&requestCreateSubscriptionId,
				      &requestCreateSubscription,
				      &requestRemoveSubscription);
  m_repState->setIntervalRequests(&requestTransfer, 
				  &requestApply,
				  &requestDeleteSS, 
				  &requestDeletePS);
  m_repState->setStartRequests(&requestStartMetaLog,
			       &requestStartDataLog,
			       &requestStartMetaScan,
			       &requestStartDataScan,
			       &requestEpochInfo);
}

Requestor::~Requestor() {
  //delete m_grepSender;
}

bool
Requestor::init(const char * connectString) 
{
  m_signalExecThread = NdbThread_Create(signalExecThread_C,
					(void **)this,
					32768,
					"Requestor_Service",
					NDB_THREAD_PRIO_LOW);

  if (m_signalExecThread == NULL) 
    return false;

  return true;
} 

/*****************************************************************************
 * Signal Queue Executor
 *****************************************************************************/

void *
Requestor::signalExecThread_C(void *g) {

  Requestor *requestor = (Requestor*)g;
  requestor->signalExecThreadRun();
  NdbThread_Exit(0);

  /* NOTREACHED */
  return 0;
}

class SigMatch 
{
public:
  int gsn;
  void (Requestor::* function)(NdbApiSignal *signal);
  
  SigMatch() { gsn = 0; function = NULL; };
  
  SigMatch(int _gsn, void (Requestor::* _function)(NdbApiSignal *signal)) {
    gsn = _gsn;
    function = _function;
  };
  
  bool check(NdbApiSignal *signal) {
    if(signal->readSignalNumber() == gsn)
      return true;
    return false;
  };
};

void
Requestor::signalExecThreadRun() 
{
  while(1) 
  {
    /**
     * @todo  Here we would like to measure the usage size of the 
     *        receive buffer of TransSS.  If the buffer contains
     *        more than X signals (maybe 1k or 10k), then we should 
     *        not do a protectedExecute.  
     *        By having the usage size measure thingy,
     *        we avoid having the Requestor requesting more 
     *        things than the TransSS can handle.
     *        /Lars
     *
     * @todo  A different implementation of this functionality 
     *        would be to send a signal to myself when the protected 
     *        execute is finished.  This solution could be 
     *        discussed.
     *        /Lars
     */
    m_repState->protectedExecute();
    NdbSleep_MilliSleep(TIME_BETWEEN_EXECUTES_MS);
  }
}

void 
Requestor::sendSignalRep(NdbApiSignal * s) {
  m_repSender->sendSignal(s);
}

void
Requestor::execSignal(void* executorObj, NdbApiSignal* signal, 
		   class LinearSectionPtr ptr[3]){

  Requestor * executor = (Requestor*)executorObj;  

  const Uint32 gsn = signal->readSignalNumber();
  const Uint32 len = signal->getLength();
  
  NdbApiSignal * s = new NdbApiSignal(executor->m_ownRef);
  switch (gsn) {
  case GSN_REP_GET_GCI_CONF:
  case GSN_REP_GET_GCI_REQ:
  case GSN_REP_GET_GCIBUFFER_REQ:
  case GSN_REP_INSERT_GCIBUFFER_REQ:
  case GSN_REP_CLEAR_SS_GCIBUFFER_REQ:
  case GSN_REP_CLEAR_PS_GCIBUFFER_REQ:
  case GSN_REP_DROP_TABLE_REQ:
  case GSN_GREP_SUB_CREATE_REQ:
  case GSN_GREP_SUB_START_REQ:
  case GSN_GREP_SUB_SYNC_REQ:
  case GSN_GREP_SUB_REMOVE_REQ:
  case GSN_GREP_CREATE_SUBID_REQ:
    s->set(0, PSREPBLOCKNO, gsn, len);
    memcpy(s->getDataPtrSend(), signal->getDataPtr(), 4 * len);
    executor->m_signalRecvQueue.receive(s);    
    break;
  default:
    REPABORT1("Illegal signal received in execSignal", gsn);
  }  
#if 0
  ndbout_c("Requestor: Inserted signal into queue (GSN: %d, Len: %d)",
	   signal->readSignalNumber(), len);
#endif
}
  
void 
Requestor::execNodeStatus(void* obj, Uint16 nodeId, 
			  bool alive, bool nfCompleted)
{
  //Requestor * thisObj = (Requestor*)obj;
  
  RLOG(("Node changed status (NodeId %d, Alive %d, nfCompleted %d)",
	nodeId, alive, nfCompleted));
  
  if(alive) {
    /**
     *  Connected - set node as connected
     *
     *  @todo  Make it possible to have multiple External REP nodes
     */
#if 0
    for(Uint32 i=0; i<thisObj->m_nodeConnectList.size(); i++) {
      if(thisObj->m_nodeConnectList[i]->nodeId == nodeId)
	thisObj->m_nodeConnectList[i]->connected = true;
    }
    thisObj->m_grepSender->setNodeId(thisObj->m_nodeConnectList[0]->nodeId);
#endif
  }

  if(!alive && !nfCompleted){
    /**
     *  ???
     */
  }
  
  if(!alive && nfCompleted){
    /**
     *  Re-connect
     */
  }
}
