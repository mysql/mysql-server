/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "manager.h"
#include "options.h"
#include "log.h"

#include <my_global.h>
#include <my_sys.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


/*
  Few notes about Instance Manager architecture:
  Instance Manager consisits of two processes: the angel process, and the
  instance manager process. Responsibilities of the angel process is to
  monitor the instance manager process, and restart it in case of
  failure/shutdown. The angel process is started only if startup option
  '--run-as-service' is provided.
  The Instance Manager process consists of several
  subsystems (thread sets):
  - the signal handling thread: it's responsibilities are to handle
    user signals and propogate them to the other threads. All other threads
    are accounted in the signal handler thread Thread Registry.
  - the listener: listens all sockets. There is a listening
    socket for each (mysql, http, snmp, rendezvous (?)) subsystem.
  - mysql subsystem: Instance Manager acts like an ordinary MySQL Server,
    but with very restricted command set. Each MySQL client connection is
    handled in a separate thread. All MySQL client connections threads
    constitute mysql subsystem.
  - http subsystem: it is also possible to talk with Instance Manager via
    http. One thread per http connection is used. Threads are pooled.
  - 'snmp' connections (FIXME: I know nothing about it yet)
  - rendezvous threads
*/

static void init_environment(char *progname);
static void daemonize(const char *log_file_name);
static void angel(const Options &options);


/*
  main, entry point
  - init environment
  - handle options
  - daemonize and run angel process (if necessary)
  - run manager process
*/

int main(int argc, char *argv[])
{
  init_environment(argv[0]);
  Options options;
  options.load(argc, argv);
  if (options.run_as_service)
  {
    /* forks, and returns only in child */
    daemonize(options.log_file_name);
    /* forks again, and returns only in child: parent becomes angel */
    angel(options);
  }
  manager(options);
  return 0;
}

/******************* Auxilary functions implementation **********************/


/*
  Init environment, common for daemon and non-daemon
*/

static void init_environment(char *progname)
{
  MY_INIT(progname);
  log_init();
  umask(0117);
  srand(time(0));
}


/*
  Become a UNIX service
  SYNOPSYS
    daemonize()
*/

static void daemonize(const char *log_file_name)
{
  pid_t pid= fork();
  switch (pid) {
  case -1:                                      // parent, fork error
    die("daemonize(): fork failed, %s", strerror(errno));
  case 0:                                       // child, fork ok
    int fd;
    /*
      Become a session leader: setsid must succeed because child is
      guaranteed not to be a process group leader (it belongs to the
      process group of the parent.)
      The goal is not to have a controlling terminal.
    */
    setsid();
    /*
      As we now don't have a controlling terminal we will not receive
      tty-related signals - no need to ignore them.
    */

    close(STDIN_FILENO);

    fd= open(log_file_name, O_WRONLY | O_CREAT | O_APPEND | O_NOCTTY,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0)
      die("daemonize(): failed to open log file %s, %s", log_file_name,
          strerror(errno));
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
      close(fd);

    /* TODO: chroot() and/or chdir() here */
    break;
  default:
    /* successfully exit from parent */
    exit(0);
  }
}


enum { CHILD_OK= 0, CHILD_NEED_RESPAWN, CHILD_EXIT_ANGEL };

static volatile sig_atomic_t child_status= CHILD_OK;

/*
  Signal handler for SIGCHLD: reap child, analyze child exit status, and set
  child_status appropriately.
*/

void reap_child(int __attribute__((unused)) signo)
{
  int child_exit_status;
  /* As we have only one child, no need to cycle waitpid */
  if (waitpid(0, &child_exit_status, WNOHANG) > 0)
  {
    if (WIFSIGNALED(child_exit_status))
      child_status= CHILD_NEED_RESPAWN;
    else
      /*
        As reap_child is not called for SIGSTOP, we should be here only
        if the child exited normally.
      */
      child_status= CHILD_EXIT_ANGEL;
  }
}

static volatile sig_atomic_t is_terminated= 0;

/*
  Signal handler for terminate signals - SIGTERM, SIGHUP, SIGINT.
  Set termination status and return.
  (q) do we need to handle SIGQUIT?
*/

void terminate(int signo)
{
  is_terminated= signo;
}


/*
  Fork a child and monitor it.
  User can explicitly kill the angel process with SIGTERM/SIGHUP/SIGINT.
  Angel process will exit silently if mysqlmanager exits normally.
*/

static void angel(const Options &options)
{
  /* install signal handlers */
  sigset_t zeromask;                            // to sigsuspend in parent
  struct sigaction sa_chld, sa_term;
  struct sigaction sa_chld_out, sa_term_out, sa_int_out, sa_hup_out;

  sigemptyset(&zeromask);
  sigemptyset(&sa_chld.sa_mask);
  sigemptyset(&sa_term.sa_mask);

  sa_chld.sa_handler= reap_child;
  sa_chld.sa_flags= SA_NOCLDSTOP;
  sa_term.sa_handler= terminate;
  sa_term.sa_flags= 0;

  /* sigaction can fail only on wrong arguments */
  sigaction(SIGCHLD, &sa_chld, &sa_chld_out);
  sigaction(SIGTERM, &sa_term, &sa_term_out);
  sigaction(SIGINT, &sa_term, &sa_int_out);
  sigaction(SIGHUP, &sa_term, &sa_hup_out);

  /* spawn a child */
spawn:
  pid_t pid= fork();
  switch (pid) {
  case -1:
    die("angel(): fork failed, %s", strerror(errno));
  case 0:                                     // child, success
    /*
      restore default actions for signals to let the manager work with
      signals as he wishes
    */
    sigaction(SIGCHLD, &sa_chld_out, 0);
    sigaction(SIGTERM, &sa_term_out, 0);
    sigaction(SIGINT, &sa_int_out, 0);
    sigaction(SIGHUP, &sa_hup_out, 0);
    /* Here we return to main, and fall into manager */
    break;
  default:                                    // parent, success
    while (child_status == CHILD_OK && is_terminated == 0)
      sigsuspend(&zeromask);

    if (is_terminated)
      log_info("angel got signal %d (%s), exiting",
               is_terminated, sys_siglist[is_terminated]);
    else if (child_status == CHILD_NEED_RESPAWN)
    {
      child_status= CHILD_OK;
      log_error("angel(): mysqlmanager exited abnormally: respawning...");
      sleep(1); /* don't respawn too fast */
      goto spawn;
    }
    /*
      mysqlmanager successfully exited, let's silently evaporate
      If we return to main we fall into the manager() function, so let's
      simply exit().
    */
    exit(0);
  }
}

