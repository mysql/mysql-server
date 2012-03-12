/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


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
#include "sql_repl.h"
#include "sql_delete.h"                         // mysql_truncate
#include "sql_parse.h"                          // command_name
#include "sql_time.h"           // calc_time_from_sec, my_time_compare
#include "tztime.h"             // my_tz_OFFSET0, struct Time_zone
#include "sql_acl.h"            // SUPER_ACL
#include "log_event.h"          // Query_log_event
#include "rpl_filter.h"
#include "rpl_rli.h"
#include "sql_audit.h"

#include <my_dir.h>
#include <stdarg.h>
#include <m_ctype.h>				// For test_if_number

#ifdef _WIN32
#include "message.h"
#endif

#include "sql_plugin.h"
#include "rpl_handler.h"
#include "debug_sync.h"
/* max size of the log message */
#define MAX_LOG_BUFFER_SIZE 1024
#define MAX_TIME_SIZE 32
#define MY_OFF_T_UNDEF (~(my_off_t)0UL)

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

LOGGER logger;

MYSQL_BIN_LOG mysql_bin_log(&sync_binlog_period);

static bool test_if_number(const char *str,
			   ulong *res, bool allow_wildcards);
static int binlog_init(void *p);
static int binlog_close_connection(handlerton *hton, THD *thd);
static int binlog_savepoint_set(handlerton *hton, THD *thd, void *sv);
static int binlog_savepoint_rollback(handlerton *hton, THD *thd, void *sv);
static int binlog_commit(handlerton *hton, THD *thd, bool all);
static int binlog_rollback(handlerton *hton, THD *thd, bool all);
static int binlog_prepare(handlerton *hton, THD *thd, bool all);

/**
   purge logs, master and slave sides both, related error code
   convertor.
   Called from @c purge_error_message(), @c MYSQL_BIN_LOG::reset_logs()

   @param  res  an internal to purging routines error code 

   @return the user level error code ER_*
*/
uint purge_log_get_error_code(int res)
{
  uint errcode= 0;

  switch (res)  {
  case 0: break;
  case LOG_INFO_EOF:	errcode= ER_UNKNOWN_TARGET_BINLOG; break;
  case LOG_INFO_IO:	errcode= ER_IO_ERR_LOG_INDEX_READ; break;
  case LOG_INFO_INVALID:errcode= ER_BINLOG_PURGE_PROHIBITED; break;
  case LOG_INFO_SEEK:	errcode= ER_FSEEK_FAIL; break;
  case LOG_INFO_MEM:	errcode= ER_OUT_OF_RESOURCES; break;
  case LOG_INFO_FATAL:	errcode= ER_BINLOG_PURGE_FATAL_ERR; break;
  case LOG_INFO_IN_USE: errcode= ER_LOG_IN_USE; break;
  case LOG_INFO_EMFILE: errcode= ER_BINLOG_PURGE_EMFILE; break;
  default:		errcode= ER_LOG_PURGE_UNKNOWN_ERR; break;
  }

  return errcode;
}

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
  Silence_log_table_errors()
  {
    m_message[0]= '\0';
  }

  virtual ~Silence_log_table_errors() {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sql_state,
                                MYSQL_ERROR::enum_warning_level level,
                                const char* msg,
                                MYSQL_ERROR ** cond_hdl);
  const char *message() const { return m_message; }
};

bool
Silence_log_table_errors::handle_condition(THD *,
                                           uint,
                                           const char*,
                                           MYSQL_ERROR::enum_warning_level,
                                           const char* msg,
                                           MYSQL_ERROR ** cond_hdl)
{
  *cond_hdl= NULL;
  strmake(m_message, msg, sizeof(m_message)-1);
  return TRUE;
}

sql_print_message_func sql_print_message_handlers[3] =
{
  sql_print_information,
  sql_print_warning,
  sql_print_error
};

/**
  Create the name of the log specified.

  This method forms a new path + file name for the
  log specified in @c name.

  @param[IN] buff    Location for building new string.
  @param[IN] name    Name of the log file.
  @param[IN] log_ext The extension for the log (e.g. .log).

  @returns Pointer to new string containing the name.
*/
char *make_log_name(char *buff, const char *name, const char* log_ext)
{
  strmake(buff, name, FN_REFLEN-5);
  return fn_format(buff, buff, mysql_real_data_home, log_ext,
                   MYF(MY_UNPACK_FILENAME|MY_REPLACE_EXT));
}

/*
  Helper class to hold a mutex for the duration of the
  block.

  Eliminates the need for explicit unlocking of mutexes on, e.g.,
  error returns.  On passing a null pointer, the sentry will not do
  anything.
 */
class Mutex_sentry
{
public:
  Mutex_sentry(mysql_mutex_t *mutex)
    : m_mutex(mutex)
  {
    if (m_mutex)
      mysql_mutex_lock(mutex);
  }

  ~Mutex_sentry()
  {
    if (m_mutex)
      mysql_mutex_unlock(m_mutex);
#ifndef DBUG_OFF
    m_mutex= 0;
#endif
  }

private:
  mysql_mutex_t *m_mutex;

  // It's not allowed to copy this object in any way
  Mutex_sentry(Mutex_sentry const&);
  void operator=(Mutex_sentry const&);
};

/*
  Helper classes to store non-transactional and transactional data
  before copying it to the binary log.
*/
class binlog_cache_data
{
public:
  binlog_cache_data(): m_pending(0), before_stmt_pos(MY_OFF_T_UNDEF),
  incident(FALSE), changes_to_non_trans_temp_table_flag(FALSE),
  saved_max_binlog_cache_size(0), ptr_binlog_cache_use(0),
  ptr_binlog_cache_disk_use(0)
  { }
  
  ~binlog_cache_data()
  {
    DBUG_ASSERT(empty());
    close_cached_file(&cache_log);
  }

  bool empty() const
  {
    return pending() == NULL && my_b_tell(&cache_log) == 0;
  }

  Rows_log_event *pending() const
  {
    return m_pending;
  }

  void set_pending(Rows_log_event *const pending)
  {
    m_pending= pending;
  }

  void set_incident(void)
  {
    incident= TRUE;
  }
  
  bool has_incident(void)
  {
    return(incident);
  }

  void set_changes_to_non_trans_temp_table()
  {
    changes_to_non_trans_temp_table_flag= TRUE;    
  }

  bool changes_to_non_trans_temp_table()
  {
    return (changes_to_non_trans_temp_table_flag);    
  }

  void reset()
  {
    compute_statistics();
    truncate(0);
    changes_to_non_trans_temp_table_flag= FALSE;
    incident= FALSE;
    before_stmt_pos= MY_OFF_T_UNDEF;
    /*
      The truncate function calls reinit_io_cache that calls my_b_flush_io_cache
      which may increase disk_writes. This breaks the disk_writes use by the
      binary log which aims to compute the ratio between in-memory cache usage
      and disk cache usage. To avoid this undesirable behavior, we reset the
      variable after truncating the cache.
    */
    cache_log.disk_writes= 0;
    DBUG_ASSERT(empty());
  }

  my_off_t get_byte_position() const
  {
    return my_b_tell(&cache_log);
  }

  my_off_t get_prev_position()
  {
     return(before_stmt_pos);
  }

  void set_prev_position(my_off_t pos)
  {
     before_stmt_pos= pos;
  }
  
  void restore_prev_position()
  {
    truncate(before_stmt_pos);
  }

  void restore_savepoint(my_off_t pos)
  {
    truncate(pos);
    if (pos < before_stmt_pos)
      before_stmt_pos= MY_OFF_T_UNDEF;
  }

  void set_binlog_cache_info(ulong param_max_binlog_cache_size,
                             ulong *param_ptr_binlog_cache_use,
                             ulong *param_ptr_binlog_cache_disk_use)
  {
    /*
      The assertions guarantee that the set_binlog_cache_info is
      called just once and information passed as parameters are
      never zero.

      This is done while calling the constructor binlog_cache_mngr.
      We cannot set informaton in the constructor binlog_cache_data
      because the space for binlog_cache_mngr is allocated through
      a placement new.

      In the future, we can refactor this and change it to avoid
      the set_binlog_info. 
    */
    DBUG_ASSERT(saved_max_binlog_cache_size == 0 &&
                param_max_binlog_cache_size != 0 &&
                ptr_binlog_cache_use == 0 &&
                param_ptr_binlog_cache_use != 0 &&
                ptr_binlog_cache_disk_use == 0 &&
                param_ptr_binlog_cache_disk_use != 0);

    saved_max_binlog_cache_size= param_max_binlog_cache_size;
    ptr_binlog_cache_use= param_ptr_binlog_cache_use;
    ptr_binlog_cache_disk_use= param_ptr_binlog_cache_disk_use;
    cache_log.end_of_file= saved_max_binlog_cache_size;
  }

  /*
    Cache to store data before copying it to the binary log.
  */
  IO_CACHE cache_log;

private:
  /*
    Pending binrows event. This event is the event where the rows are currently
    written.
   */
  Rows_log_event *m_pending;

  /*
    Binlog position before the start of the current statement.
  */
  my_off_t before_stmt_pos;
 
  /*
    This indicates that some events did not get into the cache and most likely
    it is corrupted.
  */ 
  bool incident;

  /*
    This flag indicates if the cache has changes to temporary tables.
    @TODO This a temporary fix and should be removed after BUG#54562.
  */
  bool changes_to_non_trans_temp_table_flag;

  /**
    This function computes binlog cache and disk usage.
  */
  void compute_statistics()
  {
    if (!empty())
    {
      statistic_increment(*ptr_binlog_cache_use, &LOCK_status);
      if (cache_log.disk_writes != 0)
        statistic_increment(*ptr_binlog_cache_disk_use, &LOCK_status);
    }
  }

  /*
    Stores the values of maximum size of the cache allowed when this cache
    is configured. This corresponds to either
      . max_binlog_cache_size or max_binlog_stmt_cache_size.
  */
  ulong saved_max_binlog_cache_size;

  /*
    Stores a pointer to the status variable that keeps track of the in-memory 
    cache usage. This corresponds to either
      . binlog_cache_use or binlog_stmt_cache_use.
  */
  ulong *ptr_binlog_cache_use;

  /*
    Stores a pointer to the status variable that keeps track of the disk
    cache usage. This corresponds to either
      . binlog_cache_disk_use or binlog_stmt_cache_disk_use.
  */
  ulong *ptr_binlog_cache_disk_use;

  /*
    It truncates the cache to a certain position. This includes deleting the
    pending event.
   */
  void truncate(my_off_t pos)
  {
    DBUG_PRINT("info", ("truncating to position %lu", (ulong) pos));
    if (pending())
    {
      delete pending();
      set_pending(0);
    }
    reinit_io_cache(&cache_log, WRITE_CACHE, pos, 0, 0);
    cache_log.end_of_file= saved_max_binlog_cache_size;
  }
 
  binlog_cache_data& operator=(const binlog_cache_data& info);
  binlog_cache_data(const binlog_cache_data& info);
};

class binlog_cache_mngr {
public:
  binlog_cache_mngr(ulong param_max_binlog_stmt_cache_size,
                    ulong param_max_binlog_cache_size,
                    ulong *param_ptr_binlog_stmt_cache_use,
                    ulong *param_ptr_binlog_stmt_cache_disk_use,
                    ulong *param_ptr_binlog_cache_use,
                    ulong *param_ptr_binlog_cache_disk_use)
  {
     stmt_cache.set_binlog_cache_info(param_max_binlog_stmt_cache_size,
                                      param_ptr_binlog_stmt_cache_use,
                                      param_ptr_binlog_stmt_cache_disk_use);
     trx_cache.set_binlog_cache_info(param_max_binlog_cache_size,
                                     param_ptr_binlog_cache_use,
                                     param_ptr_binlog_cache_disk_use);
  }

  void reset_cache(binlog_cache_data* cache_data)
  {
    cache_data->reset();
  }

  binlog_cache_data* get_binlog_cache_data(bool is_transactional)
  {
    return (is_transactional ? &trx_cache : &stmt_cache);
  }

  IO_CACHE* get_binlog_cache_log(bool is_transactional)
  {
    return (is_transactional ? &trx_cache.cache_log : &stmt_cache.cache_log);
  }

  binlog_cache_data stmt_cache;

  binlog_cache_data trx_cache;

private:

  binlog_cache_mngr& operator=(const binlog_cache_mngr& info);
  binlog_cache_mngr(const binlog_cache_mngr& info);
};

handlerton *binlog_hton;

bool LOGGER::is_log_table_enabled(uint log_table_type)
{
  switch (log_table_type) {
  case QUERY_LOG_SLOW:
    return (table_log_handler != NULL) && opt_slow_log;
  case QUERY_LOG_GENERAL:
    return (table_log_handler != NULL) && opt_log ;
  default:
    DBUG_ASSERT(0);
    return FALSE;                             /* make compiler happy */
  }
}


/* Check if a given table is opened log table */
int check_if_log_table(size_t db_len, const char *db, size_t table_name_len,
                       const char *table_name, bool check_if_opened)
{
  if (db_len == 5 &&
      !(lower_case_table_names ?
        my_strcasecmp(system_charset_info, db, "mysql") :
        strcmp(db, "mysql")))
  {
    if (table_name_len == 11 && !(lower_case_table_names ?
                                  my_strcasecmp(system_charset_info,
                                                table_name, "general_log") :
                                  strcmp(table_name, "general_log")))
    {
      if (!check_if_opened || logger.is_log_table_enabled(QUERY_LOG_GENERAL))
        return QUERY_LOG_GENERAL;
      return 0;
    }

    if (table_name_len == 8 && !(lower_case_table_names ?
      my_strcasecmp(system_charset_info, table_name, "slow_log") :
      strcmp(table_name, "slow_log")))
    {
      if (!check_if_opened || logger.is_log_table_enabled(QUERY_LOG_SLOW))
        return QUERY_LOG_SLOW;
      return 0;
    }
  }
  return 0;
}


Log_to_csv_event_handler::Log_to_csv_event_handler()
{
}


Log_to_csv_event_handler::~Log_to_csv_event_handler()
{
}


void Log_to_csv_event_handler::cleanup()
{
  logger.is_log_tables_initialized= FALSE;
}

/* log event handlers */

/**
  Log command to the general log table

  Log given command to the general log table.

  @param  event_time        command start timestamp
  @param  user_host         the pointer to the string with user@host info
  @param  user_host_len     length of the user_host string. this is computed
                            once and passed to all general log event handlers
  @param  thread_id         Id of the thread, issued a query
  @param  command_type      the type of the command being logged
  @param  command_type_len  the length of the string above
  @param  sql_text          the very text of the query being executed
  @param  sql_text_len      the length of sql_text string


  @return This function attempts to never call my_error(). This is
  necessary, because general logging happens already after a statement
  status has been sent to the client, so the client can not see the
  error anyway. Besides, the error is not related to the statement
  being executed and is internal, and thus should be handled
  internally (@todo: how?).
  If a write to the table has failed, the function attempts to
  write to a short error message to the file. The failure is also
  indicated in the return value. 

  @retval  FALSE   OK
  @retval  TRUE    error occured
*/

bool Log_to_csv_event_handler::
  log_general(THD *thd, time_t event_time, const char *user_host,
              uint user_host_len, int thread_id,
              const char *command_type, uint command_type_len,
              const char *sql_text, uint sql_text_len,
              CHARSET_INFO *client_cs)
{
  TABLE_LIST table_list;
  TABLE *table;
  bool result= TRUE;
  bool need_close= FALSE;
  bool need_pop= FALSE;
  bool need_rnd_end= FALSE;
  uint field_index;
  Silence_log_table_errors error_handler;
  Open_tables_backup open_tables_backup;
  ulonglong save_thd_options;
  bool save_time_zone_used;

  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  save_time_zone_used= thd->time_zone_used;

  save_thd_options= thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_BIN_LOG;

  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            GENERAL_LOG_NAME.str, GENERAL_LOG_NAME.length,
                            GENERAL_LOG_NAME.str,
                            TL_WRITE_CONCURRENT_INSERT);

  /*
    1) open_log_table generates an error of the
    table can not be opened or is corrupted.
    2) "INSERT INTO general_log" can generate warning sometimes.

    Suppress these warnings and errors, they can't be dealt with
    properly anyway.

    QQ: this problem needs to be studied in more detail.
    Comment this 2 lines and run "cast.test" to see what's happening.
  */
  thd->push_internal_handler(& error_handler);
  need_pop= TRUE;

  if (!(table= open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close= TRUE;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= TRUE;

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

  /* check that all columns exist */
  if (table->s->fields < 6)
    goto err;

  DBUG_ASSERT(table->field[0]->type() == MYSQL_TYPE_TIMESTAMP);

  ((Field_timestamp*) table->field[0])->store_timestamp((my_time_t)
                                                        event_time);

  /* do a write */
  if (table->field[1]->store(user_host, user_host_len, client_cs) ||
      table->field[2]->store((longlong) thread_id, TRUE) ||
      table->field[3]->store((longlong) server_id, TRUE) ||
      table->field[4]->store(command_type, command_type_len, client_cs))
    goto err;

  /*
    A positive return value in store() means truncation.
    Still logging a message in the log in this case.
  */
  table->field[5]->flags|= FIELDFLAG_HEX_ESCAPE;
  if (table->field[5]->store(sql_text, sql_text_len, client_cs) < 0)
    goto err;

  /* mark all fields as not null */
  table->field[1]->set_notnull();
  table->field[2]->set_notnull();
  table->field[3]->set_notnull();
  table->field[4]->set_notnull();
  table->field[5]->set_notnull();

  /* Set any extra columns to their default values */
  for (field_index= 6 ; field_index < table->s->fields ; field_index++)
  {
    table->field[field_index]->set_default();
  }

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= FALSE;

err:
  if (result && !thd->killed)
    sql_print_error("Failed to write to mysql.general_log: %s",
                    error_handler.message());

  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }
  if (need_pop)
    thd->pop_internal_handler();
  if (need_close)
    close_log_table(thd, &open_tables_backup);

  thd->variables.option_bits= save_thd_options;
  thd->time_zone_used= save_time_zone_used;
  return result;
}


/*
  Log a query to the slow log table

  SYNOPSIS
    log_slow()
    thd               THD of the query
    current_time      current timestamp
    query_start_arg   command start timestamp
    user_host         the pointer to the string with user@host info
    user_host_len     length of the user_host string. this is computed once
                      and passed to all general log event handlers
    query_time        Amount of time the query took to execute (in microseconds)
    lock_time         Amount of time the query was locked (in microseconds)
    is_command        The flag, which determines, whether the sql_text is a
                      query or an administrator command (these are treated
                      differently by the old logging routines)
    sql_text          the very text of the query or administrator command
                      processed
    sql_text_len      the length of sql_text string

  DESCRIPTION

   Log a query to the slow log table

  RETURN
    FALSE - OK
    TRUE - error occured
*/

