/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef XCOM_LOGGER_H
#define XCOM_LOGGER_H

/* Log levels definition for use without external logger */
typedef enum {
  LOG_FATAL = 0,
  LOG_ERROR = 1,
  LOG_WARN = 2,
  LOG_INFO = 3,
  LOG_DEBUG = 4,
  LOG_TRACE = 5
} xcom_log_level_t;

static const char* const log_levels[] = {"[XCOM_FATAL] ", "[XCOM_ERROR] ",
                                         "[XCOM_WARN] ",  "[XCOM_INFO] ",
                                         "[XCOM_DEBUG] ", "[XCOM_TRACE] "};

typedef void (*xcom_logger)(int level, const char* message);

#endif
