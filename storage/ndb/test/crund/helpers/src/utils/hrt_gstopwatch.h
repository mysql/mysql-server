/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_gstopwatch.h
 *
 */

#ifndef _utils_hrt_gstopwatch
#define _utils_hrt_gstopwatch
#ifdef __cplusplus
extern "C" {
#endif

#include "hrt_stopwatch.h"

/*
 * High-Resolution Time Global Stopwatch Utility
 */

// extern hrt_stopwatch gsw;

/**
 * Initializes the global stopwatch with a given storage capacity for timemarks.
 */
extern void hrt_gsw_init(int cap);

/**
 * Releases all reources allocated by the global stopwatch.
 */
extern void hrt_gsw_close(void);

/**
 * Returns the index of the global stopwatch's last timemark.
 */
extern int hrt_gsw_top(void);

/**
 * Returns the number of timemarks the global stopwatch can hold.
 */
extern int hrt_gsw_capacity(void);

/**
 * Marks the time and stores that mark in the global stopwatch returning the
 * index.
 */
extern int hrt_gsw_pushmark(void);

/**
 * Removes the top timemark from the global stopwatch.
 */
extern void hrt_gsw_popmark(void);

/**
 * Returns the real time amount between two timemarks in microseconds
 * (i.e., y - x).
 */
extern double hrt_gsw_rtmicros(int y, int x);

/**
 * Returns the cpu time amount between two timemarks in microseconds
 * (i.e., y - x).
 */
extern double hrt_gsw_ctmicros(int y, int x);

/**
 * Clears all timemarks stored in the global stopwatch.
 */
extern void hrt_gsw_clear(void);

#ifdef __cplusplus
}
#endif
#endif
