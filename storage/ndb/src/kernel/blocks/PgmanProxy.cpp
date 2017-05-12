/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "PgmanProxy.hpp"
#include "pgman.hpp"
#include <signaldata/DataFileOrd.hpp>

#define JAM_FILE_ID 470


PgmanProxy::PgmanProxy(Block_context& ctx) :
  LocalProxy(PGMAN, ctx)
{
  // GSN_LCP_FRAG_ORD
  addRecSignal(GSN_LCP_FRAG_ORD, &PgmanProxy::execLCP_FRAG_ORD);

  // GSN_END_LCPREQ
  addRecSignal(GSN_END_LCPREQ, &PgmanProxy::execEND_LCPREQ);
  addRecSignal(GSN_END_LCPCONF, &PgmanProxy::execEND_LCPCONF);
  addRecSignal(GSN_RELEASE_PAGES_CONF, &PgmanProxy::execRELEASE_PAGES_CONF);
}

PgmanProxy::~PgmanProxy()
{
}

SimulatedBlock*
PgmanProxy::newWorker(Uint32 instanceNo)
{
  return new Pgman(m_ctx, instanceNo);
}

// GSN_LCP_FRAG_ORD

void
PgmanProxy::execLCP_FRAG_ORD(Signal* signal)
{
  const LcpFragOrd* req = (const LcpFragOrd*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_LCP_FRAG_ORD& ss = ssSeize<Ss_LCP_FRAG_ORD>(ssId);
  ss.m_req = *req;
  sendREQ(signal, ss);
  ssRelease<Ss_LCP_FRAG_ORD>(ssId);
}

void
PgmanProxy::sendLCP_FRAG_ORD(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_LCP_FRAG_ORD& ss = ssFind<Ss_LCP_FRAG_ORD>(ssId);
  LcpFragOrd* req = (LcpFragOrd*)signal->getDataPtrSend();
  *req = ss.m_req;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_LCP_FRAG_ORD,
                      signal, LcpFragOrd::SignalLength, JBB, handle);
}

// GSN_END_LCPREQ

void
PgmanProxy::execEND_LCPREQ(Signal* signal)
{
  const EndLcpReq* req = (const EndLcpReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_END_LCPREQ& ss = ssSeize<Ss_END_LCPREQ>(ssId);
  ss.m_req = *req;

  const Uint32 sb = refToBlock(ss.m_req.senderRef);
  ndbrequire(sb == DBLQH || sb == LGMAN);

  if (sb == LGMAN) {
    jam();
    /*
     * At end of UNDO execution.  Extra PGMAN worker was used to
     * read up TUP pages.  Release these pages now.
     */
    ReleasePagesReq* req = (ReleasePagesReq*)signal->getDataPtrSend();
    req->senderData = ssId;
    req->senderRef = reference();
    req->requestType = ReleasePagesReq::RT_RELEASE_UNLOCKED;
    req->requestData = 0;
    // Extra worker
    sendSignal(workerRef(c_workers - 1), GSN_RELEASE_PAGES_REQ,
               signal, ReleasePagesReq::SignalLength, JBB);
    return;
  }
  /**
   * Send to extra PGMAN *after* all other PGMAN has completed
   */
  sendREQ(signal, ss, /* skip last */ true);
}

void
PgmanProxy::execRELEASE_PAGES_CONF(Signal* signal)
{
  const ReleasePagesConf* conf = (const ReleasePagesConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  sendREQ(signal, ss);
}

void
PgmanProxy::sendEND_LCPREQ(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);

  EndLcpReq* req = (EndLcpReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderData = ssId;
  req->senderRef = reference();
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_END_LCPREQ,
                      signal, EndLcpReq::SignalLength, JBB, handle);
}

void
PgmanProxy::execEND_LCPCONF(Signal* signal)
{
  const EndLcpConf* conf = (EndLcpConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  recvCONF(signal, ss);
}

void
PgmanProxy::sendEND_LCPCONF(Signal* signal, Uint32 ssId)
{
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  BlockReference senderRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (!ss.m_extraLast)
  {
    jam();
    ss.m_extraLast = true;
    ss.m_worker = c_workers - 1; // send to last PGMAN
    ss.m_workerMask.set(ss.m_worker);
    SectionHandle handle(this);
    (this->*ss.m_sendREQ)(signal, ss.m_ssId, &handle);
    return;
  }

  if (ss.m_error == 0) {
    jam();
    EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_END_LCPCONF,
               signal, EndLcpConf::SignalLength, JBB);
  } else {
    ndbrequire(false);
  }

  ssRelease<Ss_END_LCPREQ>(ssId);
}

// client methods

/*
 * Here caller must have instance 0.  The extra worker in our
 * thread is used.  These are extent pages.
 */

int
PgmanProxy::get_page(Page_cache_client& caller,
                     Signal* signal,
                     Page_cache_client::Request& req, Uint32 flags)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  int ret = pgman.get_page(signal, req, flags);
  caller.m_ptr = pgman.m_ptr;
  return ret;
}

