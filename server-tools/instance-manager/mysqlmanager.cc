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

#include <my_global.h>
#include "manager.h"

#include "options.h"
#include "log.h"

#include <my_sys.h>
#include <string.h>
#include <signal.h>
#ifndef __WIN__
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __WIN__
#include "windowsservice.h"
#endif

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
#ifndef __WIN__
static void daemonize(const char *log_file_name);
static void angel(const Options &options);
static struct passwd *check_user(const char *user);
static int set_user(const char *user, struct passwd *user_info);
#else
int HandleServiceOptions(Options options);
#endif


/*
  main, entry point
  - init environment
  - handle options
  - daemonize and run angel process (if necessary)
  - run manager process
*/

int main(int argc, char *argv[])
{
  int return_value= 1;
  init_environment(argv[0]);
  Options options;
  struct passwd *user_info;

  if (options.load(argc, argv))
    goto err;

#ifndef __WIN__
  if ((user_info= check_user(options.user)))
  {
      if (set_user(options.user, user_info))
        goto err;
  }

  if (options.run_as_service)
  {
    /* forks, and returns only in child */
    daemonize(options.log_file_name);
    /* forks again, and returns only in child: parent becomes angel */
    angel(options);
  }
#else
  if (!options.stand_alone)
  {
    if (HandleServiceOptions(options))
      goto err;
  }
  else
#endif

  manager(options);
  return_value= 0;

err:
  options.cleanup();
  my_end(0);
  return return_value;
}

/******************* Auxilary functions implementation **********************/

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
/* Change to run as another user if started with --user */

static struct passwd *check_user(const char *user)
{
  struct passwd *user_info;
  uid_t user_id= geteuid();

  /* Don't bother if we aren't superuser */
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      user_info= getpwnam(user);
      if ((!user_info || user_id != user_info->pw_uid))
        log_info("One can only use the --user switch if running as root\n");
    }
    return NULL;
  }
  if (!user)
  {
    log_info("You are running mysqlmanager as root! This might introduce security problems. It is safer to use --user option istead.\n");
    return NULL;
  }
  if (!strcmp(user, "root"))
    return NULL;                 /* Avoid problem with dynamic libraries */
 if (!(user_info= getpwnam(user)))
  {
    /* Allow a numeric uid to be used */
    const char *pos;
    for (pos= user; my_isdigit(default_charset_info, *pos); pos++)
    {}
    if (*pos)                                   /* Not numeric id */
      goto err;
    if (!(user_info= getpwuid(atoi(user))))
      goto err;
    else
      return user_info;
  }
  else
    return user_info;

err:
  log_error("Fatal error: Can't change to run as user '%s' ;  Please check that the user exists!\n", user);
  return NULL;
}

static int set_user(const char *user, struct passwd *user_info)
{
  DBUG_ASSERT(user_info);
#ifdef HAVE_INITGROUPS
  initgroups((char*) user,user_info->pw_gid);
#endif
  if (setgid(user_info->pw_gid) == -1)
  {
    log_error("setgid() failed");
    return 1;
  }
  if (setuid(user_info->pw_uid) == -1)
  {
    log_error("setuid() failed");
    return 1;
  }
  return 0;
}
#endif


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


#ifndef __WIN__
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
      log_info("angel got signal %d, exiting", is_terminated);
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

#endif
