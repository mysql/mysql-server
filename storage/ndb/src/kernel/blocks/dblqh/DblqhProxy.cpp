/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "DblqhProxy.hpp"
#include "Dblqh.hpp"
#include "DblqhCommon.hpp"

#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/ExecFragReq.hpp>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/StartFragReq.hpp>

#define JAM_FILE_ID 442

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
// #define DEBUG_EXEC_SR 1
#endif

#ifdef DEBUG_EXEC_SR
#define DEB_EXEC_SR(arglist)     \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_EXEC_SR(arglist) \
  do {                       \
  } while (0)
#endif

DblqhProxy::DblqhProxy(Block_context &ctx)
    : LocalProxy(DBLQH, ctx), c_tableRecSize(0), c_tableRec(0) {
  m_received_wait_all = false;
  m_lcp_started = false;
  m_outstanding_wait_lcp = 0;
  m_outstanding_start_node_lcp_req = 0;

  // GSN_CREATE_TAB_REQ
  addRecSignal(GSN_CREATE_TAB_REQ, &DblqhProxy::execCREATE_TAB_REQ);
  addRecSignal(GSN_CREATE_TAB_CONF, &DblqhProxy::execCREATE_TAB_CONF);
  addRecSignal(GSN_CREATE_TAB_REF, &DblqhProxy::execCREATE_TAB_REF);

  // GSN_LQHADDATTREQ
  addRecSignal(GSN_LQHADDATTREQ, &DblqhProxy::execLQHADDATTREQ);
  addRecSignal(GSN_LQHADDATTCONF, &DblqhProxy::execLQHADDATTCONF);
  addRecSignal(GSN_LQHADDATTREF, &DblqhProxy::execLQHADDATTREF);

  // GSN_LQHFRAGREQ
  addRecSignal(GSN_LQHFRAGREQ, &DblqhProxy::execLQHFRAGREQ);

  // GSN_TAB_COMMITREQ
  addRecSignal(GSN_TAB_COMMITREQ, &DblqhProxy::execTAB_COMMITREQ);
  addRecSignal(GSN_TAB_COMMITCONF, &DblqhProxy::execTAB_COMMITCONF);
  addRecSignal(GSN_TAB_COMMITREF, &DblqhProxy::execTAB_COMMITREF);

  // GSN_LCP_FRAG_ORD
  addRecSignal(GSN_LCP_FRAG_ORD, &DblqhProxy::execLCP_FRAG_ORD);
  addRecSignal(GSN_LCP_FRAG_REP, &DblqhProxy::execLCP_FRAG_REP);
  addRecSignal(GSN_END_LCPCONF, &DblqhProxy::execEND_LCPCONF);
  addRecSignal(GSN_LCP_COMPLETE_REP, &DblqhProxy::execLCP_COMPLETE_REP);

  // GSN_GCP_SAVEREQ
  addRecSignal(GSN_GCP_SAVEREQ, &DblqhProxy::execGCP_SAVEREQ);
  addRecSignal(GSN_GCP_SAVECONF, &DblqhProxy::execGCP_SAVECONF);
  addRecSignal(GSN_GCP_SAVEREF, &DblqhProxy::execGCP_SAVEREF);

  // GSN_PREP_DROP_TAB_REQ
  addRecSignal(GSN_PREP_DROP_TAB_REQ, &DblqhProxy::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_PREP_DROP_TAB_CONF, &DblqhProxy::execPREP_DROP_TAB_CONF);
  addRecSignal(GSN_PREP_DROP_TAB_REF, &DblqhProxy::execPREP_DROP_TAB_REF);

  // GSN_DROP_TAB_REQ
  addRecSignal(GSN_DROP_TAB_REQ, &DblqhProxy::execDROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_CONF, &DblqhProxy::execDROP_TAB_CONF);
  addRecSignal(GSN_DROP_TAB_REF, &DblqhProxy::execDROP_TAB_REF);

  // GSN_ALTER_TAB_REQ
  addRecSignal(GSN_ALTER_TAB_REQ, &DblqhProxy::execALTER_TAB_REQ);
  addRecSignal(GSN_ALTER_TAB_CONF, &DblqhProxy::execALTER_TAB_CONF);
  addRecSignal(GSN_ALTER_TAB_REF, &DblqhProxy::execALTER_TAB_REF);

  // GSN_START_FRAGREQ
  addRecSignal(GSN_START_FRAGREQ, &DblqhProxy::execSTART_FRAGREQ);

  // GSN_START_RECREQ
  addRecSignal(GSN_START_RECREQ, &DblqhProxy::execSTART_RECREQ);
  addRecSignal(GSN_START_RECCONF, &DblqhProxy::execSTART_RECCONF);
  addRecSignal(GSN_LOCAL_RECOVERY_COMP_REP,
               &DblqhProxy::execLOCAL_RECOVERY_COMP_REP);

  // GSN_LQH_TRANSREQ
  addRecSignal(GSN_LQH_TRANSREQ, &DblqhProxy::execLQH_TRANSREQ);
  addRecSignal(GSN_LQH_TRANSCONF, &DblqhProxy::execLQH_TRANSCONF);

  // GSN_SUB_GCP_COMPLETE_REP
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &DblqhProxy::execSUB_GCP_COMPLETE_REP);

  // GSN_UNDO_LOG_LEVEL_REP
  addRecSignal(GSN_UNDO_LOG_LEVEL_REP, &DblqhProxy::execUNDO_LOG_LEVEL_REP);

  // GSN_START_NODE_LCP_REQ
  addRecSignal(GSN_START_NODE_LCP_REQ, &DblqhProxy::execSTART_NODE_LCP_REQ);

  // GSN_START_NODE_LCP_CONF
  addRecSignal(GSN_START_NODE_LCP_CONF, &DblqhProxy::execSTART_NODE_LCP_CONF);

  // GSN_EXEC_SRREQ
  addRecSignal(GSN_EXEC_SRREQ, &DblqhProxy::execEXEC_SRREQ);
  addRecSignal(GSN_EXEC_SRCONF, &DblqhProxy::execEXEC_SRCONF);

  // GSN_EXEC_FRAG
  addRecSignal(GSN_EXEC_FRAGREQ, &DblqhProxy::execEXEC_FRAGREQ);
  addRecSignal(GSN_EXEC_FRAGCONF, &DblqhProxy::execEXEC_FRAGCONF);

  // GSN_DROP_FRAG_REQ
  addRecSignal(GSN_DROP_FRAG_REQ, &DblqhProxy::execDROP_FRAG_REQ);
  addRecSignal(GSN_DROP_FRAG_CONF, &DblqhProxy::execDROP_FRAG_CONF);
  addRecSignal(GSN_DROP_FRAG_REF, &DblqhProxy::execDROP_FRAG_REF);

  // GSN_INFO_GCP_STOP_TIMER
  addRecSignal(GSN_INFO_GCP_STOP_TIMER, &DblqhProxy::execINFO_GCP_STOP_TIMER);
}

DblqhProxy::~DblqhProxy() {}

SimulatedBlock *DblqhProxy::newWorker(Uint32 instanceNo) {
  return new Dblqh(m_ctx, instanceNo, DBLQH);
}

