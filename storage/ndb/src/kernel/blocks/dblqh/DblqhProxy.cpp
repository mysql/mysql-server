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
  addRecSignal(GSN_LQHFRAGCONF, &DblqhProxy::execLQHFRAGCONF);
  addRecSignal(GSN_LQHFRAGREF, &DblqhProxy::execLQHFRAGREF);

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

  // GSN_START_RECREQ
  addRecSignal(GSN_START_RECREQ, &DblqhProxy::execSTART_RECREQ);
  addRecSignal(GSN_START_RECCONF, &DblqhProxy::execSTART_RECCONF);
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

// GSN_LQHFRAGREQ [ sub-op ]

void
DblqhProxy::execLQHFRAGREQ(Signal* signal)
{
  Ss_LQHFRAGREQ& ss = ssSeize<Ss_LQHFRAGREQ>(1); // lost connection

  const LqhFragReq* req = (const LqhFragReq*)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void
DblqhProxy::sendLQHFRAGREQ(Signal* signal, Uint32 ssId)
{
  Ss_LQHFRAGREQ& ss = ssFind<Ss_LQHFRAGREQ>(ssId);

  LqhFragReq* req = (LqhFragReq*)signal->getDataPtrSend();
  *req = ss.m_req;

  NdbLogPartInfo lpinfo(workerInstance(ss.m_worker));
  Uint32 logPartNo = lpinfo.partNoFromId(req->logPartId);
  if (!lpinfo.partNoOwner(logPartNo)) {
    jam();
    skipReq(ss);
    return;
  }

  req->senderRef = reference();
  req->senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_LQHFRAGREQ,
             signal, LqhFragReq::SignalLength, JBB);
}

void
DblqhProxy::execLQHFRAGCONF(Signal* signal)
{
  const LqhFragConf* conf = (const LqhFragConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_LQHFRAGREQ& ss = ssFind<Ss_LQHFRAGREQ>(ssId);
  recvCONF(signal, ss);
}

void
DblqhProxy::execLQHFRAGREF(Signal* signal)
{
  const LqhFragRef* ref = (const LqhFragRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_LQHFRAGREQ& ss = ssFind<Ss_LQHFRAGREQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DblqhProxy::sendLQHFRAGCONF(Signal* signal, Uint32 ssId)
{
  Ss_LQHFRAGREQ& ss = ssFind<Ss_LQHFRAGREQ>(ssId);
  Ss_CREATE_TAB_REQ& ss_main = ssFind<Ss_CREATE_TAB_REQ>(ssId);
  BlockReference dictRef = ss_main.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    LqhFragConf* conf = (LqhFragConf*)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->lqhFragPtr = RNIL; //wl4391_todo
    conf->tableId = ss.m_req.tableId;
    conf->fragId = ss.m_req.fragId;
    conf->changeMask = 0;
    sendSignal(dictRef, GSN_LQHFRAGCONF,
               signal, LqhFragConf::SignalLength, JBB);
  } else {
    jam();
    LqhFragRef* ref = (LqhFragRef*)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    ref->tableId = ss.m_req.tableId;
    ref->fragId = ss.m_req.fragId;
    ref->requestInfo = 0;
    ref->changeMask = 0;
    sendSignal(dictRef, GSN_LQHFRAGREF,
               signal, LqhFragRef::SignalLength, JBB);
    ssRelease<Ss_CREATE_TAB_REQ>(ssId);
  }

  ssRelease<Ss_LQHFRAGREQ>(ssId);
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
    sendSignal(dictRef, GSN_DROP_TAB_CONF,
               signal, DropTabConf::SignalLength, JBB);
  }

  ssRelease<Ss_DROP_TAB_REQ>(ssId);
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

BLOCK_FUNCTIONS(DblqhProxy)
