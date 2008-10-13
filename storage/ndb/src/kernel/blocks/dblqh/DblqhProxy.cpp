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

// GSN_TAB_COMMITREQ

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
  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();
  ndbrequire(req->lastFragmentFlag);
  execLCP_COMPLETE_ORD(signal);
}

// GSN_LCP_COMPLETE_ORD [ fictional gsn ]

void
DblqhProxy::execLCP_COMPLETE_ORD(Signal* signal)
{
  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_LCP_COMPLETE_ORD& ss = ssSeize<Ss_LCP_COMPLETE_ORD>(ssId);
  ss.m_req = *req;
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

  if (!lastReply(ss))
    return;

  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    if (nodePtr.p->m_alive) {
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

  ssRelease<Ss_LCP_COMPLETE_ORD>(ssId);
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

// GSN_START_RECREQ

void
DblqhProxy::execSTART_RECREQ(Signal* signal)
{
  const StartRecReq* req = (const StartRecReq*)signal->getDataPtr();
  Ss_START_RECREQ& ss = ssSeize<Ss_START_RECREQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == StartRecReq::SignalLength);
  sendREQ(signal, ss);
}

// GSN_START_RECREQ

void
DblqhProxy::execSTART_FRAGREQ(Signal* signal)
{
  StartFragReq* req = (StartFragReq*)signal->getDataPtrSend();
  Uint32 instance = getInstanceKey(req->tableId, req->fragId);

  // wl4391_todo impl. method that fakes senders block-ref
  sendSignal(numberToRef(DBLQH, instance, getOwnNodeId()),
             GSN_START_FRAGREQ, signal, signal->getLength(), JBB);
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

  ssRelease<Ss_START_RECREQ>(ssId);
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

BLOCK_FUNCTIONS(DblqhProxy)