// GSN_NDB_STTOR

void DblqhProxy::callNDB_STTOR(Signal *signal) {
  jam();
  Ss_READ_NODES_REQ &ss = c_ss_READ_NODESREQ;
  ndbrequire(ss.m_gsn == 0);

  const Uint32 startPhase = signal->theData[2];
  switch (startPhase) {
    case 3:
      jam();
      ss.m_gsn = GSN_NDB_STTOR;
      sendREAD_NODESREQ(signal);
      break;
    default:
      jam();
      backNDB_STTOR(signal);
      break;
  }
}

// GSN_READ_CONFIG_REQ
void DblqhProxy::callREAD_CONFIG_REQ(Signal *signal) {
  jam();
  const ReadConfigReq *req = (const ReadConfigReq *)signal->getDataPtr();
  ndbrequire(req->noOfParameters == 0);

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE, &c_tableRecSize));
  c_tableRec = (Uint8 *)allocRecord("TableRec", sizeof(Uint8), c_tableRecSize);
  D("proxy:" << V(c_tableRecSize));
  Uint32 i;
  for (i = 0; i < c_tableRecSize; i++) c_tableRec[i] = 0;
  backREAD_CONFIG_REQ(signal);
}

// GSN_INFO_GCP_STOP_TIMER
void DblqhProxy::execINFO_GCP_STOP_TIMER(Signal *signal) {
  jam();
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    sendSignal(workerRef(i), GSN_INFO_GCP_STOP_TIMER, signal,
               signal->getLength(), JBB);
  }
}

// GSN_CREATE_TAB_REQ

// there is no consistent LQH connect pointer to use as ssId

void DblqhProxy::execCREATE_TAB_REQ(Signal *signal) {
  jam();
  Ss_CREATE_TAB_REQ &ss = ssSeize<Ss_CREATE_TAB_REQ>(1);

  const CreateTabReq *req = (const CreateTabReq *)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == CreateTabReq::SignalLengthLDM);

  sendREQ(signal, ss);
}

void DblqhProxy::sendCREATE_TAB_REQ(Signal *signal, Uint32 ssId,
                                    SectionHandle *handle) {
  jam();
  Ss_CREATE_TAB_REQ &ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);

  CreateTabReq *req = (CreateTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_CREATE_TAB_REQ, signal,
                      CreateTabReq::SignalLengthLDM, JBB, handle);
}

void DblqhProxy::execCREATE_TAB_CONF(Signal *signal) {
  jam();
  const CreateTabConf *conf = (const CreateTabConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_CREATE_TAB_REQ &ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execCREATE_TAB_REF(Signal *signal) {
  jam();
  const CreateTabRef *ref = (const CreateTabRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_CREATE_TAB_REQ &ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendCREATE_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_CREATE_TAB_REQ &ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  {
    const CreateTabConf *conf = (const CreateTabConf *)signal->getDataPtr();
    ss.m_lqhConnectPtr[ss.m_worker] = conf->lqhConnectPtr;
  }

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    CreateTabConf *conf = (CreateTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->lqhConnectPtr = ssId;
    sendSignal(dictRef, GSN_CREATE_TAB_CONF, signal,
               CreateTabConf::SignalLength, JBB);

    // inform DBTUP proxy
    CreateTabReq *req = (CreateTabReq *)signal->getDataPtrSend();
    *req = ss.m_req;
    EXECUTE_DIRECT(DBTUP, GSN_CREATE_TAB_REQ, signal,
                   CreateTabReq::SignalLength);

    Uint32 tableId = ss.m_req.tableId;
    ndbrequire(tableId < c_tableRecSize);
    c_tableRec[tableId] = 1;
  } else {
    jam();
    CreateTabRef *ref = (CreateTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->errorStatus = 0;
    sendSignal(dictRef, GSN_CREATE_TAB_REF, signal, CreateTabRef::SignalLength,
               JBB);
    ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  }
}

// GSN_LQHADDATTREQ [ sub-op ]

void DblqhProxy::execLQHADDATTREQ(Signal *signal) {
  jam();
  const LqhAddAttrReq *req = (const LqhAddAttrReq *)signal->getDataPtr();
  Uint32 ssId = req->lqhFragPtr;
  Ss_LQHADDATTREQ &ss = ssSeize<Ss_LQHADDATTREQ>(ssId);

  const Uint32 reqlength = LqhAddAttrReq::HeaderLength +
                           req->noOfAttributes * LqhAddAttrReq::EntryLength;
  ndbrequire(signal->getLength() == reqlength);
  memcpy(&ss.m_req, req, reqlength << 2);
  ss.m_reqlength = reqlength;

  /**
   * Count LQHFRAGREQ,
   *   so that I can release CREATE_TAB_REQ after last attribute has been
   *   processed
   */
  Ss_CREATE_TAB_REQ &ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  ndbrequire(ss_main.m_req.noOfAttributes >= req->noOfAttributes);
  ss_main.m_req.noOfAttributes -= req->noOfAttributes;

  /* Save long section(s) in ss for forwarding to
   * workers
   */
  SectionHandle handle(this, signal);
  saveSections(ss, handle);

  sendREQ(signal, ss);
}

void DblqhProxy::sendLQHADDATTREQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *handle) {
  jam();
  Ss_LQHADDATTREQ &ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  Ss_CREATE_TAB_REQ &ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);

  LqhAddAttrReq *req = (LqhAddAttrReq *)signal->getDataPtrSend();
  const Uint32 reqlength = ss.m_reqlength;
  memcpy(req, &ss.m_req, reqlength << 2);
  req->lqhFragPtr = ss_main.m_lqhConnectPtr[ss.m_worker];
  req->noOfAttributes = ss.m_req.noOfAttributes;
  req->senderData = ssId;
  req->senderAttrPtr = ss.m_req.senderAttrPtr;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_LQHADDATTREQ, signal,
                      reqlength, JBB, handle);
}

void DblqhProxy::execLQHADDATTCONF(Signal *signal) {
  jam();
  const LqhAddAttrConf *conf = (const LqhAddAttrConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_LQHADDATTREQ &ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execLQHADDATTREF(Signal *signal) {
  jam();
  const LqhAddAttrRef *ref = (const LqhAddAttrRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_LQHADDATTREQ &ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendLQHADDATTCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_LQHADDATTREQ &ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  Ss_CREATE_TAB_REQ &ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss_main.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    LqhAddAttrConf *conf = (LqhAddAttrConf *)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->senderAttrPtr = ss.m_req.senderAttrPtr;
    sendSignal(dictRef, GSN_LQHADDATTCONF, signal, LqhAddAttrConf::SignalLength,
               JBB);

    if (ss_main.m_req.noOfAttributes == 0) {
      jam();
      /**
       * All the attributes has been processed
       *   release create_table object
       */
      ssRelease<Ss_CREATE_TAB_REQ>(ssId);
    }
  } else {
    jam();
    LqhAddAttrRef *ref = (LqhAddAttrRef *)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_LQHADDATTREF, signal, LqhAddAttrRef::SignalLength,
               JBB);
    ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  }

  ssRelease<Ss_LQHADDATTREQ>(ssId);
}

// GSN_LQHFRAGREQ [ pass-through ]

void DblqhProxy::execLQHFRAGREQ(Signal *signal) {
  jam();
  LqhFragReq *req = (LqhFragReq *)signal->getDataPtrSend();
  Uint32 instanceNo = getInstance(req->tableId, req->fragId);

  /* Ensure instance hasn't quietly mapped back to proxy! */
  ndbrequire(signal->getSendersBlockRef() != reference());

  // wl4391_todo impl. method that fakes senders block-ref
  sendSignal(numberToRef(DBLQH, instanceNo, getOwnNodeId()), GSN_LQHFRAGREQ,
             signal, signal->getLength(), JBB);
}

// GSN_TAB_COMMITREQ [ sub-op ]

void DblqhProxy::execTAB_COMMITREQ(Signal *signal) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssSeize<Ss_TAB_COMMITREQ>(1);  // lost connection

  const TabCommitReq *req = (const TabCommitReq *)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void DblqhProxy::sendTAB_COMMITREQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *handle) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  TabCommitReq *req = (TabCommitReq *)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ssId;
  req->tableId = ss.m_req.tableId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_TAB_COMMITREQ, signal,
                      TabCommitReq::SignalLength, JBB, handle);
}

