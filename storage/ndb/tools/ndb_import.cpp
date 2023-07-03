/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include <ndb_global.h>
#include <ndb_opts.h>
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <ndb_rand.h>
#include "NdbImport.hpp"
#include "NdbImportUtil.hpp"
// STL
#include <string>

#include "my_alloc.h"

typedef NdbImport::OptCsv OptCsv;
typedef NdbImport::JobStatus JobStatus;
typedef NdbImport::JobStats JobStats;

static FileOutputStream g_err_out(stderr);
static NdbOut g_err(g_err_out);

#define CHK1(b) \
  if (!(b)) { \
    ret = -1; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << my_progname << ": " << e << endl; \
    ret = -1; \
    break; \
  }

#define LOG(x) \
  do { \
    g_err << x << endl; \
  } while (0)

// opts

static NdbImport::Opt g_opt;

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
  { "connections", NDB_OPT_NOSHORT,
    "Number of cluster connections to create."
    " If option --ndb-nodeid=N is given then this number of consecutive"
    " API nodes starting at N must exist",
    &g_opt.m_connections, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_connections, 0, 0, 0, 0, 0 },
  { "table", 't',
   "Name of the table where to import the data."
   "Default is the basename from the input csv file name",
   &g_opt.m_table, nullptr, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "state-dir", NDB_OPT_NOSHORT,
    "Where to write state files (t1.res etc)."
    " Default is \".\" (currect directory)",
    &g_opt.m_state_dir, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "keep-state", NDB_OPT_NOSHORT,
    "By default state files are removed when the job completes"
    " successfully, except if there were any rejects (within allowed limit)"
    " then *.rej is kept. This option keeps all state files",
    &g_opt.m_keep_state, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "stats", NDB_OPT_NOSHORT,
    "Save performance related options and internal statistics into"
    " additional state files with suffixes .sto and .stt. The files"
    " are kept also on successful completion",
    &g_opt.m_stats, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "input-type", NDB_OPT_NOSHORT,
    "Input type: csv,random"
    " (random is a test option)",
    &g_opt.m_input_type, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "input-workers", NDB_OPT_NOSHORT,
    "Number of threads processing input",
    &g_opt.m_input_workers, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_input_workers, 0, 0, 0, 0, 0 },
  { "output-type", NDB_OPT_NOSHORT,
    "Output type: ndb,null"
    " (null is a test option)",
    &g_opt.m_output_type, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "output-workers", NDB_OPT_NOSHORT,
    "Number of threads processing output or relaying db ops",
    &g_opt.m_output_workers, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_output_workers, 0, 0, 0, 0, 0 },
  { "db-workers", NDB_OPT_NOSHORT,
    "Number of threads PER datanode executing db ops",
    &g_opt.m_db_workers, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_db_workers, 0, 0, 0, 0, 0 },
  { "ignore-lines", NDB_OPT_NOSHORT,
    "Ignore given number of initial lines in input file."
    " Used to skip a non-data header."
    " To continue with an aborted job use --resume instead",
    &g_opt.m_ignore_lines, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_ignore_lines, 0, 0, 0, 0, 0 },
  { "max-rows", NDB_OPT_NOSHORT,
    "Limit number of rows proccessed."
    " Mainly a test option. Default 0 means no limit."
    " More rows may be processed",
    &g_opt.m_max_rows, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_max_rows, 0, 0, 0, 0, 0 },
  { "continue", NDB_OPT_NOSHORT,
    "If one job (e.g. CSV import) fails, continue to next job",
    &g_opt.m_continue, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "resume", NDB_OPT_NOSHORT,
    "If the job(s) are aborted due to e.g. too many rejects or"
     " too many temporary NDB errors or user interrupt,"
    " add this option to try to resume with rows not yet processed",
    &g_opt.m_resume, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "monitor", NDB_OPT_NOSHORT,
    "Periodically print status of running job if something has changed"
    " (status, rejected rows, temporary errors)."
    " Value 0 disables. Value 1 prints any change seen."
    " Higher values reduce status printing exponentially"
    " up to some pre-defined limit",
    &g_opt.m_monitor, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_monitor, 0, 0, 0, 0, 0 },
  { "ai-prefetch-sz", NDB_OPT_NOSHORT,
    "For table with an auto inc (including hidden) PK,"
    " specify number of autoincrement values"
    " that are prefetched. See mysqld",
    &g_opt.m_ai_prefetch_sz, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_ai_prefetch_sz, 0, 0, 0, 0, 0 },
  { "ai-increment", NDB_OPT_NOSHORT,
    "For table with an auto inc (including hidden) PK,"
    " specify autoincrement increment."
    " See mysqld",
    &g_opt.m_ai_increment, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_ai_increment, 0, 0, 0, 0, 0 },
  { "ai-offset", NDB_OPT_NOSHORT,
    "For table with an auto inc (including hidden) PK,"
    " specify autoincrement offset."
    " See mysqld",
    &g_opt.m_ai_offset, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_ai_offset, 0, 0, 0, 0, 0 },
  { "no-asynch", NDB_OPT_NOSHORT,
    "Run db ops as batches under single trans."
    " Used for performance comparison and does not support all features"
    " e.g. detecting individual rejected rows",
    &g_opt.m_no_asynch, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "no-hint", NDB_OPT_NOSHORT,
    "Do not use distribution key hint to select db node (TC)",
    &g_opt.m_no_hint, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "pagesize", NDB_OPT_NOSHORT,
    "Align I/O buffers to given size",
    &g_opt.m_pagesize, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_pagesize, 0, 0, 0, 0, 0 },
  { "pagecnt", NDB_OPT_NOSHORT,
    "Size of I/O buffers as multiple of pagesize."
    " CSV input worker allocates a double sized buffer",
    &g_opt.m_pagecnt, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_pagecnt, 0, 0, 0, 0, 0 },
  { "pagebuffer", NDB_OPT_NOSHORT,
    "Size of I/O buffers in bytes. Rounded up to pagesize and"
    " overrides pagecnt",
    &g_opt.m_pagebuffer, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_pagebuffer, 0, 0, 0, 0, 0 },
  { "rowbatch", NDB_OPT_NOSHORT,
    "Limit rows in row queues (0 no limit)",
    &g_opt.m_rowbatch, nullptr, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "rowbytes", NDB_OPT_NOSHORT,
    "Limit bytes in row queues (0 no limit)",
    &g_opt.m_rowbytes, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_rowbytes, 0, 0, 0, 0, 0 },
  { "opbatch", NDB_OPT_NOSHORT,
    "A db execution batch is a set of transactions and operations"
    " sent to NDB kernel."
    " This option limits NDB operations (including blob operations)"
    " in a db execution batch.  Therefore it also limits number"
    " of asynch transactions."
    " Value 0 is not valid",
    &g_opt.m_opbatch, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_opbatch, 0, 0, 0, 0, 0 },
  { "opbytes", NDB_OPT_NOSHORT,
    "Limit bytes in db execution batch (0 no limit)",
    &g_opt.m_opbytes, nullptr, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "polltimeout", NDB_OPT_NOSHORT,
    "Millisecond timeout in one poll for completed asynch transactions."
    " Polls continue until all are completed or an error has occurred",
    &g_opt.m_polltimeout, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_polltimeout, 0, 0, 0, 0, 0 },
  { "temperrors", NDB_OPT_NOSHORT,
    "Limit temporary NDB errors. Default is 0 which means that any"
    " temporary error is fatal."
    " The errors are counted per db execution batch, not per individual"
    " operations, and do not cause rows to be rejected",
    &g_opt.m_temperrors, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_temperrors, 0, 0, 0, 0, 0 },
  { "tempdelay", NDB_OPT_NOSHORT,
    "Number of milliseconds to sleep between temporary errors",
    &g_opt.m_tempdelay, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_tempdelay, 0, 0, 0, 0, 0 },
  { "rowswait", NDB_OPT_NOSHORT,
    "Number of milliseconds a worker waits for a signal that new rows"
    " can be processed",
    &g_opt.m_rowswait, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_rowswait, 0, 0, 0, 0, 0 },
  { "idlespin", NDB_OPT_NOSHORT,
    "Number of times to re-try before idlesleep",
    &g_opt.m_idlespin, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_idlespin, 0, 0, 0, 0, 0 },
  { "idlesleep", NDB_OPT_NOSHORT,
    "Number of milliseconds to sleep waiting for more to do."
    " Cause can be row queues stall (see --rowswait) or"
    " e.g. passing control between CSV workers",
    &g_opt.m_idlesleep, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_idlesleep, 0, 0, 0, 0, 0 },
  { "checkloop", NDB_OPT_NOSHORT,
    "A job and its diagnostics team periodically check for"
    " progress from lower levels. This option gives number of"
    " milliseconds to wait between such checks."
    " High values may cause data structures (rowmaps) to grow too much."
    " Low values may interfere too much with the workers",
    &g_opt.m_checkloop, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_checkloop, 0, 0, 0, 0, 0 },
  { "alloc-chunk", NDB_OPT_NOSHORT,
    "Number of free rows to alloc (seize) at a time."
    " Higher values reduce mutexing but also may reduce parallelism",
    &g_opt.m_alloc_chunk, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_alloc_chunk, 0, 0, 0, 0, 0 },
  { "rejects", NDB_OPT_NOSHORT,
    "Limit number of rejected rows (rows with permanent error) in data load."
    " Default is 0 which means that any rejected row causes a fatal error."
    " The row(s) exceeding the limit are also added to *.rej."
    " The limit is per current run (not all --resume'd runs)",
    &g_opt.m_rejects, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_rejects, 0, 0, 0, 0, 0 },
  { "fields-terminated-by", NDB_OPT_NOSHORT,
    "See MySQL LOAD DATA."
    " This and other CSV controls are scanned for escapes"
    " including \\\\,\\n,\\r,\\t",
    &g_opt.m_optcsv.m_fields_terminated_by, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-enclosed-by", NDB_OPT_NOSHORT,
    "See MySQL LOAD DATA."
     " For CSV input this is same as --fields-optionally-enclosed-by",
    &g_opt.m_optcsv.m_fields_enclosed_by, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-optionally-enclosed-by", NDB_OPT_NOSHORT,
    "See MySQL LOAD DATA",
    &g_opt.m_optcsv.m_fields_optionally_enclosed_by, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "fields-escaped-by", NDB_OPT_NOSHORT,
    "See MySQL LOAD DATA",
    &g_opt.m_optcsv.m_fields_escaped_by, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "lines-terminated-by", NDB_OPT_NOSHORT,
    "See MySQL LOAD DATA but note that default is"
    " \\n for unix and \\r\\n for windows",
    &g_opt.m_optcsv.m_lines_terminated_by, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "csvopt", NDB_OPT_NOSHORT,
    "Set some typical CSV options."
    " Useful for environments where command line quoting and escaping is hard."
    " Argument is a string of letters:"
    " d-defaults for the OS type"
    " c-fields terminated by real comma (,)"
    " q-fields optionally enclosed by double quotes (\")"
    " n-lines terminated by \\n"
    " r-lines terminated by \\r\\n",
    &g_opt.m_csvopt, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "log-level", NDB_OPT_NOSHORT,
    "Print internal log at given level (0-2 or 0-4 if debug compiled)."
    " Like --debug, this option is for developers",
    &g_opt.m_log_level, nullptr, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "abort-on-error", NDB_OPT_NOSHORT,
    "Dump core on any error, debug option",
    &g_opt.m_abort_on_error, nullptr, 0,
    GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0 },
  { "errins-type", NDB_OPT_NOSHORT,
    "Error insert type (test option, give \"list\" to list)",
    &g_opt.m_errins_type, nullptr, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "errins-delay", NDB_OPT_NOSHORT,
    "Error insert delay in milliseconds (random variation added)",
    &g_opt.m_errins_delay, nullptr, 0,
    GET_UINT, REQUIRED_ARG, g_opt.m_errins_delay, 0, 0, 0, 0, 0 },
  { "missing-ai-column", 'm',
    "Missing auto-increment column",
    &g_opt.m_missing_ai_col, nullptr, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  NdbStdOpt::end_of_options
};

