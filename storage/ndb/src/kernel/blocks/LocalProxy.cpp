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
  SimulatedBlock(blockNumber, ctx)
{
  BLOCK_CONSTRUCTOR(LocalProxy);

  ndbrequire(instance() == 0); // this is main block
  c_workers = 0;
  c_threads = 0;
  Uint32 i;
  for (i = 0; i < MaxWorkers; i++)
    c_worker[i] = 0;

  c_ssIdSeq = 0;

  // GSN_READ_CONFIG_REQ
  addRecSignal(GSN_READ_CONFIG_REQ, &LocalProxy::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_READ_CONFIG_CONF, &LocalProxy::execREAD_CONFIG_CONF, true);
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

// GSN_READ_CONFIG_REQ

void
LocalProxy::execREAD_CONFIG_REQ(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = ssSeize<Ss_READ_CONFIG_REQ>();

  const ReadConfigReq* req = (const ReadConfigReq*)signal->getDataPtr();
  ss.m_req = *req;
  ndbrequire(ss.m_req.noOfParameters == 0);

  const Uint32 workers = globalData.ndbMtLqhWorkers;
  const Uint32 threads = globalData.ndbMtLqhThreads;

  Uint32 i;
  for (i = 0; i < workers; i++) {
    const Uint32 instanceNo = 1 + i;
    SimulatedBlock* worker = newWorker(instanceNo);
    ndbrequire(worker->instance() == instanceNo);
    ndbrequire(this->getInstance(instanceNo) == worker);
    c_worker[i] = worker;

    add_lqh_worker_thr_map(number(), instanceNo);
  }

  // set after instances are created (sendpacked)
  c_workers = workers;
  c_threads = threads;

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

BLOCK_FUNCTIONS(LocalProxy)
