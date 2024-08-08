/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  @brief
  logging of commands
*/

#include "sql/log.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_string.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/strings/int2str.h"
#include "mysql_time.h"
#include "nulls.h"
#include "server_component/log_sink_buffer.h"  // log_sink_buffer_flush()
#include "sql_string.h"
#include "strxnmov.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <new>
#include <sstream>
#include <string>
#include <utility>

#include "lex_string.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_double2ulonglong.h"
#include "my_time.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/service_my_plugin_log.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "mysys_err.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"  // ER_DEFAULT
#include "sql/discrete_interval.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/mysqld.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"  // key_memory_File_query_log_name
#include "sql/query_options.h"
#include "sql/sql_audit.h"  // mysql_event_tracking_general_notify
#include "sql/sql_base.h"   // close_log_table
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"  // sql_command_flags
#include "sql/sql_plugin_ref.h"
#include "sql/sql_time.h"  // calc_time_from_sec
#include "sql/system_variables.h"
#include "sql/table.h"  // TABLE_FIELD_TYPE
#include "string_with_len.h"
#include "strmake.h"
#include "strxmov.h"
#include "thr_lock.h"
#include "thr_mutex.h"
#ifdef _WIN32
#include "sql/message.h"
#else
#endif

#include "sql/server_component/log_builtins_imp.h"

using std::max;
using std::min;