static void
short_usage_sub(void)
{
  ndb_short_usage_sub("database textfile...");
  printf(
    "\n"
    "Arguments give database and files of CSV table data.\n"
    "The basename of each file specifies the table name.\n"
    "E.g. %s test foo/t1.csv foo/t2.csv loads tables\n"
    "test.t1 test.t2.\n"
    "Alternatively, the optional parameter --table can be used to\n"
    "specify the table where to import the data avoiding the need of\n"
    "csv basename/table name matching.\n"
    "\n"
    "For each job (load of one table), results, rejected rows,\n"
    "and processed row ranges are written to \"state files\" with\n"
    "suffixes .res, .rej, and .map.  By default these are removed\n"
    "when the job completes successfully with no rejects.\n"
    "See options --state-dir and --keep-state.\n"
    "\n"
    "Windows notes: File paths are shown with forward slash (/).\n"
    "Default line-terminator is \\r\\n.\n"
    "Keyboard interrupt is not implemented.\n"
    "\n",
    my_progname);
}

static void
usage()
{
  printf("%s: load data from files to tables\n", my_progname);
}

// check opts and args

// opts
static std::string g_state_dir;

// args
struct TableArg {
  std::string m_table;
  std::string m_input_file;
  std::string m_result_file;
  std::string m_reject_file;
  std::string m_rowmap_file;
  std::string m_stopt_file;
  std::string m_stats_file;
};