bool Log_to_csv_event_handler::
  log_slow(THD *thd, time_t current_time, time_t query_start_arg,
           const char *user_host, uint user_host_len,
           ulonglong query_utime, ulonglong lock_utime, bool is_command,
           const char *sql_text, uint sql_text_len)
{
  TABLE_LIST table_list;
  TABLE *table;
  bool result= TRUE;
  bool need_close= FALSE;
  bool need_rnd_end= FALSE;
  Silence_log_table_errors error_handler;
  Open_tables_backup open_tables_backup;
  CHARSET_INFO *client_cs= thd->variables.character_set_client;
  bool save_time_zone_used;
  DBUG_ENTER("Log_to_csv_event_handler::log_slow");

  thd->push_internal_handler(& error_handler);
  /*
    CSV uses TIME_to_timestamp() internally if table needs to be repaired
    which will set thd->time_zone_used
  */
  save_time_zone_used= thd->time_zone_used;

  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            SLOW_LOG_NAME.str, SLOW_LOG_NAME.length,
                            SLOW_LOG_NAME.str,
                            TL_WRITE_CONCURRENT_INSERT);

  if (!(table= open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close= TRUE;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= TRUE;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  restore_record(table, s->default_values);    // Get empty record

  /* check that all columns exist */
  if (table->s->fields < 11)
    goto err;

  /* store the time and user values */
  DBUG_ASSERT(table->field[0]->type() == MYSQL_TYPE_TIMESTAMP);
  ((Field_timestamp*) table->field[0])->store_timestamp((my_time_t)
                                                        current_time);
  if (table->field[1]->store(user_host, user_host_len, client_cs))
    goto err;

  if (query_start_arg)
  {
    longlong query_time= (longlong) (query_utime/1000000);
    longlong lock_time=  (longlong) (lock_utime/1000000);
    /*
      A TIME field can not hold the full longlong range; query_time or
      lock_time may be truncated without warning here, if greater than
      839 hours (~35 days)
    */
    MYSQL_TIME t;
    t.neg= 0;

    /* fill in query_time field */
    calc_time_from_sec(&t, (long) min(query_time, (longlong) TIME_MAX_VALUE_SECONDS), 0);
    if (table->field[2]->store_time(&t, MYSQL_TIMESTAMP_TIME))
      goto err;
    /* lock_time */
    calc_time_from_sec(&t, (long) min(lock_time, (longlong) TIME_MAX_VALUE_SECONDS), 0);
    if (table->field[3]->store_time(&t, MYSQL_TIMESTAMP_TIME))
      goto err;
    /* rows_sent */
    if (table->field[4]->store((longlong) thd->sent_row_count, TRUE))
      goto err;
    /* rows_examined */
    if (table->field[5]->store((longlong) thd->examined_row_count, TRUE))
      goto err;
  }
  else
  {
    table->field[2]->set_null();
    table->field[3]->set_null();
    table->field[4]->set_null();
    table->field[5]->set_null();
  }
  /* fill database field */
  if (thd->db)
  {
    if (table->field[6]->store(thd->db, thd->db_length, client_cs))
      goto err;
    table->field[6]->set_notnull();
  }

  if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
  {
    if (table->
        field[7]->store((longlong)
                        thd->first_successful_insert_id_in_prev_stmt_for_binlog,
                        TRUE))
      goto err;
    table->field[7]->set_notnull();
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
        field[8]->store((longlong)
          thd->auto_inc_intervals_in_cur_stmt_for_binlog.minimum(), TRUE))
      goto err;
    table->field[8]->set_notnull();
  }

  if (table->field[9]->store((longlong) server_id, TRUE))
    goto err;
  table->field[9]->set_notnull();

  /*
    Column sql_text.
    A positive return value in store() means truncation.
    Still logging a message in the log in this case.
  */
  if (table->field[10]->store(sql_text, sql_text_len, client_cs) < 0)
    goto err;

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= FALSE;

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

int Log_to_csv_event_handler::
  activate_log(THD *thd, uint log_table_type)
{
  TABLE_LIST table_list;
  TABLE *table;
  LEX_STRING *UNINIT_VAR(log_name);
  int result;
  Open_tables_backup open_tables_backup;

  DBUG_ENTER("Log_to_csv_event_handler::activate_log");

  if (log_table_type == QUERY_LOG_GENERAL)
  {
    log_name= &GENERAL_LOG_NAME;
  }
  else
  {
    DBUG_ASSERT(log_table_type == QUERY_LOG_SLOW);

    log_name= &SLOW_LOG_NAME;
  }
  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            log_name->str, log_name->length, log_name->str,
                            TL_WRITE_CONCURRENT_INSERT);

  table= open_log_table(thd, &table_list, &open_tables_backup);
  if (table)
  {
    result= 0;
    close_log_table(thd, &open_tables_backup);
  }
  else
    result= 1;

  DBUG_RETURN(result);
}

bool Log_to_csv_event_handler::
  log_error(enum loglevel level, const char *format, va_list args)
{
  /* No log table is implemented */
  DBUG_ASSERT(0);
  return FALSE;
}

bool Log_to_file_event_handler::
  log_error(enum loglevel level, const char *format,
            va_list args)
{
  return vprint_msg_to_log(level, format, args);
}

void Log_to_file_event_handler::init_pthread_objects()
{
  mysql_log.init_pthread_objects();
  mysql_slow_log.init_pthread_objects();
}


/** Wrapper around MYSQL_LOG::write() for slow log. */

bool Log_to_file_event_handler::
  log_slow(THD *thd, time_t current_time, time_t query_start_arg,
           const char *user_host, uint user_host_len,
           ulonglong query_utime, ulonglong lock_utime, bool is_command,
           const char *sql_text, uint sql_text_len)
{
  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  bool retval= mysql_slow_log.write(thd, current_time, query_start_arg,
                                    user_host, user_host_len,
                                    query_utime, lock_utime, is_command,
                                    sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}


/**
   Wrapper around MYSQL_LOG::write() for general log. We need it since we
   want all log event handlers to have the same signature.
*/

bool Log_to_file_event_handler::
  log_general(THD *thd, time_t event_time, const char *user_host,
              uint user_host_len, int thread_id,
              const char *command_type, uint command_type_len,
              const char *sql_text, uint sql_text_len,
              CHARSET_INFO *client_cs)
{
  Silence_log_table_errors error_handler;
  thd->push_internal_handler(&error_handler);
  bool retval= mysql_log.write(event_time, user_host, user_host_len,
                               thread_id, command_type, command_type_len,
                               sql_text, sql_text_len);
  thd->pop_internal_handler();
  return retval;
}


bool Log_to_file_event_handler::init()
{
  if (!is_initialized)
  {
    if (opt_slow_log)
      mysql_slow_log.open_slow_log(opt_slow_logname);

    if (opt_log)
      mysql_log.open_query_log(opt_logname);

    is_initialized= TRUE;
  }

  return FALSE;
}


void Log_to_file_event_handler::cleanup()
{
  mysql_log.cleanup();
  mysql_slow_log.cleanup();
}

void Log_to_file_event_handler::flush()
{
  /* reopen log files */
  if (opt_log)
    mysql_log.reopen_file();
  if (opt_slow_log)
    mysql_slow_log.reopen_file();
}

/*
  Log error with all enabled log event handlers

  SYNOPSIS
    error_log_print()

    level             The level of the error significance: NOTE,
                      WARNING or ERROR.
    format            format string for the error message
    args              list of arguments for the format string

  RETURN
    FALSE - OK
    TRUE - error occured
*/

bool LOGGER::error_log_print(enum loglevel level, const char *format,
                             va_list args)
{
  bool error= FALSE;
  Log_event_handler **current_handler;

  /* currently we don't need locking here as there is no error_log table */
  for (current_handler= error_log_handler_list ; *current_handler ;)
    error= (*current_handler++)->log_error(level, format, args) || error;

  return error;
}


void LOGGER::cleanup_base()
{
  DBUG_ASSERT(inited == 1);
  mysql_rwlock_destroy(&LOCK_logger);
  if (table_log_handler)
  {
    table_log_handler->cleanup();
    delete table_log_handler;
    table_log_handler= NULL;
  }
  if (file_log_handler)
    file_log_handler->cleanup();
}


void LOGGER::cleanup_end()
{
  DBUG_ASSERT(inited == 1);
  if (file_log_handler)
  {
    delete file_log_handler;
    file_log_handler=NULL;
  }
  inited= 0;
}


/**
  Perform basic log initialization: create file-based log handler and
  init error log.
*/
void LOGGER::init_base()
{
  DBUG_ASSERT(inited == 0);
  inited= 1;

  /*
    Here we create file log handler. We don't do it for the table log handler
    here as it cannot be created so early. The reason is THD initialization,
    which depends on the system variables (parsed later).
  */
  if (!file_log_handler)
    file_log_handler= new Log_to_file_event_handler;

  /* by default we use traditional error log */
  init_error_log(LOG_FILE);

  file_log_handler->init_pthread_objects();
  mysql_rwlock_init(key_rwlock_LOCK_logger, &LOCK_logger);
}


void LOGGER::init_log_tables()
{
  if (!table_log_handler)
    table_log_handler= new Log_to_csv_event_handler;

  if (!is_log_tables_initialized &&
      !table_log_handler->init() && !file_log_handler->init())
    is_log_tables_initialized= TRUE;
}


bool LOGGER::flush_logs(THD *thd)
{
  int rc= 0;

  /*
    Now we lock logger, as nobody should be able to use logging routines while
    log tables are closed
  */
  logger.lock_exclusive();

  /* reopen log files */
  file_log_handler->flush();

  /* end of log flush */
  logger.unlock();
  return rc;
}


/**
  Close and reopen the slow log (with locks).
  
  @returns FALSE.
*/
bool LOGGER::flush_slow_log()
{
  /*
    Now we lock logger, as nobody should be able to use logging routines while
    log tables are closed
  */
  logger.lock_exclusive();

  /* Reopen slow log file */
  if (opt_slow_log)
    file_log_handler->get_mysql_slow_log()->reopen_file();

  /* End of log flush */
  logger.unlock();

  return 0;
}


/**
  Close and reopen the general log (with locks).

  @returns FALSE.
*/
bool LOGGER::flush_general_log()
{
  /*
    Now we lock logger, as nobody should be able to use logging routines while
    log tables are closed
  */
  logger.lock_exclusive();

  /* Reopen general log file */
  if (opt_log)
    file_log_handler->get_mysql_log()->reopen_file();

  /* End of log flush */
  logger.unlock();

  return 0;
}


/*
  Log slow query with all enabled log event handlers

  SYNOPSIS
    slow_log_print()

    thd                 THD of the query being logged
    query               The query being logged
    query_length        The length of the query string
    current_utime       Current time in microseconds (from undefined start)

  RETURN
    FALSE   OK
    TRUE    error occured
*/

bool LOGGER::slow_log_print(THD *thd, const char *query, uint query_length,
                            ulonglong current_utime)

{
  bool error= FALSE;
  Log_event_handler **current_handler;
  bool is_command= FALSE;
  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  Security_context *sctx= thd->security_ctx;
  uint user_host_len= 0;
  ulonglong query_utime, lock_utime;

  DBUG_ASSERT(thd->enable_slow_log);
  /*
    Print the message to the buffer if we have slow log enabled
  */

  if (*slow_log_handler_list)
  {
    time_t current_time;

    /* do not log slow queries from replication threads */
    if (thd->slave_thread && !opt_log_slow_slave_statements)
      return 0;

    lock_shared();
    if (!opt_slow_log)
    {
      unlock();
      return 0;
    }

    /* fill in user_host value: the format is "%s[%s] @ %s [%s]" */
    user_host_len= (strxnmov(user_host_buff, MAX_USER_HOST_SIZE,
                             sctx->priv_user ? sctx->priv_user : "", "[",
                             sctx->user ? sctx->user : "", "] @ ",
                             sctx->host ? sctx->host : "", " [",
                             sctx->ip ? sctx->ip : "", "]", NullS) -
                    user_host_buff);

    current_time= my_time_possible_from_micro(current_utime);
    if (thd->start_utime)
    {
      query_utime= (current_utime - thd->start_utime);
      lock_utime=  (thd->utime_after_lock - thd->start_utime);
    }
    else
    {
      query_utime= lock_utime= 0;
    }

    if (!query)
    {
      is_command= TRUE;
      query= command_name[thd->command].str;
      query_length= command_name[thd->command].length;
    }

    for (current_handler= slow_log_handler_list; *current_handler ;)
      error= (*current_handler++)->log_slow(thd, current_time, thd->start_time,
                                            user_host_buff, user_host_len,
                                            query_utime, lock_utime, is_command,
                                            query, query_length) || error;

    unlock();
  }
  return error;
}

bool LOGGER::general_log_write(THD *thd, enum enum_server_command command,
                               const char *query, uint query_length)
{
  bool error= FALSE;
  Log_event_handler **current_handler= general_log_handler_list;
  char user_host_buff[MAX_USER_HOST_SIZE + 1];
  uint user_host_len= 0;
  time_t current_time;

  DBUG_ASSERT(thd);

  lock_shared();
  if (!opt_log)
  {
    unlock();
    return 0;
  }
  user_host_len= make_user_name(thd, user_host_buff);

  current_time= my_time(0);

  mysql_audit_general_log(thd, current_time,
                          user_host_buff, user_host_len,
                          command_name[(uint) command].str,
                          command_name[(uint) command].length,
                          query, query_length);
                        
  while (*current_handler)
    error|= (*current_handler++)->
      log_general(thd, current_time, user_host_buff,
                  user_host_len, thd->thread_id,
                  command_name[(uint) command].str,
                  command_name[(uint) command].length,
                  query, query_length,
                  thd->variables.character_set_client) || error;
  unlock();

  return error;
}

bool LOGGER::general_log_print(THD *thd, enum enum_server_command command,
                               const char *format, va_list args)
{
  uint message_buff_len= 0;
  char message_buff[MAX_LOG_BUFFER_SIZE];

  /* prepare message */
  if (format)
    message_buff_len= my_vsnprintf(message_buff, sizeof(message_buff),
                                   format, args);
  else
    message_buff[0]= '\0';

  return general_log_write(thd, command, message_buff, message_buff_len);
}

void LOGGER::init_error_log(uint error_log_printer)
{
  if (error_log_printer & LOG_NONE)
  {
    error_log_handler_list[0]= 0;
    return;
  }

  switch (error_log_printer) {
  case LOG_FILE:
    error_log_handler_list[0]= file_log_handler;
    error_log_handler_list[1]= 0;
    break;
    /* these two are disabled for now */
  case LOG_TABLE:
    DBUG_ASSERT(0);
    break;
  case LOG_TABLE|LOG_FILE:
    DBUG_ASSERT(0);
    break;
  }
}

void LOGGER::init_slow_log(uint slow_log_printer)
{
  if (slow_log_printer & LOG_NONE)
  {
    slow_log_handler_list[0]= 0;
    return;
  }

  switch (slow_log_printer) {
  case LOG_FILE:
    slow_log_handler_list[0]= file_log_handler;
    slow_log_handler_list[1]= 0;
    break;
  case LOG_TABLE:
    slow_log_handler_list[0]= table_log_handler;
    slow_log_handler_list[1]= 0;
    break;
  case LOG_TABLE|LOG_FILE:
    slow_log_handler_list[0]= file_log_handler;
    slow_log_handler_list[1]= table_log_handler;
    slow_log_handler_list[2]= 0;
    break;
  }
}

void LOGGER::init_general_log(uint general_log_printer)
{
  if (general_log_printer & LOG_NONE)
  {
    general_log_handler_list[0]= 0;
    return;
  }

  switch (general_log_printer) {
  case LOG_FILE:
    general_log_handler_list[0]= file_log_handler;
    general_log_handler_list[1]= 0;
    break;
  case LOG_TABLE:
    general_log_handler_list[0]= table_log_handler;
    general_log_handler_list[1]= 0;
    break;
  case LOG_TABLE|LOG_FILE:
    general_log_handler_list[0]= file_log_handler;
    general_log_handler_list[1]= table_log_handler;
    general_log_handler_list[2]= 0;
    break;
  }
}


bool LOGGER::activate_log_handler(THD* thd, uint log_type)
{
  MYSQL_QUERY_LOG *file_log;
  bool res= FALSE;
  lock_exclusive();
  switch (log_type) {
  case QUERY_LOG_SLOW:
    if (!opt_slow_log)
    {
      file_log= file_log_handler->get_mysql_slow_log();

      file_log->open_slow_log(opt_slow_logname);
      if (table_log_handler->activate_log(thd, QUERY_LOG_SLOW))
      {
        /* Error printed by open table in activate_log() */
        res= TRUE;
        file_log->close(0);
      }
      else
      {
        init_slow_log(log_output_options);
        opt_slow_log= TRUE;
      }
    }
    break;
  case QUERY_LOG_GENERAL:
    if (!opt_log)
    {
      file_log= file_log_handler->get_mysql_log();

      file_log->open_query_log(opt_logname);
      if (table_log_handler->activate_log(thd, QUERY_LOG_GENERAL))
      {
        /* Error printed by open table in activate_log() */
        res= TRUE;
        file_log->close(0);
      }
      else
      {
        init_general_log(log_output_options);
        opt_log= TRUE;
      }
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  unlock();
  return res;
}


void LOGGER::deactivate_log_handler(THD *thd, uint log_type)
{
  my_bool *tmp_opt= 0;
  MYSQL_LOG *file_log;

  switch (log_type) {
  case QUERY_LOG_SLOW:
    tmp_opt= &opt_slow_log;
    file_log= file_log_handler->get_mysql_slow_log();
    break;
  case QUERY_LOG_GENERAL:
    tmp_opt= &opt_log;
    file_log= file_log_handler->get_mysql_log();
    break;
  default:
    MY_ASSERT_UNREACHABLE();
  }

  if (!(*tmp_opt))
    return;

  lock_exclusive();
  file_log->close(0);
  *tmp_opt= FALSE;
  unlock();
}


/* the parameters are unused for the log tables */
bool Log_to_csv_event_handler::init()
{
  return 0;
}

int LOGGER::set_handlers(uint error_log_printer,
                         uint slow_log_printer,
                         uint general_log_printer)
{
  /* error log table is not supported yet */
  DBUG_ASSERT(error_log_printer < LOG_TABLE);

  lock_exclusive();

  if ((slow_log_printer & LOG_TABLE || general_log_printer & LOG_TABLE) &&
      !is_log_tables_initialized)
  {
    slow_log_printer= (slow_log_printer & ~LOG_TABLE) | LOG_FILE;
    general_log_printer= (general_log_printer & ~LOG_TABLE) | LOG_FILE;

    sql_print_error("Failed to initialize log tables. "
                    "Falling back to the old-fashioned logs");
  }

  init_error_log(error_log_printer);
  init_slow_log(slow_log_printer);
  init_general_log(general_log_printer);

  unlock();

  return 0;
}

 /*
  Save position of binary log transaction cache.

  SYNPOSIS
    binlog_trans_log_savepos()

    thd      The thread to take the binlog data from
    pos      Pointer to variable where the position will be stored

  DESCRIPTION

    Save the current position in the binary log transaction cache into
    the variable pointed to by 'pos'
 */

static void
binlog_trans_log_savepos(THD *thd, my_off_t *pos)
{
  DBUG_ENTER("binlog_trans_log_savepos");
  DBUG_ASSERT(pos != NULL);
  if (thd_get_ha_data(thd, binlog_hton) == NULL)
    thd->binlog_setup_trx_data();
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);
  DBUG_ASSERT(mysql_bin_log.is_open());
  *pos= cache_mngr->trx_cache.get_byte_position();
  DBUG_PRINT("return", ("*pos: %lu", (ulong) *pos));
  DBUG_VOID_RETURN;
}


/*
  Truncate the binary log transaction cache.

  SYNPOSIS
    binlog_trans_log_truncate()

    thd      The thread to take the binlog data from
    pos      Position to truncate to

  DESCRIPTION

    Truncate the binary log to the given position. Will not change
    anything else.

 */
static void
binlog_trans_log_truncate(THD *thd, my_off_t pos)
{
  DBUG_ENTER("binlog_trans_log_truncate");
  DBUG_PRINT("enter", ("pos: %lu", (ulong) pos));

  DBUG_ASSERT(thd_get_ha_data(thd, binlog_hton) != NULL);
  /* Only true if binlog_trans_log_savepos() wasn't called before */
  DBUG_ASSERT(pos != ~(my_off_t) 0);

  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);
  cache_mngr->trx_cache.restore_savepoint(pos);
  DBUG_VOID_RETURN;
}


/*
  this function is mostly a placeholder.
  conceptually, binlog initialization (now mostly done in MYSQL_BIN_LOG::open)
  should be moved here.
*/

int binlog_init(void *p)
{
  binlog_hton= (handlerton *)p;
  binlog_hton->state=opt_bin_log ? SHOW_OPTION_YES : SHOW_OPTION_NO;
  binlog_hton->db_type=DB_TYPE_BINLOG;
  binlog_hton->savepoint_offset= sizeof(my_off_t);
  binlog_hton->close_connection= binlog_close_connection;
  binlog_hton->savepoint_set= binlog_savepoint_set;
  binlog_hton->savepoint_rollback= binlog_savepoint_rollback;
  binlog_hton->commit= binlog_commit;
  binlog_hton->rollback= binlog_rollback;
  binlog_hton->prepare= binlog_prepare;
  binlog_hton->flags= HTON_NOT_USER_SELECTABLE | HTON_HIDDEN;
  return 0;
}

static int binlog_close_connection(handlerton *hton, THD *thd)
{
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);
  DBUG_ASSERT(cache_mngr->trx_cache.empty() && cache_mngr->stmt_cache.empty());
  thd_set_ha_data(thd, binlog_hton, NULL);
  cache_mngr->~binlog_cache_mngr();
  my_free(cache_mngr);
  return 0;
}