enum enum_slow_query_log_table_field {
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

static const TABLE_FIELD_TYPE slow_query_log_table_fields[SQLT_FIELD_COUNT] = {
    {{STRING_WITH_LEN("start_time")},
     {STRING_WITH_LEN("timestamp(6)")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("user_host")},
     {STRING_WITH_LEN("mediumtext")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("query_time")},
     {STRING_WITH_LEN("time(6)")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("lock_time")},
     {STRING_WITH_LEN("time(6)")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("rows_sent")}, {STRING_WITH_LEN("int")}, {nullptr, 0}},
    {{STRING_WITH_LEN("rows_examined")},
     {STRING_WITH_LEN("int")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("db")},
     {STRING_WITH_LEN("varchar(512)")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("last_insert_id")},
     {STRING_WITH_LEN("int")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("insert_id")}, {STRING_WITH_LEN("int")}, {nullptr, 0}},
    {{STRING_WITH_LEN("server_id")},
     {STRING_WITH_LEN("int unsigned")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("sql_text")},
     {STRING_WITH_LEN("mediumblob")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("thread_id")},
     {STRING_WITH_LEN("bigint unsigned")},
     {nullptr, 0}}};

static const TABLE_FIELD_DEF slow_query_log_table_def = {
    SQLT_FIELD_COUNT, slow_query_log_table_fields};

enum enum_general_log_table_field {
  GLT_FIELD_EVENT_TIME = 0,
  GLT_FIELD_USER_HOST,
  GLT_FIELD_THREAD_ID,
  GLT_FIELD_SERVER_ID,
  GLT_FIELD_COMMAND_TYPE,
  GLT_FIELD_ARGUMENT,
  GLT_FIELD_COUNT
};

static const TABLE_FIELD_TYPE general_log_table_fields[GLT_FIELD_COUNT] = {
    {{STRING_WITH_LEN("event_time")},
     {STRING_WITH_LEN("timestamp(6)")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("user_host")},
     {STRING_WITH_LEN("mediumtext")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("thread_id")},
     {STRING_WITH_LEN("bigint unsigned")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("server_id")},
     {STRING_WITH_LEN("int unsigned")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("command_type")},
     {STRING_WITH_LEN("varchar(64)")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("argument")},
     {STRING_WITH_LEN("mediumblob")},
     {nullptr, 0}}};

static const TABLE_FIELD_DEF general_log_table_def = {GLT_FIELD_COUNT,
                                                      general_log_table_fields};

class Query_log_table_intact : public Table_check_intact {
 protected:
  void report_error(uint ecode, const char *fmt, ...) override
      MY_ATTRIBUTE((format(printf, 3, 4))) {
    longlong log_ecode = 0;
    switch (ecode) {
      case 0:
        log_ecode = ER_SERVER_TABLE_CHECK_FAILED;
        break;
      case ER_CANNOT_LOAD_FROM_TABLE_V2:
        log_ecode = ER_SERVER_CANNOT_LOAD_FROM_TABLE_V2;
        break;
      case ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2:
        log_ecode = ER_SERVER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2;
        break;
      case ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2:
        log_ecode = ER_SERVER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2;
        break;
      default:
        assert(false);
        return;
    }

    va_list args;
    va_start(args, fmt);
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .errcode(log_ecode)
        .messagev(fmt, args);
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

class Silence_log_table_errors : public Internal_error_handler {
  char m_message[MYSQL_ERRMSG_SIZE];

 public:
  Silence_log_table_errors() { m_message[0] = '\0'; }

  bool handle_condition(THD *, uint, const char *,
                        Sql_condition::enum_severity_level *,
                        const char *msg) override {
    strmake(m_message, msg, sizeof(m_message) - 1);
    return true;
  }

  const char *message() const { return m_message; }
};

static void ull2timeval(ulonglong utime, my_timeval *tv) {
  assert(tv != nullptr);
  assert(utime > 0); /* should hold true in this context */
  tv->m_tv_sec = static_cast<int64_t>(utime / 1000000);
  tv->m_tv_usec = utime % 1000000;
}

class File_query_log {
  File_query_log(enum_log_table_type log_type);

  ~File_query_log() {
    assert(!is_open());
    if (name != nullptr) {
      my_free(name);
      name = nullptr;
    }
    mysql_mutex_destroy(&LOCK_log);
  }

  /** @return true if the file log is open, false otherwise. */
  bool is_open() const { return log_open; }

  /**
     Open a (new) log file.

     Open the logfile, init IO_CACHE and write startup messages.

     @return true if error, false otherwise.
  */
  bool open();

  /**
     Close the log file

     @note One can do an open on the object at once after doing a close.
     The internal structures are not freed until the destructor is called.
  */
  void close();

  /**
     Change what file we log to
  */
  bool set_file(const char *new_name);

  /**
     Check if we have already printed ER_ERROR_ON_WRITE and if not,
     do so.
  */
  void check_and_print_write_error();

  /**
     Write a command to traditional general log file.
     Log given command to normal (not rotatable) log file.

     @param event_utime       Command start timestamp in micro seconds
     @param thread_id         Id of the thread that issued the query
     @param command_type      The type of the command being logged
     @param command_type_len  The length of the string above
     @param sql_text          The very text of the query being executed
     @param sql_text_len      The length of sql_text string

     @return true if error, false otherwise.
  */
  bool write_general(ulonglong event_utime, my_thread_id thread_id,
                     const char *command_type, size_t command_type_len,
                     const char *sql_text, size_t sql_text_len);

  /**
     Log a query to the traditional slow log file.

     @param thd                THD of the query
     @param current_utime      Current timestamp in microseconds
     @param query_start_utime  Command start timestamp in microseconds
     @param user_host          The pointer to the string with user\@host info
     @param user_host_len      Length of the user_host string
     @param query_utime        Number of microseconds query execution took
     @param lock_utime         Number of microseconds the query was locked
     @param is_command         The flag which determines whether the sql_text
                               is a query or an administrator command
     @param sql_text           The query or administrator in textual form
     @param sql_text_len       The length of sql_text string

     @return true if error, false otherwise.
  */
  bool write_slow(THD *thd, ulonglong current_utime,
                  ulonglong query_start_utime, const char *user_host,
                  size_t user_host_len, ulonglong query_utime,
                  ulonglong lock_utime, bool is_command, const char *sql_text,
                  size_t sql_text_len);

 private:
  /** Type of log file. */
  const enum_log_table_type m_log_type;

  /** Makes sure we only have one write at a time. */
  mysql_mutex_t LOCK_log;

  /** Log filename. */
  char *name;

  /** Path to log file. */
  char log_file_name[FN_REFLEN];

  /** Last seen current database. */
  char db[NAME_LEN + 1];

  /** Have we already printed ER_ERROR_ON_WRITE? */
  bool write_error;

  IO_CACHE log_file;

  /** True if the file log is open, false otherwise. */
  volatile bool log_open;

#ifdef HAVE_PSI_INTERFACE
  /** Instrumentation key to use for file io in @c log_file */
  PSI_file_key m_log_file_key;
#endif

  friend class Log_to_file_event_handler;
  friend class Query_logger;
};

File_query_log::File_query_log(enum_log_table_type log_type)
    : m_log_type(log_type), name(nullptr), write_error(false), log_open(false) {
  mysql_mutex_init(key_LOG_LOCK_log, &LOCK_log, MY_MUTEX_INIT_SLOW);
#ifdef HAVE_PSI_INTERFACE
  if (log_type == QUERY_LOG_GENERAL)
    m_log_file_key = key_file_general_log;
  else if (log_type == QUERY_LOG_SLOW)
    m_log_file_key = key_file_slow_log;
#endif
}

bool is_valid_log_name(const char *name, size_t len) {
  if (len > 3) {
    const char *tail = name + len - 4;
    if (my_strcasecmp(system_charset_info, tail, ".ini") == 0 ||
        my_strcasecmp(system_charset_info, tail, ".cnf") == 0) {
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
  O_NOFOLLOW flag. This will reject following symbolic links.

  @param          file                  File descriptor.
  @param          log_file_key          Key for P_S instrumentation.
  @param          open_flags            Flags to use for opening the file.
  @param          opened_file_name      Name of the open fd.
  @param [out]    real_file_name        Buffer for actual name of the fd.

  @returns file descriptor to open file with 'real_file_name', or '-1'
           in case of errors.
*/

static File mysql_file_real_name_reopen(File file,
#ifdef HAVE_PSI_FILE_INTERFACE
                                        PSI_file_key log_file_key
                                        [[maybe_unused]],
#endif
                                        int open_flags [[maybe_unused]],
                                        const char *opened_file_name,
                                        char *real_file_name) {
  assert(file);
  assert(opened_file_name);
  assert(real_file_name);

#ifdef _WIN32
  /* On Windows, O_NOFOLLOW is not supported. Verify real path from fd. */
  const DWORD real_length = GetFinalPathNameByHandle(
      my_get_osfhandle(file), real_file_name, FN_REFLEN, FILE_NAME_OPENED);

  /* May ret 0 if e.g. on a ramdisk. Ignore - return open file and name. */
  if (real_length == 0) {
    strcpy(real_file_name, opened_file_name);
    return file;
  }

  if (real_length > FN_REFLEN) {
    mysql_file_close(file, MYF(0));
    return -1;
  }

  return file;
#else
  /* On *nix, get realpath, open realpath with O_NOFOLLOW. */
  if (realpath(opened_file_name, real_file_name) == nullptr) {
    (void)mysql_file_close(file, MYF(0));
    return -1;
  }

  if (mysql_file_close(file, MYF(0))) return -1;

  /* Make sure the real path is not too long. */
  if (strlen(real_file_name) > FN_REFLEN) return -1;

  return mysql_file_open(log_file_key, real_file_name, open_flags | O_NOFOLLOW,
                         MYF(MY_WME));
#endif  //_WIN32
}

bool File_query_log::set_file(const char *new_name) {
  char *nn;

  assert(new_name && new_name[0]);

  if (!(nn = my_strdup(key_memory_File_query_log_name, new_name, MYF(MY_WME))))
    return true;

  if (name != nullptr) my_free(name);

  name = nn;

  // We can do this here since we're not actually resolving symlinks etc.
  fn_format(log_file_name, name, mysql_data_home, "", MY_UNPACK_FILENAME);

  return false;
}

bool File_query_log::open() {
  File file = -1;
  my_off_t pos = 0;
  char buff[FN_REFLEN];
  MY_STAT f_stat;
  DBUG_TRACE;

  assert(name != nullptr);

  if (is_open()) return false;

  write_error = false;

  /* File is regular writable file */
  if (my_stat(log_file_name, &f_stat, MYF(0)) && !MY_S_ISREG(f_stat.st_mode))
    goto err;

  db[0] = 0;

  /* First, open the file to make sure it exists. */
  if ((file = mysql_file_open(m_log_file_key, log_file_name,
                              O_CREAT | O_WRONLY | O_APPEND, MYF(MY_WME))) < 0)
    goto err;

#ifdef _WIN32
  char real_log_file_name[FN_REFLEN];
#else
  /* File name must have room for PATH_MAX. Checked against F_REFLEN later. */
  char real_log_file_name[PATH_MAX];
#endif  // _Win32

  /* Reopen and get real path. */
  if ((file = mysql_file_real_name_reopen(file,
#ifdef HAVE_PSI_FILE_INTERFACE
                                          m_log_file_key,
#endif
                                          O_CREAT | O_WRONLY | O_APPEND,
                                          log_file_name, real_log_file_name)) <
      0)
    goto err;

  if (!is_valid_log_name(real_log_file_name, strlen(real_log_file_name))) {
    LogErr(ERROR_LEVEL, ER_INVALID_ERROR_LOG_NAME, real_log_file_name);
    goto err;
  }

  if ((pos = mysql_file_tell(file, MYF(MY_WME))) == MY_FILEPOS_ERROR) {
    if (my_errno() == ESPIPE)
      pos = 0;
    else
      goto err;
  }

  if (init_io_cache(&log_file, file, IO_SIZE, WRITE_CACHE, pos, false,
                    MYF(MY_WME | MY_NABP)))
    goto err;

  {
    char *end;
    const size_t len =
        snprintf(buff, sizeof(buff),
                 "%s, Version: %s (%s). "
#if defined(_WIN32)
                 "started with:\nTCP Port: %d, Named Pipe: %s\n",
                 my_progname, server_version, MYSQL_COMPILATION_COMMENT_SERVER,
                 mysqld_port, mysqld_unix_port
#else
                 "started with:\nTcp port: %d  Unix socket: %s\n",
                 my_progname, server_version, MYSQL_COMPILATION_COMMENT_SERVER,
                 mysqld_port, mysqld_unix_port
#endif
        );
    end =
        my_stpncpy(buff + len, "Time                 Id Command    Argument\n",
                   sizeof(buff) - len);
    if (my_b_write(&log_file, (uchar *)buff, (uint)(end - buff)) ||
        flush_io_cache(&log_file))
      goto err;
  }

  log_open = true;
  return false;

err:
  char log_open_file_error_message[96] = "";
  if (strcmp(opt_slow_logname, name) == 0) {
    strcpy(log_open_file_error_message,
           error_message_for_error_log(ER_LOG_SLOW_CANNOT_OPEN));
  } else if (strcmp(opt_general_logname, name) == 0) {
    strcpy(log_open_file_error_message,
           error_message_for_error_log(ER_LOG_GENERAL_CANNOT_OPEN));
  }

  char errbuf[MYSYS_STRERROR_SIZE];
  my_strerror(errbuf, sizeof(errbuf), errno);
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(ERROR_LEVEL)
      .errcode(ER_LOG_FILE_CANNOT_OPEN)
      .os_errno(errno)
      .os_errmsg(errbuf)
      .lookup(ER_LOG_FILE_CANNOT_OPEN, name, errno, errbuf,
              log_open_file_error_message);
  if (file >= 0) mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);

  log_open = false;
  return true;
}

void File_query_log::close() {
  DBUG_TRACE;
  if (!is_open()) return;

  end_io_cache(&log_file);

  if (mysql_file_sync(log_file.file, MYF(MY_WME)))
    check_and_print_write_error();

  if (mysql_file_close(log_file.file, MYF(MY_WME)))
    check_and_print_write_error();

  log_open = false;
}

void File_query_log::check_and_print_write_error() {
  if (!write_error) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_strerror(errbuf, sizeof(errbuf), errno);
    write_error = true;
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .errcode(ER_FAILED_TO_WRITE_TO_FILE)
        .os_errno(errno)
        .os_errmsg(errbuf)
        .lookup(ER_FAILED_TO_WRITE_TO_FILE, name, errno, errbuf);
  }
}

bool File_query_log::write_general(ulonglong event_utime,
                                   my_thread_id thread_id,
                                   const char *command_type,
                                   size_t command_type_len,
                                   const char *sql_text, size_t sql_text_len) {
  char buff[32];
  size_t length = 0;

  mysql_mutex_lock(&LOCK_log);
  assert(is_open());

  /* Note that my_b_write() assumes it knows the length for this */
  char local_time_buff[iso8601_size];
  int time_buff_len = make_iso8601_timestamp(local_time_buff, event_utime,
                                             iso8601_sysvar_logtimestamps);

  if (my_b_write(&log_file, pointer_cast<uchar *>(local_time_buff),
                 time_buff_len))
    goto err;

  if (my_b_write(&log_file, pointer_cast<const uchar *>("\t"), 1)) goto err;

  length = snprintf(buff, 32, "%5u ", thread_id);

  if (my_b_write(&log_file, pointer_cast<uchar *>(buff), length)) goto err;

  if (my_b_write(&log_file, pointer_cast<const uchar *>(command_type),
                 command_type_len))
    goto err;

  if (my_b_write(&log_file, pointer_cast<const uchar *>("\t"), 1)) goto err;

  /* sql_text */
  if (my_b_write(&log_file, pointer_cast<const uchar *>(sql_text),
                 sql_text_len))
    goto err;

  if (my_b_write(&log_file, pointer_cast<const uchar *>("\n"), 1) ||
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
                                ulonglong query_start_utime,
                                const char *user_host, size_t,
                                ulonglong query_utime, ulonglong lock_utime,
                                bool is_command, const char *sql_text,
                                size_t sql_text_len) {
  char buff[80], *end;
  char query_time_buff[22 + 7], lock_time_buff[22 + 7];
  size_t buff_len;
  end = buff;

  mysql_mutex_lock(&LOCK_log);
  assert(is_open());

  if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT)) {
    char my_timestamp[iso8601_size];

    make_iso8601_timestamp(my_timestamp, current_utime,
                           iso8601_sysvar_logtimestamps);

    buff_len = snprintf(buff, sizeof buff, "# Time: %s\n", my_timestamp);

    /* Note that my_b_write() assumes it knows the length for this */
    if (my_b_write(&log_file, (uchar *)buff, buff_len)) goto err;

    buff_len = snprintf(buff, 32, "%5u", thd->thread_id());
    if (my_b_printf(&log_file, "# User@Host: %s  Id: %s\n", user_host, buff) ==
        (uint)-1)
      goto err;
  }

  /* For slow query log */
  sprintf(query_time_buff, "%.6f", ulonglong2double(query_utime) / 1000000.0);
  sprintf(lock_time_buff, "%.6f", ulonglong2double(lock_utime) / 1000000.0);

  /*
    As a general rule, if opt_log_slow_extra is set, the caller will
    have saved state at the beginning of execution, and passed in a
    pointer to that state in THD's copy_status_var_ptr.
  */
  if (!thd->copy_status_var_ptr) {
    if (my_b_printf(&log_file,
                    "# Query_time: %s  Lock_time: %s"
                    " Rows_sent: %lu  Rows_examined: %lu\n",
                    query_time_buff, lock_time_buff,
                    (ulong)thd->get_sent_row_count(),
                    (ulong)thd->get_examined_row_count()) == (uint)-1)
      goto err; /* purecov: inspected */
  } else {
    char start_time_buff[iso8601_size];
    char end_time_buff[iso8601_size];

    if (query_start_utime) {
      make_iso8601_timestamp(start_time_buff, query_start_utime,
                             iso8601_sysvar_logtimestamps);
      make_iso8601_timestamp(end_time_buff, query_start_utime + query_utime,
                             iso8601_sysvar_logtimestamps);
    } else {
      start_time_buff[0] = '\0'; /* purecov: inspected */
      make_iso8601_timestamp(
          end_time_buff, current_utime,
          iso8601_sysvar_logtimestamps); /* purecov: inspected */
    }

    if (my_b_printf(
            &log_file,
            "# Query_time: %s  Lock_time: %s"
            " Rows_sent: %lu  Rows_examined: %lu"
            " Thread_id: %lu Errno: %lu Killed: %lu"
            " Bytes_received: %lu Bytes_sent: %lu"
            " Read_first: %lu Read_last: %lu Read_key: %lu"
            " Read_next: %lu Read_prev: %lu"
            " Read_rnd: %lu Read_rnd_next: %lu"
            " Sort_merge_passes: %lu Sort_range_count: %lu"
            " Sort_rows: %lu Sort_scan_count: %lu"
            " Created_tmp_disk_tables: %lu"
            " Created_tmp_tables: %lu"
            " Count_hit_tmp_table_size: %lu "
            " Start: %s End: %s\n",
            query_time_buff, lock_time_buff, (ulong)thd->get_sent_row_count(),
            (ulong)thd->get_examined_row_count(), (ulong)thd->thread_id(),
            static_cast<ulong>(
                thd->is_error() ? thd->get_stmt_da()->mysql_errno() : 0),
            (ulong)thd->killed,
            (ulong)(thd->status_var.bytes_received -
                    thd->copy_status_var_ptr->bytes_received),
            (ulong)(thd->status_var.bytes_sent -
                    thd->copy_status_var_ptr->bytes_sent),
            (ulong)(thd->status_var.ha_read_first_count -
                    thd->copy_status_var_ptr->ha_read_first_count),
            (ulong)(thd->status_var.ha_read_last_count -
                    thd->copy_status_var_ptr->ha_read_last_count),
            (ulong)(thd->status_var.ha_read_key_count -
                    thd->copy_status_var_ptr->ha_read_key_count),
            (ulong)(thd->status_var.ha_read_next_count -
                    thd->copy_status_var_ptr->ha_read_next_count),
            (ulong)(thd->status_var.ha_read_prev_count -
                    thd->copy_status_var_ptr->ha_read_prev_count),
            (ulong)(thd->status_var.ha_read_rnd_count -
                    thd->copy_status_var_ptr->ha_read_rnd_count),
            (ulong)(thd->status_var.ha_read_rnd_next_count -
                    thd->copy_status_var_ptr->ha_read_rnd_next_count),
            (ulong)(thd->status_var.filesort_merge_passes -
                    thd->copy_status_var_ptr->filesort_merge_passes),
            (ulong)(thd->status_var.filesort_range_count -
                    thd->copy_status_var_ptr->filesort_range_count),
            (ulong)(thd->status_var.filesort_rows -
                    thd->copy_status_var_ptr->filesort_rows),
            (ulong)(thd->status_var.filesort_scan_count -
                    thd->copy_status_var_ptr->filesort_scan_count),
            (ulong)(thd->status_var.created_tmp_disk_tables -
                    thd->copy_status_var_ptr->created_tmp_disk_tables),
            (ulong)(thd->status_var.created_tmp_tables -
                    thd->copy_status_var_ptr->created_tmp_tables),
            (ulong)(thd->status_var.count_hit_tmp_table_size -
                    thd->copy_status_var_ptr->count_hit_tmp_table_size),
            start_time_buff, end_time_buff) == (uint)-1)
      goto err; /* purecov: inspected */
  }

  if (thd->db().str && strcmp(thd->db().str, db)) {  // Database changed
    if (my_b_printf(&log_file, "use %s;\n", thd->db().str) == (uint)-1)
      goto err;
    my_stpcpy(db, thd->db().str);
  }
  if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt) {
    end = my_stpcpy(end, ",last_insert_id=");
    end = longlong10_to_str(
        (longlong)thd->first_successful_insert_id_in_prev_stmt_for_binlog, end,
        -10);
  }
  // Save value if we do an insert.
  if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0) {
    if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT)) {
      end = my_stpcpy(end, ",insert_id=");
      end = longlong10_to_str(
          (longlong)thd->auto_inc_intervals_in_cur_stmt_for_binlog.minimum(),
          end, -10);
    }
  }

  /*
    The timestamp used to only be set when the query had checked the
    start time. Now the slow log always logs the query start time.
    This ensures logs can be used to replicate queries accurately.
  */
  end = my_stpcpy(end, ",timestamp=");
  end = longlong10_to_str(query_start_utime / 1000000, end, 10);

  if (end != buff) {
    *end++ = ';';
    *end = '\n';
    if (my_b_write(&log_file, pointer_cast<const uchar *>("SET "), 4) ||
        my_b_write(&log_file, (uchar *)buff + 1, (uint)(end - buff)))
      goto err;
  }
  if (is_command) {
    end = strxmov(buff, "# administrator command: ", NullS);
    buff_len = (ulong)(end - buff);
    DBUG_EXECUTE_IF("simulate_slow_log_write_error",
                    { DBUG_SET("+d,simulate_file_write_error"); });
    if (my_b_write(&log_file, (uchar *)buff, buff_len)) goto err;
  }
  if (my_b_write(&log_file, pointer_cast<const uchar *>(sql_text),
                 sql_text_len) ||
      my_b_write(&log_file, pointer_cast<const uchar *>(";\n"), 2) ||
      flush_io_cache(&log_file))
    goto err;

  mysql_mutex_unlock(&LOCK_log);
  return false;