static TableArg* g_tablearg = 0;
static uint g_tablecnt = 0;

const char* g_reserved_extension[] = {
  ".res",
  ".rej",
  ".map",
  ".sto",
  ".stt",
  0
};

/*
 * File I/O functions in the Windows API convert "/" to "\" (says
 * MicroSoft).  MTR uses "/" and converts it to "\" when needed.
 * It does not recognize paths in ndb_import options and arguments.
 * We solve the mess by converting all "\" to "/".  Messages will
 * show the converted paths, fix later if it matters.
 */
static void
convertpath(std::string& str)
{
#ifdef _WIN32
  // std::replace() exists but following is more clear
  for (uint i = 0; i < (uint)str.size(); i++)
  {
    if (str[i] == '\\')
      str[i] = '/';
  }
#endif
}

static int
checkarg(TableArg& arg, const char* str)
{
  int ret = 0;
  do
  {
    std::string full = str;     // foo/t1.bar.csv
    convertpath(full);
    std::string base = full;    // t1.bar.csv
    std::size_t slash = full.rfind("/");
    if (slash != std::string::npos)
    {
      base = full.substr(slash + 1);
    }
    for (const char** p = g_reserved_extension; *p != 0; p++)
    {
      std::size_t pos = base.rfind(*p);
      if (pos != std::string::npos &&
          pos + strlen(*p) == base.length())
      {
        CHK2(false, full.c_str() << ": has reserved suffix: " << *p);
      }
    }
    CHK1(ret == 0);
    std::string stem = base;    // t1.bar
    std::size_t rdot = base.rfind(".");
    if (rdot != std::string::npos)
    {
      stem = base.substr(0, rdot);
    }
    if(g_opt.m_table == nullptr)
    {
      std::string table = stem;  // t1
      std::size_t ldot = stem.find(".");
      if (ldot != std::string::npos)
      {
        table = stem.substr(0, ldot);
      }
      arg.m_table = table;
    }
    arg.m_input_file = full;
    std::string path = "";
    if (strcmp(g_opt.m_state_dir, ".") != 0)
    {
      path += g_opt.m_state_dir;
      path += "/";
    }
    arg.m_result_file = path + stem + ".res";
    arg.m_reject_file = path + stem + ".rej";
    arg.m_rowmap_file = path + stem + ".map";
    arg.m_stopt_file = path + stem + ".sto";
    arg.m_stats_file = path + stem + ".stt";
  } while (0);
  return ret;
}

