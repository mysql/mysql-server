/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbTick.h>
#include <HugoQueries.hpp>
#include <HugoQueryBuilder.hpp>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include "my_alloc.h"

int _verbose = 1;
int _help = 0;
int _batch = 128;
int _records = 1000;
int _loops = 100;
int _loops_per_query = 100;
int _depth = 4;
unsigned int _seed = 0;
static const char *_options = "";
static const char *_db = "TEST_DB";

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::mgmd_host,
    NdbStdOpt::connectstring,
    NdbStdOpt::ndb_nodeid,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::connect_retries,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    NDB_STD_OPT_DEBUG{"database", 'd', "Database", &_db, &_db, 0, GET_STR,
                      REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"options", 'o', "comma separated list of options", &_options, &_options, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"loops", 'l', "Loops", &_loops, 0, 0, GET_INT, REQUIRED_ARG, _loops, 0, 0,
     0, 0, 0},
    {"verbose", 'v', "verbosity", &_verbose, 0, 0, GET_INT, REQUIRED_ARG,
     _verbose, 0, 0, 0, 0, 0},
    {"loops_per_query", 'q', "Recreate query each #loops", &_loops_per_query, 0,
     0, GET_INT, REQUIRED_ARG, _loops_per_query, 0, 0, 0, 0, 0},
    {"batch", 'b', "Batch size (for lookups)", &_batch, 0, 0, GET_INT,
     REQUIRED_ARG, _batch, 0, 0, 0, 0, 0},
    {"records", 'r', "Records (for lookups)", &_records, 0, 0, GET_INT,
     REQUIRED_ARG, _records, 0, 0, 0, 0, 0},
    {"join-depth", 'j', "Join depth", &_depth, 0, 0, GET_INT, REQUIRED_ARG,
     _depth, 0, 0, 0, 0, 0},
    {"seed", NDB_OPT_NOSHORT, "Random seed", &_seed, &_seed, 0, GET_UINT,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static void short_usage_sub(void) { ndb_short_usage_sub(NULL); }

static void usage_extra() {
  char desc[] = "This run random joins on table-list\n";
  puts(desc);
}

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage_extra);
  if (opts.handle_options()) return -1;

  // Connect to Ndb
  Ndb_cluster_connection con;
  con.configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con.connect(12, 5, 1) != 0) {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (con.wait_until_ready(30, 0) < 0) {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _db);

  if (MyNdb.init() != 0) {
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Vector<const NdbDictionary::Table *> tables;
  for (int i = 0; i < argc; i++) {
    const char *_tabname = argv[i];
    // Check if table exists in db
    const NdbDictionary::Table *pTab =
        NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
    if (pTab == NULL) {
      ndbout << " Table " << _tabname << " does not exist!" << endl;
      return NDBT_ProgramExit(NDBT_WRONGARGS);
    } else {
      ndbout << " Discovered " << _tabname << endl;
    }
    tables.push_back(pTab);
  }
  tables.push_back(0);

  HugoQueryBuilder::OptionMask mask = 0;
  struct {
    const char *name;
    HugoQueryBuilder::QueryOption option;
  } _ops[] = {{"lookup", HugoQueryBuilder::O_LOOKUP},
              {"scan", HugoQueryBuilder::O_SCAN},
              {"pk", HugoQueryBuilder::O_PK_INDEX},
              {"uk", HugoQueryBuilder::O_UNIQUE_INDEX},
              {"oi", HugoQueryBuilder::O_ORDERED_INDEX},
              {"ts", HugoQueryBuilder::O_TABLE_SCAN},

              // end-marker
              {0, HugoQueryBuilder::O_LOOKUP}};

  Vector<BaseString> list;
  BaseString tmp(_options);
  tmp.split(list, ",");
  for (unsigned i = 0; i < list.size(); i++) {
    bool found = false;
    for (int o = 0; _ops[o].name != 0; o++) {
      if (native_strcasecmp(list[i].c_str(), _ops[o].name) == 0) {
        found = true;
        mask |= _ops[o].option;
        break;
      }
    }
    if (!found) {
      ndbout << "Unknown option " << list[i].c_str() << ", ignoring" << endl;
    }
  }

  if (_seed == 0) {
    _seed = (unsigned)NdbTick_CurrentMillisecond();
  }
  ndbout << "--seed=" << _seed << endl;
  srand(_seed);

  for (int i = 0; (_loops == 0) || (i < _loops);) {
    if (_verbose >= 1) {
      ndbout << "******\tbuilding new query (mask: 0x" << hex << (Uint64)mask
             << ")" << endl;
    }
    HugoQueryBuilder builder(&MyNdb, tables.getBase(), mask);
    builder.setJoinLevel(_depth);
    const NdbQueryDef *q = builder.createQuery();
    if (_verbose >= 2) {
      q->print();
      ndbout << endl;
    }

    for (int j = 0; j < _loops_per_query && ((_loops == 0) || (i < _loops));
         i++, j++) {
      int res = 0;
      HugoQueries hq(*q);
      if (q->isScanQuery()) {
        res = hq.runScanQuery(&MyNdb);
      } else {
        res = hq.runLookupQuery(&MyNdb, _records / _depth, _batch);
      }
      if (res != 0) {
        return NDBT_ProgramExit(NDBT_FAILED);
      }
      if (hq.m_rows_found.size() != 0) {
        printf("\tfound: [ ");
        for (unsigned i = 0; i < hq.m_rows_found.size(); i++) {
          printf("%u ", (Uint32)hq.m_rows_found[i]);
        }
        ndbout_c("]");
      }
    }
  }

  return NDBT_ProgramExit(NDBT_OK);
}
