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

#include "Grep.hpp"
#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/GrepImpl.hpp>
#include <signaldata/RepImpl.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/WaitGCP.hpp>
#include <GrepEvent.hpp>
#include <AttributeHeader.hpp>

#define CONTINUEB_DELAY 500
#define SSREPBLOCKNO 2  
#define PSREPBLOCKNO 2

//#define DEBUG_GREP
//#define DEBUG_GREP_SUBSCRIPTION
//#define DEBUG_GREP_TRANSFER
//#define DEBUG_GREP_APPLY
//#define DEBUG_GREP_DELETE

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    STARTUP of GREP Block, etc
 * ------------------------------------------------------------------------
 **************************************************************************/
static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;
void
Grep::getNodeGroupMembers(Signal* signal) {
  jam();
  /**
   * Ask DIH for nodeGroupMembers
   */
  CheckNodeGroups * sd = (CheckNodeGroups*)signal->getDataPtrSend();
  sd->blockRef = reference();
  sd->requestType =
    CheckNodeGroups::Direct |
    CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = getOwnNodeId();
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		 CheckNodeGroups::SignalLength);
  jamEntry();
  
  c_nodeGroup = sd->output;
  c_noNodesInGroup = 0;
  for (int i = 0; i < MAX_NDB_NODES; i++) {
    if (sd->mask.get(i)) {
      if (i == getOwnNodeId()) c_idInNodeGroup = c_noNodesInGroup;
      c_nodesInGroup[c_noNodesInGroup] = i;
      c_noNodesInGroup++;
    }
  }
  ndbrequire(c_noNodesInGroup > 0); // at least 1 node in the nodegroup

#ifdef NODEFAIL_DEBUG
  for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
    ndbout_c ("Grep: NodeGroup %u, me %u, me in group %u, member[%u] %u",
	      c_nodeGroup, getOwnNodeId(), c_idInNodeGroup,
	      i, c_nodesInGroup[i]);
  }
#endif
}


void
Grep::execSTTOR(Signal* signal) 
{
  jamEntry();                            
  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];
  if (startphase == 3) 
  {
    jam();
    signal->theData[0] = reference();
    g_TypeOfStart = typeOfStart;
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }
  if(startphase == 5) {
    jam();
    /**
     * we don't want any log/meta records comming to use 
     * until we are done with the recovery.
     */
    if (g_TypeOfStart == NodeState::ST_NODE_RESTART) {
      jam();
      pspart.m_recoveryMode =  true;
      getNodeGroupMembers(signal);
      for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
	Uint32 ref =numberToRef(GREP, c_nodesInGroup[i]);
	if (ref != reference())
	  sendSignal(ref, GSN_GREP_START_ME, signal,
		     1 /*SumaStartMe::SignalLength*/, JBB);
      }
    } else  pspart.m_recoveryMode =  false;

  }
 
  if(startphase == 7) {
      jam();
    if (g_TypeOfStart == NodeState::ST_NODE_RESTART) {
      pspart.m_recoveryMode =  false;
    }
  }
  
  sendSTTORRY(signal);
}


void 
Grep::PSPart::execSTART_ME(Signal* signal)
{
  jamEntry();
  GrepStartMe *   me =(GrepStartMe*)signal->getDataPtr();
  BlockReference ref = me->senderRef;
  GrepAddSubReq* const addReq = (GrepAddSubReq *)signal->getDataPtr();  


  SubscriptionPtr subPtr;
  c_subscriptions.first(c_subPtr);
  for(; !c_subPtr.isNull(); c_subscriptions.next(c_subPtr)) {
    jam();
    subPtr.i = c_subPtr.curr.i;
    subPtr.p = c_subscriptions.getPtr(subPtr.i);
    addReq->subscriptionId   = subPtr.p->m_subscriptionId;
    addReq->subscriptionKey  = subPtr.p->m_subscriptionKey;
    addReq->subscriberData   = subPtr.p->m_subscriberData;
    addReq->subscriptionType = subPtr.p->m_subscriptionType;
    addReq->senderRef        = subPtr.p->m_coordinatorRef;
    addReq->subscriberRef    =subPtr.p->m_subscriberRef;

    sendSignal(ref, 
	       GSN_GREP_ADD_SUB_REQ, 
	       signal, 
	       GrepAddSubReq::SignalLength,
	       JBB);
  }
  
  addReq->subscriptionId   = 0;
  addReq->subscriptionKey  = 0;
  addReq->subscriberData   = 0;
  addReq->subscriptionType = 0;
  addReq->senderRef        = 0;
  addReq->subscriberRef    = 0;

  sendSignal(ref, 
	     GSN_GREP_ADD_SUB_REQ, 
	     signal, 
	     GrepAddSubReq::SignalLength,
	     JBB);
}

void 
Grep::PSPart::execGREP_ADD_SUB_REQ(Signal* signal)
{
  jamEntry();
  GrepAddSubReq * const grepReq = (GrepAddSubReq *)signal->getDataPtr();
  const Uint32 subId          = grepReq->subscriptionId;
  const Uint32 subKey         = grepReq->subscriptionKey;
  const Uint32 subData        = grepReq->subscriberData;
  const Uint32 subType        = grepReq->subscriptionType;
  const Uint32 coordinatorRef = grepReq->senderRef;

  /**
   * this is ref to the REP node for this subscription.
   */
  const Uint32 subRef         = grepReq->subscriberRef;

  if(subId!=0 && subKey!=0) {
    jam();
    SubscriptionPtr subPtr;
    ndbrequire( c_subscriptionPool.seize(subPtr));
    subPtr.p->m_coordinatorRef    = coordinatorRef;
    subPtr.p->m_subscriptionId    = subId;
    subPtr.p->m_subscriptionKey   = subKey;
    subPtr.p->m_subscriberRef     = subRef;
    subPtr.p->m_subscriberData    = subData;
    subPtr.p->m_subscriptionType  = subType;
    
    c_subscriptions.add(subPtr);
  }
  else  {
    jam();
    GrepAddSubConf * conf = (GrepAddSubConf *)grepReq;
    conf->noOfSub = 
      c_subscriptionPool.getSize()-c_subscriptionPool.getNoOfFree();
    sendSignal(signal->getSendersBlockRef(),
	       GSN_GREP_ADD_SUB_CONF, 
	       signal, 
	       GrepAddSubConf::SignalLength, 
	       JBB);
  }
}

void 
Grep::PSPart::execGREP_ADD_SUB_REF(Signal* signal)
{
  /**
   * @todo fix error stuff
   */
}

void 
Grep::PSPart::execGREP_ADD_SUB_CONF(Signal* signal)
{
  jamEntry();
  GrepAddSubConf* const conf = (GrepAddSubConf *)signal->getDataPtr();
  Uint32 noOfSubscriptions = conf->noOfSub;
  Uint32 noOfRestoredSubscriptions = 
    c_subscriptionPool.getSize()-c_subscriptionPool.getNoOfFree();
  if(noOfSubscriptions!=noOfRestoredSubscriptions) {
    jam();
    /**
     *@todo send ref signal
     */ 
    ndbrequire(false);
  }
}

void
Grep::execREAD_NODESCONF(Signal* signal) 
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();
  
#if 0
  ndbout_c("Grep: Recd READ_NODESCONF");
#endif
  
  /******************************
   * Check which REP nodes exist
   ******************************/
  Uint32 i;
  for (i = 1; i < MAX_NODES; i++) 
  {
    jam();
#if 0
    ndbout_c("Grep: Found node %d of type %d", i, getNodeInfo(i).getType());
#endif
    if (getNodeInfo(i).getType() == NodeInfo::REP)
    {
      jam();
      /**
       * @todo  This should work for more than ONE rep node!
       */
      pscoord.m_repRef = numberToRef(PSREPBLOCKNO, i);
      pspart.m_repRef = numberToRef(PSREPBLOCKNO, i);
#if 0
      ndbout_c("Grep: REP node %d detected", i);
#endif
    }
  }
  
  /*****************************
   * Check which DB nodes exist
   *****************************/
  m_aliveNodes.clear();

  Uint32 count = 0;
  for(i = 0; i<MAX_NDB_NODES; i++) 
  {
    if (NodeBitmask::get(conf->allNodes, i)) 
    {
      jam();
      count++;

      NodePtr node;
      ndbrequire(m_nodes.seize(node));
      
      node.p->nodeId = i;
      if (NodeBitmask::get(conf->inactiveNodes, i)) 
      {
	node.p->alive = 0;
      } 
      else 
      {
	node.p->alive = 1;
	m_aliveNodes.set(i);
      }
    }
  }
  m_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void
