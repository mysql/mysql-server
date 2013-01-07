/*
  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "trpman.hpp"
#include <TransporterRegistry.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/DisconnectRep.hpp>
#include <signaldata/EnableCom.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <mt.hpp>

Trpman::Trpman(Block_context & ctx, Uint32 instanceno) :
  SimulatedBlock(TRPMAN, ctx, instanceno)
{
  BLOCK_CONSTRUCTOR(Trpman);

  addRecSignal(GSN_CLOSE_COMREQ, &Trpman::execCLOSE_COMREQ);
  addRecSignal(GSN_CLOSE_COMCONF, &Trpman::execCLOSE_COMCONF);
  addRecSignal(GSN_OPEN_COMORD, &Trpman::execOPEN_COMORD);
  addRecSignal(GSN_ENABLE_COMREQ, &Trpman::execENABLE_COMREQ);
  addRecSignal(GSN_DISCONNECT_REP, &Trpman::execDISCONNECT_REP);
  addRecSignal(GSN_CONNECT_REP, &Trpman::execCONNECT_REP);
  addRecSignal(GSN_ROUTE_ORD, &Trpman::execROUTE_ORD);

  addRecSignal(GSN_NDB_TAMPER, &Trpman::execNDB_TAMPER, true);
  addRecSignal(GSN_DUMP_STATE_ORD, &Trpman::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Trpman::execDBINFO_SCANREQ);
}

Trpman::~Trpman()
{
}

BLOCK_FUNCTIONS(Trpman)

#ifdef ERROR_INSERT
static NodeBitmask c_error_9000_nodes_mask;
extern Uint32 MAX_RECEIVED_SIGNALS;
#endif

bool
Trpman::handles_this_node(Uint32 nodeId)
{
#ifndef NDBD_MULTITHREADED
  return true;
#else
  if (globalData.ndbMtReceiveThreads <= (Uint32)1)
    return true;
  return (instance()== (mt_get_recv_thread_idx(nodeId) + /* proxy */ 1));
#endif
}

void
Trpman::execOPEN_COMORD(Signal* signal)
{
  // Connect to the specifed NDB node, only QMGR allowed communication
  // so far with the node

  const BlockReference userRef = signal->theData[0];
  Uint32 tStartingNode = signal->theData[1];
  Uint32 tData2 = signal->theData[2];
  jamEntry();

  const Uint32 len = signal->getLength();
  if (len == 2)
  {
#ifdef ERROR_INSERT
    if (! ((ERROR_INSERTED(9000) || ERROR_INSERTED(9002))
	   && c_error_9000_nodes_mask.get(tStartingNode)))
#endif
    {
      if (!handles_this_node(tStartingNode))
      {
        jam();
        goto done;
      }

      globalTransporterRegistry.do_connect(tStartingNode);
      globalTransporterRegistry.setIOState(tStartingNode, HaltIO);

      //-----------------------------------------------------
      // Report that the connection to the node is opened
      //-----------------------------------------------------
      signal->theData[0] = NDB_LE_CommunicationOpened;
      signal->theData[1] = tStartingNode;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      //-----------------------------------------------------
    }
  }
  else
  {
    for(unsigned int i = 1; i < MAX_NODES; i++ )
    {
      jam();
      if (i != getOwnNodeId() && getNodeInfo(i).m_type == tData2 &&
          handles_this_node(i))
      {
	jam();

#ifdef ERROR_INSERT
	if ((ERROR_INSERTED(9000) || ERROR_INSERTED(9002))
	    && c_error_9000_nodes_mask.get(i))
	  continue;
#endif
	globalTransporterRegistry.do_connect(i);
	globalTransporterRegistry.setIOState(i, HaltIO);

	signal->theData[0] = NDB_LE_CommunicationOpened;
	signal->theData[1] = i;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      }
    }
  }

done:
  /**
   * NO REPLY for now
   */
  (void)userRef;
}

void
Trpman::execCONNECT_REP(Signal *signal)
{
  const Uint32 hostId = signal->theData[0];
  jamEntry();

  const NodeInfo::NodeType type = (NodeInfo::NodeType)getNodeInfo(hostId).m_type;
  ndbrequire(type != NodeInfo::INVALID);

  /**
   * Inform QMGR that client has connected
   */
  signal->theData[0] = hostId;
  if (ERROR_INSERTED(9005))
  {
    sendSignalWithDelay(QMGR_REF, GSN_CONNECT_REP, signal, 50, 1);
  }
  else
  {
    sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
  }

  /* Automatically subscribe events for MGM nodes.
   */
  if (type == NodeInfo::MGM)
  {
    jam();
    globalTransporterRegistry.setIOState(hostId, NoHalt);
  }

  //------------------------------------------
  // Also report this event to the Event handler
  //------------------------------------------
  signal->theData[0] = NDB_LE_Connected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}

