/* Copyright (C) 2003-2006 MySQL AB

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

#ifndef __WIN__

#include "angel.h"

#include <sys/wait.h>
/*
  sys/wait.h is needed for waitpid(). Unfortunately, there is no MySQL
  include file, that can serve for this. Include it before MySQL system
  headers so that we can change system defines if needed.
*/

#include "my_global.h"
#include "my_alarm.h"
#include "my_dir.h"
#include "my_sys.h"

/* Include other IM files. */

#include "log.h"
#include "manager.h"
#include "options.h"
#include "priv.h"

/************************************************************************/

enum { CHILD_OK= 0, CHILD_NEED_RESPAWN, CHILD_EXIT_ANGEL };

static int log_fd;

static volatile sig_atomic_t child_status= CHILD_OK;
static volatile sig_atomic_t child_exit_code= 0;
static volatile sig_atomic_t shutdown_request_signo= 0;


/************************************************************************/
/**
  Open log file.

  @return
    TRUE  on error;
    FALSE on success.
*************************************************************************/

static bool open_log_file()
{
  log_info("Angel: opening log file '%s'...",
           (const char *) Options::Daemon::log_file_name);

  log_fd= open(Options::Daemon::log_file_name,
               O_WRONLY | O_CREAT | O_APPEND | O_NOCTTY,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (log_fd < 0)
  {
    log_error("Can not open log file '%s': %s.",
              (const char *) Options::Daemon::log_file_name,
              (const char *) strerror(errno));

    return TRUE;
  }

  return FALSE;
}


/************************************************************************/
/**
  Detach the process from controlling tty.

  @return
    TRUE on error;
    FALSE on success.
*************************************************************************/

static bool detach_process()
{
  /*
    Become a session leader (the goal is not to have a controlling tty).

    setsid() must succeed because child is guaranteed not to be a process
    group leader (it belongs to the process group of the parent).

    NOTE: if we now don't have a controlling tty we will not receive
    tty-related signals - no need to ignore them.
  */

  if (setsid() < 0)
  {
    log_error("setsid() failed: %s.", (const char *) strerror(errno));
    return -1;
  }

  /* Close STDIN. */

  log_info("Angel: preparing standard streams.");

  if (close(STDIN_FILENO) < 0)
  {
    log_error("Warning: can not close stdin (%s)."
              "Trying to continue...",
              (const char *) strerror(errno));
  }

  /* Dup STDOUT and STDERR to the log file. */

  if (dup2(log_fd, STDOUT_FILENO) < 0 ||
      dup2(log_fd, STDERR_FILENO) < 0)
  {
    log_error("Can not redirect stdout and stderr to the log file: %s.",
              (const char *) strerror(errno));

    return TRUE;
  }

  if (log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO)
  {
    if (close(log_fd) < 0)
    {
      log_error("Can not close original log file handler (%d): %s. "
                "Trying to continue...",
                (int) log_fd,
                (const char *) strerror(errno));
    }
  }

  return FALSE;
}


/************************************************************************/
/**
  Create PID file.

  @return
    TRUE  on error;
    FALSE on success.
*************************************************************************/

static bool create_pid_file()
{
  if (create_pid_file(Options::Daemon::angel_pid_file_name, getpid()))
  {
    log_error("Angel: can not create pid file (%s).",
              (const char *) Options::Daemon::angel_pid_file_name);

    return TRUE;
  }

  log_info("Angel: pid file (%s) created.",
           (const char *) Options::Daemon::angel_pid_file_name);

  return FALSE;
}


/************************************************************************/
/**
  SIGCHLD handler.

  Reap child, analyze child exit code, and set child_status
  appropriately.
*************************************************************************/

extern "C" void reap_child(int);

void reap_child(int __attribute__((unused)) signo)
{
  /* NOTE: As we have only one child, no need to cycle waitpid(). */

  int exit_code;

  if (waitpid(0, &exit_code, WNOHANG) > 0)
  {
    child_exit_code= exit_code;
    child_status= exit_code ? CHILD_NEED_RESPAWN : CHILD_EXIT_ANGEL;
  }
}


/************************************************************************/
/**
  SIGTERM, SIGHUP, SIGINT handler.

  Set termination status and return.
*************************************************************************/

extern "C" void terminate(int signo);
void terminate(int signo)
{
  shutdown_request_signo= signo;
}


/************************************************************************/
/**
  Angel main loop.

  @return
    The function returns exit status for global main():
      0  -- program completed successfully;
      !0 -- error occurred.
*************************************************************************/

static int angel_main_loop()
{
  /*
    Install signal handlers.

    NOTE: Although signal handlers are needed only for parent process
    (IM-angel), we should install them before fork() in order to avoid race
    condition (i.e. to be sure, that IM-angel will receive SIGCHLD in any
    case).
  */

  sigset_t wait_for_signals_mask;

  struct sigaction sa_chld;
  struct sigaction sa_term;
  struct sigaction sa_chld_orig;
  struct sigaction sa_term_orig;
  struct sigaction sa_int_orig;
  struct sigaction sa_hup_orig;

  log_info("Angel: setting necessary signal actions...");

  sigemptyset(&wait_for_signals_mask);

  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_handler= reap_child;
  sa_chld.sa_flags= SA_NOCLDSTOP;

  sigemptyset(&sa_term.sa_mask);
  sa_term.sa_handler= terminate;
  sa_term.sa_flags= 0;

  /* NOTE: sigaction() fails only if arguments are wrong. */

  sigaction(SIGCHLD, &sa_chld, &sa_chld_orig);
  sigaction(SIGTERM, &sa_term, &sa_term_orig);
  sigaction(SIGINT, &sa_term, &sa_int_orig);
  sigaction(SIGHUP, &sa_term, &sa_hup_orig);

  /* The main Angel loop. */

  while (true)
  {
    /* Spawn a new Manager. */

    log_info("Angel: forking Manager process...");

    switch (fork()) {
    case -1:
      log_error("Angel: can not fork IM-main: %s.",
                (const char *) strerror(errno));

      return -1;

    case 0:
      /*
        We are in child process, which will be IM-main:
          - Restore default signal actions to let the IM-main work with
            signals as he wishes;
          - Call Manager::main();
      */

      log_info("Angel: Manager process created successfully.");

      /* NOTE: sigaction() fails only if arguments are wrong. */

      sigaction(SIGCHLD, &sa_chld_orig, NULL);
      sigaction(SIGTERM, &sa_term_orig, NULL);
      sigaction(SIGINT, &sa_int_orig, NULL);
      sigaction(SIGHUP, &sa_hup_orig, NULL);

      log_info("Angel: executing Manager...");

      return Manager::main();
    }

    /* Wait for signals. */

    log_info("Angel: waiting for signals...");

    while (child_status == CHILD_OK && shutdown_request_signo == 0)
      sigsuspend(&wait_for_signals_mask);

    /* Exit if one of shutdown signals has been caught. */

    if (shutdown_request_signo)
    {
      log_info("Angel: received shutdown signal (%d). Exiting...",
               (int) shutdown_request_signo);

      return 0;
    }

    /* Manager process died. Respawn it if it was a failure. */

    if (child_status == CHILD_NEED_RESPAWN)
    {
      child_status= CHILD_OK;

      log_error("Angel: Manager exited abnormally (exit code: %d).",
                (int) child_exit_code);

      log_info("Angel: sleeping 1 second...");

      sleep(1); /* don't respawn too fast */

      log_info("Angel: respawning Manager...");

      continue;
    }

    /* Delete IM-angel PID file. */

    my_delete(Options::Daemon::angel_pid_file_name, MYF(0));

    /* IM-angel finished. */

    log_info("Angel: Manager exited normally. Exiting...");

    return 0;
  }
}


/************************************************************************/
/**
  Angel main function.

  @return
    The function returns exit status for global main():
      0  -- program completed successfully;
      !0 -- error occurred.
*************************************************************************/

int Angel::main()
{
  log_info("Angel: started.");

  /* Open log file. */

  if (open_log_file())
    return -1;

  /* Fork a new process. */

  log_info("Angel: daemonizing...");

  switch (fork()) {
  case -1:
    /*
      This is the main Instance Manager process, fork() failed.
      Log an error and bail out with error code.
    */

    log_error("fork() failed: %s.", (const char *) strerror(errno));
    return -1;

  case 0:
    /* We are in child process. Continue Angel::main() execution. */

    break;

  default:
    /*
      We are in the parent process. Return 0 so that parent exits
      successfully.
    */

    log_info("Angel: exiting from the original process...");

    return 0;
  }

  /* Detach child from controlling tty. */

  if (detach_process())
    return -1;

  /* Create PID file. */

  if (create_pid_file())
    return -1;

  /* Start Angel main loop. */

  return angel_main_loop();
}

#endif // __WIN__
