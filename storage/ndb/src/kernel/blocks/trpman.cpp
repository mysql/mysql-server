/*
  Copyright (c) 2011, 2026, Oracle and/or its affiliates.

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

#include "trpman.hpp"
#include "EventLogger.hpp"
#include "TransporterRegistry.hpp"
#include "mt.hpp"
#include "portlib/NdbTCP.h"
#include "portlib/NdbTick.h"
#include "signaldata/CloseComReqConf.hpp"
#include "signaldata/DisconnectRep.hpp"
#include "signaldata/DumpStateOrd.hpp"
#include "signaldata/EnableCom.hpp"
#include "signaldata/RouteOrd.hpp"

#include "EventLogger.hpp"
#include "mt.hpp"

#define JAM_FILE_ID 430

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
// #define DEBUG_MULTI_TRP 1
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

Trpman::Trpman(Block_context &ctx, Uint32 instanceno)
    : SimulatedBlock(TRPMAN, ctx, instanceno) {
  BLOCK_CONSTRUCTOR(Trpman);

  addRecSignal(GSN_CLOSE_COMREQ, &Trpman::execCLOSE_COMREQ);
  addRecSignal(GSN_CLOSE_COMCONF, &Trpman::execCLOSE_COMCONF);
  addRecSignal(GSN_OPEN_COMORD, &Trpman::execOPEN_COMORD);
  addRecSignal(GSN_ENABLE_COMREQ, &Trpman::execENABLE_COMREQ);
  addRecSignal(GSN_DISCONNECT_REP, &Trpman::execDISCONNECT_REP);
  addRecSignal(GSN_CONNECT_REP, &Trpman::execCONNECT_REP);
  addRecSignal(GSN_ROUTE_ORD, &Trpman::execROUTE_ORD);
  addRecSignal(GSN_SYNC_THREAD_VIA_REQ, &Trpman::execSYNC_THREAD_VIA_REQ);
  addRecSignal(GSN_ACTIVATE_TRP_REQ, &Trpman::execACTIVATE_TRP_REQ);
  addRecSignal(GSN_UPD_QUERY_DIST_ORD, &Trpman::execUPD_QUERY_DIST_ORD);
  addRecSignal(GSN_NODE_START_REP, &Trpman::execNODE_START_REP, true);
  addRecSignal(GSN_READ_CONFIG_REQ, &Trpman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Trpman::execSTTOR);
  addRecSignal(GSN_NDB_TAMPER, &Trpman::execNDB_TAMPER, true);
  addRecSignal(GSN_DUMP_STATE_ORD, &Trpman::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Trpman::execDBINFO_SCANREQ);
  addRecSignal(GSN_TIME_SIGNAL, &Trpman::execTIME_SIGNAL);
  m_distribution_handler_inited = false;
}

BLOCK_FUNCTIONS(Trpman)

#ifdef ERROR_INSERT
static NodeBitmask c_error_9000_nodes_mask;
extern Uint32 MAX_RECEIVED_SIGNALS;
#endif

bool Trpman::handles_this_trp(TrpId trpId) {
  /* If there's only one receiver then no question */
  if (globalData.ndbMtReceiveThreads <= (Uint32)1) return true;

  /* There's a global receiver->thread index - look it up */
  return (instance() == (get_recv_thread_idx(trpId) + /* proxy */ 1));
}

/**
 * During the setup phase the node will connect with only the base
 * transporter. Get the TrpId of this transporter - Crash if
 * multiple transporters are found as that would be a breakage
 * of the transporter protocol.
 */
TrpId Trpman::get_the_only_base_trp(NodeId nodeId) const {
  return globalTransporterRegistry.get_the_only_base_trp(nodeId);
}

void Trpman::set_db_hb_sender(NodeId dbHbSender) {
  jam();
  if (dbHbSender == ZNIL || dbHbSender == 0) {
    jam();
    m_dbHbSender = 0;
    m_dbHbSenderTrp = 0;
  } else {
    jam();
    // On which transporter will we receive heartbeat from other data node
    BlockReference recvRef = numberToRef(QMGR, getOwnNodeId());
    BlockReference sendRef = numberToRef(QMGR, dbHbSender);
    TrpId dbHbSenderTrp =
        globalTransporterRegistry.get_recv_trp(recvRef, sendRef);
    m_dbHbSender = dbHbSender;
    ndbrequire(dbHbSenderTrp != 0);
    if (m_dbHbSenderTrp != dbHbSenderTrp) {
      jam();
      m_dbHbSenderTrp = dbHbSenderTrp;
      /*
       * Skip late heartbeat detection for next receive.
       * As an acceptable side effect activity histogram will skip count next
       * receive.
       */
      NdbTick_Invalidate(&m_trp_activity[m_dbHbSenderTrp].last_recv);
    }
  }
}

void Trpman::execOPEN_COMORD(Signal *signal) {
  /**
   * Connect to the specified NDB node, only QMGR allowed communication
   * so far with the node. Even if multi-transporters will be used to
   * communicate with node, we initially open only the single base transporter.
   */
  const BlockReference userRef [[maybe_unused]] = signal->theData[0];
  jamEntry();

  const Uint32 len = signal->getLength();
  if (len == 2) {
    const NodeId tStartingNode = signal->theData[1];
    ndbrequire(tStartingNode > 0 && tStartingNode < MAX_NODES);
#ifdef ERROR_INSERT
    if (!((ERROR_INSERTED(9000) || ERROR_INSERTED(9002)) &&
          c_error_9000_nodes_mask.get(tStartingNode)))
#endif
    {
      // Connection is initially opened using a non-multi_transporter
      const TrpId trpId = get_the_only_base_trp(tStartingNode);
      if (!handles_this_trp(trpId)) {
        jam();
        goto done;
      }

      globalTransporterRegistry.start_connecting(trpId);
      globalTransporterRegistry.setIOState(trpId, HaltIO);

      //-----------------------------------------------------
      // Report that the connection to the node is opened
      //-----------------------------------------------------
      signal->theData[0] = NDB_LE_CommunicationOpened;
      signal->theData[1] = tStartingNode;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      // Clear last receive left from earlier connections
      NdbTick_Invalidate(&m_trp_activity[trpId].last_recv);
      //-----------------------------------------------------
    }
  } else {
    Uint32 tData2 = signal->theData[2];
    for (unsigned int i = 1; i < MAX_NODES; i++) {
      jam();
      if (i != getOwnNodeId() && getNodeInfo(i).m_type == tData2) {
        jam();

        const TrpId trpId = get_the_only_base_trp(i);
        if (!handles_this_trp(trpId)) continue;

#ifdef ERROR_INSERT
        if ((ERROR_INSERTED(9000) || ERROR_INSERTED(9002)) &&
            c_error_9000_nodes_mask.get(i))
          continue;
#endif

        globalTransporterRegistry.start_connecting(trpId);
        globalTransporterRegistry.setIOState(trpId, HaltIO);

        signal->theData[0] = NDB_LE_CommunicationOpened;
        signal->theData[1] = i;
        sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
        // Clear last receive left from earlier connections
        NdbTick_Invalidate(&m_trp_activity[trpId].last_recv);
      }
    }
  }
done:
  /**
   * NO REPLY for now
   */
  return;
}

void Trpman::execCONNECT_REP(Signal *signal) {
  const Uint32 hostId = signal->theData[0];
  jamEntry();

  const NodeInfo::NodeType type =
      (NodeInfo::NodeType)getNodeInfo(hostId).m_type;
  ndbrequire(type != NodeInfo::INVALID);

  /**
   * Inform QMGR that client has connected
   */
  signal->theData[0] = hostId;
  if (ERROR_INSERTED(9005)) {
    sendSignalWithDelay(QMGR_REF, GSN_CONNECT_REP, signal, 50, 1);
  } else {
    sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
  }

  /* Automatically subscribe events for MGM nodes.
   */
  if (type == NodeInfo::MGM) {
    jam();
    const TrpId trpId = globalTransporterRegistry.get_the_only_base_trp(hostId);
    if (trpId != 0) {
      globalTransporterRegistry.setIOState(trpId, NoHalt);
    }
  }

  //------------------------------------------
  // Also report this event to the Event handler
  //------------------------------------------
  signal->theData[0] = NDB_LE_Connected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}