err:
  check_and_print_write_error();
  mysql_mutex_unlock(&LOCK_log);
  return true;
}

bool Log_to_csv_event_handler::log_general(
    THD *thd, ulonglong event_utime, const char *user_host,
    size_t user_host_len, my_thread_id thread_id, const char *command_type,
    size_t command_type_len, const char *sql_text, size_t sql_text_len,
    const CHARSET_INFO *client_cs) {
  TABLE *table = nullptr;
  bool result = true;
  bool need_close = false;
  bool need_rnd_end = false;
  uint field_index;
  my_timeval tv;

  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  const bool save_time_zone_used = thd->time_zone_used;

  const ulonglong save_thd_options = thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;

  Table_ref table_list(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                       GENERAL_LOG_NAME.str, GENERAL_LOG_NAME.length,
                       GENERAL_LOG_NAME.str, TL_WRITE_CONCURRENT_INSERT);

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
  if (!(table = open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close = true;

  if (log_table_intact.check(thd, table_list.table, &general_log_table_def))
    goto err;

  if (table->file->ha_extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(false))
    goto err;

  need_rnd_end = true;

  /* Honor next number columns if present */
  table->next_number_field = table->found_next_number_field;

  /*
    NOTE: we do not call restore_record() here, as all fields are
    filled by the Logger (=> no need to load default ones).
  */

  /*
    We do not set a value for table->field[0], as it will use
    default value (which is CURRENT_TIMESTAMP).
  */

  assert(table->field[GLT_FIELD_EVENT_TIME]->type() == MYSQL_TYPE_TIMESTAMP);
  ull2timeval(event_utime, &tv);
  table->field[GLT_FIELD_EVENT_TIME]->store_timestamp(&tv);

  /* do a write */
  if (table->field[GLT_FIELD_USER_HOST]->store(user_host, user_host_len,
                                               client_cs) ||
      table->field[GLT_FIELD_THREAD_ID]->store((longlong)thread_id, true) ||
      table->field[GLT_FIELD_SERVER_ID]->store((longlong)server_id, true) ||
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
  for (field_index = GLT_FIELD_COUNT; field_index < table->s->fields;
       field_index++) {
    table->field[field_index]->set_default();
  }

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0])) goto err;

  result = false;

err:
  thd->pop_internal_handler();

  if (result && !thd->killed) {
    LogErr(ERROR_LEVEL, ER_LOG_CANNOT_WRITE, "mysql.general_log",
           error_handler.message());
  }

  if (need_rnd_end) {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }

  if (need_close) close_log_table(thd, &open_tables_backup);

  thd->variables.option_bits = save_thd_options;
  thd->time_zone_used = save_time_zone_used;
  return result;
}