static int checkerrins();

static int
checkcsvopt()
{
  int ret = 0;
  for (const char* p = g_opt.m_csvopt; *p != 0; p++)
  {
    switch (*p) {
    case 'd':
      new (&g_opt.m_optcsv) OptCsv;
      break;
    case 'c':
      g_opt.m_optcsv.m_fields_terminated_by = ",";
      break;
    case 'q':
      g_opt.m_optcsv.m_fields_optionally_enclosed_by = "\"";
      break;
    case 'n':
      g_opt.m_optcsv.m_lines_terminated_by = "\\n";
      break;
    case 'r':
      g_opt.m_optcsv.m_lines_terminated_by = "\\r\\n";
      break;
    default:
      {
        char tmp[2];
        sprintf(tmp, "%c", *p);
        CHK2(false, "m_csvopt: undefined option: " << tmp);
      }
      break;
    }
    CHK1(ret == 0);
  }
  return ret;
}

static int
checkopts(int argc, char** argv)
{
  int ret = 0;
  do
  {
    CHK1(checkerrins() == 0);
    g_state_dir = g_opt.m_state_dir;
    convertpath(g_state_dir);
    g_opt.m_state_dir = g_state_dir.c_str();
    CHK2(argc >= 1, "database argument is required, use --help for help");
    g_opt.m_database = argv[0];
    argc--;
    argv++;
    g_tablecnt = argc;
    g_tablearg = new TableArg [g_tablecnt];
    if(g_opt.m_table)
    {
      g_tablearg->m_table = std::string(g_opt.m_table);
    }
    for (uint i = 0; i < g_tablecnt; i++)
    {
      CHK1(checkarg(g_tablearg[i], argv[i]) == 0);
    }
  } while (0);
  return ret;
}