void Trpman::close_com_failed_node(Signal *signal, NodeId nodeId) {
  Uint32 num_ids;
  TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
  globalTransporterRegistry.lockMultiTransporters();
  globalTransporterRegistry.get_trps_for_node(nodeId, &trp_ids[0], num_ids,
                                              MAX_NODE_GROUP_TRANSPORTERS);

  // This TRPMAN should only close the transporters handled by its own
  // recv-thread
  for (unsigned i = 0; i < num_ids; i++) {
    const TrpId trpId = trp_ids[i];
    if (!handles_this_trp(trpId)) continue;
    globalTransporterRegistry.setIOState(trpId, HaltIO);
    globalTransporterRegistry.start_disconnecting(trpId);
  }
  globalTransporterRegistry.unlockMultiTransporters();

  // Only the TRPMAN responsible for the first transporter
  // sends the EVENT_REP for the *NodeId* disconnect.
  if (num_ids > 0 && handles_this_trp(trp_ids[0])) {
    //-----------------------------------------------------
    // Report that the connection to the node is closed
    //-----------------------------------------------------
    signal->theData[0] = NDB_LE_CommunicationClosed;
    signal->theData[1] = nodeId;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  }
}

void Trpman::execCLOSE_COMREQ(Signal *signal) {
  // Close communication with the node and halt input/output from
  // other blocks than QMGR
  jamEntry();
  ndbrequire(signal->getLength() >= CloseComReqConf::SignalLengthDB);

  CloseComReqConf *const closeCom = (CloseComReqConf *)&signal->theData[0];

  const BlockReference userRef = closeCom->xxxBlockRef;
  Uint32 requestType = closeCom->requestType;
  Uint32 failNo = closeCom->failNo;
  Uint32 noOfNodes = closeCom->noOfNodes;
  Uint32 dbHbSender = closeCom->m_dbHbSender;
  Uint32 found_nodes = 0;

  if (closeCom->failedNodeId == 0) {
    jam();
    /**
     * When data nodes have failed, we can have several
     * concurrent failures, these are handled all in one signal, in
     * this case we send the node bitmask in a section.
     */
    ndbrequire(signal->getNoOfSections() == 1);
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    NdbNodeBitmask nodes;
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(nodes.rep.data, ptr);
    releaseSections(handle);

    unsigned node_id = 0;
    while ((node_id = nodes.find(node_id + 1)) != NdbNodeBitmask::NotFound) {
      jam();
      found_nodes++;
      jamLine(node_id);
      close_com_failed_node(signal, node_id);
    }
  } else {
    jam();
    ndbrequire(signal->getNoOfSections() == 0);
    found_nodes = 1;
    ndbrequire(noOfNodes == 1);
    jamLine(Uint16(closeCom->failedNodeId));
    close_com_failed_node(signal, closeCom->failedNodeId);
  }
  ndbrequire(noOfNodes == found_nodes);

  set_db_hb_sender(dbHbSender);

  if (requestType != CloseComReqConf::RT_NO_REPLY) {
    ndbassert(
        (requestType == CloseComReqConf::RT_API_FAILURE) ||
        ((requestType == CloseComReqConf::RT_NODE_FAILURE) && (failNo != 0)));
    jam();
    CloseComReqConf *closeComConf = (CloseComReqConf *)signal->getDataPtrSend();
    closeComConf->xxxBlockRef = userRef;
    closeComConf->requestType = requestType;
    closeComConf->failNo = failNo;
    closeComConf->m_dbHbSender = 0;  // ignored

    /* Note assumption that noOfNodes and theNodes
     * bitmap is not trampled above
     * signals received from the remote node.
     */
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMCONF, signal,
               CloseComReqConf::SignalLengthDB, JBA);
  }
}

/*
  We need to implement CLOSE_COMCONF signal for the non-multithreaded
  case where message should go to QMGR, for multithreaded case it
  needs to pass through TRPMAN proxy on its way back.
*/
void Trpman::execCLOSE_COMCONF(Signal *signal) {
  jamEntry();
  ndbrequire(signal->getLength() >= CloseComReqConf::SignalLengthDB);
  sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal,
             CloseComReqConf::SignalLengthDB, JBA);
}

void Trpman::enable_com_node(Signal *signal, NodeId nodeId) {
  const TrpId trpId = get_the_only_base_trp(nodeId);
  if (!handles_this_trp(trpId)) return;

  globalTransporterRegistry.setIOState(trpId, NoHalt);
  setNodeInfo(nodeId).m_connected = true;

  //-----------------------------------------------------
  // Report that the version of the node
  //-----------------------------------------------------
  signal->theData[0] = NDB_LE_ConnectedApiVersion;
  signal->theData[1] = nodeId;
  signal->theData[2] = getNodeInfo(nodeId).m_version;
  signal->theData[3] = getNodeInfo(nodeId).m_mysql_version;

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
}

void Trpman::execENABLE_COMREQ(Signal *signal) {
  jamEntry();
  const EnableComReq *enableComReq = (const EnableComReq *)signal->getDataPtr();

  if (ERROR_INSERTED(9500) && signal->getSendersBlockRef() != reference()) {
    jam();
    g_eventLogger->info("TRPMAN %u delaying ENABLE_COMREQ %u for 5s",
                        instance(), enableComReq->m_enableNodeId);
    sendSignalWithDelay(reference(), GSN_ENABLE_COMREQ, signal, 5000,
                        signal->getLength());
    return;
  }

  /* Need to copy out signal data to not clobber it with sendSignal(). */
  BlockReference senderRef = enableComReq->m_senderRef;
  Uint32 senderData = enableComReq->m_senderData;
  Uint32 enableNodeId = enableComReq->m_enableNodeId;
  Uint32 dbHbSender = enableComReq->m_dbHbSender;

  /* Enable communication with all our NDB blocks to these nodes. */
  if (enableNodeId == 0) {
    ndbrequire(signal->getNoOfSections() == 1);
    Uint32 nodes[NodeBitmask::Size];
    memset(nodes, 0, sizeof(nodes));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NodeBitmask::Size);
    copy(nodes, ptr);
    releaseSections(handle);
    Uint32 search_from = 1;
    for (;;) {
      Uint32 tStartingNode = NodeBitmask::find(nodes, search_from);
      if (tStartingNode == NodeBitmask::NotFound) break;
      search_from = tStartingNode + 1;
      enable_com_node(signal, tStartingNode);
    }
  } else {
    enable_com_node(signal, enableNodeId);
  }

  set_db_hb_sender(dbHbSender);

  EnableComConf *enableComConf = (EnableComConf *)signal->getDataPtrSend();
  enableComConf->m_senderRef = reference();
  enableComConf->m_senderData = senderData;
  enableComConf->m_enableNodeId = enableNodeId;
  enableComConf->m_dbHbSender = 0;  // ignored
  sendSignal(senderRef, GSN_ENABLE_COMCONF, signal, EnableComConf::SignalLength,
             JBA);
}

void Trpman::execDISCONNECT_REP(Signal *signal) {
  const DisconnectRep *const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 hostId = rep->nodeId;
  jamEntry();

  setNodeInfo(hostId).m_connected = false;
  setNodeInfo(hostId).m_connectCount++;
  const NodeInfo::NodeType type = getNodeInfo(hostId).getType();
  ndbrequire(type != NodeInfo::INVALID);

  sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal, DisconnectRep::SignalLength,
             JBA);

  signal->theData[0] = hostId;
  sendSignal(CMVMI_REF, GSN_CANCEL_SUBSCRIPTION_REQ, signal, 1, JBB);

  signal->theData[0] = NDB_LE_Disconnected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}

/**
 * execROUTE_ORD
 * Allows other blocks to route signals as if they
 * came from TRPMAN
 * Useful in ndbmtd for synchronising signals w.r.t
 * external signals received from other nodes which
 * arrive from the same thread that runs TRPMAN
 */
