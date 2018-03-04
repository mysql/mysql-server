/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mt.hpp>
#include "LocalProxy.hpp"
#include <pgman.hpp>

//#define DBINFO_SCAN_TRACE
#ifdef DBINFO_SCAN_TRACE
#include <debugger/DebuggerNames.hpp>
#endif

#define JAM_FILE_ID 437


LocalProxy::LocalProxy(BlockNumber blockNumber, Block_context& ctx) :
  SimulatedBlock(blockNumber, ctx)
{
  BLOCK_CONSTRUCTOR(LocalProxy);

  ndbrequire(instance() == 0); // this is main block
  c_workers = 0;
  Uint32 i;
  for (i = 0; i < MaxWorkers; i++)
    c_worker[i] = 0;

  c_anyWorkerCounter = 0;
  c_typeOfStart = NodeState::ST_ILLEGAL_TYPE;
  c_masterNodeId = ZNIL;

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

  // GSN_NODE_STATE_REP
  addRecSignal(GSN_NODE_STATE_REP, &LocalProxy::execNODE_STATE_REP, true);

  // GSN_CHANGE_NODE_STATE_REQ
  addRecSignal(GSN_CHANGE_NODE_STATE_REQ, &LocalProxy::execCHANGE_NODE_STATE_REQ, true);
  addRecSignal(GSN_CHANGE_NODE_STATE_CONF, &LocalProxy::execCHANGE_NODE_STATE_CONF);

  // GSN_DUMP_STATE_ORD
  addRecSignal(GSN_DUMP_STATE_ORD, &LocalProxy::execDUMP_STATE_ORD);

  // GSN_NDB_TAMPER
  addRecSignal(GSN_NDB_TAMPER, &LocalProxy::execNDB_TAMPER, true);

  // GSN_TIME_SIGNAL
  addRecSignal(GSN_TIME_SIGNAL, &LocalProxy::execTIME_SIGNAL);

  // GSN_CREATE_TRIG_IMPL_REQ
  addRecSignal(GSN_CREATE_TRIG_IMPL_REQ, &LocalProxy::execCREATE_TRIG_IMPL_REQ);
  addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &LocalProxy::execCREATE_TRIG_IMPL_CONF);
  addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &LocalProxy::execCREATE_TRIG_IMPL_REF);

  // GSN_DROP_TRIG_IMPL_REQ
  addRecSignal(GSN_DROP_TRIG_IMPL_REQ, &LocalProxy::execDROP_TRIG_IMPL_REQ);
  addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &LocalProxy::execDROP_TRIG_IMPL_CONF);
  addRecSignal(GSN_DROP_TRIG_IMPL_REF, &LocalProxy::execDROP_TRIG_IMPL_REF);

  // GSN_DBINFO_SCANREQ
  addRecSignal(GSN_DBINFO_SCANREQ, &LocalProxy::execDBINFO_SCANREQ);
  addRecSignal(GSN_DBINFO_SCANCONF, &LocalProxy::execDBINFO_SCANCONF);

  // GSN_SYNC_REQ
  addRecSignal(GSN_SYNC_REQ, &LocalProxy::execSYNC_REQ, true);
  addRecSignal(GSN_SYNC_REF, &LocalProxy::execSYNC_REF);
  addRecSignal(GSN_SYNC_CONF, &LocalProxy::execSYNC_CONF);

  // GSN_SYNC_PATH_REQ
  addRecSignal(GSN_SYNC_PATH_REQ, &LocalProxy::execSYNC_PATH_REQ, true);

  // GSN_API_FAILREQ
  addRecSignal(GSN_API_FAILREQ, &LocalProxy::execAPI_FAILREQ);
  addRecSignal(GSN_API_FAILCONF, &LocalProxy::execAPI_FAILCONF);
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
  SectionHandle handle(this);
  restoreHandle(handle, ss);
  (this->*ss.m_sendREQ)(signal, ss.m_ssId, &handle);
  saveSections(ss, handle);
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
    SectionHandle handle(this);
    (this->*ss.m_sendREQ)(signal, ss.m_ssId, &handle);
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

void
LocalProxy::saveSections(SsCommon& ss, SectionHandle & handle)
{
  ss.m_sec_cnt = handle.m_cnt;
  for (Uint32 i = 0; i<ss.m_sec_cnt; i++)
    ss.m_sec_ptr[i] = handle.m_ptr[i].i;
  handle.clear();
}