void DblqhProxy::execTAB_COMMITCONF(Signal *signal) {
  jam();
  const TabCommitConf *conf = (TabCommitConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execTAB_COMMITREF(Signal *signal) {
  jam();
  const TabCommitRef *ref = (TabCommitRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  // wl4391_todo omit extra info now since DBDICT only does ndbrequire
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendTAB_COMMITCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    TabCommitConf *conf = (TabCommitConf *)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->nodeId = getOwnNodeId();
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITCONF, signal, TabCommitConf::SignalLength,
               JBB);
  } else {
    jam();
    TabCommitRef *ref = (TabCommitRef *)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->nodeId = getOwnNodeId();
    ref->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITREF, signal, TabCommitRef::SignalLength,
               JBB);
    return;
  }

  ssRelease<Ss_TAB_COMMITREQ>(ssId);
}

// LCP handling

Uint32 DblqhProxy::getNoOfOutstanding(const LcpRecord &rec) const {
  ndbrequire(rec.m_lcp_frag_ord_cnt >= rec.m_lcp_frag_rep_cnt);
  return rec.m_lcp_frag_ord_cnt - rec.m_lcp_frag_rep_cnt;
}

void DblqhProxy::execLCP_FRAG_ORD(Signal *signal) {
  jam();
  ndbrequire(signal->getLength() == LcpFragOrd::SignalLength);

  const LcpFragOrd *req = (const LcpFragOrd *)signal->getDataPtr();
  const LcpFragOrd req_copy = *req;

  bool lcp_complete_ord = req->lastFragmentFlag;

  if (c_lcpRecord.m_state == LcpRecord::L_IDLE) {
    jam();
    if (c_lcpRecord.m_lcpId == req->lcpId) {
      jam();
      /**
       * In master take over the new master doesn't get the perfect view
       * of our state immediately. So he can resend LCP_FRAG_ORDs that
       * were already sent. In this case we have already completed the
       * LCP and the new master is sending messages which it doesn't
       * know has been sent already.
       */
      D("LCP: Already completed" << V(req->lcpId));
      return;
    }
    D("LCP: start" << V(req->lcpId));
    c_lcpRecord.m_lcpId = req->lcpId;
    c_lcpRecord.m_keepGci = req->keepGci;
    c_lcpRecord.m_lcp_frag_rep_cnt = 0;
    c_lcpRecord.m_lcp_frag_ord_cnt = 0;
    c_lcpRecord.m_complete_outstanding = 0;
    c_lcpRecord.m_lastFragmentFlag = false;

    // handle start of LCP in PGMAN and TSMAN
    LcpFragOrd *req = (LcpFragOrd *)signal->getDataPtrSend();
    *req = req_copy;
    EXECUTE_DIRECT(TSMAN, GSN_LCP_FRAG_ORD, signal, LcpFragOrd::SignalLength);

    c_lcpRecord.m_state = LcpRecord::L_RUNNING;
  }

  jam();
  D("LCP: continue" << V(req->lcpId) << V(c_lcpRecord.m_lcp_frag_ord_cnt));
  ndbrequire(c_lcpRecord.m_lcpId == req->lcpId);

  if (c_lcpRecord.m_lastFragmentFlag) {
    jam();
    /**
     * We receive a new LCP_FRAG_ORD after already receiving a last flag on
     * the LCP. We can safely drop this signal since it must have been
     * generated by a new master that didn't know that the old master already
     * sent the last fragment flag.
     */
    D("LCP: lastFragmentFlag already received");
    return;
  }
  ndbrequire(c_lcpRecord.m_state == LcpRecord::L_RUNNING);
  if (lcp_complete_ord) {
    jam();
    c_lcpRecord.m_lastFragmentFlag = true;
    if (getNoOfOutstanding(c_lcpRecord) == 0) {
      jam();
      completeLCP(signal);
      return;
    }

    /**
     * Wait for all LCP_FRAG_ORD/REP to complete
     */
    return;
  } else {
    jam();
    c_lcpRecord.m_last_lcp_frag_ord = req_copy;
  }

  c_lcpRecord.m_lcp_frag_ord_cnt++;

  // Forward
  ndbrequire(req->tableId < c_tableRecSize);
  if (c_tableRec[req->tableId] == 0) {
    jam();
    /**
     * Table has been dropped.
     * Send to lqh-0...that will handle it...
     */
    sendSignal(workerRef(0), GSN_LCP_FRAG_ORD, signal, LcpFragOrd::SignalLength,
               JBB);
  } else {
    jam();
    Uint32 instanceNo = getInstance(req->tableId, req->fragmentId);
    sendSignal(numberToRef(DBLQH, instanceNo, getOwnNodeId()), GSN_LCP_FRAG_ORD,
               signal, LcpFragOrd::SignalLength, JBB);
  }
}

void DblqhProxy::execLCP_FRAG_REP(Signal *signal) {
  jam();
  LcpFragRep *conf = (LcpFragRep *)signal->getDataPtr();
  ndbrequire(c_lcpRecord.m_state == LcpRecord::L_RUNNING);
  c_lcpRecord.m_lcp_frag_rep_cnt++;

  if (signal->length() == 1) {
    /**
     * An ignored signal is reported back to keep track of number of
     * outstanding LCP_FRAG_ORD operations.
     */
    jam();
    if (c_lcpRecord.m_lastFragmentFlag &&
        (getNoOfOutstanding(c_lcpRecord) == 0)) {
      jam();
      completeLCP(signal);
    }
    return;
  }
  ndbrequire(signal->getLength() == LcpFragRep::SignalLength);
  ndbrequire(c_lcpRecord.m_lcpId == conf->lcpId);

  D("LCP: rep" << V(conf->lcpId) << V(c_lcpRecord.m_lcp_frag_rep_cnt));

  /**
   * But instead of broadcasting to all DIH's
   *   send to local that will do the broadcast
   */
  conf->nodeId = LcpFragRep::BROADCAST_REQ;
  sendSignal(DBDIH_REF, GSN_LCP_FRAG_REP, signal, LcpFragRep::SignalLength,
             JBB);

  if (c_lcpRecord.m_lastFragmentFlag) {
    jam();
    /**
     * lastFragmentFlag has arrived...
     */
    if (getNoOfOutstanding(c_lcpRecord) == 0) {
      jam();
      /*
       *   and we have all fragments has been processed
       */
      completeLCP(signal);
    }
    return;
  }
}