void Trpman::execROUTE_ORD(Signal *signal) {
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  SectionHandle handle(this, signal);

  RouteOrd *ord = (RouteOrd *)signal->getDataPtr();
  Uint32 dstRef = ord->dstRef;
  Uint32 srcRef = ord->srcRef;
  Uint32 gsn = ord->gsn;
  /* ord->cnt ignored */

  Uint32 nodeId = refToNode(dstRef);

  if (likely((nodeId == 0) || getNodeInfo(nodeId).m_connected)) {
    jam();
    Uint32 secCount = handle.m_cnt;
    ndbrequire(secCount >= 1 && secCount <= 3);

    jamLine(secCount);

    /**
     * Put section 0 in signal->theData
     */
    Uint32 sigLen = handle.m_ptr[0].sz;
    ndbrequire(sigLen <= 25);
    copy(signal->theData, handle.m_ptr[0]);

    SegmentedSectionPtr save = handle.m_ptr[0];
    for (Uint32 i = 0; i < secCount - 1; i++)
      handle.m_ptr[i] = handle.m_ptr[i + 1];
    handle.m_cnt--;

    sendSignal(dstRef, gsn, signal, sigLen, JBB, &handle);

    handle.m_cnt = 1;
    handle.m_ptr[0] = save;
    releaseSections(handle);
    return;
  }

  releaseSections(handle);
  warningEvent("Unable to route GSN: %d from %x to %x", gsn, srcRef, dstRef);
}

void Trpman::execDBINFO_SCANREQ(Signal *signal) {
  DbinfoScanReq req = *(DbinfoScanReq *)signal->theData;
  const Ndbinfo::ScanCursor *cursor =
      CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;
  char addr_buf[NDB_ADDR_STRLEN];

  jamEntry();

  switch (req.tableId) {
    case Ndbinfo::TRANSPORTER_DETAILS_TABLEID: {
      jam();
      TrpId trpId = cursor->data[0];

      NDB_TICKS now = NdbTick_getCurrentTicks();
      while (trpId <= globalTransporterRegistry.get_transporter_count()) {
        if (globalTransporterRegistry.get_transporter(trpId) == nullptr ||
            globalTransporterRegistry.is_inactive_trp(trpId) ||
            !handles_this_trp(trpId)) {
          trpId++;
          continue;
        }

        const NodeId nodeId =
            globalTransporterRegistry.get_transporter_node_id(trpId);
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());  // Node id
        row.write_uint32(instance());      // Block instance reporting
        row.write_uint32(trpId);           // Transporter id
        row.write_uint32(nodeId);          // Remote node id

        row.write_uint32(globalTransporterRegistry.getPerformState(trpId));

        const ndb_sockaddr conn_addr =
            globalTransporterRegistry.get_connect_address(trpId);
        /* Connect address */
        if (!conn_addr.is_unspecified()) {
          jam();
          char *addr_str =
              Ndb_inet_ntop(&conn_addr, addr_buf, sizeof(addr_buf));
          row.write_string(addr_str);
        } else {
          jam();
          row.write_null();
        }

        /* Bytes sent/received */
        row.write_uint64(globalTransporterRegistry.get_bytes_sent(trpId));
        row.write_uint64(globalTransporterRegistry.get_bytes_received(trpId));

        /* Connect count, overload and slowdown states */
        row.write_uint32(globalTransporterRegistry.get_connect_count(trpId));
        /* FIXME: overload & slowdown is still pr NodeId */
        row.write_uint32(
            globalTransporterRegistry.get_status_overloaded().get(nodeId));
        row.write_uint32(globalTransporterRegistry.get_overload_count(nodeId));
        row.write_uint32(
            globalTransporterRegistry.get_status_slowdown().get(nodeId));
        row.write_uint32(globalTransporterRegistry.get_slowdown_count(nodeId));

        /* TLS */
        row.write_uint32(globalTransporterRegistry.is_encrypted_link(trpId));

        /* Send buffer bytes/pages usage */
        row.write_uint64(
            globalTransporterRegistry.get_send_buffer_used_bytes(trpId));
        row.write_uint64(
            globalTransporterRegistry.get_send_buffer_max_used_bytes(trpId));
        row.write_uint64(
            globalTransporterRegistry.get_send_buffer_alloc_bytes(trpId));
        row.write_uint64(
            globalTransporterRegistry.get_send_buffer_max_alloc_bytes(trpId));

        /* Transporter type */
        row.write_uint32(globalTransporterRegistry.get_transporter_type(trpId));

        /* Heartbeat interval (ms) */
        const NodeInfo::NodeType type = getNodeInfo(nodeId).getType();
        bool is_db = (type == NodeInfo::DB);
        if (!is_db || trpId == m_dbHbSenderTrp) {
          Uint32 heartbeat_interval = is_db ? m_hbDbDb : m_hbDbApi;
          row.write_uint32(heartbeat_interval);
        } else {
          row.write_null();  // heartbeat_interval
        }

        /* Last receive (us) */
        NDB_TICKS last_recv = globalTransporterRegistry.get_last_recv(trpId);
        if (NdbTick_IsValid(last_recv))
          row.write_uint64(NdbTick_Elapsed(last_recv, now).microSec());
        else
          row.write_null();  // last_recv

        ndbinfo_send_row(signal, req, row, rl);
        trpId++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, trpId);
          return;
        }
      }
      break;
    }
    case Ndbinfo::TRANSPORTERS_TABLEID: {
      jam();
      Uint32 rnode = cursor->data[0];
      if (rnode == 0) rnode++;  // Skip node 0

      while (rnode < MAX_NODES) {
        if (globalTransporterRegistry.get_node_transporter(rnode) == nullptr) {
          rnode++;
          continue;
        }
        // Find all active transporters for each node
        Uint32 num_ids;
        TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
        globalTransporterRegistry.lockMultiTransporters();
        globalTransporterRegistry.get_trps_for_node(
            rnode, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);
        globalTransporterRegistry.unlockMultiTransporters();

        /**
         * The TRPMAN having the first transporter is responsible for reporting
         * the ndbinfo. It might dirty-read the ndbinfo added from other
         * (multi-)transporters.
         */
        if (num_ids == 0 || !handles_this_trp(trp_ids[0])) {
          rnode++;
          continue;
        }
        switch (getNodeInfo(rnode).m_type) {
          default: {
            jam();
            Uint64 bytes_sent = 0;
            Uint64 bytes_received = 0;
            Uint32 connect_count = 0;
            Uint32 perform_state = 0;
            Uint32 is_encrypted = 0;
            ndb_sockaddr conn_addr;

            // Aggregate information over all (multi?) transporters
            for (unsigned i = 0; i < num_ids; i++) {
              const TrpId trpId = trp_ids[i];
              bytes_sent += globalTransporterRegistry.get_bytes_sent(trpId);
              bytes_received +=
                  globalTransporterRegistry.get_bytes_received(trpId);
              connect_count =
                  std::max(connect_count,
                           globalTransporterRegistry.get_connect_count(trpId));

              // Not aggregated, will be the same for all trps;
              conn_addr = globalTransporterRegistry.get_connect_address(trpId);
              perform_state = globalTransporterRegistry.getPerformState(trpId);
              is_encrypted = globalTransporterRegistry.is_encrypted_link(trpId);
            }

            Ndbinfo::Row row(signal, req);
            row.write_uint32(getOwnNodeId());  // Node id
            row.write_uint32(rnode);           // Remote node id
            row.write_uint32(perform_state);

            /* Connect address */
            if (!conn_addr.is_unspecified()) {
              jam();
              char *addr_str =
                  Ndb_inet_ntop(&conn_addr, addr_buf, sizeof(addr_buf));
              row.write_string(addr_str);
            } else {
              jam();
              row.write_string("-");
            }

            /* Bytes sent/received */
            row.write_uint64(bytes_sent);
            row.write_uint64(bytes_received);

            /* Connect count, overload and slowdown states */
            row.write_uint32(connect_count);
            row.write_uint32(
                globalTransporterRegistry.get_status_overloaded().get(rnode));
            row.write_uint32(
                globalTransporterRegistry.get_overload_count(rnode));
            row.write_uint32(
                globalTransporterRegistry.get_status_slowdown().get(rnode));
            row.write_uint32(
                globalTransporterRegistry.get_slowdown_count(rnode));

            /* TLS */
            row.write_uint32(is_encrypted);

            ndbinfo_send_row(signal, req, row, rl);
            break;
          }

          case NodeInfo::INVALID:
            jam();
            break;
        }

        rnode++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, rnode);
          return;
        }
      }
      break;
    }
    case Ndbinfo::CERTIFICATES_TABLEID: {
      TlsKeyManager *keyMgr = globalTransporterRegistry.getTlsKeyManager();
      int peer_node_id = cursor->data[0];
      cert_table_entry entry;
      while (keyMgr->iterate_cert_table(peer_node_id, &entry)) {
        jam();
        Ndbinfo::Row row(signal, req);

        row.write_uint32(getOwnNodeId());
        row.write_uint32(peer_node_id);
        row.write_string(entry.name);
        row.write_string(entry.serial);
        row.write_uint32(entry.expires);

        ndbinfo_send_row(signal, req, row, rl);

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, peer_node_id);
          return;
        }
      }
      break;
    }
    case Ndbinfo::TRANSPORTER_ACTIVITY_TABLEID: {
      jam();
      Uint32 restore = cursor->data[0];
      TrpId trpId = restore & 0xFFFF;
      Uint32 bin_index = restore >> 16;

      while (trpId <= globalTransporterRegistry.get_transporter_count()) {
        if (bin_index >= TRP_ACTIVITY_HIST_BIN_COUNT ||
            globalTransporterRegistry.get_transporter(trpId) == nullptr ||
            globalTransporterRegistry.is_inactive_trp(trpId) ||
            !handles_this_trp(trpId)) {
          trpId++;
          bin_index = 0;
          continue;
        }
        const NodeId nodeId =
            globalTransporterRegistry.get_transporter_node_id(trpId);
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());  // node_id
        row.write_uint32(instance());      // block_instance
        row.write_uint32(trpId);           // trp_id
        row.write_uint32(nodeId);          // remote_node_id
        row.write_uint32(globalTransporterRegistry.get_connect_count(
            trpId));  // connect_count

        const NodeInfo::NodeType type = getNodeInfo(nodeId).getType();
        bool is_db = (type == NodeInfo::DB);

        if (!is_db || trpId == m_dbHbSenderTrp) {
          Uint32 heartbeat_interval = is_db ? m_hbDbDb : m_hbDbApi;
          row.write_uint32(heartbeat_interval);
        } else {
          row.write_null();  // heartbeat_interval
        }

        Uint64 upper_bound = is_db ? m_hbDbDb_bin_bounds[bin_index]
                                   : m_hbDbApi_bin_bounds[bin_index];
        if (upper_bound < UINT32_MAX)
          row.write_uint64(upper_bound);
        else
          row.write_null();  // upper_bound
        Uint64 activity = m_trp_activity[trpId].hist_bins[bin_index];
        row.write_uint64(activity);

        ndbinfo_send_row(signal, req, row, rl);
        bin_index++;
        if (bin_index == (is_db ? m_hbDbDb_bin_count : m_hbDbApi_bin_count)) {
          trpId++;
          bin_index = 0;
        }
        if (rl.need_break(req)) {
          jam();
          Uint32 save = (bin_index << 16) | trpId;
          ndbinfo_send_scan_break(signal, req, rl, save);
          return;
        }
      }
      break;
    }
    default:
      break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

