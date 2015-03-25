/* Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysqld_daemon.h"
#include "mysqld.h"
#include "log.h"

#include <unistd.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/**
  Daemonize mysqld.

  This function does sysv style of daemonization of mysqld.

  @return - returns write end of the pipe file descriptor
            which is used to notify the parent to exit.
*/
int mysqld::runtime::mysqld_daemonize()
{
  int pipe_fd[2];
  if (pipe(pipe_fd) < 0)
    return -1;

  pid_t pid= fork();
  if (pid == -1)
  {
    // Error
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return -1;
  }

  if (pid != 0)
  {
    // Parent, close write end of pipe.
    close(pipe_fd[1]);

    // Wait for first child to fork successfully.
    int rc,status;
    char waitstatus;
    while ((rc= waitpid(pid, &status, 0)) == -1 &&
           errno == EINTR)
    {
      // Retry if errno is EINTR.
    }
    if (rc == -1)
    {
      fprintf(stderr, "Unable to wait for process %lld\n",
                      static_cast<long long>(pid));
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return -1;
    }

    // Exit parent on signal from grand child
    rc= read(pipe_fd[0], &waitstatus, 1);
    close(pipe_fd[0]);

    if (rc != 1)
    {
      fprintf(stderr, "Unable to determine if daemon is running: %s\n",
                      strerror(errno));
      exit(MYSQLD_ABORT_EXIT);
    }
    else if (waitstatus != 1)
    {
      fprintf(stderr, "Initialization of mysqld failed: %d\n", waitstatus);
      exit(MYSQLD_ABORT_EXIT);
    }
    _exit(MYSQLD_SUCCESS_EXIT);
  }
  else
  {
    // Child, close read end of pipe file descriptor.
    close(pipe_fd[0]);

    int stdinfd;
    if ((stdinfd= open("/dev/null", O_RDONLY)) <= STDERR_FILENO)
    {
      close(pipe_fd[1]);
      exit(MYSQLD_ABORT_EXIT);
    }

    if (! (dup2(stdinfd, STDIN_FILENO) != STDIN_FILENO)
        && (setsid() > -1))
    {
      close(stdinfd);
      pid_t grand_child_pid= fork();
      switch (grand_child_pid)
      {
        case 0: // Grand child
          return pipe_fd[1];
        case -1:
          close(pipe_fd[1]);
          _exit(MYSQLD_FAILURE_EXIT);
        default:
          _exit(MYSQLD_SUCCESS_EXIT);
      }
    }
    else
    {
      close(stdinfd);
      close(pipe_fd[1]);
      _exit(MYSQLD_SUCCESS_EXIT);
    }
  }
}

/**
  Signal parent to exit.

  @param pipe_write_fd File Descriptor of write end of pipe.

  @param status status of the initialization done by grand child.
                1 means initialization complete and the server
                  is ready to accept client connections.
                0 means intialization aborted due to some failure.

  @note This function writes the status to write end of pipe.
  This notifies the parent which is block on read end of pipe.
*/
void mysqld::runtime::signal_parent(int pipe_write_fd, char status)
{
  if (pipe_write_fd != -1)
  {
    while (write(pipe_write_fd, &status, 1) == -1 && errno == EINTR)
    {
    // Retry write syscall if errno is EINTR.
    }

    close(pipe_write_fd);
  }
}