Grep::sendSTTORRY(Signal* signal) 
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 5;
  signal->theData[6] = 7;
  signal->theData[7] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 8, JBB);
}

void
Grep::execNDB_STTOR(Signal* signal) 
{
  jamEntry();                            
}

void
Grep::execDUMP_STATE_ORD(Signal* signal) 
{
  jamEntry();
  //Uint32 tCase = signal->theData[0];

#if 0
  if(sscoord.m_repRef == 0) 
  {
    ndbout << "Grep: Recd DUMP signal but has no connection with REP node"
	   << endl;
    return;
  }
#endif 

  /*
  switch (tCase) 
  {
  case 8100: sscoord.grepReq(signal, GrepReq::START_SUBSCR); break;
  case 8102: sscoord.grepReq(signal, GrepReq::START_METALOG); break;
  case 8104: sscoord.grepReq(signal, GrepReq::START_METASCAN); break;
  case 8106: sscoord.grepReq(signal, GrepReq::START_DATALOG); break;
  case 8108: sscoord.grepReq(signal, GrepReq::START_DATASCAN); break;
  case 8110: sscoord.grepReq(signal, GrepReq::STOP_SUBSCR); break;
  case 8500: sscoord.grepReq(signal, GrepReq::REMOVE_BUFFERS); break;
  case 8300: sscoord.grepReq(signal, GrepReq::SLOWSTOP); break;
  case 8400: sscoord.grepReq(signal, GrepReq::FASTSTOP); break;
  case 8600: sscoord.grepReq(signal, GrepReq::CREATE_SUBSCR); break;
  case 8700: sscoord.dropTable(signal,(Uint32)signal->theData[1]);break;
  default: break;
  }
  */
}

/**
 *  Signal received when REP node has failed
 */
void 
Grep::execAPI_FAILREQ(Signal* signal) 
{
  jamEntry();
  //Uint32          failedApiNode = signal->theData[0];
  //BlockReference  retRef = signal->theData[1];
  
  /**
   * @todo We should probably do something smart if the 
   *       PS REP node fails???? /Lars
   */

#if 0
  ndbout_c("Grep: API_FAILREQ received for API node %d.", failedApiNode);
#endif
  
  /**
   * @note  This signal received is NOT allowed to send any CONF
   *        signal, since this would screw up TC/DICT to API 
   *        "connections".
   */
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    GREP Control
 * ------------------------------------------------------------------------
 **************************************************************************/
void
Grep::execGREP_REQ(Signal* signal) 
{
  jamEntry();
  
  //GrepReq * req = (GrepReq *)signal->getDataPtr();
  
  /**
   * @todo Fix so that request is redirected to REP Server
   *  Obsolete?
   * Was:   sscoord.grepReq(signal, req->request);
   */
  ndbout_c("Warning! REP commands can only be executed at REP SERVER prompt!");
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    NODE STATE HANDLING
 * ------------------------------------------------------------------------
 **************************************************************************/
void
Grep::execNODE_FAILREP(Signal* signal) 
{
  jamEntry();
  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  bool changed = false;

  NodePtr nodePtr;
  for(m_nodes.first(nodePtr); nodePtr.i != RNIL; m_nodes.next(nodePtr)) 
  {
    jam();
    if (NodeBitmask::get(rep->theNodes, nodePtr.p->nodeId)) 
    {
      jam();
      
      if (nodePtr.p->alive) 
      {
	jam();
	ndbassert(m_aliveNodes.get(nodePtr.p->nodeId));
	changed = true;
      } 
      else 
      {
	ndbassert(!m_aliveNodes.get(nodePtr.p->nodeId));
      }
      
      nodePtr.p->alive = 0;
      m_aliveNodes.clear(nodePtr.p->nodeId);
    }
  }


  /**
   * Problem: Fix a node failure running a protocol
   * 
   * 1. Coordinator node of a protocol dies
   *      - Elect a new coordinator
   *      - send ref to user
   *      
   * 2. Non-coordinator dies.
   *      - make coordinator aware of this
   *        so that coordinator does not wait for 
   *        conf from faulty node
   *      - node recovery will restore the non-coordinator.
   *        
   */
}

void
Grep::execINCL_NODEREQ(Signal* signal) 
{
  jamEntry();
  
  //const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(m_nodes.first(node); node.i != RNIL; m_nodes.next(node)) 
  {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if (inclNode == nodeId) {
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbassert(!m_aliveNodes.get(nodeId));
      
      node.p->alive = 1;
      m_aliveNodes.set(nodeId);
      
      break;
    }
  }

  /**
   * @todo:  if we include this DIH's got to be prepared, later if needed...
   */
#if 0 
  signal->theData[0] = reference();
  
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 1, JBB);
#endif  
}


/**
 * Helper methods 
 */
void 
Grep::PSCoord::prepareOperationRec(SubCoordinatorPtr subPtr, 
				   BlockReference subscriber,
				   Uint32 subId,
				   Uint32 subKey,
				   Uint32 request) 
{
  subPtr.p->m_coordinatorRef     = reference();
  subPtr.p->m_subscriberRef      = subscriber;
  subPtr.p->m_subscriberData     = subPtr.i;
  subPtr.p->m_subscriptionId     = subId;
  subPtr.p->m_subscriptionKey    = subKey;
  subPtr.p->m_outstandingRequest = request;
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    CREATE SUBSCRIPTION ID
 * ------------------------------------------------------------------------
 * 
 *  Requests SUMA to create a unique subscription id 
 **************************************************************************/

void 
Grep::PSCoord::execGREP_CREATE_SUBID_REQ(Signal* signal) 
{
  jamEntry();

  CreateSubscriptionIdReq * req = 
    (CreateSubscriptionIdReq*)signal->getDataPtr();
  BlockReference ref = signal->getSendersBlockRef();
  
  SubCoordinatorPtr subPtr;
  if( !c_subCoordinatorPool.seize(subPtr)) {
    jam();
    SubCoordinator sub;
    sub.m_subscriberRef   = ref;
    sub.m_subscriptionId  = 0;
    sub.m_subscriptionKey = 0;
    sendRefToSS(signal, sub, GrepError::SUBSCRIPTION_ID_NOMEM );
    return;
  }
  prepareOperationRec(subPtr,
		      ref,
		      0,0,
		      GSN_CREATE_SUBID_REQ);

  
  ndbout_c("SUBID_REQ  Ref %d",ref);
  req->senderData=subPtr.p->m_subscriberData;

  sendSignal(SUMA_REF, GSN_CREATE_SUBID_REQ, signal, 
	     SubCreateReq::SignalLength, JBB);    

#if 1 //def DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSCoord: Sent CREATE_SUBID_REQ to SUMA");
#endif
}

void 
Grep::PSCoord::execCREATE_SUBID_CONF(Signal* signal) 
{
  jamEntry();
  CreateSubscriptionIdConf const * conf = 
    (CreateSubscriptionIdConf *)signal->getDataPtr();
  Uint32 subId    = conf->subscriptionId;
  Uint32 subKey   = conf->subscriptionKey;
  Uint32 subData  = conf->subscriberData;

#if 1 //def DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSCoord: Recd GREP_SUBID_CONF (subId:%d, subKey:%d)", 
	   subId, subKey);
#endif

  SubCoordinatorPtr subPtr;
  c_subCoordinatorPool.getPtr(subPtr, subData);
  BlockReference repRef = subPtr.p->m_subscriberRef;
  
  { // Check that id/key is unique
    SubCoordinator key;
    SubCoordinatorPtr tmp;
    key.m_subscriptionId  = subId;
    key.m_subscriptionKey = subKey;
    if(c_runningSubscriptions.find(tmp, key)){
      jam();
      SubCoordinator sub;
      sub.m_subscriberRef=repRef;
      sub.m_subscriptionId = subId;
      sub.m_subscriptionKey = subKey;
      sendRefToSS(signal,sub, GrepError::SUBSCRIPTION_ID_NOT_UNIQUE );
      return;
    }
  }
  
  sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_CREATE_SUBID_CONF, signal, 
	     CreateSubscriptionIdConf::SignalLength, JBB);
  c_subCoordinatorPool.release(subData);
  
  m_grep->sendEventRep(signal,
			 EventReport::GrepSubscriptionInfo, 
			 GrepEvent::GrepPS_CreateSubIdConf,
			 subId,
			 subKey,
			 (Uint32)GrepError::GE_NO_ERROR);   
}

