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


#include <ndb_global.h>
#include <ndb_opts.h>

#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_version.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>
#include <HugoTransactions.hpp>
#include <HugoQueryBuilder.hpp>
#include <HugoQueries.hpp>
#include <NdbTick.h>

int _verbose = 1;
int _help = 0;
int _batch = 128;
int _records = 1000;
int _loops = 100;
int _loops_per_query = 100;
int _depth = 4;
unsigned int _seed = 0;
static const char * _options = "";
static const char * _db = "TEST_DB";

extern const char *load_default_groups[];
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("hugoJoin"),
  { "database", 'd', "Database",
    (uchar**) &_db, (uchar**) &_db,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "options", 'o', "comma separated list of options",
    (uchar**) &_options, (uchar**) &_options,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "loops", 'l', "Loops",
    (uchar**) &_loops, 0,
    0, GET_INT, REQUIRED_ARG, _loops, 0, 0, 0, 0, 0},
  { "verbose", 'v', "verbosity",
    (uchar**) &_verbose, 0,
    0, GET_INT, REQUIRED_ARG, _verbose, 0, 0, 0, 0, 0},
  { "loops_per_query", 'q', "Recreate query each #loops",
    (uchar**) &_loops_per_query, 0,
    0, GET_INT, REQUIRED_ARG, _loops_per_query, 0, 0, 0, 0, 0},
  { "batch", 'b', "Batch size (for lookups)",
    (uchar**) &_batch, 0,
    0, GET_INT, REQUIRED_ARG, _batch, 0, 0, 0, 0, 0},
  { "records", 'r', "Records (for lookups)",
    (uchar**) &_records, 0,
    0, GET_INT, REQUIRED_ARG, _records, 0, 0, 0, 0, 0},
  { "join-depth", 'j', "Join depth",
    (uchar**) &_depth, 0,
    0, GET_INT, REQUIRED_ARG, _depth, 0, 0, 0, 0, 0},
  { "seed", NDB_OPT_NOSHORT, "Random seed",
    (uchar **) &_seed, (uchar **) &_seed, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
}

static void usage()
{
  char desc[] =
    "This run random joins on table-list\n";
  puts(desc);
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  load_defaults("my", load_default_groups, &argc, &argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    return -1;


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

  Ndb MyNdb( &con, _db);

  if(MyNdb.init() != 0)
  {
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Vector<const NdbDictionary::Table*> tables;
  for(int i = 0; i<argc; i++)
  {
    const char* _tabname = argv[i];
    // Check if table exists in db
    const NdbDictionary::Table* pTab =
      NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
    if(pTab == NULL)
    {
      ndbout << " Table " << _tabname << " does not exist!" << endl;
      return NDBT_ProgramExit(NDBT_WRONGARGS);
    }
    else
    {
      ndbout << " Discovered " << _tabname << endl;
    }
    tables.push_back(pTab);
  }
  tables.push_back(0);

  HugoQueryBuilder::OptionMask mask = 0;
  struct { const char * name; HugoQueryBuilder::QueryOption option; }
  _ops[] = {
    { "lookup", HugoQueryBuilder::O_LOOKUP },
    { "scan", HugoQueryBuilder::O_SCAN },
    { "pk", HugoQueryBuilder::O_PK_INDEX },
    { "uk", HugoQueryBuilder::O_UNIQUE_INDEX },
    { "oi", HugoQueryBuilder::O_ORDERED_INDEX },
    { "ts", HugoQueryBuilder::O_TABLE_SCAN },

    // end-marker
    { 0, HugoQueryBuilder::O_LOOKUP }
  };

  Vector<BaseString> list;
  BaseString tmp(_options);
  tmp.split(list, ",");
  for (size_t i = 0; i<list.size(); i++)
  {
    bool found = false;
    for (int o = 0; _ops[o].name != 0; o++)
    {
      if (strcasecmp(list[i].c_str(), _ops[o].name) == 0)
      {
        found = true;
        mask |= _ops[o].option;
        break;
      }
    }
    if (!found)
    {
      ndbout << "Unknown option " << list[i].c_str() << ", ignoring" << endl;
    }
  }

  if (_seed == 0)
  {
    _seed = (unsigned)NdbTick_CurrentMillisecond();
  }
  ndbout << "--seed=" << _seed << endl;
  srand(_seed);

  for (int i = 0; (_loops == 0) || (i < _loops);)
  {
    if (_verbose >= 1)
    {
      ndbout << "******\tbuilding new query (mask: 0x" << hex 
             << (Uint64)mask << ")" << endl;
    }
    HugoQueryBuilder builder(&MyNdb, tables.getBase(), mask);
    builder.setJoinLevel(_depth);
    const NdbQueryDef * q = builder.createQuery(&MyNdb);
    if (_verbose >= 2)
    {
      q->print(); ndbout << endl;
    }

    for (int j = 0; j < _loops_per_query && ((_loops == 0) || (i < _loops));
         i++, j++)
    {
      int res = 0;
      HugoQueries hq(* q);
      if (q->isScanQuery())
      {
        res = hq.runScanQuery(&MyNdb);
      }
      else
      {
        res = hq.runLookupQuery(&MyNdb, _records, _batch);
      }
      if (res != 0)
      {
        return NDBT_ProgramExit(NDBT_FAILED);
      }
      if (hq.m_rows_found.size() != 0)
      {
        printf("\tfound: [ ");
        for (size_t i = 0; i<hq.m_rows_found.size(); i++)
        {
          printf("%u ", (Uint32)hq.m_rows_found[i]);
        }
        ndbout_c("]");
      }
    }
  }

  return NDBT_ProgramExit(NDBT_OK);
}
