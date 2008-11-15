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

// wl4391_todo replace unsafe calls by signals

Uint32
PgmanProxy::create_data_file()
{
  Uint32 ret = RNIL;
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Pgman* worker = (Pgman*)workerBlock(i);
    Page_cache_client pgman(this, worker);
    Uint32 ret2 = pgman.create_data_file();
    ndbrequire(i == 0 || ret == ret2);
    ret = ret2;
  }
  return ret;
}

Uint32
PgmanProxy::alloc_data_file(Uint32 file_no)
{
  Uint32 ret = RNIL;
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Pgman* worker = (Pgman*)workerBlock(i);
    Page_cache_client pgman(this, worker);
    Uint32 ret2 = pgman.alloc_data_file(file_no);
    ndbrequire(i == 0 || ret == ret2);
    ret = ret2;
  }
  return ret;
}

void
PgmanProxy::map_file_no(Uint32 file_no, Uint32 fd)
{
  Uint32 ret = RNIL;
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Pgman* worker = (Pgman*)workerBlock(i);
    Page_cache_client pgman(this, worker);
    pgman.map_file_no(file_no, fd);
  }
}

void
PgmanProxy::free_data_file(Uint32 file_no, Uint32 fd)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Pgman* worker = (Pgman*)workerBlock(i);
    Page_cache_client pgman(this, worker);
    pgman.free_data_file(file_no, fd);
  }
}

BLOCK_FUNCTIONS(PgmanProxy)
