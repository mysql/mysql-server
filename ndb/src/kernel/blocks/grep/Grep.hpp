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

#ifndef GREP_HPP
#define GREP_HPP

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include <NodeBitmask.hpp>
#include <SignalCounter.hpp>
#include <SLList.hpp>

#include <DLList.hpp>

#include <GrepError.hpp>
#include <GrepEvent.hpp>

#include <signaldata/EventReport.hpp>
#include <signaldata/SumaImpl.hpp>


/**
 * Module in block   (Should be placed elsewhere)
 */
class BlockComponent {
public:
  BlockComponent(SimulatedBlock *);
  BlockReference reference()  { return m_sb->reference(); };
  BlockNumber number()        { return m_sb->number(); };
  
  void sendSignal(NodeReceiverGroup rg,
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf ) const {
    m_sb->sendSignal(rg, gsn, signal, length, jbuf); 
  }

  void sendSignal(BlockReference ref, 
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf ) const {
    m_sb->sendSignal(ref, gsn, signal, length, jbuf); 
  }

  void sendSignal(BlockReference ref, 
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf,
		  LinearSectionPtr ptr[3],
		  Uint32 noOfSections) const {
    m_sb->sendSignal(ref, gsn, signal, length, jbuf, ptr, noOfSections);
  }

  void sendSignalWithDelay(BlockReference ref, 
			   GlobalSignalNumber gsn, 
                           Signal* signal,
                           Uint32 delayInMilliSeconds, 
			   Uint32 length) const {

    m_sb->sendSignalWithDelay(ref, gsn, signal, delayInMilliSeconds, length); 
  }

  NodeId getOwnNodeId() const {
    return m_sb->getOwnNodeId();
  }

  bool assembleFragments(Signal * signal) {
    return m_sb->assembleFragments(signal);
  }

  void progError(int line, int err_code, const char* extra) {
    m_sb->progError(line, err_code, extra); 
  }
  
private:
  SimulatedBlock * m_sb;
};



/**
 * Participant of GREP Protocols (not necessarily a protocol coordinator)
 *
 * This object is only used on primary system
 */
#if 0
class GrepParticipant : public SimulatedBlock
{
protected:
  GrepParticipant(const Configuration & conf);
  virtual ~GrepParticipant();
  BLOCK_DEFINES(GrepParticipant);
  
protected:
  /***************************************************************************
   * SUMA Signal Interface
   ***************************************************************************/
  void execSUB_CREATE_CONF(Signal*);
  void execSUB_STARTCONF(Signal*);
  void execSUB_REMOVE_CONF(Signal*);

  void execSUB_META_DATA(Signal*);
  void execSUB_TABLE_DATA(Signal*);
  
  void execSUB_SYNC_CONF(Signal*);

  void execSUB_GCP_COMPLETE_REP(Signal*);
  void execSUB_SYNC_CONTINUE_REQ(Signal*);

  /***************************************************************************
   * GREP Coordinator Signal Interface
   ***************************************************************************/
  void execGREP_CREATE_REQ(Signal*);
  void execGREP_START_REQ(Signal*);
  void execGREP_SYNC_REQ(Signal*);
  void execGREP_REMOVE_REQ(Signal*);


protected:
  BlockReference  m_repRef;  ///< Replication node (only one rep node per grep)

private:
  BlockReference  m_coordinator;  
  Uint32          m_latestSeenGCI;
};
#endif


/**
 * GREP Coordinator
 */
class Grep : public SimulatedBlock //GrepParticipant
{
  BLOCK_DEFINES(Grep);

public:
  Grep(const Configuration & conf);
  virtual ~Grep();
  
private:
  /***************************************************************************
   * General Signal Recivers
   ***************************************************************************/
  void execSTTOR(Signal*);
  void sendSTTORRY(Signal*);
  void execNDB_STTOR(Signal*);
  void execDUMP_STATE_ORD(Signal*);
  void execREAD_NODESCONF(Signal*);
  void execNODE_FAILREP(Signal*);
  void execINCL_NODEREQ(Signal*);
  void execGREP_REQ(Signal*);
  void execAPI_FAILREQ(Signal*);
  /**
   * Forwarded to PSCoord
   */
  //CONF
  void fwdGREP_CREATE_CONF(Signal* s) { 
    pscoord.execGREP_CREATE_CONF(s); };
  void fwdGREP_START_CONF(Signal* s) { 
    pscoord.execGREP_START_CONF(s); };
  void fwdGREP_SYNC_CONF(Signal* s) { 
    pscoord.execGREP_SYNC_CONF(s); };
  void fwdGREP_REMOVE_CONF(Signal* s) { 
    pscoord.execGREP_REMOVE_CONF(s); };
  void fwdCREATE_SUBID_CONF(Signal* s) { 
    pscoord.execCREATE_SUBID_CONF(s); };

