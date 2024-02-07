/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_TIME_T_INCLUDED
#define MY_TIME_T_INCLUDED
/**
  Portable time_t replacement.  Should be signed and hold seconds for
  1970-01-01 00:00:00 -- 3001-01-18 23:59:59 on an OS with 64 bits time_t.
  On OSes with 32 bits time_t, the values will be constrained further.
*/
#ifdef __cplusplus
using my_time_t = int64_t;
#else
typedef my_time_t int64_t;
#endif

/**
  Replacement of system's struct timeval to ensure we can carry 64 bit values
  even on a platform which has 64 bits time, but only 32 bits tv_sec member,
  notably Windows. We do use the system timeval when interfacing the C API
  calls, though, so in a few cases, e.g. by THD::{start_time, user_time},
  we need to convert between representations but mostly the struct is only used
  internally by MySQL so we can use our own.
*/
struct my_timeval {
  int64_t m_tv_sec;
  int64_t m_tv_usec;
};

#endif /* MY_TIME_T_INCLUDED */
