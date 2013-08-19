/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include <m_string.h>
#include <stdarg.h>

#ifndef _WIN32
#include <syslog.h>
#endif


#ifdef _WIN32
static HANDLE hEventLog= NULL;                  // global
#endif

/**
  Sends message to the system logger. On Windows, the specified message is
  internally converted to UCS-2 encoding, while on other platforms, no
  conversion takes place and the string is passed to the syslog API as it is.

  @param cs    [in]                Character set info of the message string
  @param level [in]                Log level
  @param msg   [in]                Message to be logged

  @return
     0 Success
    -1 Error
*/
int my_syslog(const CHARSET_INFO *cs __attribute__((unused)),
              enum loglevel level,
              const char *msg)
{
  int _level;
#ifdef _WIN32
  wchar_t buff[MAX_SYSLOG_MESSAGE_SIZE];
  wchar_t *u16buf= NULL;
  size_t nchars;
  uint dummy_errors;

  DBUG_ENTER("my_syslog");

  _level= (level == INFORMATION_LEVEL) ? EVENTLOG_INFORMATION_TYPE :
    (level == WARNING_LEVEL) ? EVENTLOG_WARNING_TYPE : EVENTLOG_ERROR_TYPE;

  if (hEventLog)
  {
    nchars= my_convert((char *) buff, sizeof(buff) - sizeof(buff[0]),
                       &my_charset_utf16le_bin, msg,
                       MAX_SYSLOG_MESSAGE_SIZE, cs, &dummy_errors);

    // terminate it with NULL
    buff[nchars / sizeof(wchar_t)]= L'\0';
    u16buf= buff;

    if (!ReportEventW(hEventLog, _level, 0, 0, NULL, 1, 0,
                      (LPCWSTR*) &u16buf, NULL))
      goto err;
  }

  // Message successfully written to the event log.
  DBUG_RETURN(0);

err:
  // map error appropriately
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);

#else
  DBUG_ENTER("my_syslog");

  _level= (level == INFORMATION_LEVEL) ? LOG_INFO :
    (level == WARNING_LEVEL) ? LOG_WARNING : LOG_ERR;

  syslog(_level, "%s", msg);
  DBUG_RETURN(0);

#endif                                          /* _WIN32 */
}


/**
  Opens/Registers a new handle for system logging.
  Note: Its a thread-unsafe function. It should either
  be invoked from the main thread or some extra thread
  safety measures need to be taken.

  @param eventSourceName [in]         Name of the event generator.
                                      (Windows only)

  @return
     0 Success
    -1 Error
*/
int my_openlog(const char *eventSourceName __attribute__((unused)))
{
  DBUG_ENTER("my_openlog");
#ifndef _WIN32
  openlog(eventSourceName, LOG_NDELAY, LOG_USER);
  DBUG_RETURN(0);
#else
  if (!(hEventLog= RegisterEventSource(NULL, eventSourceName)))
    goto err;

  DBUG_RETURN(0);

err:
  // map error appropriately
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);
#endif
}


/**
  Closes/de-registers the system logging handle.
  Note: Its a thread-unsafe function. It should
  either be invoked from the main thread or some
  extra thread safety measures need to be taken.

  @return
     0 Success
    -1 Error
*/
int my_closelog(void)
{
  DBUG_ENTER("my_closelog");
#ifndef _WIN32
  closelog();
  DBUG_RETURN(0);
#else
  if (hEventLog || (! DeregisterEventSource(hEventLog)))
    goto err;

  DBUG_RETURN(0);

err:
    // map error appropriately
    my_osmaperr(GetLastError());
    DBUG_RETURN(-1);
#endif
}

