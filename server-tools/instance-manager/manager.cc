/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include <my_global.h>
#include <signal.h>

#include "thread_repository.h"
#include "listener.h"
#include "log.h"


void manager(const char *socket_file_name)
{
  Thread_repository thread_repository;
  Listener_thread_args listener_args(thread_repository, socket_file_name);

  /* write pid file */

  /* block signals */
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGHUP);

  /* all new threads will inherite this signal mask */
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
  {
    /* create the listener */
    pthread_t listener_thd_id;
    pthread_attr_t listener_thd_attr;

    pthread_attr_init(&listener_thd_attr);
    pthread_attr_setdetachstate(&listener_thd_attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&listener_thd_id, &listener_thd_attr, listener,
                       &listener_args))
      die("manager(): pthread_create(listener) failed");
  }
  /*
    To work nicely with LinuxThreads, the signal thread is the first thread
    in the process.
  */
  int signo;
  sigwait(&mask, &signo);
  thread_repository.deliver_shutdown();
  /* don't pthread_exit to kill all threads who did not shut down in time */
}