void DblqhProxy::completeLCP(Signal *signal) {
  jam();
  ndbrequire(c_lcpRecord.m_state == LcpRecord::L_RUNNING);
  c_lcpRecord.m_state = LcpRecord::L_COMPLETING_1;
  ndbrequire(c_lcpRecord.m_complete_outstanding == 0);

  /**
   * send LCP_FRAG_ORD (lastFragmentFlag = true)
   *   to all LQH instances...
   *   they will reply with LCP_COMPLETE_REP
   */
  LcpFragOrd *ord = (LcpFragOrd *)signal->getDataPtrSend();
  ord->tableId = RNIL;
  ord->fragmentId = RNIL;
  ord->lcpNo = RNIL;
  ord->lcpId = c_lcpRecord.m_lcpId;
  ord->lastFragmentFlag = true;
  ord->keepGci = c_lcpRecord.m_keepGci;
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    c_lcpRecord.m_complete_outstanding++;
    sendSignal(workerRef(i), GSN_LCP_FRAG_ORD, signal, LcpFragOrd::SignalLength,
               JBB);
  }
}

void DblqhProxy::execLCP_COMPLETE_REP(Signal *signal) {
  jamEntry();
  ndbrequire(c_lcpRecord.m_state == LcpRecord::L_COMPLETING_1);
  ndbrequire(c_lcpRecord.m_complete_outstanding);
  c_lcpRecord.m_complete_outstanding--;

  if (c_lcpRecord.m_complete_outstanding == 0) {
    /**
     * TSMAN needs to know when LCP is completed to know when it
     * can start reusing extents belonging to dropped tables.
     */
    jam();
    c_lcpRecord.m_state = LcpRecord::L_COMPLETING_2;
    c_lcpRecord.m_complete_outstanding++;
    EndLcpReq *req = (EndLcpReq *)signal->getDataPtr();
    req->senderData = 0; /* Ignored */
    req->senderRef = reference();
    req->backupPtr = 0; /* Ignored */
    req->backupId = c_lcpRecord.m_lcpId;
    req->proxyBlockNo = 0; /* Ignored */
    sendSignal(TSMAN_REF, GSN_END_LCPREQ, signal, EndLcpReq::SignalLength, JBB);
  }
}

void DblqhProxy::execEND_LCPCONF(Signal *signal) {
  jam();
  ndbrequire(c_lcpRecord.m_complete_outstanding);
  c_lcpRecord.m_complete_outstanding--;
  if (c_lcpRecord.m_complete_outstanding > 0) {
    jam();
    return;
  }
  sendLCP_COMPLETE_REP(signal);
}

void DblqhProxy::sendLCP_COMPLETE_REP(Signal *signal) {
  jam();
  ndbrequire(c_lcpRecord.m_state == LcpRecord::L_COMPLETING_2);

  LcpCompleteRep *conf = (LcpCompleteRep *)signal->getDataPtrSend();
  conf->nodeId = LcpFragRep::BROADCAST_REQ;
  conf->blockNo = DBLQH;
  conf->lcpId = c_lcpRecord.m_lcpId;
  sendSignal(DBDIH_REF, GSN_LCP_COMPLETE_REP, signal,
             LcpCompleteRep::SignalLength, JBB);

  c_lcpRecord.m_state = LcpRecord::L_IDLE;
}

// GSN_GCP_SAVEREQ

void DblqhProxy::execGCP_SAVEREQ(Signal *signal) {
  jam();
  const GCPSaveReq *req = (const GCPSaveReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_GCP_SAVEREQ &ss = ssSeize<Ss_GCP_SAVEREQ>(ssId);
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void DblqhProxy::sendGCP_SAVEREQ(Signal *signal, Uint32 ssId,
                                 SectionHandle *handle) {
  jam();
  Ss_GCP_SAVEREQ &ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  GCPSaveReq *req = (GCPSaveReq *)signal->getDataPtrSend();
  *req = ss.m_req;

  req->dihBlockRef = reference();
  req->dihPtr = ss.m_worker;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_GCP_SAVEREQ, signal,
                      GCPSaveReq::SignalLength, JBB, handle);
}

void DblqhProxy::execGCP_SAVECONF(Signal *signal) {
  jam();
  const GCPSaveConf *conf = (const GCPSaveConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_GCP_SAVEREQ &ss = ssFind<Ss_GCP_SAVEREQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execGCP_SAVEREF(Signal *signal) {
  jam();
  const GCPSaveRef *ref = (const GCPSaveRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_GCP_SAVEREQ &ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  if (ss.m_error != 0) {
    // wl4391_todo check
    jam();
    ndbrequire(ss.m_error == ref->errorCode);
  }
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendGCP_SAVECONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_GCP_SAVEREQ &ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    GCPSaveConf *conf = (GCPSaveConf *)signal->getDataPtrSend();
    conf->dihPtr = ss.m_req.dihPtr;
    conf->nodeId = getOwnNodeId();
    conf->gci = ss.m_req.gci;
    sendSignal(ss.m_req.dihBlockRef, GSN_GCP_SAVECONF, signal,
               GCPSaveConf::SignalLength, JBB);
  } else {
    jam();
    GCPSaveRef *ref = (GCPSaveRef *)signal->getDataPtrSend();
    ref->dihPtr = ss.m_req.dihPtr;
    ref->nodeId = getOwnNodeId();
    ref->gci = ss.m_req.gci;
    ref->errorCode = ss.m_error;
    sendSignal(ss.m_req.dihBlockRef, GSN_GCP_SAVEREF, signal,
               GCPSaveRef::SignalLength, JBB);
  }

  ssRelease<Ss_GCP_SAVEREQ>(ssId);
}

// GSN_SUB_GCP_COMPLETE_REP
void DblqhProxy::execSUB_GCP_COMPLETE_REP(Signal *signal) {
  jamEntry();
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    sendSignal(workerRef(i), GSN_SUB_GCP_COMPLETE_REP, signal,
               signal->getLength(), JBB);
  }
}

// GSN_UNDO_LOG_LEVEL_REP
void DblqhProxy::execUNDO_LOG_LEVEL_REP(Signal *signal) {
  jamEntry();
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    sendSignal(workerRef(i), GSN_UNDO_LOG_LEVEL_REP, signal,
               signal->getLength(), JBB);
  }
}

// GSN_START_NODE_LCP_REQ
void DblqhProxy::execSTART_NODE_LCP_REQ(Signal *signal) {
  jam();
  Uint32 current_gci = signal->theData[0];
  Uint32 restorable_gci = signal->theData[1];
  ndbrequire(m_outstanding_start_node_lcp_req == 0);
  m_outstanding_start_node_lcp_req = c_workers;
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    signal->theData[0] = current_gci;
    signal->theData[1] = restorable_gci;
    sendSignal(workerRef(i), GSN_START_NODE_LCP_REQ, signal,
               signal->getLength(), JBB);
  }
}