void
LocalProxy::restoreHandle(SectionHandle & handle, SsCommon& ss)
{
  handle.m_cnt = ss.m_sec_cnt;
  for (Uint32 i = 0; i<ss.m_sec_cnt; i++)
    handle.m_ptr[i].i = ss.m_sec_ptr[i];

  getSections(handle.m_cnt, handle.m_ptr);
  ss.m_sec_cnt = 0;
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
LocalProxy::sendREQ(Signal* signal, SsParallel& ss, bool skipLast)
{
  ndbrequire(ss.m_sendREQ != 0);

  ss.m_workerMask.clear();
  ss.m_worker = 0;
  const Uint32 count = skipLast ? c_workers - 1 : c_workers;
  SectionHandle handle(this);
  restoreHandle(handle, ss);
  while (ss.m_worker < count) {
    jam();
    ss.m_workerMask.set(ss.m_worker);
    (this->*ss.m_sendREQ)(signal, ss.m_ssId, &handle);
    ss.m_worker++;
  }
  releaseSections(handle);
}

void
LocalProxy::recvCONF(Signal* signal, SsParallel& ss)
{
  ndbrequire(ss.m_sendCONF != 0);

  BlockReference ref = signal->getSendersBlockRef();
  ndbrequire(refToMain(ref) == number());

  Uint32 ino = refToInstance(ref);
  ss.m_worker = workerIndex(ino);
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

// used in "reverse" proxying (start with worker REQs)
void
LocalProxy::setMask(SsParallel& ss)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++)
    ss.m_workerMask.set(i);
}

void
LocalProxy::setMask(SsParallel& ss, const WorkerMask& mask)
{
  ss.m_workerMask.assign(mask);
}

// load workers (before first signal)

void
LocalProxy::loadWorkers()
{
  c_workers = mt_get_instance_count(number());
  for (Uint32 i = 0; i < c_workers; i++)
  {
    jamNoBlock();
    Uint32 instanceNo = workerInstance(i);

    SimulatedBlock* worker = newWorker(instanceNo);
    ndbrequire(worker->instance() == instanceNo);
    ndbrequire(this->getInstance(instanceNo) == worker);
    c_worker[i] = worker;

    if (number() == PGMAN && i == (c_workers - 1))
    {
      ((Pgman*)worker)->set_extra_pgman();
    }
    mt_add_thr_map(number(), instanceNo);
  }
}

void
LocalProxy::forwardToWorkerIndex(Signal* signal, Uint32 index)
{
  jam();
  /**
   * We statelessly forward to one of our 
   * workers, including any sections that 
   * might be attached.
   */
  BlockReference destRef = workerRef(index);
  SectionHandle sh(this, signal);
  
  sendSignal(destRef,
             signal->header.theVerId_signalNumber,
             signal,
             signal->getLength(),
             JBB,
             &sh);
};

void
LocalProxy::forwardToAnyWorker(Signal* signal)
{
  jam();

  /* Won't work for fragmented signals */
  ndbassert(signal->header.m_fragmentInfo == 0);

  forwardToWorkerIndex(signal, getAnyWorkerIndex());
}

// GSN_READ_CONFIG_REQ