/**
  This function flushes a cache upon commit/rollback.

  @param thd                The thread whose transaction should be flushed
  @param cache_data         Pointer to the cache
  @param end_ev             The end event either commit/rollback
  @param is_transactional   The type of the cache: transactional or
                            non-transactional

  @return
    nonzero if an error pops up when flushing the cache.
*/
static inline int
binlog_flush_cache(THD *thd, binlog_cache_data* cache_data, Log_event *end_evt,
                   bool is_transactional)
{
  DBUG_ENTER("binlog_flush_cache");
  int error= 0;

  if (!cache_data->empty())
  {
    if (thd->binlog_flush_pending_rows_event(TRUE, is_transactional))
      DBUG_RETURN(1);
    /*
      Doing a commit or a rollback including non-transactional tables,
      i.e., ending a transaction where we might write the transaction
      cache to the binary log.

      We can always end the statement when ending a transaction since
      transactions are not allowed inside stored functions. If they
      were, we would have to ensure that we're not ending a statement
      inside a stored function.
    */
    error= mysql_bin_log.write(thd, &cache_data->cache_log, end_evt,
                               cache_data->has_incident());
  }
  cache_data->reset();

  DBUG_ASSERT(cache_data->empty());
  DBUG_RETURN(error);
}

/**
  This function flushes the stmt-cache upon commit.

  @param thd                The thread whose transaction should be flushed
  @param cache_mngr         Pointer to the cache manager

  @return
    nonzero if an error pops up when flushing the cache.
*/
static inline int
binlog_commit_flush_stmt_cache(THD *thd,
                               binlog_cache_mngr *cache_mngr)
{
  Query_log_event end_evt(thd, STRING_WITH_LEN("COMMIT"),
                          FALSE, FALSE, TRUE, 0);
  return (binlog_flush_cache(thd, &cache_mngr->stmt_cache, &end_evt,
                             FALSE));
}

/**
  This function flushes the trx-cache upon commit.

  @param thd                The thread whose transaction should be flushed
  @param cache_mngr         Pointer to the cache manager

  @return
    nonzero if an error pops up when flushing the cache.
*/
static inline int
binlog_commit_flush_trx_cache(THD *thd, binlog_cache_mngr *cache_mngr)
{
  Query_log_event end_evt(thd, STRING_WITH_LEN("COMMIT"),
                          TRUE, FALSE, TRUE, 0);
  return (binlog_flush_cache(thd, &cache_mngr->trx_cache, &end_evt,
                             TRUE));
}

/**
  This function flushes the trx-cache upon rollback.

  @param thd                The thread whose transaction should be flushed
  @param cache_mngr         Pointer to the cache manager

  @return
    nonzero if an error pops up when flushing the cache.
*/
static inline int
binlog_rollback_flush_trx_cache(THD *thd, binlog_cache_mngr *cache_mngr)
{
  Query_log_event end_evt(thd, STRING_WITH_LEN("ROLLBACK"),
                          TRUE, FALSE, TRUE, 0);
  return (binlog_flush_cache(thd, &cache_mngr->trx_cache, &end_evt,
                             TRUE));
}

/**
  This function flushes the trx-cache upon commit.

  @param thd                The thread whose transaction should be flushed
  @param cache_mngr         Pointer to the cache manager
  @param xid                Transaction Id

  @return
    nonzero if an error pops up when flushing the cache.
*/
static inline int
binlog_commit_flush_trx_cache(THD *thd, binlog_cache_mngr *cache_mngr,
                              my_xid xid)
{
  Xid_log_event end_evt(thd, xid);
  return (binlog_flush_cache(thd, &cache_mngr->trx_cache, &end_evt,
                             TRUE));
}

/**
  This function truncates the transactional cache upon committing or rolling
  back either a transaction or a statement.

  @param thd        The thread whose transaction should be flushed
  @param cache_mngr Pointer to the cache data to be flushed
  @param all        @c true means truncate the transaction, otherwise the
                    statement must be truncated.

  @return
    nonzero if an error pops up when truncating the transactional cache.
*/
static int
binlog_truncate_trx_cache(THD *thd, binlog_cache_mngr *cache_mngr, bool all)
{
  DBUG_ENTER("binlog_truncate_trx_cache");
  int error=0;
  /*
    This function handles transactional changes and as such this flag
    equals to true.
  */
  bool const is_transactional= TRUE;

  DBUG_PRINT("info", ("thd->options={ %s %s}, transaction: %s",
                      FLAGSTR(thd->variables.option_bits, OPTION_NOT_AUTOCOMMIT),
                      FLAGSTR(thd->variables.option_bits, OPTION_BEGIN),
                      all ? "all" : "stmt"));

  thd->binlog_remove_pending_rows_event(TRUE, is_transactional);
  /*
    If rolling back an entire transaction or a single statement not
    inside a transaction, we reset the transaction cache.
  */
  if (ending_trans(thd, all))
  {
    if (cache_mngr->trx_cache.has_incident())
      error= mysql_bin_log.write_incident(thd, TRUE);

    thd->clear_binlog_table_maps();

    cache_mngr->reset_cache(&cache_mngr->trx_cache);
  }
  /*
    If rolling back a statement in a transaction, we truncate the
    transaction cache to remove the statement.
  */
  else
    cache_mngr->trx_cache.restore_prev_position();

  DBUG_ASSERT(thd->binlog_get_pending_rows_event(is_transactional) == NULL);
  DBUG_RETURN(error);
}

static int binlog_prepare(handlerton *hton, THD *thd, bool all)
{
  /*
    do nothing.
    just pretend we can do 2pc, so that MySQL won't
    switch to 1pc.
    real work will be done in MYSQL_BIN_LOG::log_xid()
  */
  return 0;
}

/**
  This function is called once after each statement.

  It has the responsibility to flush the caches to the binary log on commits.

  @param hton  The binlog handlerton.
  @param thd   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction commit, and
               @false otherwise.

  @see handlerton::commit
*/
static int binlog_commit(handlerton *hton, THD *thd, bool all)
{
  int error= 0;
  DBUG_ENTER("binlog_commit");
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  DBUG_PRINT("debug",
             ("all: %d, in_transaction: %s, all.modified_non_trans_table: %s, stmt.modified_non_trans_table: %s",
              all,
              YESNO(thd->in_multi_stmt_transaction_mode()),
              YESNO(thd->transaction.all.modified_non_trans_table),
              YESNO(thd->transaction.stmt.modified_non_trans_table)));

  if (!cache_mngr->stmt_cache.empty())
  {
    error= binlog_commit_flush_stmt_cache(thd, cache_mngr);
  }

  if (cache_mngr->trx_cache.empty())
  {
    /*
      we're here because cache_log was flushed in MYSQL_BIN_LOG::log_xid()
    */
    cache_mngr->reset_cache(&cache_mngr->trx_cache);
    DBUG_RETURN(error);
  }

  /*
    We commit the transaction if:
     - We are not in a transaction and committing a statement, or
     - We are in a transaction and a full transaction is committed.
    Otherwise, we accumulate the changes.
  */
  if (!error && ending_trans(thd, all))
    error= binlog_commit_flush_trx_cache(thd, cache_mngr);

  /*
    This is part of the stmt rollback.
  */
  if (!all)
    cache_mngr->trx_cache.set_prev_position(MY_OFF_T_UNDEF);

  DBUG_RETURN(error);
}

/**
  This function is called when a transaction or a statement is rolled back.

  @param hton  The binlog handlerton.
  @param thd   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction rollback, and
               @false otherwise.

  @see handlerton::rollback
*/
static int binlog_rollback(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("binlog_rollback");
  int error= 0;
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  DBUG_PRINT("debug", ("all: %s, all.modified_non_trans_table: %s, stmt.modified_non_trans_table: %s",
                       YESNO(all),
                       YESNO(thd->transaction.all.modified_non_trans_table),
                       YESNO(thd->transaction.stmt.modified_non_trans_table)));

  /*
    If an incident event is set we do not flush the content of the statement
    cache because it may be corrupted.
  */
  if (cache_mngr->stmt_cache.has_incident())
  {
    error= mysql_bin_log.write_incident(thd, TRUE);
    cache_mngr->reset_cache(&cache_mngr->stmt_cache);
  }
  else if (!cache_mngr->stmt_cache.empty())
  {
    error= binlog_commit_flush_stmt_cache(thd, cache_mngr);
  }

  if (cache_mngr->trx_cache.empty())
  {
    /*
      we're here because cache_log was flushed in MYSQL_BIN_LOG::log_xid()
    */
    cache_mngr->reset_cache(&cache_mngr->trx_cache);
    DBUG_RETURN(error);
  }

  if (mysql_bin_log.check_write_error(thd))
  {
    /*
      "all == true" means that a "rollback statement" triggered the error and
      this function was called. However, this must not happen as a rollback
      is written directly to the binary log. And in auto-commit mode, a single
      statement that is rolled back has the flag all == false.
    */
    DBUG_ASSERT(!all);
    /*
      We reach this point if the effect of a statement did not properly get into
      a cache and need to be rolled back.
    */
    error |= binlog_truncate_trx_cache(thd, cache_mngr, all);
  }
  else if (!error)
  {  
    /*
      We flush the cache wrapped in a beging/rollback if:
        . aborting a single or multi-statement transaction and;
        . the OPTION_KEEP_LOG is active or;
        . the format is STMT and a non-trans table was updated or;
        . the format is MIXED and a temporary non-trans table was
          updated or;
        . the format is MIXED, non-trans table was updated and
          aborting a single statement transaction;
    */
    if (ending_trans(thd, all) &&
        ((thd->variables.option_bits & OPTION_KEEP_LOG) ||
         (trans_has_updated_non_trans_table(thd) &&
          thd->variables.binlog_format == BINLOG_FORMAT_STMT) ||
         (cache_mngr->trx_cache.changes_to_non_trans_temp_table() &&
          thd->variables.binlog_format == BINLOG_FORMAT_MIXED) ||
         (trans_has_updated_non_trans_table(thd) &&
          ending_single_stmt_trans(thd,all) &&
          thd->variables.binlog_format == BINLOG_FORMAT_MIXED)))
      error= binlog_rollback_flush_trx_cache(thd, cache_mngr);
    /*
      Truncate the cache if:
        . aborting a single or multi-statement transaction or;
        . the OPTION_KEEP_LOG is not active and;
        . the format is not STMT or no non-trans table was
          updated and;
        . the format is not MIXED or no temporary non-trans table
          was updated.
    */
    else if (ending_trans(thd, all) ||
             (!(thd->variables.option_bits & OPTION_KEEP_LOG) &&
              (!stmt_has_updated_non_trans_table(thd) ||
               thd->variables.binlog_format != BINLOG_FORMAT_STMT) &&
              (!cache_mngr->trx_cache.changes_to_non_trans_temp_table() ||
               thd->variables.binlog_format != BINLOG_FORMAT_MIXED)))
      error= binlog_truncate_trx_cache(thd, cache_mngr, all);
  }

  /* 
    This is part of the stmt rollback.
  */
  if (!all)
    cache_mngr->trx_cache.set_prev_position(MY_OFF_T_UNDEF);

  DBUG_RETURN(error);
}

void MYSQL_BIN_LOG::set_write_error(THD *thd, bool is_transactional)
{
  DBUG_ENTER("MYSQL_BIN_LOG::set_write_error");

  write_error= 1;

  if (check_write_error(thd))
    DBUG_VOID_RETURN;

  if (my_errno == EFBIG)
  {
    if (is_transactional)
    {
      my_message(ER_TRANS_CACHE_FULL, ER(ER_TRANS_CACHE_FULL), MYF(MY_WME));
    }
    else
    {
      my_message(ER_STMT_CACHE_FULL, ER(ER_STMT_CACHE_FULL), MYF(MY_WME));
    }
  }
  else
  {
    my_error(ER_ERROR_ON_WRITE, MYF(MY_WME), name, errno);
  }

  DBUG_VOID_RETURN;
}

bool MYSQL_BIN_LOG::check_write_error(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::check_write_error");

  bool checked= FALSE;

  if (!thd->is_error())
    DBUG_RETURN(checked);

  switch (thd->stmt_da->sql_errno())
  {
    case ER_TRANS_CACHE_FULL:
    case ER_STMT_CACHE_FULL:
    case ER_ERROR_ON_WRITE:
    case ER_BINLOG_LOGGING_IMPOSSIBLE:
      checked= TRUE;
    break;
  }

  DBUG_RETURN(checked);
}

/**
  @note
  How do we handle this (unlikely but legal) case:
  @verbatim
    [transaction] + [update to non-trans table] + [rollback to savepoint] ?
  @endverbatim
  The problem occurs when a savepoint is before the update to the
  non-transactional table. Then when there's a rollback to the savepoint, if we
  simply truncate the binlog cache, we lose the part of the binlog cache where
  the update is. If we want to not lose it, we need to write the SAVEPOINT
  command and the ROLLBACK TO SAVEPOINT command to the binlog cache. The latter
  is easy: it's just write at the end of the binlog cache, but the former
  should be *inserted* to the place where the user called SAVEPOINT. The
  solution is that when the user calls SAVEPOINT, we write it to the binlog
  cache (so no need to later insert it). As transactions are never intermixed
  in the binary log (i.e. they are serialized), we won't have conflicts with
  savepoint names when using mysqlbinlog or in the slave SQL thread.
  Then when ROLLBACK TO SAVEPOINT is called, if we updated some
  non-transactional table, we don't truncate the binlog cache but instead write
  ROLLBACK TO SAVEPOINT to it; otherwise we truncate the binlog cache (which
  will chop the SAVEPOINT command from the binlog cache, which is good as in
  that case there is no need to have it in the binlog).
*/

static int binlog_savepoint_set(handlerton *hton, THD *thd, void *sv)
{
  DBUG_ENTER("binlog_savepoint_set");

  binlog_trans_log_savepos(thd, (my_off_t*) sv);
  /* Write it to the binary log */

  String log_query;
  if (log_query.append(STRING_WITH_LEN("SAVEPOINT ")) ||
      log_query.append("`") ||
      log_query.append(thd->lex->ident.str, thd->lex->ident.length) ||
      log_query.append("`"))
    DBUG_RETURN(1);
  int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
  Query_log_event qinfo(thd, log_query.c_ptr_safe(), log_query.length(),
                        TRUE, FALSE, TRUE, errcode);
  DBUG_RETURN(mysql_bin_log.write(&qinfo));
}

static int binlog_savepoint_rollback(handlerton *hton, THD *thd, void *sv)
{
  DBUG_ENTER("binlog_savepoint_rollback");

  /*
    Write ROLLBACK TO SAVEPOINT to the binlog cache if we have updated some
    non-transactional table. Otherwise, truncate the binlog cache starting
    from the SAVEPOINT command.
  */
  if (unlikely(trans_has_updated_non_trans_table(thd) ||
               (thd->variables.option_bits & OPTION_KEEP_LOG)))
  {
    String log_query;
    if (log_query.append(STRING_WITH_LEN("ROLLBACK TO ")) ||
        log_query.append("`") ||
        log_query.append(thd->lex->ident.str, thd->lex->ident.length) ||
        log_query.append("`"))
      DBUG_RETURN(1);
    int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
    Query_log_event qinfo(thd, log_query.c_ptr_safe(), log_query.length(),
                          TRUE, FALSE, TRUE, errcode);
    DBUG_RETURN(mysql_bin_log.write(&qinfo));
  }
  binlog_trans_log_truncate(thd, *(my_off_t*)sv);
  DBUG_RETURN(0);
}


int check_binlog_magic(IO_CACHE* log, const char** errmsg)
{
  char magic[4];
  DBUG_ASSERT(my_b_tell(log) == 0);

  if (my_b_read(log, (uchar*) magic, sizeof(magic)))
  {
    *errmsg = "I/O error reading the header from the binary log";
    sql_print_error("%s, errno=%d, io cache code=%d", *errmsg, my_errno,
		    log->error);
    return 1;
  }
  if (memcmp(magic, BINLOG_MAGIC, sizeof(magic)))
  {
    *errmsg = "Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL";
    return 1;
  }
  return 0;
}


File open_binlog(IO_CACHE *log, const char *log_file_name, const char **errmsg)
{
  File file;
  DBUG_ENTER("open_binlog");

  if ((file= mysql_file_open(key_file_binlog,
                             log_file_name, O_RDONLY | O_BINARY | O_SHARE,
                             MYF(MY_WME))) < 0)
  {
    sql_print_error("Failed to open log (file '%s', errno %d)",
                    log_file_name, my_errno);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
                    MYF(MY_WME|MY_DONT_CHECK_FILESIZE)))
  {
    sql_print_error("Failed to create a cache on log (file '%s')",
                    log_file_name);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  DBUG_RETURN(file);

err:
  if (file >= 0)
  {
    mysql_file_close(file, MYF(0));
    end_io_cache(log);
  }
  DBUG_RETURN(-1);
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
  if ((max_found == MAX_LOG_UNIQUE_FN_EXT))
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
                                 log_type_arg, io_cache_type_arg))
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
                             MYF(MY_WME | ME_WAITTANG))) < 0 ||
      init_io_cache(&log_file, file, IO_SIZE, io_cache_type,
                    mysql_file_tell(file, MYF(MY_WME)), 0,
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
  sql_print_error("Could not use %s for logging (error %d). \
Turning logging off for the whole duration of the MySQL server process. \
To turn it on again: fix the cause, \
shutdown the MySQL server and restart it.", name, errno);
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
{
  /*
    We don't want to initialize LOCK_Log here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  bzero((char*) &log_file, sizeof(log_file));
}

void MYSQL_LOG::init_pthread_objects()
{
  DBUG_ASSERT(inited == 0);
  inited= 1;
  mysql_mutex_init(key_LOG_LOCK_log, &LOCK_log, MY_MUTEX_INIT_SLOW);
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
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
    }

    if (mysql_file_close(log_file.file, MYF(MY_WME)) && ! write_error)
    {
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
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
                            uint user_host_len, int thread_id,
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

      /* command_type, thread_id */
      length= my_snprintf(buff, 32, "%5ld ", (long) thread_id);

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
    write_error= 1;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
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
      const uchar uh[]= "# User@Host: ";
      if (my_b_write(&log_file, uh, sizeof(uh) - 1))
        tmp_errno= errno;
      if (my_b_write(&log_file, (uchar*) user_host, user_host_len))
        tmp_errno= errno;
      if (my_b_write(&log_file, (uchar*) "\n", 1))
        tmp_errno= errno;
    }
    /* For slow query log */
    sprintf(query_time_buff, "%.6f", ulonglong2double(query_utime)/1000000.0);
    sprintf(lock_time_buff,  "%.6f", ulonglong2double(lock_utime)/1000000.0);
    if (my_b_printf(&log_file,
                    "# Query_time: %s  Lock_time: %s"
                    " Rows_sent: %lu  Rows_examined: %lu\n",
                    query_time_buff, lock_time_buff,
                    (ulong) thd->sent_row_count,
                    (ulong) thd->examined_row_count) == (uint) -1)
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
      my_b_write(&log_file, (uchar*) buff, buff_len);
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
        write_error= 1;
        sql_print_error(ER(ER_ERROR_ON_WRITE), name, error);
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
    strmake(buff, log_name, min(length, FN_REFLEN-1));
    return (const char*)buff;
  }
  return log_name;
}



