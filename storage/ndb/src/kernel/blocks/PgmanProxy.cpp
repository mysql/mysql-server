/*
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#include "PgmanProxy.hpp"
#include "pgman.hpp"
#include <signaldata/DataFileOrd.hpp>

#define JAM_FILE_ID 470


PgmanProxy::PgmanProxy(Block_context& ctx) :
  LocalProxy(PGMAN, ctx)
{
  // GSN_SYNC_EXTENT_PAGES_REQ
  addRecSignal(GSN_SYNC_EXTENT_PAGES_REQ,
               &PgmanProxy::execSYNC_EXTENT_PAGES_REQ);
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

// GSN_SYNC_EXTENT_PAGES_REQ
void
PgmanProxy::execSYNC_EXTENT_PAGES_REQ(Signal *signal)
{
  // Route signal on to extra PGMAN worker that handles extent pages
  // The return signal will be sent directly from there to sender
  // Same data sent, so proxy block is merely a router here.
  jamEntry();
  sendSignal(workerRef(c_workers - 1), GSN_SYNC_EXTENT_PAGES_REQ, signal,
             SyncExtentPagesReq::SignalLength, JBB);
  return;
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

  ndbrequire(sb == LGMAN);
  {
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
}

void
PgmanProxy::execRELEASE_PAGES_CONF(Signal* signal)
{
  jam();
  const ReleasePagesConf* conf = (const ReleasePagesConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  sendREQ(signal, ss);
}

void
PgmanProxy::sendEND_LCPREQ(Signal* signal, Uint32 ssId, SectionHandle* handle)
{
  jam();
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
  jam();
  const EndLcpConf* conf = (EndLcpConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  recvCONF(signal, ss);
}

void
PgmanProxy::sendEND_LCPCONF(Signal* signal, Uint32 ssId)
{
  jam();
  Ss_END_LCPREQ& ss = ssFind<Ss_END_LCPREQ>(ssId);
  BlockReference senderRef = ss.m_req.senderRef;

  if (!lastReply(ss))
  {
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

  if (ss.m_error == 0)
  {
    jam();
    EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_END_LCPCONF,
               signal, EndLcpConf::SignalLength, JBB);
  }
  else
  {
    ndbabort();
  }

  ssRelease<Ss_END_LCPREQ>(ssId);
}

// client methods

/*
 * Here caller must have instance 0.  The extra worker in our
 * thread is used.  These are extent pages.
 */

void
PgmanProxy::get_extent_page(Page_cache_client& caller,
                            Signal* signal,
                            Page_cache_client::Request& req,
                            Uint32 flags)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  pgman.get_extent_page(signal, req, flags);
  caller.m_ptr = pgman.m_ptr;
}

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
PgmanProxy::set_lsn(Page_cache_client& caller,
                    Local_key key,
                    Uint64 lsn)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  pgman.set_lsn(key, lsn);
}

void
PgmanProxy::update_lsn(Signal *signal,
                       Page_cache_client& caller,
                       Local_key key,
                       Uint64 lsn)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  pgman.update_lsn(signal, key, lsn);
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

bool
PgmanProxy::extent_pages_available(Uint32 pages_needed,
                                          Page_cache_client& caller)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  Pgman* worker = (Pgman*)workerBlock(c_workers - 1); // extraWorkerBlock();
  return worker->extent_pages_available(pages_needed);
}

BLOCK_FUNCTIONS(PgmanProxy)
