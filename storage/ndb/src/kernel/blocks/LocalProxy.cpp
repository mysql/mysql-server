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

#include <mt.hpp>
#include "LocalProxy.hpp"

LocalProxy::LocalProxy(BlockNumber blockNumber, Block_context& ctx) :
  SimulatedBlock(blockNumber, ctx),
  c_nodeList(c_nodePool)
{
  BLOCK_CONSTRUCTOR(LocalProxy);

  ndbrequire(instance() == 0); // this is main block
  c_workers = 0;
  Uint32 i;
  for (i = 0; i < MaxWorkers; i++)
    c_worker[i] = 0;

  c_ssIdSeq = 0;

  c_typeOfStart = NodeState::ST_ILLEGAL_TYPE;
  c_masterNodeId = ZNIL;
  c_nodePool.setSize(MAX_NDB_NODES);

  // GSN_READ_CONFIG_REQ
  addRecSignal(GSN_READ_CONFIG_REQ, &LocalProxy::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_READ_CONFIG_CONF, &LocalProxy::execREAD_CONFIG_CONF, true);

  // GSN_STTOR
  addRecSignal(GSN_STTOR, &LocalProxy::execSTTOR);
  addRecSignal(GSN_STTORRY, &LocalProxy::execSTTORRY);

  // GSN_NDB_STTOR
  addRecSignal(GSN_NDB_STTOR, &LocalProxy::execNDB_STTOR);
  addRecSignal(GSN_NDB_STTORRY, &LocalProxy::execNDB_STTORRY);

  // GSN_READ_NODESREQ
  addRecSignal(GSN_READ_NODESCONF, &LocalProxy::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &LocalProxy::execREAD_NODESREF);

  // GSN_NODE_FAILREP
  addRecSignal(GSN_NODE_FAILREP, &LocalProxy::execNODE_FAILREP);
  addRecSignal(GSN_NF_COMPLETEREP, &LocalProxy::execNF_COMPLETEREP);

  // GSN_INCL_NODEREQ
  addRecSignal(GSN_INCL_NODEREQ, &LocalProxy::execINCL_NODEREQ);
  addRecSignal(GSN_INCL_NODECONF, &LocalProxy::execINCL_NODECONF);

  // GSN_DUMP_STATE_ORD
  addRecSignal(GSN_DUMP_STATE_ORD, &LocalProxy::execDUMP_STATE_ORD);

  // GSN_NDB_TAMPER
  addRecSignal(GSN_NDB_TAMPER, &LocalProxy::execNDB_TAMPER, true);

  // GSN_TIME_SIGNAL
  addRecSignal(GSN_TIME_SIGNAL, &LocalProxy::execTIME_SIGNAL);
}

LocalProxy::~LocalProxy()
{
  // dtor of main block deletes workers
}

// support routines

void
LocalProxy::sendREQ(Signal* signal, SsSequential& ss)
{
  ss.m_worker = 0;
  ndbrequire(ss.m_sendREQ != 0);
  (this->*ss.m_sendREQ)(signal, ss.m_ssId);
}

void
LocalProxy::recvCONF(Signal* signal, SsSequential& ss)
{
  ndbrequire(ss.m_sendCONF != 0);
  (this->*ss.m_sendCONF)(signal, ss.m_ssId);

  ss.m_worker++;
  if (ss.m_worker < c_workers) {
    jam();
    ndbrequire(ss.m_sendREQ != 0);
    (this->*ss.m_sendREQ)(signal, ss.m_ssId);
    return;
  }
}

void
LocalProxy::recvREF(Signal* signal, SsSequential& ss, Uint32 error)
{
  ndbrequire(error != 0);
  if (ss.m_error == 0)
    ss.m_error = error;
  recvCONF(signal, ss);
}

void
LocalProxy::skipReq(SsSequential& ss)
{
}

void
LocalProxy::skipConf(SsSequential& ss)
{
}

bool
LocalProxy::firstReply(const SsSequential& ss)
{
  return ss.m_worker == 0;
}

bool
LocalProxy::lastReply(const SsSequential& ss)
{
  return ss.m_worker + 1 == c_workers;
}

void
LocalProxy::sendREQ(Signal* signal, SsParallel& ss)
{
  ndbrequire(ss.m_sendREQ != 0);

  ss.m_workerMask.clear();
  ss.m_worker = 0;
  while (ss.m_worker < c_workers) {
    jam();
    ss.m_workerMask.set(ss.m_worker);
    (this->*ss.m_sendREQ)(signal, ss.m_ssId);
    ss.m_worker++;
  }
}