void Trpman::execNODE_START_REP(Signal *signal) {
  jamEntry();
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(9002) && signal->theData[0] == getOwnNodeId()) {
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = 9001;
    execDUMP_STATE_ORD(signal);
  }
#endif
}

unsigned Trpman::calculate_histogram_bin_limits(
    unsigned hb_interval, std::span<unsigned> bin_limits) {
  const unsigned sample_interval = TRP_TIME_SIGNAL_DELAY;
  const unsigned max_bin_count = TRP_ACTIVITY_HIST_BIN_COUNT;
  const unsigned min_bin_width = 2 * sample_interval;
  const unsigned half_hb_interval_dn = hb_interval / 2;
  const unsigned half_hb_interval_up = (hb_interval + 1) / 2;

  assert(bin_limits.size() >= max_bin_count);

  /*
   * There are four parts of bins of the histogram. The first part uses bins
   * with minimal width. The second increases the bin width exponentially. And
   * the third part will use same size for remaining bins to have histogram
   * cover 5 heartbeat intervals. Finally there is the last bin with infinite
   * bound.
   *
   * In part 3 there can be 9 bins for 1 HBinterval to 5 HB interval with half
   * HB interval steps. Part four always have one infinite bin.
   * The rest can be used by part 1 and 2.
   */
  constexpr unsigned max_bin_count_part3_4 = 9 + 1;
  constexpr unsigned max_bin_count_part1_2 =
      max_bin_count - max_bin_count_part3_4;
  /*
   * There can be up to 11 bins covering the heartbeat interval. Up to 10 in
   * part 1 and 2 the bin covering last part of heartbeat interval belongs to
   * part 3.
   */
  const unsigned count =
      std::min(hb_interval / min_bin_width, max_bin_count_part1_2 + 1);
  unsigned bin_index = 0;

  /*
   * Part 1 with minimal sized bins.
   *
   * Use minimal sized bins until a suitable scaling factor is found for
   * exponential part or that bin for heartbeat interval is next.
   */
  double factor;                   // calculated during part 1, used in part 2
  unsigned first_part2_limit = 0;  // 0 indicates no value
  unsigned first_part3_limit = 0;
  unsigned bin_limit = min_bin_width;
  while (bin_index + 1 < count) {
    bin_limits[bin_index] = bin_limit;
    bin_index++;
    if (bin_index == count - 1) {
      bin_limit += min_bin_width;
      break;
    }

    const double ratio = 1.0 * hb_interval / bin_limit;
    factor = std::pow(ratio, 1.0 / (count - bin_index));
    /*
     * If factor > 2.0 that would cause width of last bin before heartbeat
     * interval bin to be wider than half heartbeat interval, that we do not
     * allow since then it would be wider than bins in part 3. To come around
     * that we use half heartbeat interval as limit of that bin and
     * recalculate factor against that. In this case it is ok with a factor
     * > 2.0 since bins below half heartbeat interval can not that wide.
     */
    bool bad_factor = false;
    if (factor <= 2.0) {
    } else if (bin_index < count - 1) {
      const double ratio = 1.0 * half_hb_interval_dn / bin_limit;
      factor = std::pow(ratio, 1.0 / (count - bin_index - 1));
    } else
      bad_factor = true;

    if (!bad_factor) {
      const unsigned part2_limit = std::round(bin_limit * factor);
      if (part2_limit - bin_limit >= min_bin_width) {
        first_part2_limit = bin_limit = part2_limit;
        if (factor > 2.0)
          first_part3_limit = half_hb_interval_dn;
        else
          first_part3_limit = hb_interval;
        break;
      }
    }
    bin_limit += min_bin_width;
    if (bin_limit >= hb_interval) {
      break;
    }
  }

  /*
   * Part 2 with exponentially increasing bin widths.
   *
   * Widths of each bin will be scaled from previous bin width with a factor.
   * If factor is 2 or greater last width will be less than half the heartbeat
   * interval else less than the heartbeat interval. The special handling of
   * big scaling factor is to ensure the last bin width in exponential part
   * will be less than half heartbeat interval since that will be used as bin
   * width in next part and bin widths may not decrease.
   */
  if (first_part2_limit > 0) {  // std::isfinite(factor)) {
    assert(first_part2_limit == bin_limit);
    while (bin_limit < first_part3_limit &&
           (bin_limit - bin_limits[bin_index - 1]) + bin_limit <
               first_part3_limit) {
      bin_limits[bin_index] = bin_limit;
      bin_index++;
      bin_limit = std::round(bin_limit * factor);
    }
  }

  /*
   * Part 3 with fixed sized bins covering 5 HB intervals.
   *
   * For small heartbeat intervals this part will fill up with bins of minimal
   * size until bins cover 5 heartbeat intervals.
   * For bigger heartbeat intervals bins will be added using half heartbeat
   * interval as with until bins cover 5 heartbeat intervals. In this case the
   * heartbeat interval will be the boundary of a bin. If heartbeat interval is
   * odd the interval will be rounded up to ensure the bin will be wider than
   * last bin in exponential part.
   */
  unsigned bin_width = half_hb_interval_up;
  if (first_part3_limit == 0) {
    unsigned prev_bin_limit = (bin_index > 0 ? bin_limits[bin_index - 1] : 0);
    unsigned prev_bin_width [[maybe_unused]] =
        (bin_index > 1 ? prev_bin_limit - bin_limits[bin_index - 2]
                       : prev_bin_limit);
    if ((half_hb_interval_up <= min_bin_width) ||
        (hb_interval - prev_bin_limit > half_hb_interval_up)) {
      /*
       * If half hb_interval is too small, or, if width of bin with hb_interval
       * as limit would be to large, continue with smallest bin width.
       */
      assert(prev_bin_width == 0 || prev_bin_width == min_bin_width);
      bin_width = min_bin_width;
    } else {
      /*
       * All looks good, go one with next bin using hb_interval as limit with
       * half hb_interval as bin widths.
       */
      bin_limit = hb_interval;
    }
  } else {
    bin_limit = first_part3_limit;
  }
  unsigned stop = 5 * hb_interval + bin_width;
  while (bin_index < max_bin_count - 1 && bin_limit < stop) {
    bin_limits[bin_index] = bin_limit;
    bin_index++;
    bin_limit += bin_width;
  }

  /*
   * Part 4 with the last infinity bin.
   */
  bin_limits[bin_index] = UINT_MAX;
  bin_index++;

  return bin_index;
}