void DblqhProxy::execSTART_NODE_LCP_CONF(Signal *signal) {
  jamEntry();
  ndbrequire(m_outstanding_start_node_lcp_req > 0);
  m_outstanding_start_node_lcp_req--;
  if (m_outstanding_start_node_lcp_req > 0) {
    jam();
    return;
  }
  signal->theData[0] = 1;
  sendSignal(DBDIH_REF, GSN_START_NODE_LCP_CONF, signal, 1, JBB);
}

// GSN_PREP_DROP_TAB_REQ

void DblqhProxy::execPREP_DROP_TAB_REQ(Signal *signal) {
  jam();
  const PrepDropTabReq *req = (const PrepDropTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_PREP_DROP_TAB_REQ &ss = ssSeize<Ss_PREP_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == PrepDropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void DblqhProxy::sendPREP_DROP_TAB_REQ(Signal *signal, Uint32 ssId,
                                       SectionHandle *handle) {
  jam();
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);

  PrepDropTabReq *req = (PrepDropTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;  // redundant since tableId is used
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_PREP_DROP_TAB_REQ, signal,
                      PrepDropTabReq::SignalLength, JBB, handle);
}

void DblqhProxy::execPREP_DROP_TAB_CONF(Signal *signal) {
  jam();
  const PrepDropTabConf *conf = (const PrepDropTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execPREP_DROP_TAB_REF(Signal *signal) {
  jam();
  const PrepDropTabRef *ref = (const PrepDropTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendPREP_DROP_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    PrepDropTabConf *conf = (PrepDropTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_CONF, signal,
               PrepDropTabConf::SignalLength, JBB);
  } else {
    jam();
    PrepDropTabRef *ref = (PrepDropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_REF, signal,
               PrepDropTabRef::SignalLength, JBB);
  }

  ssRelease<Ss_PREP_DROP_TAB_REQ>(ssId);
}

// GSN_DROP_TAB_REQ

void DblqhProxy::execDROP_TAB_REQ(Signal *signal) {
  jam();
  const DropTabReq *req = (const DropTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_DROP_TAB_REQ &ss = ssSeize<Ss_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropTabReq::SignalLength);
  sendREQ(signal, ss);

  Uint32 tableId = ss.m_req.tableId;
  ndbrequire(tableId < c_tableRecSize);
  c_tableRec[tableId] = 0;
}

void DblqhProxy::sendDROP_TAB_REQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *handle) {
  jam();
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);

  DropTabReq *req = (DropTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;  // redundant since tableId is used
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_DROP_TAB_REQ, signal,
                      DropTabReq::SignalLength, JBB, handle);
}

void DblqhProxy::execDROP_TAB_CONF(Signal *signal) {
  jam();
  const DropTabConf *conf = (const DropTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execDROP_TAB_REF(Signal *signal) {
  jam();
  const DropTabRef *ref = (const DropTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendDROP_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    DropTabConf *conf = (DropTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_DROP_TAB_CONF, signal, DropTabConf::SignalLength,
               JBB);

    // inform DBTUP proxy
    DropTabReq *req = (DropTabReq *)signal->getDataPtrSend();
    *req = ss.m_req;
    EXECUTE_DIRECT(DBTUP, GSN_DROP_TAB_REQ, signal, DropTabReq::SignalLength);
  } else {
    jam();
    DropTabRef *ref = (DropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_DROP_TAB_REF, signal, DropTabConf::SignalLength,
               JBB);
  }

  ssRelease<Ss_DROP_TAB_REQ>(ssId);
}

// GSN_ALTER_TAB_REQ

void DblqhProxy::execALTER_TAB_REQ(Signal *signal) {
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  jamEntry();
  const AlterTabReq *req = (const AlterTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_ALTER_TAB_REQ &ss = ssSeize<Ss_ALTER_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == AlterTabReq::SignalLength);

  SectionHandle handle(this, signal);
  saveSections(ss, handle);

  sendREQ(signal, ss);
}

void DblqhProxy::sendALTER_TAB_REQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *handle) {
  jam();
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);

  AlterTabReq *req = (AlterTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_ALTER_TAB_REQ, signal,
                      AlterTabReq::SignalLength, JBB, handle);
}

void DblqhProxy::execALTER_TAB_CONF(Signal *signal) {
  jam();
  const AlterTabConf *conf = (const AlterTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execALTER_TAB_REF(Signal *signal) {
  jam();
  const AlterTabRef *ref = (const AlterTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DblqhProxy::sendALTER_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    AlterTabConf *conf = (AlterTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_ALTER_TAB_CONF, signal, AlterTabConf::SignalLength,
               JBB);
  } else {
    jam();
    AlterTabRef *ref = (AlterTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_ALTER_TAB_REF, signal, AlterTabConf::SignalLength,
               JBB);
  }

  ssRelease<Ss_ALTER_TAB_REQ>(ssId);
}

// GSN_START_FRAGREQ

void DblqhProxy::execSTART_FRAGREQ(Signal *signal) {
  jam();
  StartFragReq *req = (StartFragReq *)signal->getDataPtrSend();
  Uint32 instanceNo = getInstance(req->tableId, req->fragId);

  // wl4391_todo impl. method that fakes senders block-ref
  sendSignal(numberToRef(DBLQH, instanceNo, getOwnNodeId()), GSN_START_FRAGREQ,
             signal, signal->getLength(), JBB);
}

// GSN_START_RECREQ

void DblqhProxy::execSTART_RECREQ(Signal *signal) {
  jam();
  if (refToMain(signal->getSendersBlockRef()) == DBLQH) {
    jam();
    execSTART_RECREQ_2(signal);
    return;
  }

  StartRecReq *req = (StartRecReq *)signal->getDataPtr();
  if (signal->getNoOfSections() >= 1) {
    jam();
    Uint32 senderVersion =
        getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version;
    ndbrequire(ndbd_send_node_bitmask_in_section(senderVersion));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    memset(req->sr_nodes, 0, sizeof(req->sr_nodes));
    copy(req->sr_nodes, ptr);
    releaseSections(handle);
  } else {
    memset(req->sr_nodes + NdbNodeBitmask48::Size, 0, _NDB_NBM_DIFF_BYTES);
  }
  Ss_START_RECREQ &ss = ssSeize<Ss_START_RECREQ>();
  ss.m_req = *req;
  ss.restoreFragCompletedCount = 0;
  ss.undoDDCompletedCount = 0;
  ss.execREDOLogCompletedCount = 0;
  ss.phaseToSend = 0;

  // seize records for sub-ops
  Uint32 i;
  for (i = 0; i < ss.m_req2cnt; i++) {
    jam();
    Ss_START_RECREQ_2::Req tmp;
    tmp.proxyBlockNo = ss.m_req2[i].m_blockNo;
    Uint32 ssId2 = getSsId(&tmp);
    Ss_START_RECREQ_2 &ss2 = ssSeize<Ss_START_RECREQ_2>(ssId2);
    ss.m_req2[i].m_ssId = ssId2;

    // set wait-for bitmask in SsParallel
    setMask(ss2);
  }

  ndbrequire(signal->getLength() == StartRecReq::SignalLength ||
             signal->getLength() == StartRecReq::SignalLength_v1);
  sendREQ(signal, ss);
}

void DblqhProxy::execLOCAL_RECOVERY_COMP_REP(Signal *signal) {
  jam();
  LocalRecoveryCompleteRep *rep =
      (LocalRecoveryCompleteRep *)&signal->theData[0];

  LocalRecoveryCompleteRep::PhaseIds phaseId =
      (LocalRecoveryCompleteRep::PhaseIds)rep->phaseId;

  Uint32 ssId = rep->senderData;
  Ss_START_RECREQ &ss = ssFind<Ss_START_RECREQ>(ssId);

  switch (phaseId) {
    case LocalRecoveryCompleteRep::RESTORE_FRAG_COMPLETED: {
      jam();
      ss.restoreFragCompletedCount++;
      if (ss.restoreFragCompletedCount < c_workers) {
        jam();
        return;
      }
      break;
    }
    case LocalRecoveryCompleteRep::UNDO_DD_COMPLETED: {
      jam();
      ss.undoDDCompletedCount++;
      if (ss.undoDDCompletedCount < c_workers) {
        jam();
        return;
      }
      break;
    }
    case LocalRecoveryCompleteRep::EXECUTE_REDO_LOG_COMPLETED: {
      jam();
      ss.execREDOLogCompletedCount++;
      if (ss.execREDOLogCompletedCount < c_workers) {
        jam();
        return;
      }
      break;
    }
    default:
      ndbabort();
  }
  /* All LDM workers have completed this phase */
  ndbrequire(Uint32(ss.phaseToSend) == Uint32(phaseId));
  ss.phaseToSend++;
  {
    jam();
    rep->nodeId = getOwnNodeId();
    rep->phaseId = (Uint32)phaseId;
    sendSignal(ss.m_req.senderRef, GSN_LOCAL_RECOVERY_COMP_REP, signal,
               LocalRecoveryCompleteRep::SignalLengthMaster, JBB);
    return;
  }
}

void DblqhProxy::sendSTART_RECREQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *handle) {
  jam();
  Ss_START_RECREQ &ss = ssFind<Ss_START_RECREQ>(ssId);

  StartRecReq *req = (StartRecReq *)signal->getDataPtrSend();
  *req = ss.m_req;

  req->senderRef = reference();
  req->senderData = ssId;
  LinearSectionPtr lsptr[3];
  lsptr[0].p = req->sr_nodes;
  lsptr[0].sz = NdbNodeBitmask::getPackedLengthInWords(req->sr_nodes);
  ndbrequire(import(handle->m_ptr[0], lsptr[0].p, lsptr[0].sz));
  handle->m_cnt = 1;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_START_RECREQ, signal,
                      StartRecReq::SignalLength, JBB, handle);
}

void DblqhProxy::execSTART_RECCONF(Signal *signal) {
  jam();
  const StartRecConf *conf = (const StartRecConf *)signal->getDataPtr();

  if (refToMain(signal->getSendersBlockRef()) != DBLQH) {
    jam();
    execSTART_RECCONF_2(signal);
    return;
  }

  Uint32 ssId = conf->senderData;
  Ss_START_RECREQ &ss = ssFind<Ss_START_RECREQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::sendSTART_RECCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_START_RECREQ &ss = ssFind<Ss_START_RECREQ>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();

    /**
     * There should be no disk-ops in flight here...check it
     */
    signal->theData[0] = DumpStateOrd::LgmanCheckCallbacksClear;
    sendSignal(LGMAN_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);

    ndbrequire(ss.phaseToSend ==
               Uint32(LocalRecoveryCompleteRep::LOCAL_RECOVERY_COMPLETED));
    StartRecConf *conf = (StartRecConf *)signal->getDataPtrSend();
    conf->startingNodeId = getOwnNodeId();
    conf->senderData = ss.m_req.senderData;
    sendSignal(ss.m_req.senderRef, GSN_START_RECCONF, signal,
               StartRecConf::SignalLength, JBB);
  } else {
    ndbabort();
  }

  {
    Uint32 i;
    for (i = 0; i < ss.m_req2cnt; i++) {
      jam();
      Uint32 ssId2 = ss.m_req2[i].m_ssId;
      ssRelease<Ss_START_RECREQ_2>(ssId2);
    }
  }
  ssRelease<Ss_START_RECREQ>(ssId);
}