MYSQL_BIN_LOG::MYSQL_BIN_LOG(uint *sync_period)
  :bytes_written(0), prepared_xids(0), file_id(1), open_count(1),
   need_start_event(TRUE),
   sync_period_ptr(sync_period),
   is_relay_log(0), signal_cnt(0),
   description_event_for_exec(0), description_event_for_queue(0)
{
  /*
    We don't want to initialize locks here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  index_file_name[0] = 0;
  bzero((char*) &index_file, sizeof(index_file));
  bzero((char*) &purge_index_file, sizeof(purge_index_file));
}

/* this is called only once */

void MYSQL_BIN_LOG::cleanup()
{
  DBUG_ENTER("cleanup");
  if (inited)
  {
    inited= 0;
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT);
    delete description_event_for_queue;
    delete description_event_for_exec;
    mysql_mutex_destroy(&LOCK_log);
    mysql_mutex_destroy(&LOCK_index);
    mysql_cond_destroy(&update_cond);
  }
  DBUG_VOID_RETURN;
}


/* Init binlog-specific vars */
void MYSQL_BIN_LOG::init(bool no_auto_events_arg, ulong max_size_arg)
{
  DBUG_ENTER("MYSQL_BIN_LOG::init");
  no_auto_events= no_auto_events_arg;
  max_size= max_size_arg;
  DBUG_PRINT("info",("max_size: %lu", max_size));
  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::init_pthread_objects()
{
  MYSQL_LOG::init_pthread_objects();
  mysql_mutex_init(m_key_LOCK_index, &LOCK_index, MY_MUTEX_INIT_SLOW);
  mysql_cond_init(m_key_update_cond, &update_cond, 0);
}


bool MYSQL_BIN_LOG::open_index_file(const char *index_file_name_arg,
                                    const char *log_name, bool need_mutex)
{
  File index_file_nr= -1;
  DBUG_ASSERT(!my_b_inited(&index_file));

  /*
    First open of this class instance
    Create an index file that will hold all file names uses for logging.
    Add new entries to the end of it.
  */
  myf opt= MY_UNPACK_FILENAME;
  if (!index_file_name_arg)
  {
    index_file_name_arg= log_name;    // Use same basename for index file
    opt= MY_UNPACK_FILENAME | MY_REPLACE_EXT;
  }
  fn_format(index_file_name, index_file_name_arg, mysql_data_home,
            ".index", opt);
  if ((index_file_nr= mysql_file_open(m_key_file_log_index,
                                      index_file_name,
                                      O_RDWR | O_CREAT | O_BINARY,
                                      MYF(MY_WME))) < 0 ||
       mysql_file_sync(index_file_nr, MYF(MY_WME)) ||
       init_io_cache(&index_file, index_file_nr,
                     IO_SIZE, WRITE_CACHE,
                     mysql_file_seek(index_file_nr, 0L, MY_SEEK_END, MYF(0)),
                                     0, MYF(MY_WME | MY_WAIT_IF_FULL)) ||
      DBUG_EVALUATE_IF("fault_injection_openning_index", 1, 0))
  {
    /*
      TODO: all operations creating/deleting the index file or a log, should
      call my_sync_dir() or my_sync_dir_by_file() to be durable.
      TODO: file creation should be done with mysql_file_create()
      not mysql_file_open().
    */
    if (index_file_nr >= 0)
      mysql_file_close(index_file_nr, MYF(0));
    return TRUE;
  }

#ifdef HAVE_REPLICATION
  /*
    Sync the index by purging any binary log file that is not registered.
    In other words, either purge binary log files that were removed from
    the index but not purged from the file system due to a crash or purge
    any binary log file that was created but not register in the index
    due to a crash.
  */

  if (set_purge_index_file_name(index_file_name_arg) ||
      open_purge_index_file(FALSE) ||
      purge_index_entry(NULL, NULL, need_mutex) ||
      close_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_recovering_index", 1, 0))
  {
    sql_print_error("MYSQL_BIN_LOG::open_index_file failed to sync the index "
                    "file.");
    return TRUE;
  }
#endif

  return FALSE;
}


/**
  Open a (new) binlog file.

  - Open the log file and the index file. Register the new
  file name in it
  - When calling this when the file is in use, you must have a locks
  on LOCK_log and LOCK_index.

  @retval
    0	ok
  @retval
    1	error
*/

bool MYSQL_BIN_LOG::open(const char *log_name,
                         enum_log_type log_type_arg,
                         const char *new_name,
                         enum cache_type io_cache_type_arg,
                         bool no_auto_events_arg,
                         ulong max_size_arg,
                         bool null_created_arg,
                         bool need_mutex)
{
  File file= -1;

  DBUG_ENTER("MYSQL_BIN_LOG::open");
  DBUG_PRINT("enter",("log_type: %d",(int) log_type_arg));

  if (init_and_set_log_file_name(log_name, new_name, log_type_arg,
                                 io_cache_type_arg))
  {
    sql_print_error("MSYQL_BIN_LOG::open failed to generate new file name.");
    DBUG_RETURN(1);
  }

#ifdef HAVE_REPLICATION
  if (open_purge_index_file(TRUE) ||
      register_create_index_entry(log_file_name) ||
      sync_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_registering_index", 1, 0))
  {
    /**
        TODO: although this was introduced to appease valgrind
              when injecting emulated faults using fault_injection_registering_index
              it may be good to consider what actually happens when
              open_purge_index_file succeeds but register or sync fails.

              Perhaps we might need the code below in MYSQL_LOG_BIN::cleanup
              for "real life" purposes as well? 
     */
    DBUG_EXECUTE_IF("fault_injection_registering_index", {
      if (my_b_inited(&purge_index_file))
      {
        end_io_cache(&purge_index_file);
        my_close(purge_index_file.file, MYF(0));
      }
    });

    sql_print_error("MSYQL_BIN_LOG::open failed to sync the index file.");
    DBUG_RETURN(1);
  }
  DBUG_EXECUTE_IF("crash_create_non_critical_before_update_index", DBUG_SUICIDE(););
#endif

  write_error= 0;

  /* open the main log file */
  if (MYSQL_LOG::open(
#ifdef HAVE_PSI_INTERFACE
                      m_key_file_log,
#endif
                      log_name, log_type_arg, new_name, io_cache_type_arg))
  {
#ifdef HAVE_REPLICATION
    close_purge_index_file();
#endif
    DBUG_RETURN(1);                            /* all warnings issued */
  }

  init(no_auto_events_arg, max_size_arg);

  open_count++;

  DBUG_ASSERT(log_type == LOG_BIN);

  {
    bool write_file_name_to_index_file=0;

    if (!my_b_filelength(&log_file))
    {
      /*
	The binary log file was empty (probably newly created)
	This is the normal case and happens when the user doesn't specify
	an extension for the binary log files.
	In this case we write a standard header to it.
      */
      if (my_b_safe_write(&log_file, (uchar*) BINLOG_MAGIC,
			  BIN_LOG_HEADER_SIZE))
        goto err;
      bytes_written+= BIN_LOG_HEADER_SIZE;
      write_file_name_to_index_file= 1;
    }

    if (need_start_event && !no_auto_events)
    {
      /*
        In 4.x we set need_start_event=0 here, but in 5.0 we want a Start event
        even if this is not the very first binlog.
      */
      Format_description_log_event s(BINLOG_VERSION);
      /*
        don't set LOG_EVENT_BINLOG_IN_USE_F for SEQ_READ_APPEND io_cache
        as we won't be able to reset it later
      */
      if (io_cache_type == WRITE_CACHE)
        s.flags|= LOG_EVENT_BINLOG_IN_USE_F;
      if (!s.is_valid())
        goto err;
      s.dont_set_created= null_created_arg;
      if (s.write(&log_file))
        goto err;
      bytes_written+= s.data_written;
    }
    if (description_event_for_queue &&
        description_event_for_queue->binlog_version>=4)
    {
      /*
        This is a relay log written to by the I/O slave thread.
        Write the event so that others can later know the format of this relay
        log.
        Note that this event is very close to the original event from the
        master (it has binlog version of the master, event types of the
        master), so this is suitable to parse the next relay log's event. It
        has been produced by
        Format_description_log_event::Format_description_log_event(char* buf,).
        Why don't we want to write the description_event_for_queue if this
        event is for format<4 (3.23 or 4.x): this is because in that case, the
        description_event_for_queue describes the data received from the
        master, but not the data written to the relay log (*conversion*),
        which is in format 4 (slave's).
      */
      /*
        Set 'created' to 0, so that in next relay logs this event does not
        trigger cleaning actions on the slave in
        Format_description_log_event::apply_event_impl().
      */
      description_event_for_queue->created= 0;
      /* Don't set log_pos in event header */
      description_event_for_queue->set_artificial_event();

      if (description_event_for_queue->write(&log_file))
        goto err;
      bytes_written+= description_event_for_queue->data_written;
    }
    if (flush_io_cache(&log_file) ||
        mysql_file_sync(log_file.file, MYF(MY_WME)))
      goto err;

    if (write_file_name_to_index_file)
    {
#ifdef HAVE_REPLICATION
      DBUG_EXECUTE_IF("crash_create_critical_before_update_index", DBUG_SUICIDE(););
#endif

      DBUG_ASSERT(my_b_inited(&index_file) != 0);
      reinit_io_cache(&index_file, WRITE_CACHE,
                      my_b_filelength(&index_file), 0, 0);
      /*
        As this is a new log file, we write the file name to the index
        file. As every time we write to the index file, we sync it.
      */
      if (DBUG_EVALUATE_IF("fault_injection_updating_index", 1, 0) ||
          my_b_write(&index_file, (uchar*) log_file_name,
                     strlen(log_file_name)) ||
          my_b_write(&index_file, (uchar*) "\n", 1) ||
          flush_io_cache(&index_file) ||
          mysql_file_sync(index_file.file, MYF(MY_WME)))
        goto err;

#ifdef HAVE_REPLICATION
      DBUG_EXECUTE_IF("crash_create_after_update_index", DBUG_SUICIDE(););
#endif
    }
  }
  log_state= LOG_OPENED;

#ifdef HAVE_REPLICATION
  close_purge_index_file();
#endif

  DBUG_RETURN(0);

err:
#ifdef HAVE_REPLICATION
  if (is_inited_purge_index_file())
    purge_index_entry(NULL, NULL, need_mutex);
  close_purge_index_file();
#endif
  sql_print_error("Could not use %s for logging (error %d). \
Turning logging off for the whole duration of the MySQL server process. \
To turn it on again: fix the cause, \
shutdown the MySQL server and restart it.", name, errno);
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);
  end_io_cache(&index_file);
  my_free(name);
  name= NULL;
  log_state= LOG_CLOSED;
  DBUG_RETURN(1);
}


int MYSQL_BIN_LOG::get_current_log(LOG_INFO* linfo)
{
  mysql_mutex_lock(&LOCK_log);
  int ret = raw_get_current_log(linfo);
  mysql_mutex_unlock(&LOCK_log);
  return ret;
}

int MYSQL_BIN_LOG::raw_get_current_log(LOG_INFO* linfo)
{
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_tell(&log_file);
  return 0;
}

/**
  Move all data up in a file in an filename index file.

    We do the copy outside of the IO_CACHE as the cache buffers would just
    make things slower and more complicated.
    In most cases the copy loop should only do one read.

  @param index_file			File to move
  @param offset			Move everything from here to beginning

  @note
    File will be truncated to be 'offset' shorter or filled up with newlines

  @retval
    0	ok
*/

#ifdef HAVE_REPLICATION

static bool copy_up_file_and_fill(IO_CACHE *index_file, my_off_t offset)
{
  int bytes_read;
  my_off_t init_offset= offset;
  File file= index_file->file;
  uchar io_buf[IO_SIZE*2];
  DBUG_ENTER("copy_up_file_and_fill");

  for (;; offset+= bytes_read)
  {
    mysql_file_seek(file, offset, MY_SEEK_SET, MYF(0));
    if ((bytes_read= (int) mysql_file_read(file, io_buf, sizeof(io_buf),
                                           MYF(MY_WME)))
	< 0)
      goto err;
    if (!bytes_read)
      break;					// end of file
    mysql_file_seek(file, offset-init_offset, MY_SEEK_SET, MYF(0));
    if (mysql_file_write(file, io_buf, bytes_read, MYF(MY_WME | MY_NABP)))
      goto err;
  }
  /* The following will either truncate the file or fill the end with \n' */
  if (mysql_file_chsize(file, offset - init_offset, '\n', MYF(MY_WME)) ||
      mysql_file_sync(file, MYF(MY_WME)))
    goto err;

  /* Reset data in old index cache */
  reinit_io_cache(index_file, READ_CACHE, (my_off_t) 0, 0, 1);
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}

#endif /* HAVE_REPLICATION */

/**
  Find the position in the log-index-file for the given log name.

  @param linfo		Store here the found log file name and position to
                       the NEXT log file name in the index file.
  @param log_name	Filename to find in the index file.
                       Is a null pointer if we want to read the first entry
  @param need_lock	Set this to 1 if the parent doesn't already have a
                       lock on LOCK_index

  @note
    On systems without the truncate function the file will end with one or
    more empty lines.  These will be ignored when reading the file.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_BIN_LOG::find_log_pos(LOG_INFO *linfo, const char *log_name,
			    bool need_lock)
{
  int error= 0;
  char *full_fname= linfo->log_file_name;
  char full_log_name[FN_REFLEN], fname[FN_REFLEN];
  uint log_name_len= 0, fname_len= 0;
  DBUG_ENTER("find_log_pos");
  full_log_name[0]= full_fname[0]= 0;

  /*
    Mutex needed because we need to make sure the file pointer does not
    move from under our feet
  */
  if (need_lock)
    mysql_mutex_lock(&LOCK_index);
  mysql_mutex_assert_owner(&LOCK_index);

  // extend relative paths for log_name to be searched
  if (log_name)
  {
    if(normalize_binlog_name(full_log_name, log_name, is_relay_log))
    {
      error= LOG_INFO_EOF;
      goto end;
    }
  }

  log_name_len= log_name ? (uint) strlen(full_log_name) : 0;
  DBUG_PRINT("enter", ("log_name: %s, full_log_name: %s", 
                       log_name ? log_name : "NULL", full_log_name));

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  for (;;)
  {
    uint length;
    my_off_t offset= my_b_tell(&index_file);

    DBUG_EXECUTE_IF("simulate_find_log_pos_error",
                    error=  LOG_INFO_EOF; break;);
    /* If we get 0 or 1 characters, this is the end of the file */
    if ((length= my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
    {
      /* Did not find the given entry; Return not found or error */
      error= !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
      break;
    }

    // extend relative paths and match against full path
    if (normalize_binlog_name(full_fname, fname, is_relay_log))
    {
      error= LOG_INFO_EOF;
      break;
    }
    fname_len= (uint) strlen(full_fname);

    // if the log entry matches, null string matching anything
    if (!log_name ||
	(log_name_len == fname_len-1 && full_fname[log_name_len] == '\n' &&
	 !memcmp(full_fname, full_log_name, log_name_len)))
    {
      DBUG_PRINT("info", ("Found log file entry"));
      full_fname[fname_len-1]= 0;			// remove last \n
      linfo->index_file_start_offset= offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
  }

end:
  if (need_lock)
    mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/**
  Find the position in the log-index-file for the given log name.

  @param
    linfo		Store here the next log file name and position to
			the file name after that.
  @param
    need_lock		Set this to 1 if the parent doesn't already have a
			lock on LOCK_index

  @note
    - Before calling this function, one has to call find_log_pos()
    to set up 'linfo'
    - Mutex needed because we need to make sure the file pointer does not move
    from under our feet

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_BIN_LOG::find_next_log(LOG_INFO* linfo, bool need_lock)
{
  int error= 0;
  uint length;
  char fname[FN_REFLEN];
  char *full_fname= linfo->log_file_name;

  if (need_lock)
    mysql_mutex_lock(&LOCK_index);
  mysql_mutex_assert_owner(&LOCK_index);

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, linfo->index_file_offset, 0,
			 0);

  linfo->index_file_start_offset= linfo->index_file_offset;
  if ((length=my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
  {
    error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }

  if (fname[0] != 0)
  {
    if(normalize_binlog_name(full_fname, fname, is_relay_log))
    {
      error= LOG_INFO_EOF;
      goto err;
    }
    length= strlen(full_fname);
  }

  full_fname[length-1]= 0;			// kill \n
  linfo->index_file_offset= my_b_tell(&index_file);

err:
  if (need_lock)
    mysql_mutex_unlock(&LOCK_index);
  return error;
}


/**
  Delete all logs refered to in the index file.
  Start writing to a new log file.

  The new index file will only contain this file.

  @param thd		Thread

  @note
    If not called from slave thread, write start event to new log

  @retval
    0	ok
  @retval
    1   error
*/

bool MYSQL_BIN_LOG::reset_logs(THD* thd)
{
  LOG_INFO linfo;
  bool error=0;
  int err;
  const char* save_name;
  DBUG_ENTER("reset_logs");

  ha_reset_logs(thd);

  /*
    The following mutex is needed to ensure that no threads call
    'delete thd' as we would then risk missing a 'rollback' from this
    thread. If the transaction involved MyISAM tables, it should go
    into binlog even on rollback.
  */
  mysql_mutex_lock(&LOCK_thread_count);

  /*
    We need to get both locks to be sure that no one is trying to
    write to the index log file.
  */
  mysql_mutex_lock(&LOCK_log);
  mysql_mutex_lock(&LOCK_index);

  /* Save variables so that we can reopen the log */
  save_name=name;
  name=0;					// Protect against free
  close(LOG_CLOSE_TO_BE_OPENED);

  /*
    First delete all old log files and then update the index file.
    As we first delete the log files and do not use sort of logging,
    a crash may lead to an inconsistent state where the index has
    references to non-existent files.

    We need to invert the steps and use the purge_index_file methods
    in order to make the operation safe.
  */

  if ((err= find_log_pos(&linfo, NullS, 0)) != 0)
  {
    uint errcode= purge_log_get_error_code(err);
    sql_print_error("Failed to locate old binlog or relay log files");
    my_message(errcode, ER(errcode), MYF(0));
    error= 1;
    goto err;
  }

  for (;;)
  {
    if ((error= my_delete_allow_opened(linfo.log_file_name, MYF(0))) != 0)
    {
      if (my_errno == ENOENT) 
      {
        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                            linfo.log_file_name);
        sql_print_information("Failed to delete file '%s'",
                              linfo.log_file_name);
        my_errno= 0;
        error= 0;
      }
      else
      {
        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_BINLOG_PURGE_FATAL_ERR,
                            "a problem with deleting %s; "
                            "consider examining correspondence "
                            "of your binlog index file "
                            "to the actual binlog files",
                            linfo.log_file_name);
        error= 1;
        goto err;
      }
    }
    if (find_next_log(&linfo, 0))
      break;
  }

  /* Start logging with a new file */
  close(LOG_CLOSE_INDEX | LOG_CLOSE_TO_BE_OPENED);
  if ((error= my_delete_allow_opened(index_file_name, MYF(0))))	// Reset (open will update)
  {
    if (my_errno == ENOENT) 
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                          index_file_name);
      sql_print_information("Failed to delete file '%s'",
                            index_file_name);
      my_errno= 0;
      error= 0;
    }
    else
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_BINLOG_PURGE_FATAL_ERR,
                          "a problem with deleting %s; "
                          "consider examining correspondence "
                          "of your binlog index file "
                          "to the actual binlog files",
                          index_file_name);
      error= 1;
      goto err;
    }
  }
  if (!thd->slave_thread)
    need_start_event=1;
  if (!open_index_file(index_file_name, 0, FALSE))
    if ((error= open(save_name, log_type, 0, io_cache_type, no_auto_events, max_size, 0, FALSE)))
      goto err;
  my_free((void *) save_name);