unsigned Trpman::verify_histogram(unsigned interval,
                                  const std::span<unsigned> bin_limits) {
  const unsigned sample_interval = TRP_TIME_SIGNAL_DELAY;
  const unsigned min_bin_width = 2 * sample_interval;
  const size_t bin_count = bin_limits.size();
  const unsigned high_interval = 5 * interval;
  unsigned ret = 0;

  if (interval == 0) {
    ret |= 1;
  }
  if (min_bin_width == 0) {
    ret |= 2;
  }
  if (bin_count < 2) {
    ret |= 4;
  } else if (high_interval > bin_limits[bin_count - 2]) {
    ret |= 8;
  }
  if (bin_count > 1 && bin_limits[bin_count - 1] != UINT_MAX) {
    ret |= 16;
  }
  unsigned prev_width = bin_limits[0];
  if (prev_width != min_bin_width) {
    ret |= 32;
  }
  for (unsigned i = 1; i < bin_count; i++) {
    unsigned width = bin_limits[i] - bin_limits[i - 1];
    if (prev_width > width) {
      ret |= 64;
    }
    prev_width = width;
  }

  return ret;
}

void Trpman::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();
  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  m_dbHbSender = 0;
  m_dbHbSenderTrp = 0;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  m_hbDbDb = 5000;  // ms
  ndb_mgm_get_int_parameter(p, CFG_DB_HEARTBEAT_INTERVAL, &m_hbDbDb);
  m_hbDbDb_bin_count =
      calculate_histogram_bin_limits(m_hbDbDb, m_hbDbDb_bin_bounds);
  ndbassert(verify_histogram(m_hbDbDb,
                             {m_hbDbDb_bin_bounds, m_hbDbDb_bin_count}) == 0);

  m_hbDbApi = 1500;  // ms
  ndb_mgm_get_int_parameter(p, CFG_DB_API_HEARTBEAT_INTERVAL, &m_hbDbApi);
  m_hbDbApi_bin_count =
      calculate_histogram_bin_limits(m_hbDbApi, m_hbDbApi_bin_bounds);
  ndbassert(verify_histogram(m_hbDbApi,
                             {m_hbDbApi_bin_bounds, m_hbDbApi_bin_count}) == 0);

  memset(m_trp_activity, 0, sizeof(m_trp_activity));

  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Trpman::execSTTOR(Signal *signal) {
  jamEntry();
  Uint32 theStartPhase = signal->theData[1];

  if (theStartPhase == 1) {
    jam();
    Uint32 tmp[25];
    Uint32 len = signal->getLength();
    memcpy(tmp, signal->theData, len << 2);

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    sendTIME_SIGNAL(signal, now, TRP_TIME_SIGNAL_DELAY);

    memcpy(signal->theData, tmp, len << 2);
  }
  if (theStartPhase == 8) {
#ifdef ERROR_INSERT
    if (ERROR_INSERTED(9004)) {
      CLEAR_ERROR_INSERT_VALUE;
      Uint32 tmp[25];
      Uint32 len = signal->getLength();
      memcpy(tmp, signal->theData, sizeof(tmp));

      Uint32 db;
      for (db = 1; db < MAX_NDB_NODES; db++) {
        if (db == getOwnNodeId()) continue;
        if (getNodeInfo(db).getType() == NodeInfo::DB) break;
      }
      ndbrequire(db < MAX_NDB_NODES);

      Uint32 i = c_error_9000_nodes_mask.find(1);
      const TrpId trpId = get_the_only_base_trp(i);
      if (handles_this_trp(trpId)) {
        signal->theData[0] = i;
        sendSignal(calcQmgrBlockRef(db), GSN_API_FAILREQ, signal, 1, JBA);
        g_eventLogger->info("stopping %u using %u", i, db);

        memcpy(signal->theData, tmp, sizeof(tmp));
        sendSignalWithDelay(reference(), GSN_STTOR, signal, 100, len);
        return;
      }
    }
#endif
  }

  signal->theData[3] = 1;
  signal->theData[4] = 8;
  signal->theData[5] = 255;
  BlockReference ref = !isNdbMtLqh() ? NDBCNTR_REF : TRPMAN_REF;
  sendSignal(ref, GSN_STTORRY, signal, 6, JBB);
  return;
}

