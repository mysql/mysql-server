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

static NDBT_ThreadFunc hugoPkDelete;

struct ThrInput {
  const NdbDictionary::Table* pTab;
  int records;
  int batch;
  int stats;
};

struct ThrOutput {
  NDBT_Stats latency;
};

int main(int argc, const char** argv){
  ndb_init();

  int _records = 0;
  int _loops = 1;
  int _threads = 1;
  int _stats = 0;
  int _batch = 1;
  const char* _tabname = NULL;
  int _help = 0;
  
  struct getargs args[] = {
    { "loops", 'l', arg_integer, &_loops, "number of times to run this program(0=infinite loop)", "loops" },
    { "threads", 't', arg_integer, &_threads, "number of threads (default 1)", "threads" },
    { "stats", 's', arg_flag, &_stats, "report latency per batch", "stats" },
    //    { "batch", 'b', arg_integer, &_batch, "batch value", "batch" },
    { "records", 'r', arg_integer, &_records, "Number of records", "records" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program will delete all records in a table using PK \n";
  
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
  

  Ndb MyNdb(&con, "TEST_DB" );

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
  if (ths.connect(&con, "TEST_DB") == -1) {
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

    ths.set_func(hugoPkDelete);
    ths.start();
    ths.stop();

    if (ths.get_err())
      NDBT_ProgramExit(NDBT_FAILED);

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

  return NDBT_ProgramExit(NDBT_OK);
}

static void hugoPkDelete(NDBT_Thread& thr)
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
  ret = hugoTrans.pkDelRecords(thr.get_ndb(),
                               input->records,
                               input->batch);
  if (ret != 0)
    thr.set_err(ret);
}