// GSN_START_RECREQ_2 [ sub-op, fictional gsn ]

void DblqhProxy::execSTART_RECREQ_2(Signal *signal) {
  jam();
  ndbrequire(signal->getLength() == Ss_START_RECREQ_2::Req::SignalLength);

  const Ss_START_RECREQ_2::Req *req =
      (const Ss_START_RECREQ_2::Req *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_START_RECREQ_2 &ss = ssFind<Ss_START_RECREQ_2>(ssId);

  // reversed roles
  recvCONF(signal, ss);
}

void DblqhProxy::sendSTART_RECREQ_2(Signal *signal, Uint32 ssId) {
  jam();
  Ss_START_RECREQ_2 &ss = ssFind<Ss_START_RECREQ_2>(ssId);

  const Ss_START_RECREQ_2::Req *req =
      (const Ss_START_RECREQ_2::Req *)signal->getDataPtr();

  if (firstReply(ss)) {
    jam();
    ss.m_req = *req;
  } else {
    jam();
    /*
     * Fragments can be started from different lcpId's.  LGMAN must run
     * UNDO until lowest lcpId.  Each DBLQH instance computes the lowest
     * lcpId in START_FRAGREQ.  In MT case the proxy further computes
     * the lowest of the lcpId's from worker instances.
     */
    if (req->lcpId < ss.m_req.lcpId) {
      jam();
      ss.m_req.lcpId = req->lcpId;
    }
    ndbrequire(ss.m_req.proxyBlockNo == req->proxyBlockNo);
  }

  if (!lastReply(ss)) {
    jam();
    return;
  }

  {
    Ss_START_RECREQ_2::Req *req =
        (Ss_START_RECREQ_2::Req *)signal->getDataPtrSend();
    *req = ss.m_req;
    BlockReference ref = numberToRef(req->proxyBlockNo, getOwnNodeId());
    sendSignal(ref, GSN_START_RECREQ, signal,
               Ss_START_RECREQ_2::Req::SignalLength, JBB);
  }
}

void DblqhProxy::execSTART_RECCONF_2(Signal *signal) {
  jam();
  ndbrequire(signal->getLength() == Ss_START_RECREQ_2::Conf::SignalLength);

  const Ss_START_RECREQ_2::Conf *conf =
      (const Ss_START_RECREQ_2::Conf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_START_RECREQ_2 &ss = ssFind<Ss_START_RECREQ_2>(ssId);
  ss.m_conf = *conf;

  // reversed roles
  sendREQ(signal, ss);
}

void DblqhProxy::sendSTART_RECCONF_2(Signal *signal, Uint32 ssId,
                                     SectionHandle *handle) {
  jam();
  Ss_START_RECREQ_2 &ss = ssFind<Ss_START_RECREQ_2>(ssId);

  Ss_START_RECREQ_2::Conf *conf =
      (Ss_START_RECREQ_2::Conf *)signal->getDataPtrSend();
  *conf = ss.m_conf;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_START_RECCONF, signal,
                      Ss_START_RECREQ_2::Conf::SignalLength, JBB, handle);
}

// GSN_LQH_TRANSREQ

void DblqhProxy::execLQH_TRANSREQ(Signal *signal) {
  jamEntry();

  if (!checkNodeFailSequence(signal)) {
    jam();
    return;
  }
  const LqhTransReq *req = (const LqhTransReq *)signal->getDataPtr();
  Ss_LQH_TRANSREQ &ss = ssSeize<Ss_LQH_TRANSREQ>();
  ss.m_maxInstanceId = 0;
  ss.m_req = *req;
  if (signal->getLength() < LqhTransReq::SignalLength) {
    /**
     * TC that performs take over doesn't support taking over one
     * TC instance at a time
     */
    jam();
    ss.m_req.instanceId = RNIL;
  }
  ndbrequire(signal->getLength() <= LqhTransReq::SignalLength);
  sendREQ(signal, ss);

  /**
   * See if this is a "resend" (i.e multi TC failure)
   *   and if so, mark "old" record as invalid
   */
  Uint32 nodeId = ss.m_req.failedNodeId;
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(c_ss_LQH_TRANSREQ.m_pool); i++) {
    if (c_ss_LQH_TRANSREQ.m_pool[i].m_ssId != 0 &&
        c_ss_LQH_TRANSREQ.m_pool[i].m_ssId != ss.m_ssId &&
        c_ss_LQH_TRANSREQ.m_pool[i].m_req.failedNodeId == nodeId) {
      jam();
      c_ss_LQH_TRANSREQ.m_pool[i].m_valid = false;
    }
  }
}