bool Log_to_csv_event_handler::log_slow(
    THD *thd, ulonglong current_utime, ulonglong query_start_arg,
    const char *user_host, size_t user_host_len, ulonglong query_utime,
    ulonglong lock_utime, bool, const char *sql_text, size_t sql_text_len) {
  TABLE *table = nullptr;
  bool result = true;
  bool need_close = false;
  bool need_rnd_end = false;
  const CHARSET_INFO *client_cs = thd->variables.character_set_client;
  my_timeval tv;
  const char *reason = "";

  DBUG_TRACE;

  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  const bool save_time_zone_used = thd->time_zone_used;

  Table_ref table_list(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                       SLOW_LOG_NAME.str, SLOW_LOG_NAME.length,
                       SLOW_LOG_NAME.str, TL_WRITE_CONCURRENT_INSERT);

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);

  Open_tables_backup open_tables_backup;
  if (!(table = open_log_table(thd, &table_list, &open_tables_backup))) {
    reason = "cannot open table for slow log";
    goto err;
  }

  need_close = true;

  if (log_table_intact.check(thd, table_list.table,
                             &slow_query_log_table_def)) {
    reason = "slow table intact check failed";
    goto err;
  }

  if (table->file->ha_extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(false)) {
    reason = "mark log or init failed";
    goto err;
  }

  need_rnd_end = true;

  /* Honor next number columns if present */
  table->next_number_field = table->found_next_number_field;

  restore_record(table, s->default_values);  // Get empty record

  /* store the time and user values */
  assert(table->field[SQLT_FIELD_START_TIME]->type() == MYSQL_TYPE_TIMESTAMP);
  ull2timeval(current_utime, &tv);
  table->field[SQLT_FIELD_START_TIME]->store_timestamp(&tv);

  table->field[SQLT_FIELD_USER_HOST]->store(user_host, user_host_len,
                                            client_cs);

  if (query_start_arg) {
    ha_rows rows_examined;

    /*
      A TIME field can not hold the full longlong range; query_time or
      lock_time may be truncated without warning here, if greater than
      839 hours (~35 days)
    */
    MYSQL_TIME t;
    t.neg = false;

    // overflow TIME-max
    DBUG_EXECUTE_IF("slow_log_table_max_rows_examined", {
      query_utime = (longlong)1555826389LL * 1000000 + 1;
      lock_utime = query_utime;
    });

    /* fill in query_time field */
    query_utime = min((ulonglong)query_utime,
                      (ulonglong)TIME_MAX_VALUE_SECONDS * 1000000LL);
    calc_time_from_sec(&t, static_cast<long>(query_utime / 1000000LL),
                       query_utime % 1000000);
    table->field[SQLT_FIELD_QUERY_TIME]->store_time(&t);
    /* lock_time */
    lock_utime = min((ulonglong)lock_utime,
                     (ulonglong)TIME_MAX_VALUE_SECONDS * 1000000LL);
    calc_time_from_sec(&t, static_cast<long>(lock_utime / 1000000LL),
                       lock_utime % 1000000);
    table->field[SQLT_FIELD_LOCK_TIME]->store_time(&t);
    /* rows_sent */
    table->field[SQLT_FIELD_ROWS_SENT]->store(
        (longlong)thd->get_sent_row_count(), true);
    /* rows_examined */
    rows_examined = thd->get_examined_row_count();
    DBUG_EXECUTE_IF("slow_log_table_max_rows_examined",
                    { rows_examined = 4294967294LL; });  // overflow 4-byte int
    table->field[SQLT_FIELD_ROWS_EXAMINED]->store((longlong)rows_examined,
                                                  true);
  } else {
    table->field[SQLT_FIELD_QUERY_TIME]->set_null();
    table->field[SQLT_FIELD_LOCK_TIME]->set_null();
    table->field[SQLT_FIELD_ROWS_SENT]->set_null();
    table->field[SQLT_FIELD_ROWS_EXAMINED]->set_null();
  }
  /* fill database field */
  if (thd->db().str) {
    if (!table->field[SQLT_FIELD_DATABASE]->store(thd->db().str,
                                                  thd->db().length, client_cs))
      table->field[SQLT_FIELD_DATABASE]->set_notnull();
  }

  if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt) {
    if (!table->field[SQLT_FIELD_LAST_INSERT_ID]->store(
            (longlong)thd->first_successful_insert_id_in_prev_stmt_for_binlog,
            true))
      table->field[SQLT_FIELD_LAST_INSERT_ID]->set_notnull();
  }

  /*
    Set value if we do an insert on autoincrement column. Note that for
    some engines (those for which get_auto_increment() does not leave a
    table lock until the statement ends), this is just the first value and
    the next ones used may not be contiguous to it.
  */
  if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0) {
    if (!table->field[SQLT_FIELD_INSERT_ID]->store(
            (longlong)thd->auto_inc_intervals_in_cur_stmt_for_binlog.minimum(),
            true))
      table->field[SQLT_FIELD_INSERT_ID]->set_notnull();
  }

  if (!table->field[SQLT_FIELD_SERVER_ID]->store((longlong)server_id, true))
    table->field[SQLT_FIELD_SERVER_ID]->set_notnull();

  /*
    Column sql_text.
    A positive return value in store() means truncation.
    Still logging a message in the log in this case.
  */
  table->field[SQLT_FIELD_SQL_TEXT]->store(sql_text, sql_text_len, client_cs);

  table->field[SQLT_FIELD_THREAD_ID]->store((longlong)thd->thread_id(), true);

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0])) {
    reason = "write slow table failed";
    goto err;
  }

  result = false;

err:
  thd->pop_internal_handler();

  if (result && !thd->killed) {
    LogErr(ERROR_LEVEL, ER_LOG_CANNOT_WRITE_EXTENDED, "mysql.slow_log",
           error_handler.message(), reason);
  }

  if (need_rnd_end) {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }

  if (need_close) close_log_table(thd, &open_tables_backup);

  thd->time_zone_used = save_time_zone_used;
  return result;
}

bool Log_to_csv_event_handler::activate_log(
    THD *thd, enum_log_table_type log_table_type) {
  DBUG_TRACE;

  const char *log_name = nullptr;
  size_t log_name_length = 0;

  switch (log_table_type) {
    case QUERY_LOG_GENERAL:
      log_name = GENERAL_LOG_NAME.str;
      log_name_length = GENERAL_LOG_NAME.length;
      break;
    case QUERY_LOG_SLOW:
      log_name = SLOW_LOG_NAME.str;
      log_name_length = SLOW_LOG_NAME.length;
      break;
    default:
      assert(false);
  }

  Table_ref table_list(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                       log_name, log_name_length, log_name,
                       TL_WRITE_CONCURRENT_INSERT);

  Open_tables_backup open_tables_backup;
  if (open_log_table(thd, &table_list, &open_tables_backup) != nullptr) {
    close_log_table(thd, &open_tables_backup);
    return false;
  }
  return true;
}

/**
   Class responsible for file based logging.
   Basically a wrapper around File_query_log.
*/
class Log_to_file_event_handler : public Log_event_handler {
  File_query_log mysql_general_log;
  File_query_log mysql_slow_log;

 public:
  /**
     Wrapper around File_query_log::write_slow() for slow log.
     @see Log_event_handler::log_slow().
  */
  bool log_slow(THD *thd, ulonglong current_utime, ulonglong query_start_arg,
                const char *user_host, size_t user_host_len,
                ulonglong query_utime, ulonglong lock_utime, bool is_command,
                const char *sql_text, size_t sql_text_len) override;

  /**
     Wrapper around File_query_log::write_general() for general log.
     @see Log_event_handler::log_general().
  */
  bool log_general(THD *thd, ulonglong event_utime, const char *user_host,
                   size_t user_host_len, my_thread_id thread_id,
                   const char *command_type, size_t command_type_len,
                   const char *sql_text, size_t sql_text_len,
                   const CHARSET_INFO *client_cs) override;

 private:
  Log_to_file_event_handler()
      : mysql_general_log(QUERY_LOG_GENERAL), mysql_slow_log(QUERY_LOG_SLOW) {}

  /** Close slow and general log files. */
  void cleanup() {
    mysql_general_log.close();
    mysql_slow_log.close();
  }

  /** @return File_query_log instance responsible for writing to slow/general
   * log.*/
  File_query_log *get_query_log(enum_log_table_type log_type) {
    if (log_type == QUERY_LOG_SLOW) return &mysql_slow_log;
    assert(log_type == QUERY_LOG_GENERAL);
    return &mysql_general_log;
  }

  friend class Query_logger;
};

