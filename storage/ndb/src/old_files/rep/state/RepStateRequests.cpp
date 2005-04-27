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

#include "RepState.hpp"

#include <NdbApiSignal.hpp>
#include <SimpleProperties.hpp>
#include <UtilBuffer.hpp>

#include <signaldata/GrepImpl.hpp>
#include <signaldata/RepImpl.hpp>
#include <signaldata/SumaImpl.hpp>

#include <rep/rep_version.hpp>
#include "Channel.hpp"

/*****************************************************************************
 * Helper functions
 *****************************************************************************/

void
startSubscription(void * cbObj, NdbApiSignal* signal, 
		  SubscriptionData::Part part, 
		  Uint32 subId, Uint32 subKey)
{ 
  ExtSender * ext = (ExtSender *) cbObj;

  GrepSubStartReq * req = (GrepSubStartReq *)signal->getDataPtrSend();
  req->subscriptionId   = subId;
  req->subscriptionKey  = subKey;
  req->part             = (Uint32) part;
  signal->set(0, PSREPBLOCKNO, GSN_GREP_SUB_START_REQ,
	      GrepSubStartReq::SignalLength);   
  ext->sendSignal(signal);
}

void
scanSubscription(void * cbObj, NdbApiSignal* signal, 
		 SubscriptionData::Part part, 
		 Uint32 subId, Uint32 subKey)
{ 
  ExtSender * ext = (ExtSender *) cbObj;

  GrepSubSyncReq * req = (GrepSubSyncReq *)signal->getDataPtrSend();
  req->subscriptionId  = subId;
  req->subscriptionKey = subKey;
  req->part            = part;
  signal->set(0, PSREPBLOCKNO, GSN_GREP_SUB_SYNC_REQ,
	      GrepSubSyncReq::SignalLength);
  ext->sendSignal(signal);
}

/*****************************************************************************
 * RepState registered functions
 *
 * These registered functions are executed by RepState when
 * RepState needs to have stuff done.
 *****************************************************************************/

void
requestCreateSubscriptionId(void * cbObj, NdbApiSignal* signal) 
{
  ExtSender * ext = (ExtSender *) cbObj;

  CreateSubscriptionIdReq * req = 
    (CreateSubscriptionIdReq *)signal->getDataPtrSend();
  req->senderData = ext->getOwnRef();
  signal->set(0, PSREPBLOCKNO, GSN_GREP_CREATE_SUBID_REQ,
	      CreateSubscriptionIdReq::SignalLength);
  ext->sendSignal(signal);
    
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Sent request for creation of subscription id to PS");
#endif
}

void
requestCreateSubscription(void * cbObj, 
			  NdbApiSignal* signal,
			  Uint32 subId, 
			  Uint32 subKey,
			  Vector<struct table *> * selectedTables) 
{
  ExtSender * ext = (ExtSender *) cbObj;

  GrepSubCreateReq * req = (GrepSubCreateReq *)signal->getDataPtrSend();
  req->senderRef = ext->getOwnRef();
  req->subscriptionId = subId;
  req->subscriptionKey = subKey;
  if(selectedTables!=0) {
    UtilBuffer m_buffer;
    UtilBufferWriter w(m_buffer);
    LinearSectionPtr tablePtr[3];
    req->subscriptionType = SubCreateReq::SelectiveTableSnapshot;

    for(Uint32 i=0; i< selectedTables->size(); i++) {
      w.add(SimpleProperties::StringValue, (*selectedTables)[i]->tableName);
    }

    tablePtr[0].p = (Uint32*)m_buffer.get_data();
    tablePtr[0].sz = m_buffer.length() >> 2;

    signal->set(0, PSREPBLOCKNO, GSN_GREP_SUB_CREATE_REQ,
                GrepSubCreateReq::SignalLength);
    ext->sendFragmentedSignal(signal, tablePtr, 1);
  }
  else {
    req->subscriptionType = SubCreateReq::DatabaseSnapshot;
    signal->set(0, PSREPBLOCKNO, GSN_GREP_SUB_CREATE_REQ,
                GrepSubCreateReq::SignalLength);
    ext->sendFragmentedSignal(signal, 0, 0);
  }


  
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Requestor: Sent request for creation of subscription");
#endif
}

void
requestRemoveSubscription(void * cbObj, NdbApiSignal* signal, 
			  Uint32 subId, Uint32 subKey) 
{ 
  ExtSender * ext = (ExtSender *) cbObj;

  GrepSubRemoveReq * req = (GrepSubRemoveReq *)signal->getDataPtrSend();
  req->subscriptionId    = subId;
  req->subscriptionKey   = subKey;
  signal->set(0, PSREPBLOCKNO, GSN_GREP_SUB_REMOVE_REQ,
	      GrepSubRemoveReq::SignalLength);
  ext->sendSignal(signal);
}

void
requestTransfer(void * cbObj, NdbApiSignal * signal, 
		Uint32 nodeGrp, Uint32 first, Uint32 last) 
{
  ExtSender * ext = (ExtSender *) cbObj;

  RepGetGciBufferReq * req = (RepGetGciBufferReq*)signal->getDataPtrSend();
  req->firstGCI  = first;
  req->lastGCI   = last;
  req->nodeGrp   = nodeGrp;
  req->senderRef = ext->getOwnRef();
  signal->set(0, PSREPBLOCKNO, GSN_REP_GET_GCIBUFFER_REQ, 
	      RepGetGciBufferReq::SignalLength);   
  ext->sendSignal(signal);

#ifdef DEBUG_GREP_TRANSFER
  ndbout_c("Requestor: Requested PS GCI buffers %d:[%d-%d]", 
	   nodeGrp, first, last);
#endif
}