void
LocalProxy::recvCONF(Signal* signal, SsParallel& ss)
{
  ndbrequire(ss.m_sendCONF != 0);

  BlockReference ref = signal->getSendersBlockRef();
  ndbrequire(refToMain(ref) == number());

  ss.m_worker = refToInstance(ref) - 1;
  ndbrequire(ref == workerRef(ss.m_worker));
  ndbrequire(ss.m_worker < c_workers);
  ndbrequire(ss.m_workerMask.get(ss.m_worker));
  ss.m_workerMask.clear(ss.m_worker);

  (this->*ss.m_sendCONF)(signal, ss.m_ssId);
}

void
LocalProxy::recvREF(Signal* signal, SsParallel& ss, Uint32 error)
{
  ndbrequire(error != 0);
  if (ss.m_error == 0)
    ss.m_error = error;
  recvCONF(signal, ss);
}

void
LocalProxy::skipReq(SsParallel& ss)
{
  ndbrequire(ss.m_workerMask.get(ss.m_worker));
  ss.m_workerMask.clear(ss.m_worker);
}

// more replies expected from this worker
void
LocalProxy::skipConf(SsParallel& ss)
{
  ndbrequire(!ss.m_workerMask.get(ss.m_worker));
  ss.m_workerMask.set(ss.m_worker);
}

bool
LocalProxy::firstReply(const SsParallel& ss)
{
  const WorkerMask& mask = ss.m_workerMask;
  const Uint32 count = mask.count();

  // recvCONF has cleared current worker
  ndbrequire(ss.m_worker < c_workers);
  ndbrequire(!mask.get(ss.m_worker));
  ndbrequire(count < c_workers);
  return count + 1 == c_workers;
}

bool
LocalProxy::lastReply(const SsParallel& ss)
{
  return ss.m_workerMask.isclear();
}

// load workers (before first signal)

void
LocalProxy::loadWorkers()
{
  c_workers = getLqhWorkers();

  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    const Uint32 instanceNo = 1 + i;
    SimulatedBlock* worker = newWorker(instanceNo);
    ndbrequire(worker->instance() == instanceNo);
    ndbrequire(this->getInstance(instanceNo) == worker);
    c_worker[i] = worker;

    add_lqh_worker_thr_map(number(), instanceNo);
  }
}

// GSN_READ_CONFIG_REQ

