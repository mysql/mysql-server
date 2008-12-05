/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "DblqhProxy.hpp"
#include "Dblqh.hpp"
#include "DblqhCommon.hpp"

#include <signaldata/StartFragReq.hpp>

DblqhProxy::DblqhProxy(Block_context& ctx) :
  LocalProxy(DBLQH, ctx)
{
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
  addRecSignal(GSN_END_LCP_REQ, &DblqhProxy::execEND_LCP_REQ);
  addRecSignal(GSN_END_LCP_CONF, &DblqhProxy::execEND_LCP_CONF);
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

  // GSN_LQH_TRANSREQ
  addRecSignal(GSN_LQH_TRANSREQ, &DblqhProxy::execLQH_TRANSREQ);
  addRecSignal(GSN_LQH_TRANSCONF, &DblqhProxy::execLQH_TRANSCONF);

  // GSN_EMPTY_LCP_REQ
  addRecSignal(GSN_EMPTY_LCP_REQ, &DblqhProxy::execEMPTY_LCP_REQ);
  addRecSignal(GSN_EMPTY_LCP_CONF, &DblqhProxy::execEMPTY_LCP_CONF);

  // GSN_SUB_GCP_COMPLETE_REP
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &DblqhProxy::execSUB_GCP_COMPLETE_REP);

  // GSN_EXEC_SRREQ
  addRecSignal(GSN_EXEC_SRREQ, &DblqhProxy::execEXEC_SRREQ);
  addRecSignal(GSN_EXEC_SRCONF, &DblqhProxy::execEXEC_SRCONF);
}

DblqhProxy::~DblqhProxy()
{
}

SimulatedBlock*
DblqhProxy::newWorker(Uint32 instanceNo)
{
  return new Dblqh(m_ctx, instanceNo);
}

// GSN_NDB_STTOR

void
DblqhProxy::callNDB_STTOR(Signal* signal)
{
  Ss_READ_NODES_REQ& ss = c_ss_READ_NODESREQ;
  ndbrequire(ss.m_gsn == 0);

  const Uint32 startPhase = signal->theData[2];
  switch (startPhase) {
  case 3:
    ss.m_gsn = GSN_NDB_STTOR;
    sendREAD_NODESREQ(signal);
    break;
  default:
    backNDB_STTOR(signal);
    break;
  }
}

// GSN_CREATE_TAB_REQ

// there is no consistent LQH connect pointer to use as ssId

void
DblqhProxy::execCREATE_TAB_REQ(Signal* signal)
{
  Ss_CREATE_TAB_REQ& ss = ssSeize<Ss_CREATE_TAB_REQ>(1);

  const CreateTabReq* req = (const CreateTabReq*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == CreateTabReq::SignalLengthLDM);

  sendREQ(signal, ss);
}

void
DblqhProxy::sendCREATE_TAB_REQ(Signal* signal, Uint32 ssId)
{
  Ss_CREATE_TAB_REQ& ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);

  CreateTabReq* req = (CreateTabReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_CREATE_TAB_REQ,
             signal, CreateTabReq::SignalLengthLDM, JBB);
}