// signal handlers

#ifndef _WIN32

static void
sighandler(int sig)
{
  const char* signame = "unexpected";
  switch (sig) {
  case SIGHUP:
    signame = "hangup";
    break;
  case SIGINT:
    signame = "interrupt";
    break;
  }
  LOG(my_progname << ": caught signal " << sig << " (" << signame << ")");
  LOG(my_progname << ": please wait for any jobs to stop gracefully");
  NdbImport::set_stop_all();
}

static void
setsighandler(bool on)
{
  struct sigaction sa;
  if (on)
    sa.sa_handler = sighandler;
  else
    sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
}

#else

// TODO

static void
setsighandler(bool on)
{
}

#endif

// error insert

struct Errins {
  const char* m_type;
  const char* m_desc;
};

static const Errins
g_errins_list[] = {
  { "stopjob", "stop current job (synchronous)" },
  { "stopall", "stop all jobs" },
  { "sighup", "trigger stopall via SIGHUP" },
  { "sigint", "trigger stopall via SIGINT" },
  { "bug34917498", "use small buffer when calculate nodeid for row" },
  { 0, 0 }
};

static bool
listerrins()
{
  const char* type = g_opt.m_errins_type;
  if (type == 0 || strcmp(type, "list") != 0)
    return false;
  LOG(my_progname << ": only listing error inserts");
  const Errins* list = g_errins_list;
  for (uint i = 0; list[i].m_type != 0; i++)
  {
    LOG(list[i].m_type << " - " << list[i].m_desc);
  }
  return true;
}