void
requestApply(void * applyObj, NdbApiSignal * signal, 
	     Uint32 nodeGrp, Uint32 first, Uint32 last, Uint32 force) 
{
  AppNDB * applier = (AppNDB *) applyObj;

  if (first != last) {
    RLOG(("WARNING! Trying to apply range [%d-%d]. This is not implemeted",
	  first, last));
  }
  /**
   * Apply GCIBuffer even if it is empty.
   */
  applier->applyBuffer(nodeGrp, first, force);
  /**
   *  @todo Handle return value from the method above
   */
}

void 
requestDeleteSS(void * cbObj, NdbApiSignal * signal, 
		Uint32 nodeGrp, Uint32 firstGCI, Uint32 lastGCI) 
{
  GCIContainer * container = (GCIContainer *) cbObj;

  RLOG(("Deleting SS:%d:[%d-%d]", nodeGrp, firstGCI, lastGCI));
  
  if(firstGCI < 0 || lastGCI<=0 || nodeGrp < 0) {
    REPABORT("Illegal interval or wrong node group"); 
    //return GrepError::REP_DELETE_NEGATIVE_EPOCH;
  }

  /*********************************************
   * All buffers : Modify to the available ones
   *********************************************/
  if(firstGCI==0 && lastGCI==(Uint32)0xFFFF) {
    container->getAvailableGCIBuffers(nodeGrp, &firstGCI, &lastGCI);
  }

  if(firstGCI == 0) {
    Uint32 f, l;
    container->getAvailableGCIBuffers(nodeGrp, &f, &l);
    RLOG(("Deleting SS:[%d-%d]", f, l));
    if(f > firstGCI) firstGCI = f;
  }

  /**
   * Delete buffers
   */
  for(Uint32 i=firstGCI; i<=lastGCI; i++) {
    if(!container->destroyGCIBuffer(i, nodeGrp)) {
      RLOG(("WARNING! Delete non-existing epoch SS:%d:[%d]", nodeGrp, i)); 
    }
    //RLOG(("RepStateRequests: Deleting buffer SS:%d:[%d]", nodeGrp, i));
  }
}

void 
requestDeletePS(void * cbObj, NdbApiSignal * signal, 
		Uint32 nodeGrp, Uint32 firstGCI, Uint32 lastGCI)
{
  ExtSender * ext = (ExtSender *) cbObj;

  RepClearPSGciBufferReq * psReq = 
    (RepClearPSGciBufferReq*)signal->getDataPtrSend();
  /**
   * @todo Should have better senderData /Lars
   */
  psReq->senderData = 4711;
  psReq->senderRef = ext->getOwnRef();
  psReq->firstGCI = firstGCI;
  psReq->lastGCI = lastGCI;
  psReq->nodeGrp = nodeGrp;
  signal->set(0, PSREPBLOCKNO, GSN_REP_CLEAR_PS_GCIBUFFER_REQ,
	      RepClearPSGciBufferReq::SignalLength);   
  ext->sendSignal(signal);
  
  RLOG(("Requesting deletion of PS:%d:[%d-%d]", nodeGrp, firstGCI, lastGCI));
}

/**
 * Function that requests information from REP PS about stored GCI Buffers
 */
void 
requestEpochInfo(void * cbObj, NdbApiSignal* signal, Uint32 nodeGrp) 
{
  ExtSender * ext = (ExtSender *) cbObj;

  RepGetGciReq * req = (RepGetGciReq *) signal->getDataPtrSend();  
  req->nodeGrp = nodeGrp;
  signal->set(0, PSREPBLOCKNO, GSN_REP_GET_GCI_REQ,
	      RepGetGciReq::SignalLength);
  ext->sendSignal(signal);
}

void
requestStartMetaLog(void * cbObj, NdbApiSignal * signal,
		    Uint32 subId, Uint32 subKey)
{ 
  RLOG(("Metalog starting. Subscription %d-%d", subId, subKey));
  startSubscription(cbObj, signal, SubscriptionData::MetaData, subId, subKey);
}

void
requestStartDataLog(void * cbObj, NdbApiSignal * signal,
		    Uint32 subId, Uint32 subKey)
{ 
  RLOG(("Datalog starting. Subscription %d-%d", subId, subKey));
  startSubscription(cbObj, signal, SubscriptionData::TableData, subId, subKey);
}

void 
requestStartMetaScan(void * cbObj, NdbApiSignal* signal,
		     Uint32 subId, Uint32 subKey)
{
  RLOG(("Metascan starting. Subscription %d-%d", subId, subKey));
  scanSubscription(cbObj, signal, SubscriptionData::MetaData, subId, subKey);
}

void 
requestStartDataScan(void * cbObj, NdbApiSignal* signal,
		     Uint32 subId, Uint32 subKey)
{
  RLOG(("Datascan starting. Subscription %d-%d", subId, subKey));
  scanSubscription(cbObj, signal, SubscriptionData::TableData, subId, subKey);
}