void 
Grep::PSCoord::execCREATE_SUBID_REF(Signal* signal) {
  jamEntry();
  CreateSubscriptionIdRef const * ref = 
    (CreateSubscriptionIdRef *)signal->getDataPtr();
  Uint32 subData = ref->subscriberData;
  GrepError::GE_Code err;
  
  Uint32 sendersBlockRef = signal->getSendersBlockRef();
  if(sendersBlockRef == SUMA_REF) 
  {
    jam();
    err = GrepError::SUBSCRIPTION_ID_SUMA_FAILED_CREATE;
  } else {
    jam();
    ndbrequire(false); /* Added since errorcode err unhandled
			* TODO: fix correct errorcode
			*/
    err= GrepError::GE_NO_ERROR; // remove compiler warning
  }

  SubCoordinatorPtr subPtr;
  c_runningSubscriptions.getPtr(subPtr, subData);
  BlockReference repref = subPtr.p->m_subscriberRef;
  
  SubCoordinator sub;
  sub.m_subscriberRef   = repref;
  sub.m_subscriptionId  = 0;
  sub.m_subscriptionKey = 0;
  sendRefToSS(signal,sub, err);

}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    CREATE SUBSCRIPTION
 * ------------------------------------------------------------------------
 * 
 *  Creates a subscription for every GREP to its local SUMA.
 *  GREP node that executes createSubscription becomes the GREP Coord.
 **************************************************************************/

/**
 * Request to create a subscription (sent from SS)
 */
void
Grep::PSCoord::execGREP_SUB_CREATE_REQ(Signal* signal) 
{
  jamEntry();
  GrepSubCreateReq const * grepReq = (GrepSubCreateReq *)signal->getDataPtr();
  Uint32 subId           = grepReq->subscriptionId;
  Uint32 subKey          = grepReq->subscriptionKey;
  Uint32 subType         = grepReq->subscriptionType;
  BlockReference rep     = signal->getSendersBlockRef();

  GrepCreateReq * req    =(GrepCreateReq*)grepReq;

  SubCoordinatorPtr subPtr;

  if( !c_subCoordinatorPool.seize(subPtr)) {
    jam();
    SubCoordinator sub;
    sub.m_subscriberRef = rep;
    sub.m_subscriptionId = 0;
    sub.m_subscriptionKey = 0;
    sub.m_outstandingRequest = GSN_GREP_CREATE_REQ;
    sendRefToSS(signal, sub, GrepError::NOSPACE_IN_POOL);
    return;
  }
  prepareOperationRec(subPtr,
		      numberToRef(PSREPBLOCKNO, refToNode(rep)), subId, subKey,
		      GSN_GREP_CREATE_REQ);

  /* Get the payload of the signal.
   */
  SegmentedSectionPtr selectedTablesPtr;
  if(subType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    ndbrequire(signal->getNoOfSections()==1);    
    signal->getSection(selectedTablesPtr,0);
    signal->header.m_noOfSections = 0;
  }
  /**
   * Prepare the signal to be sent to Grep participatns
   */
  subPtr.p->m_subscriptionType = subType;
  req->senderRef        = reference();
  req->subscriberRef    = numberToRef(PSREPBLOCKNO, refToNode(rep));
  req->subscriberData   = subPtr.p->m_subscriberData;
  req->subscriptionId   = subId; 
  req->subscriptionKey  = subKey; 
  req->subscriptionType = subType;

  /*add payload if it is a selectivetablesnap*/
  if(subType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    signal->setSection(selectedTablesPtr, 0);
  }

  /******************************
   * Send to all PS participants
   ******************************/
  NodeReceiverGroup rg(GREP,  m_grep->m_aliveNodes);
  subPtr.p->m_outstandingParticipants = rg;
  sendSignal(rg,
	     GSN_GREP_CREATE_REQ, signal, 
	     GrepCreateReq::SignalLength, JBB);


#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSCoord: Sent GREP_CREATE_REQ "
	   "(subId:%d, subKey:%d, subData:%d, subType:%d) to parts",
	   subId, subKey, subPtr.p->m_subscriberData, subType);
#endif
}

void 
Grep::PSPart::execGREP_CREATE_REQ(Signal* signal) 
{
  jamEntry();
  GrepCreateReq * const grepReq = (GrepCreateReq *)signal->getDataPtr();
  const Uint32 subId          = grepReq->subscriptionId;
  const Uint32 subKey         = grepReq->subscriptionKey;
  const Uint32 subData        = grepReq->subscriberData;
  const Uint32 subType        = grepReq->subscriptionType;
  const Uint32 coordinatorRef = grepReq->senderRef;
  const Uint32 subRef         = grepReq->subscriberRef; //this is ref to the
                                                        //REP node for this 
                                                        //subscription.

  SubscriptionPtr subPtr;
  ndbrequire( c_subscriptionPool.seize(subPtr));
  subPtr.p->m_coordinatorRef     = coordinatorRef;
  subPtr.p->m_subscriptionId     = subId;
  subPtr.p->m_subscriptionKey    = subKey;
  subPtr.p->m_subscriberRef      = subRef;
  subPtr.p->m_subscriberData     = subPtr.i;
  subPtr.p->m_subscriptionType   = subType;
  subPtr.p->m_outstandingRequest = GSN_GREP_CREATE_REQ; 
  subPtr.p->m_operationPtrI      = subData;

  c_subscriptions.add(subPtr);

  SegmentedSectionPtr selectedTablesPtr;
  if(subType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    ndbrequire(signal->getNoOfSections()==1);
    signal->getSection(selectedTablesPtr,0);// SubCreateReq::TABLE_LIST);
    signal->header.m_noOfSections = 0;
  }

  /**
   * Prepare signal to be sent to SUMA
   */
  SubCreateReq * sumaReq = (SubCreateReq *)grepReq;
  sumaReq->subscriberRef    = GREP_REF;
  sumaReq->subscriberData   = subPtr.p->m_subscriberData;
  sumaReq->subscriptionId   = subPtr.p->m_subscriptionId; 
  sumaReq->subscriptionKey  = subPtr.p->m_subscriptionKey;
  sumaReq->subscriptionType = subPtr.p->m_subscriptionType;
  /*add payload if it is a selectivetablesnap*/
  if(subType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    signal->setSection(selectedTablesPtr, 0);
  }  
  sendSignal(SUMA_REF, 
	     GSN_SUB_CREATE_REQ, 
	     signal, 
	     SubCreateReq::SignalLength, 
	     JBB);
}

void
Grep::PSPart::execSUB_CREATE_CONF(Signal* signal) 
{
  jamEntry();  

  SubCreateConf * const conf = (SubCreateConf *)signal->getDataPtr();
  Uint32 subData             = conf->subscriberData;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  /**
     @todo check why this can fuck up -johan
     
     ndbrequire(subPtr.p->m_subscriptionId  == conf->subscriptionId);
     ndbrequire(subPtr.p->m_subscriptionKey == conf->subscriptionKey);
  */
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSPart: Recd SUB_CREATE_CONF "
	   "(subId:%d, subKey:%d) from SUMA",
	   conf->subscriptionId, conf->subscriptionKey);
#endif

  /*********************
   * Send conf to coord
   *********************/
  GrepCreateConf * grepConf = (GrepCreateConf*)conf;
  grepConf->senderNodeId = getOwnNodeId();
  grepConf->senderData = subPtr.p->m_operationPtrI;
  sendSignal(subPtr.p->m_coordinatorRef, GSN_GREP_CREATE_CONF, signal, 
	     GrepCreateConf::SignalLength, JBB);    
  subPtr.p->m_outstandingRequest = 0; 
}

/**
 * Handle errors that either occured in:
 * 1) PSPart
 * or
 * 2) propagated from local SUMA
 */