static int
checkerrins()
{
  int ret = 0;
  do
  {
    const char* type = g_opt.m_errins_type;
    if (type == 0)
      break;
    const Errins* list = g_errins_list;
    bool found = false;
    for (uint i = 0; list[i].m_type != 0; i++)
    {
      if (strcmp(list[i].m_type, type) == 0)
      {
        found = true;
        break;
      }
    }
    CHK2(found,  "undefined error insert: " << type);
  } while (0);
  return ret;
}

static void
doerrinsstop(NdbImport::Job& job)
{
  const char* type = g_opt.m_errins_type;
  uint delay = g_opt.m_errins_delay;
  uint ms = delay / 2 + ndb_rand() % (delay + 1);
  if (type == 0)
    return;
  if (strcmp(type, "stopjob") != 0)
    return;
  NdbSleep_MilliSleep(ms);
  job.do_stop();
}

static NdbThread* g_errins_thread = 0;

extern "C" { static void* doerrins_c(void* data); }

static void*
doerrins_c(void* data)
{
  const char* type = g_opt.m_errins_type;
  uint delay = g_opt.m_errins_delay;
  uint ms = delay / 2 + ndb_rand() % (delay + 1);
  NdbSleep_MilliSleep(ms);
  if (strcmp(type, "stopall") == 0)
  {
    NdbImport::set_stop_all();
    return 0;
  }
#ifndef _WIN32
  int pid = NdbHost_GetProcessId();
  int sig = 0;
  if (strcmp(type, "sighup") == 0)
    sig = SIGHUP;
  if (strcmp(type, "sigint") == 0)
    sig = SIGINT;
  if (sig != 0)
    ::kill(pid, sig);
#else
  // TODO
#endif
  return 0;
}

static void
doerrins()
{
  const char* type = g_opt.m_errins_type;
  if (type == 0)
    return;
  if (strcmp(type, "stopjob") == 0)
    return;
  NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_MEAN;
  uint stack_size = 64*1024;
  g_errins_thread = NdbThread_Create(
    doerrins_c, (void**)0, stack_size, "errins", prio);
  require(g_errins_thread != 0);
}

static void
clearerrins()
{
  if (g_errins_thread == 0)
    return;
  NdbThread_WaitFor(g_errins_thread, (void**)0);
  NdbThread_Destroy(&g_errins_thread);
}

// main program

static const uint g_rep_status = (1 << 0);
static const uint g_rep_resume = (1 << 1);
static const uint g_rep_stats = (1 << 2);
static const uint g_rep_reject = (1 << 3);
static const uint g_rep_temperrors = (1 << 4);
static const uint g_rep_errortexts = (1 << 5);

