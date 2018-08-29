/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/**
  @file

  @brief
  logging of commands

  @todo
    Abort logging when we get an error in reading or writing log files
*/

#include "log.h"

#include "sql_audit.h"    // mysql_audit_general_log
#include "sql_base.h"     // close_log_table
#include "sql_class.h"    // THD
#include "sql_parse.h"    // sql_command_flags
#include "sql_time.h"     // calc_time_from_sec
#include "table.h"        // TABLE_FIELD_TYPE

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <string>
#include <sstream>
#ifdef _WIN32
#include <message.h>
#else
#include <syslog.h>
#endif

using std::min;
using std::max;

/* max size of log messages (error log, plugins' logging, general log) */
static const uint MAX_LOG_BUFFER_SIZE= 1024;


#ifndef _WIN32
static int   log_syslog_facility= 0;
#endif
static char *log_syslog_ident   = NULL;
static bool  log_syslog_enabled = false;


/* 26 for regular timestamp, plus 7 (".123456") when using micro-seconds */
static const int iso8601_size= 33;

enum enum_slow_query_log_table_field
{
  SQLT_FIELD_START_TIME = 0,
  SQLT_FIELD_USER_HOST,
  SQLT_FIELD_QUERY_TIME,
  SQLT_FIELD_LOCK_TIME,
  SQLT_FIELD_ROWS_SENT,
  SQLT_FIELD_ROWS_EXAMINED,
  SQLT_FIELD_DATABASE,
  SQLT_FIELD_LAST_INSERT_ID,
  SQLT_FIELD_INSERT_ID,
  SQLT_FIELD_SERVER_ID,
  SQLT_FIELD_SQL_TEXT,
  SQLT_FIELD_THREAD_ID,
  SQLT_FIELD_COUNT
};