void 
Grep::PSPart::execSUB_CREATE_REF(Signal* signal) 
{
  jamEntry();
  SubCreateRef * const ref = (SubCreateRef *)signal->getDataPtr();
  Uint32 subData           = ref->subscriberData;
  GrepError::GE_Code err      = (GrepError::GE_Code)ref->err;
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  sendRefToPSCoord(signal, *subPtr.p, err /*error*/);
  subPtr.p->m_outstandingRequest = 0;
}

void 
Grep::PSCoord::execGREP_CREATE_CONF(Signal* signal) 
{
  jamEntry();
  GrepCreateConf const * conf = (GrepCreateConf *)signal->getDataPtr();
  Uint32 subData       = conf->senderData;
  Uint32 nodeId        = conf->senderNodeId;

  SubCoordinatorPtr subPtr;
  c_subCoordinatorPool.getPtr(subPtr, subData);
  
  ndbrequire(subPtr.p->m_outstandingRequest == GSN_GREP_CREATE_REQ);
  
  subPtr.p->m_outstandingParticipants.clearWaitingFor(nodeId);
  
  if(!subPtr.p->m_outstandingParticipants.done()) return;
  /********************************
   * All participants have CONF:ed
   ********************************/
  Uint32 subId = subPtr.p->m_subscriptionId;
  Uint32 subKey = subPtr.p->m_subscriptionKey;
    
  GrepSubCreateConf * grepConf = (GrepSubCreateConf *)signal->getDataPtr();
  grepConf->subscriptionId  = subId;
  grepConf->subscriptionKey = subKey;
  sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_SUB_CREATE_CONF, signal, 
	     GrepSubCreateConf::SignalLength, JBB);

  /**
   * Send event report
   */
  m_grep->sendEventRep(signal,
		       EventReport::GrepSubscriptionInfo,
		       GrepEvent::GrepPS_SubCreateConf,
		       subId,
		       subKey,
		       (Uint32)GrepError::GE_NO_ERROR);

  c_subCoordinatorPool.release(subPtr);

}

/**
 * Handle errors that either occured in:
 * 1) PSCoord
 * or
 * 2) propagated from PSPart 
 */
void 
Grep::PSCoord::execGREP_CREATE_REF(Signal* signal) 
{
  jamEntry();
  GrepCreateRef * const ref = (GrepCreateRef *)signal->getDataPtr();
  Uint32 subData = ref->senderData;
  Uint32 err     = ref->err;
  SubCoordinatorPtr subPtr;
  c_runningSubscriptions.getPtr(subPtr, subData);  
 
  sendRefToSS(signal, *subPtr.p, (GrepError::GE_Code)err /*error*/);
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    START SUBSCRIPTION
 * ------------------------------------------------------------------------
 * 
 *  Starts a subscription at SUMA.  
 *  Each participant starts its own subscription.
 **************************************************************************/

/**
 * Request to start subscription (Sent from SS)
 */
void
Grep::PSCoord::execGREP_SUB_START_REQ(Signal* signal) 
{
  jamEntry();
  GrepSubStartReq * const subReq = (GrepSubStartReq *)signal->getDataPtr();
  SubscriptionData::Part part    = (SubscriptionData::Part) subReq->part;
  Uint32 subId                   = subReq->subscriptionId;
  Uint32 subKey                  = subReq->subscriptionKey;
  BlockReference rep             = signal->getSendersBlockRef();

  SubCoordinatorPtr subPtr;

  if(!c_subCoordinatorPool.seize(subPtr)) {
    jam();
    SubCoordinator sub;
    sub.m_subscriberRef = rep;
    sub.m_subscriptionId = 0;
    sub.m_subscriptionKey = 0;
    sub.m_outstandingRequest = GSN_GREP_START_REQ;
    sendRefToSS(signal, sub, GrepError::NOSPACE_IN_POOL);
    return;
  }
  
  prepareOperationRec(subPtr,
		      numberToRef(PSREPBLOCKNO, refToNode(rep)), 
		      subId, subKey,
		      GSN_GREP_START_REQ);
 
  GrepStartReq * const req    = (GrepStartReq *) subReq;
  req->part                   = (Uint32) part;
  req->subscriptionId         = subPtr.p->m_subscriptionId;
  req->subscriptionKey        = subPtr.p->m_subscriptionKey;
  req->senderData             = subPtr.p->m_subscriberData;

  /***************************
   * Send to all participants
   ***************************/
  NodeReceiverGroup rg(GREP,  m_grep->m_aliveNodes);
  subPtr.p->m_outstandingParticipants = rg;
  sendSignal(rg,
	     GSN_GREP_START_REQ, 
	     signal, 
	     GrepStartReq::SignalLength, JBB);

#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSCoord: Sent GREP_START_REQ "
	   "(subId:%d, subKey:%d, senderData:%d, part:%d) to all participants",
	   req->subscriptionId, req->subscriptionKey, req->senderData, part);
#endif
}


void 
Grep::PSPart::execGREP_START_REQ(Signal* signal) 
{
  jamEntry();
  GrepStartReq * const grepReq = (GrepStartReq *) signal->getDataPtr();    
  SubscriptionData::Part part  = (SubscriptionData::Part)grepReq->part;
  Uint32 subId                 = grepReq->subscriptionId;
  Uint32 subKey                = grepReq->subscriptionKey;
  Uint32 operationPtrI         = grepReq->senderData;
  
  Subscription key;
  key.m_subscriptionId        = subId;
  key.m_subscriptionKey       = subKey;
  SubscriptionPtr subPtr;
  ndbrequire(c_subscriptions.find(subPtr, key));;
  subPtr.p->m_outstandingRequest = GSN_GREP_START_REQ; 
  subPtr.p->m_operationPtrI = operationPtrI;
  /**
   * send SUB_START_REQ to local SUMA
   */
  SubStartReq * sumaReq    = (SubStartReq *) grepReq;
  sumaReq->subscriptionId  = subId; 
  sumaReq->subscriptionKey = subKey;
  sumaReq->subscriberData  = subPtr.i;
  sumaReq->part            = (Uint32) part;

  sendSignal(SUMA_REF, GSN_SUB_START_REQ, signal, 
	     SubStartReq::SignalLength, JBB);  
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSPart: Sent SUB_START_REQ (subId:%d, subKey:%d, part:%d)", 
	   subId, subKey, (Uint32)part);
#endif
}


void
Grep::PSPart::execSUB_START_CONF(Signal* signal) 
{
  jamEntry();
  
  SubStartConf * const conf = (SubStartConf *) signal->getDataPtr();
  SubscriptionData::Part part = (SubscriptionData::Part)conf->part;
  Uint32 subId                = conf->subscriptionId;
  Uint32 subKey               = conf->subscriptionKey;
  Uint32 subData              = conf->subscriberData;
  Uint32 firstGCI             = conf->firstGCI;
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSPart: Recd SUB_START_CONF "
	   "(subId:%d, subKey:%d, subData:%d)",
	   subId, subKey, subData);
#endif

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  ndbrequire(subPtr.p->m_subscriptionId  == subId);
  ndbrequire(subPtr.p->m_subscriptionKey == subKey);

  GrepStartConf * grepConf = (GrepStartConf *)conf;
  grepConf->senderData      = subPtr.p->m_operationPtrI;
  grepConf->part            = (Uint32) part;
  grepConf->subscriptionKey = subKey;
  grepConf->subscriptionId  = subId;
  grepConf->firstGCI        = firstGCI;
  grepConf->senderNodeId    = getOwnNodeId();
  sendSignal(subPtr.p->m_coordinatorRef, GSN_GREP_START_CONF, signal, 
	     GrepStartConf::SignalLength, JBB);
  subPtr.p->m_outstandingRequest = 0; 

#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSPart: Sent GREP_START_CONF "
	   "(subId:%d, subKey:%d, subData:%d, part:%d)",
	   subId, subKey, subData, part);
#endif
}


/**
 * Handle errors that either occured in:
 * 1) PSPart
 * or
 * 2) propagated from local SUMA
 *  
 * Propagates REF signal to PSCoord
 */
void 
Grep::PSPart::execSUB_START_REF(Signal* signal) 
{
  SubStartRef * const ref = (SubStartRef *)signal->getDataPtr();
  Uint32 subData          = ref->subscriberData;
  GrepError::GE_Code err     = (GrepError::GE_Code)ref->err;
  SubscriptionData::Part part = (SubscriptionData::Part)ref->part;
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  sendRefToPSCoord(signal, *subPtr.p, err /*error*/, part);
  subPtr.p->m_outstandingRequest = 0;
}


