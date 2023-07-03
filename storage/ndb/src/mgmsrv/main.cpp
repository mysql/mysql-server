/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <ndb_opts.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include "Config.hpp"
#include "my_alloc.h"

#include <version.h>
#include <kernel_types.h>
#include <portlib/ndb_daemon.h>
#include <NdbConfig.h>
#include <NdbSleep.h>
#include <portlib/NdbDir.hpp>
#include <ndb_version.h>
#include <mgmapi_config_parameters.h>
#include <NdbAutoPtr.hpp>
#include <ndb_mgmclient.hpp>

#include <EventLogger.hpp>
#include <LogBuffer.hpp>
#include <OutputStream.hpp>


#if defined VM_TRACE || defined ERROR_INSERT
extern int g_errorInsert;
#endif

const char *load_default_groups[]= { "mysql_cluster","ndb_mgmd",0 };

// copied from mysql.cc to get readline
extern "C" {
#if defined(_WIN32)
#include <conio.h>
#elif !defined(__NETWARE__)
#include <readline.h>
extern "C" int add_history(const char *command); /* From readline directory */
#define HAVE_READLINE
#endif
}

static int 
read_and_execute(Ndb_mgmclient* com, const char * prompt, int _try_reconnect) 
{
  static char *line_read = (char *)NULL;

  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
  {
    free (line_read);
    line_read = (char *)NULL;
  }
#ifdef HAVE_READLINE
  /* Get a line from the user. */
  line_read = readline (prompt);    
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
#else
  static char linebuffer[254];
  fputs(prompt, stdout);
  linebuffer[sizeof(linebuffer)-1]=0;
  line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
  if (line_read == linebuffer) {
    char *q=linebuffer;
    while (*q > 31) q++;
    *q=0;
    line_read= strdup(linebuffer);
  }
#endif
  return com->execute(line_read,_try_reconnect);
}

