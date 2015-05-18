/* Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>
#include <NdbIndexStatImpl.hpp>
#include <ndb_rand.h>

// stats options
static const char* _dbname = 0;
static my_bool _delete = false;
static my_bool _update = false;
static my_bool _dump = false;
static int _query = 0;
static int _stats_any = 0;
// sys options
static my_bool _sys_drop = false;
static my_bool _sys_create = false;
static my_bool _sys_create_if_not_exist = false;
static my_bool _sys_create_if_not_valid = false;
static my_bool _sys_check = false;
static my_bool _sys_skip_tables = false;
static my_bool _sys_skip_events = false;
static int _sys_any = 0;
// other
static my_bool _verbose = false;
static int _loops = 1;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static Ndb* g_ndb_sys = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbIndexStat* g_is = 0;

static const char* g_tabname = 0;
static const NdbDictionary::Table* g_tab = 0;
static int g_indcount = 0;
static const char** g_indnames = 0;
static const NdbDictionary::Index** g_indlist = 0;
// current index in loop
static const char* g_indname = 0;
static const NdbDictionary::Index* g_ind = 0;

#define CHK1(b) \
  if (!(b)) { \
    ret = -1; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    ret = -1; \
    break; \
  }

static NdbError
getNdbError(Ndb_cluster_connection* ncc)
{
  NdbError err;
  err.code = g_ncc->get_latest_error();
  err.message = g_ncc->get_latest_error_msg();
  return err;
}

static int
doconnect()
{
  int ret = 0;
  do
  {
    g_ncc = new Ndb_cluster_connection(opt_ndb_connectstring);
    CHK2(g_ncc->connect(6, 5) == 0, getNdbError(g_ncc));
    CHK2(g_ncc->wait_until_ready(30, 10) == 0, getNdbError(g_ncc));

    if (!_sys_any)
    {
      g_ndb = new Ndb(g_ncc, _dbname);
      CHK2(g_ndb->init() == 0, g_ndb->getNdbError());
      CHK2(g_ndb->waitUntilReady(30) == 0, g_ndb->getNdbError());
      g_dic = g_ndb->getDictionary();
    }

    g_ndb_sys = new Ndb(g_ncc, NDB_INDEX_STAT_DB);
    CHK2(g_ndb_sys->init() == 0, g_ndb_sys->getNdbError());
    CHK2(g_ndb_sys->waitUntilReady(30) == 0, g_ndb_sys->getNdbError());

    g_is = new NdbIndexStat;
    g_info << "connected" << endl;
  }
  while (0);
  return ret;
}

static void
dodisconnect()
{
  delete g_is;
  delete g_ndb_sys;
  delete g_ndb;
  delete g_ncc;
  g_info << "disconnected" << endl;
}

static const char*
format(Uint64 us64, char* buf)
{
  Uint32 ms = (Uint32)(us64 / (Uint64)1000);
  Uint32 us = (Uint32)(us64 % (Uint64)1000);
  sprintf(buf, "%u.%03u", ms, us);
  return buf;
}

static const char*
format(double x, char* buf)
{
  sprintf(buf, "%.02f", x);
  return buf;
}

static void
show_head(const NdbIndexStat::Head& head)
{
  setOutputLevel(2);
  g_info << "table:" << g_tabname;
  g_info << " index:" << g_indname;
  g_info << " fragCount:" << head.m_fragCount;
  g_info << endl;
  g_info << "sampleVersion:" << head.m_sampleVersion;
  g_info << " loadTime:" << head.m_loadTime;
  g_info << " sampleCount:" << head.m_sampleCount;
  g_info << " keyBytes:" << head.m_keyBytes;
  g_info << endl;
  setOutputLevel(_verbose ? 2 : 0);
}

static void
show_cache_info(const char* name, const NdbIndexStat::CacheInfo& info)
{
  Uint64 us64;
  char buf[100];
  setOutputLevel(2);
  g_info << name << ":";
  g_info << " valid:" << info.m_valid;
  g_info << " sampleCount:" << info.m_sampleCount;
  g_info << " totalBytes:" << info.m_totalBytes;
  g_info << endl;
  g_info << "times in ms:";
  g_info << " save: " << format(info.m_save_time, buf);
  g_info << " sort: " << format(info.m_sort_time, buf);
  if (info.m_sampleCount != 0)
  {
    us64 = info.m_sort_time / (Uint64)info.m_sampleCount;
    g_info << " sort per sample: " << format(us64, buf);
  }
  g_info << endl;
  setOutputLevel(_verbose ? 2 : 0);
}

static void
show_cache_entry(const NdbIndexStatImpl::CacheIter& iter)
{
  setOutputLevel(2);
  const NdbPack::DataC& key = iter.m_keyData;
  const NdbPack::DataC& value = iter.m_valueData;
  char buf[8000];
  key.print(buf, sizeof(buf));
  g_info << "key:" << buf << endl;
  value.print(buf, sizeof(buf));
  g_info << "value:" << buf << endl;
  setOutputLevel(_verbose ? 2 : 0);
}

static int
doquery()
{
  int ret = 0;
  char buf[100];

  Uint8 b_lo_buffer[NdbIndexStat::BoundBufferBytes];
  Uint8 b_hi_buffer[NdbIndexStat::BoundBufferBytes];
  NdbIndexStat::Bound b_lo(g_is, b_lo_buffer);
  NdbIndexStat::Bound b_hi(g_is, b_hi_buffer);
  do
  {
    NdbIndexStat::Range r(b_lo, b_hi);
    Uint8 s_buffer[NdbIndexStat::StatBufferBytes];
    NdbIndexStat::Stat s(s_buffer);

    for (int n = 0; n < _query; n++)
    {
      g_is->reset_range(r);
      for (int i = 0; i <= 1; i++)
      {
        NdbIndexStat::Bound& b = (i == 0 ? b_lo : b_hi);

        if (ndb_rand() % 3 != 0)
        {
          if (ndb_rand() % 3 != 0)
          {
            Uint32 x = ndb_rand();
            CHK2(g_is->add_bound(b, &x) == 0, g_is->getNdbError());
          }
          else
          {
            CHK2(g_is->add_bound_null(b) == 0, g_is->getNdbError());
          }
          bool strict = (ndb_rand() % 2 == 0);
          g_is->set_bound_strict(b, strict);
        }
      }
      CHK2(ret == 0, "failed");
      CHK2(g_is->finalize_range(r) == 0, g_is->getNdbError());
      CHK2(g_is->query_stat(r, s) == 0, g_is->getNdbError());
      double rir = -1.0;
      NdbIndexStat::get_rir(s, &rir);
      g_info << "rir: " << format(rir, buf) << endl;
    }
    CHK2(ret == 0, "failed");
  }
  while (0);

  return ret;
}

static int
dostats(int i)
{
  int ret = 0;
  do
  {
    g_indname = g_indnames[i];
    g_ind = g_indlist[i];

    g_is->reset_index();
    CHK2(g_is->set_index(*g_ind, *g_tab) == 0, g_is->getNdbError());

    if (_delete)
    {
      g_info << g_indname << ": delete stats" << endl;
      if (ndb_rand() % 2 == 0)
      {
        CHK2(g_dic->deleteIndexStat(*g_ind, *g_tab) == 0, g_dic->getNdbError());
      }
      else
      {
        CHK2(g_is->delete_stat(g_ndb_sys) == 0, g_is->getNdbError());
      }
    }

    if (_update)
    {
      g_info << g_indname << ": update stats" << endl;
      if (ndb_rand() % 2 == 0)
      {
        CHK2(g_dic->updateIndexStat(*g_ind, *g_tab) == 0, g_dic->getNdbError());
      }
      else
      {
        CHK2(g_is->update_stat(g_ndb_sys) == 0, g_is->getNdbError());
      }
    }

    NdbIndexStat::Head head;
    g_is->read_head(g_ndb_sys);
    g_is->get_head(head);
    CHK2(head.m_found != -1, g_is->getNdbError());
    if (head.m_found == false)
    {
      g_info << "no stats" << endl;
      break;
    }
    show_head(head);

    g_info << "read stats" << endl;
    CHK2(g_is->read_stat(g_ndb_sys) == 0, g_is->getNdbError());
    g_is->move_cache();
    g_is->clean_cache();
    g_info << "query cache created" << endl;

    NdbIndexStat::CacheInfo infoQuery;
    g_is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
    show_cache_info("query cache", infoQuery);

    if (_dump)
    {
      NdbIndexStatImpl& impl = g_is->getImpl();
      NdbIndexStatImpl::CacheIter iter(impl);
      CHK2(impl.dump_cache_start(iter) == 0, g_is->getNdbError());
      while (impl.dump_cache_next(iter) == true)
      {
        show_cache_entry(iter);
      }
    }

    if (_query > 0)
    {
      CHK2(doquery() == 0, "failed");
    }
  }
  while (0);
  return ret;
}

static int
dostats()
{
  int ret = 0;
  do
  {
    for (int i = 0; i < g_indcount; i++)
    {
      CHK1(dostats(i) == 0);
    }
    CHK1(ret == 0);
  }
  while (0);
  return ret;
}

static int
checkobjs()
{
  int ret = 0;
  do
  {
    CHK2((g_tab = g_dic->getTable(g_tabname)) != 0,
          g_tabname << ": " << g_dic->getNdbError());

    if (g_indcount == 0)
    {
      NdbDictionary::Dictionary::List list;
      CHK2(g_dic->listIndexes(list, g_tabname) == 0, g_dic->getNdbError());
      const int count = list.count;
      g_indnames = (const char**)malloc(sizeof(char*) * count);
      CHK2(g_indnames != 0, "out of memory");
      for (int i = 0; i < count; i++)
      {
        const NdbDictionary::Dictionary::List::Element& e = list.elements[i];
        if (e.type == NdbDictionary::Object::OrderedIndex)
        {
          g_indnames[g_indcount] = strdup(e.name);
          CHK2(g_indnames[g_indcount] != 0, "out of memory");
          g_indcount++;
        }
      }
      CHK1(ret == 0);
    }
    g_indlist = (const NdbDictionary::Index**)malloc(sizeof(NdbDictionary::Index*) * g_indcount);
    CHK2(g_indlist != 0, "out of memory");
    for (int i = 0; i < g_indcount; i++)
    {
      CHK2((g_indlist[i] = g_dic->getIndex(g_indnames[i], g_tabname)) != 0,
            g_tabname << "." << g_indnames[i] << ": " << g_dic->getNdbError());
    }
  }
  while (0);
  return ret;
}

static int
dosys()
{
  int ret = 0;
  do
  {
    if (_sys_drop)
    {
      if (!_sys_skip_events)
      {
        g_info << "dropping sys events" << endl;
        CHK2(g_is->drop_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
        CHK2(g_is->check_sysevents(g_ndb_sys) == -1, "unexpected success");
        CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysEvents,
             "unexpected error: " << g_is->getNdbError());
      }

      if (!_sys_skip_tables)
      {
        g_info << "dropping all sys tables" << endl;
        CHK2(g_is->drop_systables(g_ndb_sys) == 0, g_is->getNdbError());
        CHK2(g_is->check_systables(g_ndb_sys) == -1, "unexpected success");
        CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysTables,
             "unexpected error: " << g_is->getNdbError());
      }
      g_info << "drop done" << endl;
    }

    if (_sys_create)
    {
      if (!_sys_skip_tables)
      {
        g_info << "creating all sys tables" << endl;
        CHK2(g_is->create_systables(g_ndb_sys) == 0, g_is->getNdbError());
        CHK2(g_is->check_systables(g_ndb_sys) == 0, g_is->getNdbError());
      }

      if (!_sys_skip_events)
      {
        g_info << "creating sys events" << endl;
        CHK2(g_is->create_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
        CHK2(g_is->check_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
        g_info << "create done" << endl;
      }
    }

    if (_sys_create_if_not_exist)
    {
      if (!_sys_skip_tables)
      {
        if (g_is->check_systables(g_ndb_sys) == -1)
        {
          CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysTables,
               g_is->getNdbError());
          g_info << "creating all sys tables" << endl;
          CHK2(g_is->create_systables(g_ndb_sys) == 0, g_is->getNdbError());
          CHK2(g_is->check_systables(g_ndb_sys) == 0, g_is->getNdbError());
          g_info << "create done" << endl;
        }
        else
        {
          g_info << "using existing sys tables" << endl;
        }
      }

      if (!_sys_skip_events)
      {
        if (g_is->check_sysevents(g_ndb_sys) == -1)
        {
          CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysEvents,
               g_is->getNdbError());
          g_info << "creating sys events" << endl;
          CHK2(g_is->create_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
          g_info << "create done" << endl;
        }
        else
        {
          g_info << "using existing sys events" << endl;
        }
      }
    }

    if (_sys_create_if_not_valid)
    {
      if (!_sys_skip_tables)
      {
        if (g_is->check_systables(g_ndb_sys) == -1)
        {
          if (g_is->getNdbError().code != NdbIndexStat::NoSysTables)
          {
            CHK2(g_is->getNdbError().code == NdbIndexStat::BadSysTables,
                 g_is->getNdbError());
            g_info << "dropping invalid sys tables" << endl;
            CHK2(g_is->drop_systables(g_ndb_sys) == 0, g_is->getNdbError());
            CHK2(g_is->check_systables(g_ndb_sys) == -1, "unexpected success");
            CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysTables,
                 "unexpected error: " << g_is->getNdbError());
            g_info << "drop done" << endl;
          }
          g_info << "creating all sys tables" << endl;
          CHK2(g_is->create_systables(g_ndb_sys) == 0, g_is->getNdbError());
          CHK2(g_is->check_systables(g_ndb_sys) == 0, g_is->getNdbError());
          g_info << "create done" << endl;
        }
        else
        {
          g_info << "using existing sys tables" << endl;
        }
      }
      if (!_sys_skip_events)
      {
        if (g_is->check_sysevents(g_ndb_sys) == -1)
        {
          if (g_is->getNdbError().code != NdbIndexStat::NoSysEvents)
          {
            CHK2(g_is->getNdbError().code == NdbIndexStat::BadSysEvents,
                 g_is->getNdbError());
            g_info << "dropping invalid sys events" << endl;
            CHK2(g_is->drop_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
            CHK2(g_is->check_sysevents(g_ndb_sys) == -1, "unexpected success");
            CHK2(g_is->getNdbError().code == NdbIndexStat::NoSysEvents,
                 "unexpected error: " << g_is->getNdbError());
            g_info << "drop done" << endl;
          }
          g_info << "creating sys events" << endl;
          CHK2(g_is->create_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
          CHK2(g_is->check_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
          g_info << "create done" << endl;
        }
        else
        {
          g_info << "using existing sys events" << endl;
        }
      }
    }

    if (_sys_check)
    {
      if (!_sys_skip_tables)
      {
        CHK2(g_is->check_systables(g_ndb_sys) == 0, g_is->getNdbError());
        g_info << "sys tables ok" << endl;
      }
      if (!_sys_skip_events)
      {
        CHK2(g_is->check_sysevents(g_ndb_sys) == 0, g_is->getNdbError());
        g_info << "sys events ok" << endl;
      }
    }
  }
  while (0);
  return ret;
}

static int
doall()
{
  int ret = 0;
  do
  {
    CHK2(doconnect() == 0, "connect to NDB");

    int loop = 0;
    while (++loop <= _loops)
    {
      g_info << "loop " << loop << " of " << _loops << endl;
      if (!_sys_any)
      {
        if (loop == 1)
        {
          CHK1(checkobjs() == 0);
        }
        CHK2(dostats() == 0, "at loop " << loop);
      }
      else
      {
        CHK2(dosys() == 0, "at loop " << loop);
      }
    }
    CHK1(ret == 0);
  }
  while (0);

  dodisconnect();
  return ret;
}

static struct my_option
my_long_options[] =
{
  NDB_STD_OPTS("ndb_index_stat"),
  // stats options
  { "database", 'd',
    "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "delete", NDB_OPT_NOSHORT,
    "Delete index stats of given table"
     " and stop any configured auto update",
    (uchar **)&_delete, (uchar **)&_delete, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "update", NDB_OPT_NOSHORT,
    "Update index stats of given table"
     " and restart any configured auto update",
    (uchar **)&_update, (uchar **)&_update, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "dump", NDB_OPT_NOSHORT,
    "Dump query cache",
    (uchar **)&_dump, (uchar **)&_dump, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "query", NDB_OPT_NOSHORT,
    "Perform random range queries on first key attr (must be int unsigned)",
    (uchar **)&_query, (uchar **)&_query, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  // sys options
  { "sys-drop", NDB_OPT_NOSHORT,
    "Drop any stats tables and events in NDB kernel (all stats is lost)",
    (uchar **)&_sys_drop, (uchar **)&_sys_drop, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-create", NDB_OPT_NOSHORT,
    "Create stats tables and events in NDB kernel (must not exist)",
    (uchar **)&_sys_create, (uchar **)&_sys_create, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-create-if-not-exist", NDB_OPT_NOSHORT,
    "Like --sys-create but do nothing if correct objects exist",
    (uchar **)&_sys_create_if_not_exist, (uchar **)&_sys_create_if_not_exist, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-create-if-not-valid", NDB_OPT_NOSHORT,
    "Like --sys-create-if-not-exist but first drop any invalid objects",
    (uchar **)&_sys_create_if_not_valid, (uchar **)&_sys_create_if_not_valid, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-check", NDB_OPT_NOSHORT,
    "Check that correct stats tables and events exist in NDB kernel",
    (uchar **)&_sys_check, (uchar **)&_sys_check, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-skip-tables", NDB_OPT_NOSHORT,
    "Do not apply sys options to tables",
    (uchar **)&_sys_skip_tables, (uchar **)&_sys_skip_tables, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "sys-skip-events", NDB_OPT_NOSHORT,
    "Do not apply sys options to events",
    (uchar **)&_sys_skip_events, (uchar **)&_sys_skip_events, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  // other
  { "verbose", 'v',
    "Verbose messages",
    (uchar **)&_verbose, (uchar **)&_verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "loops", NDB_OPT_NOSHORT,
    "Repeat same commands a number of times (for testing)",
    (uchar **)&_loops, (uchar **)&_loops, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 },
  { 0, 0,
    0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

const char*
load_default_groups[]= { "mysql_cluster", 0 };

static void
short_usage_sub(void)
{
  ndb_short_usage_sub("[table [index...]]");
}

static void
usage()
{
  printf("%s: ordered index stats tool and test\n", my_progname);
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static int
checkopts(int argc, char** argv)
{
  int ret = 0;
  do
  {
    _stats_any =
      (_dbname != 0) +
      (_delete != 0) +
      (_update != 0) +
      (_dump != 0) +
      (_query != 0);
    _sys_any =
      (_sys_create != 0) +
      (_sys_create_if_not_exist != 0) +
      (_sys_create_if_not_valid != 0) +
      (_sys_drop != 0) +
      ( _sys_check != 0) +
      (_sys_skip_tables != 0) +
      (_sys_skip_events != 0);
    if (!_sys_any)
    {
      if (_dbname == 0)
        _dbname = "TEST_DB";
      CHK2(argc >= 1, "stats options require table");
      g_tabname = strdup(argv[0]);
      CHK2(g_tabname != 0, "out of memory");
      g_indcount = argc - 1;
      if (g_indcount != 0)
      {
        g_indnames = (const char**)malloc(sizeof(char*) * g_indcount);
        CHK2(g_indnames != 0, "out of memory");
        for (int i = 0; i < g_indcount; i++)
        {
          g_indnames[i] = strdup(argv[1 + i]);
          CHK2(g_indnames[i] != 0, "out of memory");
        }
        CHK1(ret == 0);
      }
    }
    else
    {
      CHK2(_stats_any == 0, "cannot mix --sys options with stats options");
      CHK2(argc == 0, "--sys options take no args");
    }
  }
  while (0);
  return ret;
}

int
main(int argc, char** argv)
{
  my_progname = "ndb_index_stat";
  int ret;

  ndb_init();
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || checkopts(argc, argv) != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  setOutputLevel(_verbose ? 2 : 0);

  unsigned seed = (unsigned)time(0);
  g_info << "random seed " << seed << endl;
  ndb_srand(seed);

  ret = doall();
  if (ret == -1)
    return NDBT_ProgramExit(NDBT_FAILED);
  return NDBT_ProgramExit(NDBT_OK);
}