// call job.get_status() first
static void
doreport(NdbImport::Job& job, uint flags)
{
  require(job.m_status != JobStatus::Status_null);
  char jobname[100];
  sprintf(jobname, "job-%u", job.m_jobno);
  const uint runno = job.m_runno;
  char str_status[100] = "";
  if (flags & g_rep_status)
  {
    // including status only if job has been started
    sprintf(str_status, " [%s]", job.m_str_status);
  }
  if (runno == 0 || !(flags & g_rep_resume))
    LOG(jobname << str_status <<
        " import " << g_opt.m_database << "." << g_opt.m_table <<
        " from " << g_opt.m_input_file);
  else
    LOG(jobname << str_status <<
        " import " << g_opt.m_database << "." << g_opt.m_table <<
        " from " << g_opt.m_input_file <<
        " (resume #" << runno << ")");
  const JobStats& stats = job.m_stats;
  const uint64 rows = stats.m_rows;
  const uint64 reject = stats.m_reject;
  const uint64 new_rows = stats.m_new_rows;
  const uint64 new_reject = stats.m_new_reject;
  const uint temperrors = stats.m_temperrors;
  const std::map<uint, uint>& errormap = stats.m_errormap;
  const uint64 runtime = stats.m_runtime;
  const uint64 rowssec = stats.m_rowssec;
  if (flags & g_rep_stats)
  {
    char runtimestr[100];
    NdbImportUtil::fmt_msec_to_hhmmss(runtimestr, runtime);
    if (runno == 0)
      LOG(jobname << " imported " << rows << " rows" <<
           " in " << runtimestr <<
           " at " << rowssec << " rows/s");
    else
      LOG(jobname << " imported " << rows << " rows" <<
          " (new " << new_rows << ")" <<
           " in " << runtimestr <<
           " at " << rowssec << " rows/s");
  }
  if ((flags & g_rep_reject) &&
      reject != 0)
  {
    if (runno == 0)
      LOG(jobname << " rejected " << reject << " rows" <<
          " (limit " << g_opt.m_rejects << ")," <<
          " see " << g_opt.m_reject_file);
    else
      LOG(jobname << " rejected " << reject << " rows" <<
          " (new " << new_reject << " limit " << g_opt.m_rejects << ")," <<
          " see " << g_opt.m_reject_file);
  }
  if ((flags & g_rep_temperrors) &&
      temperrors != 0)
  {
    std::string list;
    std::map<uint, uint>::const_iterator it;
    for (it = errormap.begin(); it != errormap.end(); it++)
    {
      char buf[100];
      // showing count(code)
      sprintf(buf, " %u(%u)", it->second, it->first);
      list += buf;
    }
    LOG(jobname << " temporary errors " << temperrors <<
        list.c_str() << " (limit " << g_opt.m_temperrors << ")");
  }
  if ((flags & g_rep_errortexts) &&
      temperrors != 0)
  {
    std::map<uint, uint>::const_iterator it;
    for (it = errormap.begin(); it != errormap.end(); it++)
    {
      char buf[100];
      sprintf(buf, " %u(%u)", it->second, it->first);
      ndberror_struct error;
      error.code = it->first;
      ndberror_update(&error);
      LOG(jobname << buf << ": " << error.message);
    }
  }
  if (job.m_status == JobStatus::Status_error ||
      job.m_status == JobStatus::Status_fatal)
  {
    LOG(jobname << " " << job.get_error());
    for (uint i = 0; i < job.m_teamcnt; i++)
    {
      NdbImport::Team* team = job.m_teams[i];
      char teamname[100];
      sprintf(teamname, "%u-%s", team->m_teamno, team->get_name());
      if (team->has_error())
        LOG(jobname << " " << teamname << " " << team->get_error());
    }
  }
}

static uint
getweigth(const JobStats& stats)
{
  return stats.m_new_reject + stats.m_temperrors;
}

static void
domonitor(NdbImport::Job& job)
{
  job.get_status();
  JobStatus::Status old_status = job.m_status;
  JobStats old_stats = job.m_stats;
  const uint maxtreshold = 1000;
  uint treshold = 1;
  while (1)
  {
    NdbSleep_MilliSleep(g_opt.m_checkloop);
    job.get_status();
    JobStatus::Status status = job.m_status;
    JobStats stats = job.m_stats;
    if (job.has_error())
      break;
    if (status == JobStatus::Status_success ||
        status == JobStatus::Status_error ||
        status == JobStatus::Status_fatal)
      break;
    bool report =
      status != old_status ||
      getweigth(stats) >= getweigth(old_stats) + treshold;
    if (report)
    {
      uint flags = g_rep_status |
                   (stats.m_new_reject ? g_rep_reject : 0) |
                   g_rep_temperrors |
                   g_rep_errortexts;
      doreport(job, flags);
      old_status = status;
      old_stats = stats;
      treshold *= g_opt.m_monitor;
      if (treshold > maxtreshold)
        treshold = maxtreshold;
    }
  }
}