void DblqhProxy::sendLQH_TRANSREQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *handle) {
  jam();
  Ss_LQH_TRANSREQ &ss = ssFind<Ss_LQH_TRANSREQ>(ssId);

  LqhTransReq *req = (LqhTransReq *)signal->getDataPtrSend();
  *req = ss.m_req;

  req->senderData = ssId;
  req->senderRef = reference();
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_LQH_TRANSREQ, signal,
                      LqhTransReq::SignalLength, JBB, handle);
}

void DblqhProxy::execLQH_TRANSCONF(Signal *signal) {
  jam();
  const LqhTransConf *conf = (const LqhTransConf *)signal->getDataPtr();
  Uint32 ssId = conf->tcRef;
  Ss_LQH_TRANSREQ &ss = ssFind<Ss_LQH_TRANSREQ>(ssId);
  ss.m_conf = *conf;

  BlockReference ref = signal->getSendersBlockRef();
  ndbrequire(refToMain(ref) == number());
  const Uint32 ino = refToInstance(ref);
  const Uint32 worker = workerIndex(ino);

  ndbrequire(ref == workerRef(worker));
  ndbrequire(worker < c_workers);

  if (ss.m_valid == false) {
    jam();
    /**
     * This is an in-flight signal to an old take-over "session"
     */
    if (ss.m_conf.operationStatus == LqhTransConf::LastTransConf) {
      jam();
      ndbrequire(ss.m_workerMask.get(worker));
      ss.m_workerMask.clear(worker);
      if (ss.m_workerMask.isclear()) {
        jam();
        ssRelease<Ss_LQH_TRANSREQ>(ssId);
      }
    }
    return;
  } else if (ss.m_conf.operationStatus == LqhTransConf::LastTransConf) {
    jam();
    /**
     * When completing(LqhTransConf::LastTransConf) a LQH_TRANSREQ
     *   also check if one can release obsoleteded records
     *
     * This could have been done on each LQH_TRANSCONF, but there is no
     *   urgency, so it's ok todo only on LastTransConf
     */
    Uint32 nodeId = ss.m_req.failedNodeId;
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(c_ss_LQH_TRANSREQ.m_pool); i++) {
      if (c_ss_LQH_TRANSREQ.m_pool[i].m_ssId != 0 &&
          c_ss_LQH_TRANSREQ.m_pool[i].m_ssId != ssId &&
          c_ss_LQH_TRANSREQ.m_pool[i].m_req.failedNodeId == nodeId &&
          c_ss_LQH_TRANSREQ.m_pool[i].m_valid == false) {
        jam();
        if (c_ss_LQH_TRANSREQ.m_pool[i].m_workerMask.get(worker)) {
          jam();
          c_ss_LQH_TRANSREQ.m_pool[i].m_workerMask.clear(worker);
          if (c_ss_LQH_TRANSREQ.m_pool[i].m_workerMask.isclear()) {
            jam();
            ssRelease<Ss_LQH_TRANSREQ>(c_ss_LQH_TRANSREQ.m_pool[i].m_ssId);
          }
        }
      }
    }
  }

  recvCONF(signal, ss);
}

void DblqhProxy::sendLQH_TRANSCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_LQH_TRANSREQ &ss = ssFind<Ss_LQH_TRANSREQ>(ssId);

  if (ss.m_conf.operationStatus == LqhTransConf::LastTransConf) {
    jam();

    /**
     * Maintain running max instance id, based on max seen
     * in this pass, which is only sent in LastTransConf
     * LQH_TRANSREQ from each instance.
     */
    if (ss.m_conf.maxInstanceId > ss.m_maxInstanceId) {
      ndbrequire(ss.m_conf.maxInstanceId < NDBMT_MAX_BLOCK_INSTANCES);
      ss.m_maxInstanceId = ss.m_conf.maxInstanceId;
    }
  } else {
    jam();
    /* Forward conf to the requesting TC, and wait for more */
    LqhTransConf *conf = (LqhTransConf *)signal->getDataPtrSend();
    *conf = ss.m_conf;
    conf->tcRef = ss.m_req.senderData;
    sendSignal(ss.m_req.senderRef, GSN_LQH_TRANSCONF, signal,
               LqhTransConf::SignalLength, JBB);

    // more replies from this worker
    skipConf(ss);
  }

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    LqhTransConf *conf = (LqhTransConf *)signal->getDataPtrSend();
    conf->tcRef = ss.m_req.senderData;
    conf->lqhNodeId = getOwnNodeId();
    conf->operationStatus = LqhTransConf::LastTransConf;
    conf->maxInstanceId = ss.m_maxInstanceId;
    sendSignal(ss.m_req.senderRef, GSN_LQH_TRANSCONF, signal,
               LqhTransConf::SignalLength, JBB);
  } else {
    ndbabort();
  }

  ssRelease<Ss_LQH_TRANSREQ>(ssId);
}

// GSN_EXEC_SR_1 [fictional gsn ]

void DblqhProxy::execEXEC_SRREQ(Signal *signal) {
  jam();
  const BlockReference senderRef = signal->getSendersBlockRef();

  if (refToInstance(senderRef) != 0) {
    jam();
    execEXEC_SR_2(signal, GSN_EXEC_SRREQ);
    return;
  }

  execEXEC_SR_1(signal, GSN_EXEC_SRREQ);
}

