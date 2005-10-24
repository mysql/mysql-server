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

#ifndef QMGR_H
#define QMGR_H


#include <pc.hpp>
#include <NdbTick.h>
#include <SimulatedBlock.hpp>
#include <NodeBitmask.hpp>
#include <SignalCounter.hpp>

#include <signaldata/EventReport.hpp>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/CmRegSignalData.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/FailRep.hpp>

#include "timer.hpp"

#ifdef QMGR_C

#define NO_REG_APP 1

/* Delay values, ms -----------------------------*/
#define ZDELAY_REGREQ 1000

/* Type of refuse in CM_NODEINFOREF -------------*/
#define ZNOT_RUNNING 0

/* Type of continue in CONTINUEB ----------------*/
#define ZREGREQ_TIMELIMIT 0
#define ZHB_HANDLING 1
#define ZREGREQ_MASTER_TIMELIMIT 2
#define ZAPI_HB_HANDLING 3
#define ZTIMER_HANDLING 4
#define ZARBIT_HANDLING 5

/* Error Codes ------------------------------*/
#define ZERRTOOMANY 1101
#define ZERRALREADYREG 1102
#define ZERRNHMISSING 1103
#define ZERRNLMISSING 1104
#define ZERRAPPMISSING 1105
#define ZERROR_NOT_IN_CFGFILE 1106
#define ZERROR_TIMEOUT 1107
#define ZERROR_NOT_ZINIT 1108
#define ZERROR_NODEINFOREF 1109
#define ZERROR_NOTLOCALQMGR 1110
#define ZERROR_NOTRUNNING 1111
#define ZCOULD_NOT_OCCUR_ERROR 1112
#define ZTIME_OUT_ERROR 1113
#define ZERROR_NOT_DEAD 1114
#define ZDECLARED_FAIL_ERROR 1115
#define ZOWN_NODE_ERROR 1116
#define ZWRONG_STATE_ERROR 1117
#define ZNODE_ZERO_ERROR 1118
#define ZWRONG_NODE_ERROR 1119

#endif


class Qmgr : public SimulatedBlock {
public:
  // State values
  enum QmgrState {
    Q_NOT_ACTIVE = 0,
    Q_ACTIVE = 1
  };
  
  enum FailState {
    NORMAL = 0,
    WAITING_FOR_FAILCONF1 = 1,
    WAITING_FOR_FAILCONF2 = 2,
    WAITING_FOR_NDB_FAILCONF = 3
  };

  enum Phase {
    ZINIT = 1, 		        /* All nodes start in phase INIT         */
    ZSTARTING = 2, 		/* Node is connecting to cluster         */
    ZRUNNING = 3, 		/* Node is running in the cluster        */
    ZPREPARE_FAIL = 4,       /* PREPARATION FOR FAILURE               */
    ZFAIL_CLOSING = 5,             /* API/NDB IS DISCONNECTING              */
    ZAPI_ACTIVE = 6,            /* API IS RUNNING IN NODE                */
    ZAPI_INACTIVE = 7           /* Inactive API */
  };

  struct StartRecord {
    void reset(){ m_startKey++; m_startNode = 0;}
    Uint32 m_startKey;
    Uint32 m_startNode;
    Uint64 m_startTimeout;
    
    Uint32 m_gsn;
    SignalCounter m_nodes;
  } c_start;

  NdbNodeBitmask c_definedNodes; // DB nodes in config
  NdbNodeBitmask c_clusterNodes; // DB nodes in cluster
  NodeBitmask c_connectedNodes;  // All kinds of connected nodes
  Uint32 c_maxDynamicId;
  
  // Records
  struct NodeRec {
    UintR ndynamicId;
    Phase phase;

    QmgrState sendPrepFailReqStatus;
    QmgrState sendCommitFailReqStatus;
    QmgrState sendPresToStatus;
    FailState failState;
    BlockReference rcv[2];        // remember which failconf we have received
    BlockReference blockRef;

    NodeRec() { }
  }; /* p2c: size = 52 bytes */
  
  typedef Ptr<NodeRec> NodeRecPtr;
  
  enum ArbitState {
    ARBIT_NULL = 0,
    ARBIT_INIT = 1,             // create new ticket
    ARBIT_FIND = 2,		// find candidate arbitrator node
    ARBIT_PREP1 = 3,		// PREP db nodes with null ticket
    ARBIT_PREP2 = 4,		// PREP db nodes with current ticket
    ARBIT_START = 5,		// START arbitrator API thread
    ARBIT_RUN = 6,		// running with arbitrator
    ARBIT_CHOOSE = 7,		// ask arbitrator after network partition
    ARBIT_CRASH = 8		// crash ourselves
  };

