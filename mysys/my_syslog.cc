/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file mysys/my_syslog.cc
*/

#include <stddef.h>

#include "m_ctype.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#if defined(_WIN32)
#include "mysql/service_my_snprintf.h"
#include "mysys_priv.h"
#endif

extern CHARSET_INFO my_charset_utf16le_bin;

#ifndef _WIN32
#include <syslog.h>

/*
  Some C libraries offer a variant of this, but we roll our own so we
  won't have to worry about portability.
*/
SYSLOG_FACILITY syslog_facility[] = {
  { LOG_DAEMON, "daemon" }, /* default for mysqld */
  { LOG_USER,   "user"   }, /* default for mysql command-line client */

  { LOG_LOCAL0, "local0" }, { LOG_LOCAL1, "local1" }, { LOG_LOCAL2, "local2" },
  { LOG_LOCAL3, "local3" }, { LOG_LOCAL4, "local4" }, { LOG_LOCAL5, "local5" },
  { LOG_LOCAL6, "local6" }, { LOG_LOCAL7, "local7" },

  /* "just in case" */
  { LOG_AUTH,   "auth" },   { LOG_CRON,   "cron" },   { LOG_KERN,   "kern" },
  { LOG_LPR,    "lpr" },    { LOG_MAIL,   "mail" },   { LOG_NEWS,   "news" },
  { LOG_SYSLOG, "syslog" }, { LOG_UUCP,   "uucp" },

#if defined(LOG_FTP)
  { LOG_FTP,    "ftp" },
#endif
#if defined(LOG_AUTHPRIV)
  { LOG_AUTHPRIV, "authpriv" },
#endif

  { -1, NULL }};
#endif


#ifdef _WIN32
#define MSG_DEFAULT       0xC0000064L
static  HANDLE hEventLog= NULL;                  // global
#endif

/**
  Sends message to the system logger. On Windows, the specified message is
  internally converted to UCS-2 encoding, while on other platforms, no
  conversion takes place and the string is passed to the syslog API as it is.

  @param cs                   Character set info of the message string
  @param level                Log level
  @param msg                  Message to be logged

  @return
     0 Success
    -1 Error
*/
int my_syslog(const CHARSET_INFO *cs MY_ATTRIBUTE((unused)),
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

    if (!ReportEventW(hEventLog, _level, 0, MSG_DEFAULT, NULL, 1, 0,
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


#ifdef _WIN32

/**
   Create a key in the Windows registry.
   We'll setup a "MySQL" key in the EventLog branch (RegCreateKey),
   set our executable name (GetModuleFileName) as file-name
   ("EventMessageFile"), then set the message types we expect to
   be logging ("TypesSupported").
   If the key does not exist, sufficient privileges will be required
   to create and configure it.  If the key does exist, opening it
   should be unprivileged; modifying will fail on insufficient
   privileges, but that is non-fatal.

  @param key          Name of the event generator.
                      (Only last part of the key, e.g. "MySQL")

  @return
     0 Success
    -1 Error
*/

const char registry_prefix[]=
         "SYSTEM\\CurrentControlSet\\services\\eventlog\\Application\\";

static int windows_eventlog_create_registry_entry(const char *key)
{
  HKEY    hRegKey= NULL;
  DWORD   dwError= 0;
  TCHAR   szPath[MAX_PATH];
  DWORD   dwTypes;

  size_t  l= sizeof(registry_prefix) + strlen(key) + 1;
  char   *buff;

  int     ret= 0;

  DBUG_ENTER("my_syslog");

  if ((buff= (char *) my_malloc(PSI_NOT_INSTRUMENTED, l, MYF(0))) == NULL)
    DBUG_RETURN(-1);

  my_snprintf(buff, l, "%s%s", registry_prefix, key);

  // Opens the event source registry key; creates it first if required.
  dwError= RegCreateKey(HKEY_LOCAL_MACHINE, buff, &hRegKey);

  my_free(buff);

  if (dwError != ERROR_SUCCESS)
  {
    if (dwError == ERROR_ACCESS_DENIED)
    {
      my_message_stderr(0, "Could not create or access the registry key needed for the MySQL application\n"
                           "to log to the Windows EventLog. Run the application with sufficient\n"
                           "privileges once to create the key, add the key manually, or turn off\n"
                           "logging for that application.", MYF(0));
    }
    DBUG_RETURN(-1);
  }

  /* Name of the PE module that contains the message resource */
  GetModuleFileName(NULL, szPath, MAX_PATH);

  /* Register EventMessageFile (DLL/exec containing event identifiers) */
  dwError = RegSetValueEx(hRegKey, "EventMessageFile", 0, REG_EXPAND_SZ,
                          (PBYTE) szPath, (DWORD) (strlen(szPath) + 1));
  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED))
    ret = -1;

  /* Register supported event types */
  dwTypes= (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
            EVENTLOG_INFORMATION_TYPE);
  dwError= RegSetValueEx(hRegKey, "TypesSupported", 0, REG_DWORD,
                         (LPBYTE) &dwTypes, sizeof dwTypes);
  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED))
    ret= -1;

  RegCloseKey(hRegKey);

  DBUG_RETURN(ret);
}
#endif


/**
  Opens/Registers a new handle for system logging.
  Note: It's a thread-unsafe function. It should either
  be invoked from the main thread or some extra thread
  safety measures need to be taken.

  @param name     Name of the event source / syslog ident.
  @param option   MY_SYSLOG_PIDS to log PID with each message.
  @param facility Type of program. Passed to openlog().

  @return
     0 Success
    -1 Error, log not opened
    -2 Error, not updated, using previous values
*/
int my_openlog(const char *name, int option, int facility)
{
#ifndef _WIN32
  int opts= (option & MY_SYSLOG_PIDS) ? LOG_PID : 0;

  DBUG_ENTER("my_openlog");
  openlog(name, opts | LOG_NDELAY, facility);

#else

  HANDLE hEL_new;

  DBUG_ENTER("my_openlog");

  // OOM failsafe.  Not needed for syslog.
  if (name == NULL)
    DBUG_RETURN(-1);

  if ((windows_eventlog_create_registry_entry(name) != 0) ||
      !(hEL_new= RegisterEventSource(NULL, name)))
  {
    // map error appropriately
    my_osmaperr(GetLastError());
    DBUG_RETURN((hEventLog == NULL) ? -1 : -2);
  }
  else
  {
    if (hEventLog != NULL)
      DeregisterEventSource(hEventLog);
    hEventLog= hEL_new;
  }
#endif

  DBUG_RETURN(0);
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
  if ((hEventLog != NULL) && (! DeregisterEventSource(hEventLog)))
    goto err;

  hEventLog= NULL;
  DBUG_RETURN(0);

err:
  hEventLog= NULL;
  // map error appropriately
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);
#endif
}