void Trpman::execNDB_TAMPER(Signal *signal) {
  jamEntry();
  SimulatedBlock::execNDB_TAMPER(signal);
#ifdef ERROR_INSERT

  if (signal->theData[0] == 9003) {
    if (MAX_RECEIVED_SIGNALS < 1024) {
      MAX_RECEIVED_SIGNALS = 1024;
    } else {
      MAX_RECEIVED_SIGNALS = 1 + (rand() % 128);
    }
    g_eventLogger->info("MAX_RECEIVED_SIGNALS: %d", MAX_RECEIVED_SIGNALS);
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (ERROR_INSERTED(9006)) {
    g_eventLogger->info("Activating error 9006 for SEGV of all nodes");
    /*
     * Disable this injected crash to generate core files. We can not use the
     * CRASH_INSERTION macro here since it modifies the node start type in an
     * unwanted way when testing fix for Bug #24945638 STOPONERROR = 0 WITH
     * UNCONTROLLED EXIT RESTARTS IN SAME WAY AS PREVIOUS RESTART.
     * Instead, we explicitly turn off core file generation by directly
     * modifying the opt_core variable of main.cpp.
     */
    extern bool opt_core;
    opt_core = false;
    raise(SIGSEGV);
  }
#endif
}  // execNDB_TAMPER()

void Trpman::execDUMP_STATE_ORD(Signal *signal) {
  DumpStateOrd *const &dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg [[maybe_unused]] = dumpState->args[0];

#ifdef ERROR_INSERT
  if (arg == 9000 || arg == 9002) {
    SET_ERROR_INSERT_VALUE(arg);
    c_error_9000_nodes_mask.clear();
    for (Uint32 i = 1; i < signal->getLength(); i++)
      c_error_9000_nodes_mask.set(signal->theData[i]);
  }

  if (arg == 9001) {
    CLEAR_ERROR_INSERT_VALUE;
    if (signal->getLength() == 1 || signal->theData[1]) {
      signal->header.theLength = 2;
      for (Uint32 i = 1; i < MAX_NODES; i++) {
        if (c_error_9000_nodes_mask.get(i)) {
          signal->theData[0] = 0;
          signal->theData[1] = i;
          execOPEN_COMORD(signal);
        }
      }
    }
    c_error_9000_nodes_mask.clear();
  }

  if (arg == 9004 && signal->getLength() == 2) {
    SET_ERROR_INSERT_VALUE(9004);
    c_error_9000_nodes_mask.clear();
    c_error_9000_nodes_mask.set(signal->theData[1]);
  }

  if (arg == 9006) {
    Uint32 delay = 1000;
    switch (signal->getLength()) {
      case 1:
        break;
      case 2:
        delay = signal->theData[1];
        break;
      default: {
        Uint32 dmin = signal->theData[1];
        Uint32 dmax = signal->theData[2];
        delay = dmin + (rand() % (dmax - dmin));
        break;
      }
    }
    signal->theData[0] = arg;
    if (delay == 0) {
      execNDB_TAMPER(signal);
    } else if (delay < 10) {
      sendSignal(reference(), GSN_NDB_TAMPER, signal, 1, JBB);
    } else {
      sendSignalWithDelay(reference(), GSN_NDB_TAMPER, signal, delay, 1);
    }
  }
#endif

#ifdef ERROR_INSERT
  /* <Target NodeId> dump 9992 <NodeId list>
   * On Target NodeId, block receiving signals from NodeId list
   *
   * <Target NodeId> dump 9993 <NodeId list>
   * On Target NodeId, resume receiving signals from NodeId list
   *
   * <Target NodeId> dump 9991
   * On Target NodeId, resume receiving signals from any blocked node
   *
   *
   * See also code in QMGR for blocking receive from nodes based
   * on HB roles.
   *
   */
  if ((arg == 9993) || /* Unblock recv from nodeid */
      (arg == 9992))   /* Block recv from nodeid */
  {
    bool block = (arg == 9992);
    TransporterReceiveHandle *recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 n = 1; n < signal->getLength(); n++) {
      const NodeId nodeId = signal->theData[n];
      if ((nodeId > 0) && (nodeId < MAX_NODES)) {
        TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
        Uint32 num_ids;
        globalTransporterRegistry.lockMultiTransporters();
        globalTransporterRegistry.get_trps_for_node(
            nodeId, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);
        globalTransporterRegistry.unlockMultiTransporters();

        for (unsigned i = 0; i < num_ids; i++) {
          if (!handles_this_trp(trp_ids[i])) continue;
          if (block) {
            if (!globalTransporterRegistry.isBlocked(trp_ids[i])) {
              g_eventLogger->info(
                  "(%u)TRPMAN : Blocking receive"
                  " on transporter %u from node %u",
                  instance(), trp_ids[i], nodeId);
              globalTransporterRegistry.blockReceive(*recvdata, trp_ids[i]);
            } else {
              g_eventLogger->info(
                  "TRPMAN : Ignoring dump %u for transporter %u"
                  " (receive link already blocked)",
                  arg, trp_ids[i]);
            }
          } else {
            if (globalTransporterRegistry.isBlocked(trp_ids[i])) {
              g_eventLogger->info(
                  "(%u)TRPMAN : Unblocking receive"
                  " on transporter %u from node %u",
                  instance(), trp_ids[i], nodeId);

              globalTransporterRegistry.unblockReceive(*recvdata, trp_ids[i]);
            } else {
              g_eventLogger->info(
                  "TRPMAN : Ignoring dump %u for transporter %u"
                  " (receive link is not blocked)",
                  arg, trp_ids[i]);
            }
          }
        }
      } else {
        g_eventLogger->info("TRPMAN : Ignoring dump %u for node %u", arg,
                            nodeId);
      }
    }
  }
  if (arg == 9990) /* Block recv from all ndbd matching pattern */
  {
    Uint32 pattern = 0;
    if (signal->getLength() > 1) {
      pattern = signal->theData[1];
      g_eventLogger->info(
          "TRPMAN : Blocking receive from all ndbds matching pattern -%s-",
          ((pattern == 1) ? "Other side" : "Unknown"));
    }

    TransporterReceiveHandle *recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 node = 1; node < MAX_NDB_NODES; node++) {
      if (node == getOwnNodeId()) continue;

      // Get all node (multi-)Transporters, block all/some
      TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
      Uint32 num_ids;
      globalTransporterRegistry.lockMultiTransporters();
      globalTransporterRegistry.get_trps_for_node(node, trp_ids, num_ids,
                                                  MAX_NODE_GROUP_TRANSPORTERS);
      globalTransporterRegistry.unlockMultiTransporters();

      for (unsigned i = 0; i < num_ids; i++) {
        if (!handles_this_trp(trp_ids[i])) continue;
        if (globalTransporterRegistry.is_connected(trp_ids[i])) {
          if (getNodeInfo(node).m_type == NodeInfo::DB) {
            if (!globalTransporterRegistry.isBlocked(trp_ids[i])) {
              switch (pattern) {
                case 1: {
                  /* Match if given node is on 'other side' of
                   * 2-replica cluster
                   */
                  if ((getOwnNodeId() & 1) != (node & 1)) {
                    /* Node is on the 'other side', match */
                    break;
                  }
                  /* Node is on 'my side', don't match */
                  continue;
                }
                default:
                  break;
              }
              g_eventLogger->info(
                  "(%u)TRPMAN : Blocking receive on transporter %u"
                  " from node %u",
                  instance(), trp_ids[i], node);
              globalTransporterRegistry.blockReceive(*recvdata, trp_ids[i]);
            }
          }
        }
      }
    }
  }
  if (arg == 9991) /* Unblock recv from all blocked */
  {
    TransporterReceiveHandle *recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 node = 1; node < MAX_NODES; node++) {
      if (node == getOwnNodeId()) continue;

      // Get all node (multi-)Transporters, unblock all
      TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
      Uint32 num_ids;
      globalTransporterRegistry.lockMultiTransporters();
      globalTransporterRegistry.get_trps_for_node(node, trp_ids, num_ids,
                                                  MAX_NODE_GROUP_TRANSPORTERS);
      globalTransporterRegistry.unlockMultiTransporters();

      for (unsigned i = 0; i < num_ids; i++) {
        if (!handles_this_trp(trp_ids[i])) continue;
        if (globalTransporterRegistry.isBlocked(trp_ids[i])) {
          g_eventLogger->info(
              "(%u)TRPMAN : Unblocking receive on transporter %u"
              " from node %u",
              instance(), trp_ids[i], node);
          globalTransporterRegistry.unblockReceive(*recvdata, trp_ids[i]);
        }
      }
    }
  }
  if (arg == 9988 || /* Block send to node X */
      arg == 9989)   /* Unblock send to node X */
  {
    bool block = (arg == 9988);
    TransporterReceiveHandle *recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 n = 1; n < signal->getLength(); n++) {
      const NodeId nodeId = signal->theData[n];

      if ((nodeId > 0) && (nodeId < MAX_NODES)) {
        TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
        Uint32 num_ids;
        globalTransporterRegistry.lockMultiTransporters();
        globalTransporterRegistry.get_trps_for_node(
            nodeId, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);
        globalTransporterRegistry.unlockMultiTransporters();

        for (unsigned i = 0; i < num_ids; i++) {
          if (!handles_this_trp(trp_ids[i])) continue;
          g_eventLogger->info(
              "TRPMAN : Send on transporter %u to node %u"
              " is %sblocked",
              trp_ids[i], nodeId,
              (globalTransporterRegistry.isSendBlocked(trp_ids[i]) ? ""
                                                                   : "not "));
          if (block) {
            g_eventLogger->info(
                "TRPMAN : Blocking send on transporter %u"
                " to node %u",
                trp_ids[i], nodeId);
            globalTransporterRegistry.blockSend(*recvdata, trp_ids[i]);
          } else {
            g_eventLogger->info(
                "TRPMAN : Unblocking send on transporter %u"
                " to node %u",
                trp_ids[i], nodeId);

            globalTransporterRegistry.unblockSend(*recvdata, trp_ids[i]);
          }
        }
      } else {
        g_eventLogger->info("TRPMAN : Ignoring dump %u for node %u", arg,
                            nodeId);
      }
    }
  }

#endif
}

void Trpman::sendSYNC_THREAD_VIA_CONF(Signal *signal, Uint32 senderData,
                                      Uint32 retVal) {
  jamEntry();
  SyncThreadViaReqConf *conf = (SyncThreadViaReqConf *)signal->getDataPtr();
  conf->senderData = senderData;
  const BlockReference receiver = isMultiThreaded() ? TRPMAN_REF : QMGR_REF;
  sendSignal(receiver, GSN_SYNC_THREAD_VIA_CONF, signal, signal->getLength(),
             JBA);
}