void DblqhProxy::execEXEC_SRCONF(Signal *signal) {
  jam();
  const BlockReference senderRef = signal->getSendersBlockRef();

  if (refToInstance(senderRef) != 0) {
    jam();
    execEXEC_SR_2(signal, GSN_EXEC_SRCONF);
    return;
  }

  execEXEC_SR_1(signal, GSN_EXEC_SRCONF);
}

void DblqhProxy::execEXEC_SR_1(Signal *signal, GlobalSignalNumber gsn) {
  jam();
  ndbrequire(signal->getLength() == Ss_EXEC_SR_1::Sig::SignalLength);

  const Ss_EXEC_SR_1::Sig *sig =
      (const Ss_EXEC_SR_1::Sig *)signal->getDataPtr();
  Uint32 ssId = getSsId(sig);
  Ss_EXEC_SR_1 &ss = ssSeize<Ss_EXEC_SR_1>(ssId);
  ss.m_gsn = gsn;
  ss.m_sig = *sig;

  DEB_EXEC_SR(("Send EXEC_SRREQ to %u workers", c_workers));
  sendREQ(signal, ss);
  ssRelease<Ss_EXEC_SR_1>(ss);
}

void DblqhProxy::sendEXEC_SR_1(Signal *signal, Uint32 ssId,
                               SectionHandle *handle) {
  jam();
  Ss_EXEC_SR_1 &ss = ssFind<Ss_EXEC_SR_1>(ssId);
  signal->theData[0] = ss.m_sig.nodeId;
  sendSignalNoRelease(workerRef(ss.m_worker), ss.m_gsn, signal, 1, JBB, handle);
}

// GSN_EXEC_SRREQ_2 [ fictional gsn ]

void DblqhProxy::execEXEC_SR_2(Signal *signal, GlobalSignalNumber gsn) {
  jam();
  ndbrequire(signal->getLength() == Ss_EXEC_SR_2::Sig::SignalLength);

  const Ss_EXEC_SR_2::Sig *sig =
      (const Ss_EXEC_SR_2::Sig *)signal->getDataPtr();
  Uint32 ssId = getSsId(sig);

  bool found = false;
  Ss_EXEC_SR_2 &ss = ssFindSeize<Ss_EXEC_SR_2>(ssId, &found);
  if (!found) {
    jam();
    setMask(ss);
  }

#ifdef DEBUG_EXEC_SR
  BlockReference ref = signal->getSendersBlockRef();
  Uint32 ino = refToInstance(ref);
  DEB_EXEC_SR(
      ("Received EXEC_SRCONF from instance: %u, found: %u", ino, found));
#endif

  ndbrequire(sig->nodeId == getOwnNodeId());
  if (ss.m_sigcount == 0) {
    jam();
    ss.m_gsn = gsn;
    ss.m_sig = *sig;
  } else {
    jam();
    ndbrequire(ss.m_gsn == gsn);
    ndbrequire(memcmp(&ss.m_sig, sig, sizeof(*sig)) == 0);
  }
  ss.m_sigcount++;

  // reversed roles
  recvCONF(signal, ss);
}

void DblqhProxy::sendEXEC_SR_2(Signal *signal, Uint32 ssId) {
  jam();
  Ss_EXEC_SR_2 &ss = ssFind<Ss_EXEC_SR_2>(ssId);

  if (!lastReply(ss)) {
    jam();
    return;
  }

  NodeBitmask nodes;
  nodes.assign(NdbNodeBitmask::Size, ss.m_sig.sr_nodes);
  NodeReceiverGroup rg(DBLQH, nodes);

  signal->theData[0] = ss.m_sig.nodeId;
  sendSignal(rg, ss.m_gsn, signal, 1, JBB);

  ssRelease<Ss_EXEC_SR_2>(ssId);
}

// GSN_EXEC_FRAGREQ
void DblqhProxy::execEXEC_FRAGREQ(Signal *signal) {
  jam();
  Uint32 ref = ((ExecFragReq *)signal->getDataPtr())->dst;

  if (refToNode(ref) == getOwnNodeId()) {
    jam();
    sendSignal(ref, GSN_EXEC_FRAGREQ, signal, signal->getLength(), JBB);
  } else {
    jam();
    sendSignal(numberToRef(DBLQH, refToNode(ref)), GSN_EXEC_FRAGREQ, signal,
               signal->getLength(), JBB);
  }
}

// GSN_EXEC_FRAGCONF
void DblqhProxy::execEXEC_FRAGCONF(Signal *signal) {
  Uint32 ref = signal->theData[1];

  if (refToNode(ref) == getOwnNodeId()) {
    jam();
    sendSignal(ref, GSN_EXEC_FRAGCONF, signal, 1, JBB);
  } else {
    jam();
    sendSignal(numberToRef(DBLQH, refToNode(ref)), GSN_EXEC_FRAGCONF, signal, 2,
               JBB);
  }
}

// GSN_DROP_FRAG_REQ

void DblqhProxy::execDROP_FRAG_REQ(Signal *signal) {
  jam();
  const DropFragReq *req = (const DropFragReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_DROP_FRAG_REQ &ss = ssSeize<Ss_DROP_FRAG_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropFragReq::SignalLength);
  sendREQ(signal, ss);
}

void DblqhProxy::sendDROP_FRAG_REQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *handle) {
  jam();
  Ss_DROP_FRAG_REQ &ss = ssFind<Ss_DROP_FRAG_REQ>(ssId);

  DropFragReq *req = (DropFragReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_DROP_FRAG_REQ, signal,
                      DropFragReq::SignalLength, JBB, handle);
}

void DblqhProxy::execDROP_FRAG_CONF(Signal *signal) {
  jam();
  const DropFragConf *conf = (const DropFragConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_DROP_FRAG_REQ &ss = ssFind<Ss_DROP_FRAG_REQ>(ssId);
  recvCONF(signal, ss);
}

void DblqhProxy::execDROP_FRAG_REF(Signal *signal) {
  jam();
  const DropFragRef *ref = (const DropFragRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_DROP_FRAG_REQ &ss = ssFind<Ss_DROP_FRAG_REQ>(ssId);
  recvREF(signal, ss, ref->errCode);
}

void DblqhProxy::sendDROP_FRAG_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_DROP_FRAG_REQ &ss = ssFind<Ss_DROP_FRAG_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    DropFragConf *conf = (DropFragConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    conf->fragId = ss.m_req.fragId;
    sendSignal(dictRef, GSN_DROP_FRAG_CONF, signal, DropFragConf::SignalLength,
               JBB);
  } else {
    jam();
    DropFragRef *ref = (DropFragRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->fragId = ss.m_req.fragId;
    ref->errCode = ss.m_error;
    sendSignal(dictRef, GSN_DROP_FRAG_REF, signal, DropFragConf::SignalLength,
               JBB);
  }

  ssRelease<Ss_DROP_FRAG_REQ>(ssId);
}

BLOCK_FUNCTIONS(DblqhProxy)
