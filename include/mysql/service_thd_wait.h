/* Copyright (C) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_THD_WAIT_INCLUDED
#define MYSQL_SERVICE_THD_WAIT_INCLUDED

/**
  @file include/mysql/service_thd_wait.h
  This service provides functions for plugins and storage engines to report
  when they are going to sleep/stall.
  
  SYNOPSIS
  thd_wait_begin() - call just before a wait begins
  thd                     Thread object
                          Use NULL if the thd is NOT known.
  wait_type               Type of wait
                          1 -- short wait (e.g. for mutex)
                          2 -- medium wait (e.g. for disk io)
                          3 -- large wait (e.g. for locked row/table)
  NOTES
    This is used by the threadpool to have better knowledge of which
    threads that currently are actively running on CPUs. When a thread
    reports that it's going to sleep/stall, the threadpool scheduler is
    free to start another thread in the pool most likely. The expected wait
    time is simply an indication of how long the wait is expected to
    become, the real wait time could be very different.

  thd_wait_end() called immediately after the wait is complete

  thd_wait_end() MUST be called if thd_wait_begin() was called.

  Using thd_wait_...() service is optional but recommended.  Using it will
  improve performance as the thread pool will be more active at managing the
  thread workload.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _thd_wait_type_e {
  THD_WAIT_MUTEX= 1,
  THD_WAIT_DISKIO= 2,
  THD_WAIT_ROW_TABLE_LOCK= 3,
  THD_WAIT_GLOBAL_LOCK= 4
} thd_wait_type;

extern struct thd_wait_service_st {
  void (*thd_wait_begin_func)(MYSQL_THD, thd_wait_type);
  void (*thd_wait_end_func)(MYSQL_THD);
} *thd_wait_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define thd_wait_begin(_THD, _WAIT_TYPE) \
  thd_wait_service->thd_wait_begin_func(_THD, _WAIT_TYPE)
#define thd_wait_end(_THD) thd_wait_service->thd_wait_end_func(_THD)

#else

void thd_wait_begin(MYSQL_THD thd, thd_wait_type wait_type);
void thd_wait_end(MYSQL_THD thd);

#endif

#ifdef __cplusplus
}
#endif

#endif