void Trpman::execSYNC_THREAD_VIA_REQ(Signal *signal) {
  jam();
  SyncThreadViaReqConf *req = (SyncThreadViaReqConf *)signal->getDataPtr();

  /* Some ugliness as we have nowhere handy to put the sender's reference */
  ndbassert(refToMain(req->senderRef) == (isMultiThreaded() ? TRPMAN : QMGR));

  Callback cb = {safe_cast(&Trpman::sendSYNC_THREAD_VIA_CONF), req->senderData};
  /* Make sure all external signals handled by transporters belonging to this
   * TRPMAN have been processed.
   */
  synchronize_external_signals(signal, cb);
}

bool Trpman::getParam(const char *name, Uint32 *count) {
  /* Trpman uses synchronize_threads_for_block(THRMAN) prior sending
   * NODE_FAILREP.
   * An overestimate of the maximum possible concurrent NODE_FAILREP is one
   * node failure per NODE_FAILREP, and all nodes failing!
   */
  if (strcmp(name, "ActiveThreadSync") != 0) {
    return false;
  }
  *count = MAX_DATA_NODE_ID;
  return true;
}

void Trpman::execACTIVATE_TRP_REQ(Signal *signal) {
  ActivateTrpReq *req = (ActivateTrpReq *)&signal->theData[0];
  Uint32 node_id = req->nodeId;
  Uint32 trp_id = req->trpId;
  BlockReference ret_ref = req->senderRef;

  /* Switch from node transporter to multi transporter for heartbeat. Work will
   * redundantly be done for signal for each new multi transporter while only
   * needed to be done once for node. */
  if (m_dbHbSender == node_id) {
    ndbrequire(m_dbHbSenderTrp != 0);
    set_db_hb_sender(m_dbHbSender);  // recalculates transporter for dbHbSender
  }

  /**
   * Note similarity with ::enable_com_node(), which enable the
   * *node* communication. Now we enable an addition transporter
   * to an already enabled node.
   */
  if (is_recv_thread_for_new_trp(trp_id)) {
    globalTransporterRegistry.setIOState(trp_id, NoHalt);
    DEB_MULTI_TRP(("(%u)ACTIVATE_TRP_REQ is receiver (%u,%u)", instance(),
                   node_id, trp_id));
    ActivateTrpConf *conf = CAST_PTR(ActivateTrpConf, signal->getDataPtrSend());
    conf->nodeId = node_id;
    conf->trpId = trp_id;
    conf->senderRef = reference();
    sendSignal(ret_ref, GSN_ACTIVATE_TRP_CONF, signal,
               ActivateTrpConf::SignalLength, JBB);
  } else {
    DEB_MULTI_TRP(("(%u)ACTIVATE_TRP_REQ is not receiver (%u,%u)", instance(),
                   node_id, trp_id));
  }
}

Uint32 Trpman::distribute_signal(SignalHeader *const header,
                                 const Uint32 instance_no) {
  DistributionHandler *handle = &m_distribution_handle;
  Uint32 gsn = header->theVerId_signalNumber;
  ndbrequire(m_distribution_handler_inited);
  if (gsn == GSN_LQHKEYREQ) {
    return get_lqhkeyreq_ref(handle, instance_no);
  } else if (gsn == GSN_SCAN_FRAGREQ) {
    return get_scan_fragreq_ref(handle, instance_no);
  } else {
    return 0;
  }
}

void Trpman::execUPD_QUERY_DIST_ORD(Signal *signal) {
  /**
   * Receive an array of weights for each LDM and query thread.
   * These weights are used to create an array used for a quick round robin
   * distribution of the signals received in distribute_signal.
   */
  DistributionHandler *dist_handle = &m_distribution_handle;
  if (!m_distribution_handler_inited) {
    fill_distr_references(dist_handle);
    calculate_distribution_signal(dist_handle);
    m_distribution_handler_inited = true;
  }
  ndbrequire(signal->getNoOfSections() == 1);
  SegmentedSectionPtr ptr;
  SectionHandle handle(this, signal);
  ndbrequire(handle.getSection(ptr, 0));
  ndbrequire(ptr.sz <= NDB_ARRAY_SIZE(dist_handle->m_weights));

  memset(dist_handle->m_weights, 0, sizeof(dist_handle->m_weights));
  copy(dist_handle->m_weights, ptr);
  releaseSections(handle);
  calculate_distribution_signal(dist_handle);
}

void Trpman::execTIME_SIGNAL(Signal *signal) {
  jam();
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  sendTIME_SIGNAL(signal, now, TRP_TIME_SIGNAL_DELAY);

  for (unsigned trp_id = m_recv_data.find_first();
       trp_id != m_recv_data.NotFound;
       trp_id = m_recv_data.find_next(trp_id + 1)) {
    ndbassert(handles_this_trp(trp_id));
    if (!globalTransporterRegistry.is_connected(trp_id)) continue;

    NDB_TICKS trp_last_recv = globalTransporterRegistry.get_last_recv(trp_id);
    if (likely(NdbTick_IsValid(m_trp_activity[trp_id].last_recv))) {
      NodeId node_id =
          globalTransporterRegistry.get_transporter_node_id(trp_id);
      bool is_db = (getNodeInfo(node_id).getType() == NODE_TYPE_DB);
      /*
       * We only know the time for the current last receive, not the first
       * receive in the last 50ms after a period of no data. By that elapsed_ms
       * may be an overestimate by up to 50ms.
       */
      Uint64 elapsed_ms =
          NdbTick_Elapsed(m_trp_activity[trp_id].last_recv, trp_last_recv)
              .milliSec();

      // Update activity histogram
      unsigned hist_bin_index = 0;
      unsigned hist_bin_count;
      const Uint32 *hist_bin_bounds;
      if (is_db) {
        hist_bin_count = m_hbDbDb_bin_count;
        hist_bin_bounds = m_hbDbDb_bin_bounds;
      } else {
        hist_bin_count = m_hbDbApi_bin_count;
        hist_bin_bounds = m_hbDbApi_bin_bounds;
      }
      while (hist_bin_index < hist_bin_count &&
             hist_bin_bounds[hist_bin_index] < elapsed_ms)
        hist_bin_index++;
      m_trp_activity[trp_id].hist_bins[hist_bin_index]++;

      // Log late heartbeat
      if (!is_db || trp_id == m_dbHbSenderTrp) {
        Uint32 heartbeat_interval = is_db ? m_hbDbDb : m_hbDbApi;
        /*
         * Since elapsed_ms may be an overestimate and not to report
         * late heartbeat when it was in time add TRP_TIME_SIGNAL_DELAY.
         */
        if (elapsed_ms > heartbeat_interval + TRP_TIME_SIGNAL_DELAY) {
          signal->theData[0] = NDB_LE_LateHeartbeat;
          signal->theData[1] = node_id;
          signal->theData[2] = elapsed_ms;
          sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
        }
      }
    }
    m_trp_activity[trp_id].last_recv = trp_last_recv;
  }
  m_recv_data.clear();
}

TrpmanProxy::TrpmanProxy(Block_context &ctx) : LocalProxy(TRPMAN, ctx) {
  addRecSignal(GSN_OPEN_COMORD, &TrpmanProxy::execOPEN_COMORD);
  addRecSignal(GSN_ENABLE_COMREQ, &TrpmanProxy::execENABLE_COMREQ);
  addRecSignal(GSN_ENABLE_COMCONF, &TrpmanProxy::execENABLE_COMCONF);
  addRecSignal(GSN_CLOSE_COMREQ, &TrpmanProxy::execCLOSE_COMREQ);
  addRecSignal(GSN_CLOSE_COMCONF, &TrpmanProxy::execCLOSE_COMCONF);
  addRecSignal(GSN_ROUTE_ORD, &TrpmanProxy::execROUTE_ORD);
  addRecSignal(GSN_SYNC_THREAD_VIA_REQ, &TrpmanProxy::execSYNC_THREAD_VIA_REQ);
  addRecSignal(GSN_SYNC_THREAD_VIA_CONF,
               &TrpmanProxy::execSYNC_THREAD_VIA_CONF);
  addRecSignal(GSN_ACTIVATE_TRP_REQ, &TrpmanProxy::execACTIVATE_TRP_REQ);
  addRecSignal(GSN_NODE_START_REP, &TrpmanProxy::execNODE_START_REP, true);
}

TrpmanProxy::~TrpmanProxy() {}