void
LocalProxy::execREAD_CONFIG_REQ(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = ssSeize<Ss_READ_CONFIG_REQ>(1);

  const ReadConfigReq* req = (const ReadConfigReq*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(ss.m_req.noOfParameters == 0);
  callREAD_CONFIG_REQ(signal);
}

void
LocalProxy::callREAD_CONFIG_REQ(Signal* signal)
{
  backREAD_CONFIG_REQ(signal);
}

void
LocalProxy::backREAD_CONFIG_REQ(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = ssFind<Ss_READ_CONFIG_REQ>(1);

  // run sequentially due to big mallocs and initializations
  sendREQ(signal, ss);
}

void
LocalProxy::sendREAD_CONFIG_REQ(Signal* signal, Uint32 ssId,
                                SectionHandle* handle)
{
  Ss_READ_CONFIG_REQ& ss = ssFind<Ss_READ_CONFIG_REQ>(ssId);

  ReadConfigReq* req = (ReadConfigReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ssId;
  req->noOfParameters = 0;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_READ_CONFIG_REQ,
                      signal, ReadConfigReq::SignalLength, JBB, handle);
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

  SectionHandle handle(this);
  restoreHandle(handle, ss);
  releaseSections(handle);

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
LocalProxy::sendSTTOR(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_STTOR& ss = ssFind<Ss_STTOR>(ssId);

  memcpy(signal->getDataPtrSend(), ss.m_reqdata, ss.m_reqlength << 2);
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_STTOR,
                      signal, ss.m_reqlength, JBB, handle);
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
LocalProxy::sendNDB_STTOR(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_NDB_STTOR& ss = ssFind<Ss_NDB_STTOR>(ssId);

  NdbSttor* req = (NdbSttor*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_NDB_STTOR,
                      signal, ss.m_reqlength, JBB, handle);
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
  Ss_NODE_FAILREP& ss = ssFindSeize<Ss_NODE_FAILREP>(1, 0);
  const NodeFailRep* req = (const NodeFailRep*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == NodeFailRep::SignalLength);

  NdbNodeBitmask mask;
  mask.assign(NdbNodeBitmask::Size, req->theNodes);

  // from each worker wait for ack for each failed node
  for (Uint32 i = 0; i < c_workers; i++)
  {
    jam();
    NdbNodeBitmask& waitFor = ss.m_waitFor[i];
    waitFor.bitOR(mask);
  }

  sendREQ(signal, ss);
  if (ss.noReply(number()))
  {
    jam();
    ssRelease<Ss_NODE_FAILREP>(ss);
  }
}

void
LocalProxy::sendNODE_FAILREP(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(ssId);

  NodeFailRep* req = (NodeFailRep*)signal->getDataPtrSend();
  *req = ss.m_req;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_NODE_FAILREP,
                      signal, NodeFailRep::SignalLength, JBB, handle);
}

void
LocalProxy::execNF_COMPLETEREP(Signal* signal)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(1);
  ndbrequire(!ss.noReply(number()));
  ss.m_workerMask.set(ss.m_worker); // Avoid require in recvCONF
  recvCONF(signal, ss);
}

void
LocalProxy::sendNF_COMPLETEREP(Signal* signal, Uint32 ssId)
{
  Ss_NODE_FAILREP& ss = ssFind<Ss_NODE_FAILREP>(ssId);

  const NFCompleteRep* conf = (const NFCompleteRep*)signal->getDataPtr();
  Uint32 node = conf->failedNodeId;

  {
    NdbNodeBitmask& waitFor = ss.m_waitFor[ss.m_worker];
    ndbrequire(waitFor.get(node));
    waitFor.clear(node);
  }

  for (Uint32 i = 0; i < c_workers; i++)
  {
    jam();
    NdbNodeBitmask& waitFor = ss.m_waitFor[i];
    if (waitFor.get(node))
    {
      jam();
      /**
       * Not all threads are done with this failed node
       */
      return;
    }
  }

  {
    NFCompleteRep* conf = (NFCompleteRep*)signal->getDataPtrSend();
    conf->blockNo = number();
    conf->nodeId = getOwnNodeId();
    conf->failedNodeId = node;
    conf->unused = 0;
    conf->from = __LINE__;

    sendSignal(DBDIH_REF, GSN_NF_COMPLETEREP,
               signal, NFCompleteRep::SignalLength, JBB);

    if (number() == DBTC)
    {
      /**
       * DBTC send NF_COMPLETEREP "early" to QMGR
       *   so that it can allow api to handle node-failure of
       *   transactions eariler...
       * See Qmgr::execNF_COMPLETEREP
       */
      jam();
      sendSignal(QMGR_REF, GSN_NF_COMPLETEREP, signal,
                 NFCompleteRep::SignalLength, JBB);
    }
  }
}

// GSN_INCL_NODEREQ

void
LocalProxy::execINCL_NODEREQ(Signal* signal)
{
  Ss_INCL_NODEREQ& ss = ssSeize<Ss_INCL_NODEREQ>(1);

  ss.m_reqlength = signal->getLength();
  ndbrequire(sizeof(ss.m_req) >= (ss.m_reqlength << 2));
  memcpy(&ss.m_req, signal->getDataPtr(), ss.m_reqlength << 2);

  sendREQ(signal, ss);
}