void
LocalProxy::execREAD_CONFIG_REQ(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = ssSeize<Ss_READ_CONFIG_REQ>();

  const ReadConfigReq* req = (const ReadConfigReq*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(ss.m_req.noOfParameters == 0);

  // run sequentially due to big mallocs and initializations
  sendREQ(signal, ss);
}

void
LocalProxy::sendREAD_CONFIG_REQ(Signal* signal, Uint32 ssId)
{
  Ss_READ_CONFIG_REQ& ss = ssFind<Ss_READ_CONFIG_REQ>(ssId);

  ReadConfigReq* req = (ReadConfigReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ssId;
  req->noOfParameters = 0;
  sendSignal(workerRef(ss.m_worker), GSN_READ_CONFIG_REQ,
             signal, ReadConfigReq::SignalLength, JBB);
}

void
LocalProxy::execREAD_CONFIG_CONF(Signal* signal)
{
  const ReadConfigConf* conf = (const ReadConfigConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_READ_CONFIG_REQ& ss = ssFind<Ss_READ_CONFIG_REQ>(ssId);
  recvCONF(signal, ss);
}

void
LocalProxy::sendREAD_CONFIG_CONF(Signal* signal, Uint32 ssId)
{
  Ss_READ_CONFIG_REQ& ss = ssFind<Ss_READ_CONFIG_REQ>(ssId);

  if (!lastReply(ss))
    return;

  ReadConfigConf* conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = ss.m_req.senderData;
  sendSignal(ss.m_req.senderRef, GSN_READ_CONFIG_CONF,
             signal, ReadConfigConf::SignalLength, JBB);

  ssRelease<Ss_READ_CONFIG_REQ>(ssId);
}

// GSN_STTOR

void
LocalProxy::execSTTOR(Signal* signal)
{
  Ss_STTOR& ss = ssSeize<Ss_STTOR>(1);

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  if (startphase == 3) {
    jam();
    c_typeOfStart = typeOfStart;
  }

  ss.m_reqlength = signal->getLength();
  memcpy(ss.m_reqdata, signal->getDataPtr(), ss.m_reqlength << 2);

  callSTTOR(signal);
}

void
LocalProxy::callSTTOR(Signal* signal)
{
  backSTTOR(signal);
}

void
LocalProxy::backSTTOR(Signal* signal)
{
  Ss_STTOR& ss = ssFind<Ss_STTOR>(1);
  sendREQ(signal, ss);
}

void
LocalProxy::sendSTTOR(Signal* signal, Uint32 ssId)
{
  Ss_STTOR& ss = ssFind<Ss_STTOR>(ssId);

  memcpy(signal->getDataPtrSend(), ss.m_reqdata, ss.m_reqlength << 2);
  sendSignal(workerRef(ss.m_worker), GSN_STTOR,
             signal, ss.m_reqlength, JBB);
}

void
LocalProxy::execSTTORRY(Signal* signal)
{
  Ss_STTOR& ss = ssFind<Ss_STTOR>(1);
  recvCONF(signal, ss);
}

void
LocalProxy::sendSTTORRY(Signal* signal, Uint32 ssId)
{
  Ss_STTOR& ss = ssFind<Ss_STTOR>(ssId);

  const Uint32 conflength = signal->getLength();
  const Uint32* confdata = signal->getDataPtr();

  // the reply is identical from all
  if (firstReply(ss)) {
    ss.m_conflength = conflength;
    memcpy(ss.m_confdata, confdata, conflength << 2);
  } else {
    ndbrequire(ss.m_conflength == conflength);
    ndbrequire(memcmp(ss.m_confdata, confdata, conflength << 2) == 0);
  }

  if (!lastReply(ss))
    return;

  memcpy(signal->getDataPtrSend(), ss.m_confdata, ss.m_conflength << 2);
  sendSignal(NDBCNTR_REF, GSN_STTORRY,
             signal, ss.m_conflength, JBB);

  ssRelease<Ss_STTOR>(ssId);
}

// GSN_NDB_STTOR

void
LocalProxy::execNDB_STTOR(Signal* signal)
{
  Ss_NDB_STTOR& ss = ssSeize<Ss_NDB_STTOR>(1);

  const NdbSttor* req = (const NdbSttor*)signal->getDataPtr();
  ss.m_req = *req;

  callNDB_STTOR(signal);
}

void
LocalProxy::callNDB_STTOR(Signal* signal)
{
  backNDB_STTOR(signal);
}

void
LocalProxy::backNDB_STTOR(Signal* signal)
{
  Ss_NDB_STTOR& ss = ssFind<Ss_NDB_STTOR>(1);
  sendREQ(signal, ss);
}

void
LocalProxy::sendNDB_STTOR(Signal* signal, Uint32 ssId)
{
  Ss_NDB_STTOR& ss = ssFind<Ss_NDB_STTOR>(ssId);

  NdbSttor* req = (NdbSttor*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  sendSignal(workerRef(ss.m_worker), GSN_NDB_STTOR,
             signal, ss.m_reqlength, JBB);
}

void
LocalProxy::execNDB_STTORRY(Signal* signal)
{
  Ss_NDB_STTOR& ss = ssFind<Ss_NDB_STTOR>(1);

  // the reply contains only senderRef
  const NdbSttorry* conf = (const NdbSttorry*)signal->getDataPtr();
  ndbrequire(conf->senderRef == signal->getSendersBlockRef());
  recvCONF(signal, ss);
}

void
LocalProxy::sendNDB_STTORRY(Signal* signal, Uint32 ssId)
{
  Ss_NDB_STTOR& ss = ssFind<Ss_NDB_STTOR>(ssId);

  if (!lastReply(ss))
    return;

  NdbSttorry* conf = (NdbSttorry*)signal->getDataPtrSend();
  conf->senderRef = reference();
  sendSignal(NDBCNTR_REF, GSN_NDB_STTORRY,
             signal, NdbSttorry::SignalLength, JBB);

  ssRelease<Ss_NDB_STTOR>(ssId);
}

// GSN_READ_NODESREQ

void
LocalProxy::sendREAD_NODESREQ(Signal* signal)
{
  signal->theData[0] = reference();
  sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
}

void
LocalProxy::execREAD_NODESCONF(Signal* signal)
{
  Ss_READ_NODES_REQ& ss = c_ss_READ_NODESREQ;

  const ReadNodesConf* conf = (const ReadNodesConf*)signal->getDataPtr();

  ndbrequire(c_nodePool.getNoOfFree() == c_nodePool.getSize());
  Uint32 count = 0;
  Uint32 i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    if (NdbNodeBitmask::get(conf->allNodes, i)) {
      jam();
      count++;

      NodePtr nodePtr;
      bool ok = c_nodePool.seize(nodePtr);
      ndbrequire(ok);
      new (nodePtr.p) Node;

      nodePtr.p->m_nodeId = i;
      if (NdbNodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
        nodePtr.p->m_alive = false;
      } else {
        jam();
        nodePtr.p->m_alive = true;
      }

      c_nodeList.addLast(nodePtr);
    }
  }
  ndbrequire(count != 0 && count == conf->noOfNodes);

  c_masterNodeId = conf->masterNodeId;

  switch (ss.m_gsn) {
  case GSN_STTOR:
    backSTTOR(signal);
    break;
  case GSN_NDB_STTOR:
    backNDB_STTOR(signal);
    break;
  default:
    ndbrequire(false);
    break;
  }

  ss.m_gsn = 0;
}

void
LocalProxy::execREAD_NODESREF(Signal* signal)
{
  Ss_READ_NODES_REQ& ss = c_ss_READ_NODESREQ;
  ndbrequire(ss.m_gsn != 0);
  ndbrequire(false);
}

// GSN_NODE_FAILREP

void
LocalProxy::execNODE_FAILREP(Signal* signal)
{
  Ss_NODE_FAILREP& ss = ssSeize<Ss_NODE_FAILREP>(1);
  const NodeFailRep* req = (const NodeFailRep*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == NodeFailRep::SignalLength);

  // proxy itself
  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    if (NdbNodeBitmask::get(req->theNodes, nodePtr.p->m_nodeId)) {
      jam();
      ndbrequire(nodePtr.p->m_alive);
      nodePtr.p->m_alive = false;
    }
    c_nodeList.next(nodePtr);
  }

  // from each worker wait for ack for each failed node
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    jam();
    NdbNodeBitmask& waitFor = ss.m_waitFor[i];
    waitFor.assign(NdbNodeBitmask::Size, req->theNodes);
  }

  sendREQ(signal, ss);
  if (ss.noReply(number())) {
    jam();
    ssRelease<Ss_NODE_FAILREP>(ss);
  }
}

void
LocalProxy::sendNODE_FAILREP(Signal* signal, Uint32 ssId)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(ssId);

  NodeFailRep* req = (NodeFailRep*)signal->getDataPtrSend();
  *req = ss.m_req;
  sendSignal(workerRef(ss.m_worker), GSN_NODE_FAILREP,
             signal, NodeFailRep::SignalLength, JBB);
}