void
Trpman::execCLOSE_COMREQ(Signal* signal)
{
  // Close communication with the node and halt input/output from
  // other blocks than QMGR

  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  const BlockReference userRef = closeCom->xxxBlockRef;
  Uint32 requestType = closeCom->requestType;
  Uint32 failNo = closeCom->failNo;
//  Uint32 noOfNodes = closeCom->noOfNodes;

  jamEntry();
  for (unsigned i = 1; i < MAX_NODES; i++)
  {
    if (NodeBitmask::get(closeCom->theNodes, i) &&
        handles_this_node(i))
    {
      jam();

      //-----------------------------------------------------
      // Report that the connection to the node is closed
      //-----------------------------------------------------
      signal->theData[0] = NDB_LE_CommunicationClosed;
      signal->theData[1] = i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

      globalTransporterRegistry.setIOState(i, HaltIO);
      globalTransporterRegistry.do_disconnect(i);
    }
  }

  if (requestType != CloseComReqConf::RT_NO_REPLY)
  {
    ndbassert((requestType == CloseComReqConf::RT_API_FAILURE) ||
              ((requestType == CloseComReqConf::RT_NODE_FAILURE) &&
               (failNo != 0)));
    jam();
    CloseComReqConf* closeComConf = (CloseComReqConf *)signal->getDataPtrSend();
    closeComConf->xxxBlockRef = userRef;
    closeComConf->requestType = requestType;
    closeComConf->failNo = failNo;

    /* Note assumption that noOfNodes and theNodes
     * bitmap is not trampled above
     * signals received from the remote node.
     */
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMCONF, signal, 19, JBA);
  }
}

/*
  We need to implement CLOSE_COMCONF signal for the non-multithreaded
  case where message should go to QMGR, for multithreaded case it
  needs to pass through TRPMAN proxy on its way back.
*/
void
Trpman::execCLOSE_COMCONF(Signal *signal)
{
  jamEntry();
  sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal, 19, JBA);
}

void
Trpman::execENABLE_COMREQ(Signal* signal)
{
  jamEntry();
  const EnableComReq *enableComReq = (const EnableComReq *)signal->getDataPtr();

  /* Need to copy out signal data to not clobber it with sendSignal(). */
  BlockReference senderRef = enableComReq->m_senderRef;
  Uint32 senderData = enableComReq->m_senderData;
  Uint32 nodes[NodeBitmask::Size];
  MEMCOPY_NO_WORDS(nodes, enableComReq->m_nodeIds, NodeBitmask::Size);

  /* Enable communication with all our NDB blocks to these nodes. */
  Uint32 search_from = 1;
  for (;;)
  {
    Uint32 tStartingNode = NodeBitmask::find(nodes, search_from);
    if (tStartingNode == NodeBitmask::NotFound)
      break;
    search_from = tStartingNode + 1;

    if (!handles_this_node(tStartingNode))
      continue;
    globalTransporterRegistry.setIOState(tStartingNode, NoHalt);
    setNodeInfo(tStartingNode).m_connected = true;

    //-----------------------------------------------------
    // Report that the version of the node
    //-----------------------------------------------------
    signal->theData[0] = NDB_LE_ConnectedApiVersion;
    signal->theData[1] = tStartingNode;
    signal->theData[2] = getNodeInfo(tStartingNode).m_version;
    signal->theData[3] = getNodeInfo(tStartingNode).m_mysql_version;

    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
    //-----------------------------------------------------
  }

  EnableComConf *enableComConf = (EnableComConf *)signal->getDataPtrSend();
  enableComConf->m_senderRef = reference();
  enableComConf->m_senderData = senderData;
  MEMCOPY_NO_WORDS(enableComConf->m_nodeIds, nodes, NodeBitmask::Size);
  sendSignal(senderRef, GSN_ENABLE_COMCONF, signal,
             EnableComConf::SignalLength, JBA);
}