void
LocalProxy::sendINCL_NODEREQ(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_INCL_NODEREQ& ss = ssFind<Ss_INCL_NODEREQ>(ssId);

  Ss_INCL_NODEREQ::Req* req =
    (Ss_INCL_NODEREQ::Req*)signal->getDataPtrSend();

  memcpy(req, &ss.m_req, ss.m_reqlength << 2);
  req->senderRef = reference();
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_INCL_NODEREQ,
                      signal, ss.m_reqlength, JBB, handle);
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

// GSN_NODE_STATE_REP

void
LocalProxy::execNODE_STATE_REP(Signal* signal)
{
  Ss_NODE_STATE_REP& ss = ssSeize<Ss_NODE_STATE_REP>();
  sendREQ(signal, ss);
  SimulatedBlock::execNODE_STATE_REP(signal);
  ssRelease<Ss_NODE_STATE_REP>(ss);
}

void
LocalProxy::sendNODE_STATE_REP(Signal* signal, Uint32 ssId,
                               SectionHandle* handle)
{
  Ss_NODE_STATE_REP& ss = ssFind<Ss_NODE_STATE_REP>(ssId);

  sendSignalNoRelease(workerRef(ss.m_worker), GSN_NODE_STATE_REP,
                      signal,NodeStateRep::SignalLength, JBB, handle);
}

// GSN_CHANGE_NODE_STATE_REQ

void
LocalProxy::execCHANGE_NODE_STATE_REQ(Signal* signal)
{
  Ss_CHANGE_NODE_STATE_REQ& ss = ssSeize<Ss_CHANGE_NODE_STATE_REQ>(1);

  ChangeNodeStateReq * req = (ChangeNodeStateReq*)signal->getDataPtrSend();
  ss.m_req = *req;

  sendREQ(signal, ss);
}

void
LocalProxy::sendCHANGE_NODE_STATE_REQ(Signal* signal, Uint32 ssId,
                                      SectionHandle* handle)
{
  Ss_CHANGE_NODE_STATE_REQ& ss = ssFind<Ss_CHANGE_NODE_STATE_REQ>(ssId);

  ChangeNodeStateReq * req = (ChangeNodeStateReq*)signal->getDataPtrSend();
  req->senderRef = reference();

  sendSignalNoRelease(workerRef(ss.m_worker), GSN_CHANGE_NODE_STATE_REQ,
                      signal, ChangeNodeStateReq::SignalLength, JBB, handle);
}

void
LocalProxy::execCHANGE_NODE_STATE_CONF(Signal* signal)
{
  Ss_CHANGE_NODE_STATE_REQ& ss = ssFind<Ss_CHANGE_NODE_STATE_REQ>(1);

  ChangeNodeStateConf * conf = (ChangeNodeStateConf*)signal->getDataPtrSend();
  ndbrequire(conf->senderData == ss.m_req.senderData);
  recvCONF(signal, ss);
}

void
LocalProxy::sendCHANGE_NODE_STATE_CONF(Signal* signal, Uint32 ssId)
{
  Ss_CHANGE_NODE_STATE_REQ& ss = ssFind<Ss_CHANGE_NODE_STATE_REQ>(ssId);

  if (!lastReply(ss))
    return;

  /**
   * SimulatedBlock::execCHANGE_NODE_STATE_REQ will reply
   */
  ChangeNodeStateReq * req = (ChangeNodeStateReq*)signal->getDataPtrSend();
  * req = ss.m_req;
  SimulatedBlock::execCHANGE_NODE_STATE_REQ(signal);
  ssRelease<Ss_CHANGE_NODE_STATE_REQ>(ssId);
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
LocalProxy::sendDUMP_STATE_ORD(Signal* signal, Uint32 ssId,
                               SectionHandle* handle)
{
  Ss_DUMP_STATE_ORD& ss = ssFind<Ss_DUMP_STATE_ORD>(ssId);

  memcpy(signal->getDataPtrSend(), ss.m_reqdata, ss.m_reqlength << 2);
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_DUMP_STATE_ORD,
                      signal, ss.m_reqlength, JBB, handle);
}

// GSN_NDB_TAMPER

