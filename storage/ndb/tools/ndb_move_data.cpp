/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>
#include <NdbSleep.h>
#include <ndb_limits.h>
#include <ndb_lib_move_data.hpp>
#include "util/cstrbuf.h"

static const char* opt_dbname = "TEST_DB";
static bool opt_exclude_missing_columns = false;
static bool opt_promote_attributes = false;
static bool opt_lossy_conversions = false;
static const char* opt_staging_tries = 0;
static bool opt_drop_source = false;
static bool opt_verbose = false;
static bool opt_error_insert = false;
static bool opt_abort_on_error = false;

static char g_staging_tries_default[100];
static Ndb_move_data::Opts::Tries g_opts_tries;

static char g_source[MAX_TAB_NAME_SIZE];
static char g_sourcedb[MAX_TAB_NAME_SIZE];
static char g_sourcename[MAX_TAB_NAME_SIZE];
static char g_target[MAX_TAB_NAME_SIZE];
static char g_targetdb[MAX_TAB_NAME_SIZE];
static char g_targetname[MAX_TAB_NAME_SIZE];

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static const NdbDictionary::Table* g_sourcetab = 0;
static const NdbDictionary::Table* g_targettab = 0;

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
  err.code = ncc->get_latest_error();
  err.message = ncc->get_latest_error_msg();
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

    g_ndb = new Ndb(g_ncc, opt_dbname);
    CHK2(g_ndb->init() == 0, g_ndb->getNdbError());
    CHK2(g_ndb->waitUntilReady(30) == 0, g_ndb->getNdbError());
    g_dic = g_ndb->getDictionary();

    g_info << "connected" << endl;
  }
  while (0);
  return ret;
}

static void
dodisconnect()
{
  delete g_ndb;
  delete g_ncc;
  g_info << "disconnected" << endl;
}

static int
gettables()
{
  int ret = 0;
  do
  {
    CHK2(g_ndb->setDatabaseName(g_sourcedb) == 0, g_ndb->getNdbError());
    g_sourcetab = g_dic->getTable(g_sourcename);
    CHK2(g_sourcetab != 0, g_dic->getNdbError());

    CHK2(g_ndb->setDatabaseName(g_targetdb) == 0, g_ndb->getNdbError());
    g_targettab = g_dic->getTable(g_targetname);
    CHK2(g_targettab != 0, g_dic->getNdbError());
  }
  while (0);
  return ret;
}

static int
domove()
{
  int ret = 0;
  do
  {
    Ndb_move_data md;
    const Ndb_move_data::Stat& stat = md.get_stat();
    CHK2(md.init(g_sourcetab, g_targettab) == 0, md.get_error());

    int flags = 0;
    if (opt_abort_on_error)
      flags |= Ndb_move_data::Opts::MD_ABORT_ON_ERROR;
    if (opt_exclude_missing_columns)
      flags |= Ndb_move_data::Opts::MD_EXCLUDE_MISSING_COLUMNS;
    if (opt_promote_attributes)
      flags |= Ndb_move_data::Opts::MD_ATTRIBUTE_PROMOTION;
    if (opt_lossy_conversions)
      flags |= Ndb_move_data::Opts::MD_ATTRIBUTE_DEMOTION;
    md.set_opts_flags(flags);

    const Ndb_move_data::Opts::Tries& ot = g_opts_tries;
    int tries = 0;
    int delay = 0;
    while (1)
    {
      CHK2(ot.maxtries == 0 || tries < ot.maxtries,
           "too many temporary errors: " << tries);
      tries++;

      if (opt_error_insert)
        md.error_insert();
      if (md.move_data(g_ndb) != 0)
      {
        const Ndb_move_data::Error& error = md.get_error();
        g_err
            << "move data "
            << (error.is_temporary() ? "temporary error" : "permanent error")
            << " at try " << tries << " of " << ot.maxtries
            << " at rows moved " << stat.rows_moved
            << " total " << stat.rows_total
            << ": " << error << endl;

        CHK1(error.is_temporary());

        if (stat.rows_moved == 0) // this try
          delay *= 2;
        else
          delay /= 2;
        if (delay < ot.mindelay)
          delay = ot.mindelay;
        if (delay > ot.maxdelay)
          delay = ot.maxdelay;

        g_info << "sleep " << delay << " ms" << endl;
        NdbSleep_MilliSleep(delay); // XXX useless on last try
        continue;
      }

      g_info << "moved all " << stat.rows_total << " rows"
             << " in " << tries << " tries" << endl;
      if (opt_lossy_conversions ||
          stat.truncated) // just in case
        g_info << "truncated " << stat.truncated << " attribute values" << endl;
      break;
    }
    CHK1(ret == 0);
  }
  while (0);
  return ret;
}

