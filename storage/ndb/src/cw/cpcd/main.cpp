/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <my_sys.h>
#include <my_getopt.h>
#include <my_default.h>
#include <mysql_version.h>
#include <ndb_version.h>
#include "my_alloc.h"

#include "CPCD.hpp"
#include "APIService.hpp"
#include <NdbSleep.h>
#include <portlib/NdbDir.hpp>
#include <BaseString.hpp>
#include <logger/Logger.hpp>
#include <logger/FileLogHandler.hpp>
#include <logger/SysLogHandler.hpp>

#include "common.hpp"

static const char *work_dir = CPCD_DEFAULT_WORK_DIR;
static int unsigned port;
static int use_syslog;
static const char *logfile = NULL;
static const char *user = 0;

static struct my_option my_long_options[] =
{
  { "work-dir", 'w', "Work directory",
    (uchar**) &work_dir, (uchar**) &work_dir,  0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "port", 'p', "TCP port to listen on",
    (uchar**) &port, (uchar**) &port, 0,
    GET_INT, REQUIRED_ARG, CPCD_DEFAULT_TCP_PORT, 0, 0, 0, 0, 0 }, 
  { "syslog", 'S', "Log events to syslog",
    (uchar**) &use_syslog, (uchar**) &use_syslog, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "logfile", 'L', "File to log events to",
    (uchar**) &logfile, (uchar**) &logfile, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "debug", 'D', "Enable debug mode",
    (uchar**) &debug, (uchar**) &debug, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "user", 'u', "Run as user",
    (uchar**) &user, (uchar**) &user, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static bool
get_one_option(int optid, const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument)
{
  return 0;
}

static CPCD * g_cpcd = 0;


int main(int argc, char** argv){
  const char *load_default_groups[]= { "ndb_cpcd",0 };
  NDB_INIT(argv[0]);

  MEM_ROOT alloc{PSI_NOT_INSTRUMENTED, 512};
  load_defaults("ndb_cpcd",load_default_groups,&argc,&argv,&alloc);
  if (handle_options(&argc, &argv, my_long_options, get_one_option)) {
    print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
    puts("");
    my_print_help(my_long_options);
    my_print_variables(my_long_options);
    exit(1);
  }

  logger.setCategory("ndb_cpcd");
  logger.enable(Logger::LL_ALL);

  if(debug)
    logger.createConsoleHandler();

#ifndef _WIN32
  if(user && runas(user) != 0){
    logger.critical("Unable to change user: %s", user);
    _exit(1);
  }
#endif

  if(logfile != NULL){
    BaseString tmp;
    if(logfile[0] != '/')
      tmp.append(work_dir); 
    tmp.append(logfile);
    logger.addHandler(new FileLogHandler(tmp.c_str()));
  }
  
#ifndef _WIN32
  if(use_syslog)
    logger.addHandler(new SysLogHandler());
#endif

  logger.info("Starting");

#if defined SIGPIPE && !defined _WIN32
  (void)signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGCHLD
  /* Only "poll" for child to be alive, never use 'wait' */
  (void)signal(SIGCHLD, SIG_IGN);
#endif

  CPCD cpcd;
  g_cpcd = &cpcd;

  /* Create working directory unless it already exists */
  if (access(work_dir, F_OK))
  {
    logger.info("Working directory '%s' does not exist, trying "
                "to create it", work_dir);
    if (!NdbDir::create(work_dir,
                        NdbDir::u_rwx() | NdbDir::g_r() | NdbDir::o_r()))
    {
      logger.error("Failed to create working directory, terminating!");
      exit(1);
    }
  }

  if(strlen(work_dir) > 0){
    logger.debug("Changing dir to '%s'", work_dir);
    if(NdbDir::chdir(work_dir) != 0){
      logger.error("Cannot change directory to '%s', error: %d, terminating!",
                   work_dir, errno);
      exit(1);
    }
  }

  cpcd.loadProcessList();
  
  SocketServer * ss = new SocketServer();
  CPCDAPIService * serv = new CPCDAPIService(cpcd);
  unsigned short real_port= port; // correct type
  if(!ss->setup(serv, &real_port)){
    logger.critical("Cannot setup server: %s", strerror(errno));
    sleep(1);
    delete ss;
    delete serv;
    return 1;
  }

  ss->startServer();

  logger.debug("Start completed");
  while(true)
    NdbSleep_MilliSleep(1000);

  delete ss;
  return 0;
}
