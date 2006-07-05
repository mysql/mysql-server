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

#include "priv.h"

#include <my_global.h>
#include <mysql_com.h>

#if defined(__ia64__) || defined(__ia64)
/*
  We can live with 32K, but reserve 64K. Just to be safe.
  On ia64 we need to reserve double of the size.
*/
#define IM_THREAD_STACK_SIZE    (128*1024L)
#else
#define IM_THREAD_STACK_SIZE    (64*1024)
#endif


/* the pid of the manager process (of the signal thread on the LinuxThreads) */
pid_t manager_pid;

/*
  This flag is set if mysqlmanager has detected that it is running on the
  system using LinuxThreads
*/
bool linuxthreads;

/*
  The following string must be less then 80 characters, as
  mysql_connection.cc relies on it
*/
const LEX_STRING mysqlmanager_version= { C_STRING_WITH_LEN("1.0-beta") };

const unsigned char protocol_version= PROTOCOL_VERSION;

unsigned long net_buffer_length= 16384;

unsigned long max_allowed_packet= 16384;

unsigned long net_read_timeout= NET_WAIT_TIMEOUT;    // same as in mysqld

unsigned long net_write_timeout= 60;            // same as in mysqld

unsigned long net_retry_count= 10;              // same as in mysqld

/* needed by net_serv.cc */
unsigned int test_flags= 0;
unsigned long bytes_sent = 0L, bytes_received = 0L;
unsigned long mysqld_net_retry_count = 10L;
unsigned long open_files_limit;

/*
  Change the stack size and start a thread. Return an error if either
  pthread_attr_setstacksize or pthread_create fails.
  Arguments are the same as for pthread_create().
*/

int set_stacksize_n_create_thread(pthread_t  *thread, pthread_attr_t *attr,
                                  void *(*start_routine)(void *), void *arg)
{
  int rc= 0;

#ifndef __WIN__
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN      32768
#endif
  /*
    Set stack size to be safe on the platforms with too small
    default thread stack.
  */
  rc= pthread_attr_setstacksize(attr,
                                (size_t) (PTHREAD_STACK_MIN +
                                          IM_THREAD_STACK_SIZE));
#endif
  if (!rc)
    rc= pthread_create(thread, attr, start_routine, arg);
  return rc;
}
