
/* Copyright (C) 2003-2008 MySQL AB, 2008 Sun Microsystems, Inc.

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
#include <ndb_opts.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include "Config.hpp"

#include <version.h>
#include <kernel_types.h>
#include <my_daemon.h>
#include <NdbConfig.h>
#include <NdbSleep.h>
#include <ndb_version.h>
#include <mgmapi_config_parameters.h>
#include <NdbAutoPtr.hpp>
#include <ndb_mgmclient.hpp>

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

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgmd"),
  MY_DAEMON_LONG_OPTS(opts.)
  { "config-file", 'f', "Specify cluster configuration file",
    (uchar**) &opts.config_filename, (uchar**) &opts.config_filename, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "print-full-config", 'P', "Print full config and exit",
    (uchar**) &opts.print_full_config, (uchar**) &opts.print_full_config, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Run ndb_mgmd in daemon mode (default)",
    (uchar**) &opts.daemon, (uchar**) &opts.daemon, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "interactive", 256,
    "Run interactive. Not supported but provided for testing purposes",
    (uchar**) &opts.interactive, (uchar**) &opts.interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-nodeid-checks", 256,
    "Do not provide any node id checks",
    (uchar**) &opts.no_nodeid_checks, (uchar**) &opts.no_nodeid_checks, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodaemon", 256,
    "Don't run as daemon, but don't read from stdin",
    (uchar**) &opts.non_interactive, (uchar**) &opts.non_interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "mycnf", 256,
    "Read cluster config from my.cnf",
    (uchar**) &opts.mycnf, (uchar**) &opts.mycnf, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "bind-address", 256,
    "Local bind address",
    (uchar**) &opts.bind_address, (uchar**) &opts.bind_address, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "configdir", 256,
    "Directory for the binary configuration files",
    (uchar**) &opts.configdir, (uchar**) &opts.configdir, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", 'v',
    "Write more log messages",
    (uchar**) &opts.verbose, (uchar**) &opts.verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "reload", 256,
    "Reload config from config.ini or my.cnf if it has changed on startup",
    (uchar**) &opts.reload, (uchar**) &opts.reload, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "initial", 256,
    "Delete all binary config files and start from config.ini or my.cnf",
    (uchar**) &opts.initial, (uchar**) &opts.initial, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(my_progname, NULL);
}

static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static char **defaults_argv;

/*
   mgmd_exit()
   do_exit=true:
     if in a windows service, don't want process to exit()
     until cleanup of other threads is done
*/
#ifdef _WIN32
extern HANDLE  g_shutdown_evt;
#endif
static void mgmd_exit(int result)
{
  g_eventLogger->close();

  /* Free memory allocated by 'load_defaults' */
  free_defaults(defaults_argv);

  ndb_end(opt_ndb_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);

#ifdef _WIN32
  if(opts.service)
    SetEvent(g_shutdown_evt); // release stopper thread
  else
    exit(result);
#else
  exit(result);
#endif
}

int null_printf(const char*s,...)
{
  return 0;
}
#define DBG IF_WIN(g_eventLogger->debug,null_printf)

int event_loop(void*);
int start();
int argc_;char**argv_;
int main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  argc_= argc;
  argv_= argv;

  g_eventLogger->setCategory("MgmSrvr");

  ndb_opt_set_usage_funcs(NULL, short_usage_sub, usage);

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
      opts.print_full_config ||
      IF_WIN(1,0)) {
    opts.daemon= 0;
  }

#ifdef _WIN32
  int r=maybe_install_or_remove_service(argc_,argv_,
                                 (char*)opts.remove,(char*)opts.install,
                                       "MySQL Cluster Management Server");
  if(r!=-1)
    return r;