void
LocalProxy::execNDB_TAMPER(Signal* signal)
{
  Ss_NDB_TAMPER& ss = ssSeize<Ss_NDB_TAMPER>();

  const Uint32 siglen = signal->getLength();
  if (siglen == 1)
  {
    ss.m_errorInsert = signal->theData[0];
    ss.m_haveErrorInsertExtra = false;
  }
  else
  {
    ndbrequire(siglen == 2);
    ss.m_errorInsert = signal->theData[0];
    ss.m_haveErrorInsertExtra = true;
    ss.m_errorInsertExtra = signal->theData[1];
  }

  SimulatedBlock::execNDB_TAMPER(signal);
  sendREQ(signal, ss);
  ssRelease<Ss_NDB_TAMPER>(ss);
}

void
LocalProxy::sendNDB_TAMPER(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_NDB_TAMPER& ss = ssFind<Ss_NDB_TAMPER>(ssId);

  Uint32 siglen = 1;
  signal->theData[0] = ss.m_errorInsert;
  if (ss.m_haveErrorInsertExtra)
  {
    signal->theData[1] = ss.m_errorInsertExtra;
    siglen ++;
  }
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_NDB_TAMPER,
                      signal, siglen, JBB, handle);
}

// GSN_TIME_SIGNAL

void
LocalProxy::execTIME_SIGNAL(Signal* signal)
{
  Ss_TIME_SIGNAL& ss = ssSeize<Ss_TIME_SIGNAL>();

  sendREQ(signal, ss);
  ssRelease<Ss_TIME_SIGNAL>(ss);
}

void
LocalProxy::sendTIME_SIGNAL(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_TIME_SIGNAL& ss = ssFind<Ss_TIME_SIGNAL>(ssId);
  signal->theData[0] = 0;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_TIME_SIGNAL,
                      signal, 1, JBB, handle);
}

// GSN_CREATE_TRIG_IMPL_REQ

void
LocalProxy::execCREATE_TRIG_IMPL_REQ(Signal* signal)
{
  if (!assembleFragments(signal))
    return;

  if (ssQueue<Ss_CREATE_TRIG_IMPL_REQ>(signal))
    return;
  const CreateTrigImplReq* req = (const CreateTrigImplReq*)signal->getDataPtr();
  Ss_CREATE_TRIG_IMPL_REQ& ss = ssSeize<Ss_CREATE_TRIG_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() <= CreateTrigImplReq::SignalLength);

  SectionHandle handle(this, signal);
  saveSections(ss, handle);

  sendREQ(signal, ss);
}

void
LocalProxy::sendCREATE_TRIG_IMPL_REQ(Signal* signal, Uint32 ssId,
                                     SectionHandle * handle)
{
  Ss_CREATE_TRIG_IMPL_REQ& ss = ssFind<Ss_CREATE_TRIG_IMPL_REQ>(ssId);

  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_CREATE_TRIG_IMPL_REQ,
                      signal, CreateTrigImplReq::SignalLength, JBB,
                      handle);
}