err:
  if (error == 1)
    name= const_cast<char*>(save_name);
  mysql_mutex_unlock(&LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_index);
  mysql_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


/**
  Delete relay log files prior to rli->group_relay_log_name
  (i.e. all logs which are not involved in a non-finished group
  (transaction)), remove them from the index file and start on next
  relay log.

  IMPLEMENTATION
  - Protects index file with LOCK_index
  - Delete relevant relay log files
  - Copy all file names after these ones to the front of the index file
  - If the OS has truncate, truncate the file, else fill it with \n'
  - Read the next file name from the index file and store in rli->linfo

  @param rli	       Relay log information
  @param included     If false, all relay logs that are strictly before
                      rli->group_relay_log_name are deleted ; if true, the
                      latter is deleted too (i.e. all relay logs
                      read by the SQL slave thread are deleted).

  @note
    - This is only called from the slave-execute thread when it has read
    all commands from a relay log and want to switch to a new relay log.
    - When this happens, we can be in an active transaction as
    a transaction can span over two relay logs
    (although it is always written as a single block to the master's binary
    log, hence cannot span over two master's binary logs).

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_SEEK	Could not allocate IO cache
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

#ifdef HAVE_REPLICATION

int MYSQL_BIN_LOG::purge_first_log(Relay_log_info* rli, bool included)
{
  int error;
  char *to_purge_if_included= NULL;
  DBUG_ENTER("purge_first_log");

  DBUG_ASSERT(is_open());
  DBUG_ASSERT(rli->slave_running == 1);
  DBUG_ASSERT(!strcmp(rli->linfo.log_file_name,rli->event_relay_log_name));

  mysql_mutex_lock(&LOCK_index);
  to_purge_if_included= my_strdup(rli->group_relay_log_name, MYF(0));

  /*
    Read the next log file name from the index file and pass it back to
    the caller.
  */
  if((error=find_log_pos(&rli->linfo, rli->event_relay_log_name, 0)) || 
     (error=find_next_log(&rli->linfo, 0)))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s included: %d",
                    error,
                    llstr(rli->linfo.index_file_offset,buff),
                    rli->event_relay_log_name,
                    included);
    goto err;
  }

  /*
    Reset rli's coordinates to the current log.
  */
  rli->event_relay_log_pos= BIN_LOG_HEADER_SIZE;
  strmake(rli->event_relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->event_relay_log_name)-1);

  /*
    If we removed the rli->group_relay_log_name file,
    we must update the rli->group* coordinates, otherwise do not touch it as the
    group's execution is not finished (e.g. COMMIT not executed)
  */
  if (included)
  {
    rli->group_relay_log_pos = BIN_LOG_HEADER_SIZE;
    strmake(rli->group_relay_log_name,rli->linfo.log_file_name,
            sizeof(rli->group_relay_log_name)-1);
    rli->notify_group_relay_log_name_update();
  }

  /* Store where we are in the new file for the execution thread */
  flush_relay_log_info(rli);

  DBUG_EXECUTE_IF("crash_before_purge_logs", DBUG_SUICIDE(););

  mysql_mutex_lock(&rli->log_space_lock);
  rli->relay_log.purge_logs(to_purge_if_included, included,
                            0, 0, &rli->log_space_total);
  mysql_mutex_unlock(&rli->log_space_lock);

  /*
    Ok to broadcast after the critical region as there is no risk of
    the mutex being destroyed by this thread later - this helps save
    context switches
  */
  mysql_cond_broadcast(&rli->log_space_cond);

  /*
   * Need to update the log pos because purge logs has been called 
   * after fetching initially the log pos at the begining of the method.
   */
  if((error=find_log_pos(&rli->linfo, rli->event_relay_log_name, 0)))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s included: %d",
                    error,
                    llstr(rli->linfo.index_file_offset,buff),
                    rli->group_relay_log_name,
                    included);
    goto err;
  }

  /* If included was passed, rli->linfo should be the first entry. */
  DBUG_ASSERT(!included || rli->linfo.index_file_start_offset == 0);

err:
  my_free(to_purge_if_included);
  mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}

/**
  Update log index_file.
*/

int MYSQL_BIN_LOG::update_log_index(LOG_INFO* log_info, bool need_update_threads)
{
  if (copy_up_file_and_fill(&index_file, log_info->index_file_start_offset))
    return LOG_INFO_IO;

  // now update offsets in index file for running threads
  if (need_update_threads)
    adjust_linfo_offsets(log_info->index_file_start_offset);
  return 0;
}

/**
  Remove all logs before the given log from disk and from the index file.

  @param to_log	      Delete all log file name before this file.
  @param included            If true, to_log is deleted too.
  @param need_mutex
  @param need_update_threads If we want to update the log coordinates of
                             all threads. False for relay logs, true otherwise.
  @param freed_log_space     If not null, decrement this variable of
                             the amount of log space freed

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF		to_log not found
    LOG_INFO_EMFILE             too many files opened
    LOG_INFO_FATAL              if any other than ENOENT error from
                                mysql_file_stat() or mysql_file_delete()
*/

int MYSQL_BIN_LOG::purge_logs(const char *to_log, 
                          bool included,
                          bool need_mutex, 
                          bool need_update_threads, 
                          ulonglong *decrease_log_space)
{
  int error= 0;
  bool exit_loop= 0;
  LOG_INFO log_info;
  THD *thd= current_thd;
  DBUG_ENTER("purge_logs");
  DBUG_PRINT("info",("to_log= %s",to_log));

  if (need_mutex)
    mysql_mutex_lock(&LOCK_index);
  if ((error=find_log_pos(&log_info, to_log, 0 /*no mutex*/))) 
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs was called with file %s not "
                    "listed in the index.", to_log);
    goto err;
  }

  if ((error= open_purge_index_file(TRUE)))
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to sync the index file.");
    goto err;
  }

  /*
    File name exists in index file; delete until we find this file
    or a file that is used.
  */
  if ((error=find_log_pos(&log_info, NullS, 0 /*no mutex*/)))
    goto err;
  while ((strcmp(to_log,log_info.log_file_name) || (exit_loop=included)) &&
         !is_active(log_info.log_file_name) &&
         !log_in_use(log_info.log_file_name))
  {
    if ((error= register_purge_index_entry(log_info.log_file_name)))
    {
      sql_print_error("MYSQL_BIN_LOG::purge_logs failed to copy %s to register file.",
                      log_info.log_file_name);
      goto err;
    }

    if (find_next_log(&log_info, 0) || exit_loop)
      break;
  }

  DBUG_EXECUTE_IF("crash_purge_before_update_index", DBUG_SUICIDE(););

  if ((error= sync_purge_index_file()))
  {
    sql_print_error("MSYQL_BIN_LOG::purge_logs failed to flush register file.");
    goto err;
  }

  /* We know how many files to delete. Update index file. */
  if ((error=update_log_index(&log_info, need_update_threads)))
  {
    sql_print_error("MSYQL_BIN_LOG::purge_logs failed to update the index file");
    goto err;
  }

  DBUG_EXECUTE_IF("crash_purge_critical_after_update_index", DBUG_SUICIDE(););

err:
  /* Read each entry from purge_index_file and delete the file. */
  if (is_inited_purge_index_file() &&
      (error= purge_index_entry(thd, decrease_log_space, FALSE)))
    sql_print_error("MSYQL_BIN_LOG::purge_logs failed to process registered files"
                    " that would be purged.");
  close_purge_index_file();

  DBUG_EXECUTE_IF("crash_purge_non_critical_after_update_index", DBUG_SUICIDE(););

  if (need_mutex)
    mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::set_purge_index_file_name(const char *base_file_name)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::set_purge_index_file_name");
  if (fn_format(purge_index_file_name, base_file_name, mysql_data_home,
                ".~rec~", MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH |
                              MY_REPLACE_EXT)) == NULL)
  {
    error= 1;
    sql_print_error("MYSQL_BIN_LOG::set_purge_index_file_name failed to set "
                      "file name.");
  }
  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::open_purge_index_file(bool destroy)
{
  int error= 0;
  File file= -1;

  DBUG_ENTER("MYSQL_BIN_LOG::open_purge_index_file");

  if (destroy)
    close_purge_index_file();

  if (!my_b_inited(&purge_index_file))
  {
    if ((file= my_open(purge_index_file_name, O_RDWR | O_CREAT | O_BINARY,
                       MYF(MY_WME | ME_WAITTANG))) < 0  ||
        init_io_cache(&purge_index_file, file, IO_SIZE,
                      (destroy ? WRITE_CACHE : READ_CACHE),
                      0, 0, MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)))
    {
      error= 1;
      sql_print_error("MYSQL_BIN_LOG::open_purge_index_file failed to open register "
                      " file.");
    }
  }
  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::close_purge_index_file()
{
  int error= 0;

  DBUG_ENTER("MYSQL_BIN_LOG::close_purge_index_file");

  if (my_b_inited(&purge_index_file))
  {
    end_io_cache(&purge_index_file);
    error= my_close(purge_index_file.file, MYF(0));
  }
  my_delete(purge_index_file_name, MYF(0));
  bzero((char*) &purge_index_file, sizeof(purge_index_file));

  DBUG_RETURN(error);
}

bool MYSQL_BIN_LOG::is_inited_purge_index_file()
{
  DBUG_ENTER("MYSQL_BIN_LOG::is_inited_purge_index_file");
  DBUG_RETURN (my_b_inited(&purge_index_file));
}

int MYSQL_BIN_LOG::sync_purge_index_file()
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::sync_purge_index_file");

  if ((error= flush_io_cache(&purge_index_file)) ||
      (error= my_sync(purge_index_file.file, MYF(MY_WME))))
    DBUG_RETURN(error);

  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::register_purge_index_entry(const char *entry)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::register_purge_index_entry");

  if ((error=my_b_write(&purge_index_file, (const uchar*)entry, strlen(entry))) ||
      (error=my_b_write(&purge_index_file, (const uchar*)"\n", 1)))
    DBUG_RETURN (error);

  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::register_create_index_entry(const char *entry)
{
  DBUG_ENTER("MYSQL_BIN_LOG::register_create_index_entry");
  DBUG_RETURN(register_purge_index_entry(entry));
}

int MYSQL_BIN_LOG::purge_index_entry(THD *thd, ulonglong *decrease_log_space,
                                     bool need_mutex)
{
  MY_STAT s;
  int error= 0;
  LOG_INFO log_info;
  LOG_INFO check_log_info;

  DBUG_ENTER("MYSQL_BIN_LOG:purge_index_entry");

  DBUG_ASSERT(my_b_inited(&purge_index_file));

  if ((error=reinit_io_cache(&purge_index_file, READ_CACHE, 0, 0, 0)))
  {
    sql_print_error("MSYQL_BIN_LOG::purge_index_entry failed to reinit register file "
                    "for read");
    goto err;
  }

  for (;;)
  {
    uint length;

    if ((length=my_b_gets(&purge_index_file, log_info.log_file_name,
                          FN_REFLEN)) <= 1)
    {
      if (purge_index_file.error)
      {
        error= purge_index_file.error;
        sql_print_error("MSYQL_BIN_LOG::purge_index_entry error %d reading from "
                        "register file.", error);
        goto err;
      }

      /* Reached EOF */
      break;
    }

    /* Get rid of the trailing '\n' */
    log_info.log_file_name[length-1]= 0;

    if (!mysql_file_stat(m_key_file_log, log_info.log_file_name, &s, MYF(0)))
    {
      if (my_errno == ENOENT) 
      {
        /*
          It's not fatal if we can't stat a log file that does not exist;
          If we could not stat, we won't delete.
        */
        if (thd)
        {
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                              log_info.log_file_name);
        }
        sql_print_information("Failed to execute mysql_file_stat on file '%s'",
			      log_info.log_file_name);
        my_errno= 0;
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_BINLOG_PURGE_FATAL_ERR,
                              "a problem with getting info on being purged %s; "
                              "consider examining correspondence "
                              "of your binlog index file "
                              "to the actual binlog files",
                              log_info.log_file_name);
        }
        else
        {
          sql_print_information("Failed to delete log file '%s'; "
                                "consider examining correspondence "
                                "of your binlog index file "
                                "to the actual binlog files",
                                log_info.log_file_name);
        }
        error= LOG_INFO_FATAL;
        goto err;
      }
    }
    else
    {
      if ((error= find_log_pos(&check_log_info, log_info.log_file_name, need_mutex)))
      {
        if (error != LOG_INFO_EOF)
        {
          if (thd)
          {
            push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                ER_BINLOG_PURGE_FATAL_ERR,
                                "a problem with deleting %s and "
                                "reading the binlog index file",
                                log_info.log_file_name);
          }
          else
          {
            sql_print_information("Failed to delete file '%s' and "
                                  "read the binlog index file",
                                  log_info.log_file_name);
          }
          goto err;
        }
           
        error= 0;
        if (!need_mutex)
        {
          /*
            This is to avoid triggering an error in NDB.
          */
          ha_binlog_index_purge_file(current_thd, log_info.log_file_name);
        }

        DBUG_PRINT("info",("purging %s",log_info.log_file_name));
        if (!my_delete(log_info.log_file_name, MYF(0)))
        {
          if (decrease_log_space)
            *decrease_log_space-= s.st_size;
        }
        else
        {
          if (my_errno == ENOENT)
          {
            if (thd)
            {
              push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                  ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                                  log_info.log_file_name);
            }
            sql_print_information("Failed to delete file '%s'",
                                  log_info.log_file_name);
            my_errno= 0;
          }
          else
          {
            if (thd)
            {
              push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                  ER_BINLOG_PURGE_FATAL_ERR,
                                  "a problem with deleting %s; "
                                  "consider examining correspondence "
                                  "of your binlog index file "
                                  "to the actual binlog files",
                                  log_info.log_file_name);
            }
            else
            {
              sql_print_information("Failed to delete file '%s'; "
                                    "consider examining correspondence "
                                    "of your binlog index file "
                                    "to the actual binlog files",
                                    log_info.log_file_name);
            }
            if (my_errno == EMFILE)
            {
              DBUG_PRINT("info",
                         ("my_errno: %d, set ret = LOG_INFO_EMFILE", my_errno));
              error= LOG_INFO_EMFILE;
              goto err;
            }
            error= LOG_INFO_FATAL;
            goto err;
          }
        }
      }
    }
  }

err:
  DBUG_RETURN(error);
}

/**
  Remove all logs before the given file date from disk and from the
  index file.

  @param thd		Thread pointer
  @param purge_time	Delete all log files before given date.

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0				ok
  @retval
    LOG_INFO_PURGE_NO_ROTATE	Binary file that can't be rotated
    LOG_INFO_FATAL              if any other than ENOENT error from
                                mysql_file_stat() or mysql_file_delete()
*/

int MYSQL_BIN_LOG::purge_logs_before_date(time_t purge_time)
{
  int error;
  char to_log[FN_REFLEN];
  LOG_INFO log_info;
  MY_STAT stat_area;
  THD *thd= current_thd;
  
  DBUG_ENTER("purge_logs_before_date");

  mysql_mutex_lock(&LOCK_index);
  to_log[0]= 0;

  if ((error=find_log_pos(&log_info, NullS, 0 /*no mutex*/)))
    goto err;

  while (strcmp(log_file_name, log_info.log_file_name) &&
	 !is_active(log_info.log_file_name) &&
         !log_in_use(log_info.log_file_name))
  {
    if (!mysql_file_stat(m_key_file_log,
                         log_info.log_file_name, &stat_area, MYF(0)))
    {
      if (my_errno == ENOENT) 
      {
        /*
          It's not fatal if we can't stat a log file that does not exist.
        */
        my_errno= 0;
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_BINLOG_PURGE_FATAL_ERR,
                              "a problem with getting info on being purged %s; "
                              "consider examining correspondence "
                              "of your binlog index file "
                              "to the actual binlog files",
                              log_info.log_file_name);
        }
        else
        {
          sql_print_information("Failed to delete log file '%s'",
                                log_info.log_file_name);
        }
        error= LOG_INFO_FATAL;
        goto err;
      }
    }
    else
    {
      if (stat_area.st_mtime < purge_time) 
        strmake(to_log, 
                log_info.log_file_name, 
                sizeof(log_info.log_file_name) - 1);
      else
        break;
    }
    if (find_next_log(&log_info, 0))
      break;
  }

  error= (to_log[0] ? purge_logs(to_log, 1, 0, 1, (ulonglong *) 0) : 0);

err:
  mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}
#endif /* HAVE_REPLICATION */


/**
  Create a new log file name.

  @param buf		buf of at least FN_REFLEN where new name is stored

  @note
    If file name will be longer then FN_REFLEN it will be truncated
*/

void MYSQL_BIN_LOG::make_log_name(char* buf, const char* log_ident)
{
  uint dir_len = dirname_length(log_file_name); 
  if (dir_len >= FN_REFLEN)
    dir_len=FN_REFLEN-1;
  strnmov(buf, log_file_name, dir_len);
  strmake(buf+dir_len, log_ident, FN_REFLEN - dir_len -1);
}


/**
  Check if we are writing/reading to the given log file.
*/

bool MYSQL_BIN_LOG::is_active(const char *log_file_name_arg)
{
  return !strcmp(log_file_name, log_file_name_arg);
}


/*
  Wrappers around new_file_impl to avoid using argument
  to control locking. The argument 1) less readable 2) breaks
  incapsulation 3) allows external access to the class without
  a lock (which is not possible with private new_file_without_locking
  method).

  @retval
    nonzero - error
*/

int MYSQL_BIN_LOG::new_file()
{
  return new_file_impl(1);
}

/*
  @retval
    nonzero - error
 */
int MYSQL_BIN_LOG::new_file_without_locking()
{
  return new_file_impl(0);
}


/**
  Start writing to a new log file or reopen the old file.

  @param need_lock		Set to 1 if caller has not locked LOCK_log

  @retval
    nonzero - error

  @note
    The new file name is stored last in the index file
*/