void
DblqhProxy::execCREATE_TAB_CONF(Signal* signal)
{
  const CreateTabConf* conf = (const CreateTabConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_CREATE_TAB_REQ& ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execCREATE_TAB_REF(Signal* signal)
{
  const CreateTabRef* ref = (const CreateTabRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_CREATE_TAB_REQ& ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendCREATE_TAB_CONF(Signal* signal, Uint32 ssId)
{
  Ss_CREATE_TAB_REQ& ss = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  {
    const CreateTabConf* conf = (const CreateTabConf*)signal->getDataPtr();
    ss.m_lqhConnectPtr[ss.m_worker] = conf->lqhConnectPtr;
  }

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    CreateTabConf* conf = (CreateTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->lqhConnectPtr = ssId;
    sendSignal(dictRef, GSN_CREATE_TAB_CONF,
               signal, CreateTabConf::SignalLength, JBB);
  } else {
    CreateTabRef* ref = (CreateTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->errorStatus = 0;
    sendSignal(dictRef, GSN_CREATE_TAB_REF,
               signal, CreateTabRef::SignalLength, JBB);
    ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  }
}

// GSN_LQHADDATTREQ [ sub-op ]

void
DblqhProxy::execLQHADDATTREQ(Signal* signal)
{
  const LqhAddAttrReq* req = (const LqhAddAttrReq*)signal->getDataPtr();
  Uint32 ssId = req->lqhFragPtr;
  Ss_LQHADDATTREQ& ss = ssSeize<Ss_LQHADDATTREQ>(ssId);

  const Uint32 reqlength =
    LqhAddAttrReq::HeaderLength +
    req->noOfAttributes * LqhAddAttrReq::EntryLength;
  ndbrequire(signal->getLength() == reqlength);
  memcpy(&ss.m_req, req, reqlength << 2);
  ss.m_reqlength = reqlength;

  sendREQ(signal, ss);
}

void
DblqhProxy::sendLQHADDATTREQ(Signal* signal, Uint32 ssId)
{
  Ss_LQHADDATTREQ& ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  Ss_CREATE_TAB_REQ& ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);

  LqhAddAttrReq* req = (LqhAddAttrReq*)signal->getDataPtrSend();
  const Uint32 reqlength = ss.m_reqlength;
  memcpy(req, &ss.m_req, reqlength << 2);
  req->lqhFragPtr = ss_main.m_lqhConnectPtr[ss.m_worker];
  req->noOfAttributes = ss.m_req.noOfAttributes;
  req->senderData = ssId;
  req->senderAttrPtr = ss.m_req.senderAttrPtr;
  sendSignal(workerRef(ss.m_worker), GSN_LQHADDATTREQ,
             signal, reqlength, JBB);
}

void
DblqhProxy::execLQHADDATTCONF(Signal* signal)
{
  const LqhAddAttrConf* conf = (const LqhAddAttrConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_LQHADDATTREQ& ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execLQHADDATTREF(Signal* signal)
{
  const LqhAddAttrRef* ref = (const LqhAddAttrRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_LQHADDATTREQ& ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendLQHADDATTCONF(Signal* signal, Uint32 ssId)
{
  Ss_LQHADDATTREQ& ss = ssFind<Ss_LQHADDATTREQ>(ssId);
  Ss_CREATE_TAB_REQ& ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss_main.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    LqhAddAttrConf* conf = (LqhAddAttrConf*)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->senderAttrPtr = ss.m_req.senderAttrPtr;
    sendSignal(dictRef, GSN_LQHADDATTCONF,
               signal, LqhAddAttrConf::SignalLength, JBB);
  } else {
    jam();
    LqhAddAttrRef* ref = (LqhAddAttrRef*)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_LQHADDATTREF,
               signal, LqhAddAttrRef::SignalLength, JBB);
    ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  }

  ssRelease<Ss_LQHADDATTREQ>(ssId);
}

// GSN_LQHFRAGREQ [ pass-through ]

void
DblqhProxy::execLQHFRAGREQ(Signal* signal)
{
  LqhFragReq* req = (LqhFragReq*)signal->getDataPtrSend();
  Uint32 instance = getInstanceKey(req->tableId, req->fragId);

  // wl4391_todo impl. method that fakes senders block-ref
  sendSignal(numberToRef(DBLQH, instance, getOwnNodeId()),
             GSN_LQHFRAGREQ, signal, signal->getLength(), JBB);
}

// GSN_TAB_COMMITREQ [ sub-op ]

void
DblqhProxy::execTAB_COMMITREQ(Signal* signal)
{
  Ss_TAB_COMMITREQ& ss = ssSeize<Ss_TAB_COMMITREQ>(1); // lost connection

  const TabCommitReq* req = (const TabCommitReq*)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void
DblqhProxy::sendTAB_COMMITREQ(Signal* signal, Uint32 ssId)
{
  Ss_TAB_COMMITREQ& ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  TabCommitReq* req = (TabCommitReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ssId;
  req->tableId = ss.m_req.tableId;
  sendSignal(workerRef(ss.m_worker), GSN_TAB_COMMITREQ,
             signal, TabCommitReq::SignalLength, JBB);
}

void
DblqhProxy::execTAB_COMMITCONF(Signal* signal)
{
  const TabCommitConf* conf = (TabCommitConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_TAB_COMMITREQ& ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execTAB_COMMITREF(Signal* signal)
{
  const TabCommitRef* ref = (TabCommitRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_TAB_COMMITREQ& ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  // wl4391_todo omit extra info now since DBDICT only does ndbrequire
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendTAB_COMMITCONF(Signal* signal, Uint32 ssId)
{
  Ss_TAB_COMMITREQ& ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  Ss_CREATE_TAB_REQ& ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss_main.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    TabCommitConf* conf = (TabCommitConf*)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->nodeId = getOwnNodeId();
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITCONF,
               signal, TabCommitConf::SignalLength, JBB);
  } else {
    jam();
    TabCommitRef* ref = (TabCommitRef*)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->nodeId = getOwnNodeId();
    ref->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITREF,
               signal, TabCommitRef::SignalLength, JBB);
    return;
  }

  ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  ssRelease<Ss_TAB_COMMITREQ>(ssId);
}

// GSN_LCP_FRAG_ORD

void
DblqhProxy::execLCP_FRAG_ORD(Signal* signal)
{
  ndbrequire(signal->getLength() == LcpFragOrd::SignalLength);

  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();
  const LcpFragOrd req_copy = *req;
  Uint32 ssId = getSsId(req);

  bool found = false;
  Ss_LCP_FRAG_ORD& ss = ssFindSeize<Ss_LCP_FRAG_ORD>(ssId, &found);

  if (!found) {
    jam();
    D("LCP: start" << V(req->lcpId));
    ndbrequire(c_lcpRecord.m_idle);
    c_lcpRecord.m_idle = false;
    c_lcpRecord.m_lcpId = req->lcpId;
    c_lcpRecord.m_frags = 0;

    // handle start of LCP in PGMAN and TSMAN
    LcpFragOrd* req = (LcpFragOrd*)signal->getDataPtrSend();
    *req = req_copy;
    EXECUTE_DIRECT(PGMAN, GSN_LCP_FRAG_ORD,
                   signal, LcpFragOrd::SignalLength);
    *req = req_copy;
    EXECUTE_DIRECT(TSMAN, GSN_LCP_FRAG_ORD,
                   signal, LcpFragOrd::SignalLength);
  } else {
    jam();
    D("LCP: continue" << V(req->lcpId) << V(c_lcpRecord.m_frags));
    ndbrequire(!c_lcpRecord.m_idle);
    ndbrequire(c_lcpRecord.m_lcpId == req->lcpId);

    if (c_lcpRecord.m_frags == 0) {
      // first frag has not replied
      jam();
      D("LCP: delay" << V(req->lcpId) << V(c_lcpRecord.m_frags));
      LcpFragOrd* req = (LcpFragOrd*)signal->getDataPtrSend();
      *req = req_copy;
      sendSignalWithDelay(reference(), GSN_LCP_FRAG_ORD,
                          signal, 100, LcpFragOrd::SignalLength);
      return;
    }
  }

  if (req->lastFragmentFlag) {
    jam();
    D("LCP: complete" << V(req->lcpId));
    execLCP_COMPLETE_ORD(signal);
    return;
  }
  sendREQ(signal, ss);
}

void
DblqhProxy::sendLCP_FRAG_ORD(Signal* signal, Uint32 ssId)
{
  Ss_LCP_FRAG_ORD& ss = ssFind<Ss_LCP_FRAG_ORD>(ssId);
  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();

  NdbLogPartInfo lpinfo(workerInstance(ss.m_worker));
  if (!lpinfo.partNoOwner(req->tableId, req->fragmentId)) {
    jam();
    skipReq(ss);
    return;
  }

  if (!ss.m_active.get(ss.m_worker)) {
    jam();
    D("LCP: active" << V(ss.m_worker));
    ss.m_active.set(ss.m_worker);
  }

  sendSignal(workerRef(ss.m_worker), GSN_LCP_FRAG_ORD,
             signal, LcpFragOrd::SignalLength, JBB);
}

// increment frag count and pass through
void
DblqhProxy::execLCP_FRAG_REP(Signal* signal)
{
  ndbrequire(signal->getLength() == LcpFragRep::SignalLength);

  const LcpFragRep* conf = (const LcpFragRep*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_LCP_FRAG_ORD& ss = ssFind<Ss_LCP_FRAG_ORD>(ssId);

  ndbrequire(!c_lcpRecord.m_idle);
  ndbrequire(c_lcpRecord.m_lcpId == conf->lcpId);

  c_lcpRecord.m_frags++;
  D("LCP: rep" << V(conf->lcpId) << V(c_lcpRecord.m_frags));

  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    if (nodePtr.p->m_alive) {
      jam();
      Uint32 nodeId = nodePtr.p->m_nodeId;
      BlockReference dihRef = calcDihBlockRef(nodeId);
      sendSignal(dihRef, GSN_LCP_FRAG_REP,
                 signal, LcpFragRep::SignalLength, JBB);
    }
    c_nodeList.next(nodePtr);
  }
}

// GSN_LCP_COMPLETE_ORD [ sub-op, fictional gsn ]

void
DblqhProxy::execLCP_COMPLETE_ORD(Signal* signal)
{
  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_LCP_COMPLETE_ORD& ss = ssSeize<Ss_LCP_COMPLETE_ORD>(ssId);
  ss.m_req = *req;

  Ss_LCP_FRAG_ORD& ssLcp = ssFind<Ss_LCP_FRAG_ORD>(ssId);
  const Uint32 activeCount = ssLcp.m_active.count();
  D("LCP: complete" << V(activeCount));
  // database with no fragments is not handled
  ndbrequire(activeCount != 0);

  // seize END_LCP_REQ records
  Uint32 i;
  for (i = 0; i < ss.BlockCnt; i++) {
    EndLcpReq tmp;
    tmp.backupId = ss.m_req.lcpId;
    tmp.proxyBlockNo = ss.m_endLcp[i].m_blockNo;
    Uint32 ssIdEnd = getSsId(&tmp);
    Ss_END_LCP_REQ& ssEnd = ssSeize<Ss_END_LCP_REQ>(ssIdEnd);
    ss.m_endLcp[i].m_ssId = ssIdEnd;
    ssEnd.m_ssIdLcp = ssId;

    // set wait-for bitmask
    setMask(ssEnd, ssLcp.m_active);
  }

  sendREQ(signal, ss);
}

void
DblqhProxy::sendLCP_COMPLETE_ORD(Signal* signal, Uint32 ssId)
{
  Ss_LCP_COMPLETE_ORD& ss = ssFind<Ss_LCP_COMPLETE_ORD>(ssId);

  LcpFragOrd* req = (LcpFragOrd*)signal->getDataPtrSend();
  *req = ss.m_req;
  sendSignal(workerRef(ss.m_worker), GSN_LCP_FRAG_ORD,
             signal, LcpFragOrd::SignalLength, JBB);
}

void
DblqhProxy::execLCP_COMPLETE_REP(Signal* signal)
{
  const LcpCompleteRep* conf = (const LcpCompleteRep*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_LCP_COMPLETE_ORD& ss = ssFind<Ss_LCP_COMPLETE_ORD>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::sendLCP_COMPLETE_REP(Signal* signal, Uint32 ssId)
{
  Ss_LCP_COMPLETE_ORD& ss = ssFind<Ss_LCP_COMPLETE_ORD>(ssId);
  Uint32 i;

  if (!lastReply(ss))
    return;

  // verify the protocol
  for (i = 0; i < ss.BlockCnt; i++) {
    Uint32 ssIdEnd = ss.m_endLcp[i].m_ssId;
    Ss_END_LCP_REQ& ssEnd = ssFind<Ss_END_LCP_REQ>(ssIdEnd);
    const Uint32 rb = ssEnd.m_proxyBlockNo;
    switch (rb) {
    case PGMAN:
      ndbrequire(ssEnd.m_confcount == 1);
      break;
    case TSMAN:
      ndbrequire(ssEnd.m_confcount == 0);
      break;
    case LGMAN:
      ndbrequire(ssEnd.m_confcount == 1);
      break;
    default:
      ndbrequire(false);
      break;
    }
  }

  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    if (nodePtr.p->m_alive) {
      jam();
      Uint32 nodeId = nodePtr.p->m_nodeId;
      BlockReference dihRef = calcDihBlockRef(nodeId);

      LcpCompleteRep* conf = (LcpCompleteRep*)signal->getDataPtrSend();
      conf->nodeId = getOwnNodeId();
      conf->blockNo = DBLQH;
      conf->lcpId = ss.m_req.lcpId;
      sendSignal(dihRef, GSN_LCP_COMPLETE_REP,
                 signal, LcpCompleteRep::SignalLength, JBB);
    }
    c_nodeList.next(nodePtr);
  }

  for (i = 0; i < ss.BlockCnt; i++) {
    jam();
    Uint32 ssIdEnd = ss.m_endLcp[i].m_ssId;
    ssRelease<Ss_END_LCP_REQ>(ssIdEnd);
  }
  ssRelease<Ss_LCP_COMPLETE_ORD>(ssId);
  ssRelease<Ss_LCP_FRAG_ORD>(ssId);
  c_lcpRecord.m_idle = true;
}

// GSN_END_LCP_REQ [ sub-op ]

void
DblqhProxy::execEND_LCP_REQ(Signal* signal)
{
  ndbrequire(refToMain(signal->getSendersBlockRef()) == DBLQH);

  const EndLcpReq* req = (const EndLcpReq*)signal->getDataPtr();
  Uint32 rb = req->proxyBlockNo;
  ndbrequire(rb == PGMAN || rb == TSMAN || rb == LGMAN);

  Uint32 ssId = getSsId(req);
  Ss_END_LCP_REQ& ss = ssFind<Ss_END_LCP_REQ>(ssId);

  if (++ss.m_reqcount == 1) {
    ss.m_backupId = req->backupId;
    ss.m_proxyBlockNo = req->proxyBlockNo;
  } else {
    ndbrequire(ss.m_backupId == req->backupId);
    ndbrequire(ss.m_proxyBlockNo == req->proxyBlockNo);
  }

  // reversed roles
  recvCONF(signal, ss);
}

void
DblqhProxy::sendEND_LCP_REQ(Signal* signal, Uint32 ssId)
{
  Ss_END_LCP_REQ& ss = ssFind<Ss_END_LCP_REQ>(ssId);

  const EndLcpReq* req = (const EndLcpReq*)signal->getDataPtr();
  ss.m_req[ss.m_worker] = *req;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  {
    const Uint32 rb = ss.m_proxyBlockNo;
    EndLcpReq* req = (EndLcpReq*)signal->getDataPtrSend();
    req->senderData = ssId;
    req->senderRef = reference();
    req->backupPtr = 0;
    req->backupId = ss.m_backupId;
    EXECUTE_DIRECT(rb, GSN_END_LCP_REQ,
                   signal, EndLcpReq::SignalLength, 0);
  }
}

void
DblqhProxy::execEND_LCP_CONF(Signal* signal)
{
  const EndLcpConf* req = (const EndLcpConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_END_LCP_REQ& ss = ssFind<Ss_END_LCP_REQ>(ssId);
  ndbrequire(ss.m_confcount == 0);
  ss.m_confcount++;

  const Uint32 sb = refToMain(req->senderRef);
  ndbrequire(sb == PGMAN || sb == LGMAN);

  // reversed roles
  sendREQ(signal, ss);
}

void
DblqhProxy::sendEND_LCP_CONF(Signal* signal, Uint32 ssId)
{
  Ss_END_LCP_REQ& ss = ssFind<Ss_END_LCP_REQ>(ssId);
  Ss_LCP_FRAG_ORD& ssLcp = ssFind<Ss_LCP_FRAG_ORD>(ss.m_ssIdLcp);

  // workers handling no fragments sent no REQ and get no CONF
  if (!ssLcp.m_active.get(ss.m_worker)) {
    jam();
    return;
  }
  
  EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
  conf->senderData = ss.m_req[ss.m_worker].senderData;
  conf->senderRef = reference();
  sendSignal(workerRef(ss.m_worker), GSN_END_LCP_CONF,
             signal, EndLcpConf::SignalLength, JBB);
}

// GSN_GCP_SAVEREQ

void
DblqhProxy::execGCP_SAVEREQ(Signal* signal)
{
  const GCPSaveReq* req = (const GCPSaveReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_GCP_SAVEREQ& ss = ssSeize<Ss_GCP_SAVEREQ>(ssId);
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void
DblqhProxy::sendGCP_SAVEREQ(Signal* signal, Uint32 ssId)
{
  Ss_GCP_SAVEREQ& ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  GCPSaveReq* req = (GCPSaveReq*)signal->getDataPtrSend();
  *req = ss.m_req;

  req->dihBlockRef = reference();
  req->dihPtr = ss.m_worker;
  sendSignal(workerRef(ss.m_worker), GSN_GCP_SAVEREQ,
             signal, GCPSaveReq::SignalLength, JBB);
}

void
DblqhProxy::execGCP_SAVECONF(Signal* signal)
{
  const GCPSaveConf* conf = (const GCPSaveConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_GCP_SAVEREQ& ss = ssFind<Ss_GCP_SAVEREQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execGCP_SAVEREF(Signal* signal)
{
  const GCPSaveRef* ref = (const GCPSaveRef*)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_GCP_SAVEREQ& ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  if (ss.m_error != 0) {
    // wl4391_todo check
    ndbrequire(ss.m_error == ref->errorCode);
  }
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendGCP_SAVECONF(Signal* signal, Uint32 ssId)
{
  Ss_GCP_SAVEREQ& ss = ssFind<Ss_GCP_SAVEREQ>(ssId);

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    GCPSaveConf* conf = (GCPSaveConf*)signal->getDataPtrSend();
    conf->dihPtr = ss.m_req.dihPtr;
    conf->nodeId = getOwnNodeId();
    conf->gci = ss.m_req.gci;
    sendSignal(ss.m_req.dihBlockRef, GSN_GCP_SAVECONF,
               signal, GCPSaveConf::SignalLength, JBB);
  } else {
    jam();
    GCPSaveRef* ref = (GCPSaveRef*)signal->getDataPtrSend();
    ref->dihPtr = ss.m_req.dihPtr;
    ref->nodeId = getOwnNodeId();
    ref->gci = ss.m_req.gci;
    ref->errorCode = ss.m_error;
    sendSignal(ss.m_req.dihBlockRef, GSN_GCP_SAVEREF,
               signal, GCPSaveRef::SignalLength, JBB);
  }

  ssRelease<Ss_GCP_SAVEREQ>(ssId);
}

// GSN_SUB_GCP_COMPLETE_REP
void
DblqhProxy::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  for (Uint32 i = 0; i<c_workers; i++)
  {
    jam();
    sendSignal(workerRef(i), GSN_SUB_GCP_COMPLETE_REP, signal,
               signal->getLength(), JBB);
  }
}

// GSN_PREP_DROP_TAB_REQ

void
DblqhProxy::execPREP_DROP_TAB_REQ(Signal* signal)
{
  const PrepDropTabReq* req = (const PrepDropTabReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_PREP_DROP_TAB_REQ& ss = ssSeize<Ss_PREP_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == PrepDropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void
DblqhProxy::sendPREP_DROP_TAB_REQ(Signal* signal, Uint32 ssId)
{
  Ss_PREP_DROP_TAB_REQ& ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);

  PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId; // redundant since tableId is used
  sendSignal(workerRef(ss.m_worker), GSN_PREP_DROP_TAB_REQ,
             signal, PrepDropTabReq::SignalLength, JBB);
}

void
DblqhProxy::execPREP_DROP_TAB_CONF(Signal* signal)
{
  const PrepDropTabConf* conf = (const PrepDropTabConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_PREP_DROP_TAB_REQ& ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execPREP_DROP_TAB_REF(Signal* signal)
{
  const PrepDropTabRef* ref = (const PrepDropTabRef*)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_PREP_DROP_TAB_REQ& ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendPREP_DROP_TAB_CONF(Signal* signal, Uint32 ssId)
{
  Ss_PREP_DROP_TAB_REQ& ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_CONF,
               signal, PrepDropTabConf::SignalLength, JBB);
  } else {
    jam();
    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_REF,
               signal, PrepDropTabRef::SignalLength, JBB);
  }

  ssRelease<Ss_PREP_DROP_TAB_REQ>(ssId);
}

// GSN_DROP_TAB_REQ

void
DblqhProxy::execDROP_TAB_REQ(Signal* signal)
{
  const DropTabReq* req = (const DropTabReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_DROP_TAB_REQ& ss = ssSeize<Ss_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void
DblqhProxy::sendDROP_TAB_REQ(Signal* signal, Uint32 ssId)
{
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);

  DropTabReq* req = (DropTabReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId; // redundant since tableId is used
  sendSignal(workerRef(ss.m_worker), GSN_DROP_TAB_REQ,
             signal, DropTabReq::SignalLength, JBB);
}

void
DblqhProxy::execDROP_TAB_CONF(Signal* signal)
{
  const DropTabConf* conf = (const DropTabConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execDROP_TAB_REF(Signal* signal)
{
  const DropTabRef* ref = (const DropTabRef*)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendDROP_TAB_CONF(Signal* signal, Uint32 ssId)
{
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    DropTabConf* conf = (DropTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_DROP_TAB_CONF,
               signal, DropTabConf::SignalLength, JBB);
  } else {
    jam();
    DropTabRef* ref = (DropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_DROP_TAB_REF,
               signal, DropTabConf::SignalLength, JBB);
  }

  ssRelease<Ss_DROP_TAB_REQ>(ssId);
}

// GSN_ALTER_TAB_REQ

void
DblqhProxy::execALTER_TAB_REQ(Signal* signal)
{
  const AlterTabReq* req = (const AlterTabReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_ALTER_TAB_REQ& ss = ssSeize<Ss_ALTER_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == AlterTabReq::SignalLength);

  {
    SectionHandle handle(this, signal);
    ss.m_sections = handle.m_cnt;
    ndbrequire(ss.m_sections <= 1);
    if (ss.m_sections >= 1) {
      ss.m_sz0 = handle.m_ptr[0].p->m_sz;
      ndbrequire(ss.m_sz0 <= ss.MaxSection0);
      ::copy(ss.m_section0, handle.m_ptr[0]);
    }
    releaseSections(handle);
  }

  sendREQ(signal, ss);
}

void
DblqhProxy::sendALTER_TAB_REQ(Signal* signal, Uint32 ssId)
{
  Ss_ALTER_TAB_REQ& ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  if (ss.m_sections == 0) {
    jam();
    sendSignal(workerRef(ss.m_worker), GSN_ALTER_TAB_REQ,
               signal, AlterTabReq::SignalLength, JBB);
  } else {
    jam();
    LinearSectionPtr ptr[3];
    ptr[0].sz = ss.m_sz0;
    ptr[0].p = ss.m_section0;
    sendSignal(workerRef(ss.m_worker), GSN_ALTER_TAB_REQ,
               signal, AlterTabReq::SignalLength, JBB, ptr, 1);
  }
}

void
DblqhProxy::execALTER_TAB_CONF(Signal* signal)
{
  const AlterTabConf* conf = (const AlterTabConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_ALTER_TAB_REQ& ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execALTER_TAB_REF(Signal* signal)
{
  const AlterTabRef* ref = (const AlterTabRef*)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_ALTER_TAB_REQ& ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendALTER_TAB_CONF(Signal* signal, Uint32 ssId)
{
  Ss_ALTER_TAB_REQ& ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_ALTER_TAB_CONF,
               signal, AlterTabConf::SignalLength, JBB);
  } else {
    jam();
    AlterTabRef* ref = (AlterTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_ALTER_TAB_REF,
               signal, AlterTabConf::SignalLength, JBB);
  }

  ssRelease<Ss_ALTER_TAB_REQ>(ssId);
}

// GSN_START_FRAGREQ

void
DblqhProxy::execSTART_FRAGREQ(Signal* signal)
{
  StartFragReq* req = (StartFragReq*)signal->getDataPtrSend();
  Uint32 instance = getInstanceKey(req->tableId, req->fragId);

  // wl4391_todo impl. method that fakes senders block-ref
  sendSignal(numberToRef(DBLQH, instance, getOwnNodeId()),
             GSN_START_FRAGREQ, signal, signal->getLength(), JBB);
}

// GSN_START_RECREQ

void
DblqhProxy::execSTART_RECREQ(Signal* signal)
{
  if (refToMain(signal->getSendersBlockRef()) == DBLQH) {
    jam();
    execSTART_RECREQ_2(signal);
    return;
  }

  const StartRecReq* req = (const StartRecReq*)signal->getDataPtr();
  Ss_START_RECREQ& ss = ssSeize<Ss_START_RECREQ>();
  ss.m_req = *req;

  // seize records for sub-ops
  Uint32 i;
  for (i = 0; i < ss.m_req2cnt; i++) {
    Ss_START_RECREQ_2::Req tmp;
    tmp.proxyBlockNo = ss.m_req2[i].m_blockNo;
    Uint32 ssId2 = getSsId(&tmp);
    Ss_START_RECREQ_2& ss2 = ssSeize<Ss_START_RECREQ_2>(ssId2);
    ss.m_req2[i].m_ssId = ssId2;

    // set wait-for bitmask in SsParallel
    setMask(ss2);
  }

  ndbrequire(signal->getLength() == StartRecReq::SignalLength);
  sendREQ(signal, ss);
}

void
DblqhProxy::sendSTART_RECREQ(Signal* signal, Uint32 ssId)
{
  Ss_START_RECREQ& ss = ssFind<Ss_START_RECREQ>(ssId);

  StartRecReq* req = (StartRecReq*)signal->getDataPtrSend();
  *req = ss.m_req;

  req->senderRef = reference();
  req->senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_START_RECREQ,
             signal, StartRecReq::SignalLength, JBB);
}

void
DblqhProxy::execSTART_RECCONF(Signal* signal)
{
  const StartRecConf* conf = (const StartRecConf*)signal->getDataPtr();

  if (refToMain(signal->getSendersBlockRef()) != DBLQH) {
    jam();
    execSTART_RECCONF_2(signal);
    return;
  }

  Uint32 ssId = conf->senderData;
  Ss_START_RECREQ& ss = ssFind<Ss_START_RECREQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::sendSTART_RECCONF(Signal* signal, Uint32 ssId)
{
  Ss_START_RECREQ& ss = ssFind<Ss_START_RECREQ>(ssId);

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    StartRecConf* conf = (StartRecConf*)signal->getDataPtrSend();
    conf->startingNodeId = getOwnNodeId();
    conf->senderData = ss.m_req.senderData;
    sendSignal(ss.m_req.senderRef, GSN_START_RECCONF,
               signal, StartRecConf::SignalLength, JBB);
  } else {
    ndbrequire(false);
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

void
DblqhProxy::execSTART_RECREQ_2(Signal* signal)
{
  ndbrequire(signal->getLength() == Ss_START_RECREQ_2::Req::SignalLength);

  const Ss_START_RECREQ_2::Req* req =
    (const Ss_START_RECREQ_2::Req*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_START_RECREQ_2& ss = ssFind<Ss_START_RECREQ_2>(ssId);

  // reversed roles
  recvCONF(signal, ss);
}

void
DblqhProxy::sendSTART_RECREQ_2(Signal* signal, Uint32 ssId)
{
  Ss_START_RECREQ_2& ss = ssFind<Ss_START_RECREQ_2>(ssId);

  const Ss_START_RECREQ_2::Req* req =
    (const Ss_START_RECREQ_2::Req*)signal->getDataPtr();

  if (firstReply(ss)) {
    ss.m_req = *req;
  } else {
    // ndbrequire(ss.m_req.lcpId == req->lcpId); wl4391_todo
    ndbrequire(ss.m_req.proxyBlockNo == req->proxyBlockNo);
  }

  if (!lastReply(ss))
    return;

  {
    Ss_START_RECREQ_2::Req* req =
      (Ss_START_RECREQ_2::Req*)signal->getDataPtrSend();
    *req = ss.m_req;
    BlockReference ref = numberToRef(req->proxyBlockNo, getOwnNodeId());
    sendSignal(ref, GSN_START_RECREQ,
               signal, Ss_START_RECREQ_2::Req::SignalLength, JBB);
  }
}

void
DblqhProxy::execSTART_RECCONF_2(Signal* signal)
{
  ndbrequire(signal->getLength() == Ss_START_RECREQ_2::Conf::SignalLength);

  const Ss_START_RECREQ_2::Conf* conf =
    (const Ss_START_RECREQ_2::Conf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_START_RECREQ_2& ss = ssFind<Ss_START_RECREQ_2>(ssId);
  ss.m_conf = *conf;

  // reversed roles
  sendREQ(signal, ss);
}

void
DblqhProxy::sendSTART_RECCONF_2(Signal* signal, Uint32 ssId)
{
  Ss_START_RECREQ_2& ss = ssFind<Ss_START_RECREQ_2>(ssId);

  Ss_START_RECREQ_2::Conf* conf =
    (Ss_START_RECREQ_2::Conf*)signal->getDataPtrSend();
  *conf = ss.m_conf;
  sendSignal(workerRef(ss.m_worker), GSN_START_RECCONF,
             signal, Ss_START_RECREQ_2::Conf::SignalLength, JBB);
}

// GSN_LQH_TRANSREQ

void
DblqhProxy::execLQH_TRANSREQ(Signal* signal)
{
  const LqhTransReq* req = (const LqhTransReq*)signal->getDataPtr();
  Ss_LQH_TRANSREQ& ss = ssSeize<Ss_LQH_TRANSREQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == LqhTransReq::SignalLength);
  sendREQ(signal, ss);
}

void
DblqhProxy::sendLQH_TRANSREQ(Signal* signal, Uint32 ssId)
{
  Ss_LQH_TRANSREQ& ss = ssFind<Ss_LQH_TRANSREQ>(ssId);

  LqhTransReq* req = (LqhTransReq*)signal->getDataPtrSend();
  *req = ss.m_req;

  req->senderData = ssId;
  req->senderRef = reference();
  sendSignal(workerRef(ss.m_worker), GSN_LQH_TRANSREQ,
             signal, LqhTransReq::SignalLength, JBB);
}

void
DblqhProxy::execLQH_TRANSCONF(Signal* signal)
{
  const LqhTransConf* conf = (const LqhTransConf*)signal->getDataPtr();
  Uint32 ssId = conf->tcRef;
  Ss_LQH_TRANSREQ& ss = ssFind<Ss_LQH_TRANSREQ>(ssId);
  ss.m_conf = *conf;
  recvCONF(signal, ss);
}

void
DblqhProxy::sendLQH_TRANSCONF(Signal* signal, Uint32 ssId)
{
  Ss_LQH_TRANSREQ& ss = ssFind<Ss_LQH_TRANSREQ>(ssId);

  if (ss.m_conf.operationStatus != LqhTransConf::LastTransConf) {
    jam();
    LqhTransConf* conf = (LqhTransConf*)signal->getDataPtrSend();
    *conf = ss.m_conf;
    conf->tcRef = ss.m_req.senderData;
    sendSignal(ss.m_req.senderRef, GSN_LQH_TRANSCONF,
               signal, LqhTransConf::SignalLength, JBB);

    // more replies from this worker
    skipConf(ss);
  }

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    LqhTransConf* conf = (LqhTransConf*)signal->getDataPtrSend();
    conf->tcRef = ss.m_req.senderData;
    conf->lqhNodeId = getOwnNodeId();
    conf->operationStatus = LqhTransConf::LastTransConf;
    sendSignal(ss.m_req.senderRef, GSN_LQH_TRANSCONF,
               signal, LqhTransConf::SignalLength, JBB);
  } else {
    ndbrequire(false);
  }

  ssRelease<Ss_LQH_TRANSREQ>(ssId);
}

// GSN_EMPTY_LCP_REQ

void
DblqhProxy::execEMPTY_LCP_REQ(Signal* signal)
{
  const EmptyLcpReq* req = (const EmptyLcpReq*)signal->getDataPtr();
  Ss_EMPTY_LCP_REQ& ss = ssSeize<Ss_EMPTY_LCP_REQ>(1);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == EmptyLcpReq::SignalLength);
  sendREQ(signal, ss);
}

void
DblqhProxy::sendEMPTY_LCP_REQ(Signal* signal, Uint32 ssId)
{
  Ss_EMPTY_LCP_REQ& ss = ssFind<Ss_EMPTY_LCP_REQ>(ssId);

  EmptyLcpReq* req = (EmptyLcpReq*)signal->getDataPtrSend();
  *req = ss.m_req;

  req->senderRef = reference();
  sendSignal(workerRef(ss.m_worker), GSN_EMPTY_LCP_REQ,
             signal, EmptyLcpReq::SignalLength, JBB);
}

void
DblqhProxy::execEMPTY_LCP_CONF(Signal* signal)
{
  Ss_EMPTY_LCP_REQ& ss = ssFind<Ss_EMPTY_LCP_REQ>(1);
  recvCONF(signal, ss);
}

void
DblqhProxy::sendEMPTY_LCP_CONF(Signal* signal, Uint32 ssId)
{
  Ss_EMPTY_LCP_REQ& ss = ssFind<Ss_EMPTY_LCP_REQ>(ssId);
  const EmptyLcpConf* conf = (const EmptyLcpConf*)signal->getDataPtr();

  if (firstReply(ss)) {
    jam();
    ss.m_conf = *conf;
  } else if (ss.m_conf.idle && conf->idle) {
    jam();
    ndbrequire(ss.m_conf.lcpId == conf->lcpId);
  } else if (ss.m_conf.idle && !conf->idle) {
    jam();
    ndbrequire(ss.m_conf.lcpId == conf->lcpId);
    ss.m_conf = *conf;
  } else if (!ss.m_conf.idle && conf->idle) {
    jam();
    ndbrequire(ss.m_conf.lcpId == conf->lcpId);
  } else if (!ss.m_conf.idle && !conf->idle) {
    jam();
    if (ss.m_conf.tableId < conf->tableId ||
        (ss.m_conf.tableId == conf->tableId &&
         ss.m_conf.fragmentId < conf->fragmentId)) {
      jam();
      ss.m_conf.tableId = conf->tableId;
      ss.m_conf.fragmentId = conf->fragmentId;
      ndbrequire(ss.m_conf.lcpNo == conf->lcpNo);
      ndbrequire(ss.m_conf.lcpId == conf->lcpId);
    }
  } else {
    ndbassert(false);
  }

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    EmptyLcpConf* conf = (EmptyLcpConf*)signal->getDataPtrSend();
    *conf = ss.m_conf;
    sendSignal(ss.m_req.senderRef, GSN_EMPTY_LCP_CONF,
               signal, EmptyLcpConf::SignalLength, JBB);
  } else {
    ndbrequire(false);
  }

  ssRelease<Ss_EMPTY_LCP_REQ>(ssId);
}

// GSN_EXEC_SR_1 [fictional gsn ]

void
DblqhProxy::execEXEC_SRREQ(Signal* signal)
{
  const BlockReference senderRef = signal->getSendersBlockRef();

  if (refToInstance(senderRef) != 0) {
    jam();
    execEXEC_SR_2(signal, GSN_EXEC_SRREQ);
    return;
  }

  execEXEC_SR_1(signal, GSN_EXEC_SRREQ);
}

void
DblqhProxy::execEXEC_SRCONF(Signal* signal)
{
  const BlockReference senderRef = signal->getSendersBlockRef();

  if (refToInstance(senderRef) != 0) {
    jam();
    execEXEC_SR_2(signal, GSN_EXEC_SRCONF);
    return;
  }

  execEXEC_SR_1(signal, GSN_EXEC_SRCONF);
}

void
DblqhProxy::execEXEC_SR_1(Signal* signal, GlobalSignalNumber gsn)
{
  ndbrequire(signal->getLength() == Ss_EXEC_SR_1::Sig::SignalLength);

  const Ss_EXEC_SR_1::Sig* sig =
    (const Ss_EXEC_SR_1::Sig*)signal->getDataPtr();
  Uint32 ssId = getSsId(sig);
  Ss_EXEC_SR_1& ss = ssSeize<Ss_EXEC_SR_1>(ssId);
  ss.m_gsn = gsn;
  ss.m_sig = *sig;

  sendREQ(signal, ss);
  ssRelease<Ss_EXEC_SR_1>(ss);
}

void
DblqhProxy::sendEXEC_SR_1(Signal* signal, Uint32 ssId)
{
  Ss_EXEC_SR_1& ss = ssFind<Ss_EXEC_SR_1>(ssId);
  signal->theData[0] = ss.m_sig.nodeId;
  sendSignal(workerRef(ss.m_worker), ss.m_gsn, signal, 1, JBB);
}

// GSN_EXEC_SRREQ_2 [ fictional gsn ]

void
DblqhProxy::execEXEC_SR_2(Signal* signal, GlobalSignalNumber gsn)
{
  ndbrequire(signal->getLength() == Ss_EXEC_SR_2::Sig::SignalLength);

  const Ss_EXEC_SR_2::Sig* sig =
    (const Ss_EXEC_SR_2::Sig*)signal->getDataPtr();
  Uint32 ssId = getSsId(sig);

  bool found = false;
  Ss_EXEC_SR_2& ss = ssFindSeize<Ss_EXEC_SR_2>(ssId, &found);
  if (!found) {
    jam();
    setMask(ss);
  }

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

void
DblqhProxy::sendEXEC_SR_2(Signal* signal, Uint32 ssId)
{
  Ss_EXEC_SR_2& ss = ssFind<Ss_EXEC_SR_2>(ssId);

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

BLOCK_FUNCTIONS(DblqhProxy)