static void
removefile(const char* path)
{
  if (unlink(path) == -1)
  {
    LOG(my_progname << ": remove " << path << " failed: " << strerror(errno));
  }
}

static int
doimp()
{
  int ret = 0;
  uint jobs_defined = g_tablecnt;
  uint jobs_run = 0;
  uint jobs_fail = 0;
  NdbImport imp;
  if (g_tablecnt == 0)
  {
    LOG("note: no files to import specified");
  }
  do
  {
    CHK2(imp.set_opt(g_opt) == 0, "invalid options: " << imp.get_error());
    CHK2(imp.do_connect() == 0, "connect to NDB failed: " << imp.get_error());
    for (uint i = 0; i < g_tablecnt; i++)
    {
      const TableArg& arg = g_tablearg[i];
      // no parallel jobs yet so can use global g_opt
      g_opt.m_table = arg.m_table.c_str();
      g_opt.m_input_file = arg.m_input_file.c_str();
      g_opt.m_result_file = arg.m_result_file.c_str();
      g_opt.m_reject_file = arg.m_reject_file.c_str();
      g_opt.m_rowmap_file = arg.m_rowmap_file.c_str();
      g_opt.m_stopt_file = arg.m_stopt_file.c_str();
      g_opt.m_stats_file = arg.m_stats_file.c_str();
      CHK2(imp.set_opt(g_opt) == 0, "invalid options: "<< imp.get_error());
      NdbImport::Job job(imp);
      do
      {
        job.do_create();
        job.get_status();
        doreport(job, 0);
        uint tabid;
        if (job.add_table(g_opt.m_database, g_opt.m_table, tabid) == -1)
          break;
        job.set_table(tabid);
        job.do_start();
        doerrinsstop(job);
        if (g_opt.m_monitor != 0)
          domonitor(job);
        job.do_wait();
        if (job.remove_table(tabid) == -1)
        {
          break;
        }
      } while (0);
      bool imp_error = imp.has_error();
      bool job_error = job.has_error();
      job.get_status();
      uint flags = g_rep_status |
                   g_rep_resume |
                   g_rep_stats |
                   g_rep_reject |
                   g_rep_temperrors |
                   g_rep_errortexts;
      doreport(job, flags);
      if (imp_error)
      {
        LOG("global error: " << imp.get_error());
      }
      if (job.m_status == JobStatus::Status_success &&
          !g_opt.m_keep_state)
      {
        removefile(g_opt.m_result_file);
        if (job.m_stats.m_reject == 0)
          removefile(g_opt.m_reject_file);
        removefile(g_opt.m_rowmap_file);
      }
      job.do_destroy();
      jobs_run++;
      if (imp_error || job_error)
      {
        jobs_fail++;
        ret = -1;
        if (imp_error || !g_opt.m_continue)
          break;
      }
    }
    CHK1(ret == 0);
  } while (0);
  imp.do_disconnect();
  LOG("jobs summary:" <<
      " defined: " << jobs_defined <<
      " run: " << jobs_run <<
      " with success: " << jobs_run - jobs_fail <<
      " with failure: " << jobs_fail);
  return ret;
}

static int
doall()
{
  int ret = 0;
  do
  {
    setsighandler(true);
    doerrins();
    CHK1(doimp() == 0);
  } while (0);
  clearerrins();
  setsighandler(false);
  return ret;
}

static bool get_one_option(int optid, const struct my_option *opt, char *arg)
{
  bool ret = false;
  if(strcmp(opt->name, "csvopt") == 0)
    ret = (checkcsvopt() != 0);
  else
    ret = ndb_std_get_one_option(optid, opt, arg);
  return ret;
}


int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage);
  if (opts.handle_options(&get_one_option) != 0)
    return 1;
  if (listerrins())
    return 0;
  if (checkopts(argc, argv) != 0)
    return 1;
  if (doall() != 0)
    return 1;
  return 0;
}