int MYSQL_BIN_LOG::new_file_impl(bool need_lock)
{
  int error= 0, close_on_error= FALSE;
  char new_name[FN_REFLEN], *new_name_ptr, *old_name, *file_to_open;

  DBUG_ENTER("MYSQL_BIN_LOG::new_file_impl");
  if (!is_open())
  {
    DBUG_PRINT("info",("log is closed"));
    DBUG_RETURN(error);
  }

  if (need_lock)
    mysql_mutex_lock(&LOCK_log);
  mysql_mutex_lock(&LOCK_index);

  mysql_mutex_assert_owner(&LOCK_log);
  mysql_mutex_assert_owner(&LOCK_index);

  /*
    if binlog is used as tc log, be sure all xids are "unlogged",
    so that on recover we only need to scan one - latest - binlog file
    for prepared xids. As this is expected to be a rare event,
    simple wait strategy is enough. We're locking LOCK_log to be sure no
    new Xid_log_event's are added to the log (and prepared_xids is not
    increased), and waiting on COND_prep_xids for late threads to
    catch up.
  */
  if (prepared_xids)
  {
    tc_log_page_waits++;
    mysql_mutex_lock(&LOCK_prep_xids);
    while (prepared_xids) {
      DBUG_PRINT("info", ("prepared_xids=%lu", prepared_xids));
      mysql_cond_wait(&COND_prep_xids, &LOCK_prep_xids);
    }
    mysql_mutex_unlock(&LOCK_prep_xids);
  }

  /* Reuse old name if not binlog and not update log */
  new_name_ptr= name;

  /*
    If user hasn't specified an extension, generate a new log name
    We have to do this here and not in open as we want to store the
    new file name in the current binary log file.
  */
  if ((error= generate_new_name(new_name, name)))
    goto end;
  new_name_ptr=new_name;

  if (log_type == LOG_BIN)
  {
    if (!no_auto_events)
    {
      /*
        We log the whole file name for log file as the user may decide
        to change base names at some point.
      */
      Rotate_log_event r(new_name+dirname_length(new_name),
                         0, LOG_EVENT_OFFSET, is_relay_log ? Rotate_log_event::RELAY_LOG : 0);
      if(DBUG_EVALUATE_IF("fault_injection_new_file_rotate_event", (error=close_on_error=TRUE), FALSE) ||
         (error= r.write(&log_file)))
      {
        DBUG_EXECUTE_IF("fault_injection_new_file_rotate_event", errno=2;);
        close_on_error= TRUE;
        my_printf_error(ER_ERROR_ON_WRITE, ER(ER_CANT_OPEN_FILE), MYF(ME_FATALERROR), name, errno);
        goto end;
      }
      bytes_written += r.data_written;
    }
    /*
      Update needs to be signalled even if there is no rotate event
      log rotation should give the waiting thread a signal to
      discover EOF and move on to the next log.
    */
    signal_update();
  }
  old_name=name;
  name=0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED | LOG_CLOSE_INDEX);

  /*
     Note that at this point, log_state != LOG_CLOSED (important for is_open()).
  */

  /*
     new_file() is only used for rotation (in FLUSH LOGS or because size >
     max_binlog_size or max_relay_log_size).
     If this is a binary log, the Format_description_log_event at the beginning of
     the new file should have created=0 (to distinguish with the
     Format_description_log_event written at server startup, which should
     trigger temp tables deletion on slaves.
  */

  /* reopen index binlog file, BUG#34582 */
  file_to_open= index_file_name;
  error= open_index_file(index_file_name, 0, FALSE);
  if (!error)
  {
    /* reopen the binary log file. */
    file_to_open= new_name_ptr;
    error= open(old_name, log_type, new_name_ptr, io_cache_type,
                no_auto_events, max_size, 1, FALSE);
  }

  /* handle reopening errors */
  if (error)
  {
    my_printf_error(ER_CANT_OPEN_FILE, ER(ER_CANT_OPEN_FILE), 
                    MYF(ME_FATALERROR), file_to_open, error);
    close_on_error= TRUE;
  }

  my_free(old_name);

end:

  if (error && close_on_error /* rotate or reopen failed */)
  {
    /* 
      Close whatever was left opened.

      We are keeping the behavior as it exists today, ie,
      we disable logging and move on (see: BUG#51014).

      TODO: as part of WL#1790 consider other approaches:
       - kill mysql (safety);
       - try multiple locations for opening a log file;
       - switch server to protected/readonly mode
       - ...
    */
    close(LOG_CLOSE_INDEX);
    sql_print_error("Could not open %s for logging (error %d). "
                     "Turning logging off for the whole duration "
                     "of the MySQL server process. To turn it on "
                     "again: fix the cause, shutdown the MySQL "
                     "server and restart it.", 
                     new_name_ptr, errno);
  }

  if (need_lock)
    mysql_mutex_unlock(&LOCK_log);
  mysql_mutex_unlock(&LOCK_index);

  DBUG_RETURN(error);
}


bool MYSQL_BIN_LOG::append(Log_event* ev)
{
  bool error = 0;
  mysql_mutex_lock(&LOCK_log);
  DBUG_ENTER("MYSQL_BIN_LOG::append");

  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);
  /*
    Log_event::write() is smart enough to use my_b_write() or
    my_b_append() depending on the kind of cache we have.
  */
  if (ev->write(&log_file))
  {
    error=1;
    goto err;
  }
  bytes_written+= ev->data_written;
  DBUG_PRINT("info",("max_size: %lu",max_size));
  if (flush_and_sync(0))
    goto err;
  if ((uint) my_b_append_tell(&log_file) > max_size)
    error= new_file_without_locking();
err:
  mysql_mutex_unlock(&LOCK_log);
  signal_update();				// Safe as we don't call close
  DBUG_RETURN(error);
}


bool MYSQL_BIN_LOG::appendv(const char* buf, uint len,...)
{
  bool error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::appendv");
  va_list(args);
  va_start(args,len);

  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);

  mysql_mutex_assert_owner(&LOCK_log);
  do
  {
    if (my_b_append(&log_file,(uchar*) buf,len))
    {
      error= 1;
      goto err;
    }
    bytes_written += len;
  } while ((buf=va_arg(args,const char*)) && (len=va_arg(args,uint)));
  DBUG_PRINT("info",("max_size: %lu",max_size));
  if (flush_and_sync(0))
    goto err;
  if ((uint) my_b_append_tell(&log_file) > max_size)
    error= new_file_without_locking();
err:
  if (!error)
    signal_update();
  DBUG_RETURN(error);
}

bool MYSQL_BIN_LOG::flush_and_sync(bool *synced)
{
  int err=0, fd=log_file.file;
  if (synced)
    *synced= 0;
  mysql_mutex_assert_owner(&LOCK_log);
  if (flush_io_cache(&log_file))
    return 1;
  uint sync_period= get_sync_period();
  if (sync_period && ++sync_counter >= sync_period)
  {
    sync_counter= 0;
    err= mysql_file_sync(fd, MYF(MY_WME));
    if (synced)
      *synced= 1;
  }
  return err;
}

void MYSQL_BIN_LOG::start_union_events(THD *thd, query_id_t query_id_param)
{
  DBUG_ASSERT(!thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= TRUE;
  thd->binlog_evt_union.unioned_events= FALSE;
  thd->binlog_evt_union.unioned_events_trans= FALSE;
  thd->binlog_evt_union.first_query_id= query_id_param;
}

void MYSQL_BIN_LOG::stop_union_events(THD *thd)
{
  DBUG_ASSERT(thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= FALSE;
}

bool MYSQL_BIN_LOG::is_query_in_union(THD *thd, query_id_t query_id_param)
{
  return (thd->binlog_evt_union.do_union && 
          query_id_param >= thd->binlog_evt_union.first_query_id);
}

/** 
  This function checks if a transactional table was updated by the
  current transaction.

  @param thd The client thread that executed the current statement.
  @return
    @c true if a transactional table was updated, @c false otherwise.
*/
bool
trans_has_updated_trans_table(const THD* thd)
{
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  return (cache_mngr ? !cache_mngr->trx_cache.empty() : 0);
}

/** 
  This function checks if a transactional table was updated by the
  current statement.

  @param thd The client thread that executed the current statement.
  @return
    @c true if a transactional table was updated, @c false otherwise.
*/
bool
stmt_has_updated_trans_table(const THD *thd)
{
  Ha_trx_info *ha_info;

  for (ha_info= thd->transaction.stmt.ha_list; ha_info;
       ha_info= ha_info->next())
  {
    if (ha_info->is_trx_read_write() && ha_info->ht() != binlog_hton)
      return (TRUE);
  }
  return (FALSE);
}

/** 
  This function checks if either a trx-cache or a non-trx-cache should
  be used. If @c bin_log_direct_non_trans_update is active or the format
  is either MIXED or ROW, the cache to be used depends on the flag @c
  is_transactional. 

  On the other hand, if binlog_format is STMT or direct option is
  OFF, the trx-cache should be used if and only if the statement is
  transactional or the trx-cache is not empty. Otherwise, the
  non-trx-cache should be used.

  @param thd              The client thread.
  @param is_transactional The changes are related to a trx-table.
  @return
    @c true if a trx-cache should be used, @c false otherwise.
*/
bool use_trans_cache(const THD* thd, bool is_transactional)
{
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  return
    ((thd->is_current_stmt_binlog_format_row() ||
     thd->variables.binlog_direct_non_trans_update) ? is_transactional :
     (is_transactional || !cache_mngr->trx_cache.empty()));
}

/**
  This function checks if a transaction, either a multi-statement
  or a single statement transaction is about to commit or not.

  @param thd The client thread that executed the current statement.
  @param all Committing a transaction (i.e. TRUE) or a statement
             (i.e. FALSE).
  @return
    @c true if committing a transaction, otherwise @c false.
*/
bool ending_trans(THD* thd, const bool all)
{
  return (all || ending_single_stmt_trans(thd, all));
}

/**
  This function checks if a single statement transaction is about
  to commit or not.

  @param thd The client thread that executed the current statement.
  @param all Committing a transaction (i.e. TRUE) or a statement
             (i.e. FALSE).
  @return
    @c true if committing a single statement transaction, otherwise
    @c false.
*/
bool ending_single_stmt_trans(THD* thd, const bool all)
{
  return (!all && !thd->in_multi_stmt_transaction_mode());
}

/**
  This function checks if a non-transactional table was updated by
  the current transaction.

  @param thd The client thread that executed the current statement.
  @return
    @c true if a non-transactional table was updated, @c false
    otherwise.
*/
bool trans_has_updated_non_trans_table(const THD* thd)
{
  return (thd->transaction.all.modified_non_trans_table ||
          thd->transaction.stmt.modified_non_trans_table);
}

/**
  This function checks if a non-transactional table was updated by the
  current statement.

  @param thd The client thread that executed the current statement.
  @return
    @c true if a non-transactional table was updated, @c false otherwise.
*/
bool stmt_has_updated_non_trans_table(const THD* thd)
{
  return (thd->transaction.stmt.modified_non_trans_table);
}

/*
  These functions are placed in this file since they need access to
  binlog_hton, which has internal linkage.
*/

int THD::binlog_setup_trx_data()
{
  DBUG_ENTER("THD::binlog_setup_trx_data");
  binlog_cache_mngr *cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);

  if (cache_mngr)
    DBUG_RETURN(0);                             // Already set up

  cache_mngr= (binlog_cache_mngr*) my_malloc(sizeof(binlog_cache_mngr), MYF(MY_ZEROFILL));
  if (!cache_mngr ||
      open_cached_file(&cache_mngr->stmt_cache.cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_stmt_cache_size, MYF(MY_WME)) ||
      open_cached_file(&cache_mngr->trx_cache.cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_cache_size, MYF(MY_WME)))
  {
    my_free(cache_mngr);
    DBUG_RETURN(1);                      // Didn't manage to set it up
  }
  thd_set_ha_data(this, binlog_hton, cache_mngr);

  cache_mngr= new (thd_get_ha_data(this, binlog_hton))
              binlog_cache_mngr(max_binlog_stmt_cache_size,
                                max_binlog_cache_size,
                                &binlog_stmt_cache_use,
                                &binlog_stmt_cache_disk_use,
                                &binlog_cache_use,
                                &binlog_cache_disk_use);
  DBUG_RETURN(0);
}

/*
  Function to start a statement and optionally a transaction for the
  binary log.

  SYNOPSIS
    binlog_start_trans_and_stmt()

  DESCRIPTION

    This function does three things:
    - Start a transaction if not in autocommit mode or if a BEGIN
      statement has been seen.

    - Start a statement transaction to allow us to truncate the cache.

    - Save the currrent binlog position so that we can roll back the
      statement by truncating the cache.

      We only update the saved position if the old one was undefined,
      the reason is that there are some cases (e.g., for CREATE-SELECT)
      where the position is saved twice (e.g., both in
      select_create::prepare() and THD::binlog_write_table_map()) , but
      we should use the first. This means that calls to this function
      can be used to start the statement before the first table map
      event, to include some extra events.
 */

void
THD::binlog_start_trans_and_stmt()
{
  binlog_cache_mngr *cache_mngr= (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);
  DBUG_ENTER("binlog_start_trans_and_stmt");
  DBUG_PRINT("enter", ("cache_mngr: %p  cache_mngr->trx_cache.get_prev_position(): %lu",
                       cache_mngr,
                       (cache_mngr ? (ulong) cache_mngr->trx_cache.get_prev_position() :
                        (ulong) 0)));

  if (cache_mngr == NULL ||
      cache_mngr->trx_cache.get_prev_position() == MY_OFF_T_UNDEF)
  {
    this->binlog_set_stmt_begin();
    if (in_multi_stmt_transaction_mode())
      trans_register_ha(this, TRUE, binlog_hton);
    trans_register_ha(this, FALSE, binlog_hton);
    /*
      Mark statement transaction as read/write. We never start
      a binary log transaction and keep it read-only,
      therefore it's best to mark the transaction read/write just
      at the same time we start it.
      Not necessary to mark the normal transaction read/write
      since the statement-level flag will be propagated automatically
      inside ha_commit_trans.
    */
    ha_data[binlog_hton->slot].ha_info[0].set_trx_read_write();
  }
  DBUG_VOID_RETURN;
}

void THD::binlog_set_stmt_begin() {
  binlog_cache_mngr *cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);

  /*
    The call to binlog_trans_log_savepos() might create the cache_mngr
    structure, if it didn't exist before, so we save the position
    into an auto variable and then write it into the transaction
    data for the binary log (i.e., cache_mngr).
  */
  my_off_t pos= 0;
  binlog_trans_log_savepos(this, &pos);
  cache_mngr= (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);
  cache_mngr->trx_cache.set_prev_position(pos);
}


/**
  This function writes a table map to the binary log. 
  Note that in order to keep the signature uniform with related methods,
  we use a redundant parameter to indicate whether a transactional table
  was changed or not.
 
  @param table             a pointer to the table.
  @param is_transactional  @c true indicates a transactional table,
                           otherwise @c false a non-transactional.
  @return
    nonzero if an error pops up when writing the table map event.
*/
int THD::binlog_write_table_map(TABLE *table, bool is_transactional)
{
  int error;
  DBUG_ENTER("THD::binlog_write_table_map");
  DBUG_PRINT("enter", ("table: 0x%lx  (%s: #%lu)",
                       (long) table, table->s->table_name.str,
                       table->s->table_map_id));

  /* Pre-conditions */
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());
  DBUG_ASSERT(table->s->table_map_id != ULONG_MAX);

  Table_map_log_event
    the_event(this, table, table->s->table_map_id, is_transactional);

  if (binlog_table_maps == 0)
    binlog_start_trans_and_stmt();

  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);

  IO_CACHE *file=
    cache_mngr->get_binlog_cache_log(use_trans_cache(this, is_transactional));
  if ((error= the_event.write(file)))
    DBUG_RETURN(error);

  binlog_table_maps++;
  DBUG_RETURN(0);
}

/**
  This function retrieves a pending row event from a cache which is
  specified through the parameter @c is_transactional. Respectively, when it
  is @c true, the pending event is returned from the transactional cache.
  Otherwise from the non-transactional cache.

  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
  @return
    The row event if any. 
*/
Rows_log_event*
THD::binlog_get_pending_rows_event(bool is_transactional) const
{
  Rows_log_event* rows= NULL;
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);

  /*
    This is less than ideal, but here's the story: If there is no cache_mngr,
    prepare_pending_rows_event() has never been called (since the cache_mngr
    is set up there). In that case, we just return NULL.
   */
  if (cache_mngr)
  {
    binlog_cache_data *cache_data=
      cache_mngr->get_binlog_cache_data(use_trans_cache(this, is_transactional));

    rows= cache_data->pending();
  }
  return (rows);
}

/**
  This function stores a pending row event into a cache which is specified
  through the parameter @c is_transactional. Respectively, when it is @c
  true, the pending event is stored into the transactional cache. Otherwise
  into the non-transactional cache.

  @param evt               a pointer to the row event.
  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
*/
void
THD::binlog_set_pending_rows_event(Rows_log_event* ev, bool is_transactional)
{
  if (thd_get_ha_data(this, binlog_hton) == NULL)
    binlog_setup_trx_data();

  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(this, binlog_hton);

  DBUG_ASSERT(cache_mngr);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(use_trans_cache(this, is_transactional));

  cache_data->set_pending(ev);
}


/**
  This function removes the pending rows event, discarding any outstanding
  rows. If there is no pending rows event available, this is effectively a
  no-op.

  @param thd               a pointer to the user thread.
  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
*/
int
MYSQL_BIN_LOG::remove_pending_rows_event(THD *thd, bool is_transactional)
{
  DBUG_ENTER("MYSQL_BIN_LOG::remove_pending_rows_event");

  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  DBUG_ASSERT(cache_mngr);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(use_trans_cache(thd, is_transactional));

  if (Rows_log_event* pending= cache_data->pending())
  {
    delete pending;
    cache_data->set_pending(NULL);
  }

  DBUG_RETURN(0);
}

/*
  Moves the last bunch of rows from the pending Rows event to a cache (either
  transactional cache if is_transaction is @c true, or the non-transactional
  cache otherwise. Sets a new pending event.

  @param thd               a pointer to the user thread.
  @param evt               a pointer to the row event.
  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
*/
int
MYSQL_BIN_LOG::flush_and_set_pending_rows_event(THD *thd,
                                                Rows_log_event* event,
                                                bool is_transactional)
{
  DBUG_ENTER("MYSQL_BIN_LOG::flush_and_set_pending_rows_event(event)");
  DBUG_ASSERT(mysql_bin_log.is_open());
  DBUG_PRINT("enter", ("event: 0x%lx", (long) event));

  int error= 0;
  binlog_cache_mngr *const cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

  DBUG_ASSERT(cache_mngr);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(use_trans_cache(thd, is_transactional));

  DBUG_PRINT("info", ("cache_mngr->pending(): 0x%lx", (long) cache_data->pending()));

  if (Rows_log_event* pending= cache_data->pending())
  {
    IO_CACHE *file= &cache_data->cache_log;

    /*
      Write pending event to the cache.
    */
    if (pending->write(file))
    {
      set_write_error(thd, is_transactional);
      if (check_write_error(thd) && cache_data &&
          stmt_has_updated_non_trans_table(thd))
        cache_data->set_incident();
      DBUG_RETURN(1);
    }

    delete pending;
  }

  thd->binlog_set_pending_rows_event(event, is_transactional);

  DBUG_RETURN(error);
}

/**
  Write an event to the binary log.
*/