static int
dodrop()
{
  int ret = 0;
  do
  {
    if (!opt_drop_source)
      break;
    CHK2(g_ndb->setDatabaseName(g_sourcedb) == 0, g_ndb->getNdbError());
    CHK2(g_dic->dropTable(g_sourcename) == 0, g_dic->getNdbError());
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
    CHK1(doconnect() == 0);
    CHK1(gettables() == 0);
    CHK1(domove() == 0);
    CHK1(dodrop() == 0);
  }
  while (0);
  dodisconnect();
  return ret;
}

static struct my_option
my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "database", 'd', "Default database of source and target tables",
    &opt_dbname, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "exclude-missing-columns", NDB_OPT_NOSHORT,
    "Ignore extra columns in source or target table",
    &opt_exclude_missing_columns, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "promote-attributes", 'A',
    "Allow attribute data to be converted to a larger type",
    &opt_promote_attributes, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "lossy-conversions", 'L',
    "Allow attribute data to be truncated when converted to a smaller type",
    &opt_lossy_conversions, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "staging-tries", NDB_OPT_NOSHORT,
    "Specify tries on temporary errors."
    " Format x[,y[,z]] where"
    " x=maxtries (0=no limit) y=mindelay(ms) z=maxdelay(ms)",
    &opt_staging_tries, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "drop-source", NDB_OPT_NOSHORT,
    "Drop source table after all rows have been moved",
    &opt_drop_source, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "verbose", 'v', "Verbose messages",
    &opt_verbose, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "error-insert", NDB_OPT_NOSHORT,
    "Insert random temporary errors (testing option)",
    &opt_error_insert, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "abort-on-error", NDB_OPT_NOSHORT,
    "dump core on permanent error in move-data library (debug option)",
    &opt_abort_on_error, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

const char*
load_default_groups[]= { "mysql_cluster", 0 };

static void
short_usage_sub(void)
{
  ndb_short_usage_sub("source target ( [db.]table )");
}

static void
usage_extra()
{
  printf("%s: move rows from source table to target table\n", my_progname);
}

static void
set_staging_tries_default()
{
  Ndb_move_data::Opts::Tries ot;
  Ndb_move_data::unparse_opts_tries(g_staging_tries_default, ot);
  opt_staging_tries = g_staging_tries_default;
}

static int
checkopts(int argc, char** argv)
{
  int ret = 0;
  do
  {
    CHK2(Ndb_move_data::parse_opts_tries(opt_staging_tries, g_opts_tries) == 0,
         "option --staging-tries has invalid value " << opt_staging_tries);

    CHK2(argc == 2, "arguments are: source target");

    CHK2(strlen(opt_dbname) < MAX_TAB_NAME_SIZE, "db name too long");
    CHK2(strlen(argv[0]) < MAX_TAB_NAME_SIZE, "source name too long");
    CHK2(strlen(argv[1]) < MAX_TAB_NAME_SIZE, "target name too long");

    require(cstrbuf_copy(g_source, argv[0]) == 0);
    require(cstrbuf_copy(g_target, argv[1]) == 0);

    const char* p;
    if ((p = strchr(g_source, '.')) == 0)
    {
      require(cstrbuf_copy(g_sourcedb, opt_dbname) == 0);
      require(cstrbuf_copy(g_sourcename, g_source) == 0);
    }
    else
    {
      require(cstrbuf_copy(g_sourcedb, {g_source, size_t(p - g_source)}) == 0);
      require(cstrbuf_copy(g_sourcename, p + 1) == 0);
    }
    if ((p = strchr(g_target, '.')) == 0)
    {
      require(cstrbuf_copy(g_targetdb, opt_dbname) == 0);
      require(cstrbuf_copy(g_targetname, g_target) == 0);
    }
    else
    {
      require(cstrbuf_copy(g_targetdb, {g_target, size_t(p - g_target)}) == 0);
      require(cstrbuf_copy(g_targetname, p + 1) == 0);
    }
  }
  while (0);
  return ret;
}

int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  int ret;

  set_staging_tries_default();
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage_extra);
  ret = opts.handle_options();
  if (ret != 0 || checkopts(argc, argv) != 0)
  {
    exit(NDBT_ProgramExit(NDBT_WRONGARGS));
  }
  setOutputLevel(opt_verbose ? 2 : 0);

  ret = doall();
  if (ret == -1)
  {
    exit(NDBT_ProgramExit(NDBT_FAILED));
  }
  exit(NDBT_ProgramExit(NDBT_OK));
}