bool Log_to_file_event_handler::log_slow(
    THD *thd, ulonglong current_utime, ulonglong query_start_utime,
    const char *user_host, size_t user_host_len, ulonglong query_utime,
    ulonglong lock_utime, bool is_command, const char *sql_text,
    size_t sql_text_len) {
  if (!mysql_slow_log.is_open()) return false;

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  const bool retval = mysql_slow_log.write_slow(
      thd, current_utime, query_start_utime, user_host, user_host_len,
      query_utime, lock_utime, is_command, sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}

bool Log_to_file_event_handler::log_general(
    THD *thd, ulonglong event_utime, const char *, size_t,
    my_thread_id thread_id, const char *command_type, size_t command_type_len,
    const char *sql_text, size_t sql_text_len, const CHARSET_INFO *) {
  if (!mysql_general_log.is_open()) return false;

  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  const bool retval =
      mysql_general_log.write_general(event_utime, thread_id, command_type,
                                      command_type_len, sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}

bool Query_logger::is_log_table_enabled(enum_log_table_type log_type) const {
  if (log_type == QUERY_LOG_SLOW)
    return (opt_slow_log && (log_output_options & LOG_TABLE));
  else if (log_type == QUERY_LOG_GENERAL)
    return (opt_general_log && (log_output_options & LOG_TABLE));
  assert(false);
  return false; /* make compiler happy */
}

void Query_logger::init() {
  file_log_handler = new Log_to_file_event_handler;  // Causes mutex init
  mysql_rwlock_init(key_rwlock_LOCK_logger, &LOCK_logger);
}

void Query_logger::cleanup() {
  mysql_rwlock_destroy(&LOCK_logger);

  assert(file_log_handler);
  file_log_handler->cleanup();
  delete file_log_handler;
  file_log_handler = nullptr;
}

bool Query_logger::slow_log_write(THD *thd, const char *query,
                                  size_t query_length, bool aggregate,
                                  ulonglong lock_usec, ulonglong exec_usec) {
  assert(thd->enable_slow_log && opt_slow_log);

  if (!(*slow_log_handler_list)) return false;

  /* do not log slow queries from replication threads */
  if (thd->slave_thread && !opt_log_slow_replica_statements) return false;

  /* fill in user_host value: the format is "%s[%s] @ %s [%s]" */
  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  Security_context *sctx = thd->security_context();
  const LEX_CSTRING sctx_user = sctx->user();
  const LEX_CSTRING sctx_host = sctx->host();
  const LEX_CSTRING sctx_ip = sctx->ip();
  const size_t user_host_len =
      (strxnmov(user_host_buff, MAX_USER_HOST_SIZE, sctx->priv_user().str, "[",
                sctx_user.length ? sctx_user.str : "", "] @ ",
                sctx_host.length ? sctx_host.str : "", " [",
                sctx_ip.length ? sctx_ip.str : "", "]", NullS) -
       user_host_buff);
  const ulonglong current_utime = my_micro_time();
  ulonglong query_utime, lock_utime;
  if (aggregate) {
    query_utime = exec_usec;
    lock_utime = lock_usec;
  } else if (thd->start_utime) {
    query_utime = (current_utime - thd->start_utime);
    lock_utime = thd->get_lock_usec();
  } else {
    query_utime = 0;
    lock_utime = 0;
  }

  bool is_command = false;
  if (!query) {
    is_command = true;
    const std::string &cn = Command_names::str_global(thd->get_command());
    query = cn.c_str();
    query_length = cn.length();
  }

  mysql_rwlock_rdlock(&LOCK_logger);

  bool error = false;
  for (Log_event_handler **current_handler = slow_log_handler_list;
       *current_handler;) {
    error |= (*current_handler++)
                 ->log_slow(thd, current_utime,
                            (thd->start_time.tv_sec * 1000000ULL) +
                                thd->start_time.tv_usec,
                            user_host_buff, user_host_len, query_utime,
                            lock_utime, is_command, query, query_length);
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
static bool log_command(THD *thd, enum_server_command command) {
  if (what_to_log & (1L << (uint)command)) {
    Security_context *sctx = thd->security_context();
    if ((thd->variables.option_bits & OPTION_LOG_OFF) &&
        (sctx->check_access(SUPER_ACL) ||
         sctx->has_global_grant(STRING_WITH_LEN("CONNECTION_ADMIN")).first)) {
      /* No logging */
      return false;
    }
    return true;
  }
  return false;
}

bool Query_logger::general_log_write(THD *thd, enum_server_command command,
                                     const char *query, size_t query_length) {
  /* Send a general log message to the audit API. */
  const std::string &cn = Command_names::str_global(command);
  mysql_event_tracking_general_notify(thd, cn.c_str(), cn.length());

  /*
    Do we want to log this kind of command?
    Is general log enabled?
    Any active handlers?
  */
  if (!log_command(thd, command) || !opt_general_log ||
      !(*general_log_handler_list))
    return false;

  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  const size_t user_host_len =
      make_user_name(thd->security_context(), user_host_buff);
  const ulonglong current_utime = my_micro_time();

  mysql_rwlock_rdlock(&LOCK_logger);

  bool error = false;
  for (Log_event_handler **current_handler = general_log_handler_list;
       *current_handler;) {
    error |=
        (*current_handler++)
            ->log_general(thd, current_utime, user_host_buff, user_host_len,
                          thd->thread_id(), cn.c_str(), cn.length(), query,
                          query_length, thd->variables.character_set_client);
  }
  mysql_rwlock_unlock(&LOCK_logger);

  return error;
}

bool Query_logger::general_log_print(THD *thd, enum_server_command command,
                                     const char *format, ...) {
  /*
    Do we want to log this kind of command?
    Is general log enabled?
    Any active handlers?
  */
  if (!log_command(thd, command) || !opt_general_log ||
      !(*general_log_handler_list)) {
    /* Send a general log message to the audit API. */
    const std::string &cn = Command_names::str_global(command);
    mysql_event_tracking_general_notify(thd, cn.c_str(), cn.length());
    return false;
  }

  size_t message_buff_len = 0;
  char message_buff[LOG_BUFF_MAX];

  /* prepare message */
  if (format) {
    va_list args;
    va_start(args, format);
    message_buff_len =
        vsnprintf(message_buff, sizeof(message_buff), format, args);
    va_end(args);

    message_buff_len = std::min(message_buff_len, sizeof(message_buff) - 1);
  } else
    message_buff[0] = '\0';

  return general_log_write(thd, command, message_buff, message_buff_len);
}

void Query_logger::init_query_log(enum_log_table_type log_type,
                                  ulonglong log_printer) {
  if (log_type == QUERY_LOG_SLOW) {
    if (log_printer & LOG_NONE) {
      slow_log_handler_list[0] = nullptr;
      return;
    }

    switch (log_printer) {
      case LOG_FILE:
        slow_log_handler_list[0] = file_log_handler;
        slow_log_handler_list[1] = nullptr;
        break;
      case LOG_TABLE:
        slow_log_handler_list[0] = &table_log_handler;
        slow_log_handler_list[1] = nullptr;
        break;
      case LOG_TABLE | LOG_FILE:
        slow_log_handler_list[0] = file_log_handler;
        slow_log_handler_list[1] = &table_log_handler;
        slow_log_handler_list[2] = nullptr;
        break;
    }
  } else if (log_type == QUERY_LOG_GENERAL) {
    if (log_printer & LOG_NONE) {
      general_log_handler_list[0] = nullptr;
      return;
    }

    switch (log_printer) {
      case LOG_FILE:
        general_log_handler_list[0] = file_log_handler;
        general_log_handler_list[1] = nullptr;
        break;
      case LOG_TABLE:
        general_log_handler_list[0] = &table_log_handler;
        general_log_handler_list[1] = nullptr;
        break;
      case LOG_TABLE | LOG_FILE:
        general_log_handler_list[0] = file_log_handler;
        general_log_handler_list[1] = &table_log_handler;
        general_log_handler_list[2] = nullptr;
        break;
    }
  } else
    assert(false);
}

void Query_logger::set_handlers(ulonglong log_printer) {
  mysql_rwlock_wrlock(&LOCK_logger);

  init_query_log(QUERY_LOG_SLOW, log_printer);
  init_query_log(QUERY_LOG_GENERAL, log_printer);

  mysql_rwlock_unlock(&LOCK_logger);
}

bool Query_logger::activate_log_handler(THD *thd,
                                        enum_log_table_type log_type) {
  bool res = false;
  mysql_rwlock_wrlock(&LOCK_logger);
  if (table_log_handler.activate_log(thd, log_type) ||
      file_log_handler->get_query_log(log_type)->open())
    res = true;
  else
    init_query_log(log_type, log_output_options);
  mysql_rwlock_unlock(&LOCK_logger);
  return res;
}

void Query_logger::deactivate_log_handler(enum_log_table_type log_type) {
  mysql_rwlock_wrlock(&LOCK_logger);
  file_log_handler->get_query_log(log_type)->close();
  // table_list_handler has no state, nothing to close
  mysql_rwlock_unlock(&LOCK_logger);
}

bool Query_logger::set_log_file(enum_log_table_type log_type) {
  const char *log_name = nullptr;

  mysql_rwlock_wrlock(&LOCK_logger);

  DEBUG_SYNC(current_thd, "log_set_file_holds_lock");

  if (log_type == QUERY_LOG_SLOW)
    log_name = opt_slow_logname;
  else if (log_type == QUERY_LOG_GENERAL)
    log_name = opt_general_logname;
  else
    assert(false);

  const bool res =
      file_log_handler->get_query_log(log_type)->set_file(log_name);

  mysql_rwlock_unlock(&LOCK_logger);

  return res;
}

bool Query_logger::reopen_log_file(enum_log_table_type log_type) {
  mysql_rwlock_wrlock(&LOCK_logger);
  file_log_handler->get_query_log(log_type)->close();
  const bool res = file_log_handler->get_query_log(log_type)->open();
  mysql_rwlock_unlock(&LOCK_logger);
  return res;
}

enum_log_table_type Query_logger::check_if_log_table(
    Table_ref *table_list, bool check_if_opened) const {
  if (table_list->db_length == MYSQL_SCHEMA_NAME.length &&
      !my_strcasecmp(system_charset_info, table_list->db,
                     MYSQL_SCHEMA_NAME.str)) {
    if (table_list->table_name_length == GENERAL_LOG_NAME.length &&
        !my_strcasecmp(system_charset_info, table_list->table_name,
                       GENERAL_LOG_NAME.str)) {
      if (!check_if_opened || is_log_table_enabled(QUERY_LOG_GENERAL))
        return QUERY_LOG_GENERAL;
      return QUERY_LOG_NONE;
    }

    if (table_list->table_name_length == SLOW_LOG_NAME.length &&
        !my_strcasecmp(system_charset_info, table_list->table_name,
                       SLOW_LOG_NAME.str)) {
      if (!check_if_opened || is_log_table_enabled(QUERY_LOG_SLOW))
        return QUERY_LOG_SLOW;
      return QUERY_LOG_NONE;
    }
  }
  return QUERY_LOG_NONE;
}

bool Query_logger::is_log_file_enabled(enum_log_table_type log_type) const {
  return file_log_handler->get_query_log(log_type)->is_open();
}

Query_logger query_logger;

char *make_query_log_name(char *buff, enum_log_table_type log_type) {
  const char *log_ext = "";
  if (log_type == QUERY_LOG_GENERAL)
    log_ext = ".log";
  else if (log_type == QUERY_LOG_SLOW)
    log_ext = "-slow.log";
  else
    assert(false);

  strmake(buff, default_logfile_name, FN_REFLEN - 5);
  return fn_format(buff, buff, mysql_real_data_home, log_ext,
                   MYF(MY_UNPACK_FILENAME | MY_REPLACE_EXT));
}

bool log_slow_applicable(THD *thd) {
  DBUG_TRACE;

  /*
    The following should never be true with our current code base,
    but better to keep this here so we don't accidentally try to log a
    statement in a trigger or stored function
  */
  if (unlikely(thd->in_sub_stmt)) return false;  // Don't set time for sub stmt

  if (unlikely(thd->killed == THD::KILL_CONNECTION)) return false;

  if (unlikely(thd->is_error()) &&
      (unlikely(thd->get_stmt_da()->mysql_errno() == ER_PARSE_ERROR)))
    return false;

  const bool warn_no_index =
      ((thd->server_status &
        (SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
       opt_log_queries_not_using_indexes &&
       !(sql_command_flags[thd->lex->sql_command] & CF_STATUS_COMMAND));
  const bool log_this_query =
      ((thd->server_status & SERVER_QUERY_WAS_SLOW) || warn_no_index) &&
      (thd->get_examined_row_count() >= thd->variables.min_examined_row_limit);

  // The docs say slow queries must be counted even when the log is off.
  if (log_this_query) thd->status_var.long_query_count++;

  /*
    Do not log administrative statements unless the appropriate option is
    set.
  */
  if (thd->enable_slow_log && opt_slow_log) {
    const bool suppress_logging = log_throttle_qni.log(thd, warn_no_index);

    if (!suppress_logging && log_this_query) return true;
  }
  return false;
}

/**
  Unconditionally writes the current statement (or its rewritten version if it
  exists) to the slow query log.

  @param thd                 thread handle
*/
void log_slow_do(THD *thd) {
  THD_STAGE_INFO(thd, stage_logging_slow_query);

  if (thd->rewritten_query().length())
    query_logger.slow_log_write(thd, thd->rewritten_query().ptr(),
                                thd->rewritten_query().length(), false, 0, 0);
  else
    query_logger.slow_log_write(thd, thd->query().str, thd->query().length,
                                false, 0, 0);
}

/**
  Check whether we need to write the current statement to the slow query
  log. If so, do so. This is a wrapper for the two functions above;
  most callers should use this wrapper.  Only use the above functions
  directly if you have expensive rewriting that you only need to do if
  the query actually needs to be logged (e.g. SP variables / NAME_CONST
  substitution when executing a PROCEDURE).
  A digest of suppressed statements may be logged instead of the current
  statement.

  @param thd                 thread handle
*/
void log_slow_statement(THD *thd) {
  if (log_slow_applicable(thd)) log_slow_do(thd);
}

void Log_throttle::new_window(ulonglong now) {
  count = 0;
  window_end = now + window_size;
}

void Slow_log_throttle::new_window(ulonglong now) {
  Log_throttle::new_window(now);
  total_exec_time = 0;
  total_lock_time = 0;
}

Slow_log_throttle::Slow_log_throttle(ulong *threshold, mysql_mutex_t *lock,
                                     ulong window_usecs, log_summary_t logger,
                                     const char *msg)
    : Log_throttle(window_usecs, msg),
      total_exec_time(0),
      total_lock_time(0),
      rate(threshold),
      log_summary(logger),
      LOCK_log_throttle(lock) {}

ulong Log_throttle::prepare_summary(ulong rate) {
  ulong ret = 0;
  /*
    Previous throttling window is over or rate changed.
    Return the number of lines we throttled.
  */
  if (count > rate) {
    ret = count - rate;
    count = 0;  // prevent writing it again.
  }

  return ret;
}

void Slow_log_throttle::print_summary(THD *thd, ulong suppressed,
                                      ulonglong print_lock_time,
                                      ulonglong print_exec_time) {
  Security_context *save_sctx = thd->security_context();

  char buf[128];

  snprintf(buf, sizeof(buf), summary_template, suppressed);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->set_security_context(&aggregate_sctx);
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  (*log_summary)(thd, buf, strlen(buf), true, print_lock_time, print_exec_time);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->set_security_context(save_sctx);
  mysql_mutex_unlock(&thd->LOCK_thd_data);
}

bool Slow_log_throttle::flush(THD *thd) {
  // Write summary if we throttled.
  mysql_mutex_lock(LOCK_log_throttle);
  const ulonglong print_lock_time = total_lock_time;
  const ulonglong print_exec_time = total_exec_time;
  ulong suppressed_count = prepare_summary(*rate);
  mysql_mutex_unlock(LOCK_log_throttle);
  if (suppressed_count > 0) {
    print_summary(thd, suppressed_count, print_lock_time, print_exec_time);
    return true;
  }
  return false;
}

bool Slow_log_throttle::log(THD *thd, bool eligible) {
  bool suppress_current = false;

  /*
    If throttling is enabled, we might have to write a summary even if
    the current query is not of the type we handle.
  */
  if (*rate > 0) {
    mysql_mutex_lock(LOCK_log_throttle);

    ulong suppressed_count = 0;
    const ulonglong print_lock_time = total_lock_time;
    const ulonglong print_exec_time = total_exec_time;
    const ulonglong end_utime_of_query = my_micro_time();

    /*
      If the window has expired, we'll try to write a summary line.
      The subroutine will know whether we actually need to.
    */
    if (!in_window(end_utime_of_query)) {
      suppressed_count = prepare_summary(*rate);
      // start new window only if this is the statement type we handle
      if (eligible) new_window(end_utime_of_query);
    }
    if (eligible && inc_log_count(*rate)) {
      /*
        Current query's logging should be suppressed.
        Add its execution time and lock time to totals for the current window.
      */
      total_exec_time += (end_utime_of_query - thd->start_utime);
      total_lock_time += thd->get_lock_usec();
      suppress_current = true;
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

bool Error_log_throttle::log() {
  ulonglong end_utime_of_query = my_micro_time();
  DBUG_EXECUTE_IF(
      "simulate_error_throttle_expiry",
      end_utime_of_query += Log_throttle::LOG_THROTTLE_WINDOW_SIZE;);

  /*
    If the window has expired, we'll try to write a summary line.
    The subroutine will know whether we actually need to.
  */
  if (!in_window(end_utime_of_query)) {
    const ulong suppressed_count = prepare_summary(1);

    new_window(end_utime_of_query);

    if (suppressed_count > 0) print_summary(suppressed_count);
  }

  /*
    If this is a first error in the current window then do not suppress it.
  */
  return inc_log_count(1);
}

bool Error_log_throttle::flush() {
  // Write summary if we throttled.
  const ulong suppressed_count = prepare_summary(1);
  if (suppressed_count > 0) {
    print_summary(suppressed_count);
    return true;
  }
  return false;
}

static bool slow_log_write(THD *thd, /* purecov: inspected */
                           const char *query, size_t query_length,
                           bool aggregate, ulonglong time_usec,
                           ulonglong lock_usec) {
  return opt_slow_log &&
         query_logger.slow_log_write(thd, query, query_length, aggregate,
                                     time_usec, lock_usec);
}

Slow_log_throttle log_throttle_qni(&opt_log_throttle_queries_not_using_indexes,
                                   &LOCK_log_throttle_qni,
                                   Log_throttle::LOG_THROTTLE_WINDOW_SIZE,
                                   slow_log_write,
                                   "throttle: %10lu 'index "
                                   "not used' warning(s) suppressed.");

////////////////////////////////////////////////////////////
//
// Error Log
//
////////////////////////////////////////////////////////////

static bool error_log_initialized = false;
// This mutex prevents fprintf from different threads from being interleaved.
// It also prevents reopen while we are in the process of logging.
static mysql_mutex_t LOCK_error_log;
// This variable is different from log_error_dest.
// E.g. log_error_dest is "stderr" if we are not logging to file.
static const char *error_log_file = nullptr;

void discard_error_log_messages() {
  log_sink_buffer_flush(LOG_BUFFER_DISCARD_ONLY);
}

void flush_error_log_messages() {
  log_sink_buffer_flush(LOG_BUFFER_PROCESS_AND_DISCARD);
}

bool init_error_log() {
  assert(!error_log_initialized);
  mysql_mutex_init(key_LOCK_error_log, &LOCK_error_log, MY_MUTEX_INIT_FAST);
  /*
    ready the default filter/sink so they'll be available before/without
    the component system
  */
  error_log_initialized = true;

  if (log_builtins_init() < 0) {
    log_write_errstream(
        STRING_WITH_LEN("failed to initialize basic error logging"));
    return true;
  } else
    return false;
}

bool open_error_log(const char *filename, bool get_lock) {
  assert(filename);
  int retries = 2, errors = 0;
  MY_STAT f_stat;

  /**
    Make sure, file is writable if it exists. If file does not exists
    then make sure directory path exists and it is writable.
  */
  if (my_stat(filename, &f_stat, MYF(0))) {
    if (my_access(filename, W_OK)) {
      goto fail;
    }
  } else {
    char path[FN_REFLEN];
    size_t path_length;

    dirname_part(path, filename, &path_length);
    if (path_length && my_access(path, (F_OK | W_OK))) goto fail;
  }

  do {
    errors = 0;
    if (!my_freopen(filename, "a", stderr)) errors++;
    if (!my_freopen(filename, "a", stdout)) errors++;
  } while (retries-- && errors);

  if (errors) goto fail;

  /* The error stream must be unbuffered. */
  setbuf(stderr, nullptr);

  error_log_file = filename;  // Remember name for later reopen

  return false;

fail : {
  char errbuf[MYSYS_STRERROR_SIZE];

  if (get_lock) mysql_mutex_unlock(&LOCK_error_log);

  LogErr(ERROR_LEVEL, ER_CANT_OPEN_ERROR_LOG, filename, ": ",
         my_strerror(errbuf, sizeof(errbuf), errno));
  flush_error_log_messages();

  if (get_lock) mysql_mutex_lock(&LOCK_error_log);
}
  return true;
}

void destroy_error_log() {
  // We should have flushed before this...
  // ... but play it safe on release builds
  flush_error_log_messages();
  if (error_log_initialized) {
    error_log_initialized = false;
    error_log_file = nullptr;
    mysql_mutex_destroy(&LOCK_error_log);
    log_builtins_exit();
  }
}

bool reopen_error_log() {
  int component_failures;
  bool result = false;

  assert(error_log_initialized);

  // call flush function in all logging services
  if ((component_failures = log_builtins_error_stack_flush()) < 0) {
    // If flushing failed and there is a user session, alert the user.
    if (current_thd)
      push_warning_printf(
          current_thd, Sql_condition::SL_WARNING,
          ER_DA_ERROR_LOG_COMPONENT_FLUSH_FAILED,
          ER_THD(current_thd, ER_DA_ERROR_LOG_COMPONENT_FLUSH_FAILED),
          -component_failures);

    // Log failure to error-log.
    LogErr(ERROR_LEVEL, ER_LOG_COMPONENT_FLUSH_FAILED, -component_failures);
  }

  if (error_log_file) {
    mysql_mutex_lock(&LOCK_error_log);
    result = open_error_log(error_log_file, true);
    mysql_mutex_unlock(&LOCK_error_log);

    /*
      This may in theory get bounced to the error log if no session
      is attached to this thread (e.g. when flush/reload is called
      by SIGHUP). That's OK though as we don't hold an X-lock on
      THR_LOCK_log_stack here the way we did during
      log_builtins_error_stack_flush() above.
    */
    if (result)
      my_error(ER_DA_CANT_OPEN_ERROR_LOG, MYF(0), error_log_file, ".",
               ""); /* purecov: inspected */
  }

  return result;
}

void log_write_errstream(const char *buffer, size_t length) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("buffer: %s", buffer));

  /*
    This must work even if the mutex has not been initialized yet.
    At that point we should still be single threaded so that it is
    safe to write without mutex.
  */
  if (error_log_initialized) mysql_mutex_lock(&LOCK_error_log);

  fprintf(stderr, "%.*s\n", (int)length, buffer);
  fflush(stderr);

  if (error_log_initialized) mysql_mutex_unlock(&LOCK_error_log);
}

my_thread_id log_get_thread_id(THD *thd) { return thd->thread_id(); }

/**
  Variadic convenience function for logging.

  This fills in the array that is used by the filter and log-writer services.
  Where missing, timestamp, priority, and thread-ID (if any) are added.
  Log item source services, log item filters, and log item writers are called.

  For convenience, any number of fields may be added:

  - "well-known" field types require a type tag and the payload:
    LOG_ITEM_LOG_LABEL, "ohai"

  - "generic" field types require a type tag, a key (C-string),
    and the payload:
    LOG_ITEM_GEN_FLOAT, "myPi", 3.1415926927

  Newer items (further to the right/bottom) overwrite older ones (further
  to the left/top).

  If a message is given, it must be the last tag in the argument list.
  The message may be given verbatim as a C format string, followed by
  its arguments:

  LOG_ITEM_LOG_MESSAGE, "format string %s %d abc", "arg1", 12345

  To avoid substitutions, use

  LOG_ITEM_LOG_VERBATIM, "message from other subsys containing %user input"

  Alternatively, an error code may be specified -- the corresponding error
  message will be looked up and inserted --, followed by any arguments
  required by the error message:

  LOG_ITEM_LOG_LOOKUP, ER_CANT_SET_DATA_DIR, filename, errno, strerror(errno)

  If no message is to be included (this should never be the case for the
  error log), LOG_ITEM_END may be used instead to terminate the list.

  @param           log_type             what log should this go to?
  @param           fili                 field list:
                                        LOG_ITEM_* tag, [[key], value]
  @retval          int                  return value of log_line_submit()
*/
int log_vmessage(int log_type [[maybe_unused]], va_list fili) {
  char buff[LOG_BUFF_MAX];
  log_item_class lic;
  log_line ll;
  bool dedup;
  int wk;

  DBUG_TRACE;

  ll.count = 0;
  ll.seen = 0;
  ll.flags = 0;

  do {
    dedup = false;

    log_line_item_init(&ll);

    ll.item[ll.count].type = (log_item_type)va_arg(fili, int);

    if (ll.item[ll.count].type == LOG_ITEM_END) break;

    if ((wk = log_item_wellknown_by_type(ll.item[ll.count].type)) < 0)
      lic = LOG_UNTYPED;
    else
      lic = log_item_wellknown_get_class(wk);

    ll.item[ll.count].item_class = lic;

    // if it's not a well-known item, read the key name from va_list
    if (log_item_generic_type(ll.item[ll.count].type))
      ll.item[ll.count].key = va_arg(fili, char *);
    else if (wk >= 0)
      ll.item[ll.count].key = log_item_wellknown_get_name(wk);
    else {
      ll.item[ll.count].key = "???";
      assert(false);
    }

    // if we've already got one of this type, de-duplicate later
    if ((ll.seen & ll.item[ll.count].type) ||
        log_item_generic_type(ll.item[ll.count].type))
      dedup = true;

    // read the payload
    switch (lic) {
      case LOG_LEX_STRING:
        ll.item[ll.count].data.data_string.str = va_arg(fili, char *);
        ll.item[ll.count].data.data_string.length = va_arg(fili, size_t);
        if (ll.item[ll.count].data.data_string.str == nullptr) continue;
        break;
      case LOG_CSTRING: {
        char *p = va_arg(fili, char *);
        if (p == nullptr) continue;
        ll.item[ll.count].data.data_string.str = p;
        ll.item[ll.count].data.data_string.length = strlen(p);
        ll.item[ll.count].item_class = LOG_LEX_STRING;
      } break;
      case LOG_INTEGER:
        ll.item[ll.count].data.data_integer = va_arg(fili, longlong);
        break;
      case LOG_FLOAT:
        ll.item[ll.count].data.data_float = va_arg(fili, double);
        break;
      default:
        log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_MESSAGE,
                    "log_vmessage: unknown class %d/%d for type %d", (int)lic,
                    (int)ll.item[ll.count].item_class,
                    (int)ll.item[ll.count].type);
        log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_MESSAGE,
                    "log_vmessage: seen: 0x%lx. "
                    "trying to dump preceding %d item(s)",
                    (long)ll.seen, (int)ll.count);
        {
          int i = 0;
          while (i < ll.count) {
            if (ll.item[i].item_class == LOG_INTEGER)
              log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_MESSAGE,
                          "log_vmessage: \"%s\": %lld", ll.item[i].key,
                          (long long)ll.item[ll.count].data.data_integer);
            else if (ll.item[i].item_class == LOG_FLOAT)
              log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_MESSAGE,
                          "log_vmessage: \"%s\": %lf", ll.item[i].key,
                          (double)ll.item[ll.count].data.data_float);
            else if (ll.item[i].item_class == LOG_LEX_STRING)
              log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_MESSAGE,
                          "log_vmessage: \"%s\": \"%.*s\"", ll.item[i].key,
                          ll.item[ll.count].data.data_string.length,
                          ll.item[ll.count].data.data_string.str == nullptr
                              ? ""
                              : ll.item[ll.count].data.data_string.str);
            i++;
          }
        }
        va_end(fili);
        /*
          Bail. As the input is clearly badly broken, we don't dare try
          to free anything here.
        */
        assert(false);
        return -1;
    }

    /*
      errno is only of interest if unequal 0.
    */
    if ((ll.item[ll.count].type == LOG_ITEM_SYS_ERRNO) &&
        (ll.item[ll.count].data.data_integer == 0))
      continue;

    /*
      MySQL error can be set numerically or symbolically, so they need to
      reset each other. Submitting both is a really strange idea, mind.
     */
    const log_item_type_mask errcode_mask =
        LOG_ITEM_SQL_ERRSYMBOL | LOG_ITEM_SQL_ERRCODE;

    if ((ll.item[ll.count].type & errcode_mask) && (ll.seen & errcode_mask)) {
      size_t dd = ll.count;

      while (dd > 0) {
        dd--;

        if ((ll.item[dd].type & errcode_mask) != 0) {
          log_line_item_free(&ll, dd);

          dedup = false;
          ll.item[dd] = ll.item[ll.count];
          ll.count--;
        }
      }
    }

    /*
      For well-known messages, we replace the error code with the
      error message (and adjust the metadata accordingly).
    */
    if (ll.item[ll.count].type == LOG_ITEM_LOG_LOOKUP) {
      const size_t ec = ll.item[ll.count].data.data_integer;
      const char *msg = error_message_for_error_log(ec),
                 *key = log_item_wellknown_get_name(
                     log_item_wellknown_by_type(LOG_ITEM_LOG_MESSAGE));

      if ((msg == nullptr) || (*msg == '\0')) msg = "invalid error code";

      ll.item[ll.count].type = LOG_ITEM_LOG_MESSAGE;
      ll.item[ll.count].item_class = LOG_LEX_STRING;
      ll.item[ll.count].data.data_string.str = msg;
      ll.item[ll.count].data.data_string.length = strlen(msg);
      ll.item[ll.count].key = key;

      /*
        If no errcode and no errsymbol were set, errcode is set from
        the one used for the message (they should be the same, anyway).
        Whichever items are still missing at the point will be added
        below.
      */
      if (!(ll.seen & LOG_ITEM_SQL_ERRCODE) &&
          !(ll.seen & LOG_ITEM_SQL_ERRSYMBOL) && !log_line_full(&ll)) {
        // push up message so it remains the last item
        ll.item[ll.count + 1] = ll.item[ll.count];
        log_line_item_set(&ll, LOG_ITEM_SQL_ERRCODE)->data_integer = ec;
      }

      /*
        Currently, this can't happen (as LOG_ITEM_LOG_MESSAGE ends
        the loop), but this may change later.
      */
      if (ll.seen & ll.item[ll.count].type) dedup = true;
    }

    // message is a format string optionally followed by args
    if (ll.item[ll.count].type == LOG_ITEM_LOG_MESSAGE) {
      size_t msg_len = vsnprintf(buff, sizeof(buff),
                                 ll.item[ll.count].data.data_string.str, fili);
      if (msg_len > (sizeof(buff) - 1)) msg_len = sizeof(buff) - 1;

      buff[sizeof(buff) - 1] = '\0';
      ll.item[ll.count].data.data_string.str = buff;
      ll.item[ll.count].data.data_string.length = msg_len;
    } else if (ll.item[ll.count].type == LOG_ITEM_LOG_VERBATIM) {
      const int wellknown = log_item_wellknown_by_type(LOG_ITEM_LOG_MESSAGE);

      ll.item[ll.count].key = log_item_wellknown_get_name(wellknown);
      ll.item[ll.count].type = LOG_ITEM_LOG_MESSAGE;
      ll.item[ll.count].item_class = LOG_LEX_STRING;
      dedup = (ll.seen & ll.item[ll.count].type);
    }

    // element is given repeatedly; newer overwrites older
    if (dedup) {
      int dd = 0;

      /*
        Above, we only check whether an item of the same type already
        exists. Generic types used repeatedly will only be deduped if
        the key is the same, so there might be nothing to dedup here
        after all. (To be clear howeer, generic items of different type
        intentionally overwrite each other as long as the key is the
        same. You can NOT have a generic integer and a generic string
        both named "foo"!
      */
      while ((dd < ll.count) &&
             (log_item_generic_type(ll.item[ll.count].type)
                  ? (native_strcasecmp(ll.item[dd].key,
                                       ll.item[ll.count].key) != 0)
                  : (ll.item[dd].type != ll.item[ll.count].type)))
        dd++;

      // if it's a genuine duplicate, replace older with newer
      if (dd < ll.count) {
        log_line_item_free(&ll, dd);
        ll.item[dd] = ll.item[ll.count];
        ll.count--;
      }
    } else {
      /*
        Remember we've seen this item type. Not necessary above, even
        if the potential dedup turned out to be unnecessary (same generic,
        different key).
      */
      ll.seen |= ll.item[ll.count].type;
    }

    ll.count++;

  } while (!log_line_full(&ll) && !(ll.seen & LOG_ITEM_LOG_MESSAGE));

  va_end(fili);

  return log_line_submit(&ll);
}

/**
  Prints a printf style message to the error log.

   A thin wrapper around log_message() for local_message_hook,
   Table_check_intact::report_error, and others.

  @param level          The level of the msg significance
  @param ecode          Error code of the error message.
  @param args           va_list list of arguments for the message

*/
void error_log_print(enum loglevel level, uint ecode, va_list args) {
  DBUG_TRACE;

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .errcode(ecode)
      .prio(level)
      .messagev(EE(ecode), args);
}

/**
  Variadic convenience function for logging.

  This fills in the array that is used by the filter and log-writer services.
  Where missing, timestamp, priority, and thread-ID (if any) are added.
  Log item source services, log item filters, and log item writers are called.

  see log_vmessage() for more information.

  @param           log_type             what log should this go to?
  @param           ...                  fields: LOG_ITEM_* tag, [[key], value]
  @retval          int                  return value of log_vmessage()
*/
int log_message(int log_type, ...) {
  va_list fili;
  int ret;

  va_start(fili, log_type);
  ret = log_vmessage(log_type, fili);
  va_end(fili);

  return ret;
}

/*
  For use by plugins that wish to write a message to the error log.
  New plugins should use the service structure.
*/

int my_plugin_log_message(MYSQL_PLUGIN *plugin_ptr, plugin_log_level level,
                          const char *format, ...) {
  char format2[LOG_BUFF_MAX];
  char msg[LOG_BUFF_MAX];
  loglevel lvl;
  struct st_plugin_int *plugin = static_cast<st_plugin_int *>(*plugin_ptr);
  va_list args;

  assert(level >= MY_ERROR_LEVEL && level <= MY_INFORMATION_LEVEL);

  switch (level) {
    case MY_ERROR_LEVEL:
      lvl = ERROR_LEVEL;
      break;
    case MY_WARNING_LEVEL:
      lvl = WARNING_LEVEL;
      break;
    case MY_INFORMATION_LEVEL:
      lvl = INFORMATION_LEVEL;
      break;
    default:
      return 1;
  }

  snprintf(format2, sizeof(format2) - 1, "Plugin %.*s reported: '%s'",
           (int)plugin->name.length, plugin->name.str, format);

  va_start(args, format);
  vsnprintf(msg, sizeof(msg) - 1, format2, args);
  va_end(args);

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(lvl)
      /*
        We're not setting LOG_ITEM_SRC_LINE and LOG_ITEM_SRC_FILE
        here as we'd be interested in the location in the plugin,
        not in the plugin interface, so rather than give confusing
        or useless information, we give none. Plugins using the
        richer (service) interface can use that to add such
        information.
      */
      .component(plugin->name.str)
      .verbatim(msg);

  return 0;
}