  //REF

  void fwdGREP_CREATE_REF(Signal* s) { 
    pscoord.execGREP_CREATE_REF(s); };
  void fwdGREP_START_REF(Signal* s) { 
    pscoord.execGREP_START_REF(s); };
  void fwdGREP_SYNC_REF(Signal* s) { 
    pscoord.execGREP_SYNC_REF(s); };
  
  void fwdGREP_REMOVE_REF(Signal* s) { 
    pscoord.execGREP_REMOVE_REF(s); };

  void fwdCREATE_SUBID_REF(Signal* s) { 
    pscoord.execCREATE_SUBID_REF(s); };

  //REQ
  void fwdGREP_SUB_CREATE_REQ(Signal* s) { 
    pscoord.execGREP_SUB_CREATE_REQ(s); };
  void fwdGREP_SUB_START_REQ(Signal* s) { 
    pscoord.execGREP_SUB_START_REQ(s); };
  void fwdGREP_SUB_SYNC_REQ(Signal* s) {
    pscoord.execGREP_SUB_SYNC_REQ(s); };
  void fwdGREP_SUB_REMOVE_REQ(Signal* s) {
    pscoord.execGREP_SUB_REMOVE_REQ(s); };
  void fwdGREP_CREATE_SUBID_REQ(Signal* s) { 
    pscoord.execGREP_CREATE_SUBID_REQ(s); };

  /**
   * Forwarded to PSPart
   */

  void fwdSTART_ME(Signal* s){
    pspart.execSTART_ME(s);
  };
  void fwdGREP_ADD_SUB_REQ(Signal* s){
    pspart.execGREP_ADD_SUB_REQ(s);
  };
  void fwdGREP_ADD_SUB_REF(Signal* s){
    pspart.execGREP_ADD_SUB_REF(s);
  };
  void fwdGREP_ADD_SUB_CONF(Signal* s){
    pspart.execGREP_ADD_SUB_CONF(s);
  };

  //CONF
  void fwdSUB_CREATE_CONF(Signal* s) {
    pspart.execSUB_CREATE_CONF(s); };
  void fwdSUB_START_CONF(Signal* s) {
    pspart.execSUB_START_CONF(s); };
  void fwdSUB_REMOVE_CONF(Signal* s) {
    pspart.execSUB_REMOVE_CONF(s); };
  void fwdSUB_SYNC_CONF(Signal* s) {
    pspart.execSUB_SYNC_CONF(s); };

  //REF

  void fwdSUB_CREATE_REF(Signal* s) {
    pspart.execSUB_CREATE_REF(s); };
  void fwdSUB_START_REF(Signal* s) {
    pspart.execSUB_START_REF(s); };
  void fwdSUB_REMOVE_REF(Signal* s) {
    pspart.execSUB_REMOVE_REF(s); };
  void fwdSUB_SYNC_REF(Signal* s) {
    pspart.execSUB_SYNC_REF(s); };

  //REQ
  void fwdSUB_SYNC_CONTINUE_REQ(Signal* s) {
    pspart.execSUB_SYNC_CONTINUE_REQ(s); };
  void fwdGREP_CREATE_REQ(Signal* s) {
    pspart.execGREP_CREATE_REQ(s); };
  void fwdGREP_START_REQ(Signal* s) {
    pspart.execGREP_START_REQ(s); };
  void fwdGREP_SYNC_REQ(Signal* s) {
    pspart.execGREP_SYNC_REQ(s); };
  void fwdGREP_REMOVE_REQ(Signal* s) {
    pspart.execGREP_REMOVE_REQ(s); };

  void fwdSUB_META_DATA(Signal* s) {
    pspart.execSUB_META_DATA(s); };
  void fwdSUB_TABLE_DATA(Signal* s) {
    pspart.execSUB_TABLE_DATA(s); };