void
LocalProxy::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  const CreateTrigImplConf* conf = (const CreateTrigImplConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_CREATE_TRIG_IMPL_REQ& ss = ssFind<Ss_CREATE_TRIG_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void
LocalProxy::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  const CreateTrigImplRef* ref = (const CreateTrigImplRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_CREATE_TRIG_IMPL_REQ& ss = ssFind<Ss_CREATE_TRIG_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
LocalProxy::sendCREATE_TRIG_IMPL_CONF(Signal* signal, Uint32 ssId)
{
  Ss_CREATE_TRIG_IMPL_REQ& ss = ssFind<Ss_CREATE_TRIG_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    CreateTrigImplConf* conf = (CreateTrigImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    conf->triggerId = ss.m_req.triggerId;
    conf->triggerInfo = ss.m_req.triggerInfo;
    sendSignal(dictRef, GSN_CREATE_TRIG_IMPL_CONF,
               signal, CreateTrigImplConf::SignalLength, JBB);
  } else {
    CreateTrigImplRef* ref = (CreateTrigImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->triggerId = ss.m_req.triggerId;
    ref->triggerInfo = ss.m_req.triggerInfo;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_CREATE_TRIG_IMPL_REF,
               signal, CreateTrigImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_CREATE_TRIG_IMPL_REQ>(ssId);
}

// GSN_DROP_TRIG_IMPL_REQ

void
LocalProxy::execDROP_TRIG_IMPL_REQ(Signal* signal)
{
  if (ssQueue<Ss_DROP_TRIG_IMPL_REQ>(signal))
    return;
  const DropTrigImplReq* req = (const DropTrigImplReq*)signal->getDataPtr();
  Ss_DROP_TRIG_IMPL_REQ& ss = ssSeize<Ss_DROP_TRIG_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropTrigImplReq::SignalLength);
  sendREQ(signal, ss);
}

void
LocalProxy::sendDROP_TRIG_IMPL_REQ(Signal* signal, Uint32 ssId,
                                   SectionHandle * handle)
{
  Ss_DROP_TRIG_IMPL_REQ& ss = ssFind<Ss_DROP_TRIG_IMPL_REQ>(ssId);

  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_DROP_TRIG_IMPL_REQ,
                      signal, DropTrigImplReq::SignalLength, JBB, handle);
}

void
LocalProxy::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  const DropTrigImplConf* conf = (const DropTrigImplConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_DROP_TRIG_IMPL_REQ& ss = ssFind<Ss_DROP_TRIG_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void
LocalProxy::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  const DropTrigImplRef* ref = (const DropTrigImplRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_DROP_TRIG_IMPL_REQ& ss = ssFind<Ss_DROP_TRIG_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
LocalProxy::sendDROP_TRIG_IMPL_CONF(Signal* signal, Uint32 ssId)
{
  Ss_DROP_TRIG_IMPL_REQ& ss = ssFind<Ss_DROP_TRIG_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    DropTrigImplConf* conf = (DropTrigImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    conf->triggerId = ss.m_req.triggerId;
    sendSignal(dictRef, GSN_DROP_TRIG_IMPL_CONF,
               signal, DropTrigImplConf::SignalLength, JBB);
  } else {
    DropTrigImplRef* ref = (DropTrigImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->triggerId = ss.m_req.triggerId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_DROP_TRIG_IMPL_REF,
               signal, DropTrigImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_DROP_TRIG_IMPL_REQ>(ssId);
}

// GSN_DBINFO_SCANREQ

static Uint32
switchRef(Uint32 block, Uint32 instance, Uint32 node)
{
  const Uint32 ref = numberToRef(block, instance,  node);
#ifdef DBINFO_SCAN_TRACE
  ndbout_c("Dbinfo::LocalProxy: switching to %s(%d) in node %d, ref: 0x%.8x",
           getBlockName(block, "<unknown>"), instance, node, ref);
#endif
  return ref;
}


bool
LocalProxy::find_next(Ndbinfo::ScanCursor* cursor) const
{
  const Uint32 node = refToNode(cursor->currRef);
  const Uint32 block = refToMain(cursor->currRef);
  Uint32 instance = refToInstance(cursor->currRef);

  ndbrequire(node == getOwnNodeId());
  ndbrequire(block == number());


  Uint32 worker = (instance > 0) ? workerIndex(instance) + 1 : 0;

  if (worker < c_workers)
  {
    jam();
    cursor->currRef = switchRef(block, workerInstance(worker), node);
    return true;
  }

  cursor->currRef = numberToRef(block, node);
  return false;
}



void
LocalProxy::execDBINFO_SCANREQ(Signal* signal)
{
  jamEntry();
  const DbinfoScanReq* req = (const DbinfoScanReq*) signal->getDataPtr();
  Uint32 signal_length = signal->getLength();
  ndbrequire(signal_length == DbinfoScanReq::SignalLength+req->cursor_sz);

  Ndbinfo::ScanCursor* cursor =
    (Ndbinfo::ScanCursor*)DbinfoScan::getCursorPtr(req);

  if (Ndbinfo::ScanCursor::getHasMoreData(cursor->flags) &&
      cursor->saveCurrRef)
  {
    jam();
    /* Continue in the saved block ref */
    cursor->currRef = cursor->saveCurrRef;
    cursor->saveCurrRef = 0;

    // Set this block as sender and remember original sender
    cursor->saveSenderRef = cursor->senderRef;
    cursor->senderRef = reference();

    sendSignal(cursor->currRef, GSN_DBINFO_SCANREQ,
               signal, signal_length, JBB);
    return;
  }

  Ndbinfo::ScanCursor::setHasMoreData(cursor->flags, false);

  if (find_next(cursor))
  {
    jam();
    ndbrequire(cursor->currRef);
    ndbrequire(cursor->saveCurrRef == 0);

    // Set this block as sender and remember original sender
    cursor->saveSenderRef = cursor->senderRef;
    cursor->senderRef = reference();

    sendSignal(cursor->currRef, GSN_DBINFO_SCANREQ,
               signal, signal_length, JBB);
    return;
  }

  /* Scan is done, send SCANCONF back to caller  */
  ndbrequire(cursor->saveSenderRef == 0);

  ndbrequire(cursor->currRef);
  ndbrequire(cursor->saveCurrRef == 0);

  ndbrequire(refToInstance(cursor->currRef) == 0);
  sendSignal(cursor->senderRef, GSN_DBINFO_SCANCONF, signal, signal_length, JBB);
  return;
}

void
LocalProxy::execDBINFO_SCANCONF(Signal* signal)
{
  jamEntry();
  const DbinfoScanConf* conf = (const DbinfoScanConf*)signal->getDataPtr();
  Uint32 signal_length = signal->getLength();
  ndbrequire(signal_length == DbinfoScanConf::SignalLength+conf->cursor_sz);

  Ndbinfo::ScanCursor* cursor =
    (Ndbinfo::ScanCursor*)DbinfoScan::getCursorPtr(conf);

  if (Ndbinfo::ScanCursor::getHasMoreData(cursor->flags))
  {
    /* The instance has more data and want to continue */
    jam();

    /* Swap back saved senderRef */
    const Uint32 senderRef = cursor->senderRef = cursor->saveSenderRef;
    cursor->saveSenderRef = 0;

    /* Save currRef to continue with same instance again */
    cursor->saveCurrRef = cursor->currRef;
    cursor->currRef = reference();

    sendSignal(senderRef, GSN_DBINFO_SCANCONF, signal, signal_length, JBB);
    return;
  }

  if (conf->returnedRows)
  {
    jam();
    /*
       The instance has no more data, but it has sent rows
       to the API which need to be CONFed
    */

    /* Swap back saved senderRef */
    const Uint32 senderRef = cursor->senderRef = cursor->saveSenderRef;
    cursor->saveSenderRef = 0;

    if (find_next(cursor))
    {
      /*
        There is another instance to continue in - signal 'more data'
        and setup saveCurrRef to continue in that instance
      */
      jam();
      Ndbinfo::ScanCursor::setHasMoreData(cursor->flags, true);

      cursor->saveCurrRef = cursor->currRef;
      cursor->currRef = reference();
    }
    else
    {
      /* There was no more instances to continue in */
      ndbrequire(Ndbinfo::ScanCursor::getHasMoreData(cursor->flags) == false);

      ndbrequire(cursor->currRef == reference());
      cursor->saveCurrRef = 0;
    }

    sendSignal(senderRef, GSN_DBINFO_SCANCONF, signal, signal_length, JBB);
    return;
  }


  /* The underlying block reported completed, find next if any */
  if (find_next(cursor))
  {
    jam();

    ndbrequire(cursor->senderRef == reference());
    ndbrequire(cursor->saveSenderRef); // Should already be set

    ndbrequire(cursor->saveCurrRef == 0);

    sendSignal(cursor->currRef, GSN_DBINFO_SCANREQ,
               signal, signal_length, JBB);
    return;
  }

  /* Scan in this block and its instances are completed */

  /* Swap back saved senderRef */
  const Uint32 senderRef = cursor->senderRef = cursor->saveSenderRef;
  cursor->saveSenderRef = 0;

  ndbrequire(cursor->currRef);
  ndbrequire(cursor->saveCurrRef == 0);

  sendSignal(senderRef, GSN_DBINFO_SCANCONF, signal, signal_length, JBB);
  return;
}

// GSN_SYNC_REQ

void
LocalProxy::execSYNC_REQ(Signal* signal)
{
  Ss_SYNC_REQ& ss = ssSeize<Ss_SYNC_REQ>();

  ss.m_req = * CAST_CONSTPTR(SyncReq, signal->getDataPtr());

  sendREQ(signal, ss);
}

void
LocalProxy::sendSYNC_REQ(Signal* signal, Uint32 ssId,
                         SectionHandle* handle)
{
  Ss_SYNC_REQ& ss = ssFind<Ss_SYNC_REQ>(ssId);

  SyncReq * req = CAST_PTR(SyncReq, signal->getDataPtrSend());
  req->senderRef = reference();
  req->senderData = ssId;
  req->prio = ss.m_req.prio;

  sendSignalNoRelease(workerRef(ss.m_worker), GSN_SYNC_REQ,
                      signal, SyncReq::SignalLength,
                      JobBufferLevel(ss.m_req.prio), handle);
}

void
LocalProxy::execSYNC_REF(Signal* signal)
{
  SyncRef ref = * CAST_CONSTPTR(SyncRef, signal->getDataPtr());
  Ss_SYNC_REQ& ss = ssFind<Ss_SYNC_REQ>(ref.senderData);

  recvREF(signal, ss, ref.errorCode);
}

void
LocalProxy::execSYNC_CONF(Signal* signal)
{
  SyncConf conf = * CAST_CONSTPTR(SyncConf, signal->getDataPtr());
  Ss_SYNC_REQ& ss = ssFind<Ss_SYNC_REQ>(conf.senderData);

  recvCONF(signal, ss);
}

void
LocalProxy::sendSYNC_CONF(Signal* signal, Uint32 ssId)
{
  Ss_SYNC_REQ& ss = ssFind<Ss_SYNC_REQ>(ssId);

  if (!lastReply(ss))
    return;

  /**
   * SimulatedBlock::execSYNC_REQ will reply
   */
  if (ss.m_error == 0)
  {
    jam();
    SyncConf * conf = CAST_PTR(SyncConf, signal->getDataPtrSend());
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;

    Uint32 prio = ss.m_req.prio;
    sendSignal(ss.m_req.senderRef, GSN_SYNC_CONF, signal,
               SyncConf::SignalLength,
               JobBufferLevel(prio));
  }
  else
  {
    jam();
    SyncRef * ref = CAST_PTR(SyncRef, signal->getDataPtrSend());
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;

    Uint32 prio = ss.m_req.prio;
    sendSignal(ss.m_req.senderRef, GSN_SYNC_REF, signal,
               SyncRef::SignalLength,
               JobBufferLevel(prio));
  }
  ssRelease<Ss_SYNC_REQ>(ssId);
}

void
LocalProxy::execSYNC_PATH_REQ(Signal* signal)
{
  SyncPathReq* req = CAST_PTR(SyncPathReq, signal->getDataPtrSend());
  req->count *= c_workers;

  for (Uint32 i = 0; i < c_workers; i++)
  {
    jam();
    Uint32 ref = numberToRef(number(), workerInstance(i), getOwnNodeId());
    sendSignal(ref, GSN_SYNC_PATH_REQ, signal,
               signal->getLength(),
               JobBufferLevel(req->prio));
  }
}

// GSN_API_FAILREQ

void
LocalProxy::execAPI_FAILREQ(Signal* signal)
{
  Uint32 nodeId = signal->theData[0];
  Ss_API_FAILREQ& ss = ssSeize<Ss_API_FAILREQ>(nodeId);

  ss.m_ref = signal->theData[1];
  sendREQ(signal, ss);
}

void
LocalProxy::sendAPI_FAILREQ(Signal* signal, Uint32 ssId, SectionHandle*)
{
  Ss_API_FAILREQ& ss = ssFind<Ss_API_FAILREQ>(ssId);

  signal->theData[0] = ssId;
  signal->theData[1] = reference();
  sendSignal(workerRef(ss.m_worker), GSN_API_FAILREQ,
             signal, 2, JBB);
}

void
LocalProxy::execAPI_FAILCONF(Signal* signal)
{
  Uint32 nodeId = signal->theData[0];
  Ss_API_FAILREQ& ss = ssFind<Ss_API_FAILREQ>(nodeId);
  recvCONF(signal, ss);
}

void
LocalProxy::sendAPI_FAILCONF(Signal* signal, Uint32 ssId)
{
  Ss_API_FAILREQ& ss = ssFind<Ss_API_FAILREQ>(ssId);

  if (!lastReply(ss))
    return;

  signal->theData[0] = ssId;
  signal->theData[1] = reference();
  sendSignal(ss.m_ref, GSN_API_FAILCONF,
             signal, 2, JBB);

  ssRelease<Ss_API_FAILREQ>(ssId);
}

BLOCK_FUNCTIONS(LocalProxy)
