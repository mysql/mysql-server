/*
   Copyright (C) 2003-2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <ndb_global.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NDBT_Thread.hpp>
#include <NDBT_Stats.hpp>
#include <NdbSleep.h>
#include <getarg.h>

#include <HugoTransactions.hpp>

static NDBT_ThreadFunc hugoPkUpdate;

struct ThrInput {
  const NdbDictionary::Table* pTab;
  int records;
  int batch;
  int stats;
};

struct ThrOutput {
  NDBT_Stats latency;
};

static int _refresh = 0;

int main(int argc, const char** argv){
  ndb_init();

  int _records = 0;
  int _loops = 1;
  int _threads = 1;
  int _stats = 0;
  int _abort = 0;
  int _batch = 1;
  const char* _tabname = NULL, *db = 0;
  int _help = 0;

  struct getargs args[] = {
    { "aborts", 'a', arg_integer, &_abort, "percent of transactions that are aborted", "abort%" },
    { "loops", 'l', arg_integer, &_loops, "number of times to run this program(0=infinite loop)", "loops" },
    { "threads", 't', arg_integer, &_threads, "number of threads (default 1)", "threads" },
    { "stats", 's', arg_flag, &_stats, "report latency per batch", "stats" },
    //    { "batch", 'b', arg_integer, &_batch, "batch value", "batch" },
    { "records", 'r', arg_integer, &_records, "Number of records", "records" },
    { "usage", '?', arg_flag, &_help, "Print help", "" },
    { "database", 'd', arg_string, &db, "Database", "" },
    { "refresh", 0, arg_flag, &_refresh, "refresh record rather than update them", "" }

  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program will update all records in a table using PK\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL || _records == 0 || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

  // Connect to Ndb
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  Ndb MyNdb( &con, db ? db : "TEST_DB" );

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Check if table exists in db
  const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
  if(pTab == NULL){
    ndbout << " Table " << _tabname << " does not exist!" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  // threads
  NDBT_ThreadSet ths(_threads);

  // create Ndb object for each thread
  if (ths.connect(&con, db ? db : "TEST_DB") == -1) {
    ndbout << "connect failed: err=" << ths.get_err() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // input is options
  ThrInput input;
  ths.set_input(&input);
  input.pTab = pTab;
  input.records = _records;
  input.batch = _batch;
  input.stats = _stats;

  // output is stats
  ThrOutput output;
  ths.set_output<ThrOutput>();

  int i = 0;
  while (i < _loops || _loops == 0) {
    ndbout << i << ": ";

    ths.set_func(hugoPkUpdate);
    ths.start();
    ths.stop();

    if (ths.get_err())
    {
      ths.disconnect();
      NDBT_ProgramExit(NDBT_FAILED);
    }

    if (_stats) {
      NDBT_Stats latency;

      // add stats from each thread
      int n;
      for (n = 0; n < ths.get_count(); n++) {
        NDBT_Thread& thr = ths.get_thread(n);
        ThrOutput* output = (ThrOutput*)thr.get_output();
        latency += output->latency;
      }

      ndbout
        << "latency per batch (us): "
        << " samples=" << latency.getCount()
        << " min=" << (int)latency.getMin()
        << " max=" << (int)latency.getMax()
        << " mean=" << (int)latency.getMean()
        << " stddev=" << (int)latency.getStddev()
        << endl;
    }
    i++;
  }

  ths.disconnect();

  return NDBT_ProgramExit(NDBT_OK);
}

static void hugoPkUpdate(NDBT_Thread& thr)
{
  const ThrInput* input = (const ThrInput*)thr.get_input();
  ThrOutput* output = (ThrOutput*)thr.get_output();

  HugoTransactions hugoTrans(*input->pTab);
  output->latency.reset();
  if (input->stats)
    hugoTrans.setStatsLatency(&output->latency);

  NDBT_ThreadSet& ths = thr.get_thread_set();
  hugoTrans.setThrInfo(ths.get_count(), thr.get_thread_no());

  int ret;
  if (_refresh == 0)
  {
    ret = hugoTrans.pkUpdateRecords(thr.get_ndb(),
                                    input->records,
                                    input->batch);
  }
  else
  {
    ret = hugoTrans.pkRefreshRecords(thr.get_ndb(),
                                     0,
                                     input->records,
                                     input->batch);
  }
  if (ret != 0)
    thr.set_err(ret);
}