static const TABLE_FIELD_TYPE slow_query_log_table_fields[SQLT_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("start_time") },
    { C_STRING_WITH_LEN("timestamp(6)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("user_host") },
    { C_STRING_WITH_LEN("mediumtext") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("query_time") },
    { C_STRING_WITH_LEN("time(6)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("lock_time") },
    { C_STRING_WITH_LEN("time(6)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("rows_sent") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("rows_examined") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("varchar(512)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("last_insert_id") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("insert_id") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("server_id") },
    { C_STRING_WITH_LEN("int(10) unsigned") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_text") },
    { C_STRING_WITH_LEN("mediumblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("thread_id") },
    { C_STRING_WITH_LEN("bigint(21) unsigned") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  slow_query_log_table_def= {SQLT_FIELD_COUNT, slow_query_log_table_fields};


enum enum_general_log_table_field
{
  GLT_FIELD_EVENT_TIME = 0,
  GLT_FIELD_USER_HOST,
  GLT_FIELD_THREAD_ID,
  GLT_FIELD_SERVER_ID,
  GLT_FIELD_COMMAND_TYPE,
  GLT_FIELD_ARGUMENT,
  GLT_FIELD_COUNT
};

static const TABLE_FIELD_TYPE general_log_table_fields[GLT_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("event_time") },
    { C_STRING_WITH_LEN("timestamp(6)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("user_host") },
    { C_STRING_WITH_LEN("mediumtext") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("thread_id") },
    { C_STRING_WITH_LEN("bigint(21) unsigned") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("server_id") },
    { C_STRING_WITH_LEN("int(10) unsigned") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("command_type") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("argument") },
    { C_STRING_WITH_LEN("mediumblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  general_log_table_def= {GLT_FIELD_COUNT, general_log_table_fields};


class Query_log_table_intact : public Table_check_intact
{
protected:
  void report_error(uint, const char *fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    error_log_print(ERROR_LEVEL, fmt, args);
    va_end(args);
  }
};

/** In case of an error, a message is printed to the error log. */
static Query_log_table_intact log_table_intact;


/**
  Silence all errors and warnings reported when performing a write
  to a log table.
  Errors and warnings are not reported to the client or SQL exception
  handlers, so that the presence of logging does not interfere and affect
  the logic of an application.
*/

class Silence_log_table_errors : public Internal_error_handler
{
  char m_message[MYSQL_ERRMSG_SIZE];
public:
  Silence_log_table_errors() { m_message[0]= '\0'; }

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sql_state,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    strmake(m_message, msg, sizeof(m_message)-1);
    return true;
  }

  const char *message() const { return m_message; }
};


#ifndef _WIN32

/**
  On being handed a syslog facility name tries to look it up.
  If successful, fills in a struct with the facility ID and
  the facility's canonical name.

  @param f   [in]   Name of the faciltiy we're trying to look up.
                    Lookup is case-insensitive; leading "log_" is ignored.
  @param rsf [out]  A buffer in which to return the ID and canonical name.

  @return
    false           No errors; buffer contains valid result
    true            Something went wrong, no valid result set returned
*/
bool log_syslog_find_facility(char *f, SYSLOG_FACILITY *rsf)
{
  if (!f || !*f || !rsf)
    return true;

  if (strncasecmp(f, "log_", 4) == 0)
    f+= 4;

  for(int i= 0; syslog_facility[i].name != NULL; i++)
    if (!strcasecmp(f, syslog_facility[i].name))
    {
      rsf->id=   syslog_facility[i].id;
      rsf->name= syslog_facility[i].name;
      return false;
    }

  return true;
}

#endif


/**
  Close POSIX syslog / Windows EventLog.
*/
static void log_syslog_close()
{
  if (log_syslog_enabled)
  {
    log_syslog_enabled= false;
    my_closelog();
  }
}


/**
  Update syslog / Windows EventLog characteristics (on/off,
  identify-as, log-PIDs, facility, ...) from global variables.

  @return
    false  No errors; all characteristics updated.
    true   Unable to update characteristics.
*/
bool log_syslog_update_settings()
{
  const char *prefix;

  if (!opt_log_syslog_enable && log_syslog_enabled)
  {
    log_syslog_close();
    return false;
  }

#ifndef _WIN32
  {
    /*
      make facility
    */

    SYSLOG_FACILITY rsf = { LOG_DAEMON, "daemon" };

    DBUG_ASSERT(opt_log_syslog_facility != NULL);

    if (log_syslog_find_facility(opt_log_syslog_facility, &rsf))
    {
      log_syslog_find_facility((char *) "daemon", &rsf);
      sql_print_warning("failed to set syslog facility to \"%s\", "
                        "setting to \"%s\" (%d) instead.",
                        opt_log_syslog_facility, rsf.name, rsf.id);
      rsf.name= NULL;
    }
    log_syslog_facility= rsf.id;

    // If NaN, set to the canonical form (cut "log_", fix case)
    if ((rsf.name != NULL) && (strcmp(opt_log_syslog_facility, rsf.name) != 0))
      strcpy(opt_log_syslog_facility, rsf.name);
  }

  /*
    Logs historically have subtly different names, to meet each platform's
    conventions -- "mysqld" on unix (via mysqld_safe), and "MySQL" for the
    Win NT EventLog.
  */
  prefix= "mysqld";
#else
  prefix= "MySQL";
#endif

  // tag must not contain directory separators
  if ((opt_log_syslog_tag != NULL) &&
      (strchr(opt_log_syslog_tag, FN_LIBCHAR) != NULL))
    return true;

  if (opt_log_syslog_enable)
  {
    /*
      make ident
    */
    char *ident= NULL;

    if ((opt_log_syslog_tag == NULL) ||
        (*opt_log_syslog_tag == '\0'))
      ident= my_strdup(PSI_NOT_INSTRUMENTED, prefix, MYF(0));
    else
    {
      size_t l= 6 + 1 + 1 + strlen(opt_log_syslog_tag);

      ident= (char *) my_malloc(PSI_NOT_INSTRUMENTED, l, MYF(0));
      if (ident)
        my_snprintf(ident, l, "%s-%s", prefix, opt_log_syslog_tag);
    }

    // if we succeeded in making an ident, replace the old one
    if (ident)
    {
      char *i= log_syslog_ident;
      log_syslog_ident= ident;
      if (i)
        my_free(i);
    }
    else
      return true;

    log_syslog_close();

    int ret;

    ret= my_openlog(log_syslog_ident,
#ifndef _WIN32
                    opt_log_syslog_include_pid ? MY_SYSLOG_PIDS : 0,
                    log_syslog_facility
#else
                    0, 0
#endif
                   );

    if (ret == -1)
      return true;

    log_syslog_enabled= true;

    if (ret == -2)
    {
      my_syslog(system_charset_info, ERROR_LEVEL, "could not update log settings!");
      return true;
    }
  }

  return false;
}


/**
  Stop using syslog / EventLog. Call as late as possible.
*/
void log_syslog_exit(void)
{
  log_syslog_close();

  if (log_syslog_ident != NULL)
  {
    my_free(log_syslog_ident);
    log_syslog_ident= NULL;
  }
}


/**
  Start using syslog / EventLog.

  @return
    true   could not open syslog / EventLog with the requested characteristics
    false  no issues encountered
*/
bool log_syslog_init(void)
{
  if (log_syslog_update_settings())
  {
#ifdef _WIN32
    const char *l = "Windows EventLog";
#else
    const char *l = "syslog";
#endif
    sql_print_error("Cannot open %s; check privileges, or start server with --log_syslog=0", l);
    return true;
  }
  return false;
}


static void ull2timeval(ulonglong utime, struct timeval *tv)
{
  DBUG_ASSERT(tv != NULL);
  DBUG_ASSERT(utime > 0);      /* should hold true in this context */
  tv->tv_sec= static_cast<long>(utime / 1000000);
  tv->tv_usec=utime % 1000000;
}


/**
  Make and return an ISO 8601 / RFC 3339 compliant timestamp.
  Heeds log_timestamps.

  @param buf       A buffer of at least 26 bytes to store the timestamp in
                   (19 + tzinfo tail + \0)
  @param seconds   Seconds since the epoch, or 0 for "now"

  @return          length of timestamp (excluding \0)
*/

static int make_iso8601_timestamp(char *buf, ulonglong utime= 0)
{
  struct tm  my_tm;
  char       tzinfo[7]="Z";  // max 6 chars plus \0
  size_t     len;
  time_t     seconds;

  if (utime == 0)
    utime= my_micro_time();

  seconds= utime / 1000000;
  utime = utime % 1000000;

  if (opt_log_timestamps == 0)
    gmtime_r(&seconds, &my_tm);
  else
  {
    localtime_r(&seconds, &my_tm);

#ifdef __FreeBSD__
    /*
      The field tm_gmtoff is the offset (in seconds) of the time represented
      from UTC, with positive values indicating east of the Prime Meridian.
    */
    long tim= -my_tm.tm_gmtoff;
#elif _WIN32
    long tim = _timezone;
#else
    long tim= timezone; // seconds West of UTC.
#endif
    char dir= '-';

    if (tim < 0)
    {
      dir= '+';
      tim= -tim;
    }
    my_snprintf(tzinfo, sizeof(tzinfo), "%c%02d:%02d",
                dir, (int) (tim / (60 * 60)), (int) ((tim / 60) % 60));
  }

  len= my_snprintf(buf, iso8601_size, "%04d-%02d-%02dT%02d:%02d:%02d.%06lu%s",
                   my_tm.tm_year + 1900,
                   my_tm.tm_mon  + 1,
                   my_tm.tm_mday,
                   my_tm.tm_hour,
                   my_tm.tm_min,
                   my_tm.tm_sec,
                   (unsigned long) utime,
                   tzinfo);

  return min<int>(len, iso8601_size - 1);
}


bool is_valid_log_name(const char *name, size_t len)
{
  if (len > 3)
  {
    const char *tail= name + len - 4;
    if (my_strcasecmp(system_charset_info, tail, ".ini") == 0 ||
        my_strcasecmp(system_charset_info, tail, ".cnf") == 0)
    {
      return false;
    }
  }
  return true;
}


/**
  Get the real log file name, and possibly reopen file.

  The implementation is platform dependent due to differences in how this is
  supported:

  On Windows, we get the actual path based on the file descriptor. This path is
  copied into the supplied buffer. The 'file' parameter is returned without
  re-opening.

  On other platforms, we use realpath() to get the path with symbolic links
  expanded. Then, we close the file, and reopen the real path using the
  O_NOFOLLOW flag. This will reject folowing symbolic links.

  @param          file                  File descriptor.
  @param          log_file_key          Key for P_S instrumentation.
  @param          open_flags            Flags to use for opening the file.
  @param          opened_file_name      Name of the open fd.
  @param [out]    real_file_name        Buffer for actual name of the fd.

  @retval file descriptor to open file with 'real_file_name', or '-1'
          in case of errors.
*/

static File mysql_file_real_name_reopen(File file,
#ifdef HAVE_PSI_INTERFACE
                                        PSI_file_key log_file_key,
#endif
                                        int open_flags,
                                        const char *opened_file_name,
                                        char *real_file_name)
{
  DBUG_ASSERT(file);
  DBUG_ASSERT(opened_file_name);
  DBUG_ASSERT(real_file_name);

#ifdef _WIN32
  /* On Windows, O_NOFOLLOW is not supported. Verify real path from fd. */
  DWORD real_length= GetFinalPathNameByHandle(my_get_osfhandle(file),
                                              real_file_name,
                                              FN_REFLEN,
                                              FILE_NAME_OPENED);

  /* May ret 0 if e.g. on a ramdisk. Ignore - return open file and name. */
  if (real_length == 0)
  {
    strcpy(real_file_name, opened_file_name);
    return file;
  }

  if (real_length > FN_REFLEN)
  {
    mysql_file_close(file, MYF(0));
    return -1;
  }

  return file;
#else
  /* On *nix, get realpath, open realpath with O_NOFOLLOW. */
  if (realpath(opened_file_name, real_file_name) == NULL)
  {
    (void) mysql_file_close(file, MYF(0));
    return -1;
  }

  if (mysql_file_close(file, MYF(0)))
    return -1;

  /* Make sure the real path is not too long. */
  if (strlen(real_file_name) > FN_REFLEN)
    return -1;

  return mysql_file_open(log_file_key, real_file_name,
                         open_flags | O_NOFOLLOW,
                         MYF(MY_WME));
#endif //_WIN32
}


bool File_query_log::open()
{
  File file= -1;
  my_off_t pos= 0;
  const char *log_name= NULL;
  char buff[FN_REFLEN];
  MY_STAT f_stat;
  DBUG_ENTER("File_query_log::open");

  if (m_log_type == QUERY_LOG_SLOW)
    log_name= opt_slow_logname;
  else if (m_log_type == QUERY_LOG_GENERAL)
    log_name= opt_general_logname;
  else
    DBUG_ASSERT(false);
  DBUG_ASSERT(log_name && log_name[0]);

  write_error= false;

  if (!(name= my_strdup(key_memory_File_query_log_name, log_name, MYF(MY_WME))))
  {
    name= const_cast<char *>(log_name); // for the error message
    goto err;
  }

  fn_format(log_file_name, name, mysql_data_home, "", 4);

  /* File is regular writable file */
  if (my_stat(log_file_name, &f_stat, MYF(0)) && !MY_S_ISREG(f_stat.st_mode))
    goto err;

  db[0]= 0;

  /* First, open the file to make sure it exists. */
  if ((file= mysql_file_open(m_log_file_key,
                             log_file_name,
                             O_CREAT | O_BINARY | O_WRONLY | O_APPEND,
                             MYF(MY_WME))) < 0)
    goto err;

#ifdef _WIN32
  char real_log_file_name[FN_REFLEN];
#else
  /* File name must have room for PATH_MAX. Checked against F_REFLEN later. */
  char real_log_file_name[PATH_MAX];
#endif // _Win32

  /* Reopen and get real path. */
  if ((file= mysql_file_real_name_reopen(file,
#ifdef HAVE_PSI_INTERFACE
                                         m_log_file_key,
#endif
                                         O_CREAT | O_BINARY | O_WRONLY | O_APPEND,
                                         log_file_name, real_log_file_name)) < 0)
    goto err;

  if (!is_valid_log_name(real_log_file_name, strlen(real_log_file_name)))
  {
    sql_print_error("Invalid log file name after expanding symlinks: '%s'",
                    real_log_file_name);
    goto err;
  }

  if ((pos= mysql_file_tell(file, MYF(MY_WME))) == MY_FILEPOS_ERROR)
  {
    if (my_errno() == ESPIPE)
      pos= 0;
    else
      goto err;
  }

  if (init_io_cache(&log_file, file, IO_SIZE, WRITE_CACHE, pos, 0,
                    MYF(MY_WME | MY_NABP)))
    goto err;

  {
    char *end;
    size_t len=my_snprintf(buff, sizeof(buff), "%s, Version: %s (%s). "
#ifdef EMBEDDED_LIBRARY
                        "embedded library\n",
                        my_progname, server_version, MYSQL_COMPILATION_COMMENT
#elif _WIN32
                        "started with:\nTCP Port: %d, Named Pipe: %s\n",
                        my_progname, server_version, MYSQL_COMPILATION_COMMENT,
                        mysqld_port, mysqld_unix_port
#else
                        "started with:\nTcp port: %d  Unix socket: %s\n",
                        my_progname, server_version, MYSQL_COMPILATION_COMMENT,
                        mysqld_port, mysqld_unix_port
#endif
                        );
    end= my_stpncpy(buff + len, "Time                 Id Command    Argument\n",
                 sizeof(buff) - len);
    if (my_b_write(&log_file, (uchar*) buff, (uint) (end-buff)) ||
        flush_io_cache(&log_file))
      goto err;
  }

  log_open= true;
  DBUG_RETURN(false);

err:
  char log_open_file_error_message[96]= "";
  if (strcmp(opt_slow_logname, name) == 0)
  {
    strcpy(log_open_file_error_message, "either restart the query logging "
           "by using \"SET GLOBAL SLOW_QUERY_LOG=ON\" or");
  }
  else if (strcmp(opt_general_logname, name) == 0)
  {
    strcpy(log_open_file_error_message, "either restart the query logging "
           "by using \"SET GLOBAL GENERAL_LOG=ON\" or");
  }

  char errbuf[MYSYS_STRERROR_SIZE];
  sql_print_error("Could not use %s for logging (error %d - %s). "
                  "Turning logging off for the server process. "
                  "To turn it on again: fix the cause, "
                  "then %s restart the MySQL server.", name, errno,
                  my_strerror(errbuf, sizeof(errbuf), errno),
                  log_open_file_error_message);
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);
  my_free(name);
  name= NULL;
  log_open= false;
  DBUG_RETURN(true);
}


void File_query_log::close()
{
  DBUG_ENTER("File_query_log::close");
  if (!is_open())
    DBUG_VOID_RETURN;

  end_io_cache(&log_file);

  if (mysql_file_sync(log_file.file, MYF(MY_WME)))
    check_and_print_write_error();

  if (mysql_file_close(log_file.file, MYF(MY_WME)))
    check_and_print_write_error();

  log_open= false;
  my_free(name);
  name= NULL;
  DBUG_VOID_RETURN;
}


void File_query_log::check_and_print_write_error()
{
  if (!write_error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    write_error= true;
    sql_print_error(ER_DEFAULT(ER_ERROR_ON_WRITE), name, errno,
                    my_strerror(errbuf, sizeof(errbuf), errno));
  }
}


bool File_query_log::write_general(ulonglong event_utime,
                                   const char *user_host,
                                   size_t user_host_len,
                                   my_thread_id thread_id,
                                   const char *command_type,
                                   size_t command_type_len,
                                   const char *sql_text,
                                   size_t sql_text_len)
{
  char buff[32];
  size_t length= 0;

  mysql_mutex_lock(&LOCK_log);
  DBUG_ASSERT(is_open());

  /* Note that my_b_write() assumes it knows the length for this */
  char local_time_buff[iso8601_size];
  int  time_buff_len= make_iso8601_timestamp(local_time_buff, event_utime);

  if (my_b_write(&log_file, (uchar*) local_time_buff, time_buff_len))
    goto err;

  if (my_b_write(&log_file, (uchar*) "\t", 1))
    goto err;

  length= my_snprintf(buff, 32, "%5u ", thread_id);

  if (my_b_write(&log_file, (uchar*) buff, length))
    goto err;

  if (my_b_write(&log_file, (uchar*) command_type, command_type_len))
    goto err;

  if (my_b_write(&log_file, (uchar*) "\t", 1))
    goto err;

  /* sql_text */
  if (my_b_write(&log_file, (uchar*) sql_text, sql_text_len))
    goto err;

  if (my_b_write(&log_file, (uchar*) "\n", 1) ||
      flush_io_cache(&log_file))
    goto err;

  mysql_mutex_unlock(&LOCK_log);
  return false;

err:
  check_and_print_write_error();
  mysql_mutex_unlock(&LOCK_log);
  return true;
}


bool File_query_log::write_slow(THD *thd, ulonglong current_utime,
                                ulonglong query_start_arg,
                                const char *user_host,
                                size_t user_host_len, ulonglong query_utime,
                                ulonglong lock_utime, bool is_command,
                                const char *sql_text, size_t sql_text_len)
{
  char buff[80], *end;
  char query_time_buff[22+7], lock_time_buff[22+7];
  size_t buff_len;
  end= buff;

  mysql_mutex_lock(&LOCK_log);
  DBUG_ASSERT(is_open());

  if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT))
  {
    char my_timestamp[iso8601_size];

    make_iso8601_timestamp(my_timestamp, current_utime);

    buff_len= my_snprintf(buff, sizeof buff,
                          "# Time: %s\n", my_timestamp);

    /* Note that my_b_write() assumes it knows the length for this */
    if (my_b_write(&log_file, (uchar*) buff, buff_len))
      goto err;

    buff_len= my_snprintf(buff, 32, "%5u", thd->thread_id());
    if (my_b_printf(&log_file, "# User@Host: %s  Id: %s\n", user_host, buff)
        == (uint) -1)
      goto err;
  }

  /* For slow query log */
  sprintf(query_time_buff, "%.6f", ulonglong2double(query_utime)/1000000.0);
  sprintf(lock_time_buff,  "%.6f", ulonglong2double(lock_utime)/1000000.0);
  if (my_b_printf(&log_file,
                  "# Query_time: %s  Lock_time: %s"
                  " Rows_sent: %lu  Rows_examined: %lu\n",
                  query_time_buff, lock_time_buff,
                  (ulong) thd->get_sent_row_count(),
                  (ulong) thd->get_examined_row_count()) == (uint) -1)
    goto err;
  if (thd->db().str && strcmp(thd->db().str, db))
  {						// Database changed
    if (my_b_printf(&log_file,"use %s;\n",thd->db().str) == (uint) -1)
      goto err;
    my_stpcpy(db,thd->db().str);
  }
  if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
  {
    end=my_stpcpy(end, ",last_insert_id=");
    end=longlong10_to_str((longlong)
                          thd->first_successful_insert_id_in_prev_stmt_for_binlog,
                          end, -10);
  }
  // Save value if we do an insert.
  if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0)
  {
    if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT))
    {
      end=my_stpcpy(end,",insert_id=");
      end=longlong10_to_str((longlong)
                            thd->auto_inc_intervals_in_cur_stmt_for_binlog.minimum(),
                            end, -10);
    }
  }

  /*
    This info used to show up randomly, depending on whether the query
    checked the query start time or not. now we always write current
    timestamp to the slow log
  */
  end= my_stpcpy(end, ",timestamp=");
  end= int10_to_str((long) (current_utime / 1000000), end, 10);

  if (end != buff)
  {
    *end++=';';
    *end='\n';
    if (my_b_write(&log_file, (uchar*) "SET ", 4) ||
        my_b_write(&log_file, (uchar*) buff + 1, (uint) (end-buff)))
      goto err;
  }
  if (is_command)
  {
    end= strxmov(buff, "# administrator command: ", NullS);
    buff_len= (ulong) (end - buff);
    DBUG_EXECUTE_IF("simulate_slow_log_write_error",
                    {DBUG_SET("+d,simulate_file_write_error");});
    if (my_b_write(&log_file, (uchar*) buff, buff_len))
      goto err;
  }
  if (my_b_write(&log_file, (uchar*) sql_text, sql_text_len) ||
      my_b_write(&log_file, (uchar*) ";\n",2) ||
      flush_io_cache(&log_file))
    goto err;

  mysql_mutex_unlock(&LOCK_log);
  return false;

err:
  check_and_print_write_error();
  mysql_mutex_unlock(&LOCK_log);
  return true;
}


bool Log_to_csv_event_handler::log_general(THD *thd, ulonglong event_utime,
                                           const char *user_host,
                                           size_t user_host_len,
                                           my_thread_id thread_id,
                                           const char *command_type,
                                           size_t command_type_len,
                                           const char *sql_text,
                                           size_t sql_text_len,
                                           const CHARSET_INFO *client_cs)
{
  TABLE *table= NULL;
  bool result= true;
  bool need_close= false;
  bool need_rnd_end= false;
  uint field_index;
  struct timeval tv;

  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  bool save_time_zone_used= thd->time_zone_used;

  ulonglong save_thd_options= thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_BIN_LOG;

  TABLE_LIST table_list;
  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            GENERAL_LOG_NAME.str, GENERAL_LOG_NAME.length,
                            GENERAL_LOG_NAME.str,
                            TL_WRITE_CONCURRENT_INSERT);

  /*
    1) open_log_table generates an error if the
    table can not be opened or is corrupted.
    2) "INSERT INTO general_log" can generate warning sometimes.

    Suppress these warnings and errors, they can't be dealt with
    properly anyway.

    QQ: this problem needs to be studied in more detail.
    Comment this 2 lines and run "cast.test" to see what's happening.
  */
  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);

  Open_tables_backup open_tables_backup;
  if (!(table= open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close= true;

  if (log_table_intact.check(table_list.table, &general_log_table_def))
    goto err;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= true;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  /*
    NOTE: we do not call restore_record() here, as all fields are
    filled by the Logger (=> no need to load default ones).
  */

  /*
    We do not set a value for table->field[0], as it will use
    default value (which is CURRENT_TIMESTAMP).
  */

  DBUG_ASSERT(table->field[GLT_FIELD_EVENT_TIME]->type() == MYSQL_TYPE_TIMESTAMP);
  ull2timeval(event_utime, &tv);
  table->field[GLT_FIELD_EVENT_TIME]->store_timestamp(&tv);

  /* do a write */
  if (table->field[GLT_FIELD_USER_HOST]->store(user_host, user_host_len,
                                               client_cs) ||
      table->field[GLT_FIELD_THREAD_ID]->store((longlong) thread_id, true) ||
      table->field[GLT_FIELD_SERVER_ID]->store((longlong) server_id, true) ||
      table->field[GLT_FIELD_COMMAND_TYPE]->store(command_type,
                                                  command_type_len, client_cs))
    goto err;

  /*
    A positive return value in store() means truncation.
    Still logging a message in the log in this case.
  */
  if (table->field[GLT_FIELD_ARGUMENT]->store(sql_text, sql_text_len,
                                              client_cs) < 0)
    goto err;

  /* mark all fields as not null */
  table->field[GLT_FIELD_USER_HOST]->set_notnull();
  table->field[GLT_FIELD_THREAD_ID]->set_notnull();
  table->field[GLT_FIELD_SERVER_ID]->set_notnull();
  table->field[GLT_FIELD_COMMAND_TYPE]->set_notnull();
  table->field[GLT_FIELD_ARGUMENT]->set_notnull();

  /* Set any extra columns to their default values */
  for (field_index= GLT_FIELD_COUNT ;
       field_index < table->s->fields ;
       field_index++)
  {
    table->field[field_index]->set_default();
  }

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= false;

err:
  thd->pop_internal_handler();

  if (result && !thd->killed)
    sql_print_error("Failed to write to mysql.general_log: %s",
                    error_handler.message());

  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }

  if (need_close)
    close_log_table(thd, &open_tables_backup);

  thd->variables.option_bits= save_thd_options;
  thd->time_zone_used= save_time_zone_used;
  return result;
}


bool Log_to_csv_event_handler::log_slow(THD *thd, ulonglong current_utime,
                                        ulonglong query_start_arg,
                                        const char *user_host,
                                        size_t user_host_len,
                                        ulonglong query_utime,
                                        ulonglong lock_utime, bool is_command,
                                        const char *sql_text,
                                        size_t sql_text_len)
{
  TABLE *table= NULL;
  bool result= true;
  bool need_close= false;
  bool need_rnd_end= false;
  const CHARSET_INFO *client_cs= thd->variables.character_set_client;
  struct timeval tv;

  DBUG_ENTER("Log_to_csv_event_handler::log_slow");

  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  bool save_time_zone_used= thd->time_zone_used;

  TABLE_LIST table_list;
  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            SLOW_LOG_NAME.str, SLOW_LOG_NAME.length,
                            SLOW_LOG_NAME.str,
                            TL_WRITE_CONCURRENT_INSERT);

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(& error_handler);

  Open_tables_backup open_tables_backup;
  if (!(table= open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close= true;

  if (log_table_intact.check(table_list.table, &slow_query_log_table_def))
    goto err;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= true;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  restore_record(table, s->default_values);    // Get empty record

  /* store the time and user values */
  DBUG_ASSERT(table->field[SQLT_FIELD_START_TIME]->type() == MYSQL_TYPE_TIMESTAMP);
  ull2timeval(current_utime, &tv);
  table->field[SQLT_FIELD_START_TIME]->store_timestamp(&tv);

  if (table->field[SQLT_FIELD_USER_HOST]->store(user_host, user_host_len,
                                                client_cs))
    goto err;

  if (query_start_arg)
  {
    /*
      A TIME field can not hold the full longlong range; query_time or
      lock_time may be truncated without warning here, if greater than
      839 hours (~35 days)
    */
    MYSQL_TIME t;
    t.neg= 0;

    /* fill in query_time field */
    calc_time_from_sec(&t,
                       static_cast<long>(min((longlong)(query_utime / 1000000),
                                             (longlong)TIME_MAX_VALUE_SECONDS)),
                       query_utime % 1000000);
    if (table->field[SQLT_FIELD_QUERY_TIME]->store_time(&t))
      goto err;
    /* lock_time */
    calc_time_from_sec(&t,
                       static_cast<long>(min((longlong)(lock_utime / 1000000),
                                             (longlong)TIME_MAX_VALUE_SECONDS)),
                       lock_utime % 1000000);
    if (table->field[SQLT_FIELD_LOCK_TIME]->store_time(&t))
      goto err;
    /* rows_sent */
    if (table->field[SQLT_FIELD_ROWS_SENT]->store((longlong) thd->get_sent_row_count(), true))
      goto err;
    /* rows_examined */
    if (table->field[SQLT_FIELD_ROWS_EXAMINED]->store((longlong) thd->get_examined_row_count(), true))
      goto err;
  }
  else
  {
    table->field[SQLT_FIELD_QUERY_TIME]->set_null();
    table->field[SQLT_FIELD_LOCK_TIME]->set_null();
    table->field[SQLT_FIELD_ROWS_SENT]->set_null();
    table->field[SQLT_FIELD_ROWS_EXAMINED]->set_null();
  }
  /* fill database field */
  if (thd->db().str)
  {
    if (table->field[SQLT_FIELD_DATABASE]->store(thd->db().str,
                                                 thd->db().length,
                                                 client_cs))
      goto err;
    table->field[SQLT_FIELD_DATABASE]->set_notnull();
  }

  if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
  {
    if (table->
        field[SQLT_FIELD_LAST_INSERT_ID]->store((longlong)
                        thd->first_successful_insert_id_in_prev_stmt_for_binlog,
                        true))
      goto err;
    table->field[SQLT_FIELD_LAST_INSERT_ID]->set_notnull();
  }

  /*
    Set value if we do an insert on autoincrement column. Note that for
    some engines (those for which get_auto_increment() does not leave a
    table lock until the statement ends), this is just the first value and
    the next ones used may not be contiguous to it.
  */
  if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0)
  {
    if (table->
        field[SQLT_FIELD_INSERT_ID]->store((longlong)
          thd->auto_inc_intervals_in_cur_stmt_for_binlog.minimum(), true))
      goto err;
    table->field[SQLT_FIELD_INSERT_ID]->set_notnull();
  }

  if (table->field[SQLT_FIELD_SERVER_ID]->store((longlong) server_id, true))
    goto err;
  table->field[SQLT_FIELD_SERVER_ID]->set_notnull();

  /*
    Column sql_text.
    A positive return value in store() means truncation.
    Still logging a message in the log in this case.
  */
  if (table->field[SQLT_FIELD_SQL_TEXT]->store(sql_text, sql_text_len,
                                               client_cs) < 0)
    goto err;

  if (table->field[SQLT_FIELD_THREAD_ID]->store((longlong) thd->thread_id(),
                                                true))
    goto err;

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= false;

err:
  thd->pop_internal_handler();

  if (result && !thd->killed)
    sql_print_error("Failed to write to mysql.slow_log: %s",
                    error_handler.message());

  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }

  if (need_close)
    close_log_table(thd, &open_tables_backup);

  thd->time_zone_used= save_time_zone_used;
  DBUG_RETURN(result);
}


bool Log_to_csv_event_handler::activate_log(THD *thd,
                                            enum_log_table_type log_table_type)
{
  TABLE_LIST table_list;

  DBUG_ENTER("Log_to_csv_event_handler::activate_log");

  switch (log_table_type)
  {
  case QUERY_LOG_GENERAL:
    table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                              GENERAL_LOG_NAME.str, GENERAL_LOG_NAME.length,
                              GENERAL_LOG_NAME.str, TL_WRITE_CONCURRENT_INSERT);
    break;
  case QUERY_LOG_SLOW:
    table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                              SLOW_LOG_NAME.str, SLOW_LOG_NAME.length,
                              SLOW_LOG_NAME.str, TL_WRITE_CONCURRENT_INSERT);
    break;
  default:
    DBUG_ASSERT(false);
  }

  Open_tables_backup open_tables_backup;
  if (open_log_table(thd, &table_list, &open_tables_backup) != NULL)
  {
    close_log_table(thd, &open_tables_backup);
    DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}


bool Log_to_file_event_handler::log_slow(THD *thd, ulonglong current_utime,
                                         ulonglong query_start_arg,
                                         const char *user_host,
                                         size_t user_host_len,
                                         ulonglong query_utime,
                                         ulonglong lock_utime,
                                         bool is_command,
                                         const char *sql_text,
                                         size_t sql_text_len)
{
  if (!mysql_slow_log.is_open())
    return false;

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  bool retval= mysql_slow_log.write_slow(thd, current_utime, query_start_arg,
                                         user_host, user_host_len,
                                         query_utime, lock_utime, is_command,
                                         sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}


bool Log_to_file_event_handler::log_general(THD *thd, ulonglong event_utime,
                                            const char *user_host,
                                            size_t user_host_len,
                                            my_thread_id thread_id,
                                            const char *command_type,
                                            size_t command_type_len,
                                            const char *sql_text,
                                            size_t sql_text_len,
                                            const CHARSET_INFO *client_cs)
{
  if (!mysql_general_log.is_open())
    return false;

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  bool retval= mysql_general_log.write_general(event_utime, user_host,
                                               user_host_len, thread_id,
                                               command_type, command_type_len,
                                               sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}


void Query_logger::cleanup()
{
  mysql_rwlock_destroy(&LOCK_logger);

  DBUG_ASSERT(file_log_handler);
  file_log_handler->cleanup();
  delete file_log_handler;
  file_log_handler= NULL;
}


bool Query_logger::slow_log_write(THD *thd, const char *query,
                                  size_t query_length)
{
  DBUG_ASSERT(thd->enable_slow_log && opt_slow_log);

  if (!(*slow_log_handler_list))
    return false;

  /* do not log slow queries from replication threads */
  if (thd->slave_thread && !opt_log_slow_slave_statements)
    return false;

  mysql_rwlock_rdlock(&LOCK_logger);

  /* fill in user_host value: the format is "%s[%s] @ %s [%s]" */
  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  Security_context *sctx= thd->security_context();
  LEX_CSTRING sctx_user= sctx->user();
  LEX_CSTRING sctx_host= sctx->host();
  LEX_CSTRING sctx_ip= sctx->ip();
  size_t user_host_len= (strxnmov(user_host_buff, MAX_USER_HOST_SIZE,
                                  sctx->priv_user().str, "[",
                                  sctx_user.length ? sctx_user.str : "", "] @ ",
                                  sctx_host.length ? sctx_host.str : "", " [",
                                  sctx_ip.length ? sctx_ip.str : "", "]",
                                  NullS) - user_host_buff);
  ulonglong current_utime= thd->current_utime();
  ulonglong query_utime, lock_utime;
  if (thd->start_utime)
  {
    query_utime= (current_utime - thd->start_utime);
    lock_utime=  (thd->utime_after_lock - thd->start_utime);
  }
  else
  {
    query_utime= 0;
    lock_utime= 0;
  }

  bool is_command= false;
  if (!query)
  {
    is_command= true;
    query= command_name[thd->get_command()].str;
    query_length= command_name[thd->get_command()].length;
  }

  bool error= false;
  for (Log_event_handler **current_handler= slow_log_handler_list;
       *current_handler ;)
  {
    error|= (*current_handler++)->log_slow(thd, current_utime,
                                           (thd->start_time.tv_sec * 1000000) +
                                           thd->start_time.tv_usec,
                                           user_host_buff, user_host_len,
                                           query_utime, lock_utime, is_command,
                                           query, query_length);
  }

  mysql_rwlock_unlock(&LOCK_logger);

  return error;
}


/**
   Check if a given command should be logged to the general log.

   @param thd      Thread handle
   @param command  SQL command

   @return true if command should be logged, false otherwise.
*/
static bool log_command(THD *thd, enum_server_command command)
{
  if (what_to_log & (1L << (uint) command))
  {
    if ((thd->variables.option_bits & OPTION_LOG_OFF)
#ifndef NO_EMBEDDED_ACCESS_CHECKS
         && (thd->security_context()->check_access(SUPER_ACL))
#endif
       )
    {
      /* No logging */
      return false;
    }
    return true;
  }
  return false;
}


bool Query_logger::general_log_write(THD *thd, enum_server_command command,
                                     const char *query, size_t query_length)
{
#ifndef EMBEDDED_LIBRARY
  /* Send a general log message to the audit API. */
  mysql_audit_general_log(thd, command_name[(uint) command].str,
                          command_name[(uint) command].length);
#endif

  /*
    Do we want to log this kind of command?
    Is general log enabled?
    Any active handlers?
  */
  if (!log_command(thd, command) ||
      !opt_general_log ||
      !(*general_log_handler_list))
    return false;

  mysql_rwlock_rdlock(&LOCK_logger);

  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  size_t user_host_len= make_user_name(thd->security_context(), user_host_buff);
  ulonglong current_utime= thd->current_utime();

  bool error= false;
  for (Log_event_handler **current_handler= general_log_handler_list;
       *current_handler; )
  {
    error|= (*current_handler++)->log_general(thd, current_utime, user_host_buff,
                                              user_host_len, thd->thread_id(),
                                              command_name[(uint) command].str,
                                              command_name[(uint) command].length,
                                              query, query_length,
                                              thd->variables.character_set_client);
  }
  mysql_rwlock_unlock(&LOCK_logger);

  return error;
}


bool Query_logger::general_log_print(THD *thd, enum_server_command command,
                                     const char *format, ...)
{
  /*
    Do we want to log this kind of command?
    Is general log enabled?
    Any active handlers?
  */
  if (!log_command(thd, command) ||
      !opt_general_log ||
      !(*general_log_handler_list))
  {
#ifndef EMBEDDED_LIBRARY
    /* Send a general log message to the audit API. */
    mysql_audit_general_log(thd, command_name[(uint) command].str,
                            command_name[(uint) command].length);
#endif
    return false;
  }

  size_t message_buff_len= 0;
  char message_buff[MAX_LOG_BUFFER_SIZE];

  /* prepare message */
  if (format)
  {
    va_list args;
    va_start(args, format);
    message_buff_len= my_vsnprintf(message_buff, sizeof(message_buff),
                                   format, args);
    va_end(args);
  }
  else
    message_buff[0]= '\0';

  return general_log_write(thd, command, message_buff, message_buff_len);
}


void Query_logger::init_query_log(enum_log_table_type log_type,
                                  ulonglong log_printer)
{
  if (log_type == QUERY_LOG_SLOW)
  {
    if (log_printer & LOG_NONE)
    {
      slow_log_handler_list[0]= NULL;
      return;
    }

    switch (log_printer) {
    case LOG_FILE:
      slow_log_handler_list[0]= file_log_handler;
      slow_log_handler_list[1]= NULL;
      break;
    case LOG_TABLE:
      slow_log_handler_list[0]= &table_log_handler;
      slow_log_handler_list[1]= NULL;
      break;
    case LOG_TABLE|LOG_FILE:
      slow_log_handler_list[0]= file_log_handler;
      slow_log_handler_list[1]= &table_log_handler;
      slow_log_handler_list[2]= NULL;
      break;
    }
  }
  else if (log_type == QUERY_LOG_GENERAL)
  {
    if (log_printer & LOG_NONE)
    {
      general_log_handler_list[0]= NULL;
      return;
    }

    switch (log_printer) {
    case LOG_FILE:
      general_log_handler_list[0]= file_log_handler;
      general_log_handler_list[1]= NULL;
      break;
    case LOG_TABLE:
      general_log_handler_list[0]= &table_log_handler;
      general_log_handler_list[1]= NULL;
      break;
    case LOG_TABLE|LOG_FILE:
      general_log_handler_list[0]= file_log_handler;
      general_log_handler_list[1]= &table_log_handler;
      general_log_handler_list[2]= NULL;
      break;
    }
  }
  else
    DBUG_ASSERT(false);
}


void Query_logger::set_handlers(ulonglong log_printer)
{
  mysql_rwlock_wrlock(&LOCK_logger);

  init_query_log(QUERY_LOG_SLOW, log_printer);
  init_query_log(QUERY_LOG_GENERAL, log_printer);

  mysql_rwlock_unlock(&LOCK_logger);
}


bool Query_logger::activate_log_handler(THD* thd, enum_log_table_type log_type)
{
  bool res= false;
  mysql_rwlock_wrlock(&LOCK_logger);
  if (table_log_handler.activate_log(thd, log_type) ||
      file_log_handler->get_query_log(log_type)->open())
    res= true;
  else
    init_query_log(log_type, log_output_options);
  mysql_rwlock_unlock(&LOCK_logger);
  return res;
}


void Query_logger::deactivate_log_handler(enum_log_table_type log_type)
{
  mysql_rwlock_wrlock(&LOCK_logger);
  file_log_handler->get_query_log(log_type)->close();
  // table_list_handler has no state, nothing to close
  mysql_rwlock_unlock(&LOCK_logger);
}


bool Query_logger::reopen_log_file(enum_log_table_type log_type)
{
  mysql_rwlock_wrlock(&LOCK_logger);
  file_log_handler->get_query_log(log_type)->close();
  bool res= file_log_handler->get_query_log(log_type)->open();
  mysql_rwlock_unlock(&LOCK_logger);
  return res;
}


enum_log_table_type
Query_logger::check_if_log_table(TABLE_LIST *table_list,
                                 bool check_if_opened) const
{
  if (table_list->db_length == MYSQL_SCHEMA_NAME.length &&
      !my_strcasecmp(system_charset_info,
                     table_list->db, MYSQL_SCHEMA_NAME.str))
  {
    if (table_list->table_name_length == GENERAL_LOG_NAME.length &&
        !my_strcasecmp(system_charset_info,
                       table_list->table_name, GENERAL_LOG_NAME.str))
    {
      if (!check_if_opened || is_log_table_enabled(QUERY_LOG_GENERAL))
        return QUERY_LOG_GENERAL;
      return QUERY_LOG_NONE;
    }

    if (table_list->table_name_length == SLOW_LOG_NAME.length &&
        !my_strcasecmp(system_charset_info,
                       table_list->table_name, SLOW_LOG_NAME.str))
    {
      if (!check_if_opened || is_log_table_enabled(QUERY_LOG_SLOW))
        return QUERY_LOG_SLOW;
      return QUERY_LOG_NONE;
    }
  }
  return QUERY_LOG_NONE;
}


Query_logger query_logger;


char *make_query_log_name(char *buff, enum_log_table_type log_type)
{
  const char* log_ext= "";
  if (log_type == QUERY_LOG_GENERAL)
    log_ext= ".log";
  else if (log_type == QUERY_LOG_SLOW)
    log_ext= "-slow.log";
  else
    DBUG_ASSERT(false);

  strmake(buff, default_logfile_name, FN_REFLEN-5);
  return fn_format(buff, buff, mysql_real_data_home, log_ext,
                   MYF(MY_UNPACK_FILENAME|MY_REPLACE_EXT));
}


bool log_slow_applicable(THD *thd)
{
  DBUG_ENTER("log_slow_applicable");

  /*
    The following should never be true with our current code base,
    but better to keep this here so we don't accidently try to log a
    statement in a trigger or stored function
  */
  if (unlikely(thd->in_sub_stmt))
    DBUG_RETURN(false);                         // Don't set time for sub stmt

  /*
    Do not log administrative statements unless the appropriate option is
    set.
  */
  if (thd->enable_slow_log && opt_slow_log)
  {
    bool warn_no_index= ((thd->server_status &
                          (SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
                         opt_log_queries_not_using_indexes &&
                         !(sql_command_flags[thd->lex->sql_command] &
                           CF_STATUS_COMMAND));
    bool log_this_query=  ((thd->server_status & SERVER_QUERY_WAS_SLOW) ||
                           warn_no_index) &&
                          (thd->get_examined_row_count() >=
                           thd->variables.min_examined_row_limit);
    bool suppress_logging= log_throttle_qni.log(thd, warn_no_index);

    if (!suppress_logging && log_this_query)
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


void log_slow_do(THD *thd)
{
  THD_STAGE_INFO(thd, stage_logging_slow_query);
  thd->status_var.long_query_count++;

  if (thd->rewritten_query.length())
    query_logger.slow_log_write(thd,
                                thd->rewritten_query.c_ptr_safe(),
                                thd->rewritten_query.length());
  else
    query_logger.slow_log_write(thd, thd->query().str, thd->query().length);
}


void log_slow_statement(THD *thd)
{
  if (log_slow_applicable(thd))
    log_slow_do(thd);
}


#ifdef MYSQL_SERVER
void Log_throttle::new_window(ulonglong now)
{
  count= 0;
  window_end= now + window_size;
}


void Slow_log_throttle::new_window(ulonglong now)
{
  Log_throttle::new_window(now);
  total_exec_time= 0;
  total_lock_time= 0;
}


Slow_log_throttle::Slow_log_throttle(ulong *threshold, mysql_mutex_t *lock,
                                     ulong window_usecs,
                                     bool (*logger)(THD *, const char *, size_t),
                                     const char *msg)
  : Log_throttle(window_usecs, msg), total_exec_time(0), total_lock_time(0),
    rate(threshold), log_summary(logger), LOCK_log_throttle(lock)
{ }


ulong Log_throttle::prepare_summary(ulong rate)
{
  ulong ret= 0;
  /*
    Previous throttling window is over or rate changed.
    Return the number of lines we throttled.
  */
  if (count > rate)
  {
    ret= count - rate;
    count= 0;                                 // prevent writing it again.
  }

  return ret;
}


void Slow_log_throttle::print_summary(THD *thd, ulong suppressed,
                                      ulonglong print_lock_time,
                                      ulonglong print_exec_time)
{
  /*
    We synthesize these values so the totals in the log will be
    correct (just in case somebody analyses them), even if the
    start/stop times won't be (as they're an aggregate which will
    usually mostly lie within [ window_end - window_size ; window_end ]
  */
  ulonglong save_start_utime=      thd->start_utime;
  ulonglong save_utime_after_lock= thd->utime_after_lock;
  Security_context *save_sctx=     thd->security_context();

  char buf[128];

  my_snprintf(buf, sizeof(buf), summary_template, suppressed);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->start_utime=                thd->current_utime() - print_exec_time;
  thd->utime_after_lock=           thd->start_utime + print_lock_time;
  thd->set_security_context(&aggregate_sctx);
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  (*log_summary)(thd, buf, strlen(buf));

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->set_security_context(save_sctx);
  thd->start_utime     = save_start_utime;
  thd->utime_after_lock= save_utime_after_lock;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
}


bool Slow_log_throttle::flush(THD *thd)
{
  // Write summary if we throttled.
  mysql_mutex_lock(LOCK_log_throttle);
  ulonglong print_lock_time=  total_lock_time;
  ulonglong print_exec_time=  total_exec_time;
  ulong     suppressed_count= prepare_summary(*rate);
  mysql_mutex_unlock(LOCK_log_throttle);
  if (suppressed_count > 0)
  {
    print_summary(thd, suppressed_count, print_lock_time, print_exec_time);
    return true;
  }
  return false;
}


bool Slow_log_throttle::log(THD *thd, bool eligible)
{
  bool  suppress_current= false;

  /*
    If throttling is enabled, we might have to write a summary even if
    the current query is not of the type we handle.
  */
  if (*rate > 0)
  {
    mysql_mutex_lock(LOCK_log_throttle);

    ulong     suppressed_count=   0;
    ulonglong print_lock_time=    total_lock_time;
    ulonglong print_exec_time=    total_exec_time;
    ulonglong end_utime_of_query= thd->current_utime();

    /*
      If the window has expired, we'll try to write a summary line.
      The subroutine will know whether we actually need to.
    */
    if (!in_window(end_utime_of_query))
    {
      suppressed_count= prepare_summary(*rate);
      // start new window only if this is the statement type we handle
      if (eligible)
        new_window(end_utime_of_query);
    }
    if (eligible && inc_log_count(*rate))
    {
      /*
        Current query's logging should be suppressed.
        Add its execution time and lock time to totals for the current window.
      */
      total_exec_time += (end_utime_of_query - thd->start_utime);
      total_lock_time += (thd->utime_after_lock - thd->start_utime);
      suppress_current= true;
    }

    mysql_mutex_unlock(LOCK_log_throttle);

    /*
      print_summary() is deferred until after we release the locks to
      avoid congestion. All variables we hand in are local to the caller,
      so things would even be safe if print_summary() hadn't finished by the
      time the next one comes around (60s later at the earliest for now).
      The current design will produce correct data, but does not guarantee
      order (there is a theoretical race condition here where the above
      new_window()/unlock() may enable a different thread to print a warning
      for the new window before the current thread gets to print_summary().
      If the requirements ever change, add a print_lock to the object that
      is held during print_summary(), AND that is briefly locked before
      returning from this function if(eligible && !suppress_current).
      This should ensure correct ordering of summaries with regard to any
      follow-up summaries as well as to any (non-suppressed) warnings (of
      the type we handle) from the next window.
    */
    if (suppressed_count > 0)
      print_summary(thd, suppressed_count, print_lock_time, print_exec_time);
  }

  return suppress_current;
}


bool Error_log_throttle::log()
{
  ulonglong end_utime_of_query= my_micro_time();
  DBUG_EXECUTE_IF("simulate_error_throttle_expiry",
                  end_utime_of_query+=Log_throttle::LOG_THROTTLE_WINDOW_SIZE;);

  /*
    If the window has expired, we'll try to write a summary line.
    The subroutine will know whether we actually need to.
  */
  if (!in_window(end_utime_of_query))
  {
    ulong suppressed_count= prepare_summary(1);

    new_window(end_utime_of_query);

    if (suppressed_count > 0)
      print_summary(suppressed_count);
  }

  /*
    If this is a first error in the current window then do not suppress it.
  */
  return inc_log_count(1);
}


bool Error_log_throttle::flush()
{
  // Write summary if we throttled.
  ulong     suppressed_count= prepare_summary(1);
  if (suppressed_count > 0)
  {
    print_summary(suppressed_count);
    return true;
  }
  return false;
}


static bool slow_log_write(THD *thd, const char *query, size_t query_length)
{
  return opt_slow_log && query_logger.slow_log_write(thd, query, query_length);
}


Slow_log_throttle log_throttle_qni(&opt_log_throttle_queries_not_using_indexes,
                                   &LOCK_log_throttle_qni,
                                   Log_throttle::LOG_THROTTLE_WINDOW_SIZE,
                                   slow_log_write,
                                   "throttle: %10lu 'index "
                                   "not used' warning(s) suppressed.");
#endif // MYSQL_SERVER

////////////////////////////////////////////////////////////
//
// Error Log
//
////////////////////////////////////////////////////////////

static bool error_log_initialized= false;
// This mutex prevents fprintf from different threads from being interleaved.
// It also prevents reopen while we are in the process of logging.
static mysql_mutex_t LOCK_error_log;
// This variable is different from log_error_dest.
// E.g. log_error_dest is "stderr" if we are not logging to file.
static const char *error_log_file= NULL;
static bool error_log_buffering= true;
static std::string *buffered_messages= NULL;


void flush_error_log_messages()
{
  if (buffered_messages && !buffered_messages->empty())
  {
    fprintf(stderr, "%s", buffered_messages->c_str());
    fflush(stderr);
    delete buffered_messages;
    buffered_messages= NULL;
  }
  error_log_buffering= false;
}


void init_error_log()
{
  DBUG_ASSERT(!error_log_initialized);
  mysql_mutex_init(key_LOCK_error_log, &LOCK_error_log, MY_MUTEX_INIT_FAST);
  error_log_initialized= true;
}


bool open_error_log(const char *filename, bool get_lock)
{
  DBUG_ASSERT(filename);
  int retries= 2, errors= 0;

  do
  {
    errors= 0;
    if (!my_freopen(filename, "a", stderr))
      errors++;
#ifndef EMBEDDED_LIBRARY
    if (!my_freopen(filename, "a", stdout))
      errors++;
#endif
  }
  while (retries-- && errors);

  if (errors)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    if (get_lock)
      mysql_mutex_unlock(&LOCK_error_log);
    sql_print_error(ER_DEFAULT(ER_CANT_OPEN_ERROR_LOG), filename,
                    ": ", my_strerror(errbuf, sizeof(errbuf), errno));
    flush_error_log_messages();
    if (get_lock)
      mysql_mutex_lock(&LOCK_error_log);
    return true;
  }

  /* The error stream must be unbuffered. */
  setbuf(stderr, NULL);

  error_log_file= filename; // Remember name for later reopen

  // Write any messages buffered while we were figuring out the filename
  flush_error_log_messages();
  return false;
}


void destroy_error_log()
{
  // We should have flushed before this...
  DBUG_ASSERT(!error_log_buffering);
  // ... but play it safe on release builds
  flush_error_log_messages();
  if (error_log_initialized)
  {
    error_log_initialized= false;
    error_log_file= NULL;
    mysql_mutex_destroy(&LOCK_error_log);
  }
}


bool reopen_error_log()
{
  if (!error_log_file)
    return false;
  mysql_mutex_lock(&LOCK_error_log);
  bool result= open_error_log(error_log_file, true);
  mysql_mutex_unlock(&LOCK_error_log);
  if (result)
    my_error(ER_CANT_OPEN_ERROR_LOG, MYF(0), error_log_file, ".", "");
  return result;
}


#ifndef EMBEDDED_LIBRARY
static void print_buffer_to_file(enum loglevel level, const char *buffer,
                                 size_t length)
{
  DBUG_ENTER("print_buffer_to_file");
  DBUG_PRINT("enter",("buffer: %s", buffer));

  char my_timestamp[iso8601_size];

  my_thread_id thread_id= 0;

  /*
    If the thread system is up and running and we're in a connection,
    add the connection ID to the log-line, otherwise 0.
  */
  if (THR_THD_initialized && (current_thd != NULL))
    thread_id= current_thd->thread_id();

  make_iso8601_timestamp(my_timestamp);

  /*
    This must work even if the mutex has not been initialized yet.
    At that point we should still be single threaded so that it is
    safe to write without mutex.
  */
  if (error_log_initialized)
    mysql_mutex_lock(&LOCK_error_log);

  if (error_log_buffering)
  {
    // Logfile not open yet, buffer messages for now.
    if (buffered_messages == NULL)
      buffered_messages= new (std::nothrow) std::string();
    std::ostringstream s;
    s << my_timestamp << " " << thread_id;
    if (level == ERROR_LEVEL)
      s << " [ERROR] ";
    else if (level == WARNING_LEVEL)
      s << " [Warning] ";
    else
      s << " [Note] ";
    s << buffer << std::endl;
    buffered_messages->append(s.str());
  }
  else
  {
    fprintf(stderr, "%s %u [%s] %.*s\n",
            my_timestamp,
            thread_id,
            (level == ERROR_LEVEL ? "ERROR" : level == WARNING_LEVEL ?
             "Warning" : "Note"),
            (int) length, buffer);

    fflush(stderr);
  }

  if (error_log_initialized)
    mysql_mutex_unlock(&LOCK_error_log);
  DBUG_VOID_RETURN;
}


void error_log_print(enum loglevel level, const char *format, va_list args)
{
  char   buff[MAX_LOG_BUFFER_SIZE];
  size_t length;
  DBUG_ENTER("error_log_print");

  if (static_cast<ulong>(level) < log_error_verbosity)
  {
    length= my_vsnprintf(buff, sizeof(buff), format, args);
    print_buffer_to_file(level, buff, length);

    if (log_syslog_enabled
#ifdef _WIN32
    && !abort_loop // Don't write to the eventlog during shutdown.
#endif
      )
    {
      my_syslog(system_charset_info, level, buff);
    }
  }

  DBUG_VOID_RETURN;
}
#endif /* EMBEDDED_LIBRARY */


void sql_print_error(const char *format, ...)
{
  va_list args;
  DBUG_ENTER("sql_print_error");

  va_start(args, format);
  error_log_print(ERROR_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}


void sql_print_warning(const char *format, ...)
{
  va_list args;
  DBUG_ENTER("sql_print_warning");

  va_start(args, format);
  error_log_print(WARNING_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}


void sql_print_information(const char *format, ...)
{
  va_list args;
  DBUG_ENTER("sql_print_information");

  va_start(args, format);
  error_log_print(INFORMATION_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}


extern "C"
int my_plugin_log_message(MYSQL_PLUGIN *plugin_ptr, plugin_log_level level,
                          const char *format, ...)
{
  char format2[MAX_LOG_BUFFER_SIZE];
  loglevel lvl;
  struct st_plugin_int *plugin = static_cast<st_plugin_int *> (*plugin_ptr);
  va_list args;

  DBUG_ASSERT(level >= MY_ERROR_LEVEL || level <= MY_INFORMATION_LEVEL);

  switch (level)
  {
  case MY_ERROR_LEVEL:       lvl= ERROR_LEVEL; break;
  case MY_WARNING_LEVEL:     lvl= WARNING_LEVEL; break;
  case MY_INFORMATION_LEVEL: lvl= INFORMATION_LEVEL; break;
  default:                   return 1;
  }

  va_start(args, format);
  my_snprintf(format2, sizeof (format2) - 1, "Plugin %.*s reported: '%s'",
              (int) plugin->name.length, plugin->name.str, format);
  error_log_print(lvl, format2, args);
  va_end(args);
  return 0;
}
