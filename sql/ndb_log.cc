/*
   Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_log.h"

#include <stdio.h>

#include "my_dbug.h"
#include "mysqld_error.h"
/*
  Implements a logging interface for the ndbcluster
  plugin using the LogEvent class as defined in log_builtins.h
  Beware, the #include ordering here matters!
*/
#include "sql/log.h"
#include <mysql/components/services/log_builtins.h>


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

  int prio;

  // Assemble the message
  char msg_buf[512];
  (void)vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);

  // Print message to MySQL error log
  switch (loglevel)
  {
    case NDB_LOG_ERROR_LEVEL:
      prio= ERROR_LEVEL;
      break;
    case NDB_LOG_WARNING_LEVEL:
      prio= WARNING_LEVEL;
      break;
    case NDB_LOG_INFORMATION_LEVEL:
      prio= INFORMATION_LEVEL;
      break;
    default:
      // Should never happen, crash in debug
      DBUG_ABORT();
      prio = ERROR_LEVEL;
      break;
  }

  if (prefix)
    LogErr(prio, ER_NDB_LOG_ENTRY_WITH_PREFIX, prefix, msg_buf);
  else
    LogErr(prio, ER_NDB_LOG_ENTRY, msg_buf);
}




/*
  Automatically detect any log message prefix used by the caller, these
  are important in order to distinguish which subsystem of ndbcluster
  generated the log printout.

  @param fmt             The log message format, it may only start with one
                         of the allowed subsystem prefixes or none at all.

  @param prefix[out]     Pointer to detected prefix or NULL if none was
                         detected.
  @param fmt_start[out]  Pointer to new start of the format string in case the
                         subsystem prefix has been stripped

  @note  In debug compile the function will perform some additional
         checks to make sure that the format string has one of the
         allowed subsystem prefixes or none at all. The intention is that
         faulty prefix usage should be detected but allowed
         otherwise.

  @note This code is primarily written for backwards compatibility of log
        messages, thus allowing them to be forward ported with too much
        problem. New implementations should not add new "allowed subsystems"
        or otherwise modify this code, but rather use the logging functions
        of Ndb_component where the prefix will be automatically set correct.
*/

static
void
ndb_log_detect_prefix(const char* fmt,
                      const char** prefix, const char** fmt_start)
{
  DBUG_ENTER("ndb_log_detect_prefix");
  DBUG_PRINT("enter", ("fmt: '%s'", fmt));

  // Check if string starts with "NDB <subsystem>:" by reading
  // at most 15 chars whithout colon, then a colon and space
  char subsystem[16], colon[2];
  if (sscanf(fmt, "NDB %15[^:]%1[:] ", subsystem, colon) == 2)
  {
    DBUG_PRINT("info",("detected subsystem: '%s'", subsystem));
    static
    const char* allowed_prefixes[] =
    {
      "Binlog", // "NDB Binlog: "
      "Slave"   // "NDB Slave: "
    };
    const size_t num_allowed_prefixes =
        sizeof(allowed_prefixes)/sizeof(allowed_prefixes[0]);

    // Check if subsystem is in the list of allowed subsystem
    for (size_t i = 0; i < num_allowed_prefixes; i++)
    {
      const char* allowed_prefix = allowed_prefixes[i];

      DBUG_PRINT("info", ("checking allowed_prefix: '%s'",
                          allowed_prefix));
      if (strncmp(subsystem, allowed_prefix, strlen(allowed_prefix)) == 0)
      {
        // String started with an allowed subsystem prefix, return
        // pointer to prefix and new start of format string
        *prefix = allowed_prefix;
        *fmt_start = fmt +
                     4 + /* "NDB " */
                     strlen(allowed_prefix) +
                     2; /* ": " */
        DBUG_PRINT("info", ("Found! Returning prefix: '%s', fmt_start: '%s'",
                            *prefix, *fmt_start));
        DBUG_VOID_RETURN;
      }
    }
    // Used subsystem prefix not in allowed list, caller should
    // fix by using one of the allowed subsystem prefixes or switching
    // over to use the Ndb_component log functions.
    DBUG_ASSERT(false);
  }

  // Check if string starts with prefix "NDB", this prefix is redundant
  // since all log messages will be prefixed with NDB anyway (unless
  // using a subsystem prefix it will be "NDB <subsystem>:").
  // Crash in debug compile, caller should fix by removing prefix "NDB"
  // from the printout
  DBUG_ASSERT(strncmp(fmt, "NDB", 3) != 0);

  // Format string specifier accepted as is and no prefix was used
  // this would be the default case
  *prefix = NULL;
  *fmt_start = fmt;
  DBUG_VOID_RETURN;
}

void
ndb_log_info(const char* fmt, ...)
{
  const char* prefix;
  const char* fmt_start;
  ndb_log_detect_prefix(fmt, &prefix, &fmt_start);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, prefix, fmt_start, args);
  va_end(args);
}


void
ndb_log_warning(const char* fmt, ...)
{
  const char* prefix;
  const char* fmt_start;
  ndb_log_detect_prefix(fmt, &prefix, &fmt_start);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_WARNING_LEVEL, prefix, fmt_start, args);
  va_end(args);
}


void
ndb_log_error(const char* fmt, ...)
{
  const char* prefix;
  const char* fmt_start;
  ndb_log_detect_prefix(fmt, &prefix, &fmt_start);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_ERROR_LEVEL, prefix, fmt_start, args);
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
  // Print message only if verbose level is set high enough
  if (ndb_log_get_verbose_level() < verbose_level)
    return;

  const char* prefix;
  const char* fmt_start;
  ndb_log_detect_prefix(fmt, &prefix, &fmt_start);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, prefix, fmt_start, args);
  va_end(args);
}