/**
 *  Logging has started... (says PS Participant)
 */
void 
Grep::PSCoord::execGREP_START_CONF(Signal* signal) 
{
  jamEntry();  

  GrepStartConf * const conf = (GrepStartConf *) signal->getDataPtr();
  Uint32 subData              = conf->senderData;
  SubscriptionData::Part part = (SubscriptionData::Part)conf->part;
  Uint32 subId                = conf->subscriptionId;
  Uint32 subKey               = conf->subscriptionKey;
  Uint32 firstGCI             = conf->firstGCI;

  SubCoordinatorPtr subPtr;
  c_subCoordinatorPool.getPtr(subPtr, subData);
  ndbrequire(subPtr.p->m_outstandingRequest == GSN_GREP_START_REQ);

  subPtr.p->m_outstandingParticipants.clearWaitingFor(conf->senderNodeId);

  if(!subPtr.p->m_outstandingParticipants.done()) return;
  jam();
  
  /*************************
   * All participants ready 
   *************************/
  GrepSubStartConf * grepConf = (GrepSubStartConf *) conf;
  grepConf->part              = part;
  grepConf->subscriptionId    = subId;
  grepConf->subscriptionKey   = subKey;
  grepConf->firstGCI          = firstGCI;

  bool ok = false;
  switch(part) {
  case SubscriptionData::MetaData:
    ok = true;
    sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_SUB_START_CONF, signal, 
	       GrepSubStartConf::SignalLength, JBB);
    
    /**
     * Send event report
     */
    m_grep->sendEventRep(signal,
			 EventReport::GrepSubscriptionInfo,
			 GrepEvent::GrepPS_SubStartMetaConf,
			 subId, subKey,
			 (Uint32)GrepError::GE_NO_ERROR);
    
    c_subCoordinatorPool.release(subPtr);
    break;
  case SubscriptionData::TableData:
    ok = true;
    sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_SUB_START_CONF, signal, 
	       GrepSubStartConf::SignalLength, JBB);

    /**
     * Send event report
     */
    m_grep->sendEventRep(signal,
			 EventReport::GrepSubscriptionInfo,
			 GrepEvent::GrepPS_SubStartDataConf,
			 subId, subKey,
			 (Uint32)GrepError::GE_NO_ERROR);
    

    c_subCoordinatorPool.release(subPtr);
    break;
  }
  ndbrequire(ok);

#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSCoord: Recd SUB_START_CONF (subId:%d, subKey:%d, part:%d) "
	   "from all slaves",
	   subId, subKey, (Uint32)part); 
#endif
}

/**
 * Handle errors that either occured in:
 * 1) PSCoord
 * or
 * 2) propagated from PSPart 
 */
void 
Grep::PSCoord::execGREP_START_REF(Signal* signal) 
{
  jamEntry();
  GrepStartRef * const ref = (GrepStartRef *)signal->getDataPtr();
  Uint32 subData           = ref->senderData;
  GrepError::GE_Code err      = (GrepError::GE_Code)ref->err;
  SubscriptionData::Part part  = (SubscriptionData::Part)ref->part;

  SubCoordinatorPtr subPtr;
  c_runningSubscriptions.getPtr(subPtr, subData);  
  sendRefToSS(signal, *subPtr.p, err /*error*/, part);
}
 
/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    REMOVE SUBSCRIPTION
 * ------------------------------------------------------------------------
 * 
 *  Remove a subscription at SUMA.  
 *  Each participant removes its own subscription.
 *  We start by deleting the subscription inside the requestor
 *  since, we don't know if nodes (REP nodes or DB nodes) 
 *  have disconnected after we sent out this and 
 *  if we dont delete the sub in the requestor now, 
 *  we won't be able to create a new subscription
 **************************************************************************/

/**
 * Request to abort subscription (Sent from SS)
 */
void
Grep::PSCoord::execGREP_SUB_REMOVE_REQ(Signal* signal) 
{
  jamEntry();
  GrepSubRemoveReq * const subReq = (GrepSubRemoveReq *)signal->getDataPtr();
  Uint32 subId           = subReq->subscriptionId;
  Uint32 subKey          = subReq->subscriptionKey;
  BlockReference rep     = signal->getSendersBlockRef(); 

  SubCoordinatorPtr subPtr;
  if( !c_subCoordinatorPool.seize(subPtr)) {
    jam();
    SubCoordinator sub;
    sub.m_subscriberRef = rep;
    sub.m_subscriptionId = 0;
    sub.m_subscriptionKey = 0;
    sub.m_outstandingRequest = GSN_GREP_REMOVE_REQ;
    sendRefToSS(signal, sub, GrepError::NOSPACE_IN_POOL);
    return;
  }


  prepareOperationRec(subPtr,
		      numberToRef(PSREPBLOCKNO, refToNode(rep)), 
		      subId, subKey,
		      GSN_GREP_REMOVE_REQ);

  c_runningSubscriptions.add(subPtr);

  GrepRemoveReq * req         = (GrepRemoveReq *) subReq;
  req->subscriptionId         = subPtr.p->m_subscriptionId;
  req->subscriptionKey        = subPtr.p->m_subscriptionKey;
  req->senderData             = subPtr.p->m_subscriberData;
  req->senderRef              = subPtr.p->m_coordinatorRef;

  /***************************
   * Send to all participants
   ***************************/
  NodeReceiverGroup rg(GREP,  m_grep->m_aliveNodes);
  subPtr.p->m_outstandingParticipants = rg;
  sendSignal(rg,
	     GSN_GREP_REMOVE_REQ, signal, 
	     GrepRemoveReq::SignalLength, JBB);
}


void 
Grep::PSPart::execGREP_REMOVE_REQ(Signal* signal)
{
  jamEntry();
  GrepRemoveReq * const grepReq = (GrepRemoveReq *) signal->getDataPtr();    
  Uint32 subId        = grepReq->subscriptionId;
  Uint32 subKey       = grepReq->subscriptionKey;
  Uint32 subData      = grepReq->senderData;
  Uint32 coordinator  = grepReq->senderRef;

  Subscription key;
  key.m_subscriptionId        = subId;
  key.m_subscriptionKey       = subKey;
  SubscriptionPtr subPtr;
  
  if(!c_subscriptions.find(subPtr, key))
    {
      /**
       * The subscription was not found, so it must be deleted.
       * Send CONF back, since it does not exist (thus, it is removed)
       */
      GrepRemoveConf * grepConf = (GrepRemoveConf *)grepReq;
      grepConf->subscriptionKey = subKey;
      grepConf->subscriptionId  = subId;
      grepConf->senderData      = subData;
      grepConf->senderNodeId    = getOwnNodeId();
      sendSignal(coordinator, GSN_GREP_REMOVE_CONF, signal, 
		 GrepRemoveConf::SignalLength, JBB);
      return;      
    }

  subPtr.p->m_operationPtrI = subData;
  subPtr.p->m_coordinatorRef = coordinator;
  subPtr.p->m_outstandingRequest = GSN_GREP_REMOVE_REQ; 

  /**
   * send SUB_REMOVE_REQ to local SUMA
   */
  SubRemoveReq * sumaReq   = (SubRemoveReq *) grepReq;
  sumaReq->subscriptionId  = subId; 
  sumaReq->subscriptionKey = subKey;
  sumaReq->senderData  = subPtr.i;
  sendSignal(SUMA_REF, GSN_SUB_REMOVE_REQ, signal, 
	     SubStartReq::SignalLength, JBB);  
}


/**
 * SUB_REMOVE_CONF (from local SUMA)
 */
void
Grep::PSPart::execSUB_REMOVE_CONF(Signal* signal) 
{
  jamEntry();
  SubRemoveConf * const conf = (SubRemoveConf *) signal->getDataPtr();
  Uint32 subId     = conf->subscriptionId;
  Uint32 subKey    = conf->subscriptionKey;
  Uint32 subData   = conf->subscriberData;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  ndbrequire(subPtr.p->m_subscriptionId  == subId);
  ndbrequire(subPtr.p->m_subscriptionKey == subKey);
  subPtr.p->m_outstandingRequest = 0; 
  GrepRemoveConf * grepConf = (GrepRemoveConf *)conf;
  grepConf->subscriptionKey = subKey;
  grepConf->subscriptionId  = subId;
  grepConf->senderData      = subPtr.p->m_operationPtrI;
  grepConf->senderNodeId    = getOwnNodeId();
  sendSignal(subPtr.p->m_coordinatorRef, GSN_GREP_REMOVE_CONF, signal, 
	     GrepRemoveConf::SignalLength, JBB);
  c_subscriptions.release(subPtr);  

}


