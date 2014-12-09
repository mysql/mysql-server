/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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
#include <signaldata/AllocNodeId.hpp>

#include <RequestTracker.hpp>
#include <signaldata/StopReq.hpp>

#include "timer.hpp"

#define JAM_FILE_ID 362


#ifdef QMGR_C

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
#define ZSTART_FAILURE_LIMIT 6

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

#define QMGR_MAX_FAIL_STATE_BLOCKS 5

class Qmgr : public SimulatedBlock {
public:
  // State values
  enum QmgrState {
    Q_NOT_ACTIVE = 0,
    Q_ACTIVE = 1
  };
  
  enum FailState {
    NORMAL = 0,
    WAITING_FOR_CLOSECOMCONF_ACTIVE = 1,     /* Node had phase ZAPI_ACTIVE */
    WAITING_FOR_CLOSECOMCONF_NOTACTIVE = 2,  /* Node had phase != ZAPI_ACTIVE */
    WAITING_FOR_API_FAILCONF = 3,
    WAITING_FOR_NDB_FAILCONF = 6
  };

  enum Phase {
    ZINIT = 1, 		        /* All nodes start in phase INIT         */
    ZSTARTING = 2, 		/* Node is connecting to cluster         */
    ZRUNNING = 3, 		/* Node is running in the cluster        */
    ZPREPARE_FAIL = 4,          /* PREPARATION FOR FAILURE               */
    ZFAIL_CLOSING = 5,          /* API/NDB IS DISCONNECTING              */
    ZAPI_ACTIVE = 6,            /* API IS RUNNING IN NODE                */
    ZAPI_INACTIVE = 7,          /* Inactive API */
    ZAPI_ACTIVATION_ONGOING = 8 /* API is being activated */
  };

  struct StartRecord {
    StartRecord() {}
    void reset(){ 
      m_startKey++; 
      m_startNode = 0; 
      m_gsn = RNIL; 
      m_nodes.clearWaitingFor();
    }
    Uint32 m_startKey;
    Uint32 m_startNode;
    Uint64 m_startTimeout;
    
    Uint32 m_gsn;
    SignalCounter m_nodes;
    Uint32 m_latest_gci;

    Uint32 m_start_type;
    NdbNodeBitmask m_skip_nodes;
    NdbNodeBitmask m_starting_nodes;
    NdbNodeBitmask m_starting_nodes_w_log;
    NdbNodeBitmask m_no_nodegroup_nodes;

    Uint16 m_president_candidate;
    Uint32 m_president_candidate_gci;
    Uint16 m_regReqReqSent;
    Uint16 m_regReqReqRecv;
    Uint32 m_node_gci[MAX_NDB_NODES];
  } c_start;
  
  NdbNodeBitmask c_definedNodes; // DB nodes in config
  NdbNodeBitmask c_clusterNodes; // DB nodes in cluster
  NodeBitmask c_connectedNodes;  // All kinds of connected nodes

  /**
   * Nodes which we're checking for partitioned cluster
   *
   * i.e. nodes that connect to use, when we already have elected president
   */
  NdbNodeBitmask c_readnodes_nodes;

  Uint32 c_maxDynamicId;

  struct ConnectCheckRec
  {
    bool m_enabled;                     // Config set && all node version OK
    bool m_active;                      // Connectivity check underway?
    Timer m_timer;                      // Check timer object
    Uint32 m_currentRound;              // Last round started
    Uint32 m_tick;                      // Periods elapsed in current check
    NdbNodeBitmask m_nodesPinged;       // Nodes sent a NodePingReq in round
    NdbNodeBitmask m_nodesWaiting;      // Nodes which have not sent a response
    NdbNodeBitmask m_nodesFailedDuring; // Nodes which failed during check
    NdbNodeBitmask m_nodesSuspect;      // Nodes with suspect connectivity

    ConnectCheckRec()
    {
      m_enabled = false;
      m_active = false;
      m_currentRound = 0;
      m_tick = 0;
      m_nodesPinged.clear();
      m_nodesWaiting.clear();
      m_nodesFailedDuring.clear();
      m_nodesSuspect.clear();
    }

    void reportNodeConnect(Uint32 nodeId);
    /* reportNodeFailure.
     * return code true means the connect check is completed
     */
    bool reportNodeFailure(Uint32 nodeId);

    bool getEnabled() const {
      if (m_enabled)
      {
        assert(m_timer.getDelay() > 0);
      }
      return m_enabled;
    }
  };

  ConnectCheckRec m_connectivity_check;

