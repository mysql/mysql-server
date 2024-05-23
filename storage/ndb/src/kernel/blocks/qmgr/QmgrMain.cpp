/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define QMGR_C
#include <NdbSleep.h>
#include <NdbTick.h>
#include <ndb_version.h>
#include <NodeInfo.hpp>
#include <OwnProcessInfo.hpp>
#include <pc.hpp>
#include <signaldata/ApiBroadcast.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/ApiVersion.hpp>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/BlockCommitOrd.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/DihRestart.hpp>
#include <signaldata/DisconnectRep.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EnableCom.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/FailRep.hpp>
#include <signaldata/GetNumMultiTrp.hpp>
#include <signaldata/IsolateOrd.hpp>
#include <signaldata/LocalSysfile.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NodePing.hpp>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/PrepFailReqRef.hpp>
#include <signaldata/ProcessInfoRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/Sync.hpp>
#include <signaldata/SyncThreadViaReqConf.hpp>
#include <signaldata/TakeOverTcConf.hpp>
#include <signaldata/TrpKeepAlive.hpp>
#include <signaldata/Upgrade.hpp>
#include "Qmgr.hpp"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_sockaddr.h"

#include <TransporterRegistry.hpp>  // Get connect address

#include <EventLogger.hpp>
#include "../dbdih/Dbdih.hpp"

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
// #define DEBUG_MULTI_TRP 1
// #define DEBUG_STARTUP 1
// #define DEBUG_ARBIT 1
#endif

#ifdef DEBUG_ARBIT
#define DEB_ARBIT(arglist)       \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_ARBIT(arglist) \
  do {                     \
  } while (0)
#endif

#ifdef DEBUG_MULTI_TRP
#define DEB_MULTI_TRP(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_MULTI_TRP(arglist) \
  do {                         \
  } while (0)
#endif

#ifdef DEBUG_STARTUP
#define DEB_STARTUP(arglist)     \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_STARTUP(arglist) \
  do {                       \
  } while (0)
#endif

// #define DEBUG_QMGR_START
#ifdef DEBUG_QMGR_START
#include <DebuggerNames.hpp>
#define QMGR_DEBUG(x) ndbout << "QMGR " << __LINE__ << ": " << x << endl
#define DEBUG_START(gsn, node, msg) \
  QMGR_DEBUG(getSignalName(gsn) << " to: " << node << " - " << msg)
#define DEBUG_START2(gsn, rg, msg)                                       \
  {                                                                      \
    char nodes[NdbNodeBitmask::TextLength + 1];                          \
    QMGR_DEBUG(getSignalName(gsn)                                        \
               << " to: " << rg.m_nodes.getText(nodes) << " - " << msg); \
  }
#define DEBUG_START3(signal, msg)                                            \
  QMGR_DEBUG(getSignalName(signal->header.theVerId_signalNumber)             \
             << " from " << refToNode(signal->getSendersBlockRef()) << " - " \
             << msg);
#else
#define QMGR_DEBUG(x)
#define DEBUG_START(gsn, node, msg)
#define DEBUG_START2(gsn, rg, msg)
#define DEBUG_START3(signal, msg)
#endif

#define JAM_FILE_ID 360

/**
 * QMGR provides the following services:
 *
 * 1) Node id allocation
 * ---------------------
 *    This is a service provided to the Management server when a node is
 *    requesting a config.
 *
 * 2) Heartbeat service for data nodes
 * -----------------------------------
 *    This is a service provided to the upper levels in NDB. When the
 *    heartbeat discovers a failure it will send a FAIL_REP signal to
 *    NDBCNTR.
 *
 * 3) Master assignment
 * --------------------
 *    NDB relies on that a new master can be allocated at each failure
 *    through the usage of an algorithm to calculate the next master.
 *    To handle this nodes are entering the cluster one node at a time.
 *    This gives each node a dynamic node id, the new master is simply
 *    selected as the node with the lowest dynamic id.
 *
 *    When the cluster is started from scratch it is important to select
 *    a master that is actually part of the cluster startup and not
 *    started later through a node restart handling. To handle this
 *    QMGR makes use of the DIH_RESTART service provided by DIH.
 *    This service will provide the GCI that the node can be started
 *    from. This GCI is sent in each CM_REGREQ signal to ensure that
 *    each node can decide whether they should be assigned as master
 *    of the cluster.
 *
 *    In QMGR the master is called President and in DIH, NDBCNTR and DICT
 *    the node is called master node. All these roles are always given
 *    to the same node. Most protocols have a master role and thus most
 *    protocols need to handle master take over.
 *
 * 4) Transactional node failure service
 * -------------------------------------
 *    Whenever a node fails, we need to ensure that all nodes agree on the
 *    failed nodes. To handle this QMGR uses a prepare phase where the
 *    president sends a list of failed nodes, other nodes can add to this
 *    list in which case a new prepare phase is started. After all nodes
 *    have agreed on the list of failed nodes the QMGR president sends a
 *    list of nodes in the COMMIT_FAILREQ signal that specifies which nodes
 *    have failed. This list is then sent up to NDBCNTR that handles the
 *    spreading of this information to all other blocks in the NDB data
 *    node.
 *
 *    The information is also sent to the connected API nodes.
 *
 * 5) Arbitration service
 * ----------------------
 *    In the case where we are not sure if the cluster has been partitioned,
 *    we need to query an arbitrator to decide whether our node should survive
 *    the crash. If no arbitrator is assigned, the node will fail. The
 *    arbitrator must be prepared before the crash happens, the arbitrator
 *    can only be used for one response. After this response a new arbitrator
 *    must be selected.
 *
 *    It is also possible to not use any arbitrator service provided by NDB.
 *    In this case QMGR will write a message to the Cluster log and the
 *    external arbitrator needs to take action and shut down the node that
 *    it wants to not survive.
 *
 * 6) Skip node service
 * --------------------
 *    When starting a data node it is possible to select a set of nodes to not
 *    wait for in cluster restart. These nodes are provided as startup
 *    parameter in ndbmtd/ndbd, --nowait-nodes.
 *
 * 7) Heartbeat service for API nodes
 * ----------------------------------
 *    QMGR sends heartbeat signals to all API nodes connected with some delay.
 *    If API doesn't send any response, it will shut down the API connection.
 *
 * 8) Read nodes service
 * ---------------------
 *    This is used to check nodes in certain situations.
 *
 * 9) Connectivity check service
 * -----------------------------
 *    In the case of node failures we can configure NDB to make a full
 *    connectivity check before deciding which nodes to assign as failed
 *    nodes.
 *
 * 10) Ndbinfo membership table
 * ----------------------------
 *    Reports the current setup of nodes, their dynamic ids and neighbours.
 *
 * 11) Ndbinfo process table
 * -------------------------
 *    Reports various information required to manage NDB Cluster.
 *
 * 12) Isolate node service
 * ------------------------
 *    Connected to the connectivity check service.
 *
 * 13) Global node state service
 * -----------------------------
 *    Service used by many other blocks to inform them of node status.
 *
 * QMGR uses the following services:
 *
 * 1) Connect service
 * ------------------
 *    The transporter will inform QMGR about nodes connected through the
 *    CONNECT_REP signal.
 *
 * 2) Check node group service in DIH
 * ----------------------------------
 *    Used by master assignment service and node failure services.
 *
 * 3) DIH_RESTART service in DIH
 * -----------------------------
 *    See above in master assignment service.
 *
 * 4) Block commit service
 * -----------------------
 *    Block commits when we form a new cluster after node failures.
 *    This service is provided by DIH.
 *
 * 5) Close communication service
 * ------------------------------
 *    We need to inform transporter when a node has failed to ensure
 *    the transporter will close the communication to this node.
 *
 * 6) Enable communication service
 * -------------------------------
 *    We need to enable communication to a node after we finished node
 *    failure handling for a node.
 */

/**
 * c_start.m_gsn = GSN_CM_REGREQ
 *   Possible for all nodes
 *   c_start.m_nodes contains all nodes in config
 *
 * c_start.m_gsn = GSN_CM_NODEINFOREQ;
 *   Set when receiving CM_REGCONF
 *   State possible for starting node only (not in cluster)
 *
 *   c_start.m_nodes contains all node in alive cluster that
 *                   that has not replied to GSN_CM_NODEINFOREQ
 *                   passed by president in GSN_CM_REGCONF
 *
 * c_start.m_gsn = GSN_CM_ADD
 *   Possible for president only
 *   Set when receiving and accepting CM_REGREQ (to include node)
 *
 *   c_start.m_nodes contains all nodes in alive cluster + starting node
 *                   that has not replied to GSN_CM_ADD
 *                   by sending GSN_CM_ACKADD
 *
 * c_start.m_gsn = GSN_CM_NODEINFOCONF
 *   Possible for non presidents only
 *     c_start.m_nodes contains a node that has been accepted by president
 *     but has not connected to us yet
 */

// Signal entries and statement blocks
/* 4  P R O G R A M        */
/*******************************/
/* CMHEART_BEAT               */
/*******************************/
void Qmgr::execCM_HEARTBEAT(Signal *signal) {
  NodeRecPtr hbNodePtr;
  jamEntry();
  hbNodePtr.i = signal->theData[0];
  ptrCheckGuard(hbNodePtr, MAX_NDB_NODES, nodeRec);
  set_hb_count(hbNodePtr.i) = 0;
  return;
}  // Qmgr::execCM_HEARTBEAT()

/*******************************/
/* CM_NODEINFOREF             */
/*******************************/
void Qmgr::execCM_NODEINFOREF(Signal *signal) {
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}  // Qmgr::execCM_NODEINFOREF()

/*******************************/
/* CONTINUEB                  */
/*******************************/
void Qmgr::execCONTINUEB(Signal *signal) {
  jamEntry();
  const Uint32 tcontinuebType = signal->theData[0];
  const Uint32 tdata0 = signal->theData[1];
  const Uint32 tdata1 = signal->theData[2];
  switch (tcontinuebType) {
    case ZREGREQ_TIMELIMIT:
      jam();
      if (c_start.m_startKey != tdata0 || c_start.m_startNode != tdata1) {
        jam();
        return;
      }  // if
      regreqTimeLimitLab(signal);
      break;
    case ZREGREQ_MASTER_TIMELIMIT:
      jam();
      if (c_start.m_startKey != tdata0 || c_start.m_startNode != tdata1) {
        jam();
        return;
      }  // if
      // regreqMasterTimeLimitLab(signal);
      failReportLab(signal, c_start.m_startNode, FailRep::ZSTART_IN_REGREQ,
                    getOwnNodeId());
      return;
    case ZTIMER_HANDLING:
      jam();
      timerHandlingLab(signal);
      return;
    case ZARBIT_HANDLING:
      jam();
      runArbitThread(signal);
      return;
    case ZSTART_FAILURE_LIMIT: {
      if (cpresident != ZNIL) {
        jam();
        return;
      }
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      const Uint64 elapsed =
          NdbTick_Elapsed(c_start_election_time, now).milliSec();
      if (c_restartFailureTimeout != Uint32(~0) &&
          elapsed > c_restartFailureTimeout) {
        jam();
        BaseString tmp;
        tmp.append(
            "Shutting down node as total restart time exceeds "
            " StartFailureTimeout as set in config file ");
        if (c_restartFailureTimeout == (Uint32)~0)
          tmp.append(" 0 (inifinite)");
        else
          tmp.appfmt(" %d", c_restartFailureTimeout);

        progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, tmp.c_str());
      }
      signal->theData[0] = ZSTART_FAILURE_LIMIT;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);
      return;
    }
    case ZNOTIFY_STATE_CHANGE: {
      jam();
      handleStateChange(signal, tdata0);
      return;
    }
    case ZCHECK_MULTI_TRP_CONNECT: {
      jam();
      check_connect_multi_transporter(signal, tdata0);
      return;
    }
    case ZRESEND_GET_NUM_MULTI_TRP_REQ: {
      jam();
      send_get_num_multi_trp_req(signal, signal->theData[1]);
      return;
    }
    case ZSWITCH_MULTI_TRP: {
      jam();
      send_switch_multi_transporter(signal, signal->theData[1], true);
      return;
    }
    case ZSEND_TRP_KEEP_ALIVE: {
      jam();
      send_trp_keep_alive(signal);
      return;
    }
    default:
      jam();
      // ZCOULD_NOT_OCCUR_ERROR;
      systemErrorLab(signal, __LINE__);
      return;
  }  // switch
  return;
}  // Qmgr::execCONTINUEB()

void Qmgr::execDEBUG_SIG(Signal *signal) {
  NodeRecPtr debugNodePtr;
  jamEntry();
  debugNodePtr.i = signal->theData[0];
  ptrCheckGuard(debugNodePtr, MAX_NODES, nodeRec);
  return;
}  // Qmgr::execDEBUG_SIG()

/*******************************/
/* FAIL_REP                   */
/*******************************/
void Qmgr::execFAIL_REP(Signal *signal) {
  const FailRep *const failRep = (FailRep *)&signal->theData[0];
  const NodeId failNodeId = failRep->failNodeId;
  const FailRep::FailCause failCause = (FailRep::FailCause)failRep->failCause;
  Uint32 failSource = failRep->getFailSourceNodeId(signal->length());
  if (ERROR_INSERT_VALUE >= 951 && ERROR_INSERT_VALUE <= 960) {
    CRASH_INSERTION3();
  }
  if (!failSource) {
    /* Failure source not included, use sender of signal as 'source' */
    failSource = refToNode(signal->getSendersBlockRef());
  }

  CRASH_INSERTION(948);

  jamEntry();
  failReportLab(signal, failNodeId, failCause, failSource);
  return;
}  // Qmgr::execFAIL_REP()

/*******************************/
/* PRES_TOREQ                 */
/*******************************/
void Qmgr::execPRES_TOREQ(Signal *signal) {
  jamEntry();
  BlockReference Tblockref = signal->theData[0];
  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = ccommitFailureNr;
  sendSignal(Tblockref, GSN_PRES_TOCONF, signal, 2, JBA);
  return;
}  // Qmgr::execPRES_TOREQ()

void Qmgr::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();

  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  m_num_multi_trps = 0;
  if (isNdbMt() && globalData.ndbMtSendThreads) {
    ndb_mgm_get_int_parameter(p, CFG_DB_NODE_GROUP_TRANSPORTERS,
                              &m_num_multi_trps);
    if (m_num_multi_trps == 0) {
      jam();
      /**
       * The default assignment is to use the same number of multi
       * transporters as there are LDM instances in this node.
       * So essentially each LDM thread will have its own transporter
       * to the corresponding LDM thread in the other nodes in the
       * same node group. This will ensure that I can assign the
       * transporter to the send thread the LDM thread assists as
       * well.
       */
      m_num_multi_trps = globalData.ndbMtLqhThreads;
    } else {
      jam();
      /**
       * No reason to use more sockets than the maximum threads in one
       * thread group. We select the socket to use based on the
       * instance id of the receiving thread. So if we use more sockets
       * than threads in the largest thread group, there will be unused
       * sockets.
       *
       * So we select the configured number unless the maximum number of
       * LDM and/or TC threads is smaller than this number.
       */
      m_num_multi_trps = MIN(m_num_multi_trps, MAX(globalData.ndbMtLqhThreads,
                                                   globalData.ndbMtTcThreads));
    }
    /**
     * Whatever value this node has chosen, we will never be able to use
     * more transporters than the other node permits as well. This will be
     * established in the setup phase of multi transporters.
     */
  }
  if (m_num_multi_trps == 0) {
    jam();
    m_num_multi_trps = 1;
  }
  m_num_multi_trps = MIN(m_num_multi_trps, MAX_NODE_GROUP_TRANSPORTERS);
  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Qmgr::execSTART_ORD(Signal *signal) {
  /**
   * Start timer handling
   */
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  signal->theData[0] = ZTIMER_HANDLING;
  signal->theData[1] = Uint32(now.getUint64() >> 32);
  signal->theData[2] = Uint32(now.getUint64());
  sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBB);
}

/*
4.2  ADD NODE MODULE*/
/*##########################################################################*/
/*
4.2.1 STTOR     */
/**--------------------------------------------------------------------------
 * Start phase signal, must be handled by all blocks.
 * QMGR is only interested in the first phase.
 * During phase one we clear all registered applications.
 *---------------------------------------------------------------------------*/
/*******************************/
/* STTOR                      */
/*******************************/
void Qmgr::execSTTOR(Signal *signal) {
  jamEntry();

  switch (signal->theData[1]) {
    case 1:
      jam();
      initData(signal);
      g_eventLogger->info("Starting QMGR phase 1");
      c_ndbcntr = (Ndbcntr *)globalData.getBlock(NDBCNTR);
      startphase1(signal);
      recompute_version_info(NodeInfo::DB);
      recompute_version_info(NodeInfo::API);
      recompute_version_info(NodeInfo::MGM);
      return;
    case 3:
      jam();
      break;
    case 7:
      jam();
      if (cpresident == getOwnNodeId()) {
        jam();
        switch (arbitRec.method) {
          case ArbitRec::DISABLED:
            jam();
            break;

          case ArbitRec::METHOD_EXTERNAL:
          case ArbitRec::METHOD_DEFAULT:
            /**
             * Start arbitration thread.  This could be done as soon as
             * we have all nodes (or a winning majority).
             */
            jam();
            handleArbitStart(signal);
            break;
        }
      }
      break;
    case 9: {
      jam();
      /**
       * Enable communication to all API nodes by setting state
       *   to ZFAIL_CLOSING (which will make it auto-open in
       * checkStartInterface)
       */
      if (ERROR_INSERTED(949)) {
        jam();
        g_eventLogger->info("QMGR : Delaying allow-api-connect processing");
        sendSignalWithDelay(reference(), GSN_STTOR, signal, 1000, 2);
        return;
      }
      c_allow_api_connect = 1;
      NodeRecPtr nodePtr;
      for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
        Uint32 type = getNodeInfo(nodePtr.i).m_type;
        if (type != NodeInfo::API) continue;

        ptrAss(nodePtr, nodeRec);
        jam();
        jamLine(Uint16(nodePtr.i));
        if (nodePtr.p->phase == ZAPI_INACTIVE) {
          jam();
          set_hb_count(nodePtr.i) = 3;
          nodePtr.p->phase = ZFAIL_CLOSING;
          nodePtr.p->failState = NORMAL;
        }
      }
    }
  }

  sendSttorryLab(signal, false);
  return;
}  // Qmgr::execSTTOR()

void Qmgr::sendSttorryLab(Signal *signal, bool first_phase) {
  if (first_phase) {
    g_eventLogger->info(
        "Include node protocol completed, phase 1 in QMGR"
        " completed");
  }
  /*****************************/
  /*  STTORRY                  */
  /*****************************/
  signal->theData[3] = 3;
  signal->theData[4] = 7;
  signal->theData[5] = 9;
  signal->theData[6] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
  return;
}  // Qmgr::sendSttorryLab()

void Qmgr::startphase1(Signal *signal) {
  jamEntry();

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  nodePtr.p->phase = ZSTARTING;
  DEB_STARTUP(("phase(%u) = ZSTARTING", nodePtr.i));

  DihRestartReq *req = CAST_PTR(DihRestartReq, signal->getDataPtrSend());
  req->senderRef = reference();
  sendSignal(DBDIH_REF, GSN_DIH_RESTARTREQ, signal, DihRestartReq::SignalLength,
             JBB);
  return;
}

void Qmgr::execDIH_RESTARTREF(Signal *signal) {
  jamEntry();

  ndbrequire(signal->getNoOfSections() == 1);
  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr;
  ndbrequire(handle.getSection(ptr, 0));
  ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
  c_start.m_no_nodegroup_nodes.clear();
  copy(c_start.m_no_nodegroup_nodes.rep.data, ptr);
  releaseSections(handle);

  g_eventLogger->info(
      "DIH reported initial start, now starting the"
      " Node Inclusion Protocol");
  c_start.m_latest_gci = 0;
  execCM_INFOCONF(signal);
}

void Qmgr::execDIH_RESTARTCONF(Signal *signal) {
  jamEntry();

  ndbrequire(signal->getNoOfSections() == 1);
  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr;
  ndbrequire(handle.getSection(ptr, 0));
  ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
  c_start.m_no_nodegroup_nodes.clear();
  copy(c_start.m_no_nodegroup_nodes.rep.data, ptr);
  releaseSections(handle);

  const DihRestartConf *conf =
      CAST_CONSTPTR(DihRestartConf, signal->getDataPtr());
  c_start.m_latest_gci = conf->latest_gci;
  sendReadLocalSysfile(signal);
}

void Qmgr::sendReadLocalSysfile(Signal *signal) {
  ReadLocalSysfileReq *req = (ReadLocalSysfileReq *)signal->getDataPtrSend();
  req->userPointer = 0;
  req->userReference = reference();
  sendSignal(NDBCNTR_REF, GSN_READ_LOCAL_SYSFILE_REQ, signal,
             ReadLocalSysfileReq::SignalLength, JBB);
}

void Qmgr::execREAD_LOCAL_SYSFILE_CONF(Signal *signal) {
  ReadLocalSysfileConf *conf = (ReadLocalSysfileConf *)signal->getDataPtr();
  if (conf->nodeRestorableOnItsOwn ==
      ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN) {
    g_eventLogger->info(
        "DIH reported normal start, now starting the"
        " Node Inclusion Protocol");
  } else if (conf->nodeRestorableOnItsOwn ==
             ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN) {
    /**
     * We set gci = 1 and rely here on that gci here is simply used
     * as a tool to decide which nodes can be started up on their
     * own and which node to choose as master node. Only nodes
     * where m_latest_gci is set to a real GCI can be chosen as
     * master nodes.
     */
    g_eventLogger->info(
        "Node not restorable on its own, now starting the"
        " Node Inclusion Protocol");
    c_start.m_latest_gci = ZUNDEFINED_GCI_LIMIT;
  } else {
    g_eventLogger->info(
        "Node requires initial start, now starting the"
        " Node Inclusion Protocol");
    c_start.m_latest_gci = 0;
  }
  execCM_INFOCONF(signal);
}

void Qmgr::setHbDelay(UintR aHbDelay) {
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  hb_send_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_send_timer.reset(now);
  hb_check_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_check_timer.reset(now);
}

void Qmgr::setHbApiDelay(UintR aHbApiDelay) {
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  chbApiDelay = (aHbApiDelay < 100 ? 100 : aHbApiDelay);
  hb_api_timer.setDelay(chbApiDelay);
  hb_api_timer.reset(now);
}

void Qmgr::setArbitTimeout(UintR aArbitTimeout) {
  arbitRec.timeout = (aArbitTimeout < 10 ? 10 : aArbitTimeout);
}

void Qmgr::setCCDelay(UintR aCCDelay) {
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  if (aCCDelay == 0) {
    /* Connectivity check disabled */
    m_connectivity_check.m_enabled = false;
    m_connectivity_check.m_timer.setDelay(0);
  } else {
    m_connectivity_check.m_enabled = true;
    m_connectivity_check.m_timer.setDelay(aCCDelay < 10 ? 10 : aCCDelay);
    m_connectivity_check.m_timer.reset(now);
  }
}

void Qmgr::setTrpKeepAliveSendDelay(Uint32 delay) {
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  ka_send_timer.setDelay(delay);
  ka_send_timer.reset(now);
}

void Qmgr::execCONNECT_REP(Signal *signal) {
  jamEntry();
  const Uint32 connectedNodeId = signal->theData[0];

  if (ERROR_INSERTED(931)) {
    jam();
    g_eventLogger->info("Discarding CONNECT_REP(%d)", connectedNodeId);
    infoEvent("Discarding CONNECT_REP(%d)", connectedNodeId);
    return;
  }

  if (ERROR_INSERTED(941) &&
      getNodeInfo(connectedNodeId).getType() == NodeInfo::API) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    g_eventLogger->info("Discarding one API CONNECT_REP(%d)", connectedNodeId);
    infoEvent("Discarding one API CONNECT_REP(%d)", connectedNodeId);
    return;
  }

  if (c_connectedNodes.get(connectedNodeId) == false) {
    jam();
    setNodeInfo(connectedNodeId).m_version = 0;
    setNodeInfo(connectedNodeId).m_mysql_version = 0;
  }

  c_connectedNodes.set(connectedNodeId);
  DEB_STARTUP(("c_connectedNodes(%u) set", connectedNodeId));

  {
    NodeRecPtr connectedNodePtr;
    connectedNodePtr.i = connectedNodeId;
    ptrCheckGuard(connectedNodePtr, MAX_NODES, nodeRec);
    connectedNodePtr.p->m_secret = 0;
  }

  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NODES, nodeRec);
  NodeInfo connectedNodeInfo = getNodeInfo(connectedNodeId);
  switch (myNodePtr.p->phase) {
    case ZRUNNING:
      jam();
      if (connectedNodeInfo.getType() == NodeInfo::DB) {
        ndbrequire(!c_clusterNodes.get(connectedNodeId));
      }
      break;
    case ZSTARTING:
      jam();
      break;
    case ZPREPARE_FAIL:
    case ZFAIL_CLOSING:
      jam();
      return;
    case ZAPI_ACTIVATION_ONGOING:
      ndbabort();
    case ZAPI_ACTIVE:
      ndbabort();
    case ZAPI_INACTIVE:
      ndbabort();
    case ZINIT:
      ndbrequire(getNodeInfo(connectedNodeId).m_type == NodeInfo::MGM);
      break;
    default:
      ndbabort();
  }

  if (connectedNodeInfo.getType() != NodeInfo::DB) {
    jam();
    return;
  }

  switch (c_start.m_gsn) {
    case GSN_CM_REGREQ:
      jam();
      sendCmRegReq(signal, connectedNodeId);

      /**
       * We're waiting for CM_REGCONF c_start.m_nodes contains all configured
       *   nodes
       */
      ndbrequire(myNodePtr.p->phase == ZSTARTING);
      ndbrequire(c_start.m_nodes.isWaitingFor(connectedNodeId));
      return;
    case GSN_CM_NODEINFOREQ:
      jam();

      if (c_start.m_nodes.isWaitingFor(connectedNodeId)) {
        jam();
        ndbrequire(getOwnNodeId() != cpresident);
        ndbrequire(myNodePtr.p->phase == ZSTARTING);
        sendCmNodeInfoReq(signal, connectedNodeId, myNodePtr.p);
        return;
      }
      return;
    case GSN_CM_NODEINFOCONF: {
      jam();

      ndbrequire(getOwnNodeId() != cpresident);
      ndbrequire(myNodePtr.p->phase == ZRUNNING);
      if (c_start.m_nodes.isWaitingFor(connectedNodeId)) {
        jam();
        c_start.m_nodes.clearWaitingFor(connectedNodeId);
        c_start.m_gsn = RNIL;

        NodeRecPtr addNodePtr;
        addNodePtr.i = connectedNodeId;
        ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
        cmAddPrepare(signal, addNodePtr, myNodePtr.p);
        return;
      }
    }
    default:
      (void)1;
  }

  ReadNodesReq *req = (ReadNodesReq *)&signal->theData[0];
  ndbrequire(!c_start.m_nodes.isWaitingFor(connectedNodeId));
  ndbrequire(!c_readnodes_nodes.get(connectedNodeId));
  c_readnodes_nodes.set(connectedNodeId);
  req->myRef = reference();
  req->myVersion = NDB_VERSION_D;
  sendSignal(calcQmgrBlockRef(connectedNodeId), GSN_READ_NODESREQ, signal,
             ReadNodesReq::SignalLength, JBA);
  return;
}  // Qmgr::execCONNECT_REP()

void Qmgr::execREAD_NODESCONF(Signal *signal) {
  jamEntry();
  if (signal->getNoOfSections() > 0) {
    jam();
    const ReadNodesConf *readNodes = (ReadNodesConf *)&signal->theData[0];
    ndbrequire(signal->getNoOfSections() == 1);
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz == 5 * NdbNodeBitmask::Size);
    copy((Uint32 *)&readNodes->definedNodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    jam();

    /**
     * Handle transformation from old signal format with 5 bitmask with
     * 2 words in each bitmask to 5 bitmasks with 5 words in each bitmask.
     */
    const ReadNodesConf_v1 *readNodes_v1 =
        (ReadNodesConf_v1 *)&signal->theData[0];
    ReadNodesConf *readNodes = (ReadNodesConf *)&signal->theData[0];

    NdbNodeBitmask48 defined48Nodes;
    NdbNodeBitmask48 inactive48Nodes;
    NdbNodeBitmask48 cluster48Nodes;
    NdbNodeBitmask48 starting48Nodes;
    NdbNodeBitmask48 started48Nodes;

    defined48Nodes.assign(NdbNodeBitmask48::Size, readNodes_v1->definedNodes);
    inactive48Nodes.assign(NdbNodeBitmask48::Size, readNodes_v1->inactiveNodes);
    cluster48Nodes.assign(NdbNodeBitmask48::Size, readNodes_v1->clusterNodes);
    starting48Nodes.assign(NdbNodeBitmask48::Size, readNodes_v1->startingNodes);
    started48Nodes.assign(NdbNodeBitmask48::Size, readNodes_v1->startedNodes);

    NdbNodeBitmask clear_bitmask;
    readNodes->definedNodes = clear_bitmask;
    readNodes->inactiveNodes = clear_bitmask;
    readNodes->clusterNodes = clear_bitmask;
    readNodes->startingNodes = clear_bitmask;
    readNodes->startedNodes = clear_bitmask;

    readNodes->definedNodes = defined48Nodes;
    readNodes->inactiveNodes = inactive48Nodes;
    readNodes->clusterNodes = cluster48Nodes;
    readNodes->startingNodes = starting48Nodes;
    readNodes->startedNodes = started48Nodes;
  }

  check_readnodes_reply(signal, refToNode(signal->getSendersBlockRef()),
                        GSN_READ_NODESCONF);
}

void Qmgr::execREAD_NODESREF(Signal *signal) {
  jamEntry();
  check_readnodes_reply(signal, refToNode(signal->getSendersBlockRef()),
                        GSN_READ_NODESREF);
}

/**
 * Heartbeat Inclusion Protocol Handling
 * -------------------------------------
 * The protocol to include our node in the heartbeat protocol starts when
 * we call execCM_INFOCONF. We start by opening communication to all nodes
 * in the cluster. When we start this protocol we don't know anything about
 * which nodes are up and running and we don't which node is currently the
 * president of the heartbeat protocol.
 *
 * For us to be successful with being included in the heartbeat protocol we
 * need to be connected to all nodes currently in the heartbeat protocol. It
 * is important to remember that QMGR sees a node as alive if it is included
 * in the heartbeat protocol. Higher level notions of aliveness is handled
 * primarily by the DBDIH block, but also to some extent by NDBCNTR.
 *
 * The protocol starts by the new node sending CM_REGREQ to all nodes it is
 * connected to. Only the president will respond to this message. We could
 * have a situation where there currently isn't a president chosen. In this
 * case an election is held whereby a new president is assigned. In the rest
 * of this comment we assume that a president already exists.
 *
 * So if we were connected to the president we will get a response to the
 * CM_REGREQ from the president with CM_REGCONF. The CM_REGCONF contains
 * the set of nodes currently included in the heartbeat protocol.
 *
 * The president will send in parallel to sending CM_REGCONF a CM_ADD(prepare)
 * message to all nodes included in the protocol.
 *
 * When receiving CM_REGCONF the new node will send CM_NODEINFOREQ with
 * information about version of the binary, number of LDM workers and
 * MySQL version of binary.
 *
 * The nodes already included in the heartbeat protocol will wait until it
 * receives both the CM_ADD(prepare) from the president and the
 * CM_NODEINFOREQ from the starting node. When it receives those two
 * messages it will send CM_ACKADD(prepare) to the president and
 * CM_NODEINFOCONF to the starting node with its own node information.
 *
 * When the president received CM_ACKADD(prepare) from all nodes included
 * in the heartbeat protocol then it sends CM_ADD(AddCommit) to all nodes
 * included in the heartbeat protocol.
 *
 * When the nodes receives CM_ADD(AddCommit) from the president then
 * they will enable communication to the new node and immediately start
 * sending heartbeats to the new node. They will also include the new
 * node in their view of the nodes included in the heartbeat protocol.
 * Next they will send CM_ACKADD(AddCommit) back to the president.
 *
 * When the president has received CM_ACKADD(AddCommit) from all nodes
 * included in the heartbeat protocol then it sends CM_ADD(CommitNew)
 * to the starting node.
 *
 * This is also the point where we report the node as included in the
 * heartbeat protocol to DBDIH as from here the rest of the protocol is
 * only about informing the new node about the outcome of inclusion
 * protocol. When we receive the response to this message the new node
 * can already have proceeded a bit into its restart.
 *
 * The starting node after receiving CM_REGCONF waits for all nodes
 * included in the heartbeat protocol to send CM_NODEINFOCONF and
 * also for receiving the CM_ADD(CommitNew) from the president. When
 * all this have been received the new nodes adds itself and all nodes
 * it have been informed about into its view of the nodes included in
 * the heartbeat protocol and enables communication to all other
 * nodes included therein. Finally it sends CM_ACKADD(CommitNew) to
 * the president.
 *
 * When the president has received CM_ACKADD(CommitNew) from the starting
 * node the inclusion protocol is completed and the president is ready
 * to receive a new node into the cluster.
 *
 * It is the responsibility of the starting nodes to retry after a failed
 * node inclusion, they will do so with 3 seconds delay. This means that
 * at most one node per 3 seconds will normally be added to the cluster.
 * So this phase of adding nodes to the cluster can add up to a little bit
 * more than a minute of delay in a large cluster starting up.
 *
 * We try to depict the above in a graph here as well:
 *
 * New node           Nodes included in the heartbeat protocol        President
 * ----------------------------------------------------------------------------
 * ----CM_REGREQ--------------------->>
 * ----CM_REGREQ---------------------------------------------------------->
 *
 * <----------------CM_REGCONF---------------------------------------------
 *                                   <<------CM_ADD(Prepare)---------------
 *
 * -----CM_NODEINFOREQ--------------->>
 *
 * Nodes included in heartbeat protocol can receive CM_ADD(Prepare) and
 * CM_NODEINFOREQ in any order.
 *
 * <<---CM_NODEINFOCONF-------------- --------CM_ACKADD(Prepare)--------->>
 *
 *                                   <<-------CM_ADD(AddCommit)------------
 *
 * Here nodes enables communication to new node and starts sending heartbeats
 *
 *                                   ---------CM_ACKADD(AddCommit)------->>
 *
 * Here we report to DBDIH about new node included in heartbeat protocol
 * in master node.
 *
 * <----CM_ADD(CommitNew)--------------------------------------------------
 *
 * Here new node enables communication to new nodes and starts sending
 * heartbeat messages.
 *
 * -----CM_ACKADD(CommitNew)---------------------------------------------->
 *
 * Here the president can complete the inclusion protocol and is ready to
 * receive new nodes into the heartbeat protocol.
 */
/*******************************/
/* CM_INFOCONF                */
/*******************************/
void Qmgr::execCM_INFOCONF(Signal *signal) {
  /**
   * Open communication to all DB nodes
   */
  signal->theData[0] = 0;  // no answer
  signal->theData[1] = 0;  // no id
  signal->theData[2] = NodeInfo::DB;
  sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 3, JBB);

  cpresident = ZNIL;
  cpresidentAlive = ZFALSE;
  c_start_election_time = NdbTick_getCurrentTicks();

  signal->theData[0] = ZSTART_FAILURE_LIMIT;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);

  cmInfoconf010Lab(signal);

  return;
}  // Qmgr::execCM_INFOCONF()

Uint32 g_start_type = 0;
NdbNodeBitmask g_nowait_nodes;  // Set by clo

void Qmgr::cmInfoconf010Lab(Signal *signal) {
  c_start.m_startKey = 0;
  c_start.m_startNode = getOwnNodeId();
  c_start.m_nodes.clearWaitingFor();
  c_start.m_gsn = GSN_CM_REGREQ;
  c_start.m_starting_nodes.clear();
  c_start.m_starting_nodes_w_log.clear();
  c_start.m_regReqReqSent = 0;
  c_start.m_regReqReqRecv = 0;
  c_start.m_skip_nodes = g_nowait_nodes;
  c_start.m_skip_nodes.bitAND(c_definedNodes);
  c_start.m_start_type = g_start_type;

  NodeRecPtr nodePtr;
  cnoOfNodes = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);

    if (getNodeInfo(nodePtr.i).getType() != NodeInfo::DB) continue;

    c_start.m_nodes.setWaitingFor(nodePtr.i);
    cnoOfNodes++;

    if (!c_connectedNodes.get(nodePtr.i)) continue;

    sendCmRegReq(signal, nodePtr.i);
  }

  //----------------------------------------
  /* Wait for a while. When it returns    */
  /* we will check if we got any CM_REGREF*/
  /* or CM_REGREQ (lower nodeid than our  */
  /* own).                                */
  //----------------------------------------
  signal->theData[0] = ZREGREQ_TIMELIMIT;
  signal->theData[1] = c_start.m_startKey;
  signal->theData[2] = c_start.m_startNode;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 3000, 3);

  creadyDistCom = ZTRUE;
  return;
}  // Qmgr::cmInfoconf010Lab()

void Qmgr::sendCmRegReq(Signal *signal, Uint32 nodeId) {
  CmRegReq *req = (CmRegReq *)&signal->theData[0];
  req->blockRef = reference();
  req->nodeId = getOwnNodeId();
  req->version = NDB_VERSION;
  req->mysql_version = NDB_MYSQL_VERSION_D;
  req->latest_gci = c_start.m_latest_gci;
  req->start_type = c_start.m_start_type;
  const Uint32 ref = calcQmgrBlockRef(nodeId);
  /**
   *  Clear the additional bits, see comment above CmRegReq::SignalLength
   *  in CmRegSignalData for details.
   */
  memset(req->unused_words, 0, sizeof(req->unused_words));
  sendSignal(ref, GSN_CM_REGREQ, signal, CmRegReq::SignalLength, JBB);
  DEB_STARTUP(("CM_REGREQ sent to node %u", nodeId));
  DEBUG_START(GSN_CM_REGREQ, nodeId, "");

  c_start.m_regReqReqSent++;
}

/*
4.4.11 CM_REGREQ */
/**--------------------------------------------------------------------------
 * If this signal is received someone tries to get registrated.
 * Only the president have the authority make decisions about new nodes,
 * so only a president or a node that claims to be the president may send a
 * reply to this signal.
 * This signal can occur any time after that STTOR was received.
 * CPRESIDENT:             Timelimit has expired and someone has
 *                         decided to enter the president role
 * CPRESIDENT_CANDIDATE:
 *     Assigned when we receive a CM_REGREF, if we got more than one REF
 *     then we always keep the lowest nodenumber.
 *     We accept this nodeno as president when our timelimit expires
 * We should consider the following cases:
 * 1- We are the president. If we are busy by adding new nodes to cluster,
 *    then we have to refuse this node to be added.
 *    The refused node will try in ZREFUSE_ADD_TIME seconds again.
 *    If we are not busy then we confirm
 *
 * 2- We know the president, we dont bother us about this REQ.
 *    The president has also got this REQ and will take care of it.
 *
 * 3- The president isn't known. An election is currently ongoing.
 *    This election will not be decided until all nodes in the cluster
 *    except those specifically in skip list has been started.
 *    The skip list comes from the startup parameter --nowait-nodes.
 *    So if no one knows the President it means that we are performing
 *    a cluster startup, either initial or a normal System restart of
 *    the cluster.
 *
 *    In this case we wait until all nodes except those in the skip list
 *    have sent CM_REGREQ to us. If this is the case the node with the
 *    lowest node id AND that can start from the highest GCI promotes itself
 *    to President. Since all nodes follow the same algorithm we are certain
 *    that this will bring us to a point where all nodes has the same node
 *    as President.
 *    In addition this election ensures that the President in QMGR is also
 *    selected as Master in NDBCNTR. It should not be possible that
 *    CNTR_START_REQ gets a response where the Master says that it isn't
 *    the master.
 *
 *     To ensure that the President is equal to the Master we send the
 *     start GCI a node can handle in CM_REGREQ. This enables us to elect
 *     a President that can also act as Master for NDBCNTR.
 *--------------------------------------------------------------------------*/
/*******************************/
/* CM_REGREQ                  */
/*******************************/
static int check_start_type(Uint32 starting, Uint32 own) {
  if (starting == (1 << NodeState::ST_INITIAL_START) &&
      ((own & (1 << NodeState::ST_INITIAL_START)) == 0)) {
    return 1;
  }
  return 0;
}

void Qmgr::execCM_REGREQ(Signal *signal) {
  DEBUG_START3(signal, "");

  NodeRecPtr addNodePtr;
  jamEntry();

  CmRegReq *const cmRegReq = (CmRegReq *)&signal->theData[0];
  const BlockReference Tblockref = cmRegReq->blockRef;
  const Uint32 startingVersion = cmRegReq->version;
  Uint32 startingMysqlVersion = cmRegReq->mysql_version;
  addNodePtr.i = cmRegReq->nodeId;
  Uint32 gci = 1;
  Uint32 start_type = ~0;

  ndbrequire(cmRegReq->nodeId < MAX_NODES);

  if (!c_connectedNodes.get(cmRegReq->nodeId)) {
    jam();

    /**
     * With ndbmtd, there is a race condition such that
     *   CM_REGREQ can arrive prior to CONNECT_REP
     *   since CONNECT_REP is sent from CMVMI
     *
     * In such cases, ignore the CM_REGREQ which is safe
     *   as it will anyway be resent by starting node
     */
    g_eventLogger->info(
        "discarding CM_REGREQ from %u "
        "as we're not yet connected (isNdbMt: %u)",
        cmRegReq->nodeId, (unsigned)isNdbMt());

    return;
  }

  if (signal->getLength() == CmRegReq::SignalLength) {
    jam();
    gci = cmRegReq->latest_gci;
    start_type = cmRegReq->start_type;
  }

  if (creadyDistCom == ZFALSE) {
    jam();
    DEB_STARTUP(("Not ready for distributed communication yet"));
    /* NOT READY FOR DISTRIBUTED COMMUNICATION.*/
    return;
  }  // if

  if (!ndbCompatible_ndb_ndb(NDB_VERSION, startingVersion)) {
    jam();
    DEB_STARTUP(("Incompatible versions"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION,
                    startingVersion);
    return;
  }

  if (!ndbd_upgrade_ok(startingVersion)) {
    jam();
    infoEvent("Connection from node %u refused as it's not ok to upgrade from",
              addNodePtr.i);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION,
                    startingVersion);
    return;
  }

  if (check_start_type(start_type, c_start.m_start_type)) {
    jam();
    DEB_STARTUP(("Incompatible start types"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_START_TYPE,
                    startingVersion);
    return;
  }

  if (cpresident != getOwnNodeId()) {
    jam();

    if (cpresident == ZNIL) {
      /***
       * We don't know the president.
       * If the node to be added has lower node id
       * than it will be our president candidate. Set it as
       * candidate.
       */
      jam();
      if (gci != ZUNDEFINED_GCI_LIMIT &&
          (gci > c_start.m_president_candidate_gci ||
           (gci == c_start.m_president_candidate_gci &&
            addNodePtr.i < c_start.m_president_candidate))) {
        jam();
        c_start.m_president_candidate = addNodePtr.i;
        c_start.m_president_candidate_gci = gci;
        DEB_STARTUP(("President candidate: %u, gci: %u", addNodePtr.i, gci));
      }
      DEB_STARTUP(("Election error to %x", Tblockref));
      sendCmRegrefLab(signal, Tblockref, CmRegRef::ZELECTION, startingVersion);
      return;
    }

    /**
     * We are not the president.
     * We know the president.
     * President will answer.
     */
    DEB_STARTUP(("Not president error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_PRESIDENT,
                    startingVersion);
    return;
  }  // if

  if (c_start.m_startNode != 0) {
    jam();
    /**
     * President busy by adding another node
     */
    DEB_STARTUP(("Busy president error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_PRESIDENT,
                    startingVersion);
    return;
  }

  if (ctoStatus == Q_ACTIVE) {
    jam();
    /**
     * Active taking over as president
     */
    DEB_STARTUP(("President take over error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_TO_PRES,
                    startingVersion);
    return;
  }  // if

  if (getNodeInfo(addNodePtr.i).m_type != NodeInfo::DB) {
    jam();
    /**
     * The new node is not in config file
     */
    DEB_STARTUP(("Not in cfg error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_IN_CFG, startingVersion);
    return;
  }

  if (getNodeState().getSingleUserMode()) {
    /**
     * The cluster is in single user mode.
     * Data node is not allowed to get added in the cluster
     * while in single user mode.
     */
    // handle rolling upgrade
    jam();
    DEB_STARTUP(("Single user mode error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZSINGLE_USER_MODE,
                    startingVersion);
    return;
  }  // if

  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  Phase phase = addNodePtr.p->phase;
  if (phase != ZINIT) {
    jam();
    QMGR_DEBUG("phase = " << phase);
    DEB_STARTUP(("Not dead error"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_DEAD, startingVersion);
    return;
  }

  jam();
  /**
   * WE ARE PRESIDENT AND WE ARE NOT BUSY ADDING ANOTHER NODE.
   * WE WILL TAKE CARE OF THE INCLUSION OF THIS NODE INTO THE CLUSTER.
   * WE NEED TO START TIME SUPERVISION OF THIS. SINCE WE CANNOT STOP
   * TIMED SIGNAL IF THE INCLUSION IS INTERRUPTED WE IDENTIFY
   * EACH INCLUSION WITH A UNIQUE IDENTITY. THIS IS CHECKED WHEN
   * THE SIGNAL ARRIVES. IF IT HAS CHANGED THEN WE SIMPLY IGNORE
   * THE TIMED SIGNAL.
   */

  /**
   * Update start record
   */
  c_start.m_startKey++;
  c_start.m_startNode = addNodePtr.i;
  DEB_STARTUP(("Node %u is starting node", addNodePtr.i));

  /**
   * Assign dynamic id
   */
  UintR TdynId = (++c_maxDynamicId) & 0xFFFF;
  TdynId |= (addNodePtr.p->hbOrder << 16);
  setNodeInfo(addNodePtr.i).m_version = startingVersion;
  setNodeInfo(addNodePtr.i).m_mysql_version = startingMysqlVersion;
  recompute_version_info(NodeInfo::DB, startingVersion);
  addNodePtr.p->ndynamicId = TdynId;

  /**
   * Reply with CM_REGCONF
   */
  CmRegConf *const cmRegConf = (CmRegConf *)&signal->theData[0];
  cmRegConf->presidentBlockRef = reference();
  cmRegConf->presidentNodeId = getOwnNodeId();
  cmRegConf->presidentVersion = getNodeInfo(getOwnNodeId()).m_version;
  cmRegConf->presidentMysqlVersion =
      getNodeInfo(getOwnNodeId()).m_mysql_version;
  cmRegConf->dynamicId = TdynId;
  const Uint32 packed_nodebitmask_length =
      c_clusterNodes.getPackedLengthInWords();
#ifdef DEBUG_STARTUP
  {
    char node_mask[NdbNodeBitmask::TextLength + 1];
    c_clusterNodes.getText(node_mask);
    DEB_STARTUP(
        ("Sending CM_REGCONF from president, c_clusterNodes: %s", node_mask));
  }
#endif
  if (ndbd_send_node_bitmask_in_section(startingVersion)) {
    jam();
    // Send node bitmask in linear section.
    LinearSectionPtr lsptr[3];

    // 8192 is the size of signal->theData array.
    static_assert(CmRegConf::SignalLength_v1 + NdbNodeBitmask::Size <=
                  NDB_ARRAY_SIZE(signal->theData));
    c_clusterNodes.copyto(packed_nodebitmask_length,
                          &signal->theData[CmRegConf::SignalLength_v1]);
    lsptr[0].p = &signal->theData[CmRegConf::SignalLength_v1];
    lsptr[0].sz = packed_nodebitmask_length;

    DEB_STARTUP(("Sending CM_REGCONF to %x", Tblockref));
    sendSignal(Tblockref, GSN_CM_REGCONF, signal, CmRegConf::SignalLength, JBA,
               lsptr, 1);
  } else if (packed_nodebitmask_length <= NdbNodeBitmask48::Size) {
    jam();
    c_clusterNodes.copyto(NdbNodeBitmask48::Size, cmRegConf->allNdbNodes_v1);
    DEB_STARTUP(("2:Sending CM_REGCONF to %x", Tblockref));
    sendSignal(Tblockref, GSN_CM_REGCONF, signal, CmRegConf::SignalLength_v1,
               JBA);
  } else {
    infoEvent(
        "Connection from node %u refused as it does not support node "
        "bitmask in signal section.",
        addNodePtr.i);
    DEB_STARTUP(("Incompatible start types"));
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_START_TYPE,
                    startingVersion);
  }
  DEBUG_START(GSN_CM_REGCONF, refToNode(Tblockref), "");

  /**
   * Send CmAdd to all nodes (including starting)
   */
  c_start.m_nodes = c_clusterNodes;
  c_start.m_nodes.setWaitingFor(addNodePtr.i);
  c_start.m_gsn = GSN_CM_ADD;

  NodeReceiverGroup rg(QMGR, c_start.m_nodes);
  CmAdd *const cmAdd = (CmAdd *)signal->getDataPtrSend();
  cmAdd->requestType = CmAdd::Prepare;
  cmAdd->startingNodeId = addNodePtr.i;
  cmAdd->startingVersion = startingVersion;
  cmAdd->startingMysqlVersion = startingMysqlVersion;
  sendSignal(rg, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
  DEBUG_START2(GSN_CM_ADD, rg, "Prepare");

  /**
   * Set timer
   */
  return;
  signal->theData[0] = ZREGREQ_MASTER_TIMELIMIT;
  signal->theData[1] = c_start.m_startKey;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 30000, 2);

  return;
}  // Qmgr::execCM_REGREQ()

void Qmgr::sendCmRegrefLab(Signal *signal, BlockReference TBRef,
                           CmRegRef::ErrorCode Terror,
                           Uint32 remote_node_version) {
  const Uint32 remoteNodeVersion = remote_node_version;

  CmRegRef *ref = (CmRegRef *)signal->getDataPtrSend();
  ref->blockRef = reference();
  ref->nodeId = getOwnNodeId();
  ref->errorCode = Terror;
  ref->presidentCandidate =
      (cpresident == ZNIL ? c_start.m_president_candidate : cpresident);
  ref->candidate_latest_gci = c_start.m_president_candidate_gci;
  ref->latest_gci = c_start.m_latest_gci;
  ref->start_type = c_start.m_start_type;
  Uint32 packed_nodebitmask_length =
      c_start.m_skip_nodes.getPackedLengthInWords();

  if (ndbd_send_node_bitmask_in_section(remoteNodeVersion)) {
    jam();
    // Send node bitmask in linear section.
    LinearSectionPtr lsptr[3];
    c_start.m_skip_nodes.copyto(packed_nodebitmask_length,
                                &signal->theData[CmRegRef::SignalLength_v1]);
    lsptr[0].p = &signal->theData[CmRegRef::SignalLength_v1];
    lsptr[0].sz = packed_nodebitmask_length;

    sendSignal(TBRef, GSN_CM_REGREF, signal, CmRegRef::SignalLength, JBB, lsptr,
               1);
  } else if (packed_nodebitmask_length <= NdbNodeBitmask48::Size) {
    jam();
    c_start.m_skip_nodes.copyto(NdbNodeBitmask48::Size, ref->skip_nodes_v1);
    sendSignal(TBRef, GSN_CM_REGREF, signal, CmRegRef::SignalLength_v1, JBB);
  } else {
    /**
     * Node bitmask cannot be sent to other node since it is longer
     * than two words. We crash if the error is not ZINCOMPATIBLE_VERSION
     * or ZINCOMPATIBLE_START_TYPE since other errors may change the state
     * of qmgr. Also, other errors require us to have the correct bitmask
     * for proper functioning.
     */
    ndbrequire((Terror == CmRegRef::ZINCOMPATIBLE_VERSION) ||
               (Terror == CmRegRef::ZINCOMPATIBLE_START_TYPE));
    memset(ref->skip_nodes_v1, 0, sizeof(ref->skip_nodes_v1));
    sendSignal(TBRef, GSN_CM_REGREF, signal, CmRegRef::SignalLength_v1, JBB);
  }
  DEBUG_START(GSN_CM_REGREF, refToNode(TBRef), "");
  return;
}  // Qmgr::sendCmRegrefLab()

/*
4.4.11 CM_REGCONF */
/**--------------------------------------------------------------------------
 * President gives permission to a node which wants to join the cluster.
 * The president will prepare the cluster that a new node will be added to
 * cluster. When the new node has set up all connections to the cluster,
 * the president will send commit to all clusternodes so the phase of the
 * new node can be changed to ZRUNNING.
 *--------------------------------------------------------------------------*/
/*******************************/
/* CM_REGCONF                 */
/*******************************/
void Qmgr::execCM_REGCONF(Signal *signal) {
  DEBUG_START3(signal, "");

  NodeRecPtr myNodePtr;
  NodeRecPtr nodePtr;
  jamEntry();

  CmRegConf *const cmRegConf = (CmRegConf *)&signal->theData[0];

  DEB_STARTUP(("Received CM_REGCONF"));
  NdbNodeBitmask allNdbNodes;
  if (signal->getNoOfSections() >= 1) {
    // copy node bitmask to cmRegConf->allNdbNodes from the signal section
    jam();
    ndbrequire(ndbd_send_node_bitmask_in_section(cmRegConf->presidentVersion));
    SectionHandle handle(this, signal);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(allNdbNodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    allNdbNodes.assign(NdbNodeBitmask48::Size, cmRegConf->allNdbNodes_v1);
  }

  if (!ndbCompatible_ndb_ndb(NDB_VERSION, cmRegConf->presidentVersion)) {
    jam();
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf),
                         "incompatible version own=0x%x other=0x%x, "
                         " shutting down",
                         NDB_VERSION, cmRegConf->presidentVersion);
    progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION, buf);
    return;
  }

  if (!ndbd_upgrade_ok(cmRegConf->presidentVersion)) {
    jam();
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf),
                         "Not okay to upgrade from 0x%x, "
                         "shutting down",
                         cmRegConf->presidentVersion);
    progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION, buf);
    return;
  }

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  ndbrequire(c_start.m_gsn == GSN_CM_REGREQ);
  ndbrequire(myNodePtr.p->phase == ZSTARTING);

  cpdistref = cmRegConf->presidentBlockRef;
  cpresident = cmRegConf->presidentNodeId;
  UintR TdynamicId = cmRegConf->dynamicId;
  c_maxDynamicId = TdynamicId & 0xFFFF;
  c_clusterNodes.assign(allNdbNodes);

  myNodePtr.p->ndynamicId = TdynamicId;

  // set own MT config here or in REF, and others in CM_NODEINFOREQ/CONF
  setNodeInfo(getOwnNodeId()).m_lqh_workers = globalData.ndbMtLqhWorkers;
  setNodeInfo(getOwnNodeId()).m_query_threads = globalData.ndbMtQueryThreads;
  setNodeInfo(getOwnNodeId()).m_log_parts = globalData.ndbLogParts;

#ifdef DEBUG_STARTUP
  {
    char node_mask[NdbNodeBitmask::TextLength + 1];
    c_clusterNodes.getText(node_mask);
    DEB_STARTUP(("CM_REGCONF from president: %u, c_clusterNodes: %s",
                 cpresident, node_mask));
  }
#endif
  /*--------------------------------------------------------------*/
  // Send this as an EVENT REPORT to inform about hearing about
  // other NDB node proclaiming to be president.
  /*--------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = TdynamicId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    if (c_clusterNodes.get(nodePtr.i)) {
      jamLine(nodePtr.i);
      ptrAss(nodePtr, nodeRec);

      DEB_MULTI_TRP(("Node %u in ZRUNNING", nodePtr.i));
      ndbrequire(nodePtr.p->phase == ZINIT);
      nodePtr.p->phase = ZRUNNING;
      DEB_STARTUP(("phase(%u) = ZRUNNING", nodePtr.i));

      if (c_connectedNodes.get(nodePtr.i)) {
        jam();
        sendCmNodeInfoReq(signal, nodePtr.i, myNodePtr.p);
      }
    }
  }

  c_start.m_gsn = GSN_CM_NODEINFOREQ;
  c_start.m_nodes = c_clusterNodes;

  if (ERROR_INSERTED(937)) {
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 500, 1);
  }

  return;
}  // Qmgr::execCM_REGCONF()

void Qmgr::check_readnodes_reply(Signal *signal, Uint32 nodeId, Uint32 gsn) {
  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  NodeRecPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  ndbrequire(c_readnodes_nodes.get(nodeId));
  ReadNodesConf *conf = (ReadNodesConf *)signal->getDataPtr();
  ReadNodesReq *req = (ReadNodesReq *)signal->getDataPtrSend();
  if (gsn == GSN_READ_NODESREF) {
    jam();
  retry:
    req->myRef = reference();
    req->myVersion = NDB_VERSION_D;
    sendSignal(calcQmgrBlockRef(nodeId), GSN_READ_NODESREQ, signal,
               ReadNodesReq::SignalLength, JBA);
    return;
  }

  if (conf->masterNodeId == ZNIL) {
    jam();
    goto retry;
  }

  Uint32 president = conf->masterNodeId;
  if (president == cpresident) {
    jam();
    c_readnodes_nodes.clear(nodeId);
    return;
  }

  char buf[255];
  BaseString::snprintf(buf, sizeof(buf),
                       "check StartPartialTimeout, "
                       "node %d thinks %d is president, "
                       "I think president is: %d",
                       nodeId, president, cpresident);

  g_eventLogger->info("%s", buf);
  CRASH_INSERTION(933);

  if (getNodeState().startLevel == NodeState::SL_STARTED) {
    jam();
    NdbNodeBitmask part = conf->clusterNodes;
    FailRep *rep = (FailRep *)signal->getDataPtrSend();
    rep->failCause = FailRep::ZPARTITIONED_CLUSTER;
    rep->partitioned.president = cpresident;
    memset(rep->partitioned.partition_v1, 0,
           sizeof(rep->partitioned.partition_v1));
    rep->partitioned.partitionFailSourceNodeId = getOwnNodeId();
    Uint32 ref = calcQmgrBlockRef(nodeId);
    Uint32 i = 0;
    /* Send source of event info if a node supports it */
    Uint32 length =
        FailRep::OrigSignalLength + FailRep::PartitionedExtraLength_v1;
    Uint32 packed_bitmask_length = c_clusterNodes.getPackedLengthInWords();

    while ((i = part.find(i + 1)) != NdbNodeBitmask::NotFound) {
      if (i == nodeId) continue;
      rep->failNodeId = i;
      if (ndbd_send_node_bitmask_in_section(
              getNodeInfo(refToNode(ref)).m_version)) {
        jam();
        // Send node bitmask in signal section.
        LinearSectionPtr lsptr[3];
        Uint32 *temp_buffer =
            &signal->theData[FailRep::SignalLength +
                             FailRep::PartitionedExtraLength_v1];
        c_clusterNodes.copyto(packed_bitmask_length, temp_buffer);
        lsptr[0].p = temp_buffer;
        lsptr[0].sz = c_clusterNodes.getPackedLengthInWords();
        sendSignal(ref, GSN_FAIL_REP, signal,
                   length + FailRep::SourceExtraLength, JBA, lsptr, 1);
      } else if (packed_bitmask_length <= 2) {
        jam();
        c_clusterNodes.copyto(NdbNodeBitmask48::Size,
                              rep->partitioned.partition_v1);
        sendSignal(ref, GSN_FAIL_REP, signal,
                   length + FailRep::SourceExtraLength, JBA);
      } else {
        ndbabort();
      }
    }
    rep->failNodeId = nodeId;

    if (ndbd_send_node_bitmask_in_section(
            getNodeInfo(refToNode(ref)).m_version)) {
      jam();
      // Send node bitmask in signal section.
      LinearSectionPtr lsptr[3];
      Uint32 *temp_buffer =
          &signal->theData[FailRep::SignalLength +
                           FailRep::PartitionedExtraLength_v1];
      c_clusterNodes.copyto(packed_bitmask_length, temp_buffer);
      lsptr[0].p = temp_buffer;
      lsptr[0].sz = c_clusterNodes.getPackedLengthInWords();
      // clear the unused bits
      memset(rep->partitioned.partition_v1, 0,
             sizeof(rep->partitioned.partition_v1));
      sendSignal(ref, GSN_FAIL_REP, signal, length + FailRep::SourceExtraLength,
                 JBA, lsptr, 1);
    } else if (packed_bitmask_length <= 2) {
      jam();
      sendSignal(ref, GSN_FAIL_REP, signal, length + FailRep::SourceExtraLength,
                 JBB);
    } else {
      ndbabort();
    }
    return;
  }

  CRASH_INSERTION(932);
  CRASH_INSERTION(938);

  progError(__LINE__, NDBD_EXIT_PARTITIONED_SHUTDOWN, buf);

  ndbabort();
}

void Qmgr::sendCmNodeInfoReq(Signal *signal, Uint32 nodeId,
                             const NodeRec *self) {
  CmNodeInfoReq *const req = (CmNodeInfoReq *)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->dynamicId = self->ndynamicId;
  req->version = getNodeInfo(getOwnNodeId()).m_version;
  req->mysql_version = getNodeInfo(getOwnNodeId()).m_mysql_version;
  req->lqh_workers = getNodeInfo(getOwnNodeId()).m_lqh_workers;
  req->query_threads = getNodeInfo(getOwnNodeId()).m_query_threads;
  req->log_parts = getNodeInfo(getOwnNodeId()).m_log_parts;
  const Uint32 ref = calcQmgrBlockRef(nodeId);
  sendSignal(ref, GSN_CM_NODEINFOREQ, signal, CmNodeInfoReq::SignalLength, JBB);
  DEBUG_START(GSN_CM_NODEINFOREQ, nodeId, "");
}

/*
4.4.11 CM_REGREF */
/**--------------------------------------------------------------------------
 * Only a president or a president candidate can refuse a node to get added to
 * the cluster.
 * Refuse reasons:
 * ZBUSY         We know that the sender is the president and we have to
 *               make a new CM_REGREQ.
 * ZNOT_IN_CFG   This node number is not specified in the configfile,
 *               SYSTEM ERROR
 * ZELECTION     Sender is a president candidate, his timelimit
 *               hasn't expired so maybe someone else will show up.
 *               Update the CPRESIDENT_CANDIDATE, then wait for our
 *               timelimit to expire.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_REGREF                  */
/*******************************/
static const char *get_start_type_string(Uint32 st) {
  static char buf[256];

  if (st == 0) {
    return "<ANY>";
  } else {
    buf[0] = 0;
    for (Uint32 i = 0; i < NodeState::ST_ILLEGAL_TYPE; i++) {
      if (st & (1 << i)) {
        if (buf[0]) strcat(buf, "/");
        switch (i) {
          case NodeState::ST_INITIAL_START:
            strcat(buf, "inital start");
            break;
          case NodeState::ST_SYSTEM_RESTART:
            strcat(buf, "system restart");
            break;
          case NodeState::ST_NODE_RESTART:
            strcat(buf, "node restart");
            break;
          case NodeState::ST_INITIAL_NODE_RESTART:
            strcat(buf, "initial node restart");
            break;
        }
      }
    }
    return buf;
  }
}

void Qmgr::execCM_REGREF(Signal *signal) {
  jamEntry();

  CmRegRef *ref = (CmRegRef *)signal->getDataPtr();
  UintR TaddNodeno = ref->nodeId;
  ndbrequire(TaddNodeno < MAX_NDB_NODES);

  UintR TrefuseReason = ref->errorCode;
  Uint32 candidate = ref->presidentCandidate;
  Uint32 node_gci = 1;
  Uint32 candidate_gci = 1;
  Uint32 start_type = ~0;
  NdbNodeBitmask skip_nodes;
  DEBUG_START3(signal, TrefuseReason);

  ndbrequire(signal->getLength() >= CmRegRef::SignalLength);
  node_gci = ref->latest_gci;
  candidate_gci = ref->candidate_latest_gci;
  start_type = ref->start_type;

  // check if node bitmask is in signal section
  if (signal->getNoOfSections() >= 1) {
    jam();
    ndbrequire(signal->getLength() >= CmRegRef::SignalLength);
    SectionHandle handle(this, signal);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));

    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(skip_nodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    skip_nodes.assign(NdbNodeBitmask48::Size, ref->skip_nodes_v1);
  }

  c_start.m_regReqReqRecv++;

  // Ignore block reference in data[0]

  if (candidate != c_start.m_president_candidate) {
    jam();
    c_start.m_regReqReqRecv = ~0;
  }

  c_start.m_starting_nodes.set(TaddNodeno);
  if (node_gci > ZUNDEFINED_GCI_LIMIT) {
    jam();
    c_start.m_starting_nodes_w_log.set(TaddNodeno);
  }
  c_start.m_node_gci[TaddNodeno] = node_gci;

  skip_nodes.bitAND(c_definedNodes);
  c_start.m_skip_nodes.bitOR(skip_nodes);

  // set own MT config here or in CONF, and others in CM_NODEINFOREQ/CONF
  setNodeInfo(getOwnNodeId()).m_lqh_workers = globalData.ndbMtLqhWorkers;
  setNodeInfo(getOwnNodeId()).m_query_threads = globalData.ndbMtQueryThreads;
  setNodeInfo(getOwnNodeId()).m_log_parts = globalData.ndbLogParts;

  char buf[100];
  switch (TrefuseReason) {
    case CmRegRef::ZINCOMPATIBLE_VERSION:
      jam();
      progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION,
                "incompatible version, "
                "connection refused by running ndb node");
    case CmRegRef::ZINCOMPATIBLE_START_TYPE:
      jam();
      BaseString::snprintf(
          buf, sizeof(buf),
          "incompatible start type detected: node %d"
          " reports %s(%d) my start type: %s(%d)",
          TaddNodeno, get_start_type_string(start_type), start_type,
          get_start_type_string(c_start.m_start_type), c_start.m_start_type);
      progError(__LINE__, NDBD_EXIT_SR_RESTARTCONFLICT, buf);
      break;
    case CmRegRef::ZBUSY:
    case CmRegRef::ZBUSY_TO_PRES:
    case CmRegRef::ZBUSY_PRESIDENT:
      jam();
      cpresidentAlive = ZTRUE;
      signal->theData[3] = 0;
      break;
    case CmRegRef::ZNOT_IN_CFG:
      jam();
      progError(__LINE__, NDBD_EXIT_NODE_NOT_IN_CONFIG);
      break;
    case CmRegRef::ZNOT_DEAD:
      jam();
      progError(__LINE__, NDBD_EXIT_NODE_NOT_DEAD);
      break;
    case CmRegRef::ZSINGLE_USER_MODE:
      jam();
      progError(__LINE__, NDBD_EXIT_SINGLE_USER_MODE);
      break;
    /**
     * For generic refuse error.
     * e.g. in online upgrade, we can use this error code instead
     * of the incompatible error code.
     */
    case CmRegRef::ZGENERIC:
      jam();
      progError(__LINE__, NDBD_EXIT_GENERIC);
      break;
    case CmRegRef::ZELECTION:
      jam();
      if (candidate_gci != ZUNDEFINED_GCI_LIMIT &&
          (candidate_gci > c_start.m_president_candidate_gci ||
           (candidate_gci == c_start.m_president_candidate_gci &&
            candidate < c_start.m_president_candidate))) {
        jam();
        //----------------------------------------
        /* We may already have a candidate      */
        /* choose the lowest nodeno             */
        //----------------------------------------
        signal->theData[3] = 2;
        c_start.m_president_candidate = candidate;
        c_start.m_president_candidate_gci = candidate_gci;
        DEB_STARTUP(
            ("2:President candidate: %u, gci: %u", candidate, candidate_gci));
      } else {
        signal->theData[3] = 4;
      }  // if
      break;
    case CmRegRef::ZNOT_PRESIDENT:
      jam();
      cpresidentAlive = ZTRUE;
      signal->theData[3] = 3;
      break;
    default:
      jam();
      signal->theData[3] = 5;
      /*empty*/;
      break;
  }  // switch
  /*--------------------------------------------------------------*/
  // Send this as an EVENT REPORT to inform about hearing about
  // other NDB node proclaiming not to be president.
  /*--------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_CM_REGREF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = TaddNodeno;
  //-----------------------------------------
  // signal->theData[3] filled in above
  //-----------------------------------------
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  if (cpresidentAlive == ZTRUE) {
    jam();
    QMGR_DEBUG("cpresidentAlive");
    return;
  }

  if (c_start.m_regReqReqSent != c_start.m_regReqReqRecv) {
    jam();
    QMGR_DEBUG(c_start.m_regReqReqSent << " != " << c_start.m_regReqReqRecv);
    return;
  }

  if (c_start.m_president_candidate != getOwnNodeId()) {
    jam();
    QMGR_DEBUG("i'm not the candidate");
    return;
  }

  /**
   * All connected nodes has agreed
   */
  if (check_startup(signal)) {
    jam();
    electionWon(signal);
  }

  return;
}  // Qmgr::execCM_REGREF()

/**
 * This function contains the logic to decide if we won the election.
 * A prerequisite to win an election is that no one is president and
 * that all nodes in the cluster have tried to register (except those
 * nodes in the skip list). We will wait for a time even for the skip
 * nodes. Each node has sent its starting GCI, so we can also ensure
 * that any node elected as President can also act as Master in NDBCNTR.
 */
Uint32 Qmgr::check_startup(Signal *signal) {
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(c_start_election_time, now).milliSec();
  const Uint64 partitionedTimeout =
      c_restartPartitionedTimeout == Uint32(~0)
          ? Uint32(~0)
          : (c_restartPartialTimeout + c_restartPartitionedTimeout);

  const bool no_nodegroup_active =
      (c_restartNoNodegroupTimeout != ~Uint32(0)) &&
      (!c_start.m_no_nodegroup_nodes.isclear());

  /**
   * First see if we should wait more...
   */
  NdbNodeBitmask tmp;
  tmp.bitOR(c_start.m_skip_nodes);
  tmp.bitOR(c_start.m_starting_nodes);

  NdbNodeBitmask wait;
  wait.assign(c_definedNodes);
  wait.bitANDC(tmp);

  Uint32 retVal = 0;
  Uint32 incompleteng = MAX_NDB_NODES;  // Illegal value
  NdbNodeBitmask report_mask;

  if ((c_start.m_latest_gci == 0) ||
      (c_start.m_start_type == (1 << NodeState::ST_INITIAL_START))) {
    if (tmp.equal(c_definedNodes)) {
      jam();
      signal->theData[1] = 0x8000;
      report_mask.assign(c_definedNodes);
      report_mask.bitANDC(c_start.m_starting_nodes);
      retVal = 1;
      goto start_report;
    } else if (no_nodegroup_active) {
      jam();
      if (elapsed < c_restartNoNodegroupTimeout) {
        jam();
        signal->theData[1] = 6;
        signal->theData[2] =
            Uint32((c_restartNoNodegroupTimeout - elapsed + 500) / 1000);
        report_mask.assign(wait);
        retVal = 0;
        goto start_report;
      }
      tmp.bitOR(c_start.m_no_nodegroup_nodes);
      if (tmp.equal(c_definedNodes)) {
        jam();
        signal->theData[1] = 0x8000;
        report_mask.assign(c_definedNodes);
        report_mask.bitANDC(c_start.m_starting_nodes);
        retVal = 1;
        goto start_report;
      } else {
        jam();
        signal->theData[1] = 1;
        signal->theData[2] = ~0;
        report_mask.assign(wait);
        retVal = 0;
        goto start_report;
      }
    } else {
      jam();
      signal->theData[1] = 1;
      signal->theData[2] = ~0;
      report_mask.assign(wait);
      retVal = 0;
      goto start_report;
    }
  }

  if (c_restartNoNodegroupTimeout != Uint32(~0) &&
      elapsed >= c_restartNoNodegroupTimeout) {
    jam();
    tmp.bitOR(c_start.m_no_nodegroup_nodes);
  }

  {
    jam();
    const bool all = c_start.m_starting_nodes.equal(c_definedNodes);
    CheckNodeGroups *sd = (CheckNodeGroups *)&signal->theData[0];

    {
      /**
       * Check for missing node group directly
       */
      NdbNodeBitmask check;
      check.assign(c_definedNodes);
      check.bitANDC(c_start.m_starting_nodes);      // Keep not connected nodes
      check.bitOR(c_start.m_starting_nodes_w_log);  // Add nodes with log

      sd->blockRef = reference();
      sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
      sd->mask = check;
      EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                     CheckNodeGroups::SignalLengthArbitCheckShort);

      if (sd->output == CheckNodeGroups::Lose) {
        jam();
        goto missing_nodegroup;
      }
    }

    jam();
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = c_start.m_starting_nodes;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                   CheckNodeGroups::SignalLengthArbitCheckShort);

    const Uint32 result = sd->output;

    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = c_start.m_starting_nodes_w_log;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                   CheckNodeGroups::SignalLengthArbitCheckShort);

    const Uint32 result_w_log = sd->output;

    if (tmp.equal(c_definedNodes)) {
      /**
       * All nodes (wrt no-wait nodes) has connected...
       *   this means that we will now start or die
       */
      jam();
      switch (result_w_log) {
        case CheckNodeGroups::Lose: {
          jam();
          goto missing_nodegroup;
        }
        case CheckNodeGroups::Win: {
          jam();
          signal->theData[1] = all ? 0x8001 : 0x8002;
          report_mask.assign(c_definedNodes);
          report_mask.bitANDC(c_start.m_starting_nodes);
          retVal = 1;
          goto check_log;
        }
        case CheckNodeGroups::Partitioning: {
          jam();
          ndbrequire(result != CheckNodeGroups::Lose);
          signal->theData[1] =
              all ? 0x8001 : (result == CheckNodeGroups::Win ? 0x8002 : 0x8003);
          report_mask.assign(c_definedNodes);
          report_mask.bitANDC(c_start.m_starting_nodes);
          retVal = 1;
          goto check_log;
        }
        default: {
          ndbabort();
        }
      }
    }
    if (c_restartPartialTimeout == Uint32(~0) ||
        elapsed < c_restartPartialTimeout) {
      jam();

      signal->theData[1] = c_restartPartialTimeout == (Uint32)~0 ? 2 : 3;
      signal->theData[2] =
          c_restartPartialTimeout == Uint32(~0)
              ? Uint32(~0)
              : Uint32((c_restartPartialTimeout - elapsed + 500) / 1000);
      report_mask.assign(wait);
      retVal = 0;

      if (no_nodegroup_active && elapsed < c_restartNoNodegroupTimeout) {
        jam();
        signal->theData[1] = 7;
        signal->theData[2] =
            Uint32((c_restartNoNodegroupTimeout - elapsed + 500) / 1000);
      } else if (no_nodegroup_active &&
                 elapsed >= c_restartNoNodegroupTimeout) {
        jam();
        report_mask.bitANDC(c_start.m_no_nodegroup_nodes);
      }
      goto start_report;
    }

    /**
     * Start partial has passed...check for partitioning...
     */
    switch (result_w_log) {
      case CheckNodeGroups::Lose: {
        jam();
        goto missing_nodegroup;
      }
      case CheckNodeGroups::Partitioning: {
        jam();
        if (elapsed != Uint32(~0) && elapsed < partitionedTimeout &&
            result != CheckNodeGroups::Win) {
          jam();
          goto missinglog;
        }
      }
        [[fallthrough]];
      case CheckNodeGroups::Win: {
        jam();
        signal->theData[1] =
            all ? 0x8001 : (result == CheckNodeGroups::Win ? 0x8002 : 0x8003);
        report_mask.assign(c_definedNodes);
        report_mask.bitANDC(c_start.m_starting_nodes);
        retVal = 2;
        goto check_log;
      }
      default: {
        ndbabort();
      }
    }
  }

check_log:
  jam();
  {
    Uint32 save[1 + 1 * NdbNodeBitmask::Size];
    memcpy(save, signal->theData, sizeof(save));

    DihRestartReq *req = CAST_PTR(DihRestartReq, signal->getDataPtrSend());
    req->senderRef = 0;
    c_start.m_starting_nodes.copyto(NdbNodeBitmask::Size, req->nodemask);
    memcpy(req->node_gcis, c_start.m_node_gci, 4 * MAX_NDB_NODES);
    EXECUTE_DIRECT(DBDIH, GSN_DIH_RESTARTREQ, signal,
                   DihRestartReq::CheckLength);

    incompleteng = signal->theData[0];
    memcpy(signal->theData, save, sizeof(save));

    if (incompleteng != MAX_NDB_NODES) {
      jam();
      if (retVal == 1) {
        jam();
        goto incomplete_log;
      } else if (retVal == 2) {
        if (elapsed != Uint32(~0) && elapsed <= partitionedTimeout) {
          jam();
          goto missinglog;
        } else {
          goto incomplete_log;
        }
      }
      ndbabort();
    }
  }
  goto start_report;

missinglog:
  signal->theData[1] = c_restartPartitionedTimeout == Uint32(~0) ? 4 : 5;
  signal->theData[2] =
      partitionedTimeout == Uint32(~0)
          ? Uint32(~0)
          : Uint32((partitionedTimeout - elapsed + 500) / 1000);
  infoEvent("partitionedTimeout = %llu, elapsed = %llu", partitionedTimeout,
            elapsed);
  report_mask.assign(c_definedNodes);
  report_mask.bitANDC(c_start.m_starting_nodes);
  retVal = 0;
  goto start_report;

start_report:
  jam();
  {
    Uint32 sz = NdbNodeBitmask::Size;
    signal->theData[0] = NDB_LE_StartReport;
    signal->theData[3] = sz;
    Uint32 *ptr = signal->theData + 4;
    c_definedNodes.copyto(sz, ptr);
    ptr += sz;
    c_start.m_starting_nodes.copyto(sz, ptr);
    ptr += sz;
    c_start.m_skip_nodes.copyto(sz, ptr);
    ptr += sz;
    report_mask.copyto(sz, ptr);
    ptr += sz;
    c_start.m_no_nodegroup_nodes.copyto(sz, ptr);
    ptr += sz;
    LinearSectionPtr lsptr[3];
    lsptr[0].p = signal->theData;
    lsptr[0].sz = 4 + 5 * NdbNodeBitmask::Size;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB, lsptr, 1);
  }
  return retVal;

missing_nodegroup:
  jam();
  {
    const Uint32 extra = 100;
    char buf[2 * (NdbNodeBitmask::TextLength + 1) + extra];
    char mask1[NdbNodeBitmask::TextLength + 1];
    char mask2[NdbNodeBitmask::TextLength + 1];
    c_start.m_starting_nodes.getText(mask1);
    tmp.assign(c_start.m_starting_nodes);
    tmp.bitANDC(c_start.m_starting_nodes_w_log);
    tmp.getText(mask2);
    BaseString::snprintf(buf, sizeof(buf),
                         "Unable to start missing node group! "
                         " starting: %s (missing working fs for: %s)",
                         mask1, mask2);
    CRASH_INSERTION(944);
    progError(__LINE__, NDBD_EXIT_INSUFFICENT_NODES, buf);
    return 0;  // Deadcode
  }

incomplete_log:
  jam();
  {
    DihRestartReq *req = (DihRestartReq *)&signal->theData[0];
    for (Uint32 i = 0; i <= incompleteng; i++) {
      g_eventLogger->info("Node group GCI = %u for NG %u", req->node_gcis[i],
                          i);
    }
    for (Uint32 i = 1; i < MAX_NDB_NODES; i++) {
      if (c_start.m_node_gci[i] != 0) {
        g_eventLogger->info("Node GCI = %u for node %u", c_start.m_node_gci[i],
                            i);
      }
    }
    const Uint32 extra = 100;
    char buf[NdbNodeBitmask::TextLength + 1 + extra];
    char mask1[NdbNodeBitmask::TextLength + 1];
    c_start.m_starting_nodes.getText(mask1);
    BaseString::snprintf(buf, sizeof(buf),
                         "Incomplete log for node group: %d! "
                         " starting nodes: %s",
                         incompleteng, mask1);
    CRASH_INSERTION(944);
    progError(__LINE__, NDBD_EXIT_INSUFFICENT_NODES, buf);
    return 0;  // Deadcode
  }
}

void Qmgr::electionWon(Signal *signal) {
  NodeRecPtr myNodePtr;
  cpresident = getOwnNodeId(); /* This node becomes president. */
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  myNodePtr.p->phase = ZRUNNING;
  DEB_STARTUP(("phase(%u) = ZRUNNING", myNodePtr.i));
  DEB_MULTI_TRP(("Node %u in ZRUNNING, electionWon", myNodePtr.i));

  cpdistref = reference();
  cneighbourl = ZNIL;
  cneighbourh = ZNIL;
  myNodePtr.p->ndynamicId = 1 | (myNodePtr.p->hbOrder << 16);
  c_maxDynamicId = 1;
  c_clusterNodes.clear();
  c_clusterNodes.set(getOwnNodeId());

  cpresidentAlive = ZTRUE;
  NdbTick_Invalidate(&c_start_election_time);
  c_start.reset();

  signal->theData[0] = NDB_LE_CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = myNodePtr.p->ndynamicId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  c_start.m_starting_nodes.clear(getOwnNodeId());
  if (c_start.m_starting_nodes.isclear()) {
    jam();
    sendSttorryLab(signal, true);
  }
}

/*
4.4.11 CONTINUEB */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*--------------------------------------------------------------------------*/
/****************************>---------------------------------------------*/
/* CONTINUEB                 >        SENDER: Own block, Own node          */
/****************************>-------+INPUT : TCONTINUEB_TYPE              */
/*--------------------------------------------------------------*/
void Qmgr::regreqTimeLimitLab(Signal *signal) {
  if (cpresident == ZNIL) {
    if (c_start.m_president_candidate == ZNIL) {
      jam();
      c_start.m_president_candidate = getOwnNodeId();
    }

    cmInfoconf010Lab(signal);
  }
}  // Qmgr::regreqTimelimitLab()

/**---------------------------------------------------------------------------
 * The new node will take care of giving information about own node and ask
 * all other nodes for nodeinfo. The new node will use CM_NODEINFOREQ for
 * that purpose. When the setup of connections to all running, the president
 * will send a commit to all running nodes + the new node
 * INPUT: NODE_PTR1, must be set as ZNIL if we don't enter CONNECT_NODES)
 *                   from signal CM_NODEINFOCONF.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_NODEINFOCONF            */
/*******************************/
void Qmgr::execCM_NODEINFOCONF(Signal *signal) {
  DEBUG_START3(signal, "");

  jamEntry();

  CmNodeInfoConf *const conf = (CmNodeInfoConf *)signal->getDataPtr();

  const Uint32 nodeId = conf->nodeId;
  const Uint32 dynamicId = conf->dynamicId;
  const Uint32 version = conf->version;
  Uint32 mysql_version = conf->mysql_version;
  Uint32 lqh_workers = conf->lqh_workers;
  Uint32 query_threads = conf->query_threads;
  Uint32 log_parts = conf->log_parts;
  if (signal->length() == CmNodeInfoConf::OldSignalLength) {
    query_threads = 0;
    log_parts = lqh_workers;
  }

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  ndbrequire(nodePtr.p->phase == ZSTARTING);
  ndbrequire(c_start.m_gsn == GSN_CM_NODEINFOREQ);
  c_start.m_nodes.clearWaitingFor(nodeId);

  /**
   * Update node info
   */
  NodeRecPtr replyNodePtr;
  replyNodePtr.i = nodeId;
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->ndynamicId = dynamicId;
  replyNodePtr.p->blockRef = signal->getSendersBlockRef();
  setNodeInfo(replyNodePtr.i).m_version = version;
  setNodeInfo(replyNodePtr.i).m_mysql_version = mysql_version;
  setNodeInfo(replyNodePtr.i).m_lqh_workers = lqh_workers;
  setNodeInfo(replyNodePtr.i).m_query_threads = query_threads;
  setNodeInfo(replyNodePtr.i).m_log_parts = log_parts;

  recompute_version_info(NodeInfo::DB, version);

  if (!c_start.m_nodes.done()) {
    jam();
    return;
  }

  /**********************************************<*/
  /* Send an ack. back to the president.          */
  /* CM_ACKADD                                    */
  /* The new node has been registered by all      */
  /* running nodes and has stored nodeinfo about  */
  /* all running nodes. The new node has to wait  */
  /* for CM_ADD (commit) from president to become */
  /* a running node in the cluster.               */
  /**********************************************<*/
  sendCmAckAdd(signal, getOwnNodeId(), CmAdd::Prepare);
  return;
}  // Qmgr::execCM_NODEINFOCONF()

/**---------------------------------------------------------------------------
 * A new node sends nodeinfo about himself. The new node asks for
 * corresponding nodeinfo back in the  CM_NODEINFOCONF.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_NODEINFOREQ             */
/*******************************/
void Qmgr::execCM_NODEINFOREQ(Signal *signal) {
  jamEntry();

  const Uint32 Tblockref = signal->getSendersBlockRef();
  const Uint32 sig_len = signal->length();

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  if (nodePtr.p->phase != ZRUNNING) {
    jam();
    signal->theData[0] = reference();
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = ZNOT_RUNNING;
    sendSignal(Tblockref, GSN_CM_NODEINFOREF, signal, 3, JBB);
    return;
  }

  NodeRecPtr addNodePtr;
  CmNodeInfoReq *const req = (CmNodeInfoReq *)signal->getDataPtr();
  addNodePtr.i = req->nodeId;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  addNodePtr.p->ndynamicId = req->dynamicId;
  addNodePtr.p->blockRef = signal->getSendersBlockRef();
  setNodeInfo(addNodePtr.i).m_version = req->version;

  Uint32 mysql_version = req->mysql_version;
  setNodeInfo(addNodePtr.i).m_mysql_version = mysql_version;

  Uint32 lqh_workers = req->lqh_workers;
  setNodeInfo(addNodePtr.i).m_lqh_workers = lqh_workers;

  Uint32 query_threads = req->query_threads;
  Uint32 log_parts = req->log_parts;
  if (sig_len == CmNodeInfoReq::OldSignalLength) {
    query_threads = 0;
    log_parts = lqh_workers;
  }
  setNodeInfo(addNodePtr.i).m_query_threads = query_threads;
  setNodeInfo(addNodePtr.i).m_log_parts = log_parts;

  c_maxDynamicId = req->dynamicId & 0xFFFF;

  cmAddPrepare(signal, addNodePtr, nodePtr.p);
}  // Qmgr::execCM_NODEINFOREQ()

void Qmgr::cmAddPrepare(Signal *signal, NodeRecPtr nodePtr,
                        const NodeRec *self) {
  jam();

  switch (nodePtr.p->phase) {
    case ZINIT:
      jam();
      nodePtr.p->phase = ZSTARTING;
      DEB_STARTUP(("2:phase(%u) = ZSTARTING", nodePtr.i));
      return;
    case ZFAIL_CLOSING:
      jam();

#if 1
      warningEvent(
          "Received request to incorporate node %u, "
          "while error handling has not yet completed",
          nodePtr.i);

      ndbrequire(getOwnNodeId() != cpresident);
      ndbrequire(signal->header.theVerId_signalNumber == GSN_CM_ADD);
      c_start.m_nodes.clearWaitingFor();
      c_start.m_nodes.setWaitingFor(nodePtr.i);
      c_start.m_gsn = GSN_CM_NODEINFOCONF;
#else
      warningEvent("Enabling communication to CM_ADD node %u state=%d",
                   nodePtr.i, nodePtr.p->phase);
      nodePtr.p->phase = ZSTARTING;
      nodePtr.p->failState = NORMAL;
      signal->theData[0] = 0;
      signal->theData[1] = nodePtr.i;
      sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 2, JBB);
#endif
      return;
    case ZSTARTING:
      break;
    case ZRUNNING:
      ndbabort();
    case ZPREPARE_FAIL:
      ndbabort();
    case ZAPI_ACTIVATION_ONGOING:
      ndbabort();
    case ZAPI_ACTIVE:
      ndbabort();
    case ZAPI_INACTIVE:
      ndbabort();
  }

  sendCmAckAdd(signal, nodePtr.i, CmAdd::Prepare);
  sendApiVersionRep(signal, nodePtr);

  /* President have prepared us */
  CmNodeInfoConf *conf = (CmNodeInfoConf *)signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  conf->dynamicId = self->ndynamicId;
  conf->version = getNodeInfo(getOwnNodeId()).m_version;
  conf->mysql_version = getNodeInfo(getOwnNodeId()).m_mysql_version;
  conf->lqh_workers = getNodeInfo(getOwnNodeId()).m_lqh_workers;
  conf->query_threads = getNodeInfo(getOwnNodeId()).m_query_threads;
  conf->log_parts = getNodeInfo(getOwnNodeId()).m_log_parts;
  sendSignal(nodePtr.p->blockRef, GSN_CM_NODEINFOCONF, signal,
             CmNodeInfoConf::SignalLength, JBB);
  DEBUG_START(GSN_CM_NODEINFOCONF, refToNode(nodePtr.p->blockRef), "");
}

void Qmgr::sendApiVersionRep(Signal *signal, NodeRecPtr nodePtr) {
  {
    jam();
    Uint32 ref = calcQmgrBlockRef(nodePtr.i);
    for (Uint32 i = 1; i < MAX_NODES; i++) {
      jam();
      Uint32 version = getNodeInfo(i).m_version;
      Uint32 type = getNodeInfo(i).m_type;
      if (type != NodeInfo::DB && version) {
        jam();
        signal->theData[0] = i;
        signal->theData[1] = version;
        sendSignal(ref, GSN_NODE_VERSION_REP, signal, 2, JBB);
      }
    }
  }
}

void Qmgr::sendCmAckAdd(Signal *signal, Uint32 nodeId,
                        CmAdd::RequestType type) {
  CmAckAdd *cmAckAdd = (CmAckAdd *)signal->getDataPtrSend();
  cmAckAdd->requestType = type;
  cmAckAdd->startingNodeId = nodeId;
  cmAckAdd->senderNodeId = getOwnNodeId();
  sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
  DEBUG_START(GSN_CM_ACKADD, cpresident, "");

  switch (type) {
    case CmAdd::Prepare:
      return;
    case CmAdd::AddCommit:
    case CmAdd::CommitNew:
      break;
  }

  signal->theData[0] = nodeId;
  EXECUTE_DIRECT(NDBCNTR, GSN_CM_ADD_REP, signal, 1);
  jamEntry();
}

/*
4.4.11 CM_ADD */
/**--------------------------------------------------------------------------
 * Prepare a running node to add a new node to the cluster. The running node
 * will change phase of the new node from ZINIT to ZWAITING. The running node
 * will also mark that we have received a prepare. When the new node has sent
 * us nodeinfo we can send an acknowledgement back to the president. When all
 * running nodes has acknowledged the new node, the president will send a
 * commit and we can change phase of the new node to ZRUNNING. The president
 * will also send CM_ADD to himself.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_ADD                     */
/*******************************/
void Qmgr::execCM_ADD(Signal *signal) {
  NodeRecPtr addNodePtr;
  jamEntry();

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  CRASH_INSERTION(940);

  CmAdd *const cmAdd = (CmAdd *)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAdd->requestType;
  addNodePtr.i = cmAdd->startingNodeId;
  // const Uint32 startingVersion = cmAdd->startingVersion;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);

  DEBUG_START3(signal, type);

  if (nodePtr.p->phase == ZSTARTING) {
    jam();
    /**
     * We are joining...
     */
    ndbrequire(addNodePtr.i == nodePtr.i);
    switch (type) {
      case CmAdd::Prepare:
        ndbrequire(c_start.m_gsn == GSN_CM_NODEINFOREQ);
        /**
         * Wait for CM_NODEINFO_CONF
         */
        return;
      case CmAdd::CommitNew:
        /**
         * Tata. we're in the cluster
         */
        joinedCluster(signal, addNodePtr);
        return;
      case CmAdd::AddCommit:
        ndbabort();
    }
  }

  switch (type) {
    case CmAdd::Prepare:
      cmAddPrepare(signal, addNodePtr, nodePtr.p);
      break;
    case CmAdd::AddCommit: {
      jam();
      ndbrequire(addNodePtr.p->phase == ZSTARTING);
      addNodePtr.p->phase = ZRUNNING;
      DEB_STARTUP(("2:phase(%u) = ZRUNNING", addNodePtr.i));
      DEB_MULTI_TRP(("Node %u in ZRUNNING, AddCommit", addNodePtr.i));
      m_connectivity_check.reportNodeConnect(addNodePtr.i);
      set_hb_count(addNodePtr.i) = 0;
      c_clusterNodes.set(addNodePtr.i);
      findNeighbours(signal, __LINE__);

      /**
       * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK THAT WE MISS EARLY
       * HEARTBEATS.
       */
      sendHeartbeat(signal);
      hb_send_timer.reset(NdbTick_getCurrentTicks());

      /**
       *  ENABLE COMMUNICATION WITH ALL BLOCKS WITH THE NEWLY ADDED NODE
       */
      EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
      enableComReq->m_senderRef = reference();
      enableComReq->m_senderData = ENABLE_COM_CM_ADD_COMMIT;
      enableComReq->m_enableNodeId = addNodePtr.i;
      sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
                 EnableComReq::SignalLength, JBB);
      break;
    }
    case CmAdd::CommitNew:
      jam();
      ndbabort();
  }

}  // Qmgr::execCM_ADD()

void Qmgr::handleEnableComAddCommit(Signal *signal, Uint32 node) {
  sendCmAckAdd(signal, node, CmAdd::AddCommit);
  if (getOwnNodeId() != cpresident) {
    jam();
    c_start.reset();
  }
}

void Qmgr::execENABLE_COMCONF(Signal *signal) {
  const EnableComConf *enableComConf =
      (const EnableComConf *)signal->getDataPtr();
  Uint32 state = enableComConf->m_senderData;
  Uint32 node = enableComConf->m_enableNodeId;

  jamEntry();

  switch (state) {
    case ENABLE_COM_CM_ADD_COMMIT:
      jam();
      /* Only exactly one node possible here. */
      handleEnableComAddCommit(signal, node);
      break;

    case ENABLE_COM_CM_COMMIT_NEW:
      jam();
      handleEnableComCommitNew(signal);
      break;

    case ENABLE_COM_API_REGREQ:
      jam();
      /* Only exactly one node possible here. */
      handleEnableComApiRegreq(signal, node);
      break;

    default:
      jam();
      ndbabort();
  }
}

void Qmgr::joinedCluster(Signal *signal, NodeRecPtr nodePtr) {
  /**
   * WE HAVE BEEN INCLUDED IN THE CLUSTER WE CAN START BEING PART OF THE
   * HEARTBEAT PROTOCOL AND WE WILL ALSO ENABLE COMMUNICATION WITH ALL
   * NODES IN THE CLUSTER.
   */
  DEB_MULTI_TRP(("Node %u in ZRUNNING, AddCommit", nodePtr.i));
  nodePtr.p->phase = ZRUNNING;
  DEB_STARTUP(("3:phase(%u) = ZRUNNING", nodePtr.i));
  set_hb_count(nodePtr.i) = 0;
  findNeighbours(signal, __LINE__);
  c_clusterNodes.set(nodePtr.i);
  c_start.reset();

  /**
   * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK
   * THAT WE MISS EARLY HEARTBEATS.
   */
  sendHeartbeat(signal);
  hb_send_timer.reset(NdbTick_getCurrentTicks());

  /**
   * ENABLE COMMUNICATION WITH ALL BLOCKS IN THE CURRENT CLUSTER AND SET
   * THE NODES IN THE CLUSTER TO BE RUNNING.
   */
  EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
  enableComReq->m_senderRef = reference();
  enableComReq->m_senderData = ENABLE_COM_CM_COMMIT_NEW;
  enableComReq->m_enableNodeId = 0;
  enableComReq->m_nodeIds.clear();
  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if ((nodePtr.p->phase == ZRUNNING) && (nodePtr.i != getOwnNodeId())) {
      /*-------------------------------------------------------------------*/
      // Enable full communication to all other nodes. Not really necessary
      // to open communication to ourself.
      /*-------------------------------------------------------------------*/
      jamLine(nodePtr.i);
      enableComReq->m_nodeIds.set(nodePtr.i);
    }  // if
  }    // for

  if (!enableComReq->m_nodeIds.isclear()) {
    jam();
    LinearSectionPtr lsptr[3];
    lsptr[0].p = enableComReq->m_nodeIds.rep.data;
    lsptr[0].sz = enableComReq->m_nodeIds.getPackedLengthInWords();
    sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
               EnableComReq::SignalLength, JBB, lsptr, 1);
  } else {
    handleEnableComCommitNew(signal);
  }
}

void Qmgr::handleEnableComCommitNew(Signal *signal) {
  sendSttorryLab(signal, true);

  sendCmAckAdd(signal, getOwnNodeId(), CmAdd::CommitNew);
}

/*  4.10.7 CM_ACKADD        - PRESIDENT IS RECEIVER -       */
/*---------------------------------------------------------------------------*/
/* Entry point for an ack add signal.
 * The TTYPE defines if it is a prepare or a commit.                         */
/*---------------------------------------------------------------------------*/
void Qmgr::execCM_ACKADD(Signal *signal) {
  NodeRecPtr addNodePtr;
  NodeRecPtr senderNodePtr;
  jamEntry();

  CmAckAdd *const cmAckAdd = (CmAckAdd *)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAckAdd->requestType;
  addNodePtr.i = cmAckAdd->startingNodeId;
  senderNodePtr.i = cmAckAdd->senderNodeId;

  DEBUG_START3(signal, type);

  if (cpresident != getOwnNodeId()) {
    jam();
    /*-----------------------------------------------------------------------*/
    /* IF WE ARE NOT PRESIDENT THEN WE SHOULD NOT RECEIVE THIS MESSAGE.      */
    /*------------------------------------------------------------_----------*/
    warningEvent("Received CM_ACKADD from %d president=%d", senderNodePtr.i,
                 cpresident);
    return;
  }  // if

  if (addNodePtr.i != c_start.m_startNode) {
    jam();
    /*----------------------------------------------------------------------*/
    /* THIS IS NOT THE STARTING NODE. WE ARE ACTIVE NOW WITH ANOTHER START. */
    /*----------------------------------------------------------------------*/
    warningEvent("Received CM_ACKADD from %d with startNode=%d != own %d",
                 senderNodePtr.i, addNodePtr.i, c_start.m_startNode);
    return;
  }  // if

  ndbrequire(c_start.m_gsn == GSN_CM_ADD);
  c_start.m_nodes.clearWaitingFor(senderNodePtr.i);
  if (!c_start.m_nodes.done()) {
    jam();
    return;
  }

  switch (type) {
    case CmAdd::Prepare: {
      jam();

      /*----------------------------------------------------------------------*/
      /* ALL RUNNING NODES HAVE PREPARED THE INCLUSION OF THIS NEW NODE.      */
      /*----------------------------------------------------------------------*/
      c_start.m_gsn = GSN_CM_ADD;
      c_start.m_nodes = c_clusterNodes;

      CmAdd *const cmAdd = (CmAdd *)signal->getDataPtrSend();
      cmAdd->requestType = CmAdd::AddCommit;
      cmAdd->startingNodeId = addNodePtr.i;
      cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
      cmAdd->startingMysqlVersion = getNodeInfo(addNodePtr.i).m_mysql_version;
      NodeReceiverGroup rg(QMGR, c_clusterNodes);
      sendSignal(rg, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
      DEBUG_START2(GSN_CM_ADD, rg, "AddCommit");
      return;
    }
    case CmAdd::AddCommit: {
      jam();

      /****************************************/
      /* Send commit to the new node so he    */
      /* will change PHASE into ZRUNNING      */
      /****************************************/
      c_start.m_gsn = GSN_CM_ADD;
      c_start.m_nodes.clearWaitingFor();
      c_start.m_nodes.setWaitingFor(addNodePtr.i);

      CmAdd *const cmAdd = (CmAdd *)signal->getDataPtrSend();
      cmAdd->requestType = CmAdd::CommitNew;
      cmAdd->startingNodeId = addNodePtr.i;
      cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
      cmAdd->startingMysqlVersion = getNodeInfo(addNodePtr.i).m_mysql_version;
      sendSignal(calcQmgrBlockRef(addNodePtr.i), GSN_CM_ADD, signal,
                 CmAdd::SignalLength, JBA);
      DEBUG_START(GSN_CM_ADD, addNodePtr.i, "CommitNew");
      /**
       * Report to DBDIH that a node have been added to the nodes included
       * in the heartbeat protocol.
       */
      InclNodeHBProtocolRep *rep =
          (InclNodeHBProtocolRep *)signal->getDataPtrSend();
      rep->nodeId = addNodePtr.i;
      EXECUTE_DIRECT(DBDIH, GSN_INCL_NODE_HB_PROTOCOL_REP, signal,
                     InclNodeHBProtocolRep::SignalLength);
      return;
    }
    case CmAdd::CommitNew:
      jam();
      /**
       * Tell arbitration about new node.
       */
      handleArbitNdbAdd(signal, addNodePtr.i);
      c_start.reset();

      if (c_start.m_starting_nodes.get(addNodePtr.i)) {
        jam();
        c_start.m_starting_nodes.clear(addNodePtr.i);
        if (c_start.m_starting_nodes.isclear()) {
          jam();
          sendSttorryLab(signal, true);
        }
      }
      return;
  }  // switch
  ndbabort();
}  // Qmgr::execCM_ACKADD()

/**-------------------------------------------------------------------------
 * WE HAVE BEEN INCLUDED INTO THE CLUSTER. IT IS NOW TIME TO CALCULATE WHICH
 * ARE OUR LEFT AND RIGHT NEIGHBOURS FOR THE HEARTBEAT PROTOCOL.
 *--------------------------------------------------------------------------*/
void Qmgr::findNeighbours(Signal *signal, Uint32 from) {
  UintR toldLeftNeighbour;
  UintR tfnLeftFound;
  UintR tfnMaxFound;
  UintR tfnMinFound;
  UintR tfnRightFound;
  NodeRecPtr fnNodePtr;
  NodeRecPtr fnOwnNodePtr;

  Uint32 toldRightNeighbour = cneighbourh;
  toldLeftNeighbour = cneighbourl;
  tfnLeftFound = 0;
  tfnMaxFound = 0;
  tfnMinFound = (UintR)-1;
  tfnRightFound = (UintR)-1;
  fnOwnNodePtr.i = getOwnNodeId();
  ptrCheckGuard(fnOwnNodePtr, MAX_NDB_NODES, nodeRec);
  for (fnNodePtr.i = 1; fnNodePtr.i < MAX_NDB_NODES; fnNodePtr.i++) {
    ptrAss(fnNodePtr, nodeRec);
    if (fnNodePtr.i != fnOwnNodePtr.i) {
      jamLine(fnNodePtr.i);
      if (fnNodePtr.p->phase == ZRUNNING) {
        if (tfnMinFound > fnNodePtr.p->ndynamicId) {
          jam();
          tfnMinFound = fnNodePtr.p->ndynamicId;
        }  // if
        if (tfnMaxFound < fnNodePtr.p->ndynamicId) {
          jam();
          tfnMaxFound = fnNodePtr.p->ndynamicId;
        }  // if
        if (fnOwnNodePtr.p->ndynamicId > fnNodePtr.p->ndynamicId) {
          jam();
          if (fnNodePtr.p->ndynamicId > tfnLeftFound) {
            jam();
            tfnLeftFound = fnNodePtr.p->ndynamicId;
          }  // if
        } else {
          jam();
          if (fnNodePtr.p->ndynamicId < tfnRightFound) {
            jam();
            tfnRightFound = fnNodePtr.p->ndynamicId;
          }  // if
        }    // if
      }      // if
    }        // if
  }          // for
  if (tfnLeftFound == 0) {
    if (tfnMinFound == (UintR)-1) {
      jam();
      cneighbourl = ZNIL;
    } else {
      jam();
      cneighbourl = translateDynamicIdToNodeId(signal, tfnMaxFound);
    }  // if
  } else {
    jam();
    cneighbourl = translateDynamicIdToNodeId(signal, tfnLeftFound);
  }  // if
  if (tfnRightFound == (UintR)-1) {
    if (tfnMaxFound == 0) {
      jam();
      cneighbourh = ZNIL;
    } else {
      jam();
      cneighbourh = translateDynamicIdToNodeId(signal, tfnMinFound);
    }  // if
  } else {
    jam();
    cneighbourh = translateDynamicIdToNodeId(signal, tfnRightFound);
  }  // if
  if (toldLeftNeighbour != cneighbourl) {
    jam();
    if (cneighbourl != ZNIL) {
      jam();
      /**-------------------------------------------------------------------*/
      /* WE ARE SUPERVISING A NEW LEFT NEIGHBOUR. WE START WITH ALARM COUNT
       * EQUAL TO ZERO.
       *---------------------------------------------------------------------*/
      fnNodePtr.i = cneighbourl;
      ptrCheckGuard(fnNodePtr, MAX_NDB_NODES, nodeRec);
      set_hb_count(fnNodePtr.i) = 0;
    }  // if
  }    // if

  signal->theData[0] = NDB_LE_FIND_NEIGHBOURS;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cneighbourl;
  signal->theData[3] = cneighbourh;
  signal->theData[4] = fnOwnNodePtr.p->ndynamicId;
  UintR Tlen = 5;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, Tlen, JBB);
  g_eventLogger->info(
      "findNeighbours from: %u old (left: %u right: %u) new (%u %u)", from,
      toldLeftNeighbour, toldRightNeighbour, cneighbourl, cneighbourh);
}  // Qmgr::findNeighbours()

/*
4.10.7 INIT_DATA        */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void Qmgr::initData(Signal *signal) {
  // catch-all for missing initializations
  arbitRec = ArbitRec();

  /**
   * Timeouts
   */
  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 hbDBDB = 1500;
  Uint32 arbitTimeout = 1000;
  Uint32 arbitMethod = ARBIT_METHOD_DEFAULT;
  Uint32 ccInterval = 0;
  c_restartPartialTimeout = 30000;
  c_restartPartitionedTimeout = Uint32(~0);
  c_restartFailureTimeout = Uint32(~0);
  c_restartNoNodegroupTimeout = 15000;
  ndb_mgm_get_int_parameter(p, CFG_DB_HEARTBEAT_INTERVAL, &hbDBDB);
  ndb_mgm_get_int_parameter(p, CFG_DB_ARBIT_TIMEOUT, &arbitTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_ARBIT_METHOD, &arbitMethod);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTIAL_TIMEOUT,
                            &c_restartPartialTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTITION_TIMEOUT,
                            &c_restartPartitionedTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_NO_NODEGROUP_TIMEOUT,
                            &c_restartNoNodegroupTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT,
                            &c_restartFailureTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_CONNECT_CHECK_DELAY, &ccInterval);

  if (c_restartPartialTimeout == 0) {
    c_restartPartialTimeout = Uint32(~0);
  }

  if (c_restartPartitionedTimeout == 0) {
    c_restartPartitionedTimeout = Uint32(~0);
  }

  if (c_restartFailureTimeout == 0) {
    c_restartFailureTimeout = Uint32(~0);
  }

  if (c_restartNoNodegroupTimeout == 0) {
    c_restartNoNodegroupTimeout = Uint32(~0);
  }

  setHbDelay(hbDBDB);
  setCCDelay(ccInterval);
  setArbitTimeout(arbitTimeout);

  arbitRec.method = (ArbitRec::Method)arbitMethod;
  arbitRec.state = ARBIT_NULL;  // start state for all nodes
  DEB_ARBIT(("Arbit state = ARBIT_INIT init"));
  arbitRec.apiMask[0].clear();  // prepare for ARBIT_CFG

  Uint32 sum = 0;
  ArbitSignalData *const sd = (ArbitSignalData *)&signal->theData[0];
  for (unsigned rank = 1; rank <= 2; rank++) {
    sd->sender = getOwnNodeId();
    sd->code = rank;
    sd->node = 0;
    sd->ticket.clear();
    sd->mask.clear();
    ndb_mgm_configuration_iterator *iter =
        m_ctx.m_config.getClusterConfigIterator();
    for (ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)) {
      Uint32 tmp = 0;
      if (ndb_mgm_get_int_parameter(iter, CFG_NODE_ARBIT_RANK, &tmp) == 0 &&
          tmp == rank) {
        Uint32 nodeId = 0;
        ndbrequire(!ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &nodeId));
        sd->mask.set(nodeId);
      }
    }
    sum += sd->mask.count();
    execARBIT_CFG(signal);
  }

  if (arbitRec.method == ArbitRec::METHOD_DEFAULT && sum == 0) {
    jam();
    infoEvent("Arbitration disabled, all API nodes have rank 0");
    arbitRec.method = ArbitRec::DISABLED;
  }

  setNodeInfo(getOwnNodeId()).m_version = NDB_VERSION;
  setNodeInfo(getOwnNodeId()).m_mysql_version = NDB_MYSQL_VERSION_D;

  ndb_mgm_configuration_iterator *iter =
      m_ctx.m_config.getClusterConfigIterator();
  for (ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)) {
    jam();
    Uint32 nodeId = 0;
    if (ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &nodeId) == 0) {
      jam();
      if (nodeId < MAX_NDB_NODES &&
          getNodeInfo(nodeId).m_type == NodeInfo::DB) {
        Uint32 hbOrder = 0;
        ndb_mgm_get_int_parameter(iter, CFG_DB_HB_ORDER, &hbOrder);

        NodeRecPtr nodePtr;
        nodePtr.i = nodeId;
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        nodePtr.p->hbOrder = hbOrder;
      }
    }
  }
  int hb_order_error = check_hb_order_config();
  if (hb_order_error == -1) {
    char msg[] =
        "Illegal HeartbeatOrder config, "
        "all nodes must have non-zero config value";
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, msg);
    return;
  }
  if (hb_order_error == -2) {
    char msg[] =
        "Illegal HeartbeatOrder config, "
        "the nodes must have distinct config values";
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, msg);
    return;
  }
  ndbrequire(hb_order_error == 0);
}  // Qmgr::initData()

/**---------------------------------------------------------------------------
 * HERE WE RECEIVE THE JOB TABLE SIGNAL EVERY 10 MILLISECONDS.
 * WE WILL USE THIS TO CHECK IF IT IS TIME TO CHECK THE NEIGHBOUR NODE.
 * WE WILL ALSO SEND A SIGNAL TO BLOCKS THAT NEED A TIME SIGNAL AND
 * DO NOT WANT TO USE JOB TABLE SIGNALS.
 *---------------------------------------------------------------------------*/
void Qmgr::timerHandlingLab(Signal *signal) {
  const NDB_TICKS TcurrentTime = NdbTick_getCurrentTicks();
  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  const Uint32 sentHi = signal->theData[1];
  const Uint32 sentLo = signal->theData[2];
  const NDB_TICKS sent((Uint64(sentHi) << 32) | sentLo);
  bool send_hb_always = false;

  if (NdbTick_Compare(sent, TcurrentTime) > 0) {
    jam();
    const Uint64 backwards = NdbTick_Elapsed(TcurrentTime, sent).milliSec();
    if (backwards > 0)  // Ignore sub millisecond backticks
    {
      g_eventLogger->warning(
          "timerHandlingLab, clock ticked backwards: %llu (ms)", backwards);
      send_hb_always = true;
    }
  } else {
    const Uint64 elapsed = NdbTick_Elapsed(sent, TcurrentTime).milliSec();
    if (elapsed >= 150) {
      struct ndb_rusage curr_rusage;
      jam();
      send_hb_always = true;
      bool rusage_worked = true;
      Uint64 exec_time = 0;
      Uint64 sys_time = 0;
      Ndb_GetRUsage(&curr_rusage, false);
      if ((curr_rusage.ru_utime == 0 && curr_rusage.ru_stime == 0) ||
          (m_timer_handling_rusage.ru_utime == 0 &&
           m_timer_handling_rusage.ru_stime == 0)) {
        jam();
        rusage_worked = false;
      }
      if (rusage_worked) {
        exec_time = curr_rusage.ru_utime - m_timer_handling_rusage.ru_utime;
        sys_time = curr_rusage.ru_stime - m_timer_handling_rusage.ru_stime;
      }

      if (elapsed >= 1000) {
        if (rusage_worked) {
          g_eventLogger->warning(
              "timerHandlingLab, expected 10ms sleep"
              ", not scheduled for: %d (ms), "
              "exec_time %llu us, sys_time %llu us",
              int(elapsed), exec_time, sys_time);
        } else {
          g_eventLogger->warning(
              "timerHandlingLab, expected 10ms sleep"
              ", not scheduled for: %d (ms)",
              int(elapsed));
        }
      } else {
        if (rusage_worked) {
          g_eventLogger->info(
              "timerHandlingLab, expected 10ms sleep"
              ", not scheduled for: %d (ms), "
              "exec_time %llu us, sys_time %llu us",
              int(elapsed), exec_time, sys_time);
        } else {
          g_eventLogger->info(
              "timerHandlingLab, expected 10ms sleep"
              ", not scheduled for: %d (ms)",
              int(elapsed));
        }
      }
    }
  }

  if (myNodePtr.p->phase == ZRUNNING) {
    jam();
    /**---------------------------------------------------------------------
     * WE ARE ONLY PART OF HEARTBEAT CLUSTER IF WE ARE UP AND RUNNING.
     *---------------------------------------------------------------------*/
    if (hb_send_timer.check(TcurrentTime) || send_hb_always) {
      /**
       * We send heartbeats once per heartbeat interval and 4 missed heartbeat
       * intervals will cause a failure. If QMGR is not so responsive we're
       * having some sort of overload issue. In this case we will always take
       * the chance to send heartbeats immediately to avoid risking heartbeat
       * failures (send_hb_always == true).
       *
       * Delaying checks of heartbeat timers is much less of a problem.
       */
      jam();
      sendHeartbeat(signal);
      hb_send_timer.reset(TcurrentTime);
    }
    if (likely(!m_connectivity_check.m_active)) {
      if (hb_check_timer.check(TcurrentTime)) {
        jam();
        checkHeartbeat(signal);
        hb_check_timer.reset(TcurrentTime);
      }
    } else {
      /* Connectivity check */
      if (m_connectivity_check.m_timer.check(TcurrentTime)) {
        jam();
        checkConnectivityTimeSignal(signal);
        m_connectivity_check.m_timer.reset(TcurrentTime);
      }
    }
  }

  if (interface_check_timer.check(TcurrentTime)) {
    jam();
    interface_check_timer.reset(TcurrentTime);
    checkStartInterface(signal, TcurrentTime);
  }

  if (hb_api_timer.check(TcurrentTime)) {
    jam();
    hb_api_timer.reset(TcurrentTime);
    apiHbHandlingLab(signal, TcurrentTime);
  }

  if (ka_send_timer.getDelay() > 0 && ka_send_timer.check(TcurrentTime)) {
    jam();
    ka_send_timer.reset(TcurrentTime);
    send_trp_keep_alive_start(signal);
  }

  Ndb_GetRUsage(&m_timer_handling_rusage, false);

  //--------------------------------------------------
  // Resend this signal with 10 milliseconds delay.
  //--------------------------------------------------
  signal->theData[0] = ZTIMER_HANDLING;
  signal->theData[1] = Uint32(TcurrentTime.getUint64() >> 32);
  signal->theData[2] = Uint32(TcurrentTime.getUint64());
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 10, 3);
  return;
}  // Qmgr::timerHandlingLab()

/*---------------------------------------------------------------------------*/
/*       THIS MODULE HANDLES THE SENDING AND RECEIVING OF HEARTBEATS.        */
/*---------------------------------------------------------------------------*/
void Qmgr::sendHeartbeat(Signal *signal) {
  NodeRecPtr localNodePtr;
  localNodePtr.i = cneighbourh;
  if (localNodePtr.i == ZNIL) {
    jam();
    /**---------------------------------------------------------------------
     * THERE ARE NO NEIGHBOURS. THIS IS POSSIBLE IF WE ARE THE ONLY NODE IN
     * THE CLUSTER.IN THIS CASE WE DO NOT NEED TO SEND ANY HEARTBEAT SIGNALS.
     *-----------------------------------------------------------------------*/
    return;
  }  // if

  if (ERROR_INSERTED(946)) {
    NdbSleep_SecSleep(180);
    return;
  }

  ptrCheckGuard(localNodePtr, MAX_NDB_NODES, nodeRec);
  signal->theData[0] = getOwnNodeId();

  sendSignal(localNodePtr.p->blockRef, GSN_CM_HEARTBEAT, signal, 1, JBA);
#ifdef VM_TRACE
  signal->theData[0] = NDB_LE_SentHeartbeat;
  signal->theData[1] = localNodePtr.i;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
#endif
}  // Qmgr::sendHeartbeat()

void Qmgr::checkHeartbeat(Signal *signal) {
  NodeRecPtr nodePtr;

  nodePtr.i = cneighbourl;
  if (nodePtr.i == ZNIL) {
    jam();
    /**---------------------------------------------------------------------
     * THERE ARE NO NEIGHBOURS. THIS IS POSSIBLE IF WE ARE THE ONLY NODE IN
     * THE CLUSTER. IN THIS CASE WE DO NOT NEED TO CHECK ANY HEARTBEATS.
     *-----------------------------------------------------------------------*/
    return;
  }  // if
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  set_hb_count(nodePtr.i)++;
  ndbrequire(nodePtr.p->phase == ZRUNNING);
  ndbrequire(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB);

  if (get_hb_count(nodePtr.i) > 2) {
    signal->theData[0] = NDB_LE_MissedHeartbeat;
    signal->theData[1] = nodePtr.i;
    signal->theData[2] = get_hb_count(nodePtr.i) - 1;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }

  if (get_hb_count(nodePtr.i) > 4) {
    jam();
    if (m_connectivity_check.getEnabled()) {
      jam();
      /* Start connectivity check, indicating the cause */
      startConnectivityCheck(signal, FailRep::ZHEARTBEAT_FAILURE, nodePtr.i);
      return;
    } else {
      /**----------------------------------------------------------------------
       * OUR LEFT NEIGHBOUR HAVE KEPT QUIET FOR THREE CONSECUTIVE HEARTBEAT
       * PERIODS. THUS WE DECLARE HIM DOWN.
       *----------------------------------------------------------------------*/
      signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
      signal->theData[1] = nodePtr.i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

      failReportLab(signal, nodePtr.i, FailRep::ZHEARTBEAT_FAILURE,
                    getOwnNodeId());
      return;
    }
  }  // if
}  // Qmgr::checkHeartbeat()

void Qmgr::apiHbHandlingLab(Signal *signal, NDB_TICKS now) {
  NodeRecPtr TnodePtr;

  jam();
  for (TnodePtr.i = 1; TnodePtr.i < MAX_NODES; TnodePtr.i++) {
    const Uint32 nodeId = TnodePtr.i;
    ptrAss(TnodePtr, nodeRec);

    const NodeInfo::NodeType type = getNodeInfo(nodeId).getType();
    if (type == NodeInfo::DB) continue;

    if (type == NodeInfo::INVALID) continue;

    if (c_connectedNodes.get(nodeId)) {
      jamLine(nodeId);
      set_hb_count(TnodePtr.i)++;

      if (get_hb_count(TnodePtr.i) > 2) {
        signal->theData[0] = NDB_LE_MissedHeartbeat;
        signal->theData[1] = nodeId;
        signal->theData[2] = get_hb_count(TnodePtr.i) - 1;
        sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
      }

      if (get_hb_count(TnodePtr.i) > 4) {
        jam();
        /*------------------------------------------------------------------*/
        /* THE API NODE HAS NOT SENT ANY HEARTBEAT FOR THREE SECONDS.
         * WE WILL DISCONNECT FROM IT NOW.
         *------------------------------------------------------------------*/
        /*------------------------------------------------------------------*/
        /* We call node_failed to release all connections for this api node */
        /*------------------------------------------------------------------*/
        signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
        signal->theData[1] = nodeId;
        sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

        api_failed(signal, nodeId);
      }  // if
    }    // if
    else if (TnodePtr.p->phase == ZAPI_INACTIVE && TnodePtr.p->m_secret != 0 &&
             NdbTick_Compare(now, TnodePtr.p->m_alloc_timeout) > 0) {
      jam();
      TnodePtr.p->m_secret = 0;
      warningEvent("Releasing node id allocation for node %u", TnodePtr.i);
    }
  }  // for
  return;
}  // Qmgr::apiHbHandlingLab()

void Qmgr::checkStartInterface(Signal *signal, NDB_TICKS now) {
  NodeRecPtr nodePtr;
  /*------------------------------------------------------------------------*/
  // This method is called once per second. After a disconnect we wait at
  // least three seconds before allowing new connects. We will also ensure
  // that handling of the failure is completed before we allow new connections.
  /*------------------------------------------------------------------------*/
  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    Uint32 type = getNodeInfo(nodePtr.i).m_type;
    if (nodePtr.p->phase == ZFAIL_CLOSING) {
      jamLine(nodePtr.i);
      set_hb_count(nodePtr.i)++;
      if (c_connectedNodes.get(nodePtr.i)) {
        jam();
        /*-------------------------------------------------------------------*/
        // We need to ensure that the connection is not restored until it has
        // been disconnected for at least three seconds.
        /*-------------------------------------------------------------------*/
        set_hb_count(nodePtr.i) = 0;
      }  // if
      if ((get_hb_count(nodePtr.i) > 3) && (nodePtr.p->failState == NORMAL)) {
        /**------------------------------------------------------------------
         * WE HAVE DISCONNECTED THREE SECONDS AGO. WE ARE NOW READY TO
         * CONNECT AGAIN AND ACCEPT NEW REGISTRATIONS FROM THIS NODE.
         * WE WILL NOT ALLOW CONNECTIONS OF API NODES UNTIL API FAIL HANDLING
         * IS COMPLETE.
         *-------------------------------------------------------------------*/
        nodePtr.p->failState = NORMAL;
        nodePtr.p->m_secret = 0;
        switch (type) {
          case NodeInfo::DB:
            jam();
            nodePtr.p->phase = ZINIT;
            DEB_STARTUP(("2:phase(%u) = ZINIT", nodePtr.i));
            break;
          case NodeInfo::MGM:
            jam();
            nodePtr.p->phase = ZAPI_INACTIVE;
            break;
          case NodeInfo::API:
            jam();
            if (c_allow_api_connect) {
              jam();
              nodePtr.p->phase = ZAPI_INACTIVE;
              break;
            } else {
              /**
               * Dont allow API node to connect before c_allow_api_connect
               */
              jam();
              set_hb_count(nodePtr.i) = 3;
              continue;
            }
        }

        set_hb_count(nodePtr.i) = 0;
        signal->theData[0] = 0;
        signal->theData[1] = nodePtr.i;
        sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 2, JBB);
      } else {
        jam();
        if (((get_hb_count(nodePtr.i) + 1) % 30) == 0) {
          jam();
          char buf[256];
          if (getNodeInfo(nodePtr.i).m_type == NodeInfo::DB) {
            jam();
            BaseString::snprintf(buf, sizeof(buf),
                                 "Failure handling of node %d has not completed"
                                 " in %d seconds - state = %d",
                                 nodePtr.i, get_hb_count(nodePtr.i),
                                 nodePtr.p->failState);
            warningEvent("%s", buf);

            /**
             * Also dump DIH nf-state
             */
            signal->theData[0] = DumpStateOrd::DihTcSumaNodeFailCompleted;
            signal->theData[1] = nodePtr.i;
            sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 2, JBB);
          } else {
            jam();
            BaseString::snprintf(buf, sizeof(buf),
                                 "Failure handling of api %u has not completed"
                                 " in %d seconds - state = %d",
                                 nodePtr.i, get_hb_count(nodePtr.i),
                                 nodePtr.p->failState);
            warningEvent("%s", buf);
            if (nodePtr.p->failState == WAITING_FOR_API_FAILCONF) {
              jam();
              static_assert(NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks) == 5);
              BaseString::snprintf(buf, sizeof(buf),
                                   "  Waiting for blocks: %u %u %u %u %u",
                                   nodePtr.p->m_failconf_blocks[0],
                                   nodePtr.p->m_failconf_blocks[1],
                                   nodePtr.p->m_failconf_blocks[2],
                                   nodePtr.p->m_failconf_blocks[3],
                                   nodePtr.p->m_failconf_blocks[4]);
              warningEvent("%s", buf);
            }
          }
        }
      }
    } else if (type == NodeInfo::DB && nodePtr.p->phase == ZINIT &&
               nodePtr.p->m_secret != 0 &&
               NdbTick_Compare(now, nodePtr.p->m_alloc_timeout) > 0) {
      jam();
      nodePtr.p->m_secret = 0;
      warningEvent("Releasing node id allocation for node %u", nodePtr.i);
    }
  }  // for
  return;
}  // Qmgr::checkStartInterface()

/**-------------------------------------------------------------------------
 * This method is called when a DISCONNECT_REP signal arrived which means that
 * the API node is gone and we want to release resources in TC/DICT blocks.
 *---------------------------------------------------------------------------*/
void Qmgr::sendApiFailReq(Signal *signal, Uint16 failedNodeNo, bool sumaOnly) {
  jamEntry();
  signal->theData[0] = failedNodeNo;
  signal->theData[1] = QMGR_REF;

  /* We route the ApiFailReq signals via CMVMI
   * This is done to ensure that they are received after
   * any pending signals from the failed Api node when
   * running ndbmtd, as these signals would be enqueued from
   * the thread running CMVMI
   */
  Uint32 routedSignalSectionI = RNIL;
  ndbrequire(appendToSection(routedSignalSectionI, &signal->theData[0], 2));
  SectionHandle handle(this, routedSignalSectionI);

  /* RouteOrd data */
  RouteOrd *routeOrd = (RouteOrd *)&signal->theData[0];
  routeOrd->srcRef = reference();
  routeOrd->gsn = GSN_API_FAILREQ;
  routeOrd->from = failedNodeNo;

  NodeRecPtr failedNodePtr;
  failedNodePtr.i = failedNodeNo;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->failState = WAITING_FOR_API_FAILCONF;

  /* Send ROUTE_ORD signals to CMVMI via JBA
   * CMVMI will then immediately send the API_FAILREQ
   * signals to the destination block(s) using JBB
   * These API_FAILREQ signals will be sent *after*
   * any JBB signals enqueued from the failed API
   * by the CMVMI thread.
   */
  if (!sumaOnly) {
    jam();
    add_failconf_block(failedNodePtr, DBTC);
    routeOrd->dstRef = DBTC_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength, JBA, &handle);

    add_failconf_block(failedNodePtr, DBDICT);
    routeOrd->dstRef = DBDICT_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength, JBA, &handle);

    add_failconf_block(failedNodePtr, DBSPJ);
    routeOrd->dstRef = DBSPJ_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength, JBA, &handle);
  }

  /* Suma always notified */
  add_failconf_block(failedNodePtr, SUMA);
  routeOrd->dstRef = SUMA_REF;
  sendSignal(TRPMAN_REF, GSN_ROUTE_ORD, signal, RouteOrd::SignalLength, JBA,
             &handle);
}  // Qmgr::sendApiFailReq()

void Qmgr::execAPI_FAILREQ(Signal *signal) {
  jamEntry();
  NodeRecPtr failedNodePtr;
  failedNodePtr.i = signal->theData[0];
  // signal->theData[1] == QMGR_REF
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);

  ndbrequire(getNodeInfo(failedNodePtr.i).getType() != NodeInfo::DB);

  api_failed(signal, signal->theData[0]);
}

void Qmgr::execAPI_FAILCONF(Signal *signal) {
  NodeRecPtr failedNodePtr;

  jamEntry();
  failedNodePtr.i = signal->theData[0];
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);

  Uint32 block = refToMain(signal->theData[1]);
  if (failedNodePtr.p->failState != WAITING_FOR_API_FAILCONF ||
      !remove_failconf_block(failedNodePtr, block)) {
    jam();
    char logbuf[512] = "";
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(failedNodePtr.p->m_failconf_blocks);
         i++) {
      BaseString::snappend(logbuf, 512, "%u ",
                           failedNodePtr.p->m_failconf_blocks[i]);
    }
    g_eventLogger->info(
        "execAPI_FAILCONF from %u failedNodePtr.p->failState = %d blocks: %s",
        block, (Uint32)(failedNodePtr.p->failState), logbuf);
    systemErrorLab(signal, __LINE__);
  }  // if

  if (is_empty_failconf_block(failedNodePtr)) {
    jam();
    /**
     * When we set this state, connection will later be opened
     *   in checkStartInterface
     */
    failedNodePtr.p->failState = NORMAL;

    /**
     * Reset m_version only after all blocks has responded with API_FAILCONF
     *   so that no block risks reading 0 as node-version
     */
    setNodeInfo(failedNodePtr.i).m_version = 0;
    setNodeInfo(failedNodePtr.i).m_mysql_version = 0;
    recompute_version_info(getNodeInfo(failedNodePtr.i).m_type);
  }
  return;
}  // Qmgr::execAPI_FAILCONF()

void Qmgr::add_failconf_block(NodeRecPtr nodePtr, Uint32 block) {
  // Check that it does not already exists!!
  Uint32 pos = 0;
  for (; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++) {
    jam();
    if (nodePtr.p->m_failconf_blocks[pos] == 0) {
      jam();
      break;
    } else if (nodePtr.p->m_failconf_blocks[pos] == block) {
      jam();
      break;
    }
  }

  ndbrequire(pos != NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks));
  ndbassert(nodePtr.p->m_failconf_blocks[pos] != block);
  if (nodePtr.p->m_failconf_blocks[pos] == block) {
    jam();
    /**
     * Already in list!!
     */
#ifdef ERROR_INSERT
    ndbabort();
#endif
    return;
  }
  ndbrequire(nodePtr.p->m_failconf_blocks[pos] == 0);
  nodePtr.p->m_failconf_blocks[pos] = block;
}

bool Qmgr::remove_failconf_block(NodeRecPtr nodePtr, Uint32 block) {
  // Check that it does exists!!
  Uint32 pos = 0;
  for (; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++) {
    jam();
    if (nodePtr.p->m_failconf_blocks[pos] == 0) {
      jam();
      break;
    } else if (nodePtr.p->m_failconf_blocks[pos] == block) {
      jam();
      break;
    }
  }

  if (pos == NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks) ||
      nodePtr.p->m_failconf_blocks[pos] != block) {
    jam();
    /**
     * Not found!!
     */
    return false;
  }

  nodePtr.p->m_failconf_blocks[pos] = 0;
  for (pos++; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++) {
    jam();
    nodePtr.p->m_failconf_blocks[pos - 1] = nodePtr.p->m_failconf_blocks[pos];
  }

  return true;
}

bool Qmgr::is_empty_failconf_block(NodeRecPtr nodePtr) const {
  return nodePtr.p->m_failconf_blocks[0] == 0;
}

void Qmgr::execNDB_FAILCONF(Signal *signal) {
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;

  jamEntry();
  failedNodePtr.i = signal->theData[0];

  if (ERROR_INSERTED(930)) {
    CLEAR_ERROR_INSERT_VALUE;
    infoEvent("Discarding NDB_FAILCONF for %u", failedNodePtr.i);
    return;
  }

  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  if (failedNodePtr.p->failState == WAITING_FOR_NDB_FAILCONF) {
    g_eventLogger->info("Node %u has completed node fail handling",
                        failedNodePtr.i);
    failedNodePtr.p->failState = NORMAL;
  } else {
    jam();

    char buf[100];
    BaseString::snprintf(
        buf, 100, "Received NDB_FAILCONF for node %u with state: %d %d",
        failedNodePtr.i, failedNodePtr.p->phase, failedNodePtr.p->failState);
    progError(__LINE__, 0, buf);
    systemErrorLab(signal, __LINE__);
  }  // if

  if (cpresident == getOwnNodeId()) {
    jam();

    CRASH_INSERTION(936);
  }

  /**
   * Reset node version only after all blocks has handled the failure
   *   so that no block risks reading 0 as node version
   */
  setNodeInfo(failedNodePtr.i).m_version = 0;
  setNodeInfo(failedNodePtr.i).m_mysql_version = 0;
  recompute_version_info(NodeInfo::DB);

  /**
   * Prepare a NFCompleteRep and send to all connected API's
   * They can then abort all transaction waiting for response from
   * the failed node
   *
   * NOTE: This is sent from all nodes, as otherwise we would need
   *       take-over if cpresident dies before sending this
   */
  NFCompleteRep *const nfComp = (NFCompleteRep *)&signal->theData[0];
  nfComp->blockNo = QMGR_REF;
  nfComp->nodeId = getOwnNodeId();
  nfComp->failedNodeId = failedNodePtr.i;

  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE) {
      jamLine(nodePtr.i);
      sendSignal(nodePtr.p->blockRef, GSN_NF_COMPLETEREP, signal,
                 NFCompleteRep::SignalLength, JBB);
    }  // if
  }    // for
  return;
}  // Qmgr::execNDB_FAILCONF()

void Qmgr::execNF_COMPLETEREP(Signal *signal) {
  jamEntry();
  NFCompleteRep rep = *(NFCompleteRep *)signal->getDataPtr();
  if (rep.blockNo != DBTC) {
    jam();
    ndbassert(false);
    return;
  }

  /**
   * This is a simple way of having ndbapi to get
   * earlier information that transactions can be aborted
   */
  signal->theData[0] = rep.failedNodeId;
  // The below entries are not used by NdbAPI.
  signal->theData[1] = reference();
  signal->theData[2] = 0;  // Unknown failure number
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE) {
      jamLine(nodePtr.i);
      sendSignal(nodePtr.p->blockRef, GSN_TAKE_OVERTCCONF, signal,
                 TakeOverTcConf::SignalLength, JBB);
    }  // if
  }    // for
  return;
}

/*******************************/
/* DISCONNECT_REP             */
/*******************************/
const char *lookupConnectionError(Uint32 err);

void Qmgr::execDISCONNECT_REP(Signal *signal) {
  jamEntry();
  const DisconnectRep *const rep = (DisconnectRep *)&signal->theData[0];
  if (ERROR_INSERT_VALUE >= 951 && ERROR_INSERT_VALUE <= 960) {
    CRASH_INSERTION3();
  }
  const Uint32 nodeId = rep->nodeId;
  const Uint32 err = rep->err;
  const NodeInfo nodeInfo = getNodeInfo(nodeId);
  c_connectedNodes.clear(nodeId);
  DEB_STARTUP(("connectedNodes(%u) cleared", nodeId));

  if (nodeInfo.getType() == NodeInfo::DB) {
    c_readnodes_nodes.clear(nodeId);

    if (ERROR_INSERTED(942)) {
      g_eventLogger->info(
          "DISCONNECT_REP received from data node %u - crash insertion",
          nodeId);
      CRASH_INSERTION(942);
    }
  }

  {
    NodeRecPtr disc_nodePtr;
    disc_nodePtr.i = nodeId;
    ptrCheckGuard(disc_nodePtr, MAX_NODES, nodeRec);

    disc_nodePtr.p->m_is_activate_trp_ready_for_me = false;
    disc_nodePtr.p->m_is_activate_trp_ready_for_other = false;
    disc_nodePtr.p->m_is_multi_trp_setup = false;
    disc_nodePtr.p->m_is_freeze_thread_completed = false;
    disc_nodePtr.p->m_is_ready_to_switch_trp = false;
    disc_nodePtr.p->m_is_preparing_switch_trp = false;
    disc_nodePtr.p->m_is_using_multi_trp = false;
    disc_nodePtr.p->m_set_up_multi_trp_started = false;
    disc_nodePtr.p->m_used_num_multi_trps = 0;
    disc_nodePtr.p->m_multi_trp_blockref = 0;
    disc_nodePtr.p->m_check_multi_trp_connect_loop_count = 0;
    disc_nodePtr.p->m_num_activated_trps = 0;
    if (disc_nodePtr.p->m_is_in_same_nodegroup) {
      jam();
      DEB_MULTI_TRP(
          ("Change neighbour node setup for node %u", disc_nodePtr.i));
      check_no_multi_trp(signal, disc_nodePtr.i);
      startChangeNeighbourNode();
      setNeighbourNode(disc_nodePtr.i);
      endChangeNeighbourNode();
    }
  }

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);

  char buf[100];
  if (nodeInfo.getType() == NodeInfo::DB &&
      getNodeState().startLevel < NodeState::SL_STARTED) {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    CRASH_INSERTION(944);
    CRASH_INSERTION(946);
    BaseString::snprintf(buf, 100, "Node %u disconnected in phase: %u", nodeId,
                         nodePtr.p->phase);
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbabort();
  }

  if (getNodeInfo(nodeId).getType() != NodeInfo::DB) {
    jam();
    api_failed(signal, nodeId);
    return;
  }

  switch (nodePtr.p->phase) {
    case ZRUNNING:
      jam();
      break;
    case ZINIT:
      ndbabort();
    case ZSTARTING:
      progError(__LINE__, NDBD_EXIT_CONNECTION_SETUP_FAILED,
                lookupConnectionError(err));
    case ZPREPARE_FAIL:
      ndbabort();
    case ZFAIL_CLOSING:
      ndbabort();
    case ZAPI_ACTIVATION_ONGOING:
      ndbabort();
    case ZAPI_ACTIVE:
      ndbabort();
    case ZAPI_INACTIVE: {
      BaseString::snprintf(buf, 100, "Node %u disconnected", nodeId);
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    }
  }

  if (ERROR_INSERTED(939) && ERROR_INSERT_EXTRA == nodeId) {
    g_eventLogger->info(
        "Ignoring DISCONNECT_REP for node %u that was force disconnected",
        nodeId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

  node_failed(signal, nodeId);
}  // DISCONNECT_REP

void Qmgr::node_failed(Signal *signal, Uint16 aFailedNode) {
  NodeRecPtr failedNodePtr;
  /**------------------------------------------------------------------------
   *   A COMMUNICATION LINK HAS BEEN DISCONNECTED. WE MUST TAKE SOME ACTION
   *   DUE TO THIS.
   *-----------------------------------------------------------------------*/
  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->m_secret = 0;  // Not yet Uint64(rand()) << 32 + rand();

  ndbrequire(getNodeInfo(failedNodePtr.i).getType() == NodeInfo::DB);

  /**---------------------------------------------------------------------
   *   THE OTHER NODE IS AN NDB NODE, WE HANDLE IT AS IF A HEARTBEAT
   *   FAILURE WAS DISCOVERED.
   *---------------------------------------------------------------------*/
  switch (failedNodePtr.p->phase) {
    case ZRUNNING:
      jam();
      failReportLab(signal, aFailedNode, FailRep::ZLINK_FAILURE,
                    getOwnNodeId());
      return;
    case ZSTARTING:
      /**
       * bug#42422
       *   Force "real" failure handling
       */
      jam();
      DEB_MULTI_TRP(("Node %u in ZRUNNING, failedNode", failedNodePtr.i));
      failedNodePtr.p->phase = ZRUNNING;
      DEB_STARTUP(("4:phase(%u) = ZRUNNING", failedNodePtr.i));
      failReportLab(signal, aFailedNode, FailRep::ZLINK_FAILURE,
                    getOwnNodeId());
      return;
    case ZFAIL_CLOSING:  // Close already in progress
      jam();
      return;
    case ZPREPARE_FAIL:  // PREP_FAIL already sent CLOSE_COMREQ
      jam();
      return;
    case ZINIT: {
      jam();
      /*---------------------------------------------------------------------*/
      // The other node is still not in the cluster but disconnected.
      // We must restart communication in three seconds.
      /*---------------------------------------------------------------------*/
      failedNodePtr.p->failState = NORMAL;
      failedNodePtr.p->phase = ZFAIL_CLOSING;
      DEB_STARTUP(("phase(%u) = ZFAIL_CLOSING", failedNodePtr.i));
      set_hb_count(failedNodePtr.i) = 0;

      CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];

      closeCom->xxxBlockRef = reference();
      closeCom->requestType = CloseComReqConf::RT_NO_REPLY;
      closeCom->failNo = 0;
      closeCom->noOfNodes = 1;
      closeCom->failedNodeId = failedNodePtr.i;
      sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
                 CloseComReqConf::SignalLength, JBB);
      return;
    }
    case ZAPI_ACTIVE:  // Unexpected states handled in ::api_failed()
      ndbabort();
    case ZAPI_INACTIVE:
      ndbabort();
    case ZAPI_ACTIVATION_ONGOING:
      ndbabort();
    default:
      ndbabort();  // Unhandled state
  }                // switch

  return;
}

void Qmgr::execUPGRADE_PROTOCOL_ORD(Signal *signal) {
  const UpgradeProtocolOrd *ord = (UpgradeProtocolOrd *)signal->getDataPtr();
  switch (ord->type) {
    case UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP:
      jam();
      m_micro_gcp_enabled = true;
      return;
  }
}

void Qmgr::api_failed(Signal *signal, Uint32 nodeId) {
  jam();
  NodeRecPtr failedNodePtr;
  /**------------------------------------------------------------------------
   *   A COMMUNICATION LINK HAS BEEN DISCONNECTED. WE MUST TAKE SOME ACTION
   *   DUE TO THIS.
   *-----------------------------------------------------------------------*/
  failedNodePtr.i = nodeId;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->m_secret = 0;  // Not yet Uint64(rand()) << 32 + rand();

  if (failedNodePtr.p->phase == ZFAIL_CLOSING) {
    jam();
    if (unlikely(failedNodePtr.p->failState == NORMAL &&
                 getNodeState().startLevel < NodeState::SL_STARTED &&
                 getNodeInfo(failedNodePtr.i).getType() == NodeInfo::API)) {
      jam();

      /* Perform node failure handling (apart from disconnect)
       * as during node restart we may receive state for disconnected API
       * nodes from a nodegroup peer, that must be cleaned up
       */
      failedNodePtr.p->failState = WAITING_FOR_CLOSECOMCONF_NOTACTIVE;

      /* No connection to close, proceed to failure handling */
      CloseComReqConf *ccconf = (CloseComReqConf *)signal->theData;
      ccconf->xxxBlockRef = reference();
      ccconf->requestType = CloseComReqConf::RT_API_FAILURE;
      ccconf->failNo = RNIL;
      ccconf->noOfNodes = 1;
      ccconf->failedNodeId = nodeId;

      handleApiCloseComConf(signal);
      return;
    }

    /**
     * Normal ZFAIL_CLOSING path
     * Failure handling already in progress
     */
    return;
  }

  ndbrequire(failedNodePtr.p->failState == NORMAL);

  /* Send API_FAILREQ to peer QMGR blocks to allow them to disconnect
   * quickly
   * Local application blocks get API_FAILREQ once all pending signals
   * from the failed API have been processed.
   */
  signal->theData[0] = failedNodePtr.i;
  signal->theData[1] = QMGR_REF;
  NodeReceiverGroup rg(QMGR, c_clusterNodes);
  sendSignal(rg, GSN_API_FAILREQ, signal, 2, JBA);

  /* Now ask CMVMI to disconnect the node */
  FailState initialState = (failedNodePtr.p->phase == ZAPI_ACTIVE)
                               ? WAITING_FOR_CLOSECOMCONF_ACTIVE
                               : WAITING_FOR_CLOSECOMCONF_NOTACTIVE;

  failedNodePtr.p->failState = initialState;
  failedNodePtr.p->phase = ZFAIL_CLOSING;
  set_hb_count(failedNodePtr.i) = 0;

  CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];
  closeCom->xxxBlockRef = reference();
  closeCom->requestType = CloseComReqConf::RT_API_FAILURE;
  closeCom->failNo = 0;
  closeCom->noOfNodes = 1;
  closeCom->failedNodeId = nodeId;
  ProcessInfo *processInfo = getProcessInfo(nodeId);
  if (processInfo) {
    processInfo->invalidate();
  }
  sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
             CloseComReqConf::SignalLength, JBB);
}  // api_failed

/**--------------------------------------------------------------------------
 * AN API NODE IS REGISTERING. IF FOR THE FIRST TIME WE WILL ENABLE
 * COMMUNICATION WITH ALL NDB BLOCKS.
 *---------------------------------------------------------------------------*/
/*******************************/
/* API_REGREQ                 */
/*******************************/
void Qmgr::execAPI_REGREQ(Signal *signal) {
  jamEntry();

  ApiRegReq *req = (ApiRegReq *)signal->getDataPtr();
  const Uint32 version = req->version;
  const BlockReference ref = req->ref;

  Uint32 mysql_version = req->mysql_version;

  NodeRecPtr apiNodePtr;
  apiNodePtr.i = refToNode(ref);
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);

  if (apiNodePtr.p->phase == ZFAIL_CLOSING) {
    jam();
    /**
     * This node is pending CLOSE_COM_CONF
     *   ignore API_REGREQ
     */
    return;
  }

  if (!c_connectedNodes.get(apiNodePtr.i)) {
    jam();
    /**
     * We have not yet heard execCONNECT_REP
     *   so ignore this until we do...
     */
    return;
  }

#if 0
  g_eventLogger->info("Qmgr::execAPI_REGREQ: Recd API_REGREQ (NodeId=%d)", apiNodePtr.i);
#endif

  bool compatability_check;
  const char *extra = 0;
  NodeInfo::NodeType type = getNodeInfo(apiNodePtr.i).getType();
  switch (type) {
    case NodeInfo::API:
      jam();
      compatability_check = ndbCompatible_ndb_api(NDB_VERSION, version);
      break;
    case NodeInfo::MGM:
      compatability_check = ndbCompatible_ndb_mgmt(NDB_VERSION, version);
      break;
    case NodeInfo::DB:
    case NodeInfo::INVALID:
    default:
      sendApiRegRef(signal, ref, ApiRegRef::WrongType);
      infoEvent("Invalid connection attempt with type %d", type);
      return;
  }

  if (!ndbd_upgrade_ok(version)) {
    compatability_check = false;
  }

  if (!compatability_check) {
    jam();
    char buf[NDB_VERSION_STRING_BUF_SZ];
    infoEvent(
        "Connection attempt from %s id=%d with %s "
        "incompatible with %s%s",
        type == NodeInfo::API ? "api or mysqld" : "management server",
        apiNodePtr.i,
        ndbGetVersionString(version, mysql_version, 0, buf, sizeof(buf)),
        NDB_VERSION_STRING, extra ? extra : "");
    apiNodePtr.p->phase = ZAPI_INACTIVE;
    sendApiRegRef(signal, ref, ApiRegRef::UnsupportedVersion);
    return;
  }

  setNodeInfo(apiNodePtr.i).m_version = version;
  setNodeInfo(apiNodePtr.i).m_mysql_version = mysql_version;
  set_hb_count(apiNodePtr.i) = 0;

  NodeState state = getNodeState();
  if (apiNodePtr.p->phase == ZAPI_INACTIVE) {
    apiNodePtr.p->blockRef = ref;
    if ((state.startLevel == NodeState::SL_STARTED ||
         state.getSingleUserMode() ||
         (state.startLevel == NodeState::SL_STARTING &&
          state.starting.startPhase >= 8))) {
      jam();
      /**----------------------------------------------------------------------
       * THE API NODE IS REGISTERING. WE WILL ACCEPT IT BY CHANGING STATE AND
       * SENDING A CONFIRM. We set state to ZAPI_ACTIVATION_ONGOING to ensure
       * that we don't send unsolicited API_REGCONF or other things before we
       * actually fully enabled the node for communicating with the new API
       * node. It also avoids sending NODE_FAILREP, NF_COMPLETEREP and
       * TAKE_OVERTCCONF even before the API_REGCONF is sent. We will get a
       * fresh state of the nodes in API_REGCONF which is sufficient, no need
       * to update the API before the API got the initial state.
       *----------------------------------------------------------------------*/
      apiNodePtr.p->phase = ZAPI_ACTIVATION_ONGOING;
      EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
      enableComReq->m_senderRef = reference();
      enableComReq->m_senderData = ENABLE_COM_API_REGREQ;
      enableComReq->m_enableNodeId = apiNodePtr.i;
      sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
                 EnableComReq::SignalLength, JBB);
      return;
    }
    /**
     * The node is in some kind of STOPPING state, so we send API_REGCONF even
     * though we've not enabled communication, if the API tries to send
     * anything to us anyways it will simply be ignored since only QMGR will
     * receive signals in this state. The API receives the node states, so it
     * should be able to discover what nodes that it is able to actually use.
     */
  }
  if (apiNodePtr.p->phase == ZAPI_ACTIVATION_ONGOING) {
    jam();
    /* Waiting for TRPMAN to finish enabling communication
     * Must not send conf before then.
     */
    return;
  }

  sendApiRegConf(signal, apiNodePtr.i);
}  // Qmgr::execAPI_REGREQ()

void Qmgr::handleEnableComApiRegreq(Signal *signal, Uint32 node) {
  NodeRecPtr apiNodePtr;
  NodeInfo::NodeType type = getNodeInfo(node).getType();
  Uint32 version = getNodeInfo(node).m_version;
  recompute_version_info(type, version);

  signal->theData[0] = node;
  signal->theData[1] = version;
  NodeReceiverGroup rg(QMGR, c_clusterNodes);
  rg.m_nodes.clear(getOwnNodeId());
  sendSignal(rg, GSN_NODE_VERSION_REP, signal, 2, JBB);

  signal->theData[0] = node;
  EXECUTE_DIRECT(NDBCNTR, GSN_API_START_REP, signal, 1);

  apiNodePtr.i = node;
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
  if (apiNodePtr.p->phase == ZAPI_ACTIVATION_ONGOING) {
    /**
     * Now we're about to send API_REGCONF to an API node, this means
     * that this node can immediately start communicating to TC, SUMA
     * and so forth. The state also indicates that the API is ready
     * to receive an unsolicited API_REGCONF when the node goes to
     * state SL_STARTED.
     */
    jam();
    apiNodePtr.p->phase = ZAPI_ACTIVE;
    sendApiRegConf(signal, node);
  }
  jam();
  /**
   * Node is no longer in state ZAPI_ACTIVATION_ONGOING, the node must
   * have failed, we can ignore sending API_REGCONF to a failed node.
   */
}

void Qmgr::execNODE_STARTED_REP(Signal *signal) {
  NodeRecPtr apiNodePtr;
  for (apiNodePtr.i = 1; apiNodePtr.i < MAX_NODES; apiNodePtr.i++) {
    ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
    NodeInfo::NodeType type = getNodeInfo(apiNodePtr.i).getType();
    if (type != NodeInfo::API) {
      /* Not an API node */
      continue;
    }
    if (!c_connectedNodes.get(apiNodePtr.i)) {
      /* API not connected */
      continue;
    }
    if (apiNodePtr.p->phase != ZAPI_ACTIVE) {
      /**
       * The phase variable can be in three states for the API nodes, it can
       * be ZAPI_INACTIVE for an API node that hasn't connected, it can be
       * ZFAIL_CLOSING for an API node that recently failed and is performing
       * failure handling. It can be in the state ZAPI_ACTIVE which it enters
       * upon us receiving an API_REGREQ from the API. So at this point the
       * API is also able to receive an unsolicited API_REGCONF message.
       */
      continue;
    }
    /**
     * We will send an unsolicited API_REGCONF to the API node, this makes the
     * API node aware of our existence much faster (without it can wait up to
     * the length of a heartbeat DB-API period. For rolling restarts and other
     * similar actions this can easily cause the API to not have any usable
     * DB connections at all. This unsolicited response minimises this window
     * of unavailability to zero for all practical purposes.
     */
    sendApiRegConf(signal, apiNodePtr.i);
  }
}

void Qmgr::sendApiRegConf(Signal *signal, Uint32 node) {
  NodeRecPtr apiNodePtr;
  apiNodePtr.i = node;
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
  const BlockReference ref = apiNodePtr.p->blockRef;
  ndbassert(ref != 0);

  /* No Conf to be sent unless :
   * - API node is ACTIVE
   * - MGM node is ACTIVE | INACTIVE
   * - Data node is shutting down
   */
  ndbassert(apiNodePtr.p->phase == ZAPI_ACTIVE ||
            (apiNodePtr.p->phase == ZAPI_INACTIVE &&
             getNodeInfo(apiNodePtr.i).getType() == NodeInfo::MGM) ||
            (apiNodePtr.p->phase == ZAPI_INACTIVE &&
             getNodeState().startLevel >= NodeState::SL_STOPPING_1));

  ApiRegConf *const apiRegConf = (ApiRegConf *)&signal->theData[0];
  apiRegConf->qmgrRef = reference();
  apiRegConf->apiHeartbeatFrequency = (chbApiDelay / 10);
  apiRegConf->version = NDB_VERSION;
  apiRegConf->mysql_version = NDB_MYSQL_VERSION_D;
  apiRegConf->nodeState = getNodeState();
  {
    NodeRecPtr nodePtr;
    nodePtr.i = getOwnNodeId();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    Uint32 dynamicId = nodePtr.p->ndynamicId;

    if (apiRegConf->nodeState.masterNodeId != getOwnNodeId()) {
      jam();
      apiRegConf->nodeState.dynamicId = dynamicId;
    } else {
      apiRegConf->nodeState.dynamicId = (Uint32)(-(Int32)dynamicId);
    }
  }
  NodeVersionInfo info = getNodeVersionInfo();
  apiRegConf->minDbVersion = info.m_type[NodeInfo::DB].m_min_version;
  apiRegConf->minApiVersion = info.m_type[NodeInfo::API].m_min_version;
  apiRegConf->nodeState.m_connected_nodes.assign(c_connectedNodes);
  sendSignal(ref, GSN_API_REGCONF, signal, ApiRegConf::SignalLength, JBB);
}

void Qmgr::sendVersionedDb(NodeReceiverGroup rg, GlobalSignalNumber gsn,
                           Signal *signal, Uint32 length, JobBufferLevel jbuf,
                           Uint32 minversion) {
  jam();
  NodeVersionInfo info = getNodeVersionInfo();
  if (info.m_type[NodeInfo::DB].m_min_version >= minversion) {
    jam();
    sendSignal(rg, gsn, signal, length, jbuf);
  } else {
    jam();
    Uint32 i = 0, cnt = 0;
    while ((i = rg.m_nodes.find(i + 1)) != NodeBitmask::NotFound) {
      jam();
      if (getNodeInfo(i).m_version >= minversion) {
        jam();
        cnt++;
        sendSignal(numberToRef(rg.m_block, i), gsn, signal, length, jbuf);
      }
    }
    ndbassert((cnt == 0 && rg.m_nodes.count() == 0) ||
              (cnt < rg.m_nodes.count()));
  }
}

void Qmgr::execAPI_VERSION_REQ(Signal *signal) {
  jamEntry();
  ApiVersionReq *const req = (ApiVersionReq *)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 nodeId = req->nodeId;

  ApiVersionConf *conf = (ApiVersionConf *)req;
  static_assert(sizeof(in6_addr) <= 16,
                "Cannot fit in6_inaddr into ApiVersionConf:m_inet6_addr");
  NodeInfo nodeInfo = getNodeInfo(nodeId);
  conf->m_inet_addr = 0;
  Uint32 siglen = ApiVersionConf::SignalLengthIPv4;
  if (nodeInfo.m_connected) {
    conf->version = nodeInfo.m_version;
    conf->mysql_version = nodeInfo.m_mysql_version;
    ndb_sockaddr in =
        globalTransporterRegistry.get_connect_address_node(nodeId);
    if (in.get_in6_addr((in6_addr *)&conf->m_inet6_addr) == 0)
      siglen = ApiVersionConf::SignalLength;
    (void)in.get_in_addr((in_addr *)&conf->m_inet_addr);
  } else {
    conf->version = 0;
    conf->mysql_version = 0;
    memset(conf->m_inet6_addr, 0, sizeof(conf->m_inet6_addr));
  }
  conf->nodeId = nodeId;
  conf->isSingleUser = (nodeId == getNodeState().getSingleUserApi());
  sendSignal(senderRef, GSN_API_VERSION_CONF, signal, siglen, JBB);
}

void Qmgr::execNODE_VERSION_REP(Signal *signal) {
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  Uint32 version = signal->theData[1];

  if (nodeId < MAX_NODES) {
    jam();
    Uint32 type = getNodeInfo(nodeId).m_type;
    setNodeInfo(nodeId).m_version = version;
    recompute_version_info(type, version);
  }
}

void Qmgr::recompute_version_info(Uint32 type, Uint32 version) {
  NodeVersionInfo &info = setNodeVersionInfo();
  switch (type) {
    case NodeInfo::DB:
    case NodeInfo::API:
    case NodeInfo::MGM:
      break;
    default:
      return;
  }

  if (info.m_type[type].m_min_version == 0 ||
      version < info.m_type[type].m_min_version)
    info.m_type[type].m_min_version = version;
  if (version > info.m_type[type].m_max_version)
    info.m_type[type].m_max_version = version;
}

void Qmgr::recompute_version_info(Uint32 type) {
  switch (type) {
    case NodeInfo::DB:
    case NodeInfo::API:
    case NodeInfo::MGM:
      break;
    default:
      return;
  }

  Uint32 min = ~0, max = 0;
  Uint32 cnt = type == NodeInfo::DB ? MAX_NDB_NODES : MAX_NODES;
  for (Uint32 i = 1; i < cnt; i++) {
    if (getNodeInfo(i).m_type == type) {
      Uint32 version = getNodeInfo(i).m_version;

      if (version) {
        if (version < min) min = version;
        if (version > max) max = version;
      }
    }
  }

  NodeVersionInfo &info = setNodeVersionInfo();
  info.m_type[type].m_min_version = min == ~(Uint32)0 ? 0 : min;
  info.m_type[type].m_max_version = max;
}

#if 0
bool
Qmgr::checkAPIVersion(NodeId nodeId, 
		      Uint32 apiVersion, Uint32 ownVersion) const {
  bool ret=true;
  /**
   * First implementation...
   */
  if ((getMajor(apiVersion) < getMajor(ownVersion) ||
       getMinor(apiVersion) < getMinor(ownVersion)) &&
      apiVersion >= API_UPGRADE_VERSION) {
    jam();
    if ( getNodeInfo(nodeId).getType() !=  NodeInfo::MGM ) {
      jam();
      ret = false;
    } else {
      jam();
      /* we have a software upgrade situation, mgmtsrvr should be
       * the highest, let him decide what to do
       */
      ;
    }
  }
  return ret;
}
#endif

void Qmgr::sendApiRegRef(Signal *signal, Uint32 Tref,
                         ApiRegRef::ErrorCode err) {
  ApiRegRef *ref = (ApiRegRef *)signal->getDataPtrSend();
  ref->ref = reference();
  ref->version = NDB_VERSION;
  ref->mysql_version = NDB_MYSQL_VERSION_D;
  ref->errorCode = err;
  sendSignal(Tref, GSN_API_REGREF, signal, ApiRegRef::SignalLength, JBB);
}

/**--------------------------------------------------------------------------
 * A NODE HAS BEEN DECLARED AS DOWN. WE WILL CLOSE THE COMMUNICATION TO THIS
 * NODE IF NOT ALREADY DONE. IF WE ARE PRESIDENT OR BECOMES PRESIDENT BECAUSE
 * OF A FAILED PRESIDENT THEN WE WILL TAKE FURTHER ACTION.
 *---------------------------------------------------------------------------*/
void Qmgr::failReportLab(Signal *signal, Uint16 aFailedNode,
                         FailRep::FailCause aFailCause, Uint16 sourceNode) {
  NodeRecPtr nodePtr;
  NodeRecPtr failedNodePtr;
  NodeRecPtr myNodePtr;

  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  FailRep *rep = (FailRep *)signal->getDataPtr();

  if (check_multi_node_shutdown(signal)) {
    jam();
    return;
  }

  if (isNodeConnectivitySuspect(sourceNode) &&
      // (! isNodeConnectivitySuspect(aFailedNode)) &&  // TODO : Required?
      ((aFailCause == FailRep::ZCONNECT_CHECK_FAILURE) ||
       (aFailCause == FailRep::ZLINK_FAILURE))) {
    jam();
    /* Connectivity related failure report from a node with suspect
     * connectivity, handle differently
     */
    ndbrequire(sourceNode != getOwnNodeId());

    handleFailFromSuspect(signal, aFailCause, aFailedNode, sourceNode);
    return;
  }

  if (failedNodePtr.i == getOwnNodeId()) {
    jam();

    Uint32 code = NDBD_EXIT_NODE_DECLARED_DEAD;
    const char *msg = 0;
    // Message buffer for FailRep::ZPARTITIONED_CLUSTER
    static const Uint32 bitmaskTextLen = NdbNodeBitmask::TextLength + 1;
    char extra[2 * bitmaskTextLen + 30];

    switch (aFailCause) {
      case FailRep::ZOWN_FAILURE:
        msg = "Own failure";
        break;
      case FailRep::ZOTHER_NODE_WHEN_WE_START:
      case FailRep::ZOTHERNODE_FAILED_DURING_START:
        msg = "Other node died during start";
        break;
      case FailRep::ZIN_PREP_FAIL_REQ:
        msg = "Prep fail";
        break;
      case FailRep::ZSTART_IN_REGREQ:
        msg = "Start timeout";
        break;
      case FailRep::ZHEARTBEAT_FAILURE:
        msg = "Heartbeat failure";
        break;
      case FailRep::ZLINK_FAILURE:
        msg = "Connection failure";
        break;
      case FailRep::ZPARTITIONED_CLUSTER: {
        code = NDBD_EXIT_PARTITIONED_SHUTDOWN;
        char buf1[bitmaskTextLen], buf2[bitmaskTextLen];
        c_clusterNodes.getText(buf1);
        if (((signal->getLength() ==
              FailRep::OrigSignalLength + FailRep::PartitionedExtraLength_v1) ||
             (signal->getLength() ==
              FailRep::SignalLength + FailRep::PartitionedExtraLength_v1)) &&
            signal->header.theVerId_signalNumber == GSN_FAIL_REP) {
          jam();
          NdbNodeBitmask part;
          Uint32 senderRef = signal->getSendersBlockRef();
          Uint32 senderVersion = getNodeInfo(refToNode(senderRef)).m_version;
          if (signal->getNoOfSections() >= 1) {
            ndbrequire(ndbd_send_node_bitmask_in_section(senderVersion));
            SectionHandle handle(this, signal);
            SegmentedSectionPtr ptr;
            ndbrequire(handle.getSection(ptr, 0));

            ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
            copy(part.rep.data, ptr);

            releaseSections(handle);
          } else {
            part.assign(NdbNodeBitmask48::Size, rep->partitioned.partition_v1);
          }
          part.getText(buf2);
          BaseString::snprintf(extra, sizeof(extra),
                               "Our cluster: %s other cluster: %s", buf1, buf2);
        } else {
          jam();
          BaseString::snprintf(extra, sizeof(extra), "Our cluster: %s", buf1);
        }
        msg = extra;
        break;
      }
      case FailRep::ZMULTI_NODE_SHUTDOWN:
        msg = "Multi node shutdown";
        break;
      case FailRep::ZCONNECT_CHECK_FAILURE:
        msg = "Connectivity check failure";
        break;
      case FailRep::ZFORCED_ISOLATION:
        msg = "Forced isolation";
        if (ERROR_INSERTED(942)) {
          g_eventLogger->info(
              "FAIL_REP FORCED_ISOLATION received from data node %u - "
              "ignoring.",
              sourceNode);
          /* Let's wait for remote disconnection */
          return;
        }
        break;
      default:
        msg = "<UNKNOWN>";
    }

    CRASH_INSERTION(932);
    CRASH_INSERTION(938);

    char buf[sizeof(extra) + 100];
    BaseString::snprintf(
        buf, sizeof(buf),
        "We(%u) have been declared dead by %u (via %u) reason: %s(%u)",
        getOwnNodeId(), sourceNode, refToNode(signal->getSendersBlockRef()),
        msg ? msg : "<Unknown>", aFailCause);

    progError(__LINE__, code, buf);
    return;
  }  // if

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase != ZRUNNING) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }  // if

  if (getNodeState().startLevel < NodeState::SL_STARTED) {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    char buf[100];
    switch (aFailCause) {
      case FailRep::ZHEARTBEAT_FAILURE:
        BaseString::snprintf(buf, 100, "Node %d heartbeat failure",
                             failedNodePtr.i);
        CRASH_INSERTION(947);
        break;
      default:
        BaseString::snprintf(buf, 100, "Node %d failed", failedNodePtr.i);
    }
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
  }

  const NdbNodeBitmask TfailedNodes(cfailedNodes);
  failReport(signal, failedNodePtr.i, (UintR)ZTRUE, aFailCause, sourceNode);

  /**
   * If any node is starting now (c_start.startNode != 0)
   *   include it in nodes handled by sendPrepFailReq
   */
  if (c_start.m_startNode != 0) {
    jam();
    cfailedNodes.set(c_start.m_startNode);
  }

  if (cpresident == getOwnNodeId()) {
    jam();
    if (ctoStatus == Q_NOT_ACTIVE) {
      jam();
      /**--------------------------------------------------------------------
       * AS PRESIDENT WE ARE REQUIRED TO START THE EXCLUSION PROCESS SUCH THAT
       * THE APPLICATION SEE NODE FAILURES IN A CONSISTENT ORDER.
       * IF WE HAVE BECOME PRESIDENT NOW (CTO_STATUS = ACTIVE) THEN WE HAVE
       * TO COMPLETE THE PREVIOUS COMMIT FAILED NODE PROCESS BEFORE STARTING
       * A NEW.
       * CTO_STATUS = ACTIVE CAN ALSO MEAN THAT WE ARE PRESIDENT AND ARE
       * CURRENTLY COMMITTING A SET OF NODE CRASHES. IN THIS CASE IT IS NOT
       * ALLOWED TO START PREPARING NEW NODE CRASHES.
       *---------------------------------------------------------------------*/
      if (!cfailedNodes.equal(TfailedNodes)) {
        jam();
        cfailureNr = cfailureNr + 1;
        for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
          ptrAss(nodePtr, nodeRec);
          if (nodePtr.p->phase == ZRUNNING) {
            jamLine(nodePtr.i);
            sendPrepFailReq(signal, nodePtr.i);
          }  // if
        }    // for
      }      // if
    }        // if
  }
  return;
}  // Qmgr::failReportLab()

/**-------------------------------------------------------------------------
 * WE HAVE RECEIVED A PREPARE TO EXCLUDE A NUMBER OF NODES FROM THE CLUSTER.
 * WE WILL FIRST CHECK THAT WE HAVE NOT ANY MORE NODES THAT
 * WE ALSO HAVE EXCLUDED
 *--------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILREQ               */
/*******************************/
void Qmgr::execPREP_FAILREQ(Signal *signal) {
  NodeRecPtr myNodePtr;
  PrepFailReqRef *const prepFail = (PrepFailReqRef *)&signal->theData[0];
  BlockReference Tblockref = prepFail->xxxBlockRef;
  Uint16 TfailureNr = prepFail->failNo;
  Uint32 senderRef = signal->getSendersBlockRef();
  Uint32 senderVersion = getNodeInfo(refToNode(senderRef)).m_version;

  jamEntry();

  NdbNodeBitmask nodes;
  if (signal->getNoOfSections() >= 1) {
    jam();
    ndbrequire(ndbd_send_node_bitmask_in_section(senderVersion));
    SectionHandle handle(this, signal);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(nodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    jam();
    nodes.assign(NdbNodeBitmask48::Size, prepFail->theNodes);
  }

  // Clear 'c_start.m_startNode' if it failed.
  if (nodes.get(c_start.m_startNode)) {
    jam();
    DEB_STARTUP(("Clear c_start.m_startNode"));
    c_start.reset();
  }
  if (c_start.m_gsn == GSN_CM_NODEINFOCONF) {
    Uint32 nodeId;
    jam();
    /**
     * This is a very unusual event we are looking for, but still required
     * to be handled. The starting node has connected to the president and
     * managed to start the node inclusion protocol. We received an indication
     * of this from the president. The starting node now however fails before
     * it connected to us, so we need to clear the indication of that we
     * received CM_ADD(Prepare) from president since this belonged to an
     * already cancelled node restart.
     */
    for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
      if (c_start.m_nodes.isWaitingFor(nodeId) && nodes.get(nodeId)) {
        jamLine(nodeId);
        /* Found such a condition as described above, clear state */
        c_start.m_gsn = RNIL;
        c_start.m_nodes.clearWaitingFor();
        break;
      }
    }
  }

  if (check_multi_node_shutdown(signal)) {
    jam();
    return;
  }

  if (ERROR_INSERTED(941) && getOwnNodeId() == 4 && nodes.get(2)) {
    /* Insert ERROR_INSERT crash */
    CRASH_INSERTION(941);
  }

  cprepFailedNodes.assign(nodes);
  ndbassert(prepFail->noOfNodes == cprepFailedNodes.count());

  /**
   * Block commit until node failures has stabilized
   *
   * @See RT352
   */
  BlockCommitOrd *const block = (BlockCommitOrd *)&signal->theData[0];
  block->failNo = TfailureNr;
  EXECUTE_DIRECT(DBDIH, GSN_BLOCK_COMMIT_ORD, signal,
                 BlockCommitOrd::SignalLength);

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase != ZRUNNING) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }  // if

  if (getNodeState().startLevel < NodeState::SL_STARTED) {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    char buf[100];
    BaseString::snprintf(buf, 100, "Node failure during restart");
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
  }

  for (unsigned nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (cprepFailedNodes.get(nodeId)) {
      jam();
      failReport(signal, nodeId, (UintR)ZFALSE, FailRep::ZIN_PREP_FAIL_REQ,
                 0); /* Source node not required (or known) here */
    }                // if
  }                  // for
  sendCloseComReq(signal, Tblockref, TfailureNr);
  ccommitFailedNodes.clear();
  cprepareFailureNr = TfailureNr;
  return;
}  // Qmgr::execPREP_FAILREQ()

void Qmgr::handleApiCloseComConf(Signal *signal) {
  jam();
  CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];

  Uint32 nodeId = closeCom->failedNodeId;
  /* Api failure special case */
  /* Check that *only* 1 *API* node is included in
   * this CLOSE_COM_CONF
   */
  ndbrequire(getNodeInfo(nodeId).getType() != NodeInfo::DB);
  ndbrequire(closeCom->noOfNodes == 1);

  /* Now that we know communication from the failed Api has
   * ceased, we can send the required API_FAILREQ signals
   * and continue API failure handling
   */
  NodeRecPtr failedNodePtr;
  failedNodePtr.i = nodeId;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);

  ndbrequire(
      (failedNodePtr.p->failState == WAITING_FOR_CLOSECOMCONF_ACTIVE) ||
      (failedNodePtr.p->failState == WAITING_FOR_CLOSECOMCONF_NOTACTIVE));

  if (failedNodePtr.p->failState == WAITING_FOR_CLOSECOMCONF_ACTIVE) {
    /**
     * Inform application blocks TC, DICT, SUMA etc.
     */
    jam();
    sendApiFailReq(signal, nodeId, false);  // !sumaOnly
    if (arbitRec.node == nodeId) {
      arbitRec.code = ArbitCode::ApiFail;
      handleArbitApiFail(signal, nodeId);
    }
  } else {
    /**
     * Always inform SUMA
     */
    jam();
    sendApiFailReq(signal, nodeId, true);  // sumaOnly
  }

  if (getNodeInfo(failedNodePtr.i).getType() == NodeInfo::MGM) {
    /**
     * Allow MGM do reconnect "directly"
     */
    jam();
    set_hb_count(failedNodePtr.i) = 3;
  }

  /* Handled the single API node failure */
  return;
}

/**---------------------------------------------------------------------------
 * THE CRASHED NODES HAS BEEN EXCLUDED FROM COMMUNICATION.
 * WE WILL CHECK WHETHER ANY MORE NODES HAVE FAILED DURING THE PREPARE PROCESS.
 * IF SO WE WILL REFUSE THE PREPARE PHASE AND EXPECT A NEW PREPARE MESSAGE
 * WITH ALL FAILED NODES INCLUDED.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CLOSE_COMCONF              */
/*******************************/
void Qmgr::execCLOSE_COMCONF(Signal *signal) {
  jamEntry();

  CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];

  Uint32 requestType = closeCom->requestType;

  if (requestType == CloseComReqConf::RT_API_FAILURE) {
    jam();
    if (ERROR_INSERTED(945)) {
      if (arbitRec.code != ArbitCode::WinChoose) {
        // Delay API failure handling until arbitration in WinChoose
        sendSignalWithDelay(reference(), GSN_CLOSE_COMCONF, signal, 10,
                            signal->getLength());
        return;
      }
      CLEAR_ERROR_INSERT_VALUE;
    }
    handleApiCloseComConf(signal);
    return;
  }

  /* Normal node failure preparation path */
  ndbassert(requestType == CloseComReqConf::RT_NODE_FAILURE);
  BlockReference Tblockref = closeCom->xxxBlockRef;
  Uint16 TfailureNr = closeCom->failNo;

  if (TfailureNr != cprepareFailureNr) {
    /**
     * A new PREP_FAILREQ was already started, so ignore this
     * one, we will soon enough be here again for the new
     * failure and respond to this one instead. If we were to
     * send something, it would be ignored by President anyways.
     */
    jam();
    return;
  }

  UintR tprepFailConf = ZTRUE;

  /* Check whether the set of nodes which have had communications
   * closed is the same as the set of failed nodes.
   * If it is, we can confirm the PREP_FAIL phase for this set
   * of nodes to the President.
   * If it is not, we Refuse the PREP_FAIL phase for this set
   * of nodes, the President will start a new PREP_FAIL phase
   * for the new set.
   */
  if (!cprepFailedNodes.contains(cfailedNodes)) {
    /* Failed node(s) is missing from the set, we will not
     * confirm this Prepare_Fail phase.
     * Store the node id in the array for later.
     */
    jam();
    tprepFailConf = ZFALSE;
    cprepFailedNodes.bitOR(cfailedNodes);
  }  // if
  if (tprepFailConf == ZFALSE) {
    jam();
    /* Inform President that we cannot confirm the PREP_FAIL
     * phase as we are aware of at least one other node
     * failure
     */
    cfailedNodes = cprepFailedNodes;

    sendPrepFailReqRef(signal, Tblockref, GSN_PREP_FAILREF, reference(),
                       TfailureNr, cprepFailedNodes);
  } else {
    /* We have prepared the failure of the requested nodes
     * send confirmation to the president
     */
    jam();
    ccommitFailedNodes = cprepFailedNodes;

    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = TfailureNr;
    sendSignal(Tblockref, GSN_PREP_FAILCONF, signal, 2, JBA);
  }  // if
  return;
}  // Qmgr::execCLOSE_COMCONF()

/*---------------------------------------------------------------------------*/
/* WE HAVE RECEIVED A CONFIRM OF THAT THIS NODE HAVE PREPARED THE FAILURE.   */
/*---------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILCONF              */
/*******************************/
void Qmgr::execPREP_FAILCONF(Signal *signal) {
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];
  Uint16 TfailureNr = signal->theData[1];
  if (TfailureNr != cfailureNr) {
    jam();
    /**----------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES.
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }  // if
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendPrepFailReqStatus == Q_ACTIVE) {
        jamLine(nodePtr.i);
        return;
      }  // if
    }    // if
  }      // for
  /**
   * Check node count and groups and invoke arbitrator if necessary.
   * Continues via sendCommitFailReq() if successful.
   */
  arbitRec.failureNr = cfailureNr;
  const NodeState &s = getNodeState();
  if (s.startLevel == NodeState::SL_STOPPING_3 && s.stopping.systemShutdown) {
    jam();
    /**
     * We're performing a system shutdown,
     * don't let arbitrator shut us down
     */
    return;
  }

  switch (arbitRec.method) {
    case ArbitRec::DISABLED:
      jam();
      // No arbitration -> immediately commit the failed nodes
      sendCommitFailReq(signal);
      break;

    case ArbitRec::METHOD_EXTERNAL:
    case ArbitRec::METHOD_DEFAULT:
      jam();
      handleArbitCheck(signal);
      break;
  }
  return;
}  // Qmgr::execPREP_FAILCONF()

void Qmgr::sendCommitFailReq(Signal *signal) {
  NodeRecPtr nodePtr;
  jam();
  if (arbitRec.failureNr != cfailureNr) {
    jam();
    /**----------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES.
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }  // if
  /**-----------------------------------------------------------------------
   * WE HAVE SUCCESSFULLY PREPARED A SET OF NODE FAILURES. WE WILL NOW COMMIT
   * THESE NODE FAILURES.
   *-------------------------------------------------------------------------*/
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);

#ifdef ERROR_INSERT
    if (false && ERROR_INSERTED(935) && nodePtr.i == c_error_insert_extra) {
      g_eventLogger->info("skipping node %d", c_error_insert_extra);
      CLEAR_ERROR_INSERT_VALUE;
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
      continue;
    }
#endif

    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
      signal->theData[0] = cpdistref;
      signal->theData[1] = cfailureNr;
      sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2, JBA);
    }  // if
  }    // for
  ctoStatus = Q_ACTIVE;
  cfailedNodes.clear();
  return;
}  // sendCommitFailReq()

/*---------------------------------------------------------------------------*/
/* SOME NODE HAVE DISCOVERED A NODE FAILURE THAT WE HAVE NOT YET DISCOVERED. */
/* WE WILL START ANOTHER ROUND OF PREPARING A SET OF NODE FAILURES.          */
/*---------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILREF               */
/*******************************/
void Qmgr::execPREP_FAILREF(Signal *signal) {
  NodeRecPtr nodePtr;
  jamEntry();

  PrepFailReqRef *const prepFail = (PrepFailReqRef *)&signal->theData[0];

  Uint16 TfailureNr = prepFail->failNo;
  cprepFailedNodes.clear();

  if (signal->getNoOfSections() >= 1) {
    jam();
    Uint32 senderRef = signal->getSendersBlockRef();
    Uint32 senderVersion = getNodeInfo(refToNode(senderRef)).m_version;
    ndbrequire(ndbd_send_node_bitmask_in_section(senderVersion));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(cprepFailedNodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    jam();
    cprepFailedNodes.assign(NdbNodeBitmask48::Size, prepFail->theNodes);
  }
  ndbassert(prepFail->noOfNodes == cprepFailedNodes.count());

  if (TfailureNr != cfailureNr) {
    jam();
    /**---------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES.
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }  // if

  cfailedNodes = cprepFailedNodes;

  cfailureNr = cfailureNr + 1;
  // Failure number may not wrap
  ndbrequire(cfailureNr != 0);
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      sendPrepFailReq(signal, nodePtr.i);
    }  // if
  }    // for
  return;
}  // Qmgr::execPREP_FAILREF()

/*---------------------------------------------------------------------------*/
/*    THE PRESIDENT IS NOW COMMITTING THE PREVIOUSLY PREPARED NODE FAILURE.  */
/*---------------------------------------------------------------------------*/
/***********************/
/* COMMIT_FAILREQ     */
/***********************/
void Qmgr::execCOMMIT_FAILREQ(Signal *signal) {
  NodeRecPtr nodePtr;
  jamEntry();

  CRASH_INSERTION(935);

  BlockReference Tblockref = signal->theData[0];
  UintR TfailureNr = signal->theData[1];
  if (Tblockref != cpdistref) {
    jam();
    return;
  }  // if

  /**
   * Block commit until node failures has stabilized
   *
   * @See RT352
   */
  UnblockCommitOrd *const unblock = (UnblockCommitOrd *)&signal->theData[0];
  unblock->failNo = TfailureNr;
  EXECUTE_DIRECT(DBDIH, GSN_UNBLOCK_COMMIT_ORD, signal,
                 UnblockCommitOrd::SignalLength);

  if ((ccommitFailureNr != TfailureNr) && (!ccommitFailedNodes.isclear())) {
    jam();
    /**-----------------------------------------------------------------------
     * WE ONLY DO THIS PART OF THE COMMIT HANDLING THE FIRST TIME WE HEAR THIS
     * SIGNAL. WE CAN HEAR IT SEVERAL TIMES IF THE PRESIDENTS KEEP FAILING.
     *-----------------------------------------------------------------------*/
    ccommitFailureNr = TfailureNr;

    Uint32 nodeFailIndex = TfailureNr % MAX_DATA_NODE_FAILURES;
    NodeFailRec *TnodeFailRec = &nodeFailRec[nodeFailIndex];
    ndbrequire(TnodeFailRec->president == 0);
    TnodeFailRec->failureNr = TfailureNr;
    TnodeFailRec->president = cpresident;
    TnodeFailRec->nodes = ccommitFailedNodes;

    SyncThreadViaReqConf *syncReq = (SyncThreadViaReqConf *)&signal->theData[0];
    syncReq->senderRef = reference();
    syncReq->senderData = TfailureNr;
    syncReq->actionType = SyncThreadViaReqConf::FOR_NODE_FAILREP;
    sendSignal(TRPMAN_REF, GSN_SYNC_THREAD_VIA_REQ, signal,
               SyncThreadViaReqConf::SignalLength, JBA);

    /**--------------------------------------------------------------------
     * WE MUST PREPARE TO ACCEPT THE CRASHED NODE INTO THE CLUSTER AGAIN BY
     * SETTING UP CONNECTIONS AGAIN AFTER THREE SECONDS OF DELAY.
     *--------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      if (ccommitFailedNodes.get(nodePtr.i)) {
        jamLine(nodePtr.i);
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        nodePtr.p->phase = ZFAIL_CLOSING;
        DEB_STARTUP(("2: phase(%u) = ZFAIL_CLOSING", nodePtr.i));
        nodePtr.p->failState = WAITING_FOR_NDB_FAILCONF;
        set_hb_count(nodePtr.i) = 0;
        c_clusterNodes.clear(nodePtr.i);
      }  // if
    }    // for

    /*----------------------------------------------------------------------*/
    /*       WE INFORM THE API'S WE HAVE CONNECTED ABOUT THE FAILED NODES.  */
    /*----------------------------------------------------------------------*/
    LinearSectionPtr lsptr[3];
    lsptr->p = TnodeFailRec->nodes.rep.data;
    lsptr->sz = TnodeFailRec->nodes.getPackedLengthInWords();

    for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZAPI_ACTIVE) {
        jamLine(nodePtr.i);

        NodeFailRep *const nodeFail = (NodeFailRep *)&signal->theData[0];

        nodeFail->failNo = ccommitFailureNr;
        nodeFail->noOfNodes = ccommitFailedNodes.count();

        if (ndbd_send_node_bitmask_in_section(
                getNodeInfo(refToNode(nodePtr.p->blockRef)).m_version)) {
          sendSignal(nodePtr.p->blockRef, GSN_NODE_FAILREP, signal,
                     NodeFailRep::SignalLength, JBB, lsptr, 1);
        } else if (lsptr->sz <= NdbNodeBitmask48::Size) {
          TnodeFailRec->nodes.copyto(NdbNodeBitmask48::Size,
                                     nodeFail->theNodes);
          sendSignal(nodePtr.p->blockRef, GSN_NODE_FAILREP, signal,
                     NodeFailRep::SignalLength_v1, JBB);
        } else {
          ndbabort();
        }
      }  // if
    }    // for

    /**
     * Remove committed nodes from failed/prepared
     */
    cfailedNodes.bitANDC(ccommitFailedNodes);
    cprepFailedNodes.bitANDC(ccommitFailedNodes);
    ccommitFailedNodes.clear();
  }  // if
  /**-----------------------------------------------------------------------
   * WE WILL ALWAYS ACKNOWLEDGE THE COMMIT EVEN WHEN RECEIVING IT MULTIPLE
   * TIMES SINCE IT WILL ALWAYS COME FROM A NEW PRESIDENT.
   *------------------------------------------------------------------------*/
  signal->theData[0] = getOwnNodeId();
  sendSignal(Tblockref, GSN_COMMIT_FAILCONF, signal, 1, JBA);
  return;
}  // Qmgr::execCOMMIT_FAILREQ()

void Qmgr::execSYNC_THREAD_VIA_CONF(Signal *signal) {
  const SyncThreadViaReqConf *syncConf =
      (const SyncThreadViaReqConf *)&signal->theData[0];
  if (syncConf->actionType == SyncThreadViaReqConf::FOR_NODE_FAILREP) {
    jam();
    const Uint32 index = syncConf->senderData % MAX_DATA_NODE_FAILURES;
    NodeFailRec *TnodeFailRec = &nodeFailRec[index];
    ndbrequire(TnodeFailRec->president != 0);
    ndbrequire(TnodeFailRec->nodes.count() != 0);
    NodeFailRep *nodeFail = (NodeFailRep *)&signal->theData[0];
    nodeFail->failNo = TnodeFailRec->failureNr;
    nodeFail->masterNodeId = TnodeFailRec->president;
    nodeFail->noOfNodes = TnodeFailRec->nodes.count();

    LinearSectionPtr lsptr[3];
    lsptr->p = TnodeFailRec->nodes.rep.data;
    lsptr->sz = TnodeFailRec->nodes.getPackedLengthInWords();

    TnodeFailRec->president = 0;  // Mark entry as unused.

    if (ERROR_INSERTED(936)) {
      SectionHandle handle(this);
      ndbrequire(import(handle.m_ptr[0], lsptr[0].p, lsptr[0].sz));
      handle.m_cnt = 1;
      sendSignalWithDelay(NDBCNTR_REF, GSN_NODE_FAILREP, signal, 200,
                          NodeFailRep::SignalLength, &handle);
      releaseSections(handle);
    } else {
      sendSignal(NDBCNTR_REF, GSN_NODE_FAILREP, signal,
                 NodeFailRep::SignalLength, JBA, lsptr, 1);
    }
  } else if (syncConf->actionType ==
             SyncThreadViaReqConf::FOR_ACTIVATE_TRP_REQ) {
    jam();
    handle_activate_trp_req(signal, syncConf->senderData);
  } else {
    ndbabort();
  }
}

/*--------------------------------------------------------------------------*/
/* WE HAVE RECEIVED A CONFIRM OF THAT THIS NODE HAVE COMMITTED THE FAILURES.*/
/*--------------------------------------------------------------------------*/
/*******************************/
/* COMMIT_FAILCONF            */
/*******************************/
void Qmgr::execCOMMIT_FAILCONF(Signal *signal) {
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];

  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendCommitFailReqStatus == Q_ACTIVE) {
        jamLine(nodePtr.i);
        return;
      }  // if
    }    // if
  }      // for
  /*-----------------------------------------------------------------------*/
  /*   WE HAVE SUCCESSFULLY COMMITTED A SET OF NODE FAILURES.              */
  /*-----------------------------------------------------------------------*/
  ctoStatus = Q_NOT_ACTIVE;
  if (!cfailedNodes.isclear()) {
    jam();
    /**----------------------------------------------------------------------
     *	A FAILURE OCCURRED IN THE MIDDLE OF THE COMMIT PROCESS. WE ARE NOW
     *  READY TO START THE FAILED NODE PROCESS FOR THIS NODE.
     *----------------------------------------------------------------------*/
    cfailureNr = cfailureNr + 1;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jamLine(nodePtr.i);
        sendPrepFailReq(signal, nodePtr.i);
      }  // if
    }    // for
  }      // if
  return;
}  // Qmgr::execCOMMIT_FAILCONF()

/**--------------------------------------------------------------------------
 * IF THE PRESIDENT FAILS IN THE MIDDLE OF THE COMMIT OF A FAILED NODE THEN
 * THE NEW PRESIDENT NEEDS TO QUERY THE COMMIT STATUS IN THE RUNNING NODES.
 *---------------------------------------------------------------------------*/
/*******************************/
/* PRES_TOCONF                */
/*******************************/
void Qmgr::execPRES_TOCONF(Signal *signal) {
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];
  UintR TfailureNr = signal->theData[1];
  if (ctoFailureNr < TfailureNr) {
    jam();
    ctoFailureNr = TfailureNr;
  }  // if
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->sendPresToStatus == Q_ACTIVE) {
      jamLine(nodePtr.i);
      return;
    }  // if
  }    // for
  /*-------------------------------------------------------------------------*/
  /* WE ARE NOW READY TO DISCOVER WHETHER THE FAILURE WAS COMMITTED OR NOT.  */
  /*-------------------------------------------------------------------------*/
  if (ctoFailureNr > ccommitFailureNr) {
    jam();
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jamLine(nodePtr.i);
        nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
        signal->theData[0] = cpdistref;
        signal->theData[1] = ctoFailureNr;
        sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2, JBA);
      }  // if
    }    // for
    return;
  }  // if
  /*-------------------------------------------------------------------------*/
  /*       WE ARE NOW READY TO START THE NEW NODE FAILURE PROCESS.           */
  /*-------------------------------------------------------------------------*/
  ctoStatus = Q_NOT_ACTIVE;
  cfailureNr = cfailureNr + 1;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      sendPrepFailReq(signal, nodePtr.i);
    }  // if
  }    // for
  return;
}  // Qmgr::execPRES_TOCONF()

/*--------------------------------------------------------------------------*/
// Provide information about the configured NDB nodes in the system.
/*--------------------------------------------------------------------------*/
void Qmgr::execREAD_NODESREQ(Signal *signal) {
  jamEntry();

  ReadNodesReq *req = (ReadNodesReq *)&signal->theData[0];
  BlockReference TBref = req->myRef;
  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  NdbNodeBitmask tmp = c_definedNodes;
  tmp.bitANDC(c_clusterNodes);

  Uint32 packed_length1 = c_definedNodes.getPackedLengthInWords();
  Uint32 packed_length2 = c_clusterNodes.getPackedLengthInWords();
  Uint32 packed_length3 = tmp.getPackedLengthInWords();

  if (signal->length() >= ReadNodesReq::SignalLength) {
    jam();
    ReadNodesConf *const readNodes = (ReadNodesConf *)&signal->theData[0];

    readNodes->noOfNodes = c_definedNodes.count();
    readNodes->masterNodeId = cpresident;
    readNodes->ndynamicId = nodePtr.p->ndynamicId;

    readNodes->definedNodes = c_definedNodes;
    readNodes->clusterNodes = c_clusterNodes;
    readNodes->inactiveNodes = tmp;
    readNodes->startingNodes.clear();
    readNodes->startedNodes.clear();

    LinearSectionPtr lsptr[3];
    lsptr[0].p = readNodes->definedNodes.rep.data;
    lsptr[0].sz = 5 * NdbNodeBitmask::Size;
    sendSignal(TBref, GSN_READ_NODESCONF, signal, ReadNodesConf::SignalLength,
               JBB, lsptr, 1);
  } else if (packed_length1 <= NdbNodeBitmask48::Size &&
             packed_length2 <= NdbNodeBitmask48::Size &&
             packed_length3 <= NdbNodeBitmask48::Size) {
    jam();
    ReadNodesConf_v1 *const readNodes = (ReadNodesConf_v1 *)&signal->theData[0];
    readNodes->noOfNodes = c_definedNodes.count();
    readNodes->masterNodeId = cpresident;
    readNodes->ndynamicId = nodePtr.p->ndynamicId;

    c_definedNodes.copyto(NdbNodeBitmask::Size, readNodes->definedNodes);
    c_clusterNodes.copyto(NdbNodeBitmask::Size, readNodes->clusterNodes);
    tmp.copyto(NdbNodeBitmask::Size, readNodes->inactiveNodes);
    NdbNodeBitmask::clear(readNodes->startingNodes);
    NdbNodeBitmask::clear(readNodes->startedNodes);

    sendSignal(TBref, GSN_READ_NODESCONF, signal,
               ReadNodesConf_v1::SignalLength, JBB);
  } else {
    ndbabort();
  }
}  // Qmgr::execREAD_NODESREQ()

void Qmgr::systemErrorBecauseOtherNodeFailed(Signal *signal, Uint32 line,
                                             NodeId failedNodeId) {
  jam();

  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE,
             getOwnNodeId());

  char buf[100];
  BaseString::snprintf(
      buf, 100, "Node was shutdown during startup because node %d failed",
      failedNodeId);

  progError(line, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
}

void Qmgr::systemErrorLab(Signal *signal, Uint32 line, const char *message) {
  jam();
  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE,
             getOwnNodeId());

  // If it's known why shutdown occurred
  // an error message has been passed to this function
  progError(line, NDBD_EXIT_NDBREQUIRE, message);
}  // Qmgr::systemErrorLab()

/**---------------------------------------------------------------------------
 * A FAILURE HAVE BEEN DISCOVERED ON A NODE. WE NEED TO CLEAR A
 * NUMBER OF VARIABLES.
 *---------------------------------------------------------------------------*/
void Qmgr::failReport(Signal *signal, Uint16 aFailedNode, UintR aSendFailRep,
                      FailRep::FailCause aFailCause, Uint16 sourceNode) {
  UintR tfrMinDynamicId;
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;
  NodeRecPtr presidentNodePtr;

  ndbassert((!aSendFailRep) || (sourceNode != 0));

  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  if (failedNodePtr.p->phase == ZRUNNING) {
    jam();

#ifdef ERROR_INSERT
    if (ERROR_INSERTED(938)) {
      nodeFailCount++;
      g_eventLogger->info(
          "QMGR : execFAIL_REP(Failed : %u Source : %u  Cause : %u) : "
          "%u nodes have failed",
          aFailedNode, sourceNode, aFailCause, nodeFailCount);
      /* Count DB nodes */
      Uint32 nodeCount = 0;
      for (Uint32 i = 1; i < MAX_NDB_NODES; i++) {
        if (getNodeInfo(i).getType() == NODE_TYPE_DB) nodeCount++;
      }

      /* When > 25% of cluster has failed, resume communications */
      if (nodeFailCount > (nodeCount / 4)) {
        g_eventLogger->info(
            "QMGR : execFAIL_REP > 25%% nodes failed, resuming comms");
        Signal save = *signal;
        signal->theData[0] = 9991;
        sendSignal(CMVMI_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);
        *signal = save;
        nodeFailCount = 0;
        SET_ERROR_INSERT_VALUE(932);
      }
    }
#endif

    /* WE ALSO NEED TO ADD HERE SOME CODE THAT GETS OUR NEW NEIGHBOURS. */
    if (cpresident == getOwnNodeId()) {
      jam();
      if (failedNodePtr.p->sendCommitFailReqStatus == Q_ACTIVE) {
        jam();
        signal->theData[0] = failedNodePtr.i;
        sendSignal(QMGR_REF, GSN_COMMIT_FAILCONF, signal, 1, JBA);
      }  // if
      if (failedNodePtr.p->sendPresToStatus == Q_ACTIVE) {
        jam();
        signal->theData[0] = failedNodePtr.i;
        signal->theData[1] = ccommitFailureNr;
        sendSignal(QMGR_REF, GSN_PRES_TOCONF, signal, 2, JBA);
      }  // if
    }    // if
    DEB_STARTUP(("phase(%u) = ZPREPARE_FAIL", failedNodePtr.i));
    failedNodePtr.p->phase = ZPREPARE_FAIL;
    failedNodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    set_hb_count(failedNodePtr.i) = 0;
    if (aSendFailRep == ZTRUE) {
      jam();
      if (failedNodePtr.i != getOwnNodeId()) {
        jam();
        FailRep *const failRep = (FailRep *)&signal->theData[0];
        failRep->failNodeId = failedNodePtr.i;
        failRep->failCause = aFailCause;
        failRep->failSourceNodeId = sourceNode;
        sendSignal(failedNodePtr.p->blockRef, GSN_FAIL_REP, signal,
                   FailRep::SignalLength, JBA);
      }  // if
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          jamLine(nodePtr.i);
          FailRep *const failRep = (FailRep *)&signal->theData[0];
          failRep->failNodeId = failedNodePtr.i;
          failRep->failCause = aFailCause;
          failRep->failSourceNodeId = sourceNode;
          sendSignal(nodePtr.p->blockRef, GSN_FAIL_REP, signal,
                     FailRep::SignalLength, JBA);
        }  // if
      }    // for
    }      // if
    if (failedNodePtr.i == getOwnNodeId()) {
      jam();
      return;
    }  // if

    if (unlikely(m_connectivity_check.reportNodeFailure(failedNodePtr.i))) {
      jam();
      connectivityCheckCompleted(signal);
    }

    failedNodePtr.p->ndynamicId = 0;
    findNeighbours(signal, __LINE__);
    if (failedNodePtr.i == cpresident) {
      jam();
      /**--------------------------------------------------------------------
       * IF PRESIDENT HAVE FAILED WE MUST CALCULATE THE NEW PRESIDENT BY
       * FINDING THE NODE WITH THE MINIMUM DYNAMIC IDENTITY.
       *---------------------------------------------------------------------*/
      tfrMinDynamicId = (UintR)-1;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          jamLine(nodePtr.i);
          if ((nodePtr.p->ndynamicId & 0xFFFF) < tfrMinDynamicId) {
            jam();
            tfrMinDynamicId = (nodePtr.p->ndynamicId & 0xFFFF);
            cpresident = nodePtr.i;
          }  // if
        }    // if
      }      // for
      presidentNodePtr.i = cpresident;
      ptrCheckGuard(presidentNodePtr, MAX_NDB_NODES, nodeRec);
      cpdistref = presidentNodePtr.p->blockRef;
      if (cpresident == getOwnNodeId()) {
        CRASH_INSERTION(920);
        cfailureNr = cprepareFailureNr;
        ctoFailureNr = 0;
        ctoStatus = Q_ACTIVE;
        DEB_STARTUP(("2:Clear c_start.m_startNode"));
        c_start.reset();  // Don't take over nodes being started
        if (!ccommitFailedNodes.isclear()) {
          jam();
          /**-----------------------------------------------------------------
           * IN THIS SITUATION WE ARE UNCERTAIN OF WHETHER THE NODE FAILURE
           * PROCESS WAS COMMITTED. WE NEED TO QUERY THE OTHER NODES ABOUT
           * THEIR STATUS.
           *-----------------------------------------------------------------*/
          for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
            jam();
            ptrAss(nodePtr, nodeRec);
            if (nodePtr.p->phase == ZRUNNING) {
              jam();
              nodePtr.p->sendPresToStatus = Q_ACTIVE;
              signal->theData[0] = cpdistref;
              signal->theData[1] = cprepareFailureNr;
              sendSignal(nodePtr.p->blockRef, GSN_PRES_TOREQ, signal, 1, JBA);
            }  // if
          }    // for
        } else {
          jam();
          /*-----------------------------------------------------------------*/
          // In this case it could be that a commit process is still ongoing.
          // If so we must conclude it as the new master.
          /*-----------------------------------------------------------------*/
          for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
            ptrAss(nodePtr, nodeRec);
            if (nodePtr.p->phase == ZRUNNING) {
              jamLine(nodePtr.i);
              nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
              signal->theData[0] = cpdistref;
              signal->theData[1] = ccommitFailureNr;
              sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2,
                         JBA);
            }  // if
          }    // for
        }      // if
      }        // if
    }          // if
    cfailedNodes.set(failedNodePtr.i);
  }  // if
}  // Qmgr::failReport()

/*---------------------------------------------------------------------------*/
/*       INPUT:  TTDI_DYN_ID                                                 */
/*       OUTPUT: TTDI_NODE_ID                                                */
/*---------------------------------------------------------------------------*/
Uint16 Qmgr::translateDynamicIdToNodeId(Signal *signal, UintR TdynamicId) {
  NodeRecPtr tdiNodePtr;
  Uint16 TtdiNodeId = ZNIL;

  for (tdiNodePtr.i = 1; tdiNodePtr.i < MAX_NDB_NODES; tdiNodePtr.i++) {
    jam();
    ptrAss(tdiNodePtr, nodeRec);
    if (tdiNodePtr.p->ndynamicId == TdynamicId) {
      jam();
      TtdiNodeId = tdiNodePtr.i;
      break;
    }  // if
  }    // for
  if (TtdiNodeId == ZNIL) {
    jam();
    systemErrorLab(signal, __LINE__);
  }  // if
  return TtdiNodeId;
}  // Qmgr::translateDynamicIdToNodeId()

/**--------------------------------------------------------------------------
 *       WHEN RECEIVING PREPARE FAILURE REQUEST WE WILL IMMEDIATELY CLOSE
 *       COMMUNICATION WITH ALL THOSE NODES.
 *--------------------------------------------------------------------------*/
void Qmgr::sendCloseComReq(Signal *signal, BlockReference TBRef,
                           Uint16 aFailNo) {
  jam();
  CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];

  closeCom->xxxBlockRef = TBRef;
  closeCom->requestType = CloseComReqConf::RT_NODE_FAILURE;
  closeCom->failNo = aFailNo;
  closeCom->noOfNodes = cprepFailedNodes.count();
  {
    closeCom->failedNodeId = 0; /* Indicates we're sending bitmask */
    LinearSectionPtr lsptr[3];
    lsptr[0].p = cprepFailedNodes.rep.data;
    lsptr[0].sz = cprepFailedNodes.getPackedLengthInWords();
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
               CloseComReqConf::SignalLength, JBB, lsptr, 1);
  }

}  // Qmgr::sendCloseComReq()

void Qmgr::sendPrepFailReqRef(Signal *signal, Uint32 dstBlockRef,
                              GlobalSignalNumber gsn, Uint32 blockRef,
                              Uint32 failNo, const NdbNodeBitmask &nodes) {
  PrepFailReqRef *const prepFail = (PrepFailReqRef *)&signal->theData[0];
  prepFail->xxxBlockRef = blockRef;
  prepFail->failNo = failNo;
  prepFail->noOfNodes = nodes.count();
  Uint32 packed_length = nodes.getPackedLengthInWords();

  if (ndbd_send_node_bitmask_in_section(
          getNodeInfo(refToNode(dstBlockRef)).m_version)) {
    Uint32 *temp_failed_nodes = &signal->theData[PrepFailReqRef::SignalLength];
    nodes.copyto(NdbNodeBitmask::Size, temp_failed_nodes);
    LinearSectionPtr lsptr[3];
    lsptr[0].p = temp_failed_nodes;
    lsptr[0].sz = packed_length;
    sendSignal(dstBlockRef, gsn, signal, PrepFailReqRef::SignalLength, JBA,
               lsptr, 1);
  } else if (packed_length <= NdbNodeBitmask48::Size) {
    nodes.copyto(NdbNodeBitmask48::Size, prepFail->theNodes);
    sendSignal(dstBlockRef, gsn, signal, PrepFailReqRef::SignalLength_v1, JBA);
  } else {
    ndbabort();
  }
}

/**--------------------------------------------------------------------------
 *       SEND PREPARE FAIL REQUEST FROM PRESIDENT.
 *---------------------------------------------------------------------------*/
void Qmgr::sendPrepFailReq(Signal *signal, Uint16 aNode) {
  NodeRecPtr sendNodePtr;
  sendNodePtr.i = aNode;
  ptrCheckGuard(sendNodePtr, MAX_NDB_NODES, nodeRec);
  sendNodePtr.p->sendPrepFailReqStatus = Q_ACTIVE;

  sendPrepFailReqRef(signal, sendNodePtr.p->blockRef, GSN_PREP_FAILREQ,
                     reference(), cfailureNr, cfailedNodes);
}  // Qmgr::sendPrepFailReq()

/**
 * Arbitration module.  Rest of QMGR calls us only via
 * the "handle" routines.
 */

/**
 * Config signals are logically part of CM_REG.
 */
void Qmgr::execARBIT_CFG(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  unsigned rank = sd->code;
  ndbrequire(1 <= rank && rank <= 2);
  arbitRec.apiMask[0].bitOR(sd->mask);
  arbitRec.apiMask[rank].assign(sd->mask);
}

/**
 * ContinueB delay (0=JBA 1=JBB)
 */
Uint32 Qmgr::getArbitDelay() {
  switch (arbitRec.state) {
    case ARBIT_NULL:
      jam();
      break;
    case ARBIT_INIT:
      jam();
      return 100;
    case ARBIT_FIND:
      jam();
      return 100;
    case ARBIT_PREP1:
      jam();
      return 100;
    case ARBIT_PREP2:
      jam();
      return 100;
    case ARBIT_START:
      jam();
      return 100;
    case ARBIT_RUN:
      jam();
      return 1000;
    case ARBIT_CHOOSE:
      jam();
      return 10;
    case ARBIT_CRASH:  // if we could wait
      jam();
      return 100;
  }
  ndbabort();
  return (Uint32)-1;
}

/**
 * Time to wait for reply.  There is only 1 config parameter
 * (timeout for CHOOSE).  XXX The rest are guesses.
 */
Uint32 Qmgr::getArbitTimeout() {
  switch (arbitRec.state) {
    case ARBIT_NULL:
      jam();
      break;
    case ARBIT_INIT:  // not used
      jam();
      [[fallthrough]];
    case ARBIT_FIND:
      jam();
      /* This timeout will be used only to print out a warning
       * when a suitable arbitrator is not found.
       */
      return 60000;
    case ARBIT_PREP1:
      jam();
      [[fallthrough]];
    case ARBIT_PREP2:
      jam();
      return 1000 + cnoOfNodes * Uint32(hb_send_timer.getDelay());
    case ARBIT_START:
      jam();
      return 1000 + arbitRec.timeout;
    case ARBIT_RUN:  // not used (yet)
      jam();
      return 1000;
    case ARBIT_CHOOSE:
      jam();
      return arbitRec.timeout;
    case ARBIT_CRASH:  // if we could wait
      jam();
      return 100;
  }
  ndbabort();
  return (Uint32)-1;
}

/**
 * Start arbitration thread when we are president and database
 * is opened for the first time.
 *
 * XXX  Do arbitration check just like on node failure.  Since
 * there is no arbitrator yet, must win on counts alone.
 */
void Qmgr::handleArbitStart(Signal *signal) {
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  ndbrequire(arbitRec.state == ARBIT_NULL);
  arbitRec.state = ARBIT_INIT;
  DEB_ARBIT(("Arbit state = ARBIT_INIT from NULL"));
  arbitRec.newstate = true;
  startArbitThread(signal);
}

/**
 * Handle API node failure.  Called also by non-president nodes.
 * If we are president go back to INIT state, otherwise to NULL.
 * Start new thread to save time.
 */
void Qmgr::handleArbitApiFail(Signal *signal, Uint16 nodeId) {
  if (arbitRec.node != nodeId) {
    jam();
    return;
  }
  reportArbitEvent(signal, NDB_LE_ArbitState);
  arbitRec.node = 0;
  switch (arbitRec.state) {
    case ARBIT_NULL:  // should not happen
      jam();
      break;
    case ARBIT_INIT:
      jam();
      break;
    case ARBIT_FIND:
      jam();
      break;
    case ARBIT_PREP1:  // start from beginning
      jam();
      [[fallthrough]];
    case ARBIT_PREP2:
      jam();
      [[fallthrough]];
    case ARBIT_START:
      jam();
      [[fallthrough]];
    case ARBIT_RUN:
      if (cpresident == getOwnNodeId()) {
        jam();
        arbitRec.state = ARBIT_INIT;
        DEB_ARBIT(("Arbit state = ARBIT_INIT from RUN"));
        arbitRec.newstate = true;
        startArbitThread(signal);
      } else {
        jam();
        arbitRec.state = ARBIT_NULL;
        DEB_ARBIT(("Arbit state = ARBIT_NULL from RUN"));
      }
      break;
    case ARBIT_CHOOSE:  // XXX too late
      jam();
      break;
    case ARBIT_CRASH:
      jam();
      break;
    default:
      ndbabort();
  }
}

/**
 * Handle NDB node add.  Ignore if arbitration thread not yet
 * started.  If PREP is not ready, go back to INIT.  Otherwise
 * the new node gets arbitrator and ticket once we reach RUN state.
 * Start new thread to save time.
 */
void Qmgr::handleArbitNdbAdd(Signal *signal, Uint16 nodeId) {
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  switch (arbitRec.state) {
    case ARBIT_NULL:  // before db opened
      jam();
      break;
    case ARBIT_INIT:  // start from beginning
      jam();
      [[fallthrough]];
    case ARBIT_FIND:
      jam();
      [[fallthrough]];
    case ARBIT_PREP1:
      jam();
      [[fallthrough]];
    case ARBIT_PREP2:
      jam();
      arbitRec.state = ARBIT_INIT;
      DEB_ARBIT(("Arbit state = ARBIT_INIT from PREP2"));
      arbitRec.newstate = true;
      startArbitThread(signal);
      break;
    case ARBIT_START:  // process in RUN state
      jam();
      [[fallthrough]];
    case ARBIT_RUN:
      jam();
      arbitRec.newMask.set(nodeId);
      break;
    case ARBIT_CHOOSE:  // XXX too late
      jam();
      break;
    case ARBIT_CRASH:
      jam();
      break;
    default:
      ndbabort();
  }
}

/**
 * Check if current nodeset can survive.  The decision is
 * based on node count, node groups, and on external arbitrator
 * (if we have one).  Always starts a new thread because
 * 1) CHOOSE cannot wait 2) if we are new president we need
 * a thread 3) if we are old president it does no harm.
 *
 * The following logic governs if we will survive or not.
 * 1) If at least one node group is fully dead then we will not survive.
 * 2) If 1) is false AND at least one group is fully alive then we will
 *    survive.
 * 3) If 1) AND 2) is false AND a majority of the previously alive nodes are
 *    dead then we will not survive.
 * 4) If 1) AND 2) AND 3) is false AND a majority of the previously alive
 *    nodes are still alive, then we will survive.
 * 5) If 1) AND 2) AND 3) AND 4) is false then exactly half of the previously
 *    alive nodes are dead and the other half is alive. In this case we will
 *    ask the arbitrator whether we can continue or not. If no arbitrator is
 *    currently selected then we will fail. If an arbitrator exists then it
 *    will respond with either WIN in which case our part of the cluster will
 *    remain alive and LOSE in which case our part of the cluster will not
 *    survive.
 *
 * The number of previously alive nodes are the sum of the currently alive
 * nodes plus the number of nodes currently forming a node set that will
 * die. All other nodes was dead in a previous node fail transaction and are
 * not counted in the number of previously alive nodes.
 */
void Qmgr::handleArbitCheck(Signal *signal) {
  jam();
  Uint32 prev_alive_nodes = count_previously_alive_nodes();
  ndbrequire(cpresident == getOwnNodeId());
  NdbNodeBitmask survivorNodes;
  NdbNodeBitmask beforeFailureNodes;
  /**
   * computeArbitNdbMask will only count nodes in the state ZRUNNING, crashed
   * nodes are thus not part of this set of nodes. The method
   * count_previously_alive_nodes counts both nodes in ZRUNNING and in
   * ZPREPARE_FAIL but deducts those that was previously not started to ensure
   * that we don't rely on non-started nodes in our check for whether
   * arbitration is required.
   */
  computeArbitNdbMask(survivorNodes);
  computeBeforeFailNdbMask(beforeFailureNodes);
  {
    jam();
    CheckNodeGroups *sd = (CheckNodeGroups *)&signal->theData[0];
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck |
                      CheckNodeGroups::UseBeforeFailMask;
    sd->mask = survivorNodes;
    sd->before_fail_mask = beforeFailureNodes;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                   CheckNodeGroups::SignalLengthArbitCheckLong);
    jamEntry();
    if (ERROR_INSERTED(943)) {
      ndbout << "Requiring arbitration, even if there is no"
             << " possible split." << endl;
      sd->output = CheckNodeGroups::Partitioning;
      DEB_ARBIT(("Arbit state = ARBIT_RUN in 943"));
      arbitRec.state = ARBIT_RUN;
    }
    switch (sd->output) {
      case CheckNodeGroups::Win:
        jam();
        arbitRec.code = ArbitCode::WinGroups;
        break;
      case CheckNodeGroups::Lose:
        jam();
        arbitRec.code = ArbitCode::LoseGroups;
        break;
      case CheckNodeGroups::Partitioning:
        jam();
        arbitRec.code = ArbitCode::Partitioning;
        if (2 * survivorNodes.count() > prev_alive_nodes) {
          /**
           * We have lost nodes in all node groups so we are in a
           * potentially partitioned state. If we have the majority
           * of the nodes in this partition we will definitely
           * survive.
           */
          jam();
          arbitRec.code = ArbitCode::WinNodes;
        } else if (2 * survivorNodes.count() < prev_alive_nodes) {
          jam();
          /**
           * More than half of the live nodes failed and nodes from
           * all node groups failed, we are definitely in a losing
           * streak and we will be part of the failing side. Time
           * to crash.
           */
          arbitRec.code = ArbitCode::LoseNodes;
        } else {
          jam();
          /**
           * Half of the live nodes failed, we can be in a partitioned
           * state, use the arbitrator to decide what to do next.
           */
        }
        break;
      default:
        ndbabort();
    }
  }
  switch (arbitRec.code) {
    case ArbitCode::LoseNodes:
      jam();
      goto crashme;
    case ArbitCode::LoseGroups:
      jam();
      goto crashme;
    case ArbitCode::WinNodes:
      jam();
      [[fallthrough]];
    case ArbitCode::WinGroups:
      jam();
      if (arbitRec.state == ARBIT_RUN) {
        jam();
        break;
      }
      arbitRec.state = ARBIT_INIT;
      DEB_ARBIT(("Arbit state = ARBIT_INIT from non-RUN WinGroups"));
      arbitRec.newstate = true;
      break;
    case ArbitCode::Partitioning:
      if (arbitRec.state == ARBIT_RUN) {
        jam();
        arbitRec.state = ARBIT_CHOOSE;
        DEB_ARBIT(("Arbit state = ARBIT_CHOOSE from RUN"));
        arbitRec.newstate = true;
        break;
      }
      if (arbitRec.apiMask[0].count() != 0) {
        jam();
        arbitRec.code = ArbitCode::LoseNorun;
      } else {
        jam();
        arbitRec.code = ArbitCode::LoseNocfg;
      }
      goto crashme;
    default:
    crashme:
      jam();
      arbitRec.state = ARBIT_CRASH;
      DEB_ARBIT(("Arbit state = ARBIT_CRASH"));
      arbitRec.newstate = true;
      break;
  }
  reportArbitEvent(signal, NDB_LE_ArbitResult);
  switch (arbitRec.state) {
    default:
      jam();
      arbitRec.newMask.bitAND(survivorNodes);  // delete failed nodes
      arbitRec.recvMask.bitAND(survivorNodes);
      sendCommitFailReq(signal);  // start commit of failed nodes
      break;
    case ARBIT_CHOOSE:
      jam();
      break;
    case ARBIT_CRASH:
      jam();
      break;
  }
  startArbitThread(signal);
}

/**
 * Start a new continueB thread.  The thread id is incremented
 * so that any old thread will exit.
 */
void Qmgr::startArbitThread(Signal *signal) {
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  arbitRec.code = ArbitCode::ThreadStart;
  reportArbitEvent(signal, NDB_LE_ArbitState);
  signal->theData[1] = ++arbitRec.thread;
  runArbitThread(signal);
}

/**
 * Handle arbitration thread.  The initial thread normally ends
 * up in RUN state.  New thread can be started to save time.
 */
void Qmgr::runArbitThread(Signal *signal) {
#ifdef DEBUG_ARBIT
  char buf[256];
  NdbNodeBitmask ndbMask;
  char maskbuf[NdbNodeBitmask::TextLength + 1];
  computeArbitNdbMask(ndbMask);
  ndbout << "arbit thread:";
  ndbout << " state=" << arbitRec.state;
  ndbout << " newstate=" << arbitRec.newstate;
  ndbout << " thread=" << arbitRec.thread;
  ndbout << " node=" << arbitRec.node;
  arbitRec.ticket.getText(buf, sizeof(buf));
  ndbout << " ticket=" << buf;
  ndbMask.getText(maskbuf);
  ndbout << " ndbmask=" << maskbuf;
  ndbout << " sendcount=" << arbitRec.sendCount;
  ndbout << " recvcount=" << arbitRec.recvCount;
  arbitRec.recvMask.getText(maskbuf);
  ndbout << " recvmask=" << maskbuf;
  ndbout << " code=" << arbitRec.code;
  ndbout << endl;
#endif
  if (signal->theData[1] != arbitRec.thread) {
    jam();
    return;  // old thread dies
  }
  switch (arbitRec.state) {
    case ARBIT_INIT:  // main thread
      jam();
      stateArbitInit(signal);
      break;
    case ARBIT_FIND:
      jam();
      stateArbitFind(signal);
      break;
    case ARBIT_PREP1:
      jam();
      [[fallthrough]];
    case ARBIT_PREP2:
      jam();
      stateArbitPrep(signal);
      break;
    case ARBIT_START:
      jam();
      stateArbitStart(signal);
      break;
    case ARBIT_RUN:
      jam();
      stateArbitRun(signal);
      break;
    case ARBIT_CHOOSE:  // partitition thread
      jam();
      if (ERROR_INSERTED(945) && arbitRec.code == ArbitCode::WinChoose) {
        // Delay ARBIT_CHOOSE until NdbAPI node is disconnected
        break;
      }
      stateArbitChoose(signal);
      break;
    case ARBIT_CRASH:
      jam();
      stateArbitCrash(signal);
      break;
    default:
      ndbabort();
  }
  signal->theData[0] = ZARBIT_HANDLING;
  signal->theData[1] = arbitRec.thread;
  signal->theData[2] = arbitRec.state;  // just for signal log
  Uint32 delay = getArbitDelay();
  if (delay == 0) {
    jam();
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBA);
  } else if (delay == 1) {
    jam();
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBB);
  } else {
    jam();
    sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, delay, 3);
  }  // if
}

/**
 * Handle INIT state.  Generate next ticket.  Switch to FIND
 * state without delay.
 */
void Qmgr::stateArbitInit(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.node = 0;
    arbitRec.ticket.update();
    arbitRec.newMask.clear();
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  arbitRec.setTimestamp();  // Init arbitration timer
  arbitRec.state = ARBIT_FIND;
  DEB_ARBIT(("Arbit state = ARBIT_FIND"));
  arbitRec.newstate = true;
  stateArbitFind(signal);
}

/**
 * Handle FIND state.  Find first arbitrator which is alive
 * and invoke PREP state without delay.  If none are found,
 * loop in FIND state.  This is forever if no arbitrators
 * are configured (not the normal case).
 *
 * XXX  Add adaptive behaviour to avoid getting stuck on API
 * nodes which are alive but do not respond or die too soon.
 */
void Qmgr::stateArbitFind(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch (arbitRec.method) {
    case ArbitRec::METHOD_EXTERNAL: {
      // Don't select any API node as arbitrator
      arbitRec.node = 0;
      arbitRec.state = ARBIT_PREP1;
      DEB_ARBIT(("Arbit state = ARBIT_PREP1"));
      arbitRec.newstate = true;
      stateArbitPrep(signal);
      return;
      break;
    }

    case ArbitRec::METHOD_DEFAULT: {
      NodeRecPtr aPtr;
      // Select the best available API node as arbitrator
      for (unsigned rank = 1; rank <= 2; rank++) {
        jam();
        aPtr.i = 0;
        const unsigned stop = NodeBitmask::NotFound;
        while ((aPtr.i = arbitRec.apiMask[rank].find(aPtr.i + 1)) != stop) {
          jam();
          ptrAss(aPtr, nodeRec);
          if (aPtr.p->phase != ZAPI_ACTIVE) continue;
          ndbrequire(c_connectedNodes.get(aPtr.i));
          arbitRec.node = aPtr.i;
          arbitRec.state = ARBIT_PREP1;
          DEB_ARBIT(("2:Arbit state = ARBIT_PREP1"));
          arbitRec.newstate = true;
          stateArbitPrep(signal);
          return;
        }
      }

      /* If the president cannot find a suitable arbitrator then
       * it will report this once a minute. Success in finding
       * an arbitrator will be notified when the arbitrator
       * accepts and acks the offer.
       */

      if (arbitRec.getTimediff() > getArbitTimeout()) {
        jam();
        g_eventLogger->warning(
            "Could not find an arbitrator, cluster is not partition-safe");
        warningEvent(
            "Could not find an arbitrator, cluster is not partition-safe");
        arbitRec.setTimestamp();
      }
      return;
      break;
    }

    default:
      ndbabort();
  }
}

/**
 * Handle PREP states.  First round nulls any existing tickets.
 * Second round sends new ticket.  When all confirms have been
 * received invoke START state immediately.
 */
void Qmgr::stateArbitPrep(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;                  // send all at once
    computeArbitNdbMask(arbitRec.recvMask);  // to send and recv
    arbitRec.recvMask.clear(getOwnNodeId());
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  if (!arbitRec.sendCount) {
    jam();
    NodeRecPtr aPtr;
    aPtr.i = 0;
    const unsigned stop = NodeBitmask::NotFound;
    while ((aPtr.i = arbitRec.recvMask.find(aPtr.i + 1)) != stop) {
      jam();
      ptrAss(aPtr, nodeRec);
      ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
      sd->sender = getOwnNodeId();
      if (arbitRec.state == ARBIT_PREP1) {
        jam();
        sd->code = ArbitCode::PrepPart1;
      } else {
        jam();
        sd->code = ArbitCode::PrepPart2;
      }
      sd->node = arbitRec.node;
      sd->ticket = arbitRec.ticket;
      sd->mask.clear();
      sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPREQ, signal,
                 ArbitSignalData::SignalLength, JBB);
    }
    arbitRec.setTimestamp();  // send time
    arbitRec.sendCount = 1;
    return;
  }
  if (arbitRec.code != 0) {  // error
    jam();
    arbitRec.state = ARBIT_INIT;
    DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitPrep"));
    arbitRec.newstate = true;
    return;
  }
  if (arbitRec.recvMask.count() == 0) {  // recv all
    if (arbitRec.state == ARBIT_PREP1) {
      jam();
      DEB_ARBIT(("Arbit state = ARBIT_PREP2 stateArbitPrep"));
      arbitRec.state = ARBIT_PREP2;
      arbitRec.newstate = true;
    } else {
      jam();
      DEB_ARBIT(("Arbit state = ARBIT_START stateArbitPrep"));
      arbitRec.state = ARBIT_START;
      arbitRec.newstate = true;
      stateArbitStart(signal);
    }
    return;
  }
  if (arbitRec.getTimediff() > getArbitTimeout()) {
    jam();
    arbitRec.state = ARBIT_INIT;
    DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitPrep"));
    arbitRec.newstate = true;
    return;
  }
}

void Qmgr::execARBIT_PREPREQ(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (getOwnNodeId() == cpresident) {
    jam();
    return;  // wrong state
  }
  if (sd->sender != cpresident) {
    jam();
    return;  // wrong state
  }
  NodeRecPtr aPtr;
  aPtr.i = sd->sender;
  ptrAss(aPtr, nodeRec);
  switch (sd->code) {
    case ArbitCode::PrepPart1:  // zero them just to be sure
      jam();
      arbitRec.node = 0;
      arbitRec.ticket.clear();
      break;
    case ArbitCode::PrepPart2:  // non-president enters RUN state
      jam();
      [[fallthrough]];
    case ArbitCode::PrepAtrun:
      jam();
      arbitRec.node = sd->node;
      arbitRec.ticket = sd->ticket;
      arbitRec.code = sd->code;
      reportArbitEvent(signal, NDB_LE_ArbitState);
      arbitRec.state = ARBIT_RUN;
      arbitRec.newstate = true;
      DEB_ARBIT(("Arbit state = ARBIT_RUN PrepAtRun"));

      // Non-president node logs.
      if (!c_connectedNodes.get(arbitRec.node)) {
        char buf[20];  // needs 16 + 1 for '\0'
        arbitRec.ticket.getText(buf, sizeof(buf));
        g_eventLogger->warning(
            "President %u proposed disconnected "
            "node %u as arbitrator [ticket=%s]. "
            "Cluster may be partially connected. "
            "Connected nodes: %s",
            cpresident, arbitRec.node, buf,
            BaseString::getPrettyTextShort(c_connectedNodes).c_str());

        warningEvent(
            "President %u proposed disconnected node %u "
            "as arbitrator [ticket %s]",
            cpresident, arbitRec.node, buf);
        warningEvent("Cluster may be partially connected. Connected nodes: ");

        // Split the connected-node list, since warningEvents are
        // limited to ~24 words / 96 chars
        BaseString tmp(
            BaseString::getPrettyTextShort(c_connectedNodes).c_str());
        Vector<BaseString> split;
        tmp.split(split, "", 92);
        for (unsigned i = 0; i < split.size(); ++i) {
          warningEvent("%s", split[i].c_str());
        }
      }

      if (sd->code == ArbitCode::PrepAtrun) {
        jam();
        return;
      }
      break;
    default:
      jam();
      ndbabort();
  }
  sd->sender = getOwnNodeId();
  sd->code = 0;
  sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPCONF, signal,
             ArbitSignalData::SignalLength, JBB);
}

void Qmgr::execARBIT_PREPCONF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (!arbitRec.match(sd)) {
    jam();
    return;  // stray signal
  }
  if (arbitRec.state != ARBIT_PREP1 && arbitRec.state != ARBIT_PREP2) {
    jam();
    return;  // wrong state
  }
  if (!arbitRec.recvMask.get(sd->sender)) {
    jam();
    return;  // wrong state
  }
  arbitRec.recvMask.clear(sd->sender);
  if (arbitRec.code == 0 && sd->code != 0) {
    jam();
    arbitRec.code = sd->code;
  }  // if
}

void Qmgr::execARBIT_PREPREF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_PREPCONF(signal);
}

/**
 * Handle START state.  On first call send start request to
 * the chosen arbitrator.  Then wait for a CONF.
 */
void Qmgr::stateArbitStart(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;
    arbitRec.recvCount = 0;
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch (arbitRec.method) {
    case ArbitRec::METHOD_EXTERNAL:
      jam();
      ndbrequire(arbitRec.node == 0);  // No arbitrator selected

      // Don't start arbitrator in API node => ARBIT_RUN
      arbitRec.state = ARBIT_RUN;
      DEB_ARBIT(("Arbit state = ARBIT_RUN stateArbitStart"));
      arbitRec.newstate = true;
      return;
      break;

    case ArbitRec::METHOD_DEFAULT:
      if (!arbitRec.sendCount) {
        jam();
        BlockReference blockRef = calcApiClusterMgrBlockRef(arbitRec.node);
        ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
        sd->sender = getOwnNodeId();
        sd->code = 0;
        sd->node = arbitRec.node;
        sd->ticket = arbitRec.ticket;
        sd->mask.clear();
        sendSignal(blockRef, GSN_ARBIT_STARTREQ, signal,
                   ArbitSignalData::SignalLength, JBB);
        arbitRec.sendCount = 1;
        arbitRec.setTimestamp();  // send time
        return;
      }
      if (arbitRec.recvCount) {
        jam();
        reportArbitEvent(signal, NDB_LE_ArbitState);
        if (arbitRec.code == ArbitCode::ApiStart) {
          jam();
          arbitRec.state = ARBIT_RUN;
          DEB_ARBIT(("Arbit state = ARBIT_RUN stateArbitStart:Default"));
          arbitRec.newstate = true;
          return;
        }
        arbitRec.state = ARBIT_INIT;
        DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitStart:Default"));
        arbitRec.newstate = true;
        return;
      }
      if (arbitRec.getTimediff() > getArbitTimeout()) {
        jam();
        arbitRec.code = ArbitCode::ErrTimeout;
        reportArbitEvent(signal, NDB_LE_ArbitState);
        arbitRec.state = ARBIT_INIT;
        DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitStart:Default timeout"));
        arbitRec.newstate = true;
        return;
      }
      break;

    default:
      ndbabort();
  }
}

void Qmgr::execARBIT_STARTCONF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (!arbitRec.match(sd)) {
    jam();
    return;  // stray signal
  }
  if (arbitRec.state != ARBIT_START) {
    jam();
    return;  // wrong state
  }
  if (arbitRec.recvCount) {
    jam();
    return;  // wrong state
  }
  arbitRec.code = sd->code;
  arbitRec.recvCount = 1;
}

void Qmgr::execARBIT_STARTREF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_STARTCONF(signal);
}

/**
 * Handle RUN state.  Send ticket to any new nodes which have
 * appeared after PREP state.  We don't care about a CONF.
 */
void Qmgr::stateArbitRun(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  NodeRecPtr aPtr;
  aPtr.i = 0;
  const unsigned stop = NodeBitmask::NotFound;
  while ((aPtr.i = arbitRec.newMask.find(aPtr.i + 1)) != stop) {
    jam();
    arbitRec.newMask.clear(aPtr.i);
    ptrAss(aPtr, nodeRec);
    ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
    sd->sender = getOwnNodeId();
    sd->code = ArbitCode::PrepAtrun;
    sd->node = arbitRec.node;
    sd->ticket = arbitRec.ticket;
    sd->mask.clear();
    sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPREQ, signal,
               ArbitSignalData::SignalLength, JBB);
  }
}

/**
 * Handle CHOOSE state.  Entered only from RUN state when
 * there is a possible network partitioning.  Send CHOOSE to
 * the arbitrator.  On win switch to INIT state because a new
 * ticket must be created.
 */
void Qmgr::stateArbitChoose(Signal *signal) {
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;
    arbitRec.recvCount = 0;
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch (arbitRec.method) {
    case ArbitRec::METHOD_EXTERNAL: {
      if (!arbitRec.sendCount) {
        jam();
        ndbrequire(arbitRec.node == 0);  // No arbitrator selected
        // Don't send CHOOSE to anyone, just wait for timeout to expire
        arbitRec.sendCount = 1;
        arbitRec.setTimestamp();
        return;
      }

      if (arbitRec.getTimediff() > getArbitTimeout()) {
        jam();
        // Arbitration timeout has expired
        ndbrequire(arbitRec.node == 0);  // No arbitrator selected

        NodeBitmask nodes;
        computeArbitNdbMask(nodes);
        arbitRec.code = ArbitCode::WinWaitExternal;
        reportArbitEvent(signal, NDB_LE_ArbitResult, nodes);

        sendCommitFailReq(signal);  // start commit of failed nodes
        arbitRec.state = ARBIT_INIT;
        DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitChoose"));
        arbitRec.newstate = true;
        return;
      }
      break;
    }

    case ArbitRec::METHOD_DEFAULT: {
      if (!arbitRec.sendCount) {
        jam();
        const BlockReference blockRef =
            calcApiClusterMgrBlockRef(arbitRec.node);
        ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
        sd->sender = getOwnNodeId();
        sd->code = 0;
        sd->node = arbitRec.node;
        sd->ticket = arbitRec.ticket;
        computeArbitNdbMask(sd->mask);
        if (ERROR_INSERTED(943)) {
          ndbout << "Not sending GSN_ARBIT_CHOOSEREQ, thereby causing"
                 << " arbitration to time out." << endl;
        } else {
          sendSignal(blockRef, GSN_ARBIT_CHOOSEREQ, signal,
                     ArbitSignalData::SignalLength, JBA);
        }
        arbitRec.sendCount = 1;
        arbitRec.setTimestamp();  // send time
        return;
      }

      if (arbitRec.recvCount) {
        jam();
        reportArbitEvent(signal, NDB_LE_ArbitResult);
        if (arbitRec.code == ArbitCode::WinChoose) {
          jam();
          sendCommitFailReq(signal);  // start commit of failed nodes
          arbitRec.state = ARBIT_INIT;
          DEB_ARBIT(("Arbit state = ARBIT_INIT stateArbitChoose:Default"));
          arbitRec.newstate = true;
          return;
        }
        arbitRec.state = ARBIT_CRASH;
        DEB_ARBIT(("Arbit state = ARBIT_CRASH stateArbitChoose:Default"));
        arbitRec.newstate = true;
        stateArbitCrash(signal);  // do it at once
        return;
      }

      if (arbitRec.getTimediff() > getArbitTimeout()) {
        jam();
        // Arbitration timeout has expired
        arbitRec.code = ArbitCode::ErrTimeout;
        reportArbitEvent(signal, NDB_LE_ArbitState);
        arbitRec.state = ARBIT_CRASH;
        DEB_ARBIT(("Arbit state = ARBIT_CRASH stateArbitChoose:Def timeout"));
        arbitRec.newstate = true;
        stateArbitCrash(signal);  // do it at once
        return;
      }
      break;
    }

    default:
      ndbabort();
  }
}

void Qmgr::execARBIT_CHOOSECONF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (!arbitRec.match(sd)) {
    jam();
    return;  // stray signal
  }
  if (arbitRec.state != ARBIT_CHOOSE) {
    jam();
    return;  // wrong state
  }
  if (arbitRec.recvCount) {
    jam();
    return;  // wrong state
  }
  arbitRec.recvCount = 1;
  arbitRec.code = sd->code;
}

void Qmgr::execARBIT_CHOOSEREF(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_CHOOSECONF(signal);
}

/**
 * Handle CRASH state.  We must crash immediately.
 * XXX tell other nodes in our party to crash too.
 */
void Qmgr::stateArbitCrash(Signal *signal) {
  jam();
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);
    arbitRec.setTimestamp();
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
#ifdef ndb_arbit_crash_wait_for_event_report_to_get_out
  if (!(arbitRec.getTimediff() > getArbitTimeout())) return;
#endif
  CRASH_INSERTION(932);
  CRASH_INSERTION(938);
  CRASH_INSERTION(943);
  CRASH_INSERTION(944);
  progError(__LINE__, NDBD_EXIT_ARBIT_SHUTDOWN,
            "Arbitrator decided to shutdown this node");
}

/**
 * Arbitrator may inform us that it will exit.  This lets us
 * start looking sooner for a new one.  Handle it like API node
 * failure.
 */
void Qmgr::execARBIT_STOPREP(Signal *signal) {
  jamEntry();
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  if (!arbitRec.match(sd)) {
    jam();
    return;  // stray signal
  }
  arbitRec.code = ArbitCode::ApiExit;
  handleArbitApiFail(signal, arbitRec.node);
}

Uint32 Qmgr::count_previously_alive_nodes() {
  /**
   * This function is called as part of PREP_FAILCONF handling. This
   * means that we are preparing a node failure. This means that
   * NDBCNTR have not yet heard about the node failure and thus we
   * can still use the method is_node_started to see whether the
   * node was fully started before this failure.
   *
   * This method is called as part of arbitration check. A node is
   * only counted as previously alive if the node was fully started.
   *
   * In addition we check that the node is a data node and that the
   * QMGR node state is what we expect it to be if it was previously
   * alive.
   */
  Uint32 count = 0;
  NodeRecPtr aPtr;
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB &&
        c_ndbcntr->is_node_started(aPtr.i) &&
        (aPtr.p->phase == ZRUNNING || aPtr.p->phase == ZPREPARE_FAIL)) {
      jam();
      jamLine(Uint16(aPtr.i));
      count++;
    }
  }
  return count;
}

void Qmgr::computeArbitNdbMask(NodeBitmaskPOD &aMask) {
  NodeRecPtr aPtr;
  aMask.clear();
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB &&
        aPtr.p->phase == ZRUNNING) {
      jam();
      aMask.set(aPtr.i);
    }
  }
}

void Qmgr::computeArbitNdbMask(NdbNodeBitmaskPOD &aMask) {
  NodeRecPtr aPtr;
  aMask.clear();
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB &&
        aPtr.p->phase == ZRUNNING) {
      jam();
      aMask.set(aPtr.i);
    }
  }
}

void Qmgr::computeBeforeFailNdbMask(NdbNodeBitmaskPOD &aMask) {
  NodeRecPtr aPtr;
  aMask.clear();
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB &&
        (aPtr.p->phase == ZRUNNING || aPtr.p->phase == ZPREPARE_FAIL)) {
      jam();
      aMask.set(aPtr.i);
    }
  }
}

/**
 * Report arbitration event.  We use arbitration signal format
 * where sender (word 0) is event type.
 */
void Qmgr::reportArbitEvent(Signal *signal, Ndb_logevent_type type,
                            const NodeBitmask mask) {
  ArbitSignalData *sd = (ArbitSignalData *)&signal->theData[0];
  sd->sender = type;
  sd->code = arbitRec.code | (arbitRec.state << 16);
  sd->node = arbitRec.node;
  sd->ticket = arbitRec.ticket;
  sd->mask = mask;

  // Log to console/stdout
  LogLevel ll;
  ll.setLogLevel(LogLevel::llNodeRestart, 15);
  g_eventLogger->log(type, &signal->theData[0], ArbitSignalData::SignalLength,
                     0, &ll);

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, ArbitSignalData::SignalLength,
             JBB);
}

// end of arbitration module

void Qmgr::execDUMP_STATE_ORD(Signal *signal) {
  if (signal->theData[0] == 1) {
    unsigned max_nodes = MAX_NDB_NODES;
    if (signal->getLength() == 2) {
      max_nodes = signal->theData[1];
      if (max_nodes == 0 || max_nodes >= MAX_NODES) {
        max_nodes = MAX_NODES;
      } else {
        max_nodes++;  // Include node id argument in loop
      }
    }
    infoEvent("creadyDistCom = %d, cpresident = %d\n", creadyDistCom,
              cpresident);
    infoEvent("cpresidentAlive = %d, cpresidentCand = %d (gci: %d)\n",
              cpresidentAlive, c_start.m_president_candidate,
              c_start.m_president_candidate_gci);
    infoEvent("ctoStatus = %d\n", ctoStatus);
    for (Uint32 i = 1; i < max_nodes; i++) {
      NodeRecPtr nodePtr;
      nodePtr.i = i;
      ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);
      char buf[100];
      switch (nodePtr.p->phase) {
        case ZINIT:
          sprintf(buf, "Node %d: ZINIT(%d)", i, nodePtr.p->phase);
          break;
        case ZSTARTING:
          sprintf(buf, "Node %d: ZSTARTING(%d)", i, nodePtr.p->phase);
          break;
        case ZRUNNING:
          sprintf(buf, "Node %d: ZRUNNING(%d)", i, nodePtr.p->phase);
          break;
        case ZPREPARE_FAIL:
          sprintf(buf, "Node %d: ZPREPARE_FAIL(%d)", i, nodePtr.p->phase);
          break;
        case ZFAIL_CLOSING:
          sprintf(buf, "Node %d: ZFAIL_CLOSING(%d)", i, nodePtr.p->phase);
          break;
        case ZAPI_INACTIVE:
          sprintf(buf, "Node %d: ZAPI_INACTIVE(%d)", i, nodePtr.p->phase);
          break;
        case ZAPI_ACTIVE:
          sprintf(buf, "Node %d: ZAPI_ACTIVE(%d)", i, nodePtr.p->phase);
          break;
        case ZAPI_ACTIVATION_ONGOING:
          sprintf(buf, "Node %d: ZAPI_ACTIVATION_ONGOING(%d)", i,
                  nodePtr.p->phase);
          break;
        default:
          sprintf(buf, "Node %d: <UNKNOWN>(%d)", i, nodePtr.p->phase);
          break;
      }
      infoEvent("%s", buf);
    }
  }

#ifdef ERROR_INSERT
  if (signal->theData[0] == 935 && signal->getLength() == 2) {
    SET_ERROR_INSERT_VALUE(935);
    c_error_insert_extra = signal->theData[1];
  }
#endif

  if (signal->theData[0] == 900 && signal->getLength() == 2) {
    g_eventLogger->info("disconnecting %u", signal->theData[1]);
    api_failed(signal, signal->theData[1]);
  }

  if (signal->theData[0] == 908) {
    int tag = signal->getLength() < 2 ? -1 : signal->theData[1];
    char buf[8192];
    // for easy grepping in *out.log ...
    strcpy(buf, "HB:");
    if (tag >= 0) sprintf(buf + strlen(buf), "%d:", tag);
    sprintf(buf + strlen(buf), " pres:%u", cpresident);
    sprintf(buf + strlen(buf), " own:%u", getOwnNodeId());
    NodeRecPtr myNodePtr;
    myNodePtr.i = getOwnNodeId();
    ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
    sprintf(buf + strlen(buf), " dyn:%u-%u", myNodePtr.p->ndynamicId & 0xFFFF,
            myNodePtr.p->ndynamicId >> 16);
    sprintf(buf + strlen(buf), " mxdyn:%u", c_maxDynamicId);
    sprintf(buf + strlen(buf), " hb:%u->%u->%u", cneighbourl, getOwnNodeId(),
            cneighbourh);
    sprintf(buf + strlen(buf), " node:dyn-hi,cfg:");
    NodeRecPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      Uint32 type = getNodeInfo(nodePtr.i).m_type;
      if (type == NodeInfo::DB) {
        sprintf(buf + strlen(buf), " %u:%u-%u,%u", nodePtr.i,
                nodePtr.p->ndynamicId & 0xFFFF, nodePtr.p->ndynamicId >> 16,
                nodePtr.p->hbOrder);
      }
    }
    ndbout << buf << endl;
  }

#ifdef ERROR_INSERT
  Uint32 dumpCode = signal->theData[0];
  if ((dumpCode == 9992) || (dumpCode == 9993)) {
    if (signal->getLength() == 2) {
      Uint32 nodeId = signal->theData[1];
      Uint32 &newNodeId = signal->theData[1];
      Uint32 length = 2;
      assert(257 > MAX_NODES);
      if (nodeId > MAX_NODES) {
        const char *type = "None";
        switch (nodeId) {
          case 257: {
            /* Left (lower) neighbour */
            newNodeId = cneighbourl;
            type = "Left neighbour";
            break;
          }
          case 258: {
            /* Right (higher) neighbour */
            newNodeId = cneighbourh;
            type = "Right neighbour";
            break;
          }
          case 259: {
            /* President */
            newNodeId = cpresident;
            type = "President";
            break;
          }
        }
        g_eventLogger->info(
            "QMGR : Mapping request on node id %u to node id %u (%s)", nodeId,
            newNodeId, type);
        if (newNodeId != nodeId) {
          sendSignal(CMVMI_REF, GSN_DUMP_STATE_ORD, signal, length, JBB);
        }
      }
    }
  }

  if (dumpCode == 9994) {
    g_eventLogger->info("setCCDelay(%u)", signal->theData[1]);
    setCCDelay(signal->theData[1]);
    m_connectivity_check.m_enabled = true;
  }
#endif

  if (signal->theData[0] == 939 && signal->getLength() == 2) {
    jam();
    Uint32 nodeId = signal->theData[1];
    g_eventLogger->info("Force close communication to %u", nodeId);
    SET_ERROR_INSERT_VALUE2(939, nodeId);
    CloseComReqConf *closeCom =
        CAST_PTR(CloseComReqConf, signal->getDataPtrSend());

    closeCom->xxxBlockRef = reference();
    closeCom->requestType = CloseComReqConf::RT_NO_REPLY;
    closeCom->failNo = 0;
    closeCom->noOfNodes = 1;
    closeCom->failedNodeId = nodeId;
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
               CloseComReqConf::SignalLength, JBB);
  }
}  // Qmgr::execDUMP_STATE_ORD()

void Qmgr::execAPI_BROADCAST_REP(Signal *signal) {
  jamEntry();
  ApiBroadcastRep api = *(const ApiBroadcastRep *)signal->getDataPtr();

  SectionHandle handle(this, signal);
  Uint32 len = signal->getLength() - ApiBroadcastRep::SignalLength;
  memmove(signal->theData, signal->theData + ApiBroadcastRep::SignalLength,
          4 * len);

  NodeBitmask mask;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE &&
        getNodeInfo(nodePtr.i).m_version >= api.minVersion) {
      jam();
      mask.set(nodePtr.i);
    }
  }

  if (mask.isclear()) {
    jam();
    releaseSections(handle);
    return;
  }

  NodeReceiverGroup rg(API_CLUSTERMGR, mask);
  sendSignal(rg, api.gsn, signal, len, JBB, &handle);
}

void Qmgr::execTRP_KEEP_ALIVE(Signal *signal) {
  /*
   * This signal is sent via explicit transporter and signal may come in other
   * order than other signals from same sender.
   * That is ok since this signal is only there to generate traffic such that
   * connection is not taken as idle connection and disconnected if one run in
   * an environment there connection traffics are monitored and disconnected
   * if idle for too long.
   */
  jamEntry();
}

void Qmgr::execNODE_FAILREP(Signal *signal) {
  jamEntry();
  NodeFailRep *nodeFail = (NodeFailRep *)signal->getDataPtr();
  if (signal->getNoOfSections() >= 1) {
    ndbrequire(ndbd_send_node_bitmask_in_section(
        getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    memset(nodeFail->theNodes, 0, sizeof(nodeFail->theNodes));
    copy(nodeFail->theNodes, ptr);
    releaseSections(handle);
  } else {
    memset(nodeFail->theNodes + NdbNodeBitmask48::Size, 0, _NDB_NBM_DIFF_BYTES);
  }

  NdbNodeBitmask allFailed;
  allFailed.assign(NdbNodeBitmask::Size, nodeFail->theNodes);

  // make sure any distributed signals get acknowledged
  // destructive of the signal
  NdbNodeBitmask failedNodes;
  failedNodes.assign(NdbNodeBitmask::Size, nodeFail->theNodes);
  c_counterMgr.execNODE_FAILREP(signal, failedNodes);
  Uint32 nodeId = 0;
  while (!allFailed.isclear()) {
    nodeId = allFailed.find(nodeId + 1);
    // ndbrequire(nodeId != Bitmask::NotFound);
    allFailed.clear(nodeId);
    NodeRecPtr nodePtr;
    nodePtr.i = nodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    nodePtr.p->m_is_multi_trp_setup = false;
    nodePtr.p->m_is_ready_to_switch_trp = false;
    nodePtr.p->m_is_freeze_thread_completed = false;
    nodePtr.p->m_is_activate_trp_ready_for_me = false;
    nodePtr.p->m_is_activate_trp_ready_for_other = false;
    nodePtr.p->m_is_preparing_switch_trp = false;
    nodePtr.p->m_is_using_multi_trp = false;
    nodePtr.p->m_set_up_multi_trp_started = false;
    nodePtr.p->m_multi_trp_blockref = 0;
    nodePtr.p->m_used_num_multi_trps = 0;
    nodePtr.p->m_check_multi_trp_connect_loop_count = 0;
    nodePtr.p->m_num_activated_trps = 0;
    if (nodePtr.p->m_is_in_same_nodegroup) {
      jam();
      check_no_multi_trp(signal, nodePtr.i);
      globalTransporterRegistry.lockMultiTransporters();
      bool switch_required = false;
      Multi_Transporter *multi_trp =
          globalTransporterRegistry.get_node_multi_transporter(nodePtr.i);
      if (multi_trp != nullptr &&
          globalTransporterRegistry.get_num_active_transporters(multi_trp) >
              1) {
        /**
         * The timing of the NODE_FAILREP signal is such that the transporter
         * haven't had time to switch the active transporters yet, we know
         * this will happen, so we switch now to use the base transporter for
         * the neighbour node. The node is currently down, so will have to
         * be setup before it can be used again.
         *
         * We will restore the active transporters to be the multi
         * transporters to enable the transporters to be handled by the
         * disconnect code. This is why it is required to lock the
         * multi transporter mutex while performing this action.
         */
        switch_required = true;
        DEB_MULTI_TRP(
            ("switch_active_trp for node %u's transporter", nodePtr.i));
        globalTransporterRegistry.switch_active_trp(multi_trp);
      }

      DEB_MULTI_TRP(("Change neighbour node setup for node %u", nodePtr.i));
      startChangeNeighbourNode();
      setNeighbourNode(nodePtr.i);
      endChangeNeighbourNode();
      if (switch_required) {
        globalTransporterRegistry.switch_active_trp(multi_trp);
        DEB_MULTI_TRP(
            ("switch_active_trp for node %u's transporter", nodePtr.i));
      }
      globalTransporterRegistry.unlockMultiTransporters();
    }
  }
}

void Qmgr::execALLOC_NODEID_REQ(Signal *signal) {
  jamEntry();
  AllocNodeIdReq req = *(AllocNodeIdReq *)signal->getDataPtr();
  Uint32 error = 0;

  NodeRecPtr nodePtr;
  nodePtr.i = req.nodeId;
  if ((nodePtr.i >= MAX_NODES) ||
      ((req.nodeType == NodeInfo::DB) && (nodePtr.i >= MAX_NDB_NODES))) {
    /* Ignore messages about nodes not even within range */
    jam();
    return;
  }
  ptrAss(nodePtr, nodeRec);

  if (refToBlock(req.senderRef) != QMGR)  // request from management server
  {
    /* master */
    Dbdih *dih = (Dbdih *)globalData.getBlock(DBDIH, instance());
    bool is_dih_master = dih->is_master();
    if (getOwnNodeId() != cpresident || !is_dih_master) {
      jam();
      /**
       * Either we are not president which leads to that we are not master
       * in DIH, or we are president but hasn't yet seen our election to
       * master in DIH. Either way we respond with NotMaster, if we are
       * president and not master the response will lead to a retry which
       * is likely to be successful.
       */
      if (getOwnNodeId() == cpresident) {
        jam();
        g_eventLogger->debug("President, but not master at ALLOC_NODEID_REQ");
      }
      error = AllocNodeIdRef::NotMaster;
    } else if (!opAllocNodeIdReq.m_tracker.done()) {
      jam();
      error = AllocNodeIdRef::Busy;
    } else if (c_connectedNodes.get(req.nodeId)) {
      jam();
      error = AllocNodeIdRef::NodeConnected;
    } else if (nodePtr.p->m_secret != 0) {
      jam();
      error = AllocNodeIdRef::NodeReserved;
    } else if (req.nodeType != getNodeInfo(req.nodeId).m_type) {
      jam();
      error = AllocNodeIdRef::NodeTypeMismatch;
    } else if (req.nodeType == NodeInfo::API && c_allow_api_connect == 0) {
      jam();
      error = AllocNodeIdRef::NotReady;
    }

    if (error) {
      jam();
      g_eventLogger->debug("Alloc node id for node %u failed, err: %u",
                           nodePtr.i, error);
      AllocNodeIdRef *ref = (AllocNodeIdRef *)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->errorCode = error;
      ref->masterRef = numberToRef(QMGR, cpresident);
      ref->senderData = req.senderData;
      ref->nodeId = req.nodeId;
      sendSignal(req.senderRef, GSN_ALLOC_NODEID_REF, signal,
                 AllocNodeIdRef::SignalLength, JBB);
      return;
    }

    if (ERROR_INSERTED(934) && req.nodeId != getOwnNodeId()) {
      CRASH_INSERTION(934);
    }

    /**
     * generate secret
     */
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint32 secret_hi = Uint32(now.getUint64() >> 24);
    const Uint32 secret_lo = Uint32(now.getUint64() << 8) + getOwnNodeId();
    req.secret_hi = secret_hi;
    req.secret_lo = secret_lo;

    if (req.timeout > 60000) req.timeout = 60000;

    nodePtr.p->m_secret = (Uint64(secret_hi) << 32) + secret_lo;
    nodePtr.p->m_alloc_timeout = NdbTick_AddMilliseconds(now, req.timeout);

    opAllocNodeIdReq.m_req = req;
    opAllocNodeIdReq.m_error = 0;
    opAllocNodeIdReq.m_connectCount =
        getNodeInfo(refToNode(req.senderRef)).m_connectCount;

    jam();
    AllocNodeIdReq *req2 = (AllocNodeIdReq *)signal->getDataPtrSend();
    *req2 = req;
    req2->senderRef = reference();
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    RequestTracker &p = opAllocNodeIdReq.m_tracker;
    p.init<AllocNodeIdRef>(c_counterMgr, rg, GSN_ALLOC_NODEID_REF, 0);

    sendSignal(rg, GSN_ALLOC_NODEID_REQ, signal,
               AllocNodeIdReq::SignalLengthQMGR, JBB);
    return;
  }

  /* participant */
  if (c_connectedNodes.get(req.nodeId)) {
    jam();
    error = AllocNodeIdRef::NodeConnected;
  } else if (req.nodeType != getNodeInfo(req.nodeId).m_type) {
    jam();
    error = AllocNodeIdRef::NodeTypeMismatch;
  } else if ((nodePtr.p->failState != NORMAL) ||
             ((req.nodeType == NodeInfo::DB) &&
              (cfailedNodes.get(nodePtr.i)))) {
    /**
     * Either the node has committed its node failure in QMGR but not yet
     * completed the node internal node failure handling. Or the node
     * failure commit process is still ongoing in QMGR. We should not
     * allocate a node id in either case.
     */
    jam();
    error = AllocNodeIdRef::NodeFailureHandlingNotCompleted;
  } else if (req.nodeType == NodeInfo::API &&
             nodePtr.p->phase != ZAPI_INACTIVE) {
    jam();
    if (cpresident != getOwnNodeId() && c_allow_api_connect == 0) {
      /**
       * Don't block during NR
       */
      jam();
    } else {
      jam();
      if (nodePtr.p->phase == ZFAIL_CLOSING) {
        /* Occurs during node startup */
        error = AllocNodeIdRef::NodeFailureHandlingNotCompleted;
      } else {
        error = AllocNodeIdRef::NodeReserved;
      }
    }
  }
#if 0
  /**
   * For now only make "time/secret" based reservation on master
   *   as we otherwise also need to clear it on failure + handle
   *   master failure
   */
  else if (nodePtr.p->m_secret != 0)
  {
    jam();
    error = AllocNodeIdRef::NodeReserved;
  }
#endif

  if (error) {
    jam();
    g_eventLogger->info("Alloc nodeid for node %u failed,err: %u", req.nodeId,
                        error);
    AllocNodeIdRef *ref = (AllocNodeIdRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = error;
    ref->senderData = req.senderData;
    ref->nodeId = req.nodeId;
    ref->masterRef = numberToRef(QMGR, cpresident);
    sendSignal(req.senderRef, GSN_ALLOC_NODEID_REF, signal,
               AllocNodeIdRef::SignalLength, JBB);
    return;
  }

  AllocNodeIdConf *conf = (AllocNodeIdConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->secret_hi = req.secret_hi;
  conf->secret_lo = req.secret_lo;
  sendSignal(req.senderRef, GSN_ALLOC_NODEID_CONF, signal,
             AllocNodeIdConf::SignalLength, JBB);
}

void Qmgr::execALLOC_NODEID_CONF(Signal *signal) {
  /* master */

  jamEntry();
  const AllocNodeIdConf *conf = (AllocNodeIdConf *)signal->getDataPtr();
  opAllocNodeIdReq.m_tracker.reportConf(c_counterMgr,
                                        refToNode(conf->senderRef));

  if (signal->getLength() >= AllocNodeIdConf::SignalLength) {
    jam();
    if (opAllocNodeIdReq.m_req.secret_hi != conf->secret_hi ||
        opAllocNodeIdReq.m_req.secret_lo != conf->secret_lo) {
      jam();
      if (opAllocNodeIdReq.m_error == 0) {
        jam();
        opAllocNodeIdReq.m_error = AllocNodeIdRef::Undefined;
      }
    }
  }

  completeAllocNodeIdReq(signal);
}

void Qmgr::execALLOC_NODEID_REF(Signal *signal) {
  /* master */

  jamEntry();
  const AllocNodeIdRef *ref = (AllocNodeIdRef *)signal->getDataPtr();

  if (ref->errorCode == AllocNodeIdRef::NF_FakeErrorREF) {
    jam();
    if (ref->nodeId == refToNode(ref->senderRef)) {
      /**
       * The node id we are trying to allocate has responded with a REF,
       * this was sent in response to a node failure, so we are most
       * likely not ready to allocate this node id yet. Report node
       * failure handling not ready yet.
       */
      jam();
      opAllocNodeIdReq.m_tracker.reportRef(c_counterMgr,
                                           refToNode(ref->senderRef));
      if (opAllocNodeIdReq.m_error == 0) {
        jam();
        opAllocNodeIdReq.m_error =
            AllocNodeIdRef::NodeFailureHandlingNotCompleted;
      }
    } else {
      jam();
      opAllocNodeIdReq.m_tracker.ignoreRef(c_counterMgr,
                                           refToNode(ref->senderRef));
    }
  } else {
    jam();
    opAllocNodeIdReq.m_tracker.reportRef(c_counterMgr,
                                         refToNode(ref->senderRef));
    if (opAllocNodeIdReq.m_error == 0) {
      jam();
      opAllocNodeIdReq.m_error = ref->errorCode;
    }
  }
  completeAllocNodeIdReq(signal);
}

void Qmgr::completeAllocNodeIdReq(Signal *signal) {
  /* master */

  if (!opAllocNodeIdReq.m_tracker.done()) {
    jam();
    return;
  }

  if (opAllocNodeIdReq.m_connectCount !=
      getNodeInfo(refToNode(opAllocNodeIdReq.m_req.senderRef)).m_connectCount) {
    // management server not same version as the original requester
    jam();
    return;
  }

  if (opAllocNodeIdReq.m_tracker.hasRef()) {
    jam();

    {
      /**
       * Clear reservation
       */
      NodeRecPtr nodePtr;
      nodePtr.i = opAllocNodeIdReq.m_req.nodeId;
      ptrAss(nodePtr, nodeRec);
      nodePtr.p->m_secret = 0;
    }
    g_eventLogger->info("Alloc node id for node %u failed, err: %u",
                        opAllocNodeIdReq.m_req.nodeId,
                        opAllocNodeIdReq.m_error);

    AllocNodeIdRef *ref = (AllocNodeIdRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = opAllocNodeIdReq.m_req.senderData;
    ref->nodeId = opAllocNodeIdReq.m_req.nodeId;
    ref->errorCode = opAllocNodeIdReq.m_error;
    ref->masterRef = numberToRef(QMGR, cpresident);
    ndbassert(AllocNodeIdRef::SignalLength == 5);
    sendSignal(opAllocNodeIdReq.m_req.senderRef, GSN_ALLOC_NODEID_REF, signal,
               AllocNodeIdRef::SignalLength, JBB);
    return;
  }

  jam();

  g_eventLogger->info("Alloc node id for node %u succeeded",
                      opAllocNodeIdReq.m_req.nodeId);
  AllocNodeIdConf *conf = (AllocNodeIdConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = opAllocNodeIdReq.m_req.senderData;
  conf->nodeId = opAllocNodeIdReq.m_req.nodeId;
  conf->secret_lo = opAllocNodeIdReq.m_req.secret_lo;
  conf->secret_hi = opAllocNodeIdReq.m_req.secret_hi;
  sendSignal(opAllocNodeIdReq.m_req.senderRef, GSN_ALLOC_NODEID_CONF, signal,
             AllocNodeIdConf::SignalLength, JBB);

  /**
   * We are the master and master DIH wants to keep track of node restart
   * state to be able to control LCP start and stop and also to be able
   * to easily report this state to the user when he asks for it.
   */
  AllocNodeIdRep *rep = (AllocNodeIdRep *)signal->getDataPtrSend();
  rep->nodeId = opAllocNodeIdReq.m_req.nodeId;
  EXECUTE_DIRECT(DBDIH, GSN_ALLOC_NODEID_REP, signal,
                 AllocNodeIdRep::SignalLength);
}

void Qmgr::execSTOP_REQ(Signal *signal) {
  jamEntry();

  const StopReq *req = (const StopReq *)signal->getDataPtr();
  c_stopReq.senderRef = req->senderRef;
  c_stopReq.senderData = req->senderData;
  c_stopReq.requestInfo = req->requestInfo;
  c_stopReq.nodes.clear();
  if (signal->getNoOfSections() >= 1) {
    jam();
    SectionHandle handle(this, signal);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(c_stopReq.nodes.rep.data, ptr);
    releaseSections(handle);
  } else {
    jam();
    c_stopReq.nodes.assign(NdbNodeBitmask48::Size, req->nodes);
  }

  if (c_stopReq.senderRef) {
    jam();
    ndbrequire(c_stopReq.nodes.get(getOwnNodeId()));

    StopConf *conf = (StopConf *)signal->getDataPtrSend();
    conf->senderData = c_stopReq.senderData;
    conf->nodeState = getOwnNodeId();
    sendSignal(c_stopReq.senderRef, GSN_STOP_CONF, signal,
               StopConf::SignalLength, JBA);
  }
}

bool Qmgr::check_multi_node_shutdown(Signal *signal) {
  if (c_stopReq.senderRef && c_stopReq.nodes.get(getOwnNodeId())) {
    jam();
    if (StopReq::getPerformRestart(c_stopReq.requestInfo)) {
      jam();
      StartOrd *startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = c_stopReq.requestInfo;
      sendSignal(CMVMI_REF, GSN_START_ORD, signal, 2, JBA);
    } else {
      sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return true;
  }
  return false;
}

int Qmgr::check_hb_order_config() {
  m_hb_order_config_used = false;
  Uint32 count = 0;
  Uint32 count_zero = 0;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    const NodeInfo &nodeInfo = getNodeInfo(nodePtr.i);
    if (nodeInfo.m_type == NodeInfo::DB) {
      count++;
      if (nodePtr.p->hbOrder == 0) count_zero++;
    }
  }
  ndbrequire(count != 0);  // must have node info
  if (count_zero == count) {
    jam();
    return 0;  // no hbOrder defined
  }
  if (count_zero != 0) {
    jam();
    return -1;  // error: not all zero or all nonzero
  }
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    const NodeInfo &nodeInfo = getNodeInfo(nodePtr.i);
    if (nodeInfo.m_type == NodeInfo::DB) {
      NodeRecPtr nodePtr2;
      for (nodePtr2.i = nodePtr.i + 1; nodePtr2.i < MAX_NDB_NODES;
           nodePtr2.i++) {
        ptrAss(nodePtr2, nodeRec);
        const NodeInfo &nodeInfo2 = getNodeInfo(nodePtr2.i);
        if (nodeInfo2.m_type == NodeInfo::DB) {
          if (nodePtr.i != nodePtr2.i &&
              nodePtr.p->hbOrder == nodePtr2.p->hbOrder) {
            jam();
            return -2;  // error: duplicate nonzero value
          }
        }
      }
    }
  }
  m_hb_order_config_used = true;
  return 0;
}

static const Uint32 CC_SuspectTicks = 1;
static const Uint32 CC_FailedTicks = 2;

void Qmgr::startConnectivityCheck(Signal *signal, Uint32 reason,
                                  Uint32 causingNode) {
  jam();
  ndbrequire(m_connectivity_check.getEnabled());

  if (m_connectivity_check.m_active) {
    jam();
    /* Connectivity check underway already
     * do nothing
     */
    return;
  }

  m_connectivity_check.m_nodesPinged.clear();

  /* Send NODE_PINGREQ signal to all other running nodes, and
   * initialise connectivity check bitmasks.
   * Note that nodes may already be considered suspect due to
   * a previous connectivity check round.
   */
  Uint32 ownId = getOwnNodeId();
  NodePingReq *pingReq = CAST_PTR(NodePingReq, &signal->theData[0]);
  pingReq->senderData = ++m_connectivity_check.m_currentRound;
  pingReq->senderRef = reference();

  for (Uint32 i = 1; i < MAX_NDB_NODES; i++) {
    if (i != ownId) {
      NodeRec &node = nodeRec[i];
      if (node.phase == ZRUNNING) {
        /* If connection was considered ok, treat as unknown,
         * If it was considered slow, continue to treat
         *   as slow
         */
        sendSignal(node.blockRef, GSN_NODE_PING_REQ, signal,
                   NodePingReq::SignalLength, JBA);

        m_connectivity_check.m_nodesPinged.set(i);
      }
    }
  }

  /* Initialise result bitmasks */
  m_connectivity_check.m_nodesWaiting.assign(
      m_connectivity_check.m_nodesPinged);
  m_connectivity_check.m_nodesFailedDuring.clear();

  /* Ensure only live nodes are considered suspect */
  m_connectivity_check.m_nodesSuspect.bitAND(
      m_connectivity_check.m_nodesPinged);

  const char *reasonText = "Unknown";
  bool firstTime = true;

  switch (reason) {
    case FailRep::ZHEARTBEAT_FAILURE:
      reasonText = "Heartbeat failure";
      break;
    case FailRep::ZCONNECT_CHECK_FAILURE:
      reasonText = "Connectivity check request";
      break;
    default:
      firstTime = false;
      ndbrequire(m_connectivity_check.m_nodesSuspect.count() > 0);
      break;
  }

  if (!m_connectivity_check.m_nodesPinged.isclear()) {
    jam();
    {
      char buff[NdbNodeBitmask::TextLength + 1];
      m_connectivity_check.m_nodesPinged.getText(buff);
      if (firstTime) {
        g_eventLogger->info(
            "QMGR : Starting connectivity check of %u other nodes (%s) due to "
            "%s from node %u.",
            m_connectivity_check.m_nodesPinged.count(), buff, reasonText,
            causingNode);
      } else {
        char buff2[NdbNodeBitmask::TextLength + 1];
        m_connectivity_check.m_nodesSuspect.getText(buff2);
        g_eventLogger->info(
            "QMGR : Restarting connectivity check of %u other nodes (%s) due "
            "to %u syspect nodes (%s)",
            m_connectivity_check.m_nodesPinged.count(), buff,
            m_connectivity_check.m_nodesSuspect.count(), buff2);
      }
    }

    /* Generate cluster log event */
    Uint32 bitmaskSz = NdbNodeBitmask::Size;
    signal->theData[0] = NDB_LE_ConnectCheckStarted;
    signal->theData[1] = m_connectivity_check.m_nodesPinged.count();
    signal->theData[2] = reason;
    signal->theData[3] = causingNode;
    signal->theData[4] = bitmaskSz;
    Uint32 *sigPtr = &signal->theData[5];
    m_connectivity_check.m_nodesPinged.copyto(bitmaskSz, sigPtr);
    sigPtr += bitmaskSz;
    m_connectivity_check.m_nodesSuspect.copyto(bitmaskSz, sigPtr);

    LinearSectionPtr lsptr[3];
    lsptr[0].p = signal->theData;
    lsptr[0].sz = 5 + 2 * NdbNodeBitmask::Size;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB, lsptr, 1);

    m_connectivity_check.m_active = true;
    m_connectivity_check.m_tick = 0;
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    m_connectivity_check.m_timer.reset(now);
  } else {
    g_eventLogger->info(
        "QMGR : Connectivity check requested due to %s (from %u) not started "
        "as no other running nodes.",
        reasonText, causingNode);
  }
}

void Qmgr::execNODE_PINGREQ(Signal *signal) {
  jamEntry();
  Uint32 ownId = getOwnNodeId();
  const NodePingReq *pingReq = CAST_CONSTPTR(NodePingReq, &signal->theData[0]);
  Uint32 sendersRef = signal->getSendersBlockRef();
  Uint32 sendersNodeId = refToNode(sendersRef);
  Uint32 senderData = pingReq->senderData;

  ndbrequire(sendersNodeId != ownId);

  /* We will start our own connectivity check if necessary
   * before responding with PING_CONF to the requestor.
   * This means that the sending node will receive our PING_REQ
   * before our PING_CONF, which should avoid them starting an
   * unnecessary extra connectivity check round in some cases.
   */
  if (likely(m_connectivity_check.getEnabled())) {
    jam();
    /* We have connectivity checking configured */
    if (!m_connectivity_check.m_active) {
      jam();

      {
        /* Don't start a new connectivity check if the requesting
         * node has failed from our point of view
         */
        NodeRecPtr nodePtr;
        nodePtr.i = sendersNodeId;
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        if (unlikely(nodePtr.p->phase != ZRUNNING)) {
          jam();

          g_eventLogger->warning(
              "QMGR : Discarding NODE_PINGREQ from non-running node %u (%u)",
              sendersNodeId, nodePtr.p->phase);
          return;
        }
      }

      /* Start our own Connectivity Check now indicating reason and causing node
       */
      startConnectivityCheck(signal, FailRep::ZCONNECT_CHECK_FAILURE,
                             sendersNodeId);
    }
  } else {
    jam();
    g_eventLogger->warning(
        "QMGR : NODE_PINGREQ received from node %u, but connectivity "
        "checking not configured on this node.  Ensure all "
        "nodes have the same configuration for parameter "
        "ConnectCheckIntervalMillis.",
        sendersNodeId);
  }

  /* Now respond with NODE_PINGCONF */
  NodePingConf *pingConf = CAST_PTR(NodePingConf, &signal->theData[0]);

  pingConf->senderData = senderData;
  pingConf->senderRef = reference();

  sendSignal(sendersRef, GSN_NODE_PING_CONF, signal, NodePingConf::SignalLength,
             JBA);
}

void Qmgr::ConnectCheckRec::reportNodeConnect(Uint32 nodeId) {
  /* Clear any suspicion */
  m_nodesSuspect.clear(nodeId);
}

bool Qmgr::ConnectCheckRec::reportNodeFailure(Uint32 nodeId) {
  if (unlikely(m_active)) {
    m_nodesFailedDuring.set(nodeId);

    if (m_nodesWaiting.get(nodeId)) {
      /* We were waiting for a NODE_PING_CONF from this node,
       * remove it from the set
       */
      m_nodesWaiting.clear(nodeId);

      return m_nodesWaiting.isclear();
    }
  }
  return false;
}

void Qmgr::execNODE_PINGCONF(Signal *signal) {
  jamEntry();

  ndbrequire(m_connectivity_check.getEnabled());

  const NodePingConf *pingConf =
      CAST_CONSTPTR(NodePingConf, &signal->theData[0]);
  Uint32 sendersBlockRef = signal->getSendersBlockRef();
  Uint32 sendersNodeId = refToNode(sendersBlockRef);
  Uint32 roundNumber = pingConf->senderData;

  ndbrequire(sendersNodeId != getOwnNodeId());
  ndbrequire((m_connectivity_check.m_active) || /* Normal */
             (m_connectivity_check.m_nodesWaiting.get(
                  sendersNodeId) || /* We killed last round */
              m_connectivity_check.m_nodesFailedDuring.get(
                  sendersNodeId))); /* Someone killed */

  if (unlikely((!m_connectivity_check.m_active) ||
               (roundNumber != m_connectivity_check.m_currentRound))) {
    g_eventLogger->warning(
        "QMGR : Received NODEPING_CONF from node %u for round %u, "
        "but we are %sactive on round %u.  Discarding.",
        sendersNodeId, roundNumber,
        ((m_connectivity_check.m_active) ? "" : "in"),
        m_connectivity_check.m_currentRound);
    return;
  }

  if (ERROR_INSERTED(938)) {
    g_eventLogger->info("QMGR : execNODE_PING_CONF() from %u in tick %u",
                        sendersNodeId, m_connectivity_check.m_tick);
  }

  /* Node must have been pinged, we must be waiting for the response,
   * or the node must have already failed
   */
  ndbrequire(m_connectivity_check.m_nodesPinged.get(sendersNodeId));
  ndbrequire(m_connectivity_check.m_nodesWaiting.get(sendersNodeId) ||
             m_connectivity_check.m_nodesFailedDuring.get(sendersNodeId));

  m_connectivity_check.m_nodesWaiting.clear(sendersNodeId);

  if (likely(m_connectivity_check.m_tick < CC_SuspectTicks)) {
    jam();
    /* Node responded on time, clear any suspicion about it */
    m_connectivity_check.m_nodesSuspect.clear(sendersNodeId);
  }

  if (m_connectivity_check.m_nodesWaiting.isclear()) {
    jam();
    /* Connectivity check round is now finished */
    connectivityCheckCompleted(signal);
  }
}

void Qmgr::connectivityCheckCompleted(Signal *signal) {
  jam();

  m_connectivity_check.m_active = false;

  /* Log the following :
   * Nodes checked
   * Nodes responded ok
   * Nodes responded late (now suspect)
   * Nodes failed to respond.
   * Nodes failed during
   */
  char pinged[NdbNodeBitmask::TextLength + 1];
  char late[NdbNodeBitmask::TextLength + 1];
  char silent[NdbNodeBitmask::TextLength + 1];
  char failed[NdbNodeBitmask::TextLength + 1];

  /* Any 'waiting' nodes have been killed
   * Surviving suspects do not include them.
   */
  NdbNodeBitmask survivingSuspects(m_connectivity_check.m_nodesSuspect);
  survivingSuspects.bitANDC(m_connectivity_check.m_nodesWaiting);

  /* Nodes that failed during the check are also excluded */
  survivingSuspects.bitANDC(m_connectivity_check.m_nodesFailedDuring);

  m_connectivity_check.m_nodesPinged.getText(pinged);
  survivingSuspects.getText(late);
  m_connectivity_check.m_nodesWaiting.getText(silent);
  m_connectivity_check.m_nodesFailedDuring.getText(failed);

  g_eventLogger->info(
      "QMGR : Connectivity check completed, "
      "%u other nodes checked (%s), "
      "%u responded on time, "
      "%u responded late (%s), "
      "%u no response will be failed (%s), "
      "%u failed during check (%s)\n",
      m_connectivity_check.m_nodesPinged.count(), pinged,
      m_connectivity_check.m_nodesPinged.count() -
          m_connectivity_check.m_nodesSuspect.count(),
      survivingSuspects.count(), late,
      m_connectivity_check.m_nodesWaiting.count(), silent,
      m_connectivity_check.m_nodesFailedDuring.count(), failed);

  /* Log in Cluster log */
  signal->theData[0] = NDB_LE_ConnectCheckCompleted;
  signal->theData[1] = m_connectivity_check.m_nodesPinged.count();
  signal->theData[2] = survivingSuspects.count();
  signal->theData[3] = m_connectivity_check.m_nodesWaiting.count() +
                       m_connectivity_check.m_nodesFailedDuring.count();

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  if (survivingSuspects.count() > 0) {
    jam();
    /* Still suspect nodes, start another round */
    g_eventLogger->info(
        "QMGR : Starting new connectivity check due to suspect nodes.");
    /* Restart connectivity check, no external reason or cause */
    startConnectivityCheck(signal, 0, 0);
  } else {
    jam();
    /* No suspect nodes, stop the protocol now */

    g_eventLogger->info("QMGR : All other nodes (%u) connectivity ok.",
                        m_connectivity_check.m_nodesPinged.count() -
                            (m_connectivity_check.m_nodesWaiting.count() +
                             m_connectivity_check.m_nodesFailedDuring.count()));

    /* Send a heartbeat to our right neighbour at this point as a gesture
     * of goodwill
     */
    sendHeartbeat(signal);
    hb_send_timer.reset(NdbTick_getCurrentTicks());
  };
}

void Qmgr::checkConnectivityTimeSignal(Signal *signal) {
  /* Executed periodically when a connectivity check is
   * underway.
   * After CC_SuspectTicks have elapsed, any nodes
   * which have not responded are considered
   * 'Suspect'.
   * After CC_FailedTicks have elapsed, any nodes
   * which have not responded are considered
   * to have failed, and failure handling
   * begins.
   */
  jam();

  /* Preconditions, otherwise we shouldn't have been called */
  ndbrequire(m_connectivity_check.getEnabled());
  ndbrequire(m_connectivity_check.m_active);
  ndbrequire(!m_connectivity_check.m_nodesWaiting.isclear());

  m_connectivity_check.m_tick++;

  switch (m_connectivity_check.m_tick) {
    case CC_SuspectTicks: {
      jam();
      /* Still waiting to hear from some nodes, they are now
       * suspect
       */
      m_connectivity_check.m_nodesSuspect.bitOR(
          m_connectivity_check.m_nodesWaiting);
      return;
    }
    case CC_FailedTicks: {
      jam();
      /* Still waiting to hear from some nodes, they will now
       * be failed
       */
      m_connectivity_check.m_active = false;
      Uint32 nodeId = 0;

      while ((nodeId = m_connectivity_check.m_nodesWaiting.find(nodeId)) !=
             BitmaskImpl::NotFound) {
        jam();
        /* Log failure reason */
        /* Todo : Connectivity Check specific failure log? */
        signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
        signal->theData[1] = nodeId;

        sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

        /* Fail the node */
        /* TODO : Consider real time break here */
        failReportLab(signal, nodeId, FailRep::ZCONNECT_CHECK_FAILURE,
                      getOwnNodeId());
        nodeId++;
      }

      /* Now handle the end of the Connectivity Check */
      connectivityCheckCompleted(signal);
    }
  }
}

bool Qmgr::isNodeConnectivitySuspect(Uint32 nodeId) const {
  return m_connectivity_check.m_nodesSuspect.get(nodeId);
}

void Qmgr::handleFailFromSuspect(Signal *signal, Uint32 reason,
                                 Uint16 aFailedNode, Uint16 sourceNode) {
  jam();

  const char *reasonText = "Unknown";

  /* We have received a failure report about some node X from
   * some other node that we consider to have suspect connectivity
   * which may have caused the report.
   *
   * We will 'invert' the sense of this, and handle it as
   * a failure report of the sender, with the same cause.
   */
  switch (reason) {
    case FailRep::ZCONNECT_CHECK_FAILURE:
      jam();
      /* Suspect says that connectivity check failed for another node.
       * As suspect has bad connectivity from our point of view, we
       * blame him.
       */
      reasonText = "ZCONNECT_CHECK_FAILURE";
      break;
    case FailRep::ZLINK_FAILURE:
      jam();
      /* Suspect says that link failed for another node.
       * As suspect has bad connectivity from our point of view, we
       * blame her.
       */
      reasonText = "ZLINK_FAILURE";
      break;
    default:
      ndbabort();
  }

  g_eventLogger->warning(
      "QMGR : Received Connectivity failure notification about "
      "%u from suspect node %u with reason %s.  "
      "Mapping to failure of %u sourced by me.",
      aFailedNode, sourceNode, reasonText, sourceNode);

  signal->theData[0] = NDB_LE_NodeFailRejected;
  signal->theData[1] = reason;
  signal->theData[2] = aFailedNode;
  signal->theData[3] = sourceNode;

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  failReportLab(signal, sourceNode, (FailRep::FailCause)reason, getOwnNodeId());
}

ProcessInfo *Qmgr::getProcessInfo(Uint32 nodeId) {
  ProcessInfo *storedProcessInfo = 0;
  Int16 index = processInfoNodeIndex[nodeId];
  if (index >= 0)
    storedProcessInfo = &receivedProcessInfo[index];
  else if (nodeId == getOwnNodeId())
    storedProcessInfo = getOwnProcessInfo(getOwnNodeId());
  return storedProcessInfo;
}

void Qmgr::execDBINFO_SCANREQ(Signal *signal) {
  DbinfoScanReq req = *(DbinfoScanReq *)signal->theData;
  Ndbinfo::Ratelimit rl;

  jamEntry();
  switch (req.tableId) {
    case Ndbinfo::MEMBERSHIP_TABLEID: {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(getNodeState().nodeGroup);
      row.write_uint32(cneighbourl);
      row.write_uint32(cneighbourh);
      row.write_uint32(cpresident);

      // President successor
      Uint32 successor = 0;
      {
        NodeRecPtr nodePtr;
        UintR minDynamicId = (UintR)-1;
        for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
          jam();
          ptrAss(nodePtr, nodeRec);
          if (nodePtr.p->phase == ZRUNNING) {
            if ((nodePtr.p->ndynamicId & 0xFFFF) < minDynamicId) {
              jam();
              if (cpresident != nodePtr.i) {
                minDynamicId = (nodePtr.p->ndynamicId & 0xFFFF);
                successor = nodePtr.i;
              }
            }
          }
        }
      }
      row.write_uint32(successor);

      NodeRecPtr myNodePtr;
      myNodePtr.i = getOwnNodeId();
      ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
      row.write_uint32(myNodePtr.p->ndynamicId);

      row.write_uint32(arbitRec.node);  // arbitrator

      char ticket[20];  // Need 16 characters + 1 for trailing '\0'
      arbitRec.ticket.getText(ticket, sizeof(ticket));
      row.write_string(ticket);

      row.write_uint32(arbitRec.state);

      // arbitrator connected
      row.write_uint32(c_connectedNodes.get(arbitRec.node));

      // Find potential (rank1 and rank2) arbitrators that are connected.
      NodeRecPtr aPtr;
      // buf_size: Node nr (max 3 chars) and ', '  + trailing '\0'
      const int buf_size = 5 * MAX_NODES + 1;
      char buf[buf_size];

      for (unsigned rank = 1; rank <= 2; rank++) {
        jam();
        aPtr.i = 0;
        const unsigned stop = NodeBitmask::NotFound;
        int buf_offset = 0;
        const char *delimiter = "";

        while ((aPtr.i = arbitRec.apiMask[rank].find(aPtr.i + 1)) != stop) {
          jam();
          ptrAss(aPtr, nodeRec);
          if (c_connectedNodes.get(aPtr.i)) {
            buf_offset +=
                BaseString::snprintf(buf + buf_offset, buf_size - buf_offset,
                                     "%s%u", delimiter, aPtr.i);
            delimiter = ", ";
          }
        }

        if (buf_offset == 0)
          row.write_string("-");
        else
          row.write_string(buf);
      }

      ndbinfo_send_row(signal, req, row, rl);
      break;
    }
    case Ndbinfo::PROCESSES_TABLEID: {
      jam();
      for (int i = 1; i <= max_api_node_id; i++) {
        NodeInfo nodeInfo = getNodeInfo(i);
        if (nodeInfo.m_connected) {
          char version_buffer[NDB_VERSION_STRING_BUF_SZ];
          ndbGetVersionString(nodeInfo.m_version, nodeInfo.m_mysql_version, 0,
                              version_buffer, NDB_VERSION_STRING_BUF_SZ);

          ProcessInfo *processInfo = getProcessInfo(i);
          if (processInfo && processInfo->isValid()) {
            char uri_buffer[512];
            processInfo->getServiceUri(uri_buffer, sizeof(uri_buffer));
            Ndbinfo::Row row(signal, req);
            row.write_uint32(getOwnNodeId());              // reporting_node_id
            row.write_uint32(i);                           // node_id
            row.write_uint32(nodeInfo.getType());          // node_type
            row.write_string(version_buffer);              // node_version
            row.write_uint32(processInfo->getPid());       // process_id
            row.write_uint32(processInfo->getAngelPid());  // angel_process_id
            row.write_string(processInfo->getProcessName());  // process_name
            row.write_string(uri_buffer);                     // service_URI
            ndbinfo_send_row(signal, req, row, rl);
          } else if (nodeInfo.m_type != NodeInfo::DB &&
                     nodeInfo.m_version > 0 &&
                     !ndbd_supports_processinfo(nodeInfo.m_version)) {
            /* MGM/API node is too old to send ProcessInfoRep, so create a
               fallback-style report */

            ndb_sockaddr addr =
                globalTransporterRegistry.get_connect_address_node(i);
            char service_uri[INET6_ADDRSTRLEN + 6];
            strcpy(service_uri, "ndb://");
            Ndb_inet_ntop(&addr, service_uri + 6, 46);

            Ndbinfo::Row row(signal, req);
            row.write_uint32(getOwnNodeId());      // reporting_node_id
            row.write_uint32(i);                   // node_id
            row.write_uint32(nodeInfo.getType());  // node_type
            row.write_string(version_buffer);      // node_version
            row.write_uint32(0);                   // process_id
            row.write_uint32(0);                   // angel_process_id
            row.write_string("");                  // process_name
            row.write_string(service_uri);         // service_URI
            ndbinfo_send_row(signal, req, row, rl);
          }
        }
      }
      break;
    }
    default:
      break;
  }
  ndbinfo_send_scan_conf(signal, req, rl);
}

void Qmgr::execPROCESSINFO_REP(Signal *signal) {
  jamEntry();
  ProcessInfoRep *report = (ProcessInfoRep *)signal->theData;
  SectionHandle handle(this, signal);
  SegmentedSectionPtr pathSectionPtr, hostSectionPtr;

  ndbrequire(report->node_id < MAX_NODES);
  ProcessInfo *processInfo = getProcessInfo(report->node_id);
  if (processInfo) {
    /* Set everything except the connection name and host address */
    processInfo->initializeFromProcessInfoRep(report);

    /* Set the URI path */
    if (handle.getSection(pathSectionPtr, ProcessInfoRep::PathSectionNum)) {
      processInfo->setUriPath(pathSectionPtr.p->theData);
    }

    /* Set the host address */
    if (handle.getSection(hostSectionPtr, ProcessInfoRep::HostSectionNum)) {
      processInfo->setHostAddress(hostSectionPtr.p->theData);
    } else {
      /* Use the address from the transporter registry.
         As implemented below we use setHostAddress() with struct in_addr
         to set an IPv4 address.  An alternate more abstract version
         of ProcessInfo::setHostAddress() is also available, which
         takes a struct sockaddr * and length.
      */
      ndb_sockaddr addr =
          globalTransporterRegistry.get_connect_address_node(report->node_id);
      processInfo->setHostAddress(&addr);
    }
  }
  releaseSections(handle);
}

void Qmgr::execISOLATE_ORD(Signal *signal) {
  jamEntry();

  IsolateOrd *sig = (IsolateOrd *)signal->theData;

  ndbrequire(sig->senderRef != 0);
  Uint32 sz;
  Uint32 num_sections = signal->getNoOfSections();
  SectionHandle handle(this, signal);
  if (num_sections) {
    jam();
    ndbrequire(num_sections == 1);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(sig->nodesToIsolate, ptr);
    sz = ptr.sz;
  } else {
    jam();
    ndbrequire(signal->getLength() == IsolateOrd::SignalLengthWithBitmask48);
    memset(sig->nodesToIsolate + NdbNodeBitmask48::Size, 0,
           _NDB_NBM_DIFF_BYTES);
    sz = NdbNodeBitmask::Size;
  }
  NdbNodeBitmask victims;
  memset(&victims, 0, sizeof(victims));
  victims.assign(sz, sig->nodesToIsolate);
  ndbrequire(!victims.isclear());

  switch (sig->isolateStep) {
    case IsolateOrd::IS_REQ: {
      jam();
      releaseSections(handle);
      /* Initial request, broadcast immediately */

      /* Need to get the set of live nodes to broadcast to */
      NdbNodeBitmask hitmen(c_clusterNodes);

      sig->isolateStep = IsolateOrd::IS_BROADCAST;
      unsigned nodeId = hitmen.find_first();
      do {
        jam();
        BlockReference ref = calcQmgrBlockRef(nodeId);
        if (ndbd_send_node_bitmask_in_section(getNodeInfo(nodeId).m_version)) {
          jam();
          LinearSectionPtr lsptr[3];
          lsptr[0].p = (Uint32 *)&victims;
          lsptr[0].sz = victims.getPackedLengthInWords();
          sendSignal(ref, GSN_ISOLATE_ORD, signal, IsolateOrd::SignalLength,
                     JBA, lsptr, 1);
        } else {
          jam();
          ndbrequire(victims.getPackedLengthInWords() <= 2);
          memset(&sig->nodesToIsolate, 0, 8);
          memcpy(&sig->nodesToIsolate, &victims,
                 4 * victims.getPackedLengthInWords());
          sendSignal(ref, GSN_ISOLATE_ORD, signal,
                     IsolateOrd::SignalLengthWithBitmask48, JBA);
        }
        nodeId = hitmen.find_next(nodeId + 1);
      } while (nodeId != BitmaskImpl::NotFound);

      ndbrequire(!hitmen.isclear()); /* At least me */
      return;
    }
    case IsolateOrd::IS_BROADCAST: {
      jam();
      /* Received request, delay */
      sig->isolateStep = IsolateOrd::IS_DELAY;

      if (sig->delayMillis > 0) {
        /* Delay processing until delayMillis passes */
        jam();
        sendSignalWithDelay(reference(), GSN_ISOLATE_ORD, signal,
                            sig->delayMillis, IsolateOrd::SignalLength,
                            &handle);
        return;
      }
    }
      [[fallthrough]];
    case IsolateOrd::IS_DELAY: {
      jam();

      releaseSections(handle);
      if (ERROR_INSERTED(942)) {
        jam();
        g_eventLogger->info("QMGR discarding IsolateRequest");
        return;
      }

      /* Map to FAIL_REP signal(s) */
      Uint32 failSource = refToNode(sig->senderRef);

      unsigned nodeId = victims.find_first();
      do {
        jam();

        /* TODO : Consider checking node state and skipping if
         * failing already
         * Consider logging that action is being taken here
         */

        FailRep *failRep = (FailRep *)&signal->theData[0];
        failRep->failNodeId = nodeId;
        failRep->failCause = FailRep::ZFORCED_ISOLATION;
        failRep->failSourceNodeId = failSource;

        sendSignal(reference(), GSN_FAIL_REP, signal, 3, JBA);

        nodeId = victims.find_next(nodeId + 1);
      } while (nodeId != BitmaskImpl::NotFound);

      /* Fail rep signals are en-route... */

      return;
    }
  }

  ndbabort();
}

void Qmgr::execNODE_STATE_REP(Signal *signal) {
  jam();
  const NodeState prevState = getNodeState();
  SimulatedBlock::execNODE_STATE_REP(signal);
  const NodeState newState = getNodeState();

  /* Check whether we are changing state */
  if (prevState.startLevel != newState.startLevel ||
      prevState.nodeGroup != newState.nodeGroup) {
    jam();
    /* Inform APIs */
    signal->theData[0] = ZNOTIFY_STATE_CHANGE;
    signal->theData[1] = 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }

  return;
}

void Qmgr::handleStateChange(Signal *signal, Uint32 nodeToNotify) {
  jam();
  bool take_a_break = false;

  do {
    const NodeInfo::NodeType nt = getNodeInfo(nodeToNotify).getType();

    if (nt == NodeInfo::API || nt == NodeInfo::MGM) {
      jam();

      NodeRecPtr notifyNode;
      notifyNode.i = nodeToNotify;
      ptrCheckGuard(notifyNode, MAX_NODES, nodeRec);

      if (notifyNode.p->phase == ZAPI_ACTIVE) {
        jam();
        ndbassert(c_connectedNodes.get(nodeToNotify));

        /**
         * Ok, send an unsolicited API_REGCONF to inform
         * the API of the state change
         */
        set_hb_count(nodeToNotify) = 0;
        sendApiRegConf(signal, nodeToNotify);

        take_a_break = true;
      }
    }

    nodeToNotify++;
  } while (nodeToNotify < MAX_NODES && !take_a_break);

  if (nodeToNotify < MAX_NODES) {
    jam();
    signal->theData[0] = ZNOTIFY_STATE_CHANGE;
    signal->theData[1] = nodeToNotify;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }

  return;
}

/**
 * SET_UP_MULTI_TRP_REQ starts the setup of multi socket transporters
 * that currently is setup between two data nodes in the same node group.
 * This signal is sent in start phase 3 from NDBCNTR  when we are performing
 * an initial start or a cluster restart at a time when we know the version
 * info about other data nodes. For node restarts it is sent later in phase
 * 4 when the master has informed us of the current sysfile. We need to wait
 * for this to ensure that we know the node group information for all nodes.
 * We will only allow one use of SET_UP_MULTI_TRP_REQ per start of a data
 * node. We can still participate in setting up multi sockets after that,
 * but only when another node is starting and requesting us to assist in
 * setting up a multi socket setup.
 *
 * We cannot use multi sockets towards versions before MySQL Cluster
 * 8.0.20.
 *
 * The signal flow to accomplish this setup of multi sockets is the
 * following. It is currently only possible to setup when a node is
 * starting up, but some parts of the code is prepared to also handle
 * this change while the cluster is operational.
 *
 * The protocol below assumes that both node support multi sockets.
 *
 * NDBCNTR/DBDIH          QMGR                              QMGR neighbour
 *    SET_UP_MULTI_TRP_REQ
 *    ------------------->
 *
 * Scenario 1: QMGR Neighbour starts after first QMGR
 *                        GET_NUM_MULTI_TRP_REQ
 *                        ------------------------------------->
 *                        GET_NUM_MULTI_TRP_CONF
 *                        <------------------------------------
 *                     Create multi transporters
 *                     Connect multi transporters
 *
 *                        GET_NUM_MULTI_TRP_REQ
 *                        <------------------------------------
 *                        GET_NUM_MULTI_TRP_CONF
 *                        ------------------------------------>
 *                                                  Create multi transporter
 *                                                  Connect multi transporter
 *                       Multi transporters connect to each other
 *
 * QMGR                                                  QMGR Neighbour
 *     SWITCH_MULTI_TRP_REQ
 *   ---------------------------------------------------------->
 *                                                       When QMGR neighbour
 *                                                       has added to epoll
 *                                                       set.
 *     SWITCH_MULTI_TRP_REQ
 *   <---------------------------------------------------------
 *     SWITCH_MULTI_TRP_CONF
 *   <-------------------------------------------------------->
 *     Now both nodes are ready to perform the actual switch over
 *
 *  QMGR               THRMAN Proxy                 THRMAN
 *    FREEZE_THREAD_REQ
 *    ---------------------->
 *                           FREEZE_THREAD_REQ
 *                           -------------------------->>
 *                                                   Freeze all threads
 *                                                   except main thread
 *              FREEZE_ACTION_REQ
 *    <--------------------------------------------------
 *    Switch to using multi transporter sockets
 *
 * At this point the only thread that is active is the main thread.
 * Every other thread is frozen waiting to be woken up when the
 * new multi socket setup is set up. We will send the last signal
 * ACTIVATE_TRP_REQ on the old transporter, before we send that we
 * ensure that we have locked all send transporters and after that
 * we enable the send buffer and after that all signals will be
 * sent on the new multi sockets.
 *
 * QMGR                  THRMAN (main thread)            QMGR Neighbour
 *       ACTIVATE_TRP_REQ
 *   -------------------------------------------------------->
 *       FREEZE_ACTION_CONF
 *   -------------------------->
 *                           unlock all thread
 *                           wait until all threads woken up again
 *       FREEZE_THREAD_CONF
 *   <--------------------------
 *
 * In parallel with the above we will also do the same thing in the
 * neighbour node and this node will initiate the second round of
 * events when we receive the signal ACTIVATE_TRP_REQ.
 *
 * QMGR         TRPMAN Proxy     TRPMAN                  QMGR Neighbour
 *       ACTIVATE_TRP_REQ
 *   <--------------------------------------------------------
 *   SYNC_THREAD_VIA_REQ
 *   --------------->
 *                   SYNC_THREAD_VIA_REQ
 *                   --------------->>                  THRMANs
 *                                    SYNC_THREAD_REQ
 *                                    -------------------->>
 *                                    SYNC_THREAD_CONF
 *                                    <<--------------------
 *                   SYNC_THREAD_VIA_CONF
 *                   <<---------------
 *   SYNC_THREAD_VIA_CONF
 *   <---------------
 *
 * SYNC_THREAD_VIA_REQ/CONF is used to ensure that all receive threads
 * have delivered any signals it has received. Since at this point we
 * haven't activated the new multi sockets, and we have deactivated
 * the old socket, this means that we have a clear signal order in that
 * signal sent on old socket is always delivered to all other threads
 * before any new signal on the new multi socket transporters are
 * delivered.
 *
 *   <---------------
 *     ACTIVATE_TRP_REQ
 *   --------------->-------------->>
 *                                Activate the receive on the
 *                                new transporters
 *     ACTIVATE_TRP_CONF
 *   <<------------------------------
 *     ACTIVATE_TRP_CONF
 *   --------------------------------------------------------->
 *                                                           Here the
 *                                                        switch is completed
 *  After receiving ACTIVATE_TRP_CONF we have no use of the socket anymore
 *  and since the sender obviously has also
 *
 * If more nodes are in node group to also set up we do it after this.
 * Otherwise we are ready.
 *
 *  QMGR                           NDBCNTR/DBDIH
 *      SET_UP_MULTI_TRP_CONF
 *    ------------------------------->
 */
void Qmgr::execSET_UP_MULTI_TRP_REQ(Signal *signal) {
  jamEntry();
  if (m_ref_set_up_multi_trp_req != 0) {
    jam();
    DEB_MULTI_TRP(("Already handled SET_UP_MULTI_TRP_REQ"));
    sendSignal(signal->theData[0], GSN_SET_UP_MULTI_TRP_CONF, signal, 1, JBB);
    return;
  }
  m_ref_set_up_multi_trp_req = signal->theData[0];
  m_get_num_multi_trps_sent = 0;
  for (Uint32 node_id = 1; node_id < MAX_NDB_NODES; node_id++) {
    NodeRecPtr nodePtr;
    nodePtr.i = node_id;
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->m_used_num_multi_trps = m_num_multi_trps;
    nodePtr.p->m_initial_set_up_multi_trp_done = false;
  }
  DEB_MULTI_TRP(("m_num_multi_trps = %u", m_num_multi_trps));
  bool done = false;
  bool completed = get_num_multi_trps(signal, done);
  if (!completed) {
    jam();
    return;
  } else {
    jam();
    DEB_MULTI_TRP(("m_num_multi_trps == 1, no need to setup multi sockets"));
  }
  complete_multi_trp_setup(signal, done);
}

void Qmgr::get_node_group_mask(Signal *signal, NdbNodeBitmask &mask) {
  CheckNodeGroups *sd = (CheckNodeGroups *)signal->getDataPtrSend();
  sd->blockRef = reference();
  sd->requestType =
      CheckNodeGroups::Direct | CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = getOwnNodeId();
  EXECUTE_DIRECT_MT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                    CheckNodeGroups::SignalLength, 0);
  jamEntry();
  mask.assign(sd->mask);
  mask.clear(getOwnNodeId());
}

bool Qmgr::get_num_multi_trps(Signal *signal, bool &done) {
  jamEntry();
  NdbNodeBitmask mask;
  get_node_group_mask(signal, mask);
  m_get_num_multi_trps_sent++;
  if (m_num_multi_trps == 1) {
    jam();
    done = true;
  }
  for (Uint32 node_id = 1; node_id < MAX_NDB_NODES; node_id++) {
    if (mask.get(node_id)) {
      jam();
      jamLine(node_id);
      DEB_MULTI_TRP(("Node %u is in the same node group", node_id));
      NodeRecPtr nodePtr;
      nodePtr.i = node_id;
      ptrAss(nodePtr, nodeRec);
      nodePtr.p->m_is_in_same_nodegroup = true;
      done = true;
      Uint32 version = getNodeInfo(nodePtr.i).m_version;
      if (m_num_multi_trps > 1) {
        create_multi_transporter(nodePtr.i);
        if (nodePtr.p->phase == ZRUNNING && ndbd_use_multi_ng_trps(version) &&
            (c_ndbcntr->is_node_started(nodePtr.i) ||
             c_ndbcntr->is_node_starting(nodePtr.i))) {
          jam();
          if (ERROR_INSERTED(970)) {
            NdbSleep_MilliSleep(500);
          }
          nodePtr.p->m_set_up_multi_trp_started = true;
          inc_get_num_multi_trps_sent(nodePtr.i);
          send_get_num_multi_trp_req(signal, node_id);
        }
      }
    }
  }
  m_get_num_multi_trps_sent--;
  return (m_get_num_multi_trps_sent == 0);
}

void Qmgr::execGET_NUM_MULTI_TRP_REQ(Signal *signal) {
  jamEntry();
  GetNumMultiTrpReq *req = (GetNumMultiTrpReq *)&signal->theData[0];
  Uint32 sender_node_id = req->nodeId;

  NodeRecPtr nodePtr;
  nodePtr.i = sender_node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->m_initial_set_up_multi_trp_done =
      req->initial_set_up_multi_trp_done;
  /*
   * Set used number of multi sockets to be minimum of our own config
   * and the node config of the node contacting us.
   */
  nodePtr.p->m_used_num_multi_trps = MIN(req->numMultiTrps, m_num_multi_trps);

  if (m_initial_set_up_multi_trp_done && nodePtr.p->m_used_num_multi_trps > 1) {
    /**
     * We passed the startup phase 2 where the connection setup
     * of multi transporters happens normally. So the node sending
     * this message is a new node starting and we're either already
     * started or have passed phase 2 of the startup. We will start
     * enabling communication to this new node.
     *
     * This is only required if we want to use more than one socket.
     */
    jam();
    DEB_MULTI_TRP(("Node %u starting, prepare switch trp using %u trps",
                   sender_node_id, nodePtr.p->m_used_num_multi_trps));
    connect_multi_transporter(signal, sender_node_id);
    if (ERROR_INSERTED(972)) {
      NdbSleep_MilliSleep(500);
    }
  } else {
    jam();
    if (ERROR_INSERTED(971)) {
      NdbSleep_MilliSleep(500);
    }
  }
  if (m_ref_set_up_multi_trp_req != 0) {
    jam();
    DEB_MULTI_TRP(
        ("Node %u starting, sent GET_NUM_MULTI_TRP_REQ, get"
         " num multi %u",
         sender_node_id, nodePtr.p->m_used_num_multi_trps));
    GetNumMultiTrpConf *conf = (GetNumMultiTrpConf *)signal->getDataPtrSend();
    conf->numMultiTrps = nodePtr.p->m_used_num_multi_trps;
    conf->nodeId = getOwnNodeId();
    conf->initial_set_up_multi_trp_done = m_initial_set_up_multi_trp_done;

    BlockReference ref = calcQmgrBlockRef(sender_node_id);
    sendSignal(ref, GSN_GET_NUM_MULTI_TRP_CONF, signal,
               GetNumMultiTrpConf::SignalLength, JBB);
  } else {
    jam();
    DEB_MULTI_TRP(
        ("Node %u starting, GET_NUM_MULTI_TRP_REQ sent,"
         " we're not ready",
         sender_node_id));
    GetNumMultiTrpRef *ref = (GetNumMultiTrpRef *)signal->getDataPtrSend();
    ref->nodeId = getOwnNodeId();
    ref->errorCode = GetNumMultiTrpRef::NotReadyYet;
    BlockReference block_ref = calcQmgrBlockRef(sender_node_id);
    sendSignal(block_ref, GSN_GET_NUM_MULTI_TRP_REF, signal,
               GetNumMultiTrpRef::SignalLength, JBB);
  }
}

void Qmgr::execGET_NUM_MULTI_TRP_REF(Signal *signal) {
  GetNumMultiTrpRef ref = *(GetNumMultiTrpRef *)&signal->theData[0];
  /**
   * The other node is not ready yet, we'll wait for it to become ready before
   * progressing.
   */
  NodeRecPtr nodePtr;
  nodePtr.i = ref.nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->m_count_multi_trp_ref++;
  if (nodePtr.p->m_count_multi_trp_ref > 60) {
    jam();
    nodePtr.p->m_count_multi_trp_ref = 0;
    DEB_MULTI_TRP(("GET_NUM_MULTI_TRP_REF 60 times from %u", ref.nodeId));
    ndbassert(false);
    dec_get_num_multi_trps_sent(ref.nodeId);
    complete_multi_trp_setup(signal, false);
    return;
  }
  DEB_MULTI_TRP(("GET_NUM_MULTI_TRP_REF received from %u", ref.nodeId));
  signal->theData[0] = ZRESEND_GET_NUM_MULTI_TRP_REQ;
  signal->theData[1] = ref.nodeId;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 500, 2);
}

void Qmgr::complete_multi_trp_setup(Signal *signal, bool set_done) {
  if (m_get_num_multi_trps_sent == 0) {
    jam();
    if (set_done) {
      jam();
      m_initial_set_up_multi_trp_done = true;
    }
    sendSignal(m_ref_set_up_multi_trp_req, GSN_SET_UP_MULTI_TRP_CONF, signal, 1,
               JBB);
    if (!set_done) {
      jam();
      m_ref_set_up_multi_trp_req = 0;
    }
  } else {
    jam();
  }
}

void Qmgr::send_get_num_multi_trp_req(Signal *signal, NodeId node_id) {
  if (m_get_num_multi_trps_sent == 0) {
    jam();
    DEB_MULTI_TRP(
        ("We have already completed the SET_UP_MULTI_TRP_REQ"
         ", no need to continue retrying"));
    complete_multi_trp_setup(signal, false);
    return;
  }
  jam();
  DEB_MULTI_TRP(("Get num multi trp for node %u", node_id));
  GetNumMultiTrpReq *req = (GetNumMultiTrpReq *)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->numMultiTrps = m_num_multi_trps;
  req->initial_set_up_multi_trp_done = false;
  BlockReference ref = calcQmgrBlockRef(node_id);
  sendSignal(ref, GSN_GET_NUM_MULTI_TRP_REQ, signal,
             GetNumMultiTrpReq::SignalLength, JBB);
}

void Qmgr::inc_get_num_multi_trps_sent(NodeId node_id) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  ndbrequire(!nodePtr.p->m_is_get_num_multi_trp_active);
  m_get_num_multi_trps_sent++;
  nodePtr.p->m_is_get_num_multi_trp_active = true;
}

void Qmgr::dec_get_num_multi_trps_sent(NodeId node_id) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  ndbrequire(m_get_num_multi_trps_sent > 0);
  ndbrequire(nodePtr.p->m_is_get_num_multi_trp_active);
  m_get_num_multi_trps_sent--;
  nodePtr.p->m_is_get_num_multi_trp_active = false;
}

void Qmgr::execGET_NUM_MULTI_TRP_CONF(Signal *signal) {
  /**
   * We receive the number of sockets to use from the other node. Could
   * also be a signal we sent to ourselves if the other node isn't
   * started yet or is running a version not supporting multi sockets.
   * In these cases the number of sockets will always be 1.
   */
  jamEntry();
  CRASH_INSERTION(951);
  GetNumMultiTrpConf *conf = (GetNumMultiTrpConf *)&signal->theData[0];
  Uint32 sender_node_id = conf->nodeId;
  NodeRecPtr nodePtr;
  nodePtr.i = sender_node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  nodePtr.p->m_count_multi_trp_ref = 0;
  Uint32 rec_num_multi_trps = conf->numMultiTrps;
  Uint32 initial_set_up_multi_trp_done = conf->initial_set_up_multi_trp_done;
  ndbrequire(nodePtr.p->m_used_num_multi_trps > 0);
  ndbrequire(rec_num_multi_trps <= m_num_multi_trps);
  /**
   * If the other side cannot handle the number of multi sockets we wanted,
   * we set it to the other sides number instead.
   */
  nodePtr.p->m_used_num_multi_trps =
      MIN(conf->numMultiTrps, nodePtr.p->m_used_num_multi_trps);
  nodePtr.p->m_initial_set_up_multi_trp_done = initial_set_up_multi_trp_done;
  dec_get_num_multi_trps_sent(nodePtr.i);
  if (rec_num_multi_trps == 1) {
    jam();
    DEB_MULTI_TRP(("No need to setup multi sockets to node %u", nodePtr.i));
    complete_multi_trp_setup(signal, true);
    return;
  }
  DEB_MULTI_TRP(("GET_NUM_MULTI_TRP_CONF received from %u using %u trps",
                 sender_node_id, nodePtr.p->m_used_num_multi_trps));
  jam();
  connect_multi_transporter(signal, nodePtr.i);
  if (ERROR_INSERTED(973)) {
    NdbSleep_MilliSleep(1500);
  }
}

void Qmgr::create_multi_transporter(NodeId node_id) {
  jamEntry();
  DEB_MULTI_TRP(("Create multi trp for node %u", node_id));
  globalTransporterRegistry.createMultiTransporter(node_id, m_num_multi_trps);
}

/*
 * TRP_KEEP_ALIVE
 */

void Qmgr::send_trp_keep_alive_start(Signal *signal) {
  jam();
  c_keepalive_seqnum++;
  if (c_keep_alive_send_in_progress) {
    jam();
    g_eventLogger->warning(
        "Sending keep alive messages on all links is slow, "
        "skipping one round (%u) of sending.",
        c_keepalive_seqnum);
    return;
  }
  c_keep_alive_send_in_progress = true;
  constexpr Uint32 node_id = 0;
  signal->theData[0] = ZSEND_TRP_KEEP_ALIVE;
  signal->theData[1] = node_id;
  signal->theData[2] = c_keepalive_seqnum;
  send_trp_keep_alive(signal);
}

void Qmgr::send_trp_keep_alive(Signal *signal) {
  jam();

  Uint32 node_id = signal->theData[1];
  Uint32 keepalive_seqnum = signal->theData[2];

  if ((node_id = c_clusterNodes.find(node_id)) != NdbNodeBitmask::NotFound) {
    jam();
    NodeInfo nodeInfo = getNodeInfo(node_id);
    ndbrequire(nodeInfo.m_type == NodeInfo::DB);
    if (node_id != getOwnNodeId() && nodeInfo.m_version != 0 &&
        ndbd_support_trp_keep_alive(nodeInfo.m_version)) {
      jam();

      const BlockReference qmgr_ref = calcQmgrBlockRef(node_id);

      TrpKeepAlive *sig = reinterpret_cast<TrpKeepAlive *>(signal->theData);
      sig->senderRef = reference();
      sig->keepalive_seqnum = keepalive_seqnum;
      Signal25 *signal25 = reinterpret_cast<Signal25 *>(signal);
      sendSignalOverAllLinks(qmgr_ref, GSN_TRP_KEEP_ALIVE, signal25, 2, JBB);
    }
    node_id++;
  }

  if (node_id == NdbNodeBitmask::NotFound) {
    jam();
    c_keep_alive_send_in_progress = false;
    return;
  }

  signal->theData[0] = ZSEND_TRP_KEEP_ALIVE;
  signal->theData[1] = node_id;
  signal->theData[2] = keepalive_seqnum;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBA);
}

#include "../../../common/transporter/Multi_Transporter.hpp"
#include "../../../common/transporter/Transporter.hpp"

void Qmgr::connect_multi_transporter(Signal *signal, NodeId node_id) {
  /**
   * We have created the Multi transporters, now it is time to setup
   * connections to those that are running and also to switch over to
   * using the multi transporter. We currently only perform this as
   * part of startup. This means that if a node is already started
   * it is the responsibility of the starting node always to perform
   * the setup. If both nodes are starting the node with lowest node
   * id is responsible for the setup.
   */
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->m_check_multi_trp_connect_loop_count = 0;
  nodePtr.p->m_is_preparing_switch_trp = true;
  /**
   * Connect a multi-transporter.
   * For clients this happens by moving the transporters inside the
   * multi-transporter into the allTransporters array and initiate the
   * CONNECTING protocol with start_connecting(). The multiTransporter parts
   * then connects as any other transporter and finally report_connect'ed.
   * QMGR will wait until all parts of the MultiTransporter has CONNECTED,
   * then 'switch' the MultiTransporter.
   *
   * To differentiate between normal transporters and these transporters
   * that are part of a multi-transporter we have a method called
   * isPartOfMultiTransporter. The method set_part_of_multi_transporter
   * toggles this state, by default it is false.
   *
   * By replacing the position in theNodeIdTransporters with a
   * multi transporter we ensure that connect_server will handle the
   * connection properly.
   */
  Multi_Transporter *multi_trp =
      globalTransporterRegistry.get_node_multi_transporter(node_id);
  ndbrequire(multi_trp != nullptr);

  globalTransporterRegistry.lockMultiTransporters();
  multi_trp->set_num_inactive_transporters(nodePtr.p->m_used_num_multi_trps);
  Uint32 num_inactive_transporters = multi_trp->get_num_inactive_transporters();

  for (Uint32 i = 0; i < num_inactive_transporters; i++) {
    jam();
    Transporter *t = multi_trp->get_inactive_transporter(i);
    globalTransporterRegistry.insert_allTransporters(t);
    assign_recv_thread_new_trp(t->getTransporterIndex());
    DEB_MULTI_TRP(
        ("Start connecting trp id %u for node %u, mti = %u, server: %u",
         t->getTransporterIndex(), node_id, t->get_multi_transporter_instance(),
         t->isServer));
    globalTransporterRegistry.start_connecting(t->getTransporterIndex());
  }
  globalTransporterRegistry.unlockMultiTransporters();
  signal->theData[0] = ZCHECK_MULTI_TRP_CONNECT;
  signal->theData[1] = node_id;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 2);
}

void Qmgr::check_connect_multi_transporter(Signal *signal, NodeId node_id) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  globalTransporterRegistry.lockMultiTransporters();
  Multi_Transporter *multi_trp =
      globalTransporterRegistry.get_node_multi_transporter(node_id);
  if (nodePtr.p->phase == ZRUNNING) {
    jam();
    bool connected = true;
    Uint32 num_inactive_transporters =
        multi_trp->get_num_inactive_transporters();
    for (Uint32 i = 0; i < num_inactive_transporters; i++) {
      jam();
      Transporter *tmp_trp = multi_trp->get_inactive_transporter(i);
      const TrpId trpId = tmp_trp->getTransporterIndex();
      const bool is_connected = globalTransporterRegistry.is_connected(trpId);
      if (!is_connected) {
        jam();
        connected = false;
        break;
      }
    }
    if (!connected) {
      jam();
      globalTransporterRegistry.unlockMultiTransporters();
      nodePtr.p->m_check_multi_trp_connect_loop_count++;
      /**
       * We are only connecting to nodes already connected, thus we
       * should not fail to connect here, just in case something
       * weird happens we will still fail after waiting for
       * 30 minutes (100 * 30 * 60 times sending 10ms delayed signal).
       */
      ndbrequire(nodePtr.p->m_check_multi_trp_connect_loop_count <
                 (100 * 60 * 30));
      signal->theData[0] = ZCHECK_MULTI_TRP_CONNECT;
      signal->theData[1] = node_id;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 2);
      return;
    }
    DEB_MULTI_TRP(("Multi trp connected for node %u", node_id));
    globalTransporterRegistry.unlockMultiTransporters();
    ndbrequire(nodePtr.p->m_is_multi_trp_setup == false);
    nodePtr.p->m_is_multi_trp_setup = true;
    if (!check_all_multi_trp_nodes_connected()) {
      jam();
      /* We are not ready to start switch process yet. */
      return;
    }
    if (!select_node_id_for_switch(node_id, true)) {
      /**
       * We were already busy with a switch, could also be
       * that we didn't find any lower node id to switch to.
       * We will only initiate switch from nodes with lower
       * node ids than our node id.
       *
       * By always selecting the highest node id to start with,
       * we ensure that we select a node that hasn't initiated
       * any switch on their own. Thus we are certain that this
       * node will eventually accept our switch request even if
       * it has to process all the other neighbour nodes before
       * us. This is definitely not an optimal algorithm, but it
       * is safe in that it avoids deadlock that could lead to
       * eternal wait states.
       */
      jam();
      return;
    }
    // Done as part of switch_multi_transporter as well:
    assign_multi_trps_to_send_threads();
    send_switch_multi_transporter(signal, node_id, false);
    return;
  } else {
    /**
     * The connection is no longer using the Multi_Transporter object.
     * Can only happen when the connection is broken before we completed
     * the connection setup of all connections. No need to do anything
     * more in this case other than release mutex.
     */
    jam();
    if (ERROR_INSERTED(974)) {
      NdbSleep_MilliSleep(1500);
    }
    nodePtr.p->m_is_preparing_switch_trp = false;
    globalTransporterRegistry.unlockMultiTransporters();
    check_more_trp_switch_nodes(signal);
  }
  return;
}

void Qmgr::send_switch_multi_transporter(Signal *signal, NodeId node_id,
                                         bool retry) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  jam();
  if (!retry) {
    jam();
    ndbrequire(m_current_switch_multi_trp_node == 0);
  } else if (m_current_switch_multi_trp_node == node_id) {
    jam();
    DEB_MULTI_TRP(
        ("Retry of send SWITCH_MULTI_TRP_REQ to node %u"
         " not needed since already ongoing",
         node_id));
    return;
  } else if (m_current_switch_multi_trp_node != 0) {
    jam();
    DEB_MULTI_TRP(
        ("Retry of send SWITCH_MULTI_TRP_REQ to node %u"
         " failed since other node already started",
         node_id));
    return;
  } else if (nodePtr.p->m_is_using_multi_trp) {
    jam();
    DEB_MULTI_TRP(
        ("Retry of send SWITCH_MULTI_TRP_REQ to node %u"
         " not needed since already setup",
         node_id));
    return;
  } else {
    jam();
    DEB_MULTI_TRP(("Retry of SWITCH_MULTI_TRP_REQ to node %u", node_id));
  }
  m_current_switch_multi_trp_node = node_id;
  nodePtr.p->m_is_ready_to_switch_trp = true;
  DEB_MULTI_TRP(("Send SWITCH_MULTI_TRP_REQ to node %u", node_id));
  SwitchMultiTrpReq *req = (SwitchMultiTrpReq *)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->senderRef = reference();
  BlockReference ref = calcQmgrBlockRef(node_id);
  sendSignal(ref, GSN_SWITCH_MULTI_TRP_REQ, signal,
             SwitchMultiTrpReq::SignalLength, JBB);
  if (ERROR_INSERTED(978)) {
    NdbSleep_MilliSleep(1500);
  }
}

void Qmgr::execSWITCH_MULTI_TRP_REQ(Signal *signal) {
  SwitchMultiTrpReq *req = (SwitchMultiTrpReq *)&signal->theData[0];
  NodeId node_id = req->nodeId;
  BlockReference block_ref = req->senderRef;
  DEB_MULTI_TRP(("SWITCH_MULTI_TRP_REQ node %u", node_id));
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  assign_multi_trps_to_send_threads();

  CRASH_INSERTION(954);
  if (!check_all_multi_trp_nodes_connected()) {
    if (nodePtr.p->m_is_multi_trp_setup &&
        m_current_switch_multi_trp_node == 0) {
      ndbrequire(nodePtr.p->phase == ZRUNNING);
      ndbrequire(nodePtr.p->m_is_in_same_nodegroup);
      ndbrequire(nodePtr.p->m_is_preparing_switch_trp);
      /* Fall through to send SWITCH_MULTI_TRP_CONF */
    } else {
      jam();
      ndbrequire(m_current_switch_multi_trp_node != node_id);
      DEB_MULTI_TRP(("Send SWITCH_MULTI_TRP_REF node %u", node_id));
      SwitchMultiTrpRef *ref = (SwitchMultiTrpRef *)signal->getDataPtrSend();
      ref->nodeId = getOwnNodeId();
      ref->errorCode = SwitchMultiTrpRef::SMTR_NOT_READY_FOR_SWITCH;
      sendSignal(block_ref, GSN_SWITCH_MULTI_TRP_REF, signal,
                 SwitchMultiTrpRef::SignalLength, JBB);
      return;
    }
  } else if (m_current_switch_multi_trp_node != 0 &&
             m_current_switch_multi_trp_node != node_id) {
    /**
     * We are already trying to connect multi sockets to another
     * node. We will wait for this to complete before moving
     * on to the next node.
     */
    jam();
    DEB_MULTI_TRP(("2:Send SWITCH_MULTI_TRP_REF node %u", node_id));
    SwitchMultiTrpRef *ref = (SwitchMultiTrpRef *)signal->getDataPtrSend();
    ref->nodeId = getOwnNodeId();
    ref->errorCode = SwitchMultiTrpRef::SMTR_NOT_READY_FOR_SWITCH;
    sendSignal(block_ref, GSN_SWITCH_MULTI_TRP_REF, signal,
               SwitchMultiTrpRef::SignalLength, JBB);
    return;
  }
  /**
   * We haven't selected any node to connect multi sockets to yet.
   * In that case it is safe to answer positively since we know
   * that this cannot cause any deadlock.
   */
  if (m_current_switch_multi_trp_node == 0) {
    jam();
    ndbrequire(!nodePtr.p->m_is_ready_to_switch_trp);
    SwitchMultiTrpReq *req = (SwitchMultiTrpReq *)signal->getDataPtrSend();
    req->nodeId = getOwnNodeId();
    req->senderRef = reference();
    BlockReference ref = calcQmgrBlockRef(node_id);
    sendSignal(ref, GSN_SWITCH_MULTI_TRP_REQ, signal,
               SwitchMultiTrpReq::SignalLength, JBB);
  } else {
    ndbrequire(m_current_switch_multi_trp_node == node_id);
  }
  ndbrequire(nodePtr.p->m_is_multi_trp_setup)
      nodePtr.p->m_is_ready_to_switch_trp = true;
  m_current_switch_multi_trp_node = node_id;
  jam();
  DEB_MULTI_TRP(("Send SWITCH_MULTI_TRP_CONF node %u", node_id));
  if (ERROR_INSERTED(979)) {
    NdbSleep_MilliSleep(1500);
  }
  SwitchMultiTrpConf *conf = (SwitchMultiTrpConf *)signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  sendSignal(block_ref, GSN_SWITCH_MULTI_TRP_CONF, signal,
             SwitchMultiTrpConf::SignalLength, JBB);
}

void Qmgr::execSWITCH_MULTI_TRP_CONF(Signal *signal) {
  /**
   * This signal can get lost if the other node fails and we have
   * already started.
   *
   * The TransporterRegistry will ensure that we switch back to using a
   * single transporter in this case, the DISCONNECT_REP code and the
   * NODE_FAILREP code will ensure that we reset the variables used
   * to setup the multi sockets next time the node starts up.
   */
  jamEntry();
  CRASH_INSERTION(955);
  SwitchMultiTrpConf *conf = (SwitchMultiTrpConf *)&signal->theData[0];
  Uint32 node_id = conf->nodeId;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  ndbrequire(nodePtr.p->m_is_ready_to_switch_trp == true);
  ndbrequire(nodePtr.p->m_is_multi_trp_setup == true);
  DEB_MULTI_TRP(("Recvd SWITCH_MULTI_TRP_CONF node %u", node_id));
  if (ERROR_INSERTED(980)) {
    NdbSleep_MilliSleep(1500);
  }
  switch_multi_transporter(signal, node_id);
}

void Qmgr::execSWITCH_MULTI_TRP_REF(Signal *signal) {
  /**
   * The other node wasn't ready to connect multi sockets to us yet.
   * We will wait for a short time and try again.
   */
  SwitchMultiTrpRef *ref = (SwitchMultiTrpRef *)&signal->theData[0];
  Uint32 node_id = ref->nodeId;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  ndbrequire(m_current_switch_multi_trp_node == node_id);
  ndbrequire(nodePtr.p->m_is_ready_to_switch_trp);
  m_current_switch_multi_trp_node = 0;
  nodePtr.p->m_is_ready_to_switch_trp = false;
  DEB_MULTI_TRP(("Recvd SWITCH_MULTI_TRP_REF from node %u", node_id));
  signal->theData[0] = ZSWITCH_MULTI_TRP;
  signal->theData[1] = node_id;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
}

void Qmgr::switch_multi_transporter(Signal *signal, NodeId node_id) {
  ndbrequire(m_current_switch_multi_trp_node == node_id);
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  g_eventLogger->info("Switch to %u multi trp for node %u",
                      nodePtr.p->m_used_num_multi_trps, node_id);
  nodePtr.p->m_is_preparing_switch_trp = false;
  nodePtr.p->m_is_ready_to_switch_trp = false;
  nodePtr.p->m_is_multi_trp_setup = false;
  /**
   * We have now reached the point where it is time to switch the transporter
   * from using the old transporters, currently in the active transporter set.
   *
   * The switch must be made such that we don't risk changing signal order
   * for signals sent from one thread to another thread in another node.
   *
   * To accomplish this we will ensure that all block threads are blocked
   * in THRMAN. THRMAN exists in each block thread. So a signal to THRMAN
   * in each THRMAN can be used to quickly synchronize all threads in the
   * node and keep them waiting in THRMAN. When all threads have stopped we
   * will also call lockMultiTransporters to avoid the connect threads from
   * interfering in the middle of this change and finally we will lock
   * the send mutex on the node we are changing to ensure that also the
   * send threads avoid interference with this process.
   *
   * At this point also each thread will have flushed the send buffers to
   * ensure that we can ensure that the last signal sent in the node
   * connection is a ACTIVATE_TRP_REQ signal. When the receiver gets this
   * signal he can activate the receiving from the new transporters since
   * we have ensured that no more signals will be received on the old
   * transporters.
   *
   * When all this things have been prepared and the ACTIVATE_TRP_REQ signal
   * is sent, now is the time to switch the active transporters and also
   * to change the MultiTransporter to use the new hash algorithm, this
   * is automatic by changing the number of transporters.
   *
   * We close the original socket when ACTIVATE_TRP_CONF is received from
   * the other side indicating that we are now in communication with the
   * other side over the new transporters.
   */
  FreezeThreadReq *req = CAST_PTR(FreezeThreadReq, signal->getDataPtrSend());
  req->nodeId = node_id;
  req->senderRef = reference();
  sendSignal(THRMAN_REF, GSN_FREEZE_THREAD_REQ, signal,
             FreezeThreadReq::SignalLength, JBA);
  return;
}

void Qmgr::execFREEZE_ACTION_REQ(Signal *signal) {
  jamEntry();
  FreezeActionReq *req = (FreezeActionReq *)&signal->theData[0];
  Uint32 node_id = req->nodeId;
  BlockReference ret_ref = req->senderRef;
  CRASH_INSERTION(956);
  if (ERROR_INSERTED(981)) {
    NdbSleep_MilliSleep(1500);
  }
  /**
   * All threads except our thread is now frozen.
   *
   * Before we send the final signal on the current transporter we switch to
   * having the multi socket transporters as neighbours. By so doing we ensure
   * that the current transporter is inserted into the non-neighbour list when
   * sending the signal. If we would change after the sending we would miss
   * sending this signal since we change to the new neighbour setup after
   * sending, but before we perform the actual send.
   *
   * It is a bit tricky to change the neighbour transporters. We check the
   * neighbour in sendSignal and expect that in do_send that the same
   * neighbour handling is performed. We handle this here by first changing
   * the neighbour setting and next sending the signal. This ensures that
   * the transporter will be handled by non-neighbour handling.
   *
   * We will lock the send to
   * the current transporter to ensure that the transporter will notice when
   * the last signal have been sent. Next we will send the last signal
   * on the the currently active socket. When this signal is sent we will flush
   * the send buffers to ensure that the transporter knows when the last data
   * have been sent. We will then flag to the transporter that it should
   * shutdown the socket for writes. When both sides have performed this
   * action the socket will be closed.
   *
   * These actions will ensure that ACTIVATE_TRP_REQ is the last data
   * received on the current transporter and ensure that from now on
   * all sends are directed to the new set of transporters.
   * To ensure that no other thread is changing the multi transporter
   * setup we will lock the multi transporter mutex while performing
   * these actions. The only other thread that can be active here is
   * the send threads since we blocked all other threads at this point.
   *
   * Next we will release all mutexes and send FREEZE_ACTION_CONF to
   * THRMAN to ensure that things get started again. We will receive
   * FREEZE_THREAD_CONF back from THRMAN when all threads are in action
   * again.
   */
  DEB_MULTI_TRP(("Block threads frozen for node %u", node_id));

  globalTransporterRegistry.lockMultiTransporters();
  Multi_Transporter *multi_trp =
      globalTransporterRegistry.get_node_multi_transporter(node_id);
  if (is_multi_socket_setup_active(node_id, true)) {
    jam();

    Transporter *current_trp = multi_trp->get_active_transporter(0);
    current_trp->lock_send_transporter();

    Uint32 num_inactive_transporters =
        multi_trp->get_num_inactive_transporters();
    for (Uint32 i = 0; i < num_inactive_transporters; i++) {
      jam();
      Transporter *tmp_trp = multi_trp->get_inactive_transporter(i);
      tmp_trp->lock_send_transporter();
    }

    ActivateTrpReq *act_trp_req =
        CAST_PTR(ActivateTrpReq, signal->getDataPtrSend());
    act_trp_req->nodeId = getOwnNodeId();
    act_trp_req->numTrps = num_inactive_transporters;
    act_trp_req->senderRef = reference();
    sendSignal(calcQmgrBlockRef(node_id), GSN_ACTIVATE_TRP_REQ, signal,
               ActivateTrpReq::SignalLength, JBB);

    flush_send_buffers();
    /* Either perform send or insert_trp below TODO */
    current_trp->unlock_send_transporter();

    if (ERROR_INSERTED(982)) {
      NdbSleep_MilliSleep(2500);
    }
    multi_trp->switch_active_trp();

    Uint32 num_active_transporters = multi_trp->get_num_active_transporters();
    for (Uint32 i = 0; i < num_active_transporters; i++) {
      jam();
      Transporter *tmp_trp = multi_trp->get_active_transporter(i);
      tmp_trp->unlock_send_transporter();
    }
    globalTransporterRegistry.unlockMultiTransporters();

    if (ERROR_INSERTED(983)) {
      NdbSleep_MilliSleep(2500);
    }
    DEB_MULTI_TRP(("Change neighbour node setup for node %u", node_id));
    startChangeNeighbourNode();
    setNeighbourNode(node_id);
    endChangeNeighbourNode();

    if (ERROR_INSERTED(984)) {
      NdbSleep_MilliSleep(2500);
    }
    DEB_MULTI_TRP(
        ("Now communication is active with node %u using multi trp"
         ", using %u transporters",
         node_id, num_active_transporters));
  } else {
    jam();
    DEB_MULTI_TRP(("Node %u failed when freezing threads", node_id));
    globalTransporterRegistry.unlockMultiTransporters();
  }
  FreezeActionConf *conf = CAST_PTR(FreezeActionConf, signal->getDataPtrSend());
  conf->nodeId = node_id;
  sendSignal(ret_ref, GSN_FREEZE_ACTION_CONF, signal,
             FreezeActionConf::SignalLength, JBA);
}

bool Qmgr::is_multi_socket_setup_active(Uint32 node_id, bool locked) {
  bool ret_val = false;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  if (!locked) {
    globalTransporterRegistry.lockMultiTransporters();
  }
  if (c_connectedNodes.get(node_id) && nodePtr.p->phase == ZRUNNING) {
    jam();
    DEB_MULTI_TRP(("Multi socket setup for node %u is active", node_id));
    ret_val = true;
  }
  if (!locked) {
    globalTransporterRegistry.unlockMultiTransporters();
  }
  return ret_val;
}

void Qmgr::execFREEZE_THREAD_CONF(Signal *signal) {
  FreezeThreadConf *conf = (FreezeThreadConf *)&signal->theData[0];
  Uint32 node_id = conf->nodeId;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  CRASH_INSERTION(957);
  if (is_multi_socket_setup_active(node_id, false)) {
    jam();
    nodePtr.p->m_is_freeze_thread_completed = true;
    DEB_MULTI_TRP(("Freeze block threads for node %u completed", node_id));
    if (ERROR_INSERTED(985)) {
      NdbSleep_MilliSleep(1500);
    }
    check_switch_completed(signal, node_id);
  } else {
    jam();
    DEB_MULTI_TRP(("2:Node %u failed when freezing threads", node_id));
  }
}

void Qmgr::execACTIVATE_TRP_REQ(Signal *signal) {
  /**
   * Receiving this signal implies that node sending it is still
   * seen as being up and running.
   */
  jamEntry();
  CRASH_INSERTION(958);
  ActivateTrpReq *req = (ActivateTrpReq *)&signal->theData[0];
  Uint32 node_id = req->nodeId;
  Uint32 num_trps = req->numTrps;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->m_multi_trp_blockref = req->senderRef;
  nodePtr.p->m_num_activated_trps = num_trps;
  ndbrequire(num_trps == nodePtr.p->m_used_num_multi_trps);

  if (ERROR_INSERTED(977)) {
    NdbSleep_MilliSleep(1500);
  }
  SyncThreadViaReqConf *syncReq =
      (SyncThreadViaReqConf *)signal->getDataPtrSend();
  syncReq->senderRef = reference();
  syncReq->senderData = node_id;
  syncReq->actionType = SyncThreadViaReqConf::FOR_ACTIVATE_TRP_REQ;
  sendSignal(TRPMAN_REF, GSN_SYNC_THREAD_VIA_REQ, signal,
             SyncThreadViaReqConf::SignalLength, JBA);
}

void Qmgr::handle_activate_trp_req(Signal *signal, Uint32 node_id) {
  jam();
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  Uint32 num_trps = nodePtr.p->m_num_activated_trps;
  CRASH_INSERTION(959);
  nodePtr.p->m_num_activated_trps = 0;
  DEB_MULTI_TRP(("Activate receive in multi trp for node %u, from ref: %x",
                 node_id, nodePtr.p->m_multi_trp_blockref));
  globalTransporterRegistry.lockMultiTransporters();
  Multi_Transporter *multi_trp =
      globalTransporterRegistry.get_node_multi_transporter(node_id);
  if (is_multi_socket_setup_active(node_id, true)) {
    jam();
    Transporter *t;
    for (Uint32 i = 0; i < num_trps; i++) {
      if (multi_trp->get_num_inactive_transporters() == num_trps) {
        jam();
        t = multi_trp->get_inactive_transporter(i);
      } else {
        jam();
        t = multi_trp->get_active_transporter(i);
        ndbrequire(multi_trp->get_num_active_transporters());
      }
      Uint32 trp_id = t->getTransporterIndex();
      ActivateTrpReq *act_trp_req =
          CAST_PTR(ActivateTrpReq, signal->getDataPtrSend());
      act_trp_req->nodeId = node_id;
      act_trp_req->trpId = trp_id;
      act_trp_req->numTrps = num_trps;
      act_trp_req->senderRef = reference();
      sendSignal(TRPMAN_REF, GSN_ACTIVATE_TRP_REQ, signal,
                 ActivateTrpReq::SignalLength, JBB);
      if (ERROR_INSERTED(986)) {
        NdbSleep_MilliSleep(500);
      }
    }
  }
  globalTransporterRegistry.unlockMultiTransporters();
}

void Qmgr::execACTIVATE_TRP_CONF(Signal *signal) {
  jamEntry();
  ActivateTrpConf *conf = (ActivateTrpConf *)&signal->theData[0];
  Uint32 node_id = conf->nodeId;
  BlockReference sender_ref = conf->senderRef;
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  DEB_MULTI_TRP(
      ("ACTIVATE_TRP_CONF(QMGR) own node %u about node %u"
       ", ref: %x",
       getOwnNodeId(), node_id, sender_ref));
  if (refToNode(sender_ref) == getOwnNodeId()) {
    if (is_multi_socket_setup_active(node_id, false)) {
      jam();
      CRASH_INSERTION(960);
      nodePtr.p->m_num_activated_trps++;
      if (nodePtr.p->m_num_activated_trps < nodePtr.p->m_used_num_multi_trps) {
        jam();
        return;
      }
      DEB_MULTI_TRP(
          ("Complete activation recv for multi trp node %u,"
           " own node: %u",
           node_id, getOwnNodeId()));
      ndbrequire(nodePtr.p->m_num_activated_trps ==
                 nodePtr.p->m_used_num_multi_trps);
      ActivateTrpConf *conf =
          CAST_PTR(ActivateTrpConf, signal->getDataPtrSend());
      conf->nodeId = getOwnNodeId();
      conf->senderRef = reference();
      BlockReference ref = nodePtr.p->m_multi_trp_blockref;
      nodePtr.p->m_multi_trp_blockref = 0;
      ndbrequire(refToNode(ref) == node_id);
      ndbrequire(refToMain(ref) == QMGR);
      sendSignal(ref, GSN_ACTIVATE_TRP_CONF, signal,
                 ActivateTrpConf::SignalLength, JBB);
      nodePtr.p->m_is_activate_trp_ready_for_me = true;
      if (ERROR_INSERTED(975)) {
        NdbSleep_MilliSleep(1500);
      }
      check_switch_completed(signal, node_id);
    } else {
      jam();
      DEB_MULTI_TRP(("Node %u failed in multi trp activation", node_id));
    }
  } else {
    jam();
    CRASH_INSERTION(952);
    DEB_MULTI_TRP(("Completed activation recv for multi trp node %u", node_id));
    ndbrequire(is_multi_socket_setup_active(node_id, false));
    nodePtr.p->m_is_activate_trp_ready_for_other = true;
    check_switch_completed(signal, node_id);
  }
}

void Qmgr::check_switch_completed(Signal *signal, NodeId node_id) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  if (!(nodePtr.p->m_is_activate_trp_ready_for_other &&
        nodePtr.p->m_is_activate_trp_ready_for_me &&
        nodePtr.p->m_is_freeze_thread_completed)) {
    jam();
    DEB_MULTI_TRP(("Still waiting for node %u switch to complete", node_id));
    return;
  }

  /**
   * When switch has completed the now 'inactive_transporter' will not be
   * needed any more and is disconnected.
   */
  globalTransporterRegistry.lockMultiTransporters();
  Multi_Transporter *multi_trp =
      globalTransporterRegistry.get_node_multi_transporter(node_id);
  ndbrequire(multi_trp != nullptr);
  Uint32 num_inactive_transporters = multi_trp->get_num_inactive_transporters();
  for (Uint32 i = 0; i < num_inactive_transporters; i++) {
    jam();
    Transporter *tmp_trp = multi_trp->get_inactive_transporter(i);
    TrpId trp_id = tmp_trp->getTransporterIndex();
    globalTransporterRegistry.start_disconnecting(trp_id);
  }
  globalTransporterRegistry.unlockMultiTransporters();
  /**
   * We have now completed the switch to new set of transporters, the
   * old set is inactive and will be put back if the node fails. We
   * are now ready to see if any more nodes require attention.
   */
  if (ERROR_INSERTED(976)) {
    NdbSleep_MilliSleep(1500);
  }
  m_current_switch_multi_trp_node = 0;
  nodePtr.p->m_is_using_multi_trp = true;
  nodePtr.p->m_is_ready_to_switch_trp = false;
  nodePtr.p->m_is_activate_trp_ready_for_me = false;
  nodePtr.p->m_is_activate_trp_ready_for_other = false;
  nodePtr.p->m_is_freeze_thread_completed = false;
  nodePtr.p->m_set_up_multi_trp_started = false;
  DEB_MULTI_TRP(("Completed switch to multi trp for node %u", node_id));
  CRASH_INSERTION(953);
  check_more_trp_switch_nodes(signal);
}

void Qmgr::check_more_trp_switch_nodes(Signal *signal) {
  if (!check_all_multi_trp_nodes_connected()) {
    jam();
    /* Still waiting for nodes to complete connect */
    DEB_MULTI_TRP(("Still waiting for nodes to complete connect"));
    return;
  }
  NodeId node_id = 0;
  if (select_node_id_for_switch(node_id, false)) {
    jam();
    send_switch_multi_transporter(signal, node_id, false);
    return;
  }
  if (m_initial_set_up_multi_trp_done) {
    jam();
    DEB_MULTI_TRP(("Initial setup already done"));
    return;
  }
  if (m_get_num_multi_trps_sent != 0) {
    jam();
    DEB_MULTI_TRP(("Still waiting for GET_NUM_MULTI_TRP_REQ"));
    return;
  }
  bool done = true;
  for (Uint32 node_id = 1; node_id < MAX_NDB_NODES; node_id++) {
    NodeRecPtr nodePtr;
    nodePtr.i = node_id;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    if (nodePtr.p->m_is_in_same_nodegroup && nodePtr.p->phase == ZRUNNING &&
        nodePtr.p->m_set_up_multi_trp_started) {
      if (!nodePtr.p->m_is_using_multi_trp) {
        jam();
        done = false;
      }
    }
  }
  if (done) {
    jam();
    DEB_MULTI_TRP(("Initial setup of multi trp now done"));
    m_initial_set_up_multi_trp_done = true;
    sendSignal(m_ref_set_up_multi_trp_req, GSN_SET_UP_MULTI_TRP_CONF, signal, 1,
               JBB);
  } else {
    DEB_MULTI_TRP(("Not done with setup of multi trp yet"));
    jam();
  }
}

void Qmgr::check_no_multi_trp(Signal *signal, NodeId node_id) {
  NodeRecPtr nodePtr;
  nodePtr.i = node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  if (nodePtr.p->m_is_get_num_multi_trp_active) {
    jam();
    dec_get_num_multi_trps_sent(nodePtr.i);
  }
  DEB_MULTI_TRP(("check_no_multi_trp for node %u", node_id));
  if (node_id == m_current_switch_multi_trp_node) {
    jam();
    m_current_switch_multi_trp_node = 0;
    check_more_trp_switch_nodes(signal);
  }
}

bool Qmgr::check_all_multi_trp_nodes_connected() {
  /**
   * Wait for all neighbour nodes to connect all multi transporters
   * before proceeding with the next phase where we start switching
   * to multi transporter setup.
   */
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING && nodePtr.p->m_is_in_same_nodegroup &&
        (nodePtr.p->m_is_preparing_switch_trp ||
         nodePtr.p->m_is_get_num_multi_trp_active)) {
      /* Neighbour node preparing switch */
      jam();
      jamLine(Uint16(nodePtr.i));
      if (!nodePtr.p->m_is_multi_trp_setup) {
        jam();
        /* Still waiting for connections of this node to complete */
        return false;
      }
    }
  }
  jam();
  /* All nodes to connect are done */
  return true;
}

bool Qmgr::select_node_id_for_switch(NodeId &node_id, bool check_found) {
  NodeId max_node_id = 0;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING && nodePtr.p->m_is_in_same_nodegroup &&
        nodePtr.p->m_is_preparing_switch_trp &&
        nodePtr.p->m_is_multi_trp_setup) {
      if (nodePtr.i > max_node_id) {
        jam();
        jamLine(Uint16(nodePtr.i));
        max_node_id = nodePtr.i;
      }
    }
  }
  ndbrequire((!check_found) || (max_node_id != 0));
  if (m_current_switch_multi_trp_node != 0) {
    jam();
    return false;
  }
  if (max_node_id < getOwnNodeId()) {
    jam();
    return false;
  }
  node_id = max_node_id;
  nodePtr.i = max_node_id;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  ndbrequire(!nodePtr.p->m_is_ready_to_switch_trp);
  jam();
  return true;
}
