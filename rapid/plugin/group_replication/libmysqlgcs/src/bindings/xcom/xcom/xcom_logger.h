/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_LOGGER_H
#define XCOM_LOGGER_H

/*
  Log levels definition for use without external logger. Note that this can
  only be changed in lock-step with GCS. Otherwise, things will break when
  XCOM is used along with GCS.

*/
typedef enum
{
  XCOM_LOG_FATAL= 0,
  XCOM_LOG_ERROR= 1,
  XCOM_LOG_WARN= 2,
  XCOM_LOG_INFO= 3,
} xcom_log_level_t;

static const char* const xcom_log_levels[]=
{
  "[MYSQL_XCOM_FATAL] ",
  "[MYSQL_XCOM_ERROR] ",
  "[MYSQL_XCOM_WARN] ",
  "[MYSQL_XCOM_INFO] ",
};

/*
Debug options that are defined for use without external debugger. Note that
this can only be changed in lock-step with GCS. Otherwise, things will break
when XCOM is used along with GCS.

Assuming that we are not using all bits available to define a debug level,
GCS_INVALID_DEBUG will give us the last possible entry that is not used.

The GCS_DEBUG_NONE, GCS_DEBUG_ALL and GCS_INVALID_DEBUG are options that apply
to both GCS and XCOM but we don't prefix it with XCOM to avoid big names.
*/
#if ! defined(GCS_XCOM_DEBUG_INFORMATION)
#define GCS_XCOM_DEBUG_INFORMATION
typedef enum
{
   GCS_DEBUG_NONE    = 0x00000000,
   GCS_DEBUG_BASIC   = 0x00000001,
   GCS_DEBUG_TRACE   = 0x00000002,
   XCOM_DEBUG_BASIC  = 0x00000004,
   XCOM_DEBUG_TRACE  = 0x00000008,
   GCS_INVALID_DEBUG = ~(0x7FFFFFFF),
   GCS_DEBUG_ALL     = ~(GCS_DEBUG_NONE)
} gcs_xcom_debug_option_t;

static const char* const gcs_xcom_debug_strings[]=
{
  "GCS_DEBUG_BASIC",
  "GCS_DEBUG_TRACE",
  "XCOM_DEBUG_BASIC",
  "XCOM_DEBUG_TRACE",
  "GCS_DEBUG_ALL",
  "GCS_DEBUG_NONE",
};
#endif


typedef void (*xcom_logger)(const int64_t level, const char *message);
typedef void (*xcom_debugger)(const char *format, ...) MY_ATTRIBUTE((format(printf, 1, 2)));
typedef int (*xcom_debugger_check)(const int64_t debug_options);
#endif
