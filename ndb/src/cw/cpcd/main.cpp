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
#include <ndb_opts.h>

#include "CPCD.hpp"
#include "APIService.hpp"
#include <NdbMain.h>
#include <NdbSleep.h>
#include <BaseString.hpp>
#include <logger/Logger.hpp>
#include <logger/FileLogHandler.hpp>
#include <logger/SysLogHandler.hpp>

#include "common.hpp"

static const char *work_dir = CPCD_DEFAULT_WORK_DIR;
static int port;
static int use_syslog;
static const char *logfile = NULL;
static const char *config_file = CPCD_DEFAULT_CONFIG_FILE;
static const char *user = 0;

static struct my_option my_long_options[] =
{
  { "work-dir", 'w', "Work directory",
    (gptr*) &work_dir, (gptr*) &work_dir,  0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "port", 'p', "TCP port to listen on",
    (gptr*) &port, (gptr*) &port, 0,
    GET_INT, REQUIRED_ARG, CPCD_DEFAULT_TCP_PORT, 0, 0, 0, 0, 0 }, 
  { "syslog", 'S', "Log events to syslog",
    (gptr*) &use_syslog, (gptr*) &use_syslog, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "logfile", 'L', "File to log events to",
    (gptr*) &logfile, (gptr*) &logfile, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "debug", 'D', "Enable debug mode",
    (gptr*) &debug, (gptr*) &debug, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "config", 'c', "Config file",
    (gptr*) &config_file, (gptr*) &config_file, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "user", 'u', "Run as user",
    (gptr*) &user, (gptr*) &user, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  return 0;
}

static CPCD * g_cpcd = 0;
#if 0
extern "C" static void sig_child(int signo, siginfo_t*, void*);
#endif

const char *progname = "ndb_cpcd";

int main(int argc, char** argv){
  int save_argc= argc;
  char** save_argv= argv;
  const char *load_default_groups[]= { "ndb_cpcd",0 };
  MY_INIT(argv[0]);

  load_defaults("ndb_cpcd",load_default_groups,&argc,&argv);
  if (handle_options(&argc, &argv, my_long_options, get_one_option)) {
    my_print_help(my_long_options);
    my_print_variables(my_long_options);
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