void
Trpman::execDISCONNECT_REP(Signal *signal)
{
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 hostId = rep->nodeId;
  jamEntry();

  setNodeInfo(hostId).m_connected = false;
  setNodeInfo(hostId).m_connectCount++;
  const NodeInfo::NodeType type = getNodeInfo(hostId).getType();
  ndbrequire(type != NodeInfo::INVALID);

  sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal,
             DisconnectRep::SignalLength, JBA);

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
void
Trpman::execROUTE_ORD(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal))
  {
    jam();
    return;
  }

  SectionHandle handle(this, signal);

  RouteOrd* ord = (RouteOrd*)signal->getDataPtr();
  Uint32 dstRef = ord->dstRef;
  Uint32 srcRef = ord->srcRef;
  Uint32 gsn = ord->gsn;
  /* ord->cnt ignored */

  Uint32 nodeId = refToNode(dstRef);

  if (likely((nodeId == 0) ||
             getNodeInfo(nodeId).m_connected))
  {
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
      handle.m_ptr[i] = handle.m_ptr[i+1];
    handle.m_cnt--;

    sendSignal(dstRef, gsn, signal, sigLen, JBB, &handle);

    handle.m_cnt = 1;
    handle.m_ptr[0] = save;
    releaseSections(handle);
    return ;
  }

  releaseSections(handle);
  warningEvent("Unable to route GSN: %d from %x to %x",
	       gsn, srcRef, dstRef);
}

void
Trpman::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::TRANSPORTERS_TABLEID:
  {
    jam();
    Uint32 rnode = cursor->data[0];
    if (rnode == 0)
      rnode++; // Skip node 0

    while (rnode < MAX_NODES)
    {
      if (!handles_this_node(rnode))
      {
        rnode++;
        continue;
      }

      switch(getNodeInfo(rnode).m_type)
      {
      default:
      {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId()); // Node id
        row.write_uint32(rnode); // Remote node id
        row.write_uint32(globalTransporterRegistry.getPerformState(rnode)); // State

        /* Connect address */
        if (globalTransporterRegistry.get_transporter(rnode) != NULL &&
            globalTransporterRegistry.get_connect_address(rnode).s_addr != 0)
        {
          row.write_string(inet_ntoa(globalTransporterRegistry.get_connect_address(rnode)));
        }
        else
        {
          row.write_string("-");
        }

        /* Bytes sent/received */
        if (globalTransporterRegistry.get_transporter(rnode) != NULL)
        {
          row.write_uint64(globalTransporterRegistry.get_bytes_sent(rnode));
          row.write_uint64(globalTransporterRegistry.get_bytes_received(rnode));
        }
        else
        {
          row.write_uint64(0);
          row.write_uint64(0);
        }

        ndbinfo_send_row(signal, req, row, rl);
        break;
      }

      case NodeInfo::INVALID:
        jam();
       break;
      }

      rnode++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, rnode);
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

void
Trpman::execNDB_TAMPER(Signal* signal)
{
  jamEntry();
#ifdef ERROR_INSERT
  if (signal->theData[0] == 9003)
  {
    if (MAX_RECEIVED_SIGNALS < 1024)
    {
      MAX_RECEIVED_SIGNALS = 1024;
    }
    else
    {
      MAX_RECEIVED_SIGNALS = 1 + (rand() % 128);
    }
    ndbout_c("MAX_RECEIVED_SIGNALS: %d", MAX_RECEIVED_SIGNALS);
    CLEAR_ERROR_INSERT_VALUE;
  }
#endif
}//execNDB_TAMPER()

void
Trpman::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0]; (void)arg;