/* Global variables */
bool g_StopServer= false;
bool g_RestartServer= false;
bool g_StopLogging= false;
static MgmtSrvr* mgm;
static MgmtSrvr::MgmtOpts opts;
static const char* opt_logname = "MgmtSrvr";
static const char* opt_nowait_nodes = 0;

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NDB_STD_OPT_DEBUG
  { "config-file", 'f', "Specify cluster configuration file",
    &opts.config_filename, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "print-full-config", 'P', "Print full config and exit",
    &opts.print_full_config, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Run ndb_mgmd in daemon mode (default)",
    &opts.daemon, nullptr, nullptr, GET_BOOL, NO_ARG,
    1, 0, 0, 0, 0, 0 },
  { "interactive", NDB_OPT_NOSHORT,
    "Run interactive. Not supported but provided for testing purposes",
    &opts.interactive,nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "no-nodeid-checks", NDB_OPT_NOSHORT, "Do not provide any node id checks",
    &opts.no_nodeid_checks, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "nodaemon", NDB_OPT_NOSHORT,
    "Don't run as daemon, but don't read from stdin",
    &opts.non_interactive, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "mycnf", NDB_OPT_NOSHORT, "Read cluster config from my.cnf",
    &opts.mycnf, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0 },
  { "bind-address", NDB_OPT_NOSHORT, "Local bind address",
    &opts.bind_address, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "cluster-config-suffix", NDB_OPT_NOSHORT, "Override defaults-group-suffix "
    "when reading cluster_config sections in my.cnf.",
    &opts.cluster_config_suffix, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "configdir", NDB_OPT_NOSHORT,
    "Directory for the binary configuration files (alias for --config-dir)",
    &opts.configdir, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "config-dir", NDB_OPT_NOSHORT,
    "Directory for the binary configuration files",
    &opts.configdir, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "config-cache", NDB_OPT_NOSHORT,
    "Enable configuration cache and change management",
    &opts.config_cache, nullptr, nullptr, GET_BOOL, NO_ARG,
    1, 0, 1, 0, 0, 0 },
  { "verbose", 'v', "Write more log messages",
    &opts.verbose,nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 1, 0, 0, 0 },
  { "reload", NDB_OPT_NOSHORT,
    "Reload config from config.ini or my.cnf if it has changed on startup",
    &opts.reload, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 1, 0, 0, 0 },
  { "initial", NDB_OPT_NOSHORT,
    "Delete all binary config files and start from config.ini or my.cnf",
    &opts.initial, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 1, 0, 0, 0 },
  { "log-name", NDB_OPT_NOSHORT,
    "Name to use when logging messages for this node",
    &opt_logname, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
  { "nowait-nodes", NDB_OPT_NOSHORT,
    "Nodes that will not be waited for during start",
    &opt_nowait_nodes,nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
#if defined VM_TRACE || defined ERROR_INSERT
  { "error-insert", NDB_OPT_NOSHORT,
    "Start with error insert variable set",
    &g_errorInsert, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0 },
#endif
  NdbStdOpt::end_of_options
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
  ndb_service_print_options("ndb_mgmd");
}

static void mgmd_exit(int result)
{
  g_eventLogger->close();

  ndb_end(opt_ndb_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);

  ndb_daemon_exit(result);
}

#ifndef _WIN32
static void mgmd_sigterm_handler(int signum)
{
  g_eventLogger->info("Received SIGTERM. Performing stop.");
  mgmd_exit(0);
}
#endif

struct ThdData
{
  FILE* f;
  LogBuffer* logBuf;
};

/**
 * This function/thread is responsible for getting
 * bytes from the log buffer and writing them
 * to the mgmd local log file.
 */

void* async_local_log_func(void* args)
{
  ThdData* data = (ThdData*)args;
  FILE* f = data->f;
  LogBuffer* logBuf = data->logBuf;
  const size_t get_bytes = 512;
  char buf[get_bytes + 1];
  size_t bytes;
  int part_bytes = 0, bytes_printed = 0;

  while(!g_StopLogging)
  {
    part_bytes = 0;
    bytes_printed = 0;

    if((bytes = logBuf->get(buf, get_bytes)))
    {
      fwrite(buf, bytes, 1, f);
      fflush(f);
    }
  }

  while((bytes = logBuf->get(buf, get_bytes, 1)))// flush remaining logs
  {
    fwrite(buf, bytes, 1, f);
    fflush(f);
  }

  // print lost count in the end, if any
  size_t lost_count = logBuf->getLostCount();
  if(lost_count)
  {
    fprintf(f, LostMsgHandler::LOST_BYTES_FMT, lost_count);
    fflush(f);
  }

  return NULL;
}

static void mgmd_run()
{
  LogBuffer* logBufLocalLog = new LogBuffer(32768); // 32kB

  struct NdbThread* locallog_threadvar= NULL;
  ThdData thread_args=
  {
    stdout,
    logBufLocalLog,
  };

  // Create log thread which logs data to the mgmd local log.
  locallog_threadvar = NdbThread_Create(async_local_log_func,
                                        (void**)&thread_args,
                                        0,
                                        "async_local_log_thread",
                                        NDB_THREAD_PRIO_MEAN);

  BufferedOutputStream* ndbouts_bufferedoutputstream = new BufferedOutputStream(logBufLocalLog);

  // Make ndbout point to the BufferedOutputStream.
  NdbOut_ReInit(ndbouts_bufferedoutputstream, ndbouts_bufferedoutputstream);

  /* Start mgm services */
  if (!mgm->start()) {
    delete mgm;
    mgmd_exit(1);
  }

  if (opts.interactive) {
    int port= mgm->getPort();
    BaseString con_str;
    if(opts.bind_address)
      con_str.appfmt("host=%s %d", opts.bind_address, port);
    else
      con_str.appfmt("localhost:%d", port);
    Ndb_mgmclient com(con_str.c_str(), "ndb_mgm> ", 1, 5);
    while(!g_StopServer){
      if (!read_and_execute(&com, "ndb_mgm> ", 1))
        g_StopServer = true;
    }
  }
  else
  {
    g_eventLogger->info("MySQL Cluster Management Server %s started",
                        NDB_VERSION_STRING);

    while (!g_StopServer)
      NdbSleep_MilliSleep(500);
  }

  g_eventLogger->info("Shutting down server...");
  delete mgm;
  g_eventLogger->info("Shutdown complete");

  if(g_RestartServer){
    g_eventLogger->info("Restarting server...");
    g_RestartServer= g_StopServer= false;
  }

  /**
   * Stopping the log thread is done at the very end since the
   * node logs should be available until complete shutdown.
   */
  void* dummy_return_status;
  g_StopLogging = true;
  NdbThread_WaitFor(locallog_threadvar, &dummy_return_status);
  delete ndbouts_bufferedoutputstream;
  NdbThread_Destroy(&locallog_threadvar);
  delete logBufLocalLog;
}

#include "../common/util/parse_mask.hpp"

static int mgmd_main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  Ndb_opts ndb_opts(argc, argv, my_long_options, load_default_groups);
  ndb_opts.set_usage_funcs(short_usage_sub);

  printf("MySQL Cluster Management Server %s\n", NDB_VERSION_STRING);

  int ho_error;
#ifndef NDEBUG
  opt_debug= IF_WIN("d:t:i:F:o,c:\\ndb_mgmd.trace",
                    "d:t:i:F:o,/tmp/ndb_mgmd.trace");
#endif

  if ((ho_error=ndb_opts.handle_options()))
    mgmd_exit(ho_error);

  if (argc > 0) {
    std::string invalid_args;
    for (int i = 0; i < argc; i++) invalid_args += ' ' + std::string(argv[i]);
    fprintf(stderr, "ERROR: Unknown option -%s specified.\n",
            invalid_args.c_str());
    mgmd_exit(1);
  }

  /**
    config_filename is set to nullptr when --skip-config-file is specified
   */
  if (opts.config_filename == disabled_my_option) {
    opts.config_filename = nullptr;
  }

  if (opts.interactive ||
      opts.non_interactive ||
      opts.print_full_config) {
    opts.daemon= 0;
  }

  if (opts.mycnf && opts.config_filename)
  {
    fprintf(stderr, "ERROR: Both --mycnf and -f is not supported\n");
    mgmd_exit(1);
  }

  /* Validation to prevent using relative path for config-dir */
  if (opts.config_cache && (opts.configdir != disabled_my_option) &&
      (strcmp(opts.configdir, MYSQLCLUSTERDIR) != 0)) {
    bool absolute_path = false;
    if (strncmp(opts.configdir, "/", 1) == 0) absolute_path = true;
#ifdef _WIN32
    if (strncmp(opts.configdir, "\\", 1) == 0) absolute_path = true;
    if (strlen(opts.configdir) >= 3 &&
        ((opts.configdir[0] >= 'a' && opts.configdir[0] <= 'z') ||
         (opts.configdir[0] >= 'A' && opts.configdir[0] <= 'Z')) &&
        opts.configdir[1] == ':' &&
        (opts.configdir[2] == '\\' || opts.configdir[2] == '/'))
      absolute_path = true;
#endif
    if (!absolute_path) {
      fprintf(
          stderr,
          "ERROR: Relative path ('%s') not supported for configdir, specify "
          "absolute path.\n",
          opts.configdir);
      mgmd_exit(1);
    }
  }

  /*validation is added to prevent user using
  wrong short option for --config-file.*/
  if (opt_ndb_connectstring)
  {
    // file path mostly starts with . or /
    if (strncmp(opt_ndb_connectstring, "/", 1) == 0 ||
        strncmp(opt_ndb_connectstring, ".", 1) == 0)
    {
      fprintf(stderr, "ERROR: --ndb-connectstring can't start with '.' or"
          " '/'\n");
      mgmd_exit(1);
    }

    // ndb-connectstring is ignored when config file option is provided
    if (opts.config_filename) {
      fprintf(stderr,
              "WARNING: --ndb-connectstring is ignored when mgmd is started "
              "with -f or config-file.\n");
    }
  }

  if (opt_nowait_nodes)
  {
    int res = parse_mask(opt_nowait_nodes, opts.nowait_nodes);
    if(res == -2 || (res > 0 && opts.nowait_nodes.get(0)))
    {
      fprintf(stderr, "ERROR: Invalid nodeid specified in nowait-nodes: '%s'\n",
              opt_nowait_nodes);
      mgmd_exit(1);
    }
    else if (res < 0)
    {
      fprintf(stderr, "ERROR: Unable to parse nowait-nodes argument: '%s'\n",
              opt_nowait_nodes);
      mgmd_exit(1);
    }
  }

  if (opts.bind_address)
  {
    int len = strlen(opts.bind_address);
    if ((opts.bind_address[0] == '[') &&
        (opts.bind_address[len - 1] == ']'))
    {
      opts.bind_address = strdup(opts.bind_address + 1);
    }
    else
    {
      opts.bind_address = strdup(opts.bind_address);
    }
  }

  /* Setup use of event logger */
  g_eventLogger->setCategory(opt_logname);

  /* Output to console initially */
  g_eventLogger->createConsoleHandler();

#ifdef _WIN32
  /* Output to Windows event log */
  g_eventLogger->createEventLogHandler("MySQL Cluster Management Server");
#endif

  if (opts.verbose)
    g_eventLogger->enable(Logger::LL_ALL); // --verbose turns on everything

  /**
     Install signal handler for SIGPIPE
     Done in TransporterFacade as well.. what about Configretriever?
   */
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, mgmd_sigterm_handler);
#endif

  while (!g_StopServer)
  {
    NdbOut_Init();
    mgm= new MgmtSrvr(opts);
    if (mgm == NULL) {
      g_eventLogger->critical("Out of memory, couldn't create MgmtSrvr");
      fprintf(stderr, "CRITICAL: Out of memory, couldn't create MgmtSrvr\n");
      mgmd_exit(1);
    }

    /* Init mgm, load or fetch config */
    if (!mgm->init()) {
      delete mgm;
      mgmd_exit(1);
    }

    if (NdbDir::chdir(NdbConfig_get_path(NULL)) != 0)
    {
      g_eventLogger->warning("Cannot change directory to '%s', error: %d",
                             NdbConfig_get_path(NULL), errno);
      // Ignore error
    }

    if (opts.daemon)
    {
      NodeId localNodeId= mgm->getOwnNodeId();
      if (localNodeId == 0) {
        g_eventLogger->error("Couldn't get own node id");
        fprintf(stderr, "ERROR: Couldn't get own node id\n");
        delete mgm;
        mgmd_exit(1);
      }

      char *lockfile= NdbConfig_PidFileName(localNodeId);
      char *logfile=  NdbConfig_StdoutFileName(localNodeId);
      if (ndb_daemonize(lockfile, logfile))
      {
        g_eventLogger->error("Couldn't start as daemon, error: '%s'",
                             ndb_daemon_error);
        fprintf(stderr, "Couldn't start as daemon, error: '%s' \n",
                ndb_daemon_error);
        mgmd_exit(1);
      }
    }

    mgmd_run();
  }

  mgmd_exit(0);
  return 0;
}


static void mgmd_stop(void)
{
  g_StopServer= true;
}


int main(int argc, char** argv)
{
  return ndb_daemon_init(argc, argv, mgmd_main, mgmd_stop,
                         "ndb_mgmd", "MySQL Cluster Management Server");
}
