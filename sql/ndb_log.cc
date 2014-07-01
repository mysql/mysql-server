/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_log.h"

// Use log.h until there is a plugin API which provides printing to error log
// without polluting the message with it's own hardcoded string and without
// need to pass in a MYSQL_PLUGIN pointer. Presumably 'my_plugin_log_service'
// can be extended with a my_log_message(level, prefix, message, ...) function
#include "log.h"


/*
  Print message to MySQL Server's error log(s)

  @param loglevel    Selects the loglevel used when
                     printing the message to log.
  @param prefix      Prefix to be used in front of the message in
                     addition to "NDB" i.e "NDB <prefix>:"
  @param[in]  fmt    printf-like format string
  @param[in]  ap     Arguments

*/

void
ndb_log_print(enum ndb_log_loglevel loglevel,
              const char* prefix, const char* fmt, va_list args)
{
  DBUG_ASSERT(fmt);

  // Assemble the message
  char msg_buf[512];
  (void)my_vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);

  // Print message to MySQL error log
  switch (loglevel)
  {
    case NDB_LOG_ERROR_LEVEL:
      if (prefix)
        sql_print_error("NDB %s: %s", prefix, msg_buf);
      else
        sql_print_error("NDB: %s", msg_buf);
      break;
    case NDB_LOG_WARNING_LEVEL:
      if (prefix)
        sql_print_warning("NDB %s: %s", prefix, msg_buf);
      else
        sql_print_warning("NDB: %s", msg_buf);
      break;
    case NDB_LOG_INFORMATION_LEVEL:
      if (prefix)
        sql_print_information("NDB %s: %s", prefix, msg_buf);
      else
        sql_print_information("NDB: %s", msg_buf);
      break;
  }
}


void
ndb_log_info(const char* fmt, ...)
{
  DBUG_ASSERT(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, NULL, fmt, args);
  va_end(args);
}


void
ndb_log_warning(const char* fmt, ...)
{
  DBUG_ASSERT(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_WARNING_LEVEL, NULL, fmt, args);
  va_end(args);
}


void
ndb_log_error(const char* fmt, ...)
{
  DBUG_ASSERT(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_ERROR_LEVEL, NULL, fmt, args);
  va_end(args);
}


// the verbose level is currently controlled by "ndb_extra_logging"
extern ulong opt_ndb_extra_logging;

unsigned
ndb_log_get_verbose_level(void)
{
  return opt_ndb_extra_logging;
}


void
ndb_log_verbose(unsigned verbose_level, const char* fmt, ...)
{
  DBUG_ASSERT(fmt);

  // Print message only if verbose level is set high enough
  if (ndb_log_get_verbose_level() < verbose_level)
    return;

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, NULL, fmt, args);
  va_end(args);
}