  void fwdSUB_GCP_COMPLETE_REP(Signal* s) {
    pspart.execSUB_GCP_COMPLETE_REP(s); };  

  void sendEventRep(Signal * signal,
		    EventReport::EventType type, 
		    GrepEvent::Subscription event,
		    Uint32 subId,
		    Uint32 subKey,
		    Uint32 err,
		    Uint32 gci=0);
   
  void getNodeGroupMembers(Signal* signal);
  

  /***************************************************************************
   * Block Data
   ***************************************************************************/
  struct Node {
    Uint32 nodeId;
    Uint32 alive;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  typedef Ptr<Node> NodePtr;
  
  NodeId            m_masterNodeId;
  SLList<Node>      m_nodes;
  NdbNodeBitmask    m_aliveNodes;
  ArrayPool<Node>   m_nodePool;  

  /**
   * for all Suma's to keep track of other Suma's in Node group
   */
  Uint32 c_nodeGroup;
  Uint32 c_noNodesInGroup;
  Uint32 c_idInNodeGroup;
  NodeId c_nodesInGroup[4];
  

public:
  /*************************************************************************** 
   * GREP PS Coordinator
   ***************************************************************************/
  class PSCoord : public BlockComponent { 

  private:

    struct SubCoordinator {
      Uint32 m_subscriberRef;
      Uint32 m_subscriberData;
      Uint32 m_coordinatorRef;
      Uint32 m_subscriptionId;
      Uint32 m_subscriptionKey;
      Uint32 m_subscriptionType;
      NdbNodeBitmask m_participants;
      Uint32 m_outstandingRequest;      
      SignalCounter m_outstandingParticipants;
      
      Uint32 nextHash;
      union { Uint32 prevHash; Uint32 nextPool; };
      
      Uint32 hashValue() const {
      return m_subscriptionId + m_subscriptionKey;
      }
      
      bool equal(const SubCoordinator & s) const {
	return 
	  m_subscriptionId == s.m_subscriptionId && 
	  m_subscriptionKey == s.m_subscriptionKey;
      }

    };

    typedef Ptr<SubCoordinator> SubCoordinatorPtr;
    ArrayPool<SubCoordinator> c_subCoordinatorPool;
    DLHashTable<SubCoordinator>::Iterator c_subPtr;
    DLHashTable<SubCoordinator> c_runningSubscriptions;    

    void prepareOperationRec(SubCoordinatorPtr ptr, 
			     BlockReference subscriber,
			     Uint32 subId,
			     Uint32 subKey,
			     Uint32 request);

  public:
    PSCoord(class Grep *);
    
    void execGREP_CREATE_CONF(Signal*);
    void execGREP_START_CONF(Signal*);
    void execGREP_SYNC_CONF(Signal*);
    void execGREP_REMOVE_CONF(Signal*);

    void execGREP_CREATE_REF(Signal*);
    void execGREP_START_REF(Signal*);
    void execGREP_SYNC_REF(Signal*);
    void execGREP_REMOVE_REF(Signal*);
    
    
    void execCREATE_SUBID_CONF(Signal*);  //comes from SUMA
    void execGREP_CREATE_SUBID_REQ(Signal*);  

    void execGREP_SUB_CREATE_REQ(Signal*);  
    void execGREP_SUB_START_REQ(Signal*);
    void execGREP_SUB_SYNC_REQ(Signal*);
    void execGREP_SUB_REMOVE_REQ(Signal*);
    


    void execCREATE_SUBID_REF(Signal*);  



    void sendCreateSubIdRef_SS(Signal * signal, 
			       Uint32 subId,
			       Uint32 subKey,
			       BlockReference to,
			       GrepError::Code err);


    void sendSubRemoveRef_SS(Signal * signal, 
			  SubCoordinator sub,
			  GrepError::Code err);

    void sendRefToSS(Signal * signal, 
		     SubCoordinator sub,
		     GrepError::Code err,
		     SubscriptionData::Part part = (SubscriptionData::Part)0);
        
    void setRepRef(BlockReference rr) { m_repRef = rr; };
    //void setAliveNodes(NdbNodeBitmask an) { m_aliveNodes = an; };

    BlockReference  m_repRef;  ///< Rep node (only one rep node per grep)
    //    NdbNodeBitmask  m_aliveNodes;

    Uint32          m_outstandingRequest;
    SignalCounter   m_outstandingParticipants;