#ifdef _DEBUG
  /* it is impossible to attach a debugger to a starting service
  ** so it is necessary to log to a known place to diagnose
  ** problems.  services don't have a stdout/stderr so the only
  ** way to write debug info is to a file.
  ** change this path if you don't have a c:\
  */
  if(opts.service) {
    char *fn= "c:\\ndb_mgmd_debug.log";
    g_eventLogger->createFileHandler(fn);
    DBG(NdbConfig_StdoutFileName(0));
    DBG(NdbConfig_get_path(0));
  } else
#endif
#endif
  /* Output to console initially */
  g_eventLogger->createConsoleHandler();

  if (opts.verbose)
    g_eventLogger->enable(Logger::LL_DEBUG);

  if (opts.mycnf && opts.config_filename)
  {
    g_eventLogger->error("Both --mycnf and -f is not supported");
    mgmd_exit(1);
  }

  /**
     Install signal handler for SIGPIPE
     Done in TransporterFacade as well.. what about Configretriever?
   */
#if !defined NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
  return start();
}

int daemon_stop()
{
  g_StopServer= true;
  return 0;
}

int start()
{
  g_eventLogger->info("NDB Cluster Management Server. %s", NDB_VERSION_STRING);

  mgm= new MgmtSrvr(opts, opt_connect_str);
  if (mgm == NULL) {
    DBG("mgm is NULL");
    g_eventLogger->critical("Out of memory, couldn't create MgmtSrvr");
    mgmd_exit(1);
  }

  /* Init mgm, load or fetch config */
  if (!mgm->init()) {
    delete mgm;
    mgmd_exit(1);
  }

  my_setwd(NdbConfig_get_path(0), MYF(0));
#ifdef _WIN32
  DBG("cl %s",GetCommandLine());
#endif
  if (IF_WIN(opts.service,opts.daemon)) {
    DBG("service name %s",IF_WIN(opts.service,""));
    NodeId localNodeId= mgm->getOwnNodeId();
    if (localNodeId == 0) {
      g_eventLogger->error("Couldn't get own node id");
      delete mgm;
      mgmd_exit(1);
    }
    struct MY_DAEMON thedaemon= {event_loop,daemon_stop};
    char *lockfile= NdbConfig_PidFileName(localNodeId);
    char *logfile=  NdbConfig_StdoutFileName(localNodeId);
    DBG("to open %s,%s", lockfile, logfile);
    if (my_daemon_prefiles(lockfile, logfile)) {
      g_eventLogger->error("daemon_prefiles %s", my_daemon_error);
      mgmd_exit(1);
    }
    if(my_daemon_files()) {
      g_eventLogger->error("daemon_files %s", my_daemon_error);
      mgmd_exit(1);
    }
    return my_daemon_run((char*)IF_WIN(opts.service,0),&thedaemon);
  }
#ifdef _WIN32
  if(opts.daemon) {
    g_eventLogger->error("no daemon mode on windows, use -i to set up a service.");
    mgmd_exit(1);
  }
#endif
  return event_loop(0);
}

int event_loop(void*)
{
  if (!mgm->start()) { /* Start mgm services */
    delete mgm;
    mgmd_exit(1);
  }

  if(opts.interactive) {
    int port= mgm->getPort();
    BaseString con_str;
    if(opts.bind_address)
      con_str.appfmt("host=%s:%d", opts.bind_address, port);
    else
      con_str.appfmt("localhost:%d", port);
    Ndb_mgmclient com(con_str.c_str(), 1);
    while(g_StopServer != true && read_and_execute(&com, "ndb_mgm> ", 1));
  }
  else {
    while(g_StopServer != true) {
      NdbSleep_MilliSleep(500);
    }
  }

  g_eventLogger->info("Shutting down server...");
  delete mgm;
  g_eventLogger->info("Shutdown complete");

  if(g_RestartServer){
    g_eventLogger->info("Restarting server...");
    g_RestartServer= g_StopServer= false;
    int ex= start();
    if(ex)
      mgmd_exit(ex);
  }

  mgmd_exit(0);
  return 0;
}

