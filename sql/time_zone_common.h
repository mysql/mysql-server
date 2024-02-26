#ifndef TIME_ZONE_COMMON_H
#define TIME_ZONE_COMMON_H
/* Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include "my_alloc.h"   // MEM_ROOT
#include "my_time_t.h"  // my_time_t
/**
  @file
  Contains common functionality shared between mysqld and
  mysql_tz_info_to_sql.
*/

/*
  Now we don't use abbreviations in server but we will do this in future.
*/
#if !defined(NDEBUG)
/* Let use abbreviations for debug purposes */
#undef ABBR_ARE_USED
#define ABBR_ARE_USED
#endif /* !defined(NDEBUG) */

/* Structure describing local time type (e.g. Moscow summer time (MSD)) */
typedef struct ttinfo {
  long tt_gmtoff;     // Offset from UTC in seconds
  unsigned tt_isdst;  // Is daylight saving time or not. Used to set tm_isdst
#ifdef ABBR_ARE_USED
  unsigned tt_abbrind;  // Index of start of abbreviation for this time type.
#endif
  /*
    We don't use tt_ttisstd and tt_ttisgmt members of original elsie-code
    struct since we don't support POSIX-style TZ descriptions in variables.
  */
} TRAN_TYPE_INFO;

/* Structure describing leap-second corrections. */
typedef struct lsinfo {
  my_time_t ls_trans;  // Transition time
  long ls_corr;        // Correction to apply
} LS_INFO;

/*
  Structure with information describing ranges of my_time_t shifted to local
  time (my_time_t + offset). Used for local MYSQL_TIME -> my_time_t conversion.
  See comments for TIME_to_gmt_sec() for more info.
*/
typedef struct revtinfo {
  long rt_offset;    // Offset of local time from UTC in seconds
  unsigned rt_type;  // Type of period 0 - Normal period. 1 - Spring time-gap
} REVT_INFO;

#ifdef TZNAME_MAX
#define MY_TZNAME_MAX TZNAME_MAX
#endif
#ifndef TZNAME_MAX
#define MY_TZNAME_MAX 255
#endif

/*
  Structure which fully describes time zone which is
  described in our db or in zoneinfo files.
*/
struct TIME_ZONE_INFO {
  unsigned leapcnt;  // Number of leap-second corrections
  unsigned timecnt;  // Number of transitions between time types
  unsigned typecnt;  // Number of local time types
  size_t charcnt;    // Number of characters used for abbreviations
  unsigned
      revcnt;  // Number of transition descr. for TIME->my_time_t conversion
  /* The following are dynamical arrays are allocated in MEM_ROOT */
  my_time_t *ats;        // Times of transitions between time types
  uchar *types;          // Local time types for transitions
  TRAN_TYPE_INFO *ttis;  // Local time types descriptions
#ifdef ABBR_ARE_USED
  /* Storage for local time types abbreviations. They are stored as ASCIIZ */
  char *chars;
#endif
  /*
    Leap seconds corrections descriptions, this array is shared by
    all time zones who use leap seconds.
  */
  LS_INFO *lsis;
  /*
    Starting points and descriptions of shifted my_time_t (my_time_t + offset)
    ranges on which shifted my_time_t -> my_time_t mapping is linear or
    undefined. Used for tm -> my_time_t conversion.
  */
  my_time_t *revts;
  REVT_INFO *revtis;
  /*
    Time type which is used for times smaller than first transition or if
    there are no transitions at all.
  */
  TRAN_TYPE_INFO *fallback_tti;
};

/*
  Finish preparation of time zone description for use in TIME_to_gmt_sec()
  and gmt_sec_to_TIME() functions.

  SYNOPSIS
    prepare_tz_info()
      sp - pointer to time zone description
      storage - pointer to MEM_ROOT where arrays for map allocated

  DESCRIPTION
    First task of this function is to find fallback time type which will
    be used if there are no transitions or we have moment in time before
    any transitions.
    Second task is to build "shifted my_time_t" -> my_time_t map used in
    MYSQL_TIME -> my_time_t conversion.
    Note: See description of TIME_to_gmt_sec() function first.
    In order to perform MYSQL_TIME -> my_time_t conversion we need to build
  table which defines "shifted by tz offset and leap seconds my_time_t" ->
    my_time_t function which is almost the same (except ranges of ambiguity)
    as reverse function to piecewise linear function used for my_time_t ->
    "shifted my_time_t" conversion and which is also specified as table in
    zoneinfo file or in our db (It is specified as start of time type ranges
    and time type offsets). So basic idea is very simple - let us iterate
    through my_time_t space from one point of discontinuity of my_time_t ->
    "shifted my_time_t" function to another and build our approximation of
    reverse function. (Actually we iterate through ranges on which
    my_time_t -> "shifted my_time_t" is linear function).

  RETURN VALUES
    0	Ok
    1	Error
*/
bool prepare_tz_info(TIME_ZONE_INFO *sp, MEM_ROOT *storage);

#endif  // TIME_ZONE_COMMON_H
