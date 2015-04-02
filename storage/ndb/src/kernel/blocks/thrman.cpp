/*
   Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <EventLogger.hpp>

#define JAM_FILE_ID 440

extern EventLogger * g_eventLogger;

Thrman::Thrman(Block_context & ctx, Uint32 instanceno) :
  SimulatedBlock(THRMAN, ctx, instanceno)
{
  BLOCK_CONSTRUCTOR(Thrman);

  addRecSignal(GSN_DBINFO_SCANREQ, &Thrman::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Thrman::execCONTINUEB);
  addRecSignal(GSN_GET_CPU_USAGE_REQ, &Thrman::execGET_CPU_USAGE_REQ);
  addRecSignal(GSN_READ_CONFIG_REQ, &Thrman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Thrman::execSTTOR);

  current_cpu_load = 90;
}

Thrman::~Thrman()
{
}

BLOCK_FUNCTIONS(Thrman)

void Thrman::execREAD_CONFIG_REQ(Signal *signal)
{
  jamEntry();

  /* Receive signal */
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  /* Send return signal */
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal,
             ReadConfigConf::SignalLength, JBB);
}

void
Thrman::execSTTOR(Signal *signal)
{
  jamEntry();

  const Uint32 startPhase  = signal->theData[1];

  switch (startPhase) {
  case 1:
    Ndb_GetRUsage(&last_rusage);
    prev_cpu_usage_check = NdbTick_getCurrentTicks();
    sendNextCONTINUEB(signal);
    break;
  default:
    ndbrequire(false);
  }
  sendSTTORRY(signal);
}

void
Thrman::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255; // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : THRMAN_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 6, JBB);
}

void
Thrman::execCONTINUEB(Signal *signal)
{
  ndbrequire(signal->theData[0] == ZCONTINUEB_MEASURE_CPU_USAGE);
  measure_cpu_usage();
  sendNextCONTINUEB(signal);
}

void
Thrman::sendNextCONTINUEB(Signal *signal)
{
  signal->theData[0] = ZCONTINUEB_MEASURE_CPU_USAGE;
  sendSignalWithDelay(reference(),
                      GSN_CONTINUEB,
                      signal,
                      1000,
                      1);
}

void
Thrman::measure_cpu_usage(void)
{
  struct ndb_rusage curr_rusage;
  Uint64 elapsed_micros;

  Uint64 user_micros;
  Uint64 kernel_micros;
  Uint64 total_micros;

  /**
   * Start by making a new CPU usage measurement. After that we will
   * measure how much time has passed since last measurement and from
   * this we can calculate a percentage of CPU usage that this thread
   * has had for the last second or so.
   */
  int res = Ndb_GetRUsage(&curr_rusage);
  if (res != 0)
  {
#ifdef DEBUG_CPU_USAGE
    ndbout << "res = " << -res << endl;
#endif
  }
  NDB_TICKS curr_time = NdbTick_getCurrentTicks();
  if ((curr_rusage.ru_utime == 0 && curr_rusage.ru_stime == 0) ||
      (last_rusage.ru_utime == 0 && last_rusage.ru_stime == 0))
  {
    /**
     * We lack at least one valid measurement to make any conclusions.
     * We set CPU usage to 90 as the default value.
     */
    current_cpu_load = default_cpu_load;
  }
  else
  {

    elapsed_micros = NdbTick_Elapsed(prev_cpu_usage_check,
                                     curr_time).microSec();

    user_micros = curr_rusage.ru_utime - last_rusage.ru_utime;
    kernel_micros = curr_rusage.ru_stime - last_rusage.ru_stime;
    total_micros = user_micros + kernel_micros;

    if (elapsed_micros != 0)
    {
      /* Handle errors in time measurements */
      current_cpu_load = (total_micros * 100) / elapsed_micros;
    }
    if (current_cpu_load > 100)
    {
      /* Handle errors in time measurements */
      current_cpu_load = 100;
    }
  }
#ifdef DEBUG_CPU_USAGE
  if (current_cpu_load != 0)
  {
    ndbout << "CPU usage is now " << current_cpu_load << "%";
    ndbout << " in instance " << instance() << endl;
  }
#endif
  last_rusage = curr_rusage;
  prev_cpu_usage_check = curr_time;
}

void
Thrman::execGET_CPU_USAGE_REQ(Signal *signal)
{
  signal->theData[0] = current_cpu_load;
}

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
    Ndb_GetRUsage(&os_rusage);
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

