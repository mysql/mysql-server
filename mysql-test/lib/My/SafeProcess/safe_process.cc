/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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


/*
  Utility program that encapsulates process creation, monitoring
  and bulletproof process cleanup

  Usage:
    safe_process [options to safe_process] -- progname arg1 ... argn

  To safeguard mysqld you would invoke safe_process with a few options
  for safe_process itself followed by a double dash to indicate start
  of the command line for the program you really want to start

  $> safe_process --output=output.log -- mysqld --datadir=var/data1 ...

  This would redirect output to output.log and then start mysqld,
  once it has done that it will continue to monitor the child as well
  as the parent.

  The safe_process then checks the follwing things:
  1. Child exits, propagate the childs return code to the parent
     by exiting with the same return code as the child.

  2. Parent dies, immediately kill the child and exit, thus the
     parent does not need to properly cleanup any child, it is handled
     automatically.

  3. Signal's recieced by the process will trigger same action as 2)

*/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

int verbose= 0;
int terminated= 0;
pid_t child_pid= -1;
char safe_process_name[32]= {0};


static void message(const char* fmt, ...)
{
  if (!verbose)
    return;
  va_list args;
  fprintf(stderr, "%s: ", safe_process_name);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  fflush(stderr);
}


static void die(const char* fmt, ...)
{
  va_list args;
  fprintf(stderr, "%s: FATAL ERROR, ", safe_process_name);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  if (int last_err= errno)
    fprintf(stderr, "error: %d, %s\n", last_err, strerror(last_err));
  exit(1);
}


static void kill_child(void)
{
  int status= 0;

  message("Killing child: %d", child_pid);
  // Terminate whole process group
  kill(-child_pid, SIGKILL);

  pid_t ret_pid= waitpid(child_pid, &status, 0);
  if (ret_pid == child_pid)
  {
    int exit_code= 1;
    if (WIFEXITED(status))
    {
      // Process has exited, collect return status
      exit_code= WEXITSTATUS(status);
      message("Child exit: %d", exit_code);
      // Exit with exit status of the child
      exit(exit_code);
    }

    if (WIFSIGNALED(status))
      message("Child killed by signal: %d", WTERMSIG(status));

    exit(exit_code);
  }
  exit(1);
}


extern "C" void handle_abort(int sig)
{
    message("Got signal %d, child_pid: %d, sending ABRT", sig, child_pid);

    if (child_pid > 0) {
	kill (-child_pid, SIGABRT);	// Don't wait for it to terminate
    }
}


extern "C" void handle_signal(int sig)
{
  message("Got signal %d, child_pid: %d", sig, child_pid);
  terminated= 1;

  if (child_pid > 0)
    kill_child();

  // Ignore further signals
  signal(SIGTERM, SIG_IGN);
  signal(SIGINT,  SIG_IGN);

  // Continune execution, allow the child to be started and
  // finally terminated by monitor loop
}


int main(int argc, char* const argv[] )
{
  char* const* child_argv= 0;
  pid_t own_pid= getpid();
  pid_t parent_pid= getppid();
  bool nocore = false;
  struct sigaction sa,sa_abort;

  sa.sa_handler= handle_signal;
  sa.sa_flags= SA_NOCLDSTOP;
  sigemptyset(&sa.sa_mask);

  sa_abort.sa_handler= handle_abort;
  sigemptyset(&sa_abort.sa_mask);
  /* Install signal handlers */
  sigaction(SIGTERM, &sa,NULL);
  sigaction(SIGINT, &sa,NULL);
  sigaction(SIGCHLD, &sa,NULL);
  sigaction(SIGABRT, &sa_abort,NULL);

  sprintf(safe_process_name, "safe_process[%ld]", (long) own_pid);

  message("Started");

  /* Parse arguments */
  for (int i= 1; i < argc; i++) {
    const char* arg= argv[i];
    if (strcmp(arg, "--") == 0 && strlen(arg) == 2) {
      /* Got the "--" delimiter */
      if (i >= argc)
        die("No real args -> nothing to do");
      child_argv= &argv[i+1];
      break;
    } else {
      if ( strcmp(arg, "--verbose") == 0 )
        verbose++;
      else if ( strncmp(arg, "--parent-pid", 12) == 0 )
      {
        /* Override parent_pid with a value provided by user */
        const char* start;
        if ((start= strstr(arg, "=")) == NULL)
          die("Could not find start of option value in '%s'", arg);
        start++; /* Step past = */
        if ((parent_pid= atoi(start)) == 0)
          die("Invalid value '%s' passed to --parent-id", start);
      }
      else if ( strcmp(arg, "--nocore") == 0 )
      {
        nocore = true;	// Don't allow the process to dump core
      }
      else if ( strncmp (arg, "--env ", 6) == 0 )
      {
	putenv(strdup(arg+6));
      }
      else
        die("Unknown option: %s", arg);
    }
  }
  if (!child_argv || *child_argv == 0)
    die("nothing to do");

  message("parent_pid: %d", parent_pid);
  if (parent_pid == own_pid)
    die("parent_pid is equal to own pid!");

  char buf;
  int pfd[2];
  if (pipe(pfd) == -1)
    die("Failed to create pipe");

  /* Create the child process */
  while((child_pid= fork()) == -1)
  {
    message("fork failed");
    sleep(1);
  }

  if (child_pid == 0)
  {
    close(pfd[0]); // Close unused read end

    // Use default signal handlers in child
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    // Make this process it's own process group to be able to kill
    // it and any childs(that hasn't changed group themself)
    setpgid(0, 0);

    if (nocore)
    {
      struct rlimit corelim = { 0, 0 };
      if (setrlimit (RLIMIT_CORE, &corelim) < 0)
      {
        message("setrlimit failed, errno=%d", errno);
      }
    }

    // Signal that child is ready
    buf= 37;
    if ((write(pfd[1], &buf, 1)) < 1)
      die("Failed to signal that child is ready");
    // Close write end
    close(pfd[1]);

    if (execvp(child_argv[0], child_argv) < 0)
      die("Failed to exec child");
  }

  close(pfd[1]); // Close unused write end

  // Wait for child to signal it's ready
  if ((read(pfd[0], &buf, 1)) < 1)
    die("Failed to read signal from child");

  if (buf != 37)
    die("Didn't get 37 from pipe");
  close(pfd[0]); // Close read end

  /* Monitor loop */
  message("Started child %d, terminated: %d", child_pid, terminated);

  while(!terminated)
  {
    // Check if parent is still alive
    if (kill(parent_pid, 0) != 0){
      message("Parent is not alive anymore");
      break;
    }

    // Check if child has exited, normally this will be
    // detected immediately with SIGCHLD handler
    int status= 0;
    pid_t ret_pid= waitpid(child_pid, &status, WNOHANG);
    if (ret_pid == child_pid)
    {
      int ret_code= 2;
      if (WIFEXITED(status))
      {
        // Process has exited, collect return status
        ret_code= WEXITSTATUS(status);
        message("Child exit: %d", ret_code);
        // Exit with exit status of the child
        exit(ret_code);
      }

      if (WIFSIGNALED(status))
        message("Child killed by signal: %d", WTERMSIG(status));

      exit(ret_code);
    }
    sleep(1);
  }
  kill_child();

  return 1;
}