  // Records
  struct NodeRec {
    /*
     * Dynamic id is received from president.  Lower half is next
     * c_maxDynamicId and upper half is hbOrder.  Heartbeat circle is
     * ordered by full dynamic id.  When president fails, only the lower
     * half of dynamic id is used by other nodes to agree on next
     * president (the one with minimum value).
     */
    UintR ndynamicId;
    /*
     * HeartbeatOrder from config.ini.  Takes effect when this node
     * becomes president and starts handing out dynamic ids to starting
     * nodes.  To define a new order, two rolling restarts is required.
     */
    Uint32 hbOrder;
    Phase phase;

    QmgrState sendPrepFailReqStatus;
    QmgrState sendCommitFailReqStatus;
    QmgrState sendPresToStatus;
    FailState failState;
    BlockReference blockRef;
    Uint64 m_secret;
    NDB_TICKS m_alloc_timeout;
    Uint16 m_failconf_blocks[QMGR_MAX_FAIL_STATE_BLOCKS];

    NodeRec() { bzero(m_failconf_blocks, sizeof(m_failconf_blocks)); }
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
    ArbitRec() {}

    enum Method {
      DISABLED = ARBIT_METHOD_DISABLED, // Arbitration disabled
      METHOD_DEFAULT = ARBIT_METHOD_DEFAULT, // Default arbitration
      // Delay commit to give "external" time to arbitrate
      METHOD_EXTERNAL = ARBIT_METHOD_WAITEXTERNAL
    } method;

    ArbitState state;		// state
    bool newstate;		// flag to initialize new state
    unsigned thread;		// identifies a continueB "thread"
    NodeId node;		// current arbitrator candidate
    ArbitTicket ticket;		// ticket
    NodeBitmask apiMask[1+2];	// arbitrators 0=all 1,2=per rank
    NdbNodeBitmask newMask;	// new nodes to process in RUN state
    Uint8 sendCount;		// control send/recv of signals
    Uint8 recvCount;
    NdbNodeBitmask recvMask;	// left to recv
    Uint32 code;		// code field from signal
    Uint32 failureNr;           // cfailureNr at arbitration start
    Uint32 timeout;             // timeout for CHOOSE state
    NDB_TICKS timestamp;	// timestamp for checking timeouts

    inline bool match(ArbitSignalData* sd) {
      return
	node == sd->node &&
	ticket.match(sd->ticket);
    }

    inline void setTimestamp() {
      timestamp = NdbTick_getCurrentTicks();
    }

    inline Uint64 getTimediff() {
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      return NdbTick_Elapsed(timestamp, now).milliSec();
    }
  };
  
  /* State values for handling ENABLE_COMREQ / ENABLE_COMCONF. */
  enum EnableComState {
    ENABLE_COM_CM_ADD_COMMIT = 0,
    ENABLE_COM_CM_COMMIT_NEW = 1,
    ENABLE_COM_API_REGREQ = 2
  };

public:
  Qmgr(Block_context&);
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
  void execSTOP_REQ(Signal* signal);

  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONNECT_REP(Signal* signal);
  void execNDB_FAILCONF(Signal* signal);
  void execNF_COMPLETEREP(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execCM_INFOCONF(Signal* signal);
  void execCLOSE_COMCONF(Signal* signal);
  void execAPI_REGREQ(Signal* signal);
  void execAPI_FAILCONF(Signal* signal);
  void execREAD_NODESREQ(Signal* signal);
  void execAPI_FAILREQ(Signal* signal);

  void execREAD_NODESREF(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);

  void execDIH_RESTARTREF(Signal* signal);
  void execDIH_RESTARTCONF(Signal* signal);
  
  void execAPI_VERSION_REQ(Signal* signal);
  void execAPI_BROADCAST_REP(Signal* signal);

  void execNODE_FAILREP(Signal *);
  void execALLOC_NODEID_REQ(Signal *);
  void execALLOC_NODEID_CONF(Signal *);
  void execALLOC_NODEID_REF(Signal *);
  void completeAllocNodeIdReq(Signal *);
  void execENABLE_COMCONF(Signal *signal);
  void handleEnableComAddCommit(Signal *signal, Uint32 node);
  void handleEnableComCommitNew(Signal *signal);
  void handleEnableComApiRegreq(Signal *signal, Uint32 node);
  void sendApiRegConf(Signal *signal, Uint32 node);
  
  void execSTART_ORD(Signal*);

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

  void execUPGRADE_PROTOCOL_ORD(Signal*);
  
  // Connectivity check signals
  void execNODE_PINGREQ(Signal* signal);
  void execNODE_PINGCONF(Signal* signal);

  // Ndbinfo signal
  void execDBINFO_SCANREQ(Signal *signal);

  // NDBCNTR informing us our node is fully started
  void execNODE_STARTED_REP(Signal *signal);

  // Statement blocks
  void check_readnodes_reply(Signal* signal, Uint32 nodeId, Uint32 gsn);
  Uint32 check_startup(Signal* signal);

  void api_failed(Signal* signal, Uint32 aFailedNode);
  void node_failed(Signal* signal, Uint16 aFailedNode);
  void checkStartInterface(Signal* signal, NDB_TICKS now);
  void failReport(Signal* signal,
                  Uint16 aFailedNode,
                  UintR aSendFailRep,
                  FailRep::FailCause failCause,
                  Uint16 sourceNode);
  void findNeighbours(Signal* signal, Uint32 from);
  Uint16 translateDynamicIdToNodeId(Signal* signal, UintR TdynamicId);

  void initData(Signal* signal);
  void sendCloseComReq(Signal* signal, BlockReference TBRef, Uint16 TfailNo);
  void sendPrepFailReq(Signal* signal, Uint16 aNode);
  void sendApiFailReq(Signal* signal, Uint16 aFailedNode, bool sumaOnly);
  void sendApiRegRef(Signal*, Uint32 ref, ApiRegRef::ErrorCode);

  // Generated statement blocks
  void startphase1(Signal* signal);
  void electionWon(Signal* signal);
  void cmInfoconf010Lab(Signal* signal);
  
  void apiHbHandlingLab(Signal* signal, NDB_TICKS now);
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
		     FailRep::FailCause aFailCause,
                     Uint16 sourceNode);
  void sendCommitFailReq(Signal* signal);
  void presToConfLab(Signal* signal);
  void sendSttorryLab(Signal* signal, bool first_phase);
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
  void setCCDelay(UintR aCCDelay);

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
  void computeArbitNdbMask(NodeBitmaskPOD& aMask);
  void computeArbitNdbMask(NdbNodeBitmaskPOD& aMask);
  void reportArbitEvent(Signal* signal, Ndb_logevent_type type,
                        const NodeBitmask mask = NodeBitmask());