void
PgmanProxy::update_lsn(Page_cache_client& caller,
                       Local_key key, Uint64 lsn)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  pgman.update_lsn(key, lsn);
}

int
PgmanProxy::drop_page(Page_cache_client& caller,
                      Local_key key, Uint32 page_id)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  int ret = pgman.drop_page(key, page_id);
  return ret;
}

/*
 * Following contact all workers.  First the method is called
 * on extra worker.  Then DATA_FILE_ORD is sent to LQH workers.
 * The result must be same since configurations are identical.
 */

Uint32
PgmanProxy::create_data_file(Signal* signal, Uint32 version)
{
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Uint32 ret = worker->create_data_file(version);
  Uint32 i;
  for (i = 0; i < c_workers - 1; i++) {
    jam();
    send_data_file_ord(signal, i, ret, version,
                       DataFileOrd::CreateDataFile);
  }
  return ret;
}

Uint32
PgmanProxy::alloc_data_file(Signal* signal, Uint32 file_no, Uint32 version)
{
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Uint32 ret = worker->alloc_data_file(file_no, version);
  Uint32 i;
  for (i = 0; i < c_workers - 1; i++) {
    jam();
    send_data_file_ord(signal, i, ret, version,
                       DataFileOrd::AllocDataFile, file_no);
  }
  return ret;
}

void
PgmanProxy::map_file_no(Signal* signal, Uint32 file_no, Uint32 fd)
{
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  worker->map_file_no(file_no, fd);
  Uint32 i;
  for (i = 0; i < c_workers - 1; i++) {
    jam();
    send_data_file_ord(signal, i, ~(Uint32)0, 0,
                       DataFileOrd::MapFileNo, file_no, fd);
  }
}

void
PgmanProxy::free_data_file(Signal* signal, Uint32 file_no, Uint32 fd)
{
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  worker->free_data_file(file_no, fd);
  Uint32 i;
  for (i = 0; i < c_workers - 1; i++) {
    jam();
    send_data_file_ord(signal, i, ~(Uint32)0, 0,
                       DataFileOrd::FreeDataFile, file_no, fd);
  }
}

void
PgmanProxy::send_data_file_ord(Signal* signal,
                               Uint32 i,
                               Uint32 ret,
                               Uint32 version,
                               Uint32 cmd,
                               Uint32 file_no,
                               Uint32 fd)
{
  DataFileOrd* ord = (DataFileOrd*)signal->getDataPtrSend();
  ord->ret = ret;
  ord->version = version;
  ord->cmd = cmd;
  ord->file_no = file_no;
  ord->fd = fd;
  sendSignal(workerRef(i), GSN_DATA_FILE_ORD,
             signal, DataFileOrd::SignalLength, JBB);
}

BLOCK_FUNCTIONS(PgmanProxy)
