/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "thrman.hpp"
#include <mt.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <NdbGetRUsage.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

Thrman::Thrman(Block_context & ctx, Uint32 instanceno) :
  SimulatedBlock(THRMAN, ctx, instanceno)
{
  BLOCK_CONSTRUCTOR(Thrman);

  addRecSignal(GSN_DBINFO_SCANREQ, &Thrman::execDBINFO_SCANREQ);
}

Thrman::~Thrman()
{
}

BLOCK_FUNCTIONS(Thrman)

void
Thrman::execDBINFO_SCANREQ(Signal* signal)
{
  jamEntry();

  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  switch(req.tableId) {
  case Ndbinfo::THREADBLOCKS_TABLEID: {
    Uint32 arr[NO_OF_BLOCKS];
    Uint32 len = mt_get_blocklist(this, arr, NDB_ARRAY_SIZE(arr));
    Uint32 pos = cursor->data[0];
    for (; ; )
    {
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(getThreadId());             // thr_no
      row.write_uint32(blockToMain(arr[pos]));     // block_number
      row.write_uint32(blockToInstance(arr[pos])); // block_instance
      ndbinfo_send_row(signal, req, row, rl);

      pos++;
      if (pos == len)
      {
        jam();
        break;
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pos);
        return;
      }
    }
    break;
  }
  case Ndbinfo::THREADSTAT_TABLEID:{
    ndb_thr_stat stat;
    mt_get_thr_stat(this, &stat);
    Ndbinfo::Row row(signal, req);
    row.write_uint32(getOwnNodeId());
    row.write_uint32(getThreadId());  // thr_no
    row.write_string(stat.name);
    row.write_uint64(stat.loop_cnt);
    row.write_uint64(stat.exec_cnt);
    row.write_uint64(stat.wait_cnt);
    row.write_uint64(stat.local_sent_prioa);
    row.write_uint64(stat.local_sent_priob);
    row.write_uint64(stat.remote_sent_prioa);
    row.write_uint64(stat.remote_sent_priob);

    row.write_uint64(stat.os_tid);
    row.write_uint64(NdbTick_CurrentMillisecond());

    struct ndb_rusage os_rusage;
    Ndb_GetRUSage(&os_rusage);
    row.write_uint64(os_rusage.ru_utime);
    row.write_uint64(os_rusage.ru_stime);
    row.write_uint64(os_rusage.ru_minflt);
    row.write_uint64(os_rusage.ru_majflt);
    row.write_uint64(os_rusage.ru_nvcsw);
    row.write_uint64(os_rusage.ru_nivcsw);
    ndbinfo_send_row(signal, req, row, rl);
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

ThrmanProxy::ThrmanProxy(Block_context & ctx) :
  LocalProxy(THRMAN, ctx)
{
}

ThrmanProxy::~ThrmanProxy()
{
}

SimulatedBlock*
ThrmanProxy::newWorker(Uint32 instanceNo)
{
  return new Thrman(m_ctx, instanceNo);
}