void
LocalProxy::execNF_COMPLETEREP(Signal* signal)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(1);
  ndbrequire(!ss.noReply(number()));
  recvCONF(signal, ss);
}

void
LocalProxy::sendNF_COMPLETEREP(Signal* signal, Uint32 ssId)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(ssId);

  {
    const NFCompleteRep* conf = (const NFCompleteRep*)signal->getDataPtr();

    NdbNodeBitmask& waitFor = ss.m_waitFor[ss.m_worker];
    ndbrequire(waitFor.get(conf->failedNodeId));
    waitFor.clear(conf->failedNodeId);
    
    if (!waitFor.isclear()) {
      // worker has not replied for all failed nodes
      skipConf(ss);
    }
  }

  if (!lastReply(ss))
    return;

  NdbNodeBitmask theNodes;
  theNodes.assign(NdbNodeBitmask::Size, ss.m_req.theNodes);

  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    if (theNodes.get(nodePtr.p->m_nodeId)) {
      jam();
      NFCompleteRep* conf = (NFCompleteRep*)signal->getDataPtrSend();
      conf->blockNo = number();
      conf->nodeId = getOwnNodeId();
      conf->failedNodeId = nodePtr.p->m_nodeId;
      conf->unused = 0;
      conf->from = __LINE__;

      sendSignal(DBDIH_REF, GSN_NF_COMPLETEREP,
                 signal, NFCompleteRep::SignalLength, JBB);
    }

    c_nodeList.next(nodePtr);
  }

  ssRelease<Ss_NODE_FAILREP>(ssId);
}

// GSN_INCL_NODEREQ