    Grep *          m_grep;
  } pscoord;
  friend class PSCoord; 

  /*************************************************************************** 
   * GREP PS Participant
   ***************************************************************************
   * Participant of GREP Protocols (not necessarily a protocol coordinator)
   *
   * This object is only used on primary system
   ***************************************************************************/
  class PSPart: public BlockComponent
  {
    //protected:
    //GrepParticipant(const Configuration & conf);
    //virtual ~GrepParticipant();
    //BLOCK_DEFINES(GrepParticipant);

    struct Subscription {
      Uint32 m_subscriberRef;
      Uint32 m_subscriberData;    
      Uint32 m_subscriptionId;
      Uint32 m_subscriptionKey;    
      Uint32 m_subscriptionType;
      Uint32 m_coordinatorRef;
      Uint32 m_outstandingRequest;      
      Uint32 m_operationPtrI;
      Uint32 nextHash;
      union { Uint32 prevHash; Uint32 nextPool; };
      
      Uint32 hashValue() const {
	return m_subscriptionId + m_subscriptionKey;
      }
      
      bool equal(const Subscription & s) const {
	return 
	  m_subscriptionId == s.m_subscriptionId && 
	  m_subscriptionKey == s.m_subscriptionKey;
      }
      
    };
    typedef Ptr<Subscription> SubscriptionPtr;

    DLHashTable<Subscription> c_subscriptions;
    DLHashTable<Subscription>::Iterator c_subPtr;
    ArrayPool<Subscription> c_subscriptionPool;

  public:
    PSPart(class Grep *);


    //protected:
    /*************************************************************************
     * SUMA Signal Interface
     *************************************************************************/
    void execSUB_CREATE_CONF(Signal*);
    void execSUB_START_CONF(Signal*);
    void execSUB_SYNC_CONF(Signal*);
    void execSUB_REMOVE_CONF(Signal*);

    void execSUB_CREATE_REF(Signal*);
    void execSUB_START_REF(Signal*);
    void execSUB_SYNC_REF(Signal*);
    void execSUB_REMOVE_REF(Signal*);

    
    void execSUB_META_DATA(Signal*);
    void execSUB_TABLE_DATA(Signal*);
    
    
    void execSUB_GCP_COMPLETE_REP(Signal*);
    void execSUB_SYNC_CONTINUE_REQ(Signal*);
    
    /*************************************************************************
     * GREP Coordinator Signal Interface
     *************************************************************************/
    void execGREP_CREATE_REQ(Signal*);
    void execGREP_START_REQ(Signal*);
    void execGREP_SYNC_REQ(Signal*);
    void execGREP_REMOVE_REQ(Signal*);

    /**
     * NR/NF signals
     */
    void execSTART_ME(Signal *);    
    void execGREP_ADD_SUB_REQ(Signal *);
    void execGREP_ADD_SUB_REF(Signal *);
    void execGREP_ADD_SUB_CONF(Signal *);
    
    /*************************************************************************
     * GREP Coordinator error handling interface
     *************************************************************************/

    void sendRefToPSCoord(Signal * signal, 
			  Subscription sub,
			  GrepError::Code err,
			  SubscriptionData::Part part = (SubscriptionData::Part)0);

    //protected:
    BlockReference  m_repRef;  ///< Replication node 
                               ///< (only one rep node per grep)
    bool            m_recoveryMode;
    
  private:
    BlockReference  m_coordinator;  
    Uint32          m_firstScanGCI;
    Uint32          m_lastScanGCI;
    Uint32          m_latestSeenGCI;
    Grep *          m_grep;
  } pspart;
  friend class PSPart; 

  /***************************************************************************
   * AddRecSignal Stuff (should maybe be gerneralized)
   ***************************************************************************/
  typedef void (Grep::* ExecSignalLocal1) (Signal* signal); 
  typedef void (Grep::PSCoord::* ExecSignalLocal2) (Signal* signal); 
  typedef void (Grep::PSPart::* ExecSignalLocal4) (Signal* signal); 
};


/*************************************************************************
 * Requestor
 * 
 * The following methods are callbacks (registered functions) 
 * for the Requestor.  The Requestor calls these when it needs 
 * something to be done.
 *************************************************************************/
void startSubscription(void * cbObj, Signal*, int type);  
void scanSubscription(void * cbObj, Signal*, int type);

#endif