bool MYSQL_BIN_LOG::write(Log_event *event_info)
{
  THD *thd= event_info->thd;
  bool error= 1;
  DBUG_ENTER("MYSQL_BIN_LOG::write(Log_event *)");
  binlog_cache_data *cache_data= 0;
  bool is_trans_cache= FALSE;

  if (thd->binlog_evt_union.do_union)
  {
    /*
      In Stored function; Remember that function call caused an update.
      We will log the function call to the binary log on function exit
    */
    thd->binlog_evt_union.unioned_events= TRUE;
    thd->binlog_evt_union.unioned_events_trans |=
      event_info->use_trans_cache();
    DBUG_RETURN(0);
  }

  /*
    We only end the statement if we are in a top-level statement.  If
    we are inside a stored function, we do not end the statement since
    this will close all tables on the slave.
  */
  bool const end_stmt=
    thd->locked_tables_mode && thd->lex->requires_prelocking();
  if (thd->binlog_flush_pending_rows_event(end_stmt,
                                           event_info->use_trans_cache()))
    DBUG_RETURN(error);

  /*
     In most cases this is only called if 'is_open()' is true; in fact this is
     mostly called if is_open() *was* true a few instructions before, but it
     could have changed since.
  */
  if (likely(is_open()))
  {
#ifdef HAVE_REPLICATION
    /*
      In the future we need to add to the following if tests like
      "do the involved tables match (to be implemented)
      binlog_[wild_]{do|ignore}_table?" (WL#1049)"
    */
    const char *local_db= event_info->get_db();
    if ((thd && !(thd->variables.option_bits & OPTION_BIN_LOG)) ||
	(thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT &&
         thd->lex->sql_command != SQLCOM_SAVEPOINT &&
         !binlog_filter->db_ok(local_db)))
      DBUG_RETURN(0);
#endif /* HAVE_REPLICATION */

    IO_CACHE *file= NULL;

    if (event_info->use_direct_logging())
    {
      file= &log_file;
      mysql_mutex_lock(&LOCK_log);
    }
    else
    {
      if (thd->binlog_setup_trx_data())
        goto err;

      binlog_cache_mngr *const cache_mngr=
        (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);

      is_trans_cache= use_trans_cache(thd, event_info->use_trans_cache());
      file= cache_mngr->get_binlog_cache_log(is_trans_cache);
      cache_data= cache_mngr->get_binlog_cache_data(is_trans_cache);

      if (thd->lex->stmt_accessed_non_trans_temp_table())
        cache_data->set_changes_to_non_trans_temp_table();

      thd->binlog_start_trans_and_stmt();
    }
    DBUG_PRINT("info",("event type: %d",event_info->get_type_code()));

    /*
       No check for auto events flag here - this write method should
       never be called if auto-events are enabled.

       Write first log events which describe the 'run environment'
       of the SQL command. If row-based binlogging, Insert_id, Rand
       and other kind of "setting context" events are not needed.
    */
    if (thd)
    {
      if (!thd->is_current_stmt_binlog_format_row())
      {
        if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
        {
          Intvar_log_event e(thd,(uchar) LAST_INSERT_ID_EVENT,
                             thd->first_successful_insert_id_in_prev_stmt_for_binlog);
          if (e.write(file))
            goto err;
        }
        if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0)
        {
          DBUG_PRINT("info",("number of auto_inc intervals: %u",
                             thd->auto_inc_intervals_in_cur_stmt_for_binlog.
                             nb_elements()));
          Intvar_log_event e(thd, (uchar) INSERT_ID_EVENT,
                             thd->auto_inc_intervals_in_cur_stmt_for_binlog.
                             minimum());
          if (e.write(file))
            goto err;
        }
        if (thd->rand_used)
        {
          Rand_log_event e(thd,thd->rand_saved_seed1,thd->rand_saved_seed2);
          if (e.write(file))
            goto err;
        }
        if (thd->user_var_events.elements)
        {
          for (uint i= 0; i < thd->user_var_events.elements; i++)
          {
            BINLOG_USER_VAR_EVENT *user_var_event;
            get_dynamic(&thd->user_var_events,(uchar*) &user_var_event, i);

            /* setting flags for user var log event */
            uchar flags= User_var_log_event::UNDEF_F;
            if (user_var_event->unsigned_flag)
              flags|= User_var_log_event::UNSIGNED_F;

            User_var_log_event e(thd, user_var_event->user_var_event->name.str,
                                 user_var_event->user_var_event->name.length,
                                 user_var_event->value,
                                 user_var_event->length,
                                 user_var_event->type,
                                 user_var_event->charset_number,
                                 flags);
            if (e.write(file))
              goto err;
          }
        }
      }
    }

    /*
      Write the event.
    */
    if (event_info->write(file) ||
        DBUG_EVALUATE_IF("injecting_fault_writing", 1, 0))
      goto err;

    error= 0;
err:
    if (event_info->use_direct_logging())
    {
      if (!error)
      {
        bool synced;
        if ((error= flush_and_sync(&synced)))
        {
          mysql_mutex_unlock(&LOCK_log);
        }
        else if ((error= RUN_HOOK(binlog_storage, after_flush,
                 (thd, log_file_name, file->pos_in_file, synced))))
        {
          sql_print_error("Failed to run 'after_flush' hooks");
          mysql_mutex_unlock(&LOCK_log);
        } 
        else
        {
          bool check_purge;
          signal_update();
          error= rotate(false, &check_purge);
          mysql_mutex_unlock(&LOCK_log);
          if (!error && check_purge)
            purge();
        }
      }
      else
      {
        mysql_mutex_unlock(&LOCK_log);
      }
    }

    if (error)
    {
      set_write_error(thd, is_trans_cache);
      if (check_write_error(thd) && cache_data &&
          stmt_has_updated_non_trans_table(thd))
        cache_data->set_incident();
    }
  }

  DBUG_RETURN(error);
}


int error_log_print(enum loglevel level, const char *format,
                    va_list args)
{
  return logger.error_log_print(level, format, args);
}


bool slow_log_print(THD *thd, const char *query, uint query_length,
                    ulonglong current_utime)
{
  return logger.slow_log_print(thd, query, query_length, current_utime);
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
  The method executes rotation when LOCK_log is already acquired
  by the caller.

  @param force_rotate  caller can request the log rotation
  @param check_purge   is set to true if rotation took place

  @note
    If rotation fails, for instance the server was unable 
    to create a new log file, we still try to write an 
    incident event to the current log.

  @retval
    nonzero - error in rotating routine.
*/
int MYSQL_BIN_LOG::rotate(bool force_rotate, bool* check_purge)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::rotate");

  //todo: fix the macro def and restore safe_mutex_assert_owner(&LOCK_log);
  *check_purge= false;

  if (force_rotate || (my_b_tell(&log_file) >= (my_off_t) max_size))
  {
    if ((error= new_file_without_locking()))
      /** 
         Be conservative... There are possible lost events (eg, 
         failing to log the Execute_load_query_log_event
         on a LOAD DATA while using a non-transactional
         table)!

         We give it a shot and try to write an incident event anyway
         to the current log. 
      */
      if (!write_incident(current_thd, FALSE))
        flush_and_sync(0);

    *check_purge= true;
  }
  DBUG_RETURN(error);
}

/**
  The method executes logs purging routine.

  @retval
    nonzero - error in rotating routine.
*/
void MYSQL_BIN_LOG::purge()
{
#ifdef HAVE_REPLICATION
  if (expire_logs_days)
  {
    DEBUG_SYNC(current_thd, "at_purge_logs_before_date");
    time_t purge_time= my_time(0) - expire_logs_days*24*60*60;
    if (purge_time >= 0)
    {
      purge_logs_before_date(purge_time);
    }
  }
#endif
}

/**
  The method is a shortcut of @c rotate() and @c purge().
  LOCK_log is acquired prior to rotate and is released after it.

  @param force_rotate  caller can request the log rotation

  @retval
    nonzero - error in rotating routine.
*/
int MYSQL_BIN_LOG::rotate_and_purge(bool force_rotate)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::rotate_and_purge");
  bool check_purge= false;

  //todo: fix the macro def and restore safe_mutex_assert_not_owner(&LOCK_log);
  mysql_mutex_lock(&LOCK_log);
  error= rotate(force_rotate, &check_purge);
  /*
    NOTE: Run purge_logs wo/ holding LOCK_log because it does not need
          the mutex. Otherwise causes various deadlocks.
  */
  mysql_mutex_unlock(&LOCK_log);

  if (!error && check_purge)
    purge();

  DBUG_RETURN(error);
}

uint MYSQL_BIN_LOG::next_file_id()
{
  uint res;
  mysql_mutex_lock(&LOCK_log);
  res = file_id++;
  mysql_mutex_unlock(&LOCK_log);
  return res;
}


/*
  Write the contents of a cache to the binary log.

  SYNOPSIS
    write_cache()
    cache    Cache to write to the binary log
    lock_log True if the LOCK_log mutex should be aquired, false otherwise
    sync_log True if the log should be flushed and synced

  DESCRIPTION
    Write the contents of the cache to the binary log. The cache will
    be reset as a READ_CACHE to be able to read the contents from it.
 */

int MYSQL_BIN_LOG::write_cache(IO_CACHE *cache, bool lock_log, bool sync_log)
{
  Mutex_sentry sentry(lock_log ? &LOCK_log : NULL);

  if (reinit_io_cache(cache, READ_CACHE, 0, 0, 0))
    return ER_ERROR_ON_WRITE;
  uint length= my_b_bytes_in_cache(cache), group, carry, hdr_offs;
  long val;
  uchar header[LOG_EVENT_HEADER_LEN];

  /*
    The events in the buffer have incorrect end_log_pos data
    (relative to beginning of group rather than absolute),
    so we'll recalculate them in situ so the binlog is always
    correct, even in the middle of a group. This is possible
    because we now know the start position of the group (the
    offset of this cache in the log, if you will); all we need
    to do is to find all event-headers, and add the position of
    the group to the end_log_pos of each event.  This is pretty
    straight forward, except that we read the cache in segments,
    so an event-header might end up on the cache-border and get
    split.
  */

  group= (uint)my_b_tell(&log_file);
  hdr_offs= carry= 0;

  do
  {
    /*
      if we only got a partial header in the last iteration,
      get the other half now and process a full header.
    */
    if (unlikely(carry > 0))
    {
      DBUG_ASSERT(carry < LOG_EVENT_HEADER_LEN);

      /* assemble both halves */
      memcpy(&header[carry], (char *)cache->read_pos, LOG_EVENT_HEADER_LEN - carry);

      /* fix end_log_pos */
      val= uint4korr(&header[LOG_POS_OFFSET]) + group;
      int4store(&header[LOG_POS_OFFSET], val);

      /* write the first half of the split header */
      if (my_b_write(&log_file, header, carry))
        return ER_ERROR_ON_WRITE;

      /*
        copy fixed second half of header to cache so the correct
        version will be written later.
      */
      memcpy((char *)cache->read_pos, &header[carry], LOG_EVENT_HEADER_LEN - carry);

      /* next event header at ... */
      hdr_offs = uint4korr(&header[EVENT_LEN_OFFSET]) - carry;

      carry= 0;
    }

    /* if there is anything to write, process it. */

    if (likely(length > 0))
    {
      /*
        process all event-headers in this (partial) cache.
        if next header is beyond current read-buffer,
        we'll get it later (though not necessarily in the
        very next iteration, just "eventually").
      */

      while (hdr_offs < length)
      {
        /*
          partial header only? save what we can get, process once
          we get the rest.
        */

        if (hdr_offs + LOG_EVENT_HEADER_LEN > length)
        {
          carry= length - hdr_offs;
          memcpy(header, (char *)cache->read_pos + hdr_offs, carry);
          length= hdr_offs;
        }
        else
        {
          /* we've got a full event-header, and it came in one piece */

          uchar *log_pos= (uchar *)cache->read_pos + hdr_offs + LOG_POS_OFFSET;

          /* fix end_log_pos */
          val= uint4korr(log_pos) + group;
          int4store(log_pos, val);

          /* next event header at ... */
          log_pos= (uchar *)cache->read_pos + hdr_offs + EVENT_LEN_OFFSET;
          hdr_offs += uint4korr(log_pos);

        }
      }

      /*
        Adjust hdr_offs. Note that it may still point beyond the segment
        read in the next iteration; if the current event is very long,
        it may take a couple of read-iterations (and subsequent adjustments
        of hdr_offs) for it to point into the then-current segment.
        If we have a split header (!carry), hdr_offs will be set at the
        beginning of the next iteration, overwriting the value we set here:
      */
      hdr_offs -= length;
    }

    /* Write data to the binary log file */
    if (my_b_write(&log_file, cache->read_pos, length))
      return ER_ERROR_ON_WRITE;
    cache->read_pos=cache->read_end;		// Mark buffer used up
  } while ((length= my_b_fill(cache)));

  DBUG_ASSERT(carry == 0);

  if (sync_log)
    return flush_and_sync(0);

  return 0;                                     // All OK
}

/*
  Helper function to get the error code of the query to be binlogged.
 */
int query_error_code(THD *thd, bool not_killed)
{
  int error;
  
  if (not_killed || (thd->killed == THD::KILL_BAD_DATA))
  {
    error= thd->is_error() ? thd->stmt_da->sql_errno() : 0;

    /* thd->stmt_da->sql_errno() might be ER_SERVER_SHUTDOWN or
       ER_QUERY_INTERRUPTED, So here we need to make sure that error
       is not set to these errors when specified not_killed by the
       caller.
    */
    if (error == ER_SERVER_SHUTDOWN || error == ER_QUERY_INTERRUPTED)
      error= 0;
  }
  else
  {
    /* killed status for DELAYED INSERT thread should never be used */
    DBUG_ASSERT(!(thd->system_thread & SYSTEM_THREAD_DELAYED_INSERT));
    error= thd->killed_errno();
  }

  return error;
}

bool MYSQL_BIN_LOG::write_incident(THD *thd, bool lock)
{
  uint error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::write_incident");

  if (!is_open())
    DBUG_RETURN(error);

  LEX_STRING const write_error_msg=
    { C_STRING_WITH_LEN("error writing to the binary log") };
  Incident incident= INCIDENT_LOST_EVENTS;
  Incident_log_event ev(thd, incident, write_error_msg);
  if (lock)
    mysql_mutex_lock(&LOCK_log);
  error= ev.write(&log_file);
  if (lock)
  {
    if (!error && !(error= flush_and_sync(0)))
    {
      bool check_purge= false;
      signal_update();
      error= rotate(false, &check_purge);
      mysql_mutex_unlock(&LOCK_log);
      if (!error && check_purge)
        purge();
    }
    else
    {
      mysql_mutex_unlock(&LOCK_log);
    }
  }
  DBUG_RETURN(error);
}

/**
  Write a cached log entry to the binary log.
  - To support transaction over replication, we wrap the transaction
  with BEGIN/COMMIT or BEGIN/ROLLBACK in the binary log.
  We want to write a BEGIN/ROLLBACK block when a non-transactional table
  was updated in a transaction which was rolled back. This is to ensure
  that the same updates are run on the slave.

  @param thd
  @param cache		The cache to copy to the binlog
  @param commit_event   The commit event to print after writing the
                        contents of the cache.
  @param incident       Defines if an incident event should be created to
                        notify that some non-transactional changes did
                        not get into the binlog.

  @note
    We only come here if there is something in the cache.
  @note
    The thing in the cache is always a complete transaction.
  @note
    'cache' needs to be reinitialized after this functions returns.
*/

bool MYSQL_BIN_LOG::write(THD *thd, IO_CACHE *cache, Log_event *commit_event,
                          bool incident)
{
  DBUG_ENTER("MYSQL_BIN_LOG::write(THD *, IO_CACHE *, Log_event *)");

  DBUG_ASSERT(is_open());
  if (likely(is_open()))                       // Should always be true
  {
    bool check_purge;
    
    mysql_mutex_lock(&LOCK_log);
    /*
      We only bother to write to the binary log if there is anything
      to write.
     */
    if (my_b_tell(cache) > 0)
    {
      /*
        Log "BEGIN" at the beginning of every transaction.  Here, a
        transaction is either a BEGIN..COMMIT block or a single
        statement in autocommit mode.
      */
      Query_log_event qinfo(thd, STRING_WITH_LEN("BEGIN"), TRUE, FALSE, TRUE, 0);
      if (qinfo.write(&log_file))
        goto err;
      DBUG_EXECUTE_IF("crash_before_writing_xid",
                      {
                        if ((write_error= write_cache(cache, false, true)))
                          DBUG_PRINT("info", ("error writing binlog cache: %d",
                                               write_error));
                        DBUG_PRINT("info", ("crashing before writing xid"));
                        DBUG_SUICIDE();
                      });

      if ((write_error= write_cache(cache, false, false)))
        goto err;

      if (commit_event && commit_event->write(&log_file))
        goto err;

      if (incident && write_incident(thd, FALSE))
        goto err;

      bool synced= 0;
      if (flush_and_sync(&synced))
        goto err;
      DBUG_EXECUTE_IF("half_binlogged_transaction", DBUG_SUICIDE(););
      if (cache->error)				// Error on read
      {
        sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name, errno);
        write_error=1;				// Don't give more errors
        goto err;
      }

      if (RUN_HOOK(binlog_storage, after_flush,
                   (thd, log_file_name, log_file.pos_in_file, synced)))
      {
        sql_print_error("Failed to run 'after_flush' hooks");
        write_error=1;
        goto err;
      }

      signal_update();
    }

    /*
      if commit_event is Xid_log_event, increase the number of
      prepared_xids (it's decreasd in ::unlog()). Binlog cannot be rotated
      if there're prepared xids in it - see the comment in new_file() for
      an explanation.
      If the commit_event is not Xid_log_event (then it's a Query_log_event)
      rotate binlog, if necessary.
    */
    if (commit_event && commit_event->get_type_code() == XID_EVENT)
    {
      mysql_mutex_lock(&LOCK_prep_xids);
      prepared_xids++;
      mysql_mutex_unlock(&LOCK_prep_xids);
      mysql_mutex_unlock(&LOCK_log);
    }
    else
    {
      if (rotate(false, &check_purge))
        goto err;
      mysql_mutex_unlock(&LOCK_log);
      if (check_purge) 
        purge();
    }
  }

  DBUG_RETURN(0);

err:
  if (!write_error)
  {
    write_error= 1;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
  }
  mysql_mutex_unlock(&LOCK_log);
  DBUG_RETURN(1);
}


/**
  Wait until we get a signal that the relay log has been updated.

  @param thd		Thread variable

  @note
    One must have a lock on LOCK_log before calling this function.
    This lock will be released before return! That's required by
    THD::enter_cond() (see NOTES in sql_class.h).
*/

void MYSQL_BIN_LOG::wait_for_update_relay_log(THD* thd)
{
  const char *old_msg;
  DBUG_ENTER("wait_for_update_relay_log");

  old_msg= thd->enter_cond(&update_cond, &LOCK_log,
                           "Slave has read all relay log; "
                           "waiting for the slave I/O "
                           "thread to update it" );
  mysql_cond_wait(&update_cond, &LOCK_log);
  thd->exit_cond(old_msg);
  DBUG_VOID_RETURN;
}

/**
  Wait until we get a signal that the binary log has been updated.
  Applies to master only.
     
  NOTES
  @param[in] thd        a THD struct
  @param[in] timeout    a pointer to a timespec;
                        NULL means to wait w/o timeout.
  @retval    0          if got signalled on update
  @retval    non-0      if wait timeout elapsed
  @note
    LOCK_log must be taken before calling this function.
    LOCK_log is being released while the thread is waiting.
    LOCK_log is released by the caller.
*/

int MYSQL_BIN_LOG::wait_for_update_bin_log(THD* thd,
                                           const struct timespec *timeout)
{
  int ret= 0;
  DBUG_ENTER("wait_for_update_bin_log");

  if (!timeout)
    mysql_cond_wait(&update_cond, &LOCK_log);
  else
    ret= mysql_cond_timedwait(&update_cond, &LOCK_log,
                              const_cast<struct timespec *>(timeout));
  DBUG_RETURN(ret);
}


