/*
   Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_LOG_H
#define NDB_LOG_H

#include <stdarg.h>

#include "my_compiler.h"

// Returns the current verbose level
unsigned ndb_log_get_verbose_level(void);

/*
  Write messages to the MySQL Servers error log(s)

  NOTE! Messages will always be prefixed with "NDB:" and
  "NDB <prefix>:" if one of the prefix functions are used
*/
void ndb_log_info(const char *fmt, ...) MY_ATTRIBUTE((format(printf, 1, 2)));

void ndb_log_warning(const char *fmt, ...) MY_ATTRIBUTE((format(printf, 1, 2)));

void ndb_log_error(const char *fmt, ...) MY_ATTRIBUTE((format(printf, 1, 2)));

void ndb_log_verbose(unsigned verbose_level, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));

enum ndb_log_loglevel {
  NDB_LOG_ERROR_LEVEL = 0,
  NDB_LOG_WARNING_LEVEL = 1,
  NDB_LOG_INFORMATION_LEVEL = 2
};
void ndb_log_print(enum ndb_log_loglevel loglevel, const char *prefix,
                   const char *fmt, va_list va_args)
    MY_ATTRIBUTE((format(printf, 3, 0)));

/*
  @brief Write potentially long message to standard error.

  @note The above ndb_log* functions all have limitations in terms
  of how long messages they can write to the MySQL Server error log.
  When it's necessary to write something which is known to be longer
  than the limit(normally 512 bytes), this function can be used instead.

  @param[in]  fmt    printf-like format string
  @param[in]  ...    Variable arguments matching format string
*/
void ndb_log_error_dump(const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));

/*
  @brief All the logs printed before the error log has been opened are
  buffered and printed later to the right file after the error log has
  been opened. This function flushes out all the buffered logs to the
  stderr. This needs to be called if the ndbcluster plugin exits with an
  error before the error log has been opened.
*/
void ndb_log_flush_buffered_messages();

#endif
