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
#include <ndb_opts.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include "Config.hpp"

#include <version.h>
#include <kernel_types.h>
#include <NdbDaemon.h>
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

NDB_STD_OPTS_VARS;

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
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage()
{
  printf("Usage: %s [OPTIONS]\n", my_progname);
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


int main(int argc, char** argv)
{
  NDB_INIT(argv[0]);

  g_eventLogger->setCategory("MgmSrvr");

  load_defaults("my",load_default_groups,&argc,&argv);

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_mgmd.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    exit(ho_error);

  if (opts.interactive ||
      opts.non_interactive ||
      opts.print_full_config) {
    opts.daemon= 0;
  }

  /* Output to console initially */
  g_eventLogger->createConsoleHandler();

  if (opts.mycnf && opts.config_filename)
  {
    g_eventLogger->error("Both --mycnf and -f is not supported");
    exit(1);
  }

  if (opts.mycnf == 0 && opts.config_filename == 0)
  {
    struct stat buf;
    if (stat("config.ini", &buf) != -1)
      opts.config_filename = "config.ini";
  }
start:

  g_eventLogger->info("NDB Cluster Management Server. %s", NDB_VERSION_STRING);

  mgm= new MgmtSrvr(opts, opt_connect_str);
  if (mgm == NULL) {
    g_eventLogger->critical("Out of memory, couldn't create MgmtSrvr");
    exit(1);
  }

  /**
     Install signal handler for SIGPIPE
     Done in TransporterFacade as well.. what about Configretriever?
   */
#if !defined NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  /* Init mgm, load or fetch config */
  if (!mgm->init()) {
    delete mgm;
    exit(1);
  }

  my_setwd(NdbConfig_get_path(0), MYF(0));

  if (opts.daemon) {

    NodeId localNodeId= mgm->getOwnNodeId();
    if (localNodeId == 0) {
      g_eventLogger->error("Couldn't get own node id");
      delete mgm;
      exit(1);
    }

    // Become a daemon
    char *lockfile= NdbConfig_PidFileName(localNodeId);
    char *logfile=  NdbConfig_StdoutFileName(localNodeId);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

    if (NdbDaemon_Make(lockfile, logfile, 0) == -1) {
      g_eventLogger->error("Cannot become daemon: %s", NdbDaemon_ErrorText);
      delete mgm;
      exit(1);
    }
  }

  /* Start mgm services */
  if (!mgm->start()) {
    delete mgm;
    exit(1);
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
    while(g_StopServer != true)
      NdbSleep_MilliSleep(500);
  }

  g_eventLogger->info("Shutting down server...");
  delete mgm;
  g_eventLogger->info("Shutdown complete");

  if(g_RestartServer){
    g_eventLogger->info("Restarting server...");
    g_RestartServer= g_StopServer= false;
    goto start;
  }

  ndb_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  return 0;
}

