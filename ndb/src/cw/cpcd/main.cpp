/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>	/* Needed for mkdir(2) */

#include "CPCD.hpp"
#include "APIService.hpp"
#include <NdbMain.h>
#include <NdbSleep.h>
#include <BaseString.hpp>
#include <getarg.h>
#include <logger/Logger.hpp>
#include <logger/FileLogHandler.hpp>
#include <logger/SysLogHandler.hpp>

#include "common.hpp"

static char *work_dir = CPCD_DEFAULT_WORK_DIR;
static int port = CPCD_DEFAULT_TCP_PORT;
static int use_syslog = 0;
static char *logfile = NULL;
static char *config_file = CPCD_DEFAULT_CONFIG_FILE;
static char *user = 0;

static struct getargs args[] = {
  { "work-dir", 'w', arg_string, &work_dir, 
    "Work directory", "directory" },
  { "port", 'p', arg_integer, &port, 
    "TCP port to listen on", "port" },
  { "syslog", 'S', arg_flag, &use_syslog,
    "Log events to syslog", NULL},
  { "logfile", 'L', arg_string, &logfile,
    "File to log events to", "file"},
  { "debug", 'D', arg_flag, &debug,
    "Enable debug mode", NULL},
  { "config", 'c', arg_string, &config_file, "Config file", NULL },
  { "user", 'u', arg_string, &user, "Run as user", NULL }
};

static const int num_args = sizeof(args) / sizeof(args[0]);

static CPCD * g_cpcd = 0;
#if 0
extern "C" static void sig_child(int signo, siginfo_t*, void*);
#endif

const char *progname = "ndb_cpcd";

NDB_MAIN(ndb_cpcd){
  int optind = 0;

  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, progname, "");
    exit(1);
  }

  Properties p;
  insert_file(config_file, p);
  if(parse_config_file(args, num_args, p)){
    ndbout_c("Invalid config file: %s", config_file);
    exit(1);
  }

  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, progname, "");
    exit(1);
  }

  logger.setCategory(progname);
  logger.enable(Logger::LL_ALL);

  if(debug)
    logger.createConsoleHandler();

  if(user && runas(user) != 0){
    logger.critical("Unable to change user: %s", user);
    _exit(1);
  }

  if(logfile != NULL){
    BaseString tmp;
    if(logfile[0] != '/')
      tmp.append(work_dir); 
    tmp.append(logfile);
    logger.addHandler(new FileLogHandler(tmp.c_str()));
  }
  
  if(use_syslog)
    logger.addHandler(new SysLogHandler());

  logger.info("Starting");

  CPCD cpcd;
  g_cpcd = &cpcd;
  
  /* XXX This will probably not work on !unix */
  int err = mkdir(work_dir, S_IRWXU | S_IRGRP | S_IROTH);
  if(err != 0) {
    switch(errno) {
    case EEXIST:
      break;
    default:
      fprintf(stderr, "Cannot mkdir %s: %s\n", work_dir, strerror(errno));
      exit(1);
    }
  }

  if(strlen(work_dir) > 0){
    logger.debug("Changing dir to '%s'", work_dir);
    if((err = chdir(work_dir)) != 0){
      fprintf(stderr, "Cannot chdir %s: %s\n", work_dir, strerror(errno));
      exit(1);
    }
  }

  cpcd.loadProcessList();
  
  SocketServer * ss = new SocketServer();
  CPCDAPIService * serv = new CPCDAPIService(cpcd);
  if(!ss->setup(serv, port)){
    logger.critical("Cannot setup server: %s", strerror(errno));
    sleep(1);
    delete ss;
    delete serv;
    return 1;
  }

  ss->startServer();

  {  
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#if 0
    struct sigaction act;
    act.sa_handler = 0;
    act.sa_sigaction = sig_child;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &act, 0);
#endif
  }
  
  logger.debug("Start completed");
  while(true) NdbSleep_MilliSleep(1000);
  
  delete ss;
  return 0;
}

#if 0
extern "C" 
void 
sig_child(int signo, siginfo_t* info, void*){
  printf("signo: %d si_signo: %d si_errno: %d si_code: %d si_pid: %d\n",
	 signo, 
	 info->si_signo,
	 info->si_errno,
	 info->si_code,
	 info->si_pid);
  
}
#endif
