/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include "Config.hpp"

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
extern EventLogger * g_eventLogger;


const char *load_default_groups[]= { "mysql_cluster","ndb_mgmd",0 };

// copied from mysql.cc to get readline
extern "C" {
#if defined( __WIN__)
#include <conio.h>
#elif !defined(__NETWARE__)
#include <readline/readline.h>
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
static MgmtSrvr* mgm;
static MgmtSrvr::MgmtOpts opts;
static const char* opt_logname = "MgmtSrvr";
static const char* opt_nowait_nodes = 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgmd"),
  { "config-file", 'f', "Specify cluster configuration file",
    (uchar**) &opts.config_filename, (uchar**) &opts.config_filename, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "print-full-config", 'P', "Print full config and exit",
    (uchar**) &opts.print_full_config, (uchar**) &opts.print_full_config, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Run ndb_mgmd in daemon mode (default)",
    (uchar**) &opts.daemon, (uchar**) &opts.daemon, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "interactive", NDB_OPT_NOSHORT,
    "Run interactive. Not supported but provided for testing purposes",
    (uchar**) &opts.interactive, (uchar**) &opts.interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-nodeid-checks", NDB_OPT_NOSHORT,
    "Do not provide any node id checks",
    (uchar**) &opts.no_nodeid_checks, (uchar**) &opts.no_nodeid_checks, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodaemon", NDB_OPT_NOSHORT,
    "Don't run as daemon, but don't read from stdin",
    (uchar**) &opts.non_interactive, (uchar**) &opts.non_interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "mycnf", NDB_OPT_NOSHORT,
    "Read cluster config from my.cnf",
    (uchar**) &opts.mycnf, (uchar**) &opts.mycnf, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "bind-address", NDB_OPT_NOSHORT,
    "Local bind address",
    (uchar**) &opts.bind_address, (uchar**) &opts.bind_address, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "configdir", NDB_OPT_NOSHORT,
    "Directory for the binary configuration files (alias for --config-dir)",
    (uchar**) &opts.configdir, (uchar**) &opts.configdir, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "config-dir", NDB_OPT_NOSHORT,
    "Directory for the binary configuration files",
    (uchar**) &opts.configdir, (uchar**) &opts.configdir, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "config-cache", NDB_OPT_NOSHORT,
    "Enable configuration cache and change management",
    (uchar**) &opts.config_cache, (uchar**) &opts.config_cache, 0,
    GET_BOOL, NO_ARG, 1, 0, 1, 0, 0, 0 },
  { "verbose", 'v',
    "Write more log messages",
    (uchar**) &opts.verbose, (uchar**) &opts.verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "reload", NDB_OPT_NOSHORT,
    "Reload config from config.ini or my.cnf if it has changed on startup",
    (uchar**) &opts.reload, (uchar**) &opts.reload, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "initial", NDB_OPT_NOSHORT,
    "Delete all binary config files and start from config.ini or my.cnf",
    (uchar**) &opts.initial, (uchar**) &opts.initial, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "log-name", NDB_OPT_NOSHORT,
    "Name to use when logging messages for this node",
    (uchar**) &opt_logname, (uchar**) &opt_logname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nowait-nodes", NDB_OPT_NOSHORT,
    "Nodes that will not be waited for during start",
    (uchar**) &opt_nowait_nodes, (uchar**) &opt_nowait_nodes, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
  ndb_service_print_options("ndb_mgmd");
}

static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static char **defaults_argv;

static void mgmd_exit(int result)
{
  g_eventLogger->close();

  /* Free memory allocated by 'load_defaults' */
  free_defaults(defaults_argv);

  ndb_end(opt_ndb_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);

  ndb_daemon_exit(result);
}

#include "../common/util/parse_mask.hpp"

static int mgmd_main(int argc, char** argv)
{
  NDB_INIT(argv[0]);

  printf("MySQL Cluster Management Server %s\n", NDB_VERSION_STRING);

  ndb_opt_set_usage_funcs(short_usage_sub, usage);

  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv= argv; /* Must be freed by 'free_defaults' */

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= IF_WIN("d:t:i:F:o,c:\\ndb_mgmd.trace",
                    "d:t:i:F:o,/tmp/ndb_mgmd.trace");
#endif

  if ((ho_error=handle_options(&argc, &argv, my_long_options,
                               ndb_std_get_one_option)))
    mgmd_exit(ho_error);

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
#if !defined NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  while (!g_StopServer)
  {
    mgm= new MgmtSrvr(opts);
    if (mgm == NULL) {
      g_eventLogger->critical("Out of memory, couldn't create MgmtSrvr");
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
        delete mgm;
        mgmd_exit(1);
      }

      char *lockfile= NdbConfig_PidFileName(localNodeId);
      char *logfile=  NdbConfig_StdoutFileName(localNodeId);
      if (ndb_daemonize(lockfile, logfile))
      {
        g_eventLogger->error("Couldn't start as daemon, error: '%s'",
                             ndb_daemon_error);
        mgmd_exit(1);
      }
    }

    /* Start mgm services */
    if (!mgm->start()) {
      delete mgm;
      mgmd_exit(1);
    }

    if (opts.interactive) {
      int port= mgm->getPort();
      BaseString con_str;
      if(opts.bind_address)
        con_str.appfmt("host=%s:%d", opts.bind_address, port);
      else
        con_str.appfmt("localhost:%d", port);
      Ndb_mgmclient com(con_str.c_str(), 1);
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
