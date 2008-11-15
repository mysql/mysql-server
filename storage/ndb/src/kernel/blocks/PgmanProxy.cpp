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

#include "PgmanProxy.hpp"
#include "pgman.hpp"
#include <signaldata/DataFileOrd.hpp>

PgmanProxy::PgmanProxy(Block_context& ctx) :
  LocalProxy(PGMAN, ctx)
{
  c_extraWorkers = 1;
}

PgmanProxy::~PgmanProxy()
{
}

SimulatedBlock*
PgmanProxy::newWorker(Uint32 instanceNo)
{
  return new Pgman(m_ctx, instanceNo);
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
  Pgman* worker = (Pgman*)extraWorkerBlock();
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
  Pgman* worker = (Pgman*)extraWorkerBlock();
  Page_cache_client pgman(block, worker);
  pgman.update_lsn(key, lsn);
}

int
PgmanProxy::drop_page(Page_cache_client& caller,
                      Local_key key, Uint32 page_id)
{
  ndbrequire(blockToInstance(caller.m_block) == 0);
  SimulatedBlock* block = globalData.getBlock(caller.m_block);
  Pgman* worker = (Pgman*)extraWorkerBlock();
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
PgmanProxy::create_data_file(Signal* signal)
{
  Pgman* worker = (Pgman*)extraWorkerBlock();
  Uint32 ret = worker->create_data_file();
  Uint32 i;
  for (i = 0; i < c_lqhWorkers; i++) {
    jam();
    send_data_file_ord(signal, i, ret,
                       DataFileOrd::CreateDataFile);
  }
  return ret;
}

Uint32
PgmanProxy::alloc_data_file(Signal* signal, Uint32 file_no)
{
  Pgman* worker = (Pgman*)extraWorkerBlock();
  Uint32 ret = worker->alloc_data_file(file_no);
  Uint32 i;
  for (i = 0; i < c_lqhWorkers; i++) {
    jam();
    send_data_file_ord(signal, i, ret,
                       DataFileOrd::AllocDataFile, file_no);
  }
  return ret;
}

void
PgmanProxy::map_file_no(Signal* signal, Uint32 file_no, Uint32 fd)
{
  Pgman* worker = (Pgman*)extraWorkerBlock();
  worker->map_file_no(file_no, fd);
  Uint32 i;
  for (i = 0; i < c_lqhWorkers; i++) {
    jam();
    send_data_file_ord(signal, i, ~(Uint32)0,
                       DataFileOrd::MapFileNo, file_no, fd);
  }
}

void
PgmanProxy::free_data_file(Signal* signal, Uint32 file_no, Uint32 fd)
{
  Pgman* worker = (Pgman*)extraWorkerBlock();
  worker->free_data_file(file_no, fd);
  Uint32 i;
  for (i = 0; i < c_lqhWorkers; i++) {
    jam();
    send_data_file_ord(signal, i, ~(Uint32)0,
                       DataFileOrd::FreeDataFile, file_no, fd);
  }
}

void
PgmanProxy::send_data_file_ord(Signal* signal, Uint32 i, Uint32 ret,
                               Uint32 cmd, Uint32 file_no, Uint32 fd)
{
  DataFileOrd* ord = (DataFileOrd*)signal->getDataPtrSend();
  ord->ret = ret;
  ord->cmd = cmd;
  ord->file_no = file_no;
  ord->fd = fd;
  sendSignal(workerRef(i), GSN_DATA_FILE_ORD,
             signal, DataFileOrd::SignalLength, JBB);
}

BLOCK_FUNCTIONS(PgmanProxy)