#ifdef ERROR_INSERT
  if (arg == 9000 || arg == 9002)
  {
    SET_ERROR_INSERT_VALUE(arg);
    for (Uint32 i = 1; i<signal->getLength(); i++)
      c_error_9000_nodes_mask.set(signal->theData[i]);
  }

  if (arg == 9001)
  {
    CLEAR_ERROR_INSERT_VALUE;
    if (signal->getLength() == 1 || signal->theData[1])
    {
      signal->header.theLength = 2;
      for (Uint32 i = 1; i<MAX_NODES; i++)
      {
        if (c_error_9000_nodes_mask.get(i) &&
            handles_this_node(i))
        {
          signal->theData[0] = 0;
          signal->theData[1] = i;
          execOPEN_COMORD(signal);
        }
      }
    }
    c_error_9000_nodes_mask.clear();
  }

  if (arg == 9004 && signal->getLength() == 2)
  {
    SET_ERROR_INSERT_VALUE(9004);
    c_error_9000_nodes_mask.clear();
    c_error_9000_nodes_mask.set(signal->theData[1]);
  }

  if (arg == 9005 && signal->getLength() == 2 && ERROR_INSERTED(9004))
  {
    Uint32 db = signal->theData[1];
    Uint32 i = c_error_9000_nodes_mask.find(1);
    if (handles_this_node(i))
    {
      signal->theData[0] = i;
      sendSignal(calcQmgrBlockRef(db),GSN_API_FAILREQ, signal, 1, JBA);
      ndbout_c("stopping %u using %u", i, db);
    }
    CLEAR_ERROR_INSERT_VALUE;
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
  if((arg == 9993) ||  /* Unblock recv from nodeid */
     (arg == 9992))    /* Block recv from nodeid */
  {
    bool block = (arg == 9992);
    TransporterReceiveHandle * recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 n = 1; n < signal->getLength(); n++)
    {
      Uint32 nodeId = signal->theData[n];
      if (!handles_this_node(nodeId))
        continue;

      if ((nodeId > 0) &&
          (nodeId < MAX_NODES))
      {
        if (block)
        {
          ndbout_c("TRPMAN : Blocking receive from node %u", nodeId);
          globalTransporterRegistry.blockReceive(*recvdata, nodeId);
        }
        else
        {
          ndbout_c("TRPMAN : Unblocking receive from node %u", nodeId);

          globalTransporterRegistry.unblockReceive(*recvdata, nodeId);
        }
      }
      else
      {
        ndbout_c("TRPMAN : Ignoring dump %u for node %u",
                 arg, nodeId);
      }
    }
  }
  if (arg == 9990) /* Block recv from all ndbd matching pattern */
  {
    Uint32 pattern = 0;
    if (signal->getLength() > 1)
    {
      pattern = signal->theData[1];
      ndbout_c("TRPMAN : Blocking receive from all ndbds matching pattern -%s-",
               ((pattern == 1)? "Other side":"Unknown"));
    }

    TransporterReceiveHandle * recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 node = 1; node < MAX_NDB_NODES; node++)
    {
      if (!handles_this_node(node))
        continue;
      if (globalTransporterRegistry.is_connected(node))
      {
        if (getNodeInfo(node).m_type == NodeInfo::DB)
        {
          if (!globalTransporterRegistry.isBlocked(node))
          {
            switch (pattern)
            {
            case 1:
            {
              /* Match if given node is on 'other side' of
               * 2-replica cluster
               */
              if ((getOwnNodeId() & 1) != (node & 1))
              {
                /* Node is on the 'other side', match */
                break;
              }
              /* Node is on 'my side', don't match */
              continue;
            }
            default:
              break;
            }
            ndbout_c("TRPMAN : Blocking receive from node %u", node);
            globalTransporterRegistry.blockReceive(*recvdata, node);
          }
        }
      }
    }
  }
  if (arg == 9991) /* Unblock recv from all blocked */
  {
    TransporterReceiveHandle * recvdata = mt_get_trp_receive_handle(instance());
    assert(recvdata != 0);
    for (Uint32 node = 1; node < MAX_NODES; node++)
    {
      if (!handles_this_node(node))
        continue;
      if (globalTransporterRegistry.isBlocked(node))
      {
        ndbout_c("CMVMI : Unblocking receive from node %u", node);
        globalTransporterRegistry.unblockReceive(*recvdata, node);
      }
    }
  }
#endif
}

TrpmanProxy::TrpmanProxy(Block_context & ctx) :
  LocalProxy(TRPMAN, ctx)
{
  addRecSignal(GSN_OPEN_COMORD, &TrpmanProxy::execOPEN_COMORD);
  addRecSignal(GSN_ENABLE_COMREQ, &TrpmanProxy::execENABLE_COMREQ);
  addRecSignal(GSN_ENABLE_COMCONF, &TrpmanProxy::execENABLE_COMCONF);
  addRecSignal(GSN_CLOSE_COMREQ, &TrpmanProxy::execCLOSE_COMREQ);
  addRecSignal(GSN_CLOSE_COMCONF, &TrpmanProxy::execCLOSE_COMCONF);
  addRecSignal(GSN_ROUTE_ORD, &TrpmanProxy::execROUTE_ORD);
}

TrpmanProxy::~TrpmanProxy()
{
}

SimulatedBlock*
TrpmanProxy::newWorker(Uint32 instanceNo)
{
  return new Trpman(m_ctx, instanceNo);
}

BLOCK_FUNCTIONS(TrpmanProxy);

// GSN_OPEN_COMORD

void
TrpmanProxy::execOPEN_COMORD(Signal* signal)
{
  jamEntry();

  for (Uint32 i = 0; i<c_workers; i++)
  {
    jam();
    sendSignal(workerRef(i), GSN_OPEN_COMORD, signal,
               signal->getLength(), JBB);
  }
}

