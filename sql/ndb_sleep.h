/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SLEEP_H
#define NDB_SLEEP_H

#if defined(_WIN32)
#include <winbase.h>
#else
#include <sys/select.h>
#include <time.h>
#endif

/* Wait a given number of milliseconds */
static inline
void ndb_milli_sleep(time_t milliseconds)
{
#if defined(_WIN32)
  Sleep((DWORD)milliseconds+1);      /* Sleep() has millisecond arg */
#else
  struct timeval t;
  t.tv_sec=  milliseconds / 1000L;
  t.tv_usec= milliseconds % 1000L;
  select(0,0,0,0,&t); /* sleep */
#endif
}


/* perform random sleep in the range milli_sleep to 5*milli_sleep */
static inline
void ndb_retry_sleep(unsigned milli_sleep)
{
  ndb_milli_sleep(milli_sleep + 5*(rand()%(milli_sleep/5)));
}

#endif