/**
 * SUB_REMOVE_CONF (from local SUMA)
 */
void
Grep::PSPart::execSUB_REMOVE_REF(Signal* signal) 
{
  jamEntry();
  SubRemoveRef * const ref = (SubRemoveRef *)signal->getDataPtr();
  Uint32 subData           = ref->subscriberData;
  /*  GrepError::GE_Code err      = (GrepError::GE_Code)ref->err;*/
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  
  //sendSubRemoveRef_PSCoord(signal, *subPtr.p, err /*error*/);
}


/**
 *  Aborting has been carried out (says Participants)
 */
void 
Grep::PSCoord::execGREP_REMOVE_CONF(Signal* signal) 
{
  jamEntry();
  GrepRemoveConf * const conf = (GrepRemoveConf *) signal->getDataPtr();
  Uint32 subId                = conf->subscriptionId;
  Uint32 subKey               = conf->subscriptionKey;
  Uint32 senderNodeId         = conf->senderNodeId;
  Uint32 subData              = conf->senderData;
  SubCoordinatorPtr subPtr;
  c_subCoordinatorPool.getPtr(subPtr, subData);

  ndbrequire(subPtr.p->m_outstandingRequest == GSN_GREP_REMOVE_REQ);
  
  subPtr.p->m_outstandingParticipants.clearWaitingFor(senderNodeId);

  if(!subPtr.p->m_outstandingParticipants.done()) { 
    jam();
    return;
  }
  jam();
  
  /*************************
   * All participants ready 
   *************************/

  m_grep->sendEventRep(signal,
		       EventReport::GrepSubscriptionInfo,
		       GrepEvent::GrepPS_SubRemoveConf,
		       subId, subKey,
		       GrepError::GE_NO_ERROR);

  GrepSubRemoveConf * grepConf = (GrepSubRemoveConf *) conf;
  grepConf->subscriptionId = subId;
  grepConf->subscriptionKey = subKey;
  sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_SUB_REMOVE_CONF, signal, 
	     GrepSubRemoveConf::SignalLength, JBB);
  
  c_subCoordinatorPool.release(subPtr);
}



void 
Grep::PSCoord::execGREP_REMOVE_REF(Signal* signal) 
{
  jamEntry();
  GrepRemoveRef * const ref = (GrepRemoveRef *)signal->getDataPtr();
  Uint32 subData = ref->senderData;
  Uint32 err     = ref->err;
  SubCoordinatorPtr subPtr;

  /**
   * Get the operationrecord matching subdata and remove it. Subsequent 
   * execGREP_REMOVE_REF will simply be ignored at this stage.
   */
  for( c_runningSubscriptions.first(c_subPtr); 
       !c_subPtr.isNull(); c_runningSubscriptions.next(c_subPtr)) {
    jam();
    subPtr.i = c_subPtr.curr.i;
    subPtr.p = c_runningSubscriptions.getPtr(subPtr.i);
    if(subData == subPtr.i) 
      {
      sendRefToSS(signal, *subPtr.p, (GrepError::GE_Code)err /*error*/);
      c_runningSubscriptions.release(subPtr);
    return;
    }
  }
  return;
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       LOG RECORDS (COMING IN FROM LOCAL SUMA)
 * ------------------------------------------------------------------------
 * 
 *  After the subscription is started, we get log records from SUMA.
 *  Both table data and meta data log records are received.
 *
 *  TODO:
 *  @todo Changes in meta data is currently not 
 *	  allowed during global replication
 **************************************************************************/

void
Grep::PSPart::execSUB_META_DATA(Signal* signal) 
{
  jamEntry();
  if(m_recoveryMode) {
    jam();
    return;
  }
  /**
   * METASCAN and METALOG
   */
  SubMetaData * data = (SubMetaData *) signal->getDataPtrSend();
  SubscriptionPtr subPtr;  
  c_subscriptions.getPtr(subPtr, data->subscriberData);

  /***************************
   * Forward data to REP node
   ***************************/
  sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_META_DATA, signal, 
	     SubMetaData::SignalLength, JBB);
#ifdef DEBUG_GREP_SUBSCRIPTION
  ndbout_c("Grep::PSPart: Sent SUB_META_DATA to REP "
	   "(TableId: %d, SenderData: %d, GCI: %d)",
	   data->tableId, data->senderData, data->gci);
#endif
}

/**
 * Receive table data from SUMA and dispatches it to REP node.
 */
void
Grep::PSPart::execSUB_TABLE_DATA(Signal* signal) 
{
  jamEntry();
  if(m_recoveryMode) {
    jam();
    return;
  }
  ndbrequire(m_repRef!=0);
  
  if(!assembleFragments(signal)) { jam(); return; }
  
  /**
   * Check if it is SCAN or LOG data that has arrived
   */
  if(signal->getNoOfSections() == 2)
  {
    jam();
    /**
     * DATASCAN - Not marked with GCI, so mark with latest seen GCI 
     */
    if(m_firstScanGCI == 1 && m_lastScanGCI == 0) {
      m_firstScanGCI = m_latestSeenGCI;
      m_lastScanGCI = m_latestSeenGCI;
    }
    SubTableData * data = (SubTableData*)signal->getDataPtrSend();
    Uint32 subData      = data->senderData;
    data->gci           = m_latestSeenGCI;  
    data->logType       = SubTableData::SCAN;
    
    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, subData);
    sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_TABLE_DATA, signal, 
	       SubTableData::SignalLength, JBB);
#ifdef DEBUG_GREP
    ndbout_c("Grep::PSPart: Sent SUB_TABLE_DATA (Scan, GCI: %d)", 
	     data->gci);
#endif
  } 
  else 
  {
    jam();
    /**
     * DATALOG (TRIGGER) - Already marked with GCI
     */
    SubTableData * data = (SubTableData*)signal->getDataPtrSend();
    data->logType       = SubTableData::LOG;
    Uint32 subData      = data->senderData;
    if (data->gci > m_latestSeenGCI) m_latestSeenGCI = data->gci;

    // Reformat to sections and send to replication node.
    LinearSectionPtr ptr[3];
    ptr[0].p  =  signal->theData + 25;
    ptr[0].sz =  data->noOfAttributes;
    ptr[1].p  =  signal->theData + 25 + MAX_ATTRIBUTES_IN_TABLE;
    ptr[1].sz =  data->dataSize;

    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, subData);
    sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_TABLE_DATA,
	       signal, SubTableData::SignalLength, JBB, ptr, 2);       
#ifdef DEBUG_GREP
    ndbout_c("Grep::PSPart: Sent SUB_TABLE_DATA (Log, GCI: %d)", 
	     data->gci);
#endif
  }
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    START SYNCHRONIZATION
 * ------------------------------------------------------------------------
 * 
 *  
 **************************************************************************/

/**
 * Request to start sync (from Rep SS)
 */
void
Grep::PSCoord::execGREP_SUB_SYNC_REQ(Signal* signal) 
{
  jamEntry();
  GrepSubSyncReq * const subReq = (GrepSubSyncReq*)signal->getDataPtr();
  SubscriptionData::Part part   = (SubscriptionData::Part) subReq->part;
  Uint32 subId                  = subReq->subscriptionId;
  Uint32 subKey                 = subReq->subscriptionKey;
  BlockReference rep            = signal->getSendersBlockRef(); 

  SubCoordinatorPtr subPtr;
  if( !c_subCoordinatorPool.seize(subPtr)) {
    jam();
    SubCoordinator sub;
    sub.m_subscriberRef = rep;
    sub.m_subscriptionId = 0;
    sub.m_subscriptionKey = 0;
    sub.m_outstandingRequest = GSN_GREP_SYNC_REQ;
    sendRefToSS(signal, sub, GrepError::NOSPACE_IN_POOL);
    return;
  }

  prepareOperationRec(subPtr,
		      numberToRef(PSREPBLOCKNO, refToNode(rep)), 
		      subId, subKey,
		      GSN_GREP_SYNC_REQ);

  GrepSyncReq * req = (GrepSyncReq *)subReq;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->senderData       = subPtr.p->m_subscriberData;
  req->part             = (Uint32)part;
  
  /***************************
   * Send to all participants
   ***************************/
  NodeReceiverGroup rg(GREP,  m_grep->m_aliveNodes);
  subPtr.p->m_outstandingParticipants = rg;
  sendSignal(rg,
	     GSN_GREP_SYNC_REQ, signal, GrepSyncReq::SignalLength, JBB);
}


