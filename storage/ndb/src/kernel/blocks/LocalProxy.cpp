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

  // GSN_READ_CONFIG_REQ
  addRecSignal(GSN_READ_CONFIG_REQ, &LocalProxy::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_READ_CONFIG_CONF, &LocalProxy::execREAD_CONFIG_CONF, true);
}

LocalProxy::~LocalProxy()
{
  // dtor of main block deletes workers
}

// GSN_READ_CONFIG_REQ

void
LocalProxy::execREAD_CONFIG_REQ(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = c_ss_READ_CONFIG_REQ;
  ndbrequire(!ss.m_active);
  ss.m_active = true;

  const ReadConfigReq* req = (const ReadConfigReq*)signal->getDataPtr();
  ss.m_readConfigReq = *req;
  ndbrequire(ss.m_readConfigReq.noOfParameters == 0);

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
  sendREAD_CONFIG_REQ(signal, 0);
}

void
LocalProxy::sendREAD_CONFIG_REQ(Signal* signal, Uint32 i)
{
  Ss_READ_CONFIG_REQ& ss = c_ss_READ_CONFIG_REQ;

  ReadConfigReq* req = (ReadConfigReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = i;
  req->noOfParameters = 0;
  sendSignal(workerRef(i), GSN_READ_CONFIG_REQ,
             signal, ReadConfigReq::SignalLength, JBB);
  // for verification only
  ss.m_worker = i;
}

void
LocalProxy::execREAD_CONFIG_CONF(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = c_ss_READ_CONFIG_REQ;
  ndbrequire(ss.m_active);

  const ReadConfigConf* conf = (const ReadConfigConf*)signal->getDataPtr();
  ndbrequire(ss.m_worker == conf->senderData);
  if (ss.m_worker + 1 < c_workers) {
    jam();
    sendREAD_CONFIG_REQ(signal, ss.m_worker + 1);
    return;
  }

  sendREAD_CONFIG_CONF(signal);
  ss.m_active = false;
}

void
LocalProxy::sendREAD_CONFIG_CONF(Signal* signal)
{
  Ss_READ_CONFIG_REQ& ss = c_ss_READ_CONFIG_REQ;

  ReadConfigConf* conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = ss.m_readConfigReq.senderData;
  sendSignal(ss.m_readConfigReq.senderRef, GSN_READ_CONFIG_CONF,
             signal, ReadConfigConf::SignalLength, JBB);
}

BLOCK_FUNCTIONS(LocalProxy)
