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

#ifndef MY_XP_UTIL_INCLUDED
#define MY_XP_UTIL_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/mysql_gcs.h"

#ifdef _WIN32
#include <winsock2.h>  // Must come before <windows.h>.
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <Winsock2.h>
#include <ws2tcpip.h>
#ifndef INET_ADDRSTRLEN
#define socklen_t int
#endif
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#endif
#include <errno.h>
#include <stdint.h>
#include <cassert>
#include <iostream>

#define INT_MAX32     0x7FFFFFFFL
#define MY_MIN(a, b)  ((a) < (b) ? (a) : (b))


#ifdef _WIN32

#define OFFSET_TO_EPOC ((__int64) 134774 * 24 * 60 * 60 * 1000 * 1000 * 10)
#define MS 10000000

#include<time.h>

#endif


/**
  @class My_xp_util

  Class where cross platform utilities reside as static methods.
*/
class My_xp_util
{
public:
  /**
    Current thread sleeps for the parameterized number of seconds.

    @param seconds number of seconds for invoking thread to sleep
  */

  static void sleep_seconds(unsigned int seconds);

  /* Code ported from MySQL Server to deal with timespec. */
#ifdef _WIN32
  static uint64_t query_performance_frequency, query_performance_offset;

  static void win_init_time();
#endif

  /**
    Init time.
  */

  static void init_time();


  /**
    Get the system's time.

    @return system's time in 100s of nanoseconds
  */

  static uint64_t getsystime(void);


  /**
    Set the value of the timespec struct equal to the argument in nanoseconds.
  */

  static inline void set_timespec_nsec(struct timespec *abstime, uint64_t nsec)
  {
    uint64_t now= My_xp_util::getsystime() + (nsec / 100);
    uint64_t tv_sec= now / 10000000ULL;
  #if SIZEOF_TIME_T < SIZEOF_LONG_LONG
    /* Ensure that the number of seconds don't overflow. */
    tv_sec= MY_MIN(tv_sec, ((uint64_t)INT_MAX32));
  #endif
    abstime->tv_sec=  (time_t)tv_sec;
    abstime->tv_nsec= (now % 10000000ULL) * 100 + (nsec % 100);
  }


  /**
    Set the value of the timespec struct equal to the argument in seconds.
  */

  static inline void set_timespec(struct timespec *abstime, uint64_t sec)
  {
    My_xp_util::set_timespec_nsec(abstime, sec * 1000000000ULL);
  }


  /**
    Compare two timespec structs.

    @retval  1 If ts1 ends after ts2.
    @retval -1 If ts1 ends before ts2.
    @retval  0 If ts1 is equal to ts2.
  */

  static inline int cmp_timespec(struct timespec *ts1, struct timespec *ts2)
  {
    if (ts1->tv_sec > ts2->tv_sec ||
        (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec > ts2->tv_nsec))
      return 1;
    if (ts1->tv_sec < ts2->tv_sec ||
        (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec < ts2->tv_nsec))
      return -1;
    return 0;
  }


  /**
    Diff two timespec structs.
    ts1 has to be larger than ts2, otherwise it will return unexpected value.

    @return  difference between the two arguments.
  */

  static inline uint64_t diff_timespec(struct timespec *ts1,
                                       struct timespec *ts2)
  {
    return static_cast<uint64_t>(ts1->tv_sec - ts2->tv_sec) * 1000000000ULL +
      static_cast<uint64_t>(ts1->tv_nsec) - static_cast<uint32_t>(ts2->tv_nsec);
  }
};


/**
  @class My_xp_socket_util

  Interface for socket utility methods.
*/
class My_xp_socket_util
{
public:

  /**
    Disable Nagle algorithm in the specified socket.

    @param fd file descriptor of the socket
  */

  virtual int disable_nagle_in_socket(int fd)= 0;

  virtual ~My_xp_socket_util() {};
};


class My_xp_socket_util_impl : public My_xp_socket_util
{
public:
  int disable_nagle_in_socket(int fd);
  explicit My_xp_socket_util_impl() {};
  ~My_xp_socket_util_impl() {};
};

#endif // MY_XP_UTIL_INCLUDED