/**
 *  Sync req from Grep::PSCoord to PS particpant 
 */
void 
Grep::PSPart::execGREP_SYNC_REQ(Signal* signal) 
{
  jamEntry();
  
  GrepSyncReq * const grepReq = (GrepSyncReq *) signal->getDataPtr();    
  Uint32 part                 = grepReq->part;
  Uint32 subId                = grepReq->subscriptionId;
  Uint32 subKey               = grepReq->subscriptionKey;
  Uint32 subData              = grepReq->senderData;

  Subscription key;
  key.m_subscriptionId        = subId;
  key.m_subscriptionKey       = subKey;
  SubscriptionPtr subPtr;
  ndbrequire(c_subscriptions.find(subPtr, key));
  subPtr.p->m_operationPtrI   = subData;
  subPtr.p->m_outstandingRequest = GSN_GREP_SYNC_REQ; 
  /**********************************
   * Send SUB_SYNC_REQ to local SUMA
   **********************************/
  SubSyncReq * sumaReq      = (SubSyncReq *)grepReq;    
  sumaReq->subscriptionId   = subId; 
  sumaReq->subscriptionKey  = subKey;
  sumaReq->subscriberData   = subPtr.i;
  sumaReq->part             = part;
  sendSignal(SUMA_REF, GSN_SUB_SYNC_REQ, signal, 
	     SubSyncReq::SignalLength, JBB);
}


/**
 * SYNC conf from SUMA
 */
void 
Grep::PSPart::execSUB_SYNC_CONF(Signal* signal) 
{
  jamEntry();
  
  SubSyncConf * const conf = (SubSyncConf *) signal->getDataPtr();
  Uint32 part              = conf->part;
  Uint32 subId             = conf->subscriptionId;
  Uint32 subKey            = conf->subscriptionKey;
  Uint32 subData           = conf->subscriberData;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);

  ndbrequire(subPtr.p->m_subscriptionId  == subId);
  ndbrequire(subPtr.p->m_subscriptionKey == subKey);
  
  GrepSyncConf * grepConf     = (GrepSyncConf *)conf;
  grepConf->senderNodeId      = getOwnNodeId();
  grepConf->part              = part;
  grepConf->firstGCI          = m_firstScanGCI;
  grepConf->lastGCI           = m_lastScanGCI;
  grepConf->subscriptionId    = subId;
  grepConf->subscriptionKey   = subKey;
  grepConf->senderData        = subPtr.p->m_operationPtrI;
  sendSignal(subPtr.p->m_coordinatorRef, GSN_GREP_SYNC_CONF, signal, 
	     GrepSyncConf::SignalLength, JBB);

  m_firstScanGCI = 1;
  m_lastScanGCI = 0;
  subPtr.p->m_outstandingRequest = 0;
}

/**
 * Handle errors that either occured in:
 * 1) PSPart
 * or
 * 2) propagated from local SUMA
 *  
 * Propagates REF signal to PSCoord
 */
void 
Grep::PSPart::execSUB_SYNC_REF(Signal* signal) {
  jamEntry();
  SubSyncRef * const ref = (SubSyncRef *)signal->getDataPtr();
  Uint32 subData              = ref->subscriberData;
  GrepError::GE_Code err     = (GrepError::GE_Code)ref->err;
  SubscriptionData::Part part = (SubscriptionData::Part)ref->part;
  
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subData);
  sendRefToPSCoord(signal, *subPtr.p, err /*error*/ ,part);
  subPtr.p->m_outstandingRequest = 0;
}

/**
 *  Syncing has started... (says PS Participant)
 */
void 
Grep::PSCoord::execGREP_SYNC_CONF(Signal* signal) 
{
  jamEntry();

  GrepSyncConf const * conf = (GrepSyncConf *)signal->getDataPtr();
  Uint32 part               = conf->part;
  Uint32 firstGCI           = conf->firstGCI;
  Uint32 lastGCI            = conf->lastGCI;
  Uint32 subId              = conf->subscriptionId;
  Uint32 subKey             = conf->subscriptionKey;
  Uint32 subData            = conf->senderData;
  
  SubCoordinatorPtr subPtr;
  c_subCoordinatorPool.getPtr(subPtr, subData);
  ndbrequire(subPtr.p->m_outstandingRequest == GSN_GREP_SYNC_REQ);
  
  subPtr.p->m_outstandingParticipants.clearWaitingFor(conf->senderNodeId);
  if(!subPtr.p->m_outstandingParticipants.done()) return;

  /**
   * Send event
   */
  GrepEvent::Subscription event;
  if(part == SubscriptionData::MetaData) 
    event = GrepEvent::GrepPS_SubSyncMetaConf;
  else
    event = GrepEvent::GrepPS_SubSyncDataConf;
  
  /* @todo Johan: Add firstGCI here. /Lars */
  m_grep->sendEventRep(signal, EventReport::GrepSubscriptionInfo,
		       event, subId, subKey,
		       (Uint32)GrepError::GE_NO_ERROR,
		       lastGCI);

  /*************************
   * All participants ready 
   *************************/
  GrepSubSyncConf * grepConf = (GrepSubSyncConf *)conf;
  grepConf->part             = part;
  grepConf->firstGCI         = firstGCI;
  grepConf->lastGCI          = lastGCI;
  grepConf->subscriptionId   = subId;
  grepConf->subscriptionKey  = subKey;

  sendSignal(subPtr.p->m_subscriberRef, GSN_GREP_SUB_SYNC_CONF, signal, 
	     GrepSubSyncConf::SignalLength, JBB);
  c_subCoordinatorPool.release(subPtr);
}

/**
 * Handle errors that either occured in:
 * 1) PSCoord
 * or
 * 2) propagated from PSPart 
 */
void 
Grep::PSCoord::execGREP_SYNC_REF(Signal* signal) {
  jamEntry();
  GrepSyncRef * const ref = (GrepSyncRef *)signal->getDataPtr();
  Uint32 subData              = ref->senderData;
  SubscriptionData::Part part = (SubscriptionData::Part)ref->part;
  GrepError::GE_Code err         = (GrepError::GE_Code)ref->err;
  SubCoordinatorPtr subPtr;
  c_runningSubscriptions.getPtr(subPtr, subData);  
  sendRefToSS(signal, *subPtr.p, err /*error*/, part);
}