  struct ArbitRec {
    ArbitState state;		// state
    bool newstate;		// flag to initialize new state
    unsigned thread;		// identifies a continueB "thread"
    NodeId node;		// current arbitrator candidate
    ArbitTicket ticket;		// ticket
    NodeBitmask apiMask[1+2];	// arbitrators 0=all 1,2=per rank
    NodeBitmask newMask;	// new nodes to process in RUN state
    Uint8 sendCount;		// control send/recv of signals
    Uint8 recvCount;
    NodeBitmask recvMask;	// left to recv
    Uint32 code;		// code field from signal
    Uint32 failureNr;            // cfailureNr at arbitration start
    Uint32 timeout;             // timeout for CHOOSE state
    NDB_TICKS timestamp;	// timestamp for checking timeouts

    inline bool match(ArbitSignalData* sd) {
      return
	node == sd->node &&
	ticket.match(sd->ticket);
    }

    inline void setTimestamp() {
      timestamp = NdbTick_CurrentMillisecond();
    }

    inline NDB_TICKS getTimediff() {
      NDB_TICKS now = NdbTick_CurrentMillisecond();
      return now < timestamp ? 0 : now - timestamp;
    }
  };
  
public:
  Qmgr(const class Configuration &);
  virtual ~Qmgr();

private:
  BLOCK_DEFINES(Qmgr);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execCM_HEARTBEAT(Signal* signal);
  void execCM_ADD(Signal* signal);
  void execCM_ACKADD(Signal* signal);
  void execCM_REGREQ(Signal* signal);
  void execCM_REGCONF(Signal* signal);
  void execCM_REGREF(Signal* signal);
  void execCM_NODEINFOREQ(Signal* signal);
  void execCM_NODEINFOCONF(Signal* signal);
  void execCM_NODEINFOREF(Signal* signal);
  void execPREP_FAILREQ(Signal* signal);
  void execPREP_FAILCONF(Signal* signal);
  void execPREP_FAILREF(Signal* signal);
  void execCOMMIT_FAILREQ(Signal* signal);
  void execCOMMIT_FAILCONF(Signal* signal);
  void execFAIL_REP(Signal* signal);
  void execPRES_TOREQ(Signal* signal);
  void execPRES_TOCONF(Signal* signal);
  void execDISCONNECT_REP(Signal* signal);
  void execSYSTEM_ERROR(Signal* signal);

  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONNECT_REP(Signal* signal);
  void execNDB_FAILCONF(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execCM_INFOCONF(Signal* signal);
  void execCLOSE_COMCONF(Signal* signal);
  void execAPI_REGREQ(Signal* signal);
  void execAPI_FAILCONF(Signal* signal);
  void execREAD_NODESREQ(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);


  void execAPI_VERSION_REQ(Signal* signal);
  void execAPI_BROADCAST_REP(Signal* signal);

  // Arbitration signals
  void execARBIT_CFG(Signal* signal);
  void execARBIT_PREPREQ(Signal* signal);
  void execARBIT_PREPCONF(Signal* signal);
  void execARBIT_PREPREF(Signal* signal);
  void execARBIT_STARTCONF(Signal* signal);
  void execARBIT_STARTREF(Signal* signal);
  void execARBIT_CHOOSECONF(Signal* signal);
  void execARBIT_CHOOSEREF(Signal* signal);
  void execARBIT_STOPREP(Signal* signal);

  // Statement blocks
  void node_failed(Signal* signal, Uint16 aFailedNode);
  void checkStartInterface(Signal* signal);
  void failReport(Signal* signal,
                  Uint16 aFailedNode,
                  UintR aSendFailRep,
                  FailRep::FailCause failCause);
  void findNeighbours(Signal* signal);
  Uint16 translateDynamicIdToNodeId(Signal* signal, UintR TdynamicId);

  void initData(Signal* signal);
  void sendCloseComReq(Signal* signal, BlockReference TBRef, Uint16 TfailNo);
  void sendPrepFailReq(Signal* signal, Uint16 aNode);
  void sendApiFailReq(Signal* signal, Uint16 aFailedNode);
  void sendApiRegRef(Signal*, Uint32 ref, ApiRegRef::ErrorCode);

  // Generated statement blocks
  void startphase1(Signal* signal);
  void electionWon();
  void cmInfoconf010Lab(Signal* signal);
  void apiHbHandlingLab(Signal* signal);
  void timerHandlingLab(Signal* signal);
  void hbReceivedLab(Signal* signal);
  void sendCmRegrefLab(Signal* signal, BlockReference ref, 
		       CmRegRef::ErrorCode);
  void systemErrorBecauseOtherNodeFailed(Signal* signal, Uint32 line, NodeId);
  void systemErrorLab(Signal* signal, Uint32 line,
		      const char* message = NULL);
  void prepFailReqLab(Signal* signal);
  void prepFailConfLab(Signal* signal);
  void prepFailRefLab(Signal* signal);
  void commitFailReqLab(Signal* signal);
  void commitFailConfLab(Signal* signal);
  void failReportLab(Signal* signal, Uint16 aFailedNode, 
		     FailRep::FailCause aFailCause);
  void sendCommitFailReq(Signal* signal);
  void presToConfLab(Signal* signal);
  void sendSttorryLab(Signal* signal);
  void sttor020Lab(Signal* signal);
  void closeComConfLab(Signal* signal);
  void apiRegReqLab(Signal* signal);
  void regreqTimeLimitLab(Signal* signal);
  void regreqTimeMasterLimitLab(Signal* signal);
  void cmRegreq010Lab(Signal* signal);
  void cmRegconf010Lab(Signal* signal);
  void sttor010Lab(Signal* signal);
  void sendHeartbeat(Signal* signal);
  void checkHeartbeat(Signal* signal);
  void setHbDelay(UintR aHbDelay);
  void setHbApiDelay(UintR aHbApiDelay);
  void setArbitTimeout(UintR aArbitTimeout);

  // Interface to arbitration module
  void handleArbitStart(Signal* signal);
  void handleArbitApiFail(Signal* signal, Uint16 nodeId);
  void handleArbitNdbAdd(Signal* signal, Uint16 nodeId);
  void handleArbitCheck(Signal* signal);

  // Private arbitration routines
  Uint32 getArbitDelay();
  Uint32 getArbitTimeout();
  void startArbitThread(Signal* signal);
  void runArbitThread(Signal* signal);
  void stateArbitInit(Signal* signal);
  void stateArbitFind(Signal* signal);
  void stateArbitPrep(Signal* signal);
  void stateArbitStart(Signal* signal);
  void stateArbitRun(Signal* signal);
  void stateArbitChoose(Signal* signal);
  void stateArbitCrash(Signal* signal);
  void computeArbitNdbMask(NodeBitmask& aMask);
  void reportArbitEvent(Signal* signal, Ndb_logevent_type type);

  // Initialisation
  void initData();
  void initRecords();

  // Transit signals
  // Variables
  
  bool checkAPIVersion(NodeId, Uint32 nodeVersion, Uint32 ownVersion) const;
  bool checkNDBVersion(NodeId, Uint32 nodeVersion, Uint32 ownVersion) const;

  void cmAddPrepare(Signal* signal, NodeRecPtr nodePtr, const NodeRec* self);
  void sendCmAckAdd(Signal *, Uint32 nodeId, CmAdd::RequestType);
  void joinedCluster(Signal* signal, NodeRecPtr nodePtr);
  void sendCmRegReq(Signal * signal, Uint32 nodeId);
  void sendCmNodeInfoReq(Signal* signal, Uint32 nodeId, const NodeRec * self);

private:
  void sendPrepFailReqRef(Signal* signal, 
			  Uint32 dstBlockRef,
			  GlobalSignalNumber gsn,
			  Uint32 blockRef,
			  Uint32 failNo,
			  Uint32 noOfNodes,
			  const NodeId theNodes[]);
    

  
  /* Wait this time until we try to join the       */
  /* cluster again                                 */

  /**** Common stored variables ****/

  NodeRec *nodeRec;
  ArbitRec arbitRec;

  /* Block references ------------------------------*/
  BlockReference cpdistref;	 /* Dist. ref of president   */

  /* Node numbers. ---------------------------------*/
  Uint16 cneighbourl; 		 /* Node no. of lower neighbour  */
  Uint16 cneighbourh; 		 /* Node no. of higher neighbour */
  Uint16 cpresident; 		 /* Node no. of president        */

  /* Counters --------------------------------------*/
  Uint16 cnoOfNodes; 		 /* Static node counter          */
  /* Status flags ----------------------------------*/

  Uint32 c_restartPartialTimeout;

  Uint16 creadyDistCom;
  Uint16 c_regReqReqSent;
  Uint16 c_regReqReqRecv;
  Uint64 c_stopElectionTime;
  Uint16 cpresidentCandidate;
  Uint16 cdelayRegreq;
  Uint16 cpresidentAlive;
  Uint16 cnoFailedNodes;
  Uint16 cnoPrepFailedNodes;
  Uint16 cnoCommitFailedNodes;
  Uint16 cactivateApiCheck;
  UintR chbApiDelay;

  UintR ccommitFailureNr;
  UintR cprepareFailureNr;
  UintR ctoFailureNr;
  UintR cfailureNr;

  QmgrState ctoStatus;
  UintR cLqhTimeSignalCount;
  bool cHbSent;
  NDB_TICKS clatestTransactionCheck;

  class Timer interface_check_timer;
  class Timer hb_check_timer;
  class Timer hb_send_timer;
  class Timer hb_api_timer;


  Uint16 cfailedNodes[MAX_NDB_NODES];
  Uint16 cprepFailedNodes[MAX_NDB_NODES];
  Uint16 ccommitFailedNodes[MAX_NDB_NODES];

};

#endif
