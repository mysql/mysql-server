/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"

#include <errno.h>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"

void My_xp_util::sleep_seconds(unsigned int seconds)
{
#ifdef _WIN32
  Sleep(seconds * 1000);
#else
  sleep(seconds);
#endif
}

void My_xp_util::init_time()
{
#ifdef _WIN32
  win_init_time();
#endif
}

#ifdef _WIN32
uint64_t My_xp_util::query_performance_frequency= 0;
uint64_t My_xp_util::query_performance_offset= 0;

void My_xp_util::win_init_time()
{
  /* The following is used by time functions */
  FILETIME ft;
  LARGE_INTEGER li, t_cnt;

  if (QueryPerformanceFrequency((LARGE_INTEGER *)&query_performance_frequency) == 0)
    query_performance_frequency= 0;
  else
  {
    GetSystemTimeAsFileTime(&ft);
    li.LowPart=  ft.dwLowDateTime;
    li.HighPart= ft.dwHighDateTime;
    query_performance_offset= li.QuadPart-OFFSET_TO_EPOC;
    QueryPerformanceCounter(&t_cnt);
    query_performance_offset-= (t_cnt.QuadPart /
                                query_performance_frequency * MS +
                                t_cnt.QuadPart %
                                query_performance_frequency * MS /
                                query_performance_frequency);
  }
}

#endif


uint64_t My_xp_util::getsystime()
{
#ifdef _WIN32
  LARGE_INTEGER t_cnt;
  if (query_performance_frequency)
  {
    QueryPerformanceCounter(&t_cnt);
    return ((t_cnt.QuadPart / query_performance_frequency * 10000000) +
            ((t_cnt.QuadPart % query_performance_frequency) * 10000000 /
             query_performance_frequency) + query_performance_offset);
  }
  return 0;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (uint64_t)tv.tv_sec*10000000+(uint64_t)tv.tv_usec*10;
#endif
}


int My_xp_socket_util_impl::disable_nagle_in_socket(int fd)
{
  int ret= -1;
  if(fd != -1)
  {
    int optval= 1;
    /* Casting optval to char * so Windows does not complain. */
    ret= setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval,
                    static_cast<socklen_t>(sizeof(int)));
  }
  if (ret < 0)
    MYSQL_GCS_LOG_ERROR("Error manipulating a connection's socket. Error: "
                        << errno)
  return ret;
}