/**
  Close the log file.

  @param exiting     Bitmask for one or more of the following bits:
          - LOG_CLOSE_INDEX : if we should close the index file
          - LOG_CLOSE_TO_BE_OPENED : if we intend to call open
                                     at once after close.
          - LOG_CLOSE_STOP_EVENT : write a 'stop' event to the log

  @note
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_BIN_LOG::close(uint exiting)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_BIN_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (log_state == LOG_OPENED)
  {
#ifdef HAVE_REPLICATION
    if (log_type == LOG_BIN && !no_auto_events &&
	(exiting & LOG_CLOSE_STOP_EVENT))
    {
      Stop_log_event s;
      s.write(&log_file);
      bytes_written+= s.data_written;
      signal_update();
    }
#endif /* HAVE_REPLICATION */

    /* don't pwrite in a file opened with O_APPEND - it doesn't work */
    if (log_file.type == WRITE_CACHE && log_type == LOG_BIN)
    {
      my_off_t offset= BIN_LOG_HEADER_SIZE + FLAGS_OFFSET;
      my_off_t org_position= mysql_file_tell(log_file.file, MYF(0));
      uchar flags= 0;            // clearing LOG_EVENT_BINLOG_IN_USE_F
      mysql_file_pwrite(log_file.file, &flags, 1, offset, MYF(0));
      /*
        Restore position so that anything we have in the IO_cache is written
        to the correct position.
        We need the seek here, as mysql_file_pwrite() is not guaranteed to keep the
        original position on system that doesn't support pwrite().
      */
      mysql_file_seek(log_file.file, org_position, MY_SEEK_SET, MYF(0));
    }

    /* this will cleanup IO_CACHE, sync and close the file */
    MYSQL_LOG::close(exiting);
  }

  /*
    The following test is needed even if is_open() is not set, as we may have
    called a not complete close earlier and the index file is still open.
  */

  if ((exiting & LOG_CLOSE_INDEX) && my_b_inited(&index_file))
  {
    end_io_cache(&index_file);
    if (mysql_file_close(index_file.file, MYF(0)) < 0 && ! write_error)
    {
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), index_file_name, errno);
    }
  }
  log_state= (exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED;
  my_free(name);
  name= NULL;
  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::set_max_size(ulong max_size_arg)
{
  /*
    We need to take locks, otherwise this may happen:
    new_file() is called, calls open(old_max_size), then before open() starts,
    set_max_size() sets max_size to max_size_arg, then open() starts and
    uses the old_max_size argument, so max_size_arg has been overwritten and
    it's like if the SET command was never run.
  */
  DBUG_ENTER("MYSQL_BIN_LOG::set_max_size");
  mysql_mutex_lock(&LOCK_log);
  if (is_open())
    max_size= max_size_arg;
  mysql_mutex_unlock(&LOCK_log);
  DBUG_VOID_RETURN;
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
  if (outstream && !my_freopen(filename, "a", outstream))
    return TRUE;

  if (errstream && !my_freopen(filename, "a", errstream))
    return TRUE;

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

void MYSQL_BIN_LOG::signal_update()
{
  DBUG_ENTER("MYSQL_BIN_LOG::signal_update");
  signal_cnt++;
  mysql_cond_broadcast(&update_cond);
  DBUG_VOID_RETURN;
}

#ifdef _WIN32
static void print_buffer_to_nt_eventlog(enum loglevel level, char *buff,
                                        size_t length, size_t buffLen)
{
  HANDLE event;
  char   *buffptr= buff;
  DBUG_ENTER("print_buffer_to_nt_eventlog");

  /* Add ending CR/LF's to string, overwrite last chars if necessary */
  strmov(buffptr+min(length, buffLen-5), "\r\n\r\n");

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
  time_t skr;
  struct tm tm_tmp;
  struct tm *start;
  DBUG_ENTER("print_buffer_to_file");
  DBUG_PRINT("enter",("buffer: %s", buffer));

  mysql_mutex_lock(&LOCK_error_log);

  skr= my_time(0);
  localtime_r(&skr, &tm_tmp);
  start=&tm_tmp;

  fprintf(stderr, "%02d%02d%02d %2d:%02d:%02d [%s] %.*s\n",
          start->tm_year % 100,
          start->tm_mon+1,
          start->tm_mday,
          start->tm_hour,
          start->tm_min,
          start->tm_sec,
          (level == ERROR_LEVEL ? "ERROR" : level == WARNING_LEVEL ?
           "Warning" : "Note"),
          (int) length, buffer);

  fflush(stderr);

  mysql_mutex_unlock(&LOCK_error_log);
  DBUG_VOID_RETURN;
}

/**
  Prints a printf style message to the error log and, under NT, to the
  Windows event log.

  This function prints the message into a buffer and then sends that buffer
  to other functions to write that message to other logging sources.

  @param level          The level of the msg significance
  @param format         Printf style format of message
  @param args           va_list list of arguments for the message

  @returns
    The function always returns 0. The return value is present in the
    signature to be compatible with other logging routines, which could
    return an error (e.g. logging to the log tables)
*/
int vprint_msg_to_log(enum loglevel level, const char *format, va_list args)
{
  char   buff[1024];
  size_t length;
  DBUG_ENTER("vprint_msg_to_log");

  length= my_vsnprintf(buff, sizeof(buff), format, args);
  print_buffer_to_file(level, buff, length);

#ifdef _WIN32
  print_buffer_to_nt_eventlog(level, buff, length, sizeof(buff));
#endif

  DBUG_RETURN(0);
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
  DBUG_ASSERT(TC_LOG_PAGE_SIZE % tc_log_page_size == 0);

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
  if (!(pages=(PAGE *)my_malloc(npages*sizeof(PAGE), MYF(MY_WME|MY_ZEROFILL))))
    goto err;
  inited=3;
  for (pg=pages, i=0; i < npages; i++, pg++)
  {
    pg->next=pg+1;
    pg->waiters=0;
    pg->state=PS_POOL;
    mysql_mutex_init(key_PAGE_lock, &pg->lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_PAGE_cond, &pg->cond, 0);
    pg->start=(my_xid *)(data + i*tc_log_page_size);
    pg->end=(my_xid *)(pg->start + tc_log_page_size);
    pg->size=pg->free=tc_log_page_size/sizeof(my_xid);
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

  mysql_mutex_init(key_LOCK_sync, &LOCK_sync, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_active, &LOCK_active, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_pool, &LOCK_pool, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_active, &COND_active, 0);
  mysql_cond_init(key_COND_pool, &COND_pool, 0);

  inited=6;

  syncing= 0;
  active=pages;
  pool=pages+1;
  pool_last=pages+npages-1;

  return 0;

err:
  close();
  return 1;
}

/**
  there is no active page, let's got one from the pool.

  Two strategies here:
    -# take the first from the pool
    -# if there're waiters - take the one with the most free space.

  @todo
    TODO page merging. try to allocate adjacent page first,
    so that they can be flushed both in one sync
*/

void TC_LOG_MMAP::get_active_from_pool()
{
  PAGE **p, **best_p=0;
  int best_free;

  if (syncing)
    mysql_mutex_lock(&LOCK_pool);

  do
  {
    best_p= p= &pool;
    if ((*p)->waiters == 0) // can the first page be used ?
      break;                // yes - take it.

    best_free=0;            // no - trying second strategy
    for (p=&(*p)->next; *p; p=&(*p)->next)
    {
      if ((*p)->waiters == 0 && (*p)->free > best_free)
      {
        best_free=(*p)->free;
        best_p=p;
      }
    }
  }
  while ((*best_p == 0 || best_free == 0) && overflow());

  active=*best_p;
  if (active->free == active->size) // we've chosen an empty page
  {
    tc_log_cur_pages_used++;
    set_if_bigger(tc_log_max_pages_used, tc_log_cur_pages_used);
  }

  if ((*best_p)->next)              // unlink the page from the pool
    *best_p=(*best_p)->next;
  else
    pool_last=*best_p;

  if (syncing)
    mysql_mutex_unlock(&LOCK_pool);
}

/**
  @todo
  perhaps, increase log size ?
*/
int TC_LOG_MMAP::overflow()
{
  /*
    simple overflow handling - just wait
    TODO perhaps, increase log size ?
    let's check the behaviour of tc_log_page_waits first
  */
  tc_log_page_waits++;
  mysql_cond_wait(&COND_pool, &LOCK_pool);
  return 1; // always return 1
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

   If tc_log == MYSQL_LOG then tc_log writes transaction to binlog and
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

int TC_LOG_MMAP::log_xid(THD *thd, my_xid xid)
{
  int err;
  PAGE *p;
  ulong cookie;

  mysql_mutex_lock(&LOCK_active);

  /*
    if active page is full - just wait...
    frankly speaking, active->free here accessed outside of mutex
    protection, but it's safe, because it only means we may miss an
    unlog() for the active page, and we're not waiting for it here -
    unlog() does not signal COND_active.
  */
  while (unlikely(active && active->free == 0))
    mysql_cond_wait(&COND_active, &LOCK_active);

  /* no active page ? take one from the pool */
  if (active == 0)
    get_active_from_pool();

  p=active;
  mysql_mutex_lock(&p->lock);

  /* searching for an empty slot */
  while (*p->ptr)
  {
    p->ptr++;
    DBUG_ASSERT(p->ptr < p->end);               // because p->free > 0
  }

  /* found! store xid there and mark the page dirty */
  cookie= (ulong)((uchar *)p->ptr - data);      // can never be zero
  *p->ptr++= xid;
  p->free--;
  p->state= PS_DIRTY;

  /* to sync or not to sync - this is the question */
  mysql_mutex_unlock(&LOCK_active);
  mysql_mutex_lock(&LOCK_sync);
  mysql_mutex_unlock(&p->lock);

  if (syncing)
  {                                          // somebody's syncing. let's wait
    p->waiters++;
    /*
      note - it must be while (), not do ... while () here
      as p->state may be not PS_DIRTY when we come here
    */
    while (p->state == PS_DIRTY && syncing)
      mysql_cond_wait(&p->cond, &LOCK_sync);
    p->waiters--;
    err= p->state == PS_ERROR;
    if (p->state != PS_DIRTY)                   // page was synced
    {
      if (p->waiters == 0)
        mysql_cond_signal(&COND_pool);       // in case somebody's waiting
      mysql_mutex_unlock(&LOCK_sync);
      goto done;                             // we're done
    }
  }                                          // page was not synced! do it now
  DBUG_ASSERT(active == p && syncing == 0);
  mysql_mutex_lock(&LOCK_active);
  syncing=p;                                 // place is vacant - take it
  active=0;                                  // page is not active anymore
  mysql_cond_broadcast(&COND_active);        // in case somebody's waiting
  mysql_mutex_unlock(&LOCK_active);
  mysql_mutex_unlock(&LOCK_sync);
  err= sync();

done:
  return err ? 0 : cookie;
}

int TC_LOG_MMAP::sync()
{
  int err;

  DBUG_ASSERT(syncing != active);

  /*
    sit down and relax - this can take a while...
    note - no locks are held at this point
  */
  err= my_msync(fd, syncing->start, 1, MS_SYNC);

  /* page is synced. let's move it to the pool */
  mysql_mutex_lock(&LOCK_pool);
  pool_last->next=syncing;
  pool_last=syncing;
  syncing->next=0;
  syncing->state= err ? PS_ERROR : PS_POOL;
  mysql_cond_broadcast(&syncing->cond);      // signal "sync done"
  mysql_cond_signal(&COND_pool);             // in case somebody's waiting
  mysql_mutex_unlock(&LOCK_pool);

  /* marking 'syncing' slot free */
  mysql_mutex_lock(&LOCK_sync);
  syncing=0;
  mysql_cond_signal(&active->cond);        // wake up a new syncer
  mysql_mutex_unlock(&LOCK_sync);
  return err;
}

/**
  erase xid from the page, update page free space counters/pointers.
  cookie points directly to the memory where xid was logged.
*/

int TC_LOG_MMAP::unlog(ulong cookie, my_xid xid)
{
  PAGE *p=pages+(cookie/tc_log_page_size);
  my_xid *x=(my_xid *)(data+cookie);

  DBUG_ASSERT(*x == xid);
  DBUG_ASSERT(x >= p->start && x < p->end);
  *x=0;

  mysql_mutex_lock(&p->lock);
  p->free++;
  DBUG_ASSERT(p->free <= p->size);
  set_if_smaller(p->ptr, x);
  if (p->free == p->size)               // the page is completely empty
    statistic_decrement(tc_log_cur_pages_used, &LOCK_status);
  if (p->waiters == 0)                 // the page is in pool and ready to rock
    mysql_cond_signal(&COND_pool);     // ping ... for overflow()
  mysql_mutex_unlock(&p->lock);
  return 0;
}

void TC_LOG_MMAP::close()
{
  uint i;
  switch (inited) {
  case 6:
    mysql_mutex_destroy(&LOCK_sync);
    mysql_mutex_destroy(&LOCK_active);
    mysql_mutex_destroy(&LOCK_pool);
    mysql_cond_destroy(&COND_pool);
  case 5:
    data[0]='A'; // garble the first (signature) byte, in case mysql_file_delete fails
  case 4:
    for (i=0; i < npages; i++)
    {
      if (pages[i].ptr == 0)
        break;
      mysql_mutex_destroy(&pages[i].lock);
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
  bzero(data, (size_t)file_length);
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

/****** transaction coordinator log for 2pc - binlog() based solution ******/
#define TC_LOG_BINLOG MYSQL_BIN_LOG

/**
  @todo
  keep in-memory list of prepared transactions
  (add to list in log(), remove on unlog())
  and copy it to the new binlog if rotated
  but let's check the behaviour of tc_log_page_waits first!
*/

int TC_LOG_BINLOG::open(const char *opt_name)
{
  LOG_INFO log_info;
  int      error= 1;

  DBUG_ASSERT(total_ha_2pc > 1);
  DBUG_ASSERT(opt_name && opt_name[0]);

  mysql_mutex_init(key_BINLOG_LOCK_prep_xids,
                   &LOCK_prep_xids, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_BINLOG_COND_prep_xids, &COND_prep_xids, 0);

  if (!my_b_inited(&index_file))
  {
    /* There was a failure to open the index file, can't open the binlog */
    cleanup();
    return 1;
  }

  if (using_heuristic_recover())
  {
    /* generate a new binlog to mask a corrupted one */
    open(opt_name, LOG_BIN, 0, WRITE_CACHE, 0, max_binlog_size, 0, TRUE);
    cleanup();
    return 1;
  }

  if ((error= find_log_pos(&log_info, NullS, 1)))
  {
    if (error != LOG_INFO_EOF)
      sql_print_error("find_log_pos() failed (error: %d)", error);
    else
      error= 0;
    goto err;
  }

  {
    const char *errmsg;
    IO_CACHE    log;
    File        file;
    Log_event  *ev=0;
    Format_description_log_event fdle(BINLOG_VERSION);
    char        log_name[FN_REFLEN];

    if (! fdle.is_valid())
      goto err;

    do
    {
      strmake(log_name, log_info.log_file_name, sizeof(log_name)-1);
    } while (!(error= find_next_log(&log_info, 1)));

    if (error !=  LOG_INFO_EOF)
    {
      sql_print_error("find_log_pos() failed (error: %d)", error);
      goto err;
    }

    if ((file= open_binlog(&log, log_name, &errmsg)) < 0)
    {
      sql_print_error("%s", errmsg);
      goto err;
    }

    if ((ev= Log_event::read_log_event(&log, 0, &fdle)) &&
        ev->get_type_code() == FORMAT_DESCRIPTION_EVENT &&
        ev->flags & LOG_EVENT_BINLOG_IN_USE_F)
    {
      sql_print_information("Recovering after a crash using %s", opt_name);
      error= recover(&log, (Format_description_log_event *)ev);
    }
    else
      error=0;

    delete ev;
    end_io_cache(&log);
    mysql_file_close(file, MYF(MY_WME));

    if (error)
      goto err;
  }

err:
  return error;
}

/** This is called on shutdown, after ha_panic. */
void TC_LOG_BINLOG::close()
{
  DBUG_ASSERT(prepared_xids==0);
  mysql_mutex_destroy(&LOCK_prep_xids);
  mysql_cond_destroy(&COND_prep_xids);
}

/**
  @todo
  group commit

  @retval
    0    error
  @retval
    1    success
*/
int TC_LOG_BINLOG::log_xid(THD *thd, my_xid xid)
{
  DBUG_ENTER("TC_LOG_BINLOG::log");
  binlog_cache_mngr *cache_mngr=
    (binlog_cache_mngr*) thd_get_ha_data(thd, binlog_hton);
  /*
    We always commit the entire transaction when writing an XID. Also
    note that the return value is inverted.
   */
  DBUG_RETURN(!binlog_commit_flush_stmt_cache(thd, cache_mngr) &&
              !binlog_commit_flush_trx_cache(thd, cache_mngr, xid));
}

int TC_LOG_BINLOG::unlog(ulong cookie, my_xid xid)
{
  DBUG_ENTER("TC_LOG_BINLOG::unlog");
  mysql_mutex_lock(&LOCK_prep_xids);
  // prepared_xids can be 0 if the transaction had ignorable errors.
  DBUG_ASSERT(prepared_xids >= 0);
  if (prepared_xids > 0)
    prepared_xids--;
  if (prepared_xids == 0) {
    DBUG_PRINT("info", ("prepared_xids=%lu", prepared_xids));
    mysql_cond_signal(&COND_prep_xids);
  }
  mysql_mutex_unlock(&LOCK_prep_xids);
  DBUG_RETURN(rotate_and_purge(0));     // as ::write() did not rotate
}

int TC_LOG_BINLOG::recover(IO_CACHE *log, Format_description_log_event *fdle)
{
  Log_event  *ev;
  HASH xids;
  MEM_ROOT mem_root;

  if (! fdle->is_valid() ||
      my_hash_init(&xids, &my_charset_bin, TC_LOG_PAGE_SIZE/3, 0,
                   sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  init_alloc_root(&mem_root, TC_LOG_PAGE_SIZE, TC_LOG_PAGE_SIZE);

  fdle->flags&= ~LOG_EVENT_BINLOG_IN_USE_F; // abort on the first error

  while ((ev= Log_event::read_log_event(log,0,fdle)) && ev->is_valid())
  {
    if (ev->get_type_code() == XID_EVENT)
    {
      Xid_log_event *xev=(Xid_log_event *)ev;
      uchar *x= (uchar *) memdup_root(&mem_root, (uchar*) &xev->xid,
                                      sizeof(xev->xid));
      if (!x || my_hash_insert(&xids, x))
        goto err2;
    }
    delete ev;
  }

  if (ha_recover(&xids))
    goto err2;

  free_root(&mem_root, MYF(0));
  my_hash_free(&xids);
  return 0;

err2:
  free_root(&mem_root, MYF(0));
  my_hash_free(&xids);
err1:
  sql_print_error("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete (or rename) binary log and start mysqld with "
                  "--tc-heuristic-recover={commit|rollback}");
  return 1;
}


#ifdef INNODB_COMPATIBILITY_HOOKS
/**
  Get the file name of the MySQL binlog.
  @return the name of the binlog file
*/
extern "C"
const char* mysql_bin_log_file_name(void)
{
  return mysql_bin_log.get_log_fname();
}
/**
  Get the current position of the MySQL binlog.
  @return byte offset from the beginning of the binlog
*/
extern "C"
ulonglong mysql_bin_log_file_pos(void)
{
  return (ulonglong) mysql_bin_log.get_log_file()->pos_in_file;
}
#endif /* INNODB_COMPATIBILITY_HOOKS */


struct st_mysql_storage_engine binlog_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(binlog)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &binlog_storage_engine,
  "binlog",
  "MySQL AB",
  "This is a pseudo storage engine to represent the binlog in a transaction",
  PLUGIN_LICENSE_GPL,
  binlog_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