SimulatedBlock *TrpmanProxy::newWorker(Uint32 instanceNo) {
  return new Trpman(m_ctx, instanceNo);
}

BLOCK_FUNCTIONS(TrpmanProxy)

// GSN_OPEN_COMORD

void TrpmanProxy::execOPEN_COMORD(Signal *signal) {
  jamEntry();

  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    sendSignal(workerRef(i), GSN_OPEN_COMORD, signal, signal->getLength(), JBB);
  }
}

// GSN_CLOSE_COMREQ

void TrpmanProxy::execCLOSE_COMREQ(Signal *signal) {
  jamEntry();
  ndbrequire(signal->getLength() >= CloseComReqConf::SignalLengthDB);
  Ss_CLOSE_COMREQ &ss = ssSeize<Ss_CLOSE_COMREQ>();
  const CloseComReqConf *req = (const CloseComReqConf *)signal->getDataPtr();
  ss.m_req = *req;
  if (req->failedNodeId == 0) {
    ndbrequire(signal->getNoOfSections() == 1);
    SectionHandle handle(this, signal);
    saveSections(ss, handle);
  } else {
    ndbrequire(signal->getNoOfSections() == 0);
  }
  sendREQ(signal, ss);
}

void TrpmanProxy::sendCLOSE_COMREQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *handle) {
  jam();
  Ss_CLOSE_COMREQ &ss = ssFind<Ss_CLOSE_COMREQ>(ssId);
  CloseComReqConf *req = (CloseComReqConf *)signal->getDataPtrSend();

  *req = ss.m_req;
  req->xxxBlockRef = reference();
  req->failNo = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_CLOSE_COMREQ, signal,
                      CloseComReqConf::SignalLengthDB, JBB, handle);
}

void TrpmanProxy::execCLOSE_COMCONF(Signal *signal) {
  const CloseComReqConf *conf = (const CloseComReqConf *)signal->getDataPtr();
  ndbrequire(signal->getLength() >= CloseComReqConf::SignalLengthDB);
  Uint32 ssId = conf->failNo;
  jamEntry();
  Ss_CLOSE_COMREQ &ss = ssFind<Ss_CLOSE_COMREQ>(ssId);
  recvCONF(signal, ss);
}

void TrpmanProxy::sendCLOSE_COMCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_CLOSE_COMREQ &ss = ssFind<Ss_CLOSE_COMREQ>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  CloseComReqConf *conf = (CloseComReqConf *)signal->getDataPtrSend();
  *conf = ss.m_req;
  sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal,
             CloseComReqConf::SignalLengthDB, JBB);
  ssRelease<Ss_CLOSE_COMREQ>(ssId);
}

// GSN_ENABLE_COMREQ

void TrpmanProxy::execENABLE_COMREQ(Signal *signal) {
  jamEntry();
  Ss_ENABLE_COMREQ &ss = ssSeize<Ss_ENABLE_COMREQ>();
  const EnableComReq *req = (const EnableComReq *)signal->getDataPtr();
  ss.m_req = *req;
  SectionHandle handle(this, signal);
  saveSections(ss, handle);
  sendREQ(signal, ss);
}

void TrpmanProxy::sendENABLE_COMREQ(Signal *signal, Uint32 ssId,
                                    SectionHandle *handle) {
  jam();
  Ss_ENABLE_COMREQ &ss = ssFind<Ss_ENABLE_COMREQ>(ssId);
  EnableComReq *req = (EnableComReq *)signal->getDataPtrSend();

  *req = ss.m_req;
  req->m_senderRef = reference();
  req->m_senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_ENABLE_COMREQ, signal,
                      EnableComReq::SignalLength, JBB, handle);
}

void TrpmanProxy::execENABLE_COMCONF(Signal *signal) {
  const EnableComConf *conf = (const EnableComConf *)signal->getDataPtr();
  Uint32 ssId = conf->m_senderData;
  jamEntry();
  Ss_ENABLE_COMREQ &ss = ssFind<Ss_ENABLE_COMREQ>(ssId);
  recvCONF(signal, ss);
}

void TrpmanProxy::sendENABLE_COMCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_ENABLE_COMREQ &ss = ssFind<Ss_ENABLE_COMREQ>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  EnableComReq *conf = (EnableComReq *)signal->getDataPtr();
  *conf = ss.m_req;
  sendSignal(conf->m_senderRef, GSN_ENABLE_COMCONF, signal,
             EnableComReq::SignalLength, JBB);
  ssRelease<Ss_ENABLE_COMREQ>(ssId);
}

// GSN_ROUTE_ORD

void TrpmanProxy::execROUTE_ORD(Signal *signal) {
  jamEntry();

  RouteOrd *ord = (RouteOrd *)signal->getDataPtr();
  Uint32 nodeId = ord->from;
  ndbassert(nodeId != 0);

  Uint32 workerIndex = 0;

  if (globalData.ndbMtReceiveThreads > (Uint32)1) {
    /**
     * This signal is sent from QMGR at API node failures to ensure that all
     * signals have been received from the API before continue. We know that
     * API nodes have only one transporter, so therefore we can use
     * get_trps_for_node returning only one transporter id.
     */
    TrpId trp_id;
    Uint32 num_ids;
    globalTransporterRegistry.lockMultiTransporters();
    globalTransporterRegistry.get_trps_for_node(nodeId, &trp_id, num_ids, 1);
    globalTransporterRegistry.unlockMultiTransporters();
    workerIndex = get_recv_thread_idx(trp_id);
    ndbrequire(workerIndex < globalData.ndbMtReceiveThreads);
  }

  SectionHandle handle(this, signal);
  sendSignal(workerRef(workerIndex), GSN_ROUTE_ORD, signal, signal->getLength(),
             JBB, &handle);
}

// GSN_SYNC_THREAD_VIA

void TrpmanProxy::execSYNC_THREAD_VIA_REQ(Signal *signal) {
  jamEntry();
  Ss_SYNC_THREAD_VIA &ss = ssSeize<Ss_SYNC_THREAD_VIA>();
  const SyncThreadViaReqConf *req =
      (const SyncThreadViaReqConf *)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void TrpmanProxy::sendSYNC_THREAD_VIA_REQ(Signal *signal, Uint32 ssId,
                                          SectionHandle *) {
  jam();
  SyncThreadViaReqConf *req = (SyncThreadViaReqConf *)signal->getDataPtr();
  req->senderRef = reference();
  req->senderData = ssId;
  Ss_SYNC_THREAD_VIA &ss = ssFind<Ss_SYNC_THREAD_VIA>(ssId);
  sendSignal(workerRef(ss.m_worker), GSN_SYNC_THREAD_VIA_REQ, signal,
             SyncThreadViaReqConf::SignalLength, JBA);
}

void TrpmanProxy::execSYNC_THREAD_VIA_CONF(Signal *signal) {
  jamEntry();
  const SyncThreadViaReqConf *conf =
      (const SyncThreadViaReqConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_SYNC_THREAD_VIA &ss = ssFind<Ss_SYNC_THREAD_VIA>(ssId);
  recvCONF(signal, ss);
}

void TrpmanProxy::sendSYNC_THREAD_VIA_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_SYNC_THREAD_VIA &ss = ssFind<Ss_SYNC_THREAD_VIA>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  SyncThreadViaReqConf *conf = (SyncThreadViaReqConf *)signal->getDataPtr();
  *conf = ss.m_req;
  sendSignal(conf->senderRef, GSN_SYNC_THREAD_VIA_CONF, signal,
             NodeFailRep::SignalLength, JBB);
  ssRelease<Ss_SYNC_THREAD_VIA>(ssId);
}

void TrpmanProxy::execACTIVATE_TRP_REQ(Signal *signal) {
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    Uint32 ref = numberToRef(number(), workerInstance(i), getOwnNodeId());
    sendSignal(ref, GSN_ACTIVATE_TRP_REQ, signal, signal->getLength(), JBB);
  }
}

void TrpmanProxy::execNODE_START_REP(Signal *signal) {
  jamEntry();
  // Resend to workers
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    Uint32 ref = numberToRef(number(), workerInstance(i), getOwnNodeId());
    sendSignal(ref, GSN_NODE_START_REP, signal, signal->getLength(), JBB);
  }
}