void
Grep::PSCoord::sendRefToSS(Signal * signal, 
			   SubCoordinator sub,
			   GrepError::GE_Code err,
			   SubscriptionData::Part part) {
  /**
  
    GrepCreateRef * ref = (GrepCreateRef*)signal->getDataPtrSend();
    ref->senderData = sub.m_subscriberData;
    ref->subscriptionId = sub.m_subscriptionId;
    ref->subscriptionKey = sub.m_subscriptionKey;
    ref->err = err;
    sendSignal(sub.m_coordinatorRef, GSN_GREP_CREATE_REF, signal, 
	       GrepCreateRef::SignalLength, JBB);    
*/

  jam();
  GrepEvent::Subscription event;
  switch(sub.m_outstandingRequest) {
  case GSN_GREP_CREATE_SUBID_REQ: 
    {
      jam();
      CreateSubscriptionIdRef * ref = 
	(CreateSubscriptionIdRef*)signal->getDataPtrSend();
      ref->err             = (Uint32)err;
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      sendSignal(sub.m_subscriberRef, 
		 GSN_GREP_CREATE_SUBID_REF,
		 signal,
		 CreateSubscriptionIdRef::SignalLength,
		 JBB);
      event = GrepEvent::GrepPS_CreateSubIdRef;
    }
    break;
  case GSN_GREP_CREATE_REQ: 
    {
      jam();
      GrepSubCreateRef * ref = (GrepSubCreateRef*)signal->getDataPtrSend();
      ref->err = (Uint32)err;
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      sendSignal(sub.m_subscriberRef, GSN_GREP_SUB_CREATE_REF, signal,
		 GrepSubCreateRef::SignalLength, JBB);
      event = GrepEvent::GrepPS_SubCreateRef;
    }
    break;
  case GSN_GREP_SYNC_REQ: 
    {
      jam();
      GrepSubSyncRef * ref = (GrepSubSyncRef*)signal->getDataPtrSend(); 
      ref->err = (Uint32)err;
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->part            = (SubscriptionData::Part) part;
      sendSignal(sub.m_subscriberRef, 
		 GSN_GREP_SUB_SYNC_REF,
		 signal,
		 GrepSubSyncRef::SignalLength,
		 JBB);
      if(part == SubscriptionData::MetaData) 
	event = GrepEvent::GrepPS_SubSyncMetaRef;
      else
	event = GrepEvent::GrepPS_SubSyncDataRef;
    }
    break;
  case GSN_GREP_START_REQ:  
    {
      jam();
      GrepSubStartRef * ref = (GrepSubStartRef*)signal->getDataPtrSend();
      ref->err = (Uint32)err;
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      
      sendSignal(sub.m_subscriberRef, GSN_GREP_SUB_START_REF,
		 signal, GrepSubStartRef::SignalLength, JBB);
      if(part == SubscriptionData::MetaData) 
	event = GrepEvent::GrepPS_SubStartMetaRef;
      else
	event = GrepEvent::GrepPS_SubStartDataRef;  
      /**
       * Send event report
       */
      m_grep->sendEventRep(signal,
			   EventReport::GrepSubscriptionAlert,
			   event,
			   sub.m_subscriptionId,
			   sub.m_subscriptionKey,
			   (Uint32)err);
    }
    break;
  case GSN_GREP_REMOVE_REQ:
    {
      jam();
      GrepSubRemoveRef * ref = (GrepSubRemoveRef*)signal->getDataPtrSend();
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->err             = (Uint32)err;
      
      sendSignal(sub.m_subscriberRef, 
		 GSN_GREP_SUB_REMOVE_REF,
		 signal,
		 GrepSubRemoveRef::SignalLength,
		 JBB);
      
      event = GrepEvent::GrepPS_SubRemoveRef;   
    }
    break;
  default:
    ndbrequire(false);
    event= GrepEvent::Rep_Disconnect; // remove compiler warning
  }  
  /**
   * Finally, send an event.
   */
  m_grep->sendEventRep(signal,
		       EventReport::GrepSubscriptionAlert,
		       event,
		       sub.m_subscriptionId,
		       sub.m_subscriptionKey,
		       err);
 
}


void
Grep::PSPart::sendRefToPSCoord(Signal * signal, 
			       Subscription sub,
			       GrepError::GE_Code err,
			       SubscriptionData::Part part) {

  jam();
  GrepEvent::Subscription event;
  switch(sub.m_outstandingRequest) {
    
  case GSN_GREP_CREATE_REQ: 
    {
      GrepCreateRef * ref = (GrepCreateRef*)signal->getDataPtrSend();
      ref->senderData = sub.m_subscriberData;
      ref->subscriptionId = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->err = err;
      sendSignal(sub.m_coordinatorRef, GSN_GREP_CREATE_REF, signal, 
		 GrepCreateRef::SignalLength, JBB);    
      
      event =  GrepEvent::GrepPS_SubCreateRef;
    }
    break;
  case GSN_GREP_SYNC_REQ: 
    {
      GrepSyncRef * ref = (GrepSyncRef*)signal->getDataPtrSend();
      ref->senderData = sub.m_subscriberData;
      ref->subscriptionId = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->part = part;
      ref->err = err;
      sendSignal(sub.m_coordinatorRef, 
		 GSN_GREP_SYNC_REF, signal, 
		 GrepSyncRef::SignalLength, JBB);    
      if(part == SubscriptionData::MetaData) 
	event = GrepEvent::GrepPS_SubSyncMetaRef;
      else
	event = GrepEvent::GrepPS_SubSyncDataRef;    
    }
    break;
  case GSN_GREP_START_REQ:  
    {
      jam();
      GrepStartRef * ref = (GrepStartRef*)signal->getDataPtrSend();
      ref->senderData = sub.m_subscriberData;
      ref->subscriptionId = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->part = (Uint32) part;
      ref->err = err;
      sendSignal(sub.m_coordinatorRef, GSN_GREP_START_REF, signal, 
		 GrepStartRef::SignalLength, JBB);			    
      if(part == SubscriptionData::MetaData) 
	event = GrepEvent::GrepPS_SubStartMetaRef;
      else 
	event = GrepEvent::GrepPS_SubStartDataRef;    
    }
    break;

  case GSN_GREP_REMOVE_REQ:
    {
      jamEntry();
      GrepRemoveRef * ref  = (GrepRemoveRef*)signal->getDataPtrSend();
      ref->senderData      = sub.m_operationPtrI;
      ref->subscriptionId  = sub.m_subscriptionId;
      ref->subscriptionKey = sub.m_subscriptionKey;
      ref->err             = err;
      sendSignal(sub.m_coordinatorRef, GSN_GREP_REMOVE_REF, signal, 
		 GrepCreateRef::SignalLength, JBB);    
      
    }
    break;
  default:
    ndbrequire(false);
    event= GrepEvent::Rep_Disconnect; // remove compiler warning
  }
  
  /**
   * Finally, send an event.
   */
  m_grep->sendEventRep(signal,
		       EventReport::GrepSubscriptionAlert,
		       event,
		       sub.m_subscriptionId,
		       sub.m_subscriptionKey,
		       err);
 
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:    GREP PS Coordinator GCP 
 * ------------------------------------------------------------------------
 * 
 *  
 **************************************************************************/

void 
Grep::PSPart::execSUB_GCP_COMPLETE_REP(Signal* signal) 
{
  jamEntry();
  if(m_recoveryMode) {
    jam();
    return;
  }
  SubGcpCompleteRep * rep  = (SubGcpCompleteRep *)signal->getDataPtrSend();
  rep->senderRef           = reference();

  if (rep->gci > m_latestSeenGCI) m_latestSeenGCI = rep->gci;
  SubscriptionPtr subPtr;
  c_subscriptions.first(c_subPtr);
  for(; !c_subPtr.isNull(); c_subscriptions.next(c_subPtr)) {
    
    subPtr.i = c_subPtr.curr.i;
    subPtr.p = c_subscriptions.getPtr(subPtr.i);  
    sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_GCP_COMPLETE_REP, signal, 
	       SubGcpCompleteRep::SignalLength, JBB);
  }

#ifdef DEBUG_GREP
  ndbout_c("Grep::PSPart: Recd SUB_GCP_COMPLETE_REP "
	   "(GCI: %d, nodeId: %d) from SUMA", 
	   rep->gci, refToNode(rep->senderRef));
#endif
}


void
Grep::PSPart::execSUB_SYNC_CONTINUE_REQ(Signal* signal) 
{
  jamEntry();
  SubSyncContinueReq * const req = (SubSyncContinueReq*)signal->getDataPtr();
  Uint32 subData                 = req->subscriberData;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr,subData);
  
  /**
   * @todo Figure out how to control how much data we can receive?
   */
  SubSyncContinueConf * conf = (SubSyncContinueConf*)req;
  conf->subscriptionId       = subPtr.p->m_subscriptionId;
  conf->subscriptionKey      = subPtr.p->m_subscriptionKey;
  sendSignal(SUMA_REF, GSN_SUB_SYNC_CONTINUE_CONF, signal, 
	     SubSyncContinueConf::SignalLength, JBB);  
}

void
Grep::sendEventRep(Signal * signal,
		   EventReport::EventType type, 
		   GrepEvent::Subscription event,
		   Uint32 subId,
		   Uint32 subKey,
		   Uint32 err,
		   Uint32 other) {
  jam();
  signal->theData[0] = type;
  signal->theData[1] = event;
  signal->theData[2] = subId;
  signal->theData[3] = subKey; 
  signal->theData[4] = err;
  
  if(other==0)
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5 ,JBB);        
  else {
    signal->theData[5] = other;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 6 ,JBB);        
  }
}
