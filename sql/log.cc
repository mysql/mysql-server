/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "log.h"
#include "sql_base.h"                           // open_log_table
#include "sql_delete.h"                         // mysql_truncate
#include "sql_parse.h"                          // command_name
#include "sql_time.h"           // calc_time_from_sec, my_time_compare
#include "tztime.h"             // my_tz_OFFSET0, struct Time_zone
#include "auth_common.h"        // SUPER_ACL
#include "sql_audit.h"
#include "mysql/service_my_plugin_log.h"

#include <my_dir.h>
#include <stdarg.h>

#ifdef _WIN32
#include "message.h"
#endif

using std::min;
using std::max;

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

  virtual ~Silence_log_table_errors() {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sql_state,
                                Sql_condition::enum_severity_level *level,
                                const char* msg,
                                Sql_condition ** cond_hdl)
  {
    *cond_hdl= NULL;
    strmake(m_message, msg, sizeof(m_message)-1);
    return true;
  }

  const char *message() const { return m_message; }
};


static void ull2timeval(ulonglong utime, struct timeval *tv)
{
  DBUG_ASSERT(tv != NULL);
  DBUG_ASSERT(utime > 0);      /* should hold true in this context */
  tv->tv_sec= utime / 1000000;
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

int make_iso8601_timestamp(char *buf, ulonglong utime= 0)
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


bool File_query_log::open()
{
  File file= -1;
  my_off_t pos= 0;
  const char *log_name= NULL;
  char buff[FN_REFLEN];
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

  db[0]= 0;

  if ((file= mysql_file_open(m_log_file_key,
                             log_file_name,
                             O_CREAT | O_BINARY | O_WRONLY | O_APPEND,
                             MYF(MY_WME | ME_WAITTANG))) < 0)
    goto err;

  if ((pos= mysql_file_tell(file, MYF(MY_WME))) == MY_FILEPOS_ERROR)
  {
    if (my_errno == ESPIPE)
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

  length= my_snprintf(buff, 32, "%5lu ", thread_id);

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

    buff_len= my_snprintf(buff, 32, "%5lu", thd->thread_id);
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
  if (thd->db && strcmp(thd->db, db))
  {						// Database changed
    if (my_b_printf(&log_file,"use %s;\n",thd->db) == (uint) -1)
      goto err;
    my_stpcpy(db,thd->db);
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
  end= int10_to_str((long) current_utime / 1000000, end, 10);

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
    calc_time_from_sec(&t, min<long>((longlong) (query_utime / 1000000),
                                     (longlong) TIME_MAX_VALUE_SECONDS),
                       query_utime % 1000000);
    if (table->field[SQLT_FIELD_QUERY_TIME]->store_time(&t))
      goto err;
    /* lock_time */
    calc_time_from_sec(&t, min<long>((longlong) (lock_utime / 1000000),
                                     (longlong) TIME_MAX_VALUE_SECONDS),
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
  if (thd->db)
  {
    if (table->field[SQLT_FIELD_DATABASE]->store(thd->db, thd->db_length,
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

  if (table->field[SQLT_FIELD_THREAD_ID]->store((longlong) thd->thread_id,
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
  Security_context *sctx= thd->security_ctx;
  uint user_host_len= (strxnmov(user_host_buff, MAX_USER_HOST_SIZE,
                                sctx->priv_user ? sctx->priv_user : "", "[",
                                sctx->user ? sctx->user : "", "] @ ",
                                sctx->get_host()->length() ?
                                sctx->get_host()->ptr() : "", " [",
                                sctx->get_ip()->length() ? sctx->get_ip()->ptr() :
                                "", "]", NullS) - user_host_buff);
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
         && (thd->security_ctx->master_access & SUPER_ACL)
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
  // Is general log enabled? Any active handlers?
  if (!opt_general_log || !(*general_log_handler_list))
    return false;

  // Do we want to log this kind of command?
  if (!log_command(thd, command))
    return false;

  mysql_rwlock_rdlock(&LOCK_logger);

  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  size_t user_host_len= make_user_name(thd, user_host_buff);
  ulonglong current_utime= thd->current_utime();

  mysql_audit_general_log(thd, current_utime / 1000000,
                          user_host_buff, user_host_len,
                          command_name[(uint) command].str,
                          command_name[(uint) command].length,
                          query, query_length);

  bool error= false;
  for (Log_event_handler **current_handler= general_log_handler_list;
       *current_handler; )
  {
    error|= (*current_handler++)->log_general(thd, current_utime, user_host_buff,
                                              user_host_len, thd->thread_id,
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
  // Is general log enabled? Any active handlers?
  if (!opt_general_log || !(*general_log_handler_list))
    return false;

  // Do we want to log this kind of command?
  if (!log_command(thd, command))
    return false;

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
                                  uint log_printer)
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


void Query_logger::set_handlers(uint log_printer)
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
{
  aggregate_sctx.init();
}


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
  Security_context *save_sctx=     thd->security_ctx;

  char buf[128];

  my_snprintf(buf, sizeof(buf), summary_template, suppressed);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->start_utime=                thd->current_utime() - print_exec_time;
  thd->utime_after_lock=           thd->start_utime + print_lock_time;
  thd->security_ctx=               (Security_context *) &aggregate_sctx;
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  (*log_summary)(thd, buf, strlen(buf));

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->security_ctx    = save_sctx;
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


bool Error_log_throttle::log(THD *thd)
{
  ulonglong end_utime_of_query= thd->current_utime();

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


bool Error_log_throttle::flush(THD *thd)
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


bool reopen_fstreams(const char *filename,
                     FILE *outstream, FILE *errstream)
{
  int retries= 2, errors= 0;

  do
  {
    errors= 0;
    if (errstream && !my_freopen(filename, "a", errstream))
      errors++;
    if (outstream && !my_freopen(filename, "a", outstream))
      errors++;
  }
  while(retries-- && errors);

  if(errors)
    return true;

  /* The error stream must be unbuffered. */
  if (errstream)
    setbuf(errstream, NULL);

  return false;
}


bool flush_error_log()
{
  bool result= false;
  if (opt_error_log)
  {
    mysql_mutex_lock(&LOCK_error_log);
    result= reopen_fstreams(log_error_file, stdout, stderr);
    mysql_mutex_unlock(&LOCK_error_log);
  }
  return result;
}


#ifdef _WIN32
static int eventSource = 0;

static void setup_windows_event_source()
{
  HKEY    hRegKey= NULL;
  DWORD   dwError= 0;
  TCHAR   szPath[MAX_PATH];
  DWORD dwTypes;

  if (eventSource)               // Ensure that we are only called once
    return;
  eventSource= 1;

  // Create the event source registry key
  dwError= RegCreateKey(HKEY_LOCAL_MACHINE,
                          "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\MySQL", 
                          &hRegKey);

  /* Name of the PE module that contains the message resource */
  GetModuleFileName(NULL, szPath, MAX_PATH);

  /* Register EventMessageFile */
  dwError = RegSetValueEx(hRegKey, "EventMessageFile", 0, REG_EXPAND_SZ,
                          (PBYTE) szPath, (DWORD) (strlen(szPath) + 1));

  /* Register supported event types */
  dwTypes= (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
            EVENTLOG_INFORMATION_TYPE);
  dwError= RegSetValueEx(hRegKey, "TypesSupported", 0, REG_DWORD,
                         (LPBYTE) &dwTypes, sizeof dwTypes);

  RegCloseKey(hRegKey);
}

#endif /* _WIN32 */

/**
  Find a unique filename for 'filename.#'.

  Set '#' to the number next to the maximum found in the most
  recent log file extension.

  This function will return nonzero if: (i) the generated name
  exceeds FN_REFLEN; (ii) if the number of extensions is exhausted;
  or (iii) some other error happened while examining the filesystem.

  @return
    nonzero if not possible to get unique filename.
*/

static int find_uniq_filename(char *name)
{
  uint                  i;
  char                  buff[FN_REFLEN], ext_buf[FN_REFLEN];
  struct st_my_dir     *dir_info;
  reg1 struct fileinfo *file_info;
  ulong                 max_found= 0, next= 0, number= 0;
  size_t		buf_length, length;
  char			*start, *end;
  int                   error= 0;
  DBUG_ENTER("find_uniq_filename");

  length= dirname_part(buff, name, &buf_length);
  start=  name + length;
  end=    strend(start);

  *end='.';
  length= (size_t) (end - start + 1);

  if ((DBUG_EVALUATE_IF("error_unique_log_filename", 1, 
      !(dir_info= my_dir(buff,MYF(MY_DONT_SORT))))))
  {						// This shouldn't happen
    strmov(end,".1");				// use name+1
    DBUG_RETURN(1);
  }
  file_info= dir_info->dir_entry;
  for (i= dir_info->number_off_files ; i-- ; file_info++)
  {
    if (memcmp(file_info->name, start, length) == 0 &&
	test_if_number(file_info->name+length, &number,0))
    {
      set_if_bigger(max_found,(ulong) number);
    }
  }
  my_dirend(dir_info);

  /* check if reached the maximum possible extension number */
  if (max_found == MAX_LOG_UNIQUE_FN_EXT)
  {
    sql_print_error("Log filename extension number exhausted: %06lu. \
Please fix this by archiving old logs and \
updating the index files.", max_found);
    error= 1;
    goto end;
  }

  next= max_found + 1;
  if (sprintf(ext_buf, "%06lu", next)<0)
  {
    error= 1;
    goto end;
  }
  *end++='.';

  /* 
    Check if the generated extension size + the file name exceeds the
    buffer size used. If one did not check this, then the filename might be
    truncated, resulting in error.
   */
  if (((strlen(ext_buf) + (end - name)) >= FN_REFLEN))
  {
    sql_print_error("Log filename too large: %s%s (%zu). \
Please fix this by archiving old logs and updating the \
index files.", name, ext_buf, (strlen(ext_buf) + (end - name)));
    error= 1;
    goto end;
  }

  if (sprintf(end, "%06lu", next)<0)
  {
    error= 1;
    goto end;
  }

  /* print warning if reaching the end of available extensions. */
  if ((next > (MAX_LOG_UNIQUE_FN_EXT - LOG_WARN_UNIQUE_FN_EXT_LEFT)))
    sql_print_warning("Next log extension: %lu. \
Remaining log filename extensions: %lu. \
Please consider archiving some logs.", next, (MAX_LOG_UNIQUE_FN_EXT - next));

end:
  DBUG_RETURN(error);
}


void MYSQL_LOG::init(enum_log_type log_type_arg,
                     enum cache_type io_cache_type_arg)
{
  DBUG_ENTER("MYSQL_LOG::init");
  log_type= log_type_arg;
  io_cache_type= io_cache_type_arg;
  DBUG_PRINT("info",("log_type: %d", log_type));
  DBUG_VOID_RETURN;
}


bool MYSQL_LOG::init_and_set_log_file_name(const char *log_name,
                                           const char *new_name,
                                           enum_log_type log_type_arg,
                                           enum cache_type io_cache_type_arg)
{
  init(log_type_arg, io_cache_type_arg);

  if (new_name && !strmov(log_file_name, new_name))
    return TRUE;
  else if (!new_name && generate_new_name(log_file_name, log_name))
    return TRUE;

  return FALSE;
}


/*
  Open a (new) log file.

  SYNOPSIS
    open()

    log_name            The name of the log to open
    log_type_arg        The type of the log. E.g. LOG_NORMAL
    new_name            The new name for the logfile. This is only needed
                        when the method is used to open the binlog file.
    io_cache_type_arg   The type of the IO_CACHE to use for this log file

  DESCRIPTION
    Open the logfile, init IO_CACHE and write startup messages
    (in case of general and slow query logs).

  RETURN VALUES
    0   ok
    1   error
*/

bool MYSQL_LOG::open(
#ifdef HAVE_PSI_INTERFACE
                     PSI_file_key log_file_key,
#endif
                     const char *log_name, enum_log_type log_type_arg,
                     const char *new_name, enum cache_type io_cache_type_arg)
{
  char buff[FN_REFLEN];
  File file= -1;
  my_off_t pos= 0;
  int open_flags= O_CREAT | O_BINARY;
  DBUG_ENTER("MYSQL_LOG::open");
  DBUG_PRINT("enter", ("log_type: %d", (int) log_type_arg));

  write_error= 0;

  if (!(name= my_strdup(log_name, MYF(MY_WME))))
  {
    name= (char *)log_name; // for the error message
    goto err;
  }

  if (init_and_set_log_file_name(name, new_name,
                                 log_type_arg, io_cache_type_arg) ||
      DBUG_EVALUATE_IF("fault_injection_init_name", log_type == LOG_BIN, 0))
    goto err;

  if (io_cache_type == SEQ_READ_APPEND)
    open_flags |= O_RDWR | O_APPEND;
  else
    open_flags |= O_WRONLY | (log_type == LOG_BIN ? 0 : O_APPEND);

  db[0]= 0;

#ifdef HAVE_PSI_INTERFACE
  /* Keep the key for reopen */
  m_log_file_key= log_file_key;
#endif

  if ((file= mysql_file_open(log_file_key,
                             log_file_name, open_flags,
                             MYF(MY_WME | ME_WAITTANG))) < 0)
    goto err;

  if ((pos= mysql_file_tell(file, MYF(MY_WME))) == MY_FILEPOS_ERROR)
  {
    if (my_errno == ESPIPE)
      pos= 0;
    else
      goto err;
  }

  if (init_io_cache(&log_file, file, IO_SIZE, io_cache_type, pos, 0,
                    MYF(MY_WME | MY_NABP |
                        ((log_type == LOG_BIN) ? MY_WAIT_IF_FULL : 0))))
    goto err;

  if (log_type == LOG_NORMAL)
  {
    char *end;
    int len=my_snprintf(buff, sizeof(buff), "%s, Version: %s (%s). "
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
    end= strnmov(buff + len, "Time                 Id Command    Argument\n",
                 sizeof(buff) - len);
    if (my_b_write(&log_file, (uchar*) buff, (uint) (end-buff)) ||
	flush_io_cache(&log_file))
      goto err;
  }

  log_state= LOG_OPENED;
  DBUG_RETURN(0);

err:
  if (log_type == LOG_BIN && binlogging_impossible_mode == ABORT_SERVER)
  {
    THD *thd= current_thd;
    /*
      On fatal error when code enters here we should forcefully clear the
      previous errors so that a new critical error message can be pushed
      to the client side.
     */
    thd->clear_error();
    my_error(ER_BINLOG_LOGGING_IMPOSSIBLE, MYF(0), "Either disc is full or "
             "file system is read only while opening the binlog. Aborting the "
             "server");
    thd->protocol->end_statement();
    _exit(EXIT_FAILURE);
  }
  else
    sql_print_error("Could not open %s for logging (error %d). "
                    "Turning logging off for the whole duration "
                    "of the MySQL server process. To turn it on "
                    "again: fix the cause, shutdown the MySQL "
                    "server and restart it.",
                    name, errno);
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);
  my_free(name);
  name= NULL;
  log_state= LOG_CLOSED;
  DBUG_RETURN(1);
}

MYSQL_LOG::MYSQL_LOG()
  : name(0), write_error(FALSE), inited(FALSE), log_type(LOG_UNKNOWN),
    log_state(LOG_CLOSED)
#ifdef HAVE_PSI_INTERFACE
  , m_key_LOCK_log(key_LOG_LOCK_log)
#endif
{
  /*
    We don't want to initialize LOCK_Log here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  memset(&log_file, 0, sizeof(log_file));
}

void MYSQL_LOG::init_pthread_objects()
{
  DBUG_ASSERT(inited == 0);
  inited= 1;
  mysql_mutex_init(m_key_LOCK_log, &LOCK_log, MY_MUTEX_INIT_SLOW);
}

/*
  Close the log file

  SYNOPSIS
    close()
    exiting     Bitmask. For the slow and general logs the only used bit is
                LOG_CLOSE_TO_BE_OPENED. This is used if we intend to call
                open at once after close.

  NOTES
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_LOG::close(uint exiting)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (log_state == LOG_OPENED)
  {
    end_io_cache(&log_file);

    if (mysql_file_sync(log_file.file, MYF(MY_WME)) && ! write_error)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      write_error= 1;
      sql_print_error(ER_DEFAULT(ER_ERROR_ON_WRITE), name, errno,
                      my_strerror(errbuf, sizeof(errbuf), errno));
    }

    if (mysql_file_close(log_file.file, MYF(MY_WME)) && ! write_error)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      write_error= 1;
      sql_print_error(ER_DEFAULT(ER_ERROR_ON_WRITE), name, errno,
                      my_strerror(errbuf, sizeof(errbuf), errno));
    }
  }

  log_state= (exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED;
  my_free(name);
  name= NULL;
  DBUG_VOID_RETURN;
}

/** This is called only once. */

void MYSQL_LOG::cleanup()
{
  DBUG_ENTER("cleanup");
  if (inited)
  {
    inited= 0;
    mysql_mutex_destroy(&LOCK_log);
    close(0);
  }
  DBUG_VOID_RETURN;
}


int MYSQL_LOG::generate_new_name(char *new_name, const char *log_name)
{
  fn_format(new_name, log_name, mysql_data_home, "", 4);
  if (log_type == LOG_BIN)
  {
    if (!fn_ext(log_name)[0])
    {
      if (find_uniq_filename(new_name))
      {
        my_printf_error(ER_NO_UNIQUE_LOGFILE, ER(ER_NO_UNIQUE_LOGFILE),
                        MYF(ME_FATALERROR), log_name);
	sql_print_error(ER(ER_NO_UNIQUE_LOGFILE), log_name);
	return 1;
      }
    }
  }
  return 0;
}


/*
  Reopen the log file

  SYNOPSIS
    reopen_file()

  DESCRIPTION
    Reopen the log file. The method is used during FLUSH LOGS
    and locks LOCK_log mutex
*/


void MYSQL_QUERY_LOG::reopen_file()
{
  char *save_name;

  DBUG_ENTER("MYSQL_LOG::reopen_file");
  if (!is_open())
  {
    DBUG_PRINT("info",("log is closed"));
    DBUG_VOID_RETURN;
  }

  mysql_mutex_lock(&LOCK_log);

  save_name= name;
  name= 0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED);

  /*
     Note that at this point, log_state != LOG_CLOSED (important for is_open()).
  */

  open(
#ifdef HAVE_PSI_INTERFACE
       m_log_file_key,
#endif
       save_name, log_type, 0, io_cache_type);
  my_free(save_name);

  mysql_mutex_unlock(&LOCK_log);

  DBUG_VOID_RETURN;
}


/*
  Write a command to traditional general log file

  SYNOPSIS
    write()

    event_time        command start timestamp
    user_host         the pointer to the string with user@host info
    user_host_len     length of the user_host string. this is computed once
                      and passed to all general log  event handlers
    thread_id         Id of the thread, issued a query
    command_type      the type of the command being logged
    command_type_len  the length of the string above
    sql_text          the very text of the query being executed
    sql_text_len      the length of sql_text string

  DESCRIPTION

   Log given command to to normal (not rotable) log file

  RETURN
    FASE - OK
    TRUE - error occured
*/

bool MYSQL_QUERY_LOG::write(time_t event_time, const char *user_host,
                            uint user_host_len, my_thread_id thread_id,
                            const char *command_type, uint command_type_len,
                            const char *sql_text, uint sql_text_len)
{
  char buff[32];
  uint length= 0;
  char local_time_buff[MAX_TIME_SIZE];
  struct tm start;
  uint time_buff_len= 0;

  mysql_mutex_lock(&LOCK_log);

  /* Test if someone closed between the is_open test and lock */
  if (is_open())
  {
    /* for testing output of timestamp and thread id */
    DBUG_EXECUTE_IF("reset_log_last_time", last_time= 0;);

    /* Note that my_b_write() assumes it knows the length for this */
      if (event_time != last_time)
      {
        last_time= event_time;

        localtime_r(&event_time, &start);

        time_buff_len= my_snprintf(local_time_buff, MAX_TIME_SIZE,
                                   "%02d%02d%02d %2d:%02d:%02d\t",
                                   start.tm_year % 100, start.tm_mon + 1,
                                   start.tm_mday, start.tm_hour,
                                   start.tm_min, start.tm_sec);

        if (my_b_write(&log_file, (uchar*) local_time_buff, time_buff_len))
          goto err;
      }
      else
        if (my_b_write(&log_file, (uchar*) "\t\t" ,2) < 0)
          goto err;

    length= my_snprintf(buff, 32, "%5lu ", thread_id);

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
  }

  mysql_mutex_unlock(&LOCK_log);
  return FALSE;
err:

  if (!write_error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    write_error= 1;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno,
                    my_strerror(errbuf, sizeof(errbuf), errno));
  }
  mysql_mutex_unlock(&LOCK_log);
  return TRUE;
}


/*
  Log a query to the traditional slow log file

  SYNOPSIS
    write()

    thd               THD of the query
    current_time      current timestamp
    query_start_arg   command start timestamp
    user_host         the pointer to the string with user@host info
    user_host_len     length of the user_host string. this is computed once
                      and passed to all general log event handlers
    query_utime       Amount of time the query took to execute (in microseconds)
    lock_utime        Amount of time the query was locked (in microseconds)
    is_command        The flag, which determines, whether the sql_text is a
                      query or an administrator command.
    sql_text          the very text of the query or administrator command
                      processed
    sql_text_len      the length of sql_text string

  DESCRIPTION

   Log a query to the slow log file.

  RETURN
    FALSE - OK
    TRUE - error occured
*/

bool MYSQL_QUERY_LOG::write(THD *thd, time_t current_time,
                            time_t query_start_arg, const char *user_host,
                            uint user_host_len, ulonglong query_utime,
                            ulonglong lock_utime, bool is_command,
                            const char *sql_text, uint sql_text_len)
{
  bool error= 0;
  DBUG_ENTER("MYSQL_QUERY_LOG::write");

  mysql_mutex_lock(&LOCK_log);

  if (!is_open())
  {
    mysql_mutex_unlock(&LOCK_log);
    DBUG_RETURN(0);
  }

  if (is_open())
  {						// Safety agains reopen
    int tmp_errno= 0;
    char buff[80], *end;
    char query_time_buff[22+7], lock_time_buff[22+7];
    uint buff_len;
    end= buff;

    if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT))
    {
      if (current_time != last_time)
      {
        last_time= current_time;
        struct tm start;
        localtime_r(&current_time, &start);

        buff_len= my_snprintf(buff, sizeof buff,
                              "# Time: %02d%02d%02d %2d:%02d:%02d\n",
                              start.tm_year % 100, start.tm_mon + 1,
                              start.tm_mday, start.tm_hour,
                              start.tm_min, start.tm_sec);

        /* Note that my_b_write() assumes it knows the length for this */
        if (my_b_write(&log_file, (uchar*) buff, buff_len))
          tmp_errno= errno;
      }
      buff_len= my_snprintf(buff, 32, "%5lu", thd->thread_id);
      if (my_b_printf(&log_file, "# User@Host: %s  Id: %s\n", user_host, buff)
          == (uint) -1)
        tmp_errno= errno;
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
      tmp_errno= errno;
    if (thd->db && strcmp(thd->db, db))
    {						// Database changed
      if (my_b_printf(&log_file,"use %s;\n",thd->db) == (uint) -1)
        tmp_errno= errno;
      strmov(db,thd->db);
    }
    if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
    {
      end=strmov(end, ",last_insert_id=");
      end=longlong10_to_str((longlong)
                            thd->first_successful_insert_id_in_prev_stmt_for_binlog,
                            end, -10);
    }
    // Save value if we do an insert.
    if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0)
    {
      if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT))
      {
        end=strmov(end,",insert_id=");
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
    end= strmov(end, ",timestamp=");
    end= int10_to_str((long) current_time, end, 10);

    if (end != buff)
    {
      *end++=';';
      *end='\n';
      if (my_b_write(&log_file, (uchar*) "SET ", 4) ||
          my_b_write(&log_file, (uchar*) buff + 1, (uint) (end-buff)))
        tmp_errno= errno;
    }
    if (is_command)
    {
      end= strxmov(buff, "# administrator command: ", NullS);
      buff_len= (ulong) (end - buff);
      DBUG_EXECUTE_IF("simulate_slow_log_write_error",
                      {DBUG_SET("+d,simulate_file_write_error");});
      if(my_b_write(&log_file, (uchar*) buff, buff_len))
        tmp_errno= errno;
    }
    if (my_b_write(&log_file, (uchar*) sql_text, sql_text_len) ||
        my_b_write(&log_file, (uchar*) ";\n",2) ||
        flush_io_cache(&log_file))
      tmp_errno= errno;
    if (tmp_errno)
    {
      error= 1;
      if (! write_error)
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        write_error= 1;
        sql_print_error(ER(ER_ERROR_ON_WRITE), name, error,
                        my_strerror(errbuf, sizeof(errbuf), errno));
      }
    }
  }
  mysql_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


/**
  @todo
  The following should be using fn_format();  We just need to
  first change fn_format() to cut the file name if it's too long.
*/
const char *MYSQL_LOG::generate_name(const char *log_name,
                                      const char *suffix,
                                      bool strip_ext, char *buff)
{
  if (!log_name || !log_name[0])
  {
    strmake(buff, pidfile_name, FN_REFLEN - strlen(suffix) - 1);
    return (const char *)
      fn_format(buff, buff, "", suffix, MYF(MY_REPLACE_EXT|MY_REPLACE_DIR));
  }
  // get rid of extension if the log is binary to avoid problems
  if (strip_ext)
  {
    char *p= fn_ext(log_name);
    uint length= (uint) (p - log_name);
    strmake(buff, log_name, min<size_t>(length, FN_REFLEN-1));
    return (const char*)buff;
  }
  return log_name;
}


int error_log_print(enum loglevel level, const char *format,
                    va_list args)
{
  return logger.error_log_print(level, format, args);
}


bool slow_log_print(THD *thd, const char *query, uint query_length)
{
  return logger.slow_log_print(thd, query, query_length);
}


bool LOGGER::log_command(THD *thd, enum enum_server_command command)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *sctx= thd->security_ctx;
#endif
  /*
    Log command if we have at least one log event handler enabled and want
    to log this king of commands
  */
  if (*general_log_handler_list && (what_to_log & (1L << (uint) command)))
  {
    if ((thd->variables.option_bits & OPTION_LOG_OFF)
#ifndef NO_EMBEDDED_ACCESS_CHECKS
         && (sctx->master_access & SUPER_ACL)
#endif
       )
    {
      /* No logging */
      return FALSE;
    }

    return TRUE;
  }

  return FALSE;
}


bool general_log_print(THD *thd, enum enum_server_command command,
                       const char *format, ...)
{
  va_list args;
  uint error= 0;

  /* Print the message to the buffer if we want to log this king of commands */
  if (! logger.log_command(thd, command))
    return FALSE;

  va_start(args, format);
  error= logger.general_log_print(thd, command, format, args);
  va_end(args);

  return error;
}

bool general_log_write(THD *thd, enum enum_server_command command,
                       const char *query, uint query_length)
{
  /* Write the message to the log if we want to log this king of commands */
  if (logger.log_command(thd, command))
    return logger.general_log_write(thd, command, query, query_length);

  return FALSE;
}

/**
  Check if a string is a valid number.

  @param str			String to test
  @param res			Store value here
  @param allow_wildcards	Set to 1 if we should ignore '%' and '_'

  @note
    For the moment the allow_wildcards argument is not used
    Should be move to some other file.

  @retval
    1	String is a number
  @retval
    0	String is not a number
*/

static bool test_if_number(register const char *str,
			   ulong *res, bool allow_wildcards)
{
  reg2 int flag;
  const char *start;
  DBUG_ENTER("test_if_number");

  flag=0; start=str;
  while (*str++ == ' ') ;
  if (*--str == '-' || *str == '+')
    str++;
  while (my_isdigit(files_charset_info,*str) ||
	 (allow_wildcards && (*str == wild_many || *str == wild_one)))
  {
    flag=1;
    str++;
  }
  if (*str == '.')
  {
    for (str++ ;
	 my_isdigit(files_charset_info,*str) ||
	   (allow_wildcards && (*str == wild_many || *str == wild_one)) ;
	 str++, flag=1) ;
  }
  if (*str != 0 || flag == 0)
    DBUG_RETURN(0);
  if (res)
    *res=atol(start);
  DBUG_RETURN(1);			/* Number ok */
} /* test_if_number */


void sql_perror(const char *message)
{
#ifdef HAVE_STRERROR
  sql_print_error("%s: %s",message, strerror(errno));
#else
  perror(message);
#endif
}


/*
  Change the file associated with two output streams. Used to
  redirect stdout and stderr to a file. The streams are reopened
  only for appending (writing at end of file).
*/
extern "C" my_bool reopen_fstreams(const char *filename,
                                   FILE *outstream, FILE *errstream)
{
  int retries= 2, errors= 0;

  do
  {
    errors= 0;
    if (errstream && !my_freopen(filename, "a", errstream))
      errors++;
    if (outstream && !my_freopen(filename, "a", outstream))
      errors++;
  }
  while(retries-- && errors);

  if(errors)
    return true;

  /* The error stream must be unbuffered. */
  if (errstream)
    setbuf(errstream, NULL);

  return FALSE;
}


/*
  Unfortunately, there seems to be no good way
  to restore the original streams upon failure.
*/
static bool redirect_std_streams(const char *file)
{
  if (reopen_fstreams(file, stdout, stderr))
    return TRUE;

  setbuf(stderr, NULL);
  return FALSE;
}


bool flush_error_log()
{
  bool result= 0;
  if (opt_error_log)
  {
    mysql_mutex_lock(&LOCK_error_log);
    if (redirect_std_streams(log_error_file))
      result= 1;
    mysql_mutex_unlock(&LOCK_error_log);
  }
  return result;
}

#ifdef _WIN32
static void print_buffer_to_nt_eventlog(enum loglevel level, char *buff,
                                        size_t length, size_t buffLen)
{
  HANDLE event;
  char   *buffptr= buff;
  DBUG_ENTER("print_buffer_to_nt_eventlog");

  /* Add ending CR/LF's to string, overwrite last chars if necessary */
  my_stpcpy(buffptr+min(length, buffLen-5), "\r\n\r\n");

  setup_windows_event_source();
  if ((event= RegisterEventSource(NULL,"MySQL")))
  {
    switch (level) {
      case ERROR_LEVEL:
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, MSG_DEFAULT, NULL, 1, 0,
                    (LPCSTR*)&buffptr, NULL);
        break;
      case WARNING_LEVEL:
        ReportEvent(event, EVENTLOG_WARNING_TYPE, 0, MSG_DEFAULT, NULL, 1, 0,
                    (LPCSTR*) &buffptr, NULL);
        break;
      case INFORMATION_LEVEL:
        ReportEvent(event, EVENTLOG_INFORMATION_TYPE, 0, MSG_DEFAULT, NULL, 1,
                    0, (LPCSTR*) &buffptr, NULL);
        break;
    }
    DeregisterEventSource(event);
  }

  DBUG_VOID_RETURN;
}
#endif /* _WIN32 */


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
    thread_id= current_thd->thread_id;

  make_iso8601_timestamp(my_timestamp);

  mysql_mutex_lock(&LOCK_error_log);

  fprintf(stderr, "%s %lu [%s] %.*s\n",
          my_timestamp,
          thread_id,
          (level == ERROR_LEVEL ? "ERROR" : level == WARNING_LEVEL ?
           "Warning" : "Note"),
          (int) length, buffer);

  fflush(stderr);

  mysql_mutex_unlock(&LOCK_error_log);
  DBUG_VOID_RETURN;
}


void error_log_print(enum loglevel level, const char *format, va_list args)
{
  char   buff[1024];
  size_t length;
  DBUG_ENTER("error_log_print");

  if (static_cast<ulong>(level) < log_error_verbosity)
  {
    length= my_vsnprintf(buff, sizeof(buff), format, args);
    print_buffer_to_file(level, buff, length);

#ifdef _WIN32
    print_buffer_to_nt_eventlog(level, buff, length, sizeof(buff));
#endif
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
  char format2[MYSQL_ERRMSG_SIZE];
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


/********* transaction coordinator log for 2pc - mmap() based solution *******/

/*
  the log consists of a file, mmapped to a memory.
  file is divided on pages of tc_log_page_size size.
  (usable size of the first page is smaller because of log header)
  there's PAGE control structure for each page
  each page (or rather PAGE control structure) can be in one of three
  states - active, syncing, pool.
  there could be only one page in active or syncing states,
  but many in pool - pool is fifo queue.
  usual lifecycle of a page is pool->active->syncing->pool
  "active" page - is a page where new xid's are logged.
  the page stays active as long as syncing slot is taken.
  "syncing" page is being synced to disk. no new xid can be added to it.
  when the sync is done the page is moved to a pool and an active page
  becomes "syncing".

  the result of such an architecture is a natural "commit grouping" -
  If commits are coming faster than the system can sync, they do not
  stall. Instead, all commit that came since the last sync are
  logged to the same page, and they all are synced with the next -
  one - sync. Thus, thought individual commits are delayed, throughput
  is not decreasing.

  when a xid is added to an active page, the thread of this xid waits
  for a page's condition until the page is synced. when syncing slot
  becomes vacant one of these waiters is awaken to take care of syncing.
  it syncs the page and signals all waiters that the page is synced.
  PAGE::waiters is used to count these waiters, and a page may never
  become active again until waiters==0 (that is all waiters from the
  previous sync have noticed the sync was completed)

  note, that the page becomes "dirty" and has to be synced only when a
  new xid is added into it. Removing a xid from a page does not make it
  dirty - we don't sync removals to disk.
*/

ulong tc_log_page_waits= 0;

#ifdef HAVE_MMAP

#define TC_LOG_HEADER_SIZE (sizeof(tc_log_magic)+1)

static const char tc_log_magic[]={(char) 254, 0x23, 0x05, 0x74};

ulong opt_tc_log_size= TC_LOG_MIN_SIZE;
ulong tc_log_max_pages_used=0, tc_log_page_size=0, tc_log_cur_pages_used=0;

int TC_LOG_MMAP::open(const char *opt_name)
{
  uint i;
  bool crashed=FALSE;
  PAGE *pg;

  DBUG_ASSERT(total_ha_2pc > 1);
  DBUG_ASSERT(opt_name && opt_name[0]);

  tc_log_page_size= my_getpagesize();
  if (TC_LOG_PAGE_SIZE > tc_log_page_size)
  {
    DBUG_ASSERT(TC_LOG_PAGE_SIZE % tc_log_page_size == 0);
  }

  fn_format(logname,opt_name,mysql_data_home,"",MY_UNPACK_FILENAME);
  if ((fd= mysql_file_open(key_file_tclog, logname, O_RDWR, MYF(0))) < 0)
  {
    if (my_errno != ENOENT)
      goto err;
    if (using_heuristic_recover())
      return 1;
    if ((fd= mysql_file_create(key_file_tclog, logname, CREATE_MODE,
                               O_RDWR, MYF(MY_WME))) < 0)
      goto err;
    inited=1;
    file_length= opt_tc_log_size;
    if (mysql_file_chsize(fd, file_length, 0, MYF(MY_WME)))
      goto err;
  }
  else
  {
    inited= 1;
    crashed= TRUE;
    sql_print_information("Recovering after a crash using %s", opt_name);
    if (tc_heuristic_recover)
    {
      sql_print_error("Cannot perform automatic crash recovery when "
                      "--tc-heuristic-recover is used");
      goto err;
    }
    file_length= mysql_file_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME+MY_FAE));
    if (file_length == MY_FILEPOS_ERROR || file_length % tc_log_page_size)
      goto err;
  }

  data= (uchar *)my_mmap(0, (size_t)file_length, PROT_READ|PROT_WRITE,
                        MAP_NOSYNC|MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
  {
    my_errno=errno;
    goto err;
  }
  inited=2;

  npages=(uint)file_length/tc_log_page_size;
  DBUG_ASSERT(npages >= 3);             // to guarantee non-empty pool
  if (!(pages=(PAGE *)my_malloc(key_memory_TC_LOG_MMAP_pages,
                                npages*sizeof(PAGE), MYF(MY_WME|MY_ZEROFILL))))
    goto err;
  inited=3;
  for (pg=pages, i=0; i < npages; i++, pg++)
  {
    pg->next=pg+1;
    pg->waiters=0;
    pg->state=PS_POOL;
    mysql_cond_init(key_PAGE_cond, &pg->cond, 0);
    pg->size=pg->free=tc_log_page_size/sizeof(my_xid);
    pg->start= (my_xid *)(data + i*tc_log_page_size);
    pg->end= pg->start + pg->size;
    pg->ptr= pg->start;
  }
  pages[0].size=pages[0].free=
                (tc_log_page_size-TC_LOG_HEADER_SIZE)/sizeof(my_xid);
  pages[0].start=pages[0].end-pages[0].size;
  pages[npages-1].next=0;
  inited=4;

  if (crashed && recover())
      goto err;

  memcpy(data, tc_log_magic, sizeof(tc_log_magic));
  data[sizeof(tc_log_magic)]= (uchar)total_ha_2pc;
  my_msync(fd, data, tc_log_page_size, MS_SYNC);
  inited=5;

  mysql_mutex_init(key_LOCK_tc, &LOCK_tc, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_active, &COND_active, 0);
  mysql_cond_init(key_COND_pool, &COND_pool, 0);

  inited=6;

  syncing= 0;
  active=pages;
  pool=pages+1;
  pool_last_ptr= &pages[npages-1].next;

  return 0;

err:
  close();
  return 1;
}


/**
  Get the total amount of potentially usable slots for XIDs in TC log.
*/

uint TC_LOG_MMAP::size() const
{
  return (tc_log_page_size-TC_LOG_HEADER_SIZE)/sizeof(my_xid) +
         (npages - 1) * (tc_log_page_size/sizeof(my_xid));
}


/**
  there is no active page, let's got one from the pool.

  Two strategies here:
    -# take the first from the pool
    -# if there're waiters - take the one with the most free space.

  @todo
    TODO page merging. try to allocate adjacent page first,
    so that they can be flushed both in one sync

  @returns Pointer to qualifying page or NULL if no page in the
           pool can be made active.
*/

TC_LOG_MMAP::PAGE* TC_LOG_MMAP::get_active_from_pool()
{
  PAGE **best_p= &pool;

  if ((*best_p)->waiters != 0 || (*best_p)->free == 0)
  {
    /* if the first page can't be used try second strategy */
    int best_free=0;
    PAGE **p= &pool;
    for (p=&(*p)->next; *p; p=&(*p)->next)
    {
      if ((*p)->waiters == 0 && (*p)->free > best_free)
      {
        best_free=(*p)->free;
        best_p=p;
      }
    }
    if (*best_p == NULL || best_free == 0)
      return NULL;
  }

  PAGE *new_active= *best_p;
  if (new_active->free == new_active->size) // we've chosen an empty page
  {
    tc_log_cur_pages_used++;
    set_if_bigger(tc_log_max_pages_used, tc_log_cur_pages_used);
  }

  *best_p= (*best_p)->next;
  if (! *best_p)
    pool_last_ptr= best_p;

  return new_active;
}

/**
  @todo
  perhaps, increase log size ?
*/
void TC_LOG_MMAP::overflow()
{
  /*
    simple overflow handling - just wait
    TODO perhaps, increase log size ?
    let's check the behaviour of tc_log_page_waits first
  */
  ulong old_log_page_waits= tc_log_page_waits;

  mysql_cond_wait(&COND_pool, &LOCK_tc);

  if (old_log_page_waits == tc_log_page_waits)
  {
    /*
      When several threads are waiting in overflow() simultaneously
      we want to increase counter only once and not for each thread.
    */
    tc_log_page_waits++;
  }
}

/**
  Commit the transaction.

  @note When the TC_LOG inteface was changed, this function was added
  and uses the functions that were there with the old interface to
  implement the logic.
 */
TC_LOG::enum_result TC_LOG_MMAP::commit(THD *thd, bool all)
{
  DBUG_ENTER("TC_LOG_MMAP::commit");
  ulong cookie= 0;
  my_xid xid= thd->get_transaction()->xid_state()->get_xid()->get_my_xid();

  if (all && xid)
    if (!(cookie= log_xid(xid)))
      DBUG_RETURN(RESULT_ABORTED);    // Failed to log the transaction

  if (ha_commit_low(thd, all))
    DBUG_RETURN(RESULT_INCONSISTENT); // Transaction logged, but not committed

  /* If cookie is non-zero, something was logged */
  if (cookie)
    unlog(cookie, xid);

  DBUG_RETURN(RESULT_SUCCESS);
}


/**
  Record that transaction XID is committed on the persistent storage.

    This function is called in the middle of two-phase commit:
    First all resources prepare the transaction, then tc_log->log() is called,
    then all resources commit the transaction, then tc_log->unlog() is called.

    All access to active page is serialized but it's not a problem, as
    we're assuming that fsync() will be a main bottleneck.
    That is, parallelizing writes to log pages we'll decrease number of
    threads waiting for a page, but then all these threads will be waiting
    for a fsync() anyway

   If tc_log == MYSQL_BIN_LOG then tc_log writes transaction to binlog and
   records XID in a special Xid_log_event.
   If tc_log = TC_LOG_MMAP then xid is written in a special memory-mapped
   log.

  @retval
    0  - error
  @retval
    \# - otherwise, "cookie", a number that will be passed as an argument
    to unlog() call. tc_log can define it any way it wants,
    and use for whatever purposes. TC_LOG_MMAP sets it
    to the position in memory where xid was logged to.
*/

ulong TC_LOG_MMAP::log_xid(my_xid xid)
{
  mysql_mutex_lock(&LOCK_tc);

  while (true)
  {
    /* If active page is full - just wait... */
    while (unlikely(active && active->free == 0))
      mysql_cond_wait(&COND_active, &LOCK_tc);

    /* no active page ? take one from the pool. */
    if (active == NULL)
    {
      active= get_active_from_pool();

      /* There are no pages with free slots? Wait and retry. */
      if (active == NULL)
      {
        overflow();
        continue;
      }
    }

    break;
  }

  PAGE *p= active;
  ulong cookie= store_xid_in_empty_slot(xid, p, data);
  bool err;

  if (syncing)
  {                                          // somebody's syncing. let's wait
    err= wait_sync_completion(p);
    if (p->state != PS_DIRTY)                   // page was synced
    {
      if (p->waiters == 0)
        mysql_cond_broadcast(&COND_pool);    // in case somebody's waiting
      mysql_mutex_unlock(&LOCK_tc);
      goto done;                             // we're done
    }
  }                                          // page was not synced! do it now
  DBUG_ASSERT(active == p && syncing == NULL);
  syncing= p;                                 // place is vacant - take it
  active= NULL;                                  // page is not active anymore
  mysql_cond_broadcast(&COND_active);        // in case somebody's waiting
  mysql_mutex_unlock(&LOCK_tc);
  err= sync();

done:
  return err ? 0 : cookie;
}


/**
  Write the page data being synchronized to the disk.

  @return
    @retval false   Success
    @retval true    Failure
*/
bool TC_LOG_MMAP::sync()
{
  DBUG_ASSERT(syncing != active);

  /*
    sit down and relax - this can take a while...
    note - no locks are held at this point
  */

  int err= do_msync_and_fsync(fd, syncing->start,
                              syncing->size*sizeof(my_xid), MS_SYNC);

  mysql_mutex_lock(&LOCK_tc);
  /* Page is synced. Let's move it to the pool. */
  *pool_last_ptr= syncing;
  pool_last_ptr= &(syncing->next);
  syncing->next= NULL;
  syncing->state= err ? PS_ERROR : PS_POOL;
  mysql_cond_broadcast(&COND_pool);          // in case somebody's waiting

  /* Wake-up all threads which are waiting for syncing of the same page. */
  mysql_cond_broadcast(&syncing->cond);

  /* Mark syncing slot as free and wake-up new syncer. */
  syncing= NULL;
  if (active)
    mysql_cond_signal(&active->cond);

  mysql_mutex_unlock(&LOCK_tc);
  return err != 0;
}

/**
  erase xid from the page, update page free space counters/pointers.
  cookie points directly to the memory where xid was logged.
*/

void TC_LOG_MMAP::unlog(ulong cookie, my_xid xid)
{
  PAGE *p= pages + (cookie / tc_log_page_size);
  my_xid *x= (my_xid *)(data + cookie);

  DBUG_ASSERT(*x == xid);
  DBUG_ASSERT(x >= p->start && x < p->end);
  *x= 0;

  mysql_mutex_lock(&LOCK_tc);
  p->free++;
  DBUG_ASSERT(p->free <= p->size);
  set_if_smaller(p->ptr, x);
  if (p->free == p->size)               // the page is completely empty
    tc_log_cur_pages_used--;
  if (p->waiters == 0)                 // the page is in pool and ready to rock
    mysql_cond_broadcast(&COND_pool);  // ping ... for overflow()
  mysql_mutex_unlock(&LOCK_tc);
}

void TC_LOG_MMAP::close()
{
  uint i;
  switch (inited) {
  case 6:
    mysql_mutex_destroy(&LOCK_tc);
    mysql_cond_destroy(&COND_pool);
  case 5:
    data[0]='A'; // garble the first (signature) byte, in case mysql_file_delete fails
  case 4:
    for (i=0; i < npages; i++)
    {
      if (pages[i].ptr == 0)
        break;
      mysql_cond_destroy(&pages[i].cond);
    }
  case 3:
    my_free(pages);
  case 2:
    my_munmap((char*)data, (size_t)file_length);
  case 1:
    mysql_file_close(fd, MYF(0));
  }
  if (inited>=5) // cannot do in the switch because of Windows
    mysql_file_delete(key_file_tclog, logname, MYF(MY_WME));
  inited=0;
}

int TC_LOG_MMAP::recover()
{
  HASH xids;
  PAGE *p=pages, *end_p=pages+npages;

  if (memcmp(data, tc_log_magic, sizeof(tc_log_magic)))
  {
    sql_print_error("Bad magic header in tc log");
    goto err1;
  }

  /*
    the first byte after magic signature is set to current
    number of storage engines on startup
  */
  if (data[sizeof(tc_log_magic)] != total_ha_2pc)
  {
    sql_print_error("Recovery failed! You must enable "
                    "exactly %d storage engines that support "
                    "two-phase commit protocol",
                    data[sizeof(tc_log_magic)]);
    goto err1;
  }

  if (my_hash_init(&xids, &my_charset_bin, tc_log_page_size/3, 0,
                   sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  for ( ; p < end_p ; p++)
  {
    for (my_xid *x=p->start; x < p->end; x++)
      if (*x && my_hash_insert(&xids, (uchar *)x))
        goto err2; // OOM
  }

  if (ha_recover(&xids))
    goto err2;

  my_hash_free(&xids);
  memset(data, 0, (size_t)file_length);
  return 0;

err2:
  my_hash_free(&xids);
err1:
  sql_print_error("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete tc log and start mysqld with "
                  "--tc-heuristic-recover={commit|rollback}");
  return 1;
}
#endif

TC_LOG *tc_log;
TC_LOG_DUMMY tc_log_dummy;
TC_LOG_MMAP  tc_log_mmap;

/**
  Perform heuristic recovery, if --tc-heuristic-recover was used.

  @note
    no matter whether heuristic recovery was successful or not
    mysqld must exit. So, return value is the same in both cases.

  @retval
    0	no heuristic recovery was requested
  @retval
    1   heuristic recovery was performed
*/

int TC_LOG::using_heuristic_recover()
{
  if (!tc_heuristic_recover)
    return 0;

  sql_print_information("Heuristic crash recovery mode");
  if (ha_recover(0))
    sql_print_error("Heuristic crash recovery failed");
  sql_print_information("Please restart mysqld without --tc-heuristic-recover");
  return 1;
}
