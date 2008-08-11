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

DblqhProxy::DblqhProxy(Block_context& ctx) :
  LocalProxy(DBLQH, ctx)
{
  // GSN_SEND_PACKED
  addRecSignal(GSN_SEND_PACKED, &DblqhProxy::execSEND_PACKED);

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
}

DblqhProxy::~DblqhProxy()
{
}

SimulatedBlock*
DblqhProxy::newWorker(Uint32 instanceNo)
{
  return new Dblqh(m_ctx, instanceNo);
}

// GSN_SEND_PACKED

void
DblqhProxy::execSEND_PACKED(Signal* signal)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    ndbrequire(c_worker[i] != 0);
    Dblqh* dblqh = static_cast<Dblqh*>(c_worker[i]);
    dblqh->execSEND_PACKED(signal);
  }
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

  if (!isLogPartOwner(ss.m_worker, req->logPartId)) {
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

BLOCK_FUNCTIONS(DblqhProxy)
