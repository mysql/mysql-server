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
#include <kernel/NodeBitmask.hpp>
#include <portlib/ndb_daemon.h>
#include "util/ndb_openssl_evp.h"

#include "my_alloc.h"
#include "ndbd.hpp"
#include "angel.hpp"

#include "../common/util/parse_mask.hpp"
#include "OwnProcessInfo.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 485


static int opt_daemon, opt_no_daemon, opt_foreground,
  opt_initialstart, opt_verbose;
static const char* opt_nowait_nodes = 0;
static const char* opt_bind_address = 0;
static int opt_report_fd;
static int opt_initial;
static int opt_no_start;
static unsigned opt_allocated_nodeid;
static int opt_angel_pid;
static unsigned long opt_logbuffer_size;

extern NdbNodeBitmask g_nowait_nodes;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndbd"),
  { "initial", NDB_OPT_NOSHORT,
    "Perform initial start of ndbd, including cleaning the file system. "
    "Consult documentation before using this",
    &opt_initial, &opt_initial, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nostart", 'n',
    "Don't start ndbd immediately. Ndbd will await command from ndb_mgmd",
    &opt_no_start, &opt_no_start, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Start ndbd as daemon (default)",
    &opt_daemon, &opt_daemon, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "nodaemon", NDB_OPT_NOSHORT,
    "Do not start ndbd as daemon, provided for testing purposes",
    &opt_no_daemon, &opt_no_daemon, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "foreground", NDB_OPT_NOSHORT,
    "Run real ndbd in foreground, provided for debugging purposes"
    " (implies --nodaemon)",
    &opt_foreground, &opt_foreground, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nowait-nodes", NDB_OPT_NOSHORT,
    "Nodes that will not be waited for during start",
    &opt_nowait_nodes, &opt_nowait_nodes, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "initial-start", NDB_OPT_NOSHORT,
    "Perform a partial initial start of the cluster.  "
    "Each node should be started with this option, as well as --nowait-nodes",
    &opt_initialstart, &opt_initialstart, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "bind-address", NDB_OPT_NOSHORT,
    "Local bind address",
    &opt_bind_address, &opt_bind_address, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", 'v',
    "Write more log messages",
    &opt_verbose, &opt_verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "report-fd", 256,
    "INTERNAL: fd where to write extra shutdown status",
    &opt_report_fd, &opt_report_fd, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, INT_MAX, 0, 0, 0 },
  { "allocated-nodeid", 256,
    "INTERNAL: nodeid allocated by angel process",
    &opt_allocated_nodeid, &opt_allocated_nodeid, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX, 0, 0, 0 },
  { "angel-pid", NDB_OPT_NOSHORT,
    "INTERNAL: angel process id",
    &opt_angel_pid, &opt_angel_pid, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, UINT_MAX, 0, 0, 0 },
  { "logbuffer-size", NDB_OPT_NOSHORT,
    "Size of the log buffer for data node ndb_x_out.log",
    &opt_logbuffer_size, &opt_logbuffer_size, 0,
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    GET_ULONG, REQUIRED_ARG, 1024*1024, 2048, ULONG_MAX, 0, 0, 0
#else
    GET_ULONG, REQUIRED_ARG, 32768, 2048, ULONG_MAX, 0, 0, 0
#endif
  },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

const char *load_default_groups[]= { "mysql_cluster", "ndbd", 0 };


static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
  ndb_service_print_options("ndbd");
}

/**
 * C++ Standard 3.6.1/3:
 *  The function main shall not be used (3.2) within a program.
 *
 * So call "main" "real_main" to avoid this rule...
 */
int
real_main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  Ndb_opts::release();  // because ndbd can fork and call real_main() again
  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);

  // Print to stdout/console
  g_eventLogger->createConsoleHandler();

#ifdef _WIN32
  /* Output to Windows event log */
  g_eventLogger->createEventLogHandler("MySQL Cluster Data Node Daemon");
#endif

  g_eventLogger->setCategory("ndbd");

  // Turn on max loglevel for startup messages
  g_eventLogger->m_logLevel.setLogLevel(LogLevel::llStartUp, 15);

  opts.set_usage_funcs(short_usage_sub);

#ifndef NDEBUG
  opt_debug= "d:t:O,/tmp/ndbd.trace";
#endif

  // Save the original program name and arguments for angel
  const char* progname = argv[0];
  Vector<BaseString> original_args;
  for (int i = 0; i < argc; i++)
  {
    if (ndb_is_load_default_arg_separator(argv[i]))
      continue;
    original_args.push_back(argv[i]);
  }

  int ho_error = opts.handle_options(ndb_std_get_one_option);
  if (ho_error != 0) {
    exit(ho_error);
  }

  if (opt_no_daemon || opt_foreground) {
    // --nodaemon or --forground implies --daemon=0
    opt_daemon= 0;
  }

  // Turn on debug printouts if --verbose
  if (opt_verbose)
    g_eventLogger->enable(Logger::LL_DEBUG);

  if (opt_nowait_nodes)
  {
    int res = parse_mask(opt_nowait_nodes, g_nowait_nodes);
    if(res == -2 || (res > 0 && g_nowait_nodes.get(0)))
    {
      g_eventLogger->error("Invalid nodeid specified in nowait-nodes: %s",
                           opt_nowait_nodes);
      exit(-1);
    }
    else if (res < 0)
    {
      g_eventLogger->error("Unable to parse nowait-nodes argument: %s",
                           opt_nowait_nodes);
      exit(-1);
    }
  }

 if(opt_angel_pid)
  {
    setOwnProcessInfoAngelPid(opt_angel_pid);
  }

  if (opt_foreground ||
      opt_allocated_nodeid ||
      opt_report_fd)
  {
    /**
      This is where we start running the real data node process after
      reading options. This function will never return.
    */
    ndbd_run(opt_foreground, opt_report_fd,
             opt_ndb_connectstring, opt_ndb_nodeid, opt_bind_address,
             opt_no_start, opt_initial, opt_initialstart,
             opt_allocated_nodeid, opt_connect_retries, opt_connect_retry_delay,
             opt_logbuffer_size);
  }

  /**
    The angel process takes care of automatic restarts, by default this is
    the default to have an angel process. When an angel process is used the
    program will enter into angel_run from where we fork off the real data
    node process, the real process will always have opt_allocated_nodeid
    set since we don't want the nodeid to change between restarts.
  */
  angel_run(progname,
            original_args,
            opt_ndb_connectstring,
            opt_ndb_nodeid,
            opt_bind_address,
            opt_initial,
            opt_no_start,
            opt_daemon,
            opt_connect_retries,
            opt_connect_retry_delay);

  return 1; // Never reached
}

int
main(int argc, char** argv)
{
  ndb_openssl_evp::library_init();
  int rc = ndb_daemon_init(argc, argv, real_main, angel_stop,
                           "ndbd", "MySQL Cluster Data Node Daemon");
  ndb_openssl_evp::library_end();
  return rc;
}