void
LocalProxy::execINCL_NODEREQ(Signal* signal)
{
  Ss_INCL_NODEREQ& ss = ssSeize<Ss_INCL_NODEREQ>(1);

  ss.m_reqlength = signal->getLength();
  ndbrequire(sizeof(ss.m_req) >= (ss.m_reqlength << 2));
  memcpy(&ss.m_req, signal->getDataPtr(), ss.m_reqlength << 2);

  // proxy itself
  NodePtr nodePtr;
  c_nodeList.first(nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  while (nodePtr.i != RNIL) {
    jam();
    if (ss.m_req.inclNodeId == nodePtr.p->m_nodeId) {
      jam();
      ndbrequire(!nodePtr.p->m_alive);
      nodePtr.p->m_alive = true;
    }
    c_nodeList.next(nodePtr);
  }

  sendREQ(signal, ss);
}

void
LocalProxy::sendINCL_NODEREQ(Signal* signal, Uint32 ssId)
{
  Ss_INCL_NODEREQ& ss = ssFind<Ss_INCL_NODEREQ>(ssId);

  Ss_INCL_NODEREQ::Req* req =
    (Ss_INCL_NODEREQ::Req*)signal->getDataPtrSend();

  memcpy(req, &ss.m_req, ss.m_reqlength << 2);
  req->senderRef = reference();
  sendSignal(workerRef(ss.m_worker), GSN_INCL_NODEREQ,
             signal, ss.m_reqlength, JBB);
}

void
LocalProxy::execINCL_NODECONF(Signal* signal)
{
  Ss_INCL_NODEREQ& ss = ssFind<Ss_INCL_NODEREQ>(1);
  recvCONF(signal, ss);
}

void
LocalProxy::sendINCL_NODECONF(Signal* signal, Uint32 ssId)
{
  Ss_INCL_NODEREQ& ss = ssFind<Ss_INCL_NODEREQ>(ssId);

  if (!lastReply(ss))
    return;

  Ss_INCL_NODEREQ::Conf* conf =
    (Ss_INCL_NODEREQ::Conf*)signal->getDataPtrSend();

  conf->inclNodeId = ss.m_req.inclNodeId;
  conf->senderRef = reference();
  sendSignal(ss.m_req.senderRef, GSN_INCL_NODECONF,
             signal, 2, JBB);

  ssRelease<Ss_INCL_NODEREQ>(ssId);
}

// GSN_DUMP_STATE_ORD

void
LocalProxy::execDUMP_STATE_ORD(Signal* signal)
{
  Ss_DUMP_STATE_ORD& ss = ssSeize<Ss_DUMP_STATE_ORD>();

  ss.m_reqlength = signal->getLength();
  memcpy(ss.m_reqdata, signal->getDataPtr(), ss.m_reqlength << 2);
  sendREQ(signal, ss);
  ssRelease<Ss_DUMP_STATE_ORD>(ss);
}

void
LocalProxy::sendDUMP_STATE_ORD(Signal* signal, Uint32 ssId)
{
  Ss_DUMP_STATE_ORD& ss = ssFind<Ss_DUMP_STATE_ORD>(ssId);

  memcpy(signal->getDataPtrSend(), ss.m_reqdata, ss.m_reqlength << 2);
  sendSignal(workerRef(ss.m_worker), GSN_DUMP_STATE_ORD,
             signal, ss.m_reqlength, JBB);
}

// GSN_NDB_TAMPER

void
LocalProxy::execNDB_TAMPER(Signal* signal)
{
  Ss_NDB_TAMPER& ss = ssSeize<Ss_NDB_TAMPER>();

  ndbrequire(signal->getLength() == 1);
  ss.m_errorInsert = signal->theData[0];

  SimulatedBlock::execNDB_TAMPER(signal);
  sendREQ(signal, ss);
  ssRelease<Ss_NDB_TAMPER>(ss);
}

void
LocalProxy::sendNDB_TAMPER(Signal* signal, Uint32 ssId)
{
  Ss_NDB_TAMPER& ss = ssFind<Ss_NDB_TAMPER>(ssId);

  signal->theData[0] = ss.m_errorInsert;
  sendSignal(workerRef(ss.m_worker), GSN_NDB_TAMPER,
             signal, 1, JBB);
}

// GSN_TIME_SIGNAL

void
LocalProxy::execTIME_SIGNAL(Signal* signal)
{
  Ss_TIME_SIGNAL& ss = ssSeize<Ss_TIME_SIGNAL>();

  // could use same for MT TC
  ndbrequire(number() == DBLQH);
  sendREQ(signal, ss);
  ssRelease<Ss_TIME_SIGNAL>(ss);
}

void
LocalProxy::sendTIME_SIGNAL(Signal* signal, Uint32 ssId)
{
  Ss_TIME_SIGNAL& ss = ssFind<Ss_TIME_SIGNAL>(ssId);
  signal->theData[0] = 0;
  sendSignal(workerRef(ss.m_worker), GSN_TIME_SIGNAL,
             signal, 1, JBB);
}

BLOCK_FUNCTIONS(LocalProxy)