  // Interface to Connectivity Check
  void startConnectivityCheck(Signal* signal, Uint32 reason, Uint32 node);
  void checkConnectivityTimeSignal(Signal* signal);
  void connectivityCheckCompleted(Signal* signal);
  bool isNodeConnectivitySuspect(Uint32 nodeId) const;
  void handleFailFromSuspect(Signal* signal,
                             Uint32 reason,
                             Uint16 aFailedNode,
                             Uint16 sourceNode);

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
			  const NdbNodeBitmask& nodes);

  void handleApiCloseComConf(Signal* signal);
  void add_failconf_block(NodeRecPtr, Uint32 block);
  bool remove_failconf_block(NodeRecPtr, Uint32 block);
  bool is_empty_failconf_block(NodeRecPtr) const;
  
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
  Uint32 c_restartPartionedTimeout;
  Uint32 c_restartFailureTimeout;
  Uint32 c_restartNoNodegroupTimeout;
  NDB_TICKS c_start_election_time;

  Uint16 creadyDistCom;

  Uint16 cdelayRegreq;
  Uint16 cpresidentAlive;
  Uint16 c_allow_api_connect;
  UintR chbApiDelay;

  UintR ccommitFailureNr;
  UintR cprepareFailureNr;
  UintR ctoFailureNr;
  UintR cfailureNr;

  QmgrState ctoStatus;
  bool cHbSent;

  Timer interface_check_timer;
  Timer hb_check_timer;
  Timer hb_send_timer;
  Timer hb_api_timer;


  NdbNodeBitmask cfailedNodes;
  NdbNodeBitmask cprepFailedNodes;
  NdbNodeBitmask ccommitFailedNodes;
  
  struct OpAllocNodeIdReq {
    RequestTracker m_tracker;
    AllocNodeIdReq m_req;
    Uint32 m_connectCount;
    Uint32 m_error;
  };

  struct OpAllocNodeIdReq opAllocNodeIdReq;
  
  StopReq c_stopReq;
  bool check_multi_node_shutdown(Signal* signal);

  void recompute_version_info(Uint32 type);
  void recompute_version_info(Uint32 type, Uint32 version);
  void execNODE_VERSION_REP(Signal* signal);
  void sendApiVersionRep(Signal* signal, NodeRecPtr nodePtr);
  void sendVersionedDb(NodeReceiverGroup rg,
                       GlobalSignalNumber gsn, 
                       Signal* signal, 
                       Uint32 length, 
                       JobBufferLevel jbuf,
                       Uint32 minversion);

  bool m_micro_gcp_enabled;

  // user-defined hbOrder must set all values non-zero and distinct
  int check_hb_order_config();
  bool m_hb_order_config_used;

#ifdef ERROR_INSERT
  Uint32 nodeFailCount;
#endif

  Uint32 get_hb_count(Uint32 nodeId) const {
    return globalData.get_hb_count(nodeId);
  }

  Uint32& set_hb_count(Uint32 nodeId) {
    return globalData.set_hb_count(nodeId);
  }

  void execISOLATE_ORD(Signal* signal);
};


#undef JAM_FILE_ID

#endif