// GSN_CLOSE_COMREQ

void
TrpmanProxy::execCLOSE_COMREQ(Signal* signal)
{
  jamEntry();
  Ss_CLOSE_COMREQ& ss = ssSeize<Ss_CLOSE_COMREQ>();
  const CloseComReqConf* req = (const CloseComReqConf*)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void
TrpmanProxy::sendCLOSE_COMREQ(Signal *signal, Uint32 ssId, SectionHandle*)
{
  jam();
  Ss_CLOSE_COMREQ& ss = ssFind<Ss_CLOSE_COMREQ>(ssId);
  CloseComReqConf* req = (CloseComReqConf*)signal->getDataPtrSend();

  *req = ss.m_req;
  req->xxxBlockRef = reference();
  req->failNo = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_CLOSE_COMREQ, signal,
             CloseComReqConf::SignalLength, JBB);
}

void
TrpmanProxy::execCLOSE_COMCONF(Signal* signal)
{
  const CloseComReqConf* conf = (const CloseComReqConf*)signal->getDataPtr();
  Uint32 ssId = conf->failNo;
  jamEntry();
  Ss_CLOSE_COMREQ& ss = ssFind<Ss_CLOSE_COMREQ>(ssId);
  recvCONF(signal, ss);
}

void
TrpmanProxy::sendCLOSE_COMCONF(Signal *signal, Uint32 ssId)
{
  jam();
  Ss_CLOSE_COMREQ& ss = ssFind<Ss_CLOSE_COMREQ>(ssId);

  if (!lastReply(ss))
  {
    jam();
    return;
  }

  CloseComReqConf* conf = (CloseComReqConf*)signal->getDataPtrSend();
  *conf = ss.m_req;
  sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal,
             CloseComReqConf::SignalLength, JBB);
  ssRelease<Ss_CLOSE_COMREQ>(ssId);
}

// GSN_ENABLE_COMREQ

void
TrpmanProxy::execENABLE_COMREQ(Signal* signal)
{
  jamEntry();
  Ss_ENABLE_COMREQ& ss = ssSeize<Ss_ENABLE_COMREQ>();
  const EnableComReq* req = (const EnableComReq*)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void
TrpmanProxy::sendENABLE_COMREQ(Signal *signal, Uint32 ssId, SectionHandle*)
{
  jam();
  Ss_ENABLE_COMREQ& ss = ssFind<Ss_ENABLE_COMREQ>(ssId);
  EnableComReq* req = (EnableComReq*)signal->getDataPtrSend();

  *req = ss.m_req;
  req->m_senderRef = reference();
  req->m_senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_ENABLE_COMREQ, signal,
             EnableComReq::SignalLength, JBB);
}

void
TrpmanProxy::execENABLE_COMCONF(Signal* signal)
{
  const EnableComConf* conf = (const EnableComConf*)signal->getDataPtr();
  Uint32 ssId = conf->m_senderData;
  jamEntry();
  Ss_ENABLE_COMREQ& ss = ssFind<Ss_ENABLE_COMREQ>(ssId);
  recvCONF(signal, ss);
}

void
TrpmanProxy::sendENABLE_COMCONF(Signal *signal, Uint32 ssId)
{
  jam();
  Ss_ENABLE_COMREQ& ss = ssFind<Ss_ENABLE_COMREQ>(ssId);

  if (!lastReply(ss))
  {
    jam();
    return;
  }

  EnableComReq* conf = (EnableComReq*)signal->getDataPtr();
  *conf = ss.m_req;
  sendSignal(conf->m_senderRef, GSN_ENABLE_COMCONF, signal,
             EnableComReq::SignalLength, JBB);
  ssRelease<Ss_ENABLE_COMREQ>(ssId);
}

// GSN_ROUTE_ORD

void
TrpmanProxy::execROUTE_ORD(Signal* signal)
{
  RouteOrd* ord = (RouteOrd*)signal->getDataPtr();
  Uint32 nodeId = ord->from;
  jamEntry();

  ndbassert(nodeId != 0);
#ifndef NDBD_MULTITHREADED
  Uint32 workerId = 0;
#else
  Uint32 workerId = mt_get_recv_thread_idx(nodeId);
#endif
  SectionHandle handle(this, signal);
  sendSignal(workerRef(workerId), GSN_ROUTE_ORD, signal,
             signal->getLength(), JBB, &handle);
}
