/* Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LOG_H
#define LOG_H

#include "unireg.h"                    // REQUIRED: for other includes
#include "handler.h"                            /* my_xid */

/**
  the struct aggregates two paramenters that identify an event
  uniquely in scope of communication of a particular master and slave couple.
  I.e there can not be 2 events from the same staying connected master which
  have the same coordinates.
  @note
  Such identifier is not yet unique generally as the event originating master
  is resetable. Also the crashed master can be replaced with some other.
*/
typedef struct event_coordinates
{
  char * file_name; // binlog file name (directories stripped)
  my_off_t  pos;       // event's position in the binlog file
} LOG_POS_COORD;

/**
  Transaction Coordinator Log.

  A base abstract class for three different implementations of the
  transaction coordinator.

  The server uses the transaction coordinator to order transactions
  correctly and there are three different implementations: one using
  an in-memory structure, one dummy that does not do anything, and one
  using the binary log for transaction coordination.
*/
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() {}
  virtual ~TC_LOG() {}

  enum enum_result {
    RESULT_SUCCESS,
    RESULT_ABORTED,
    RESULT_INCONSISTENT
  };

  virtual int open(const char *opt_name)=0;
  virtual void close()=0;

  /**
     Log a commit record of the transaction to the transaction
     coordinator log.

     When the function returns, the transaction commit is properly
     logged to the transaction coordinator log and can be committed in
     the storage engines.

     @param thd Session to log transaction for.
     @param all @c True if this is a "real" commit, @c false if it is a "statement" commit.

     @return Error code on failure, zero on success.
   */
  virtual enum_result commit(THD *thd, bool all) = 0;

  /**
     Log a rollback record of the transaction to the transaction
     coordinator log.

     When the function returns, the transaction have been aborted in
     the transaction coordinator log.

     @param thd Session to log transaction record for.

     @param all @c true if an explicit commit or an implicit commit
     for a statement, @c false if an internal commit of the statement.

     @return Error code on failure, zero on success.
   */
  virtual int rollback(THD *thd, bool all) = 0;
  /**
     Log a prepare record of the transaction to the storage engines.

     @param thd Session to log transaction record for.

     @param all @c true if an explicit commit or an implicit commit
     for a statement, @c false if an internal commit of the statement.

     @return Error code on failure, zero on success.
   */
  virtual int prepare(THD *thd, bool all) = 0;
};


class TC_LOG_DUMMY: public TC_LOG // use it to disable the logging
{
public:
  TC_LOG_DUMMY() {}
  int open(const char *opt_name)        { return 0; }
  void close()                          { }
  enum_result commit(THD *thd, bool all) {
    return ha_commit_low(thd, all) ? RESULT_ABORTED : RESULT_SUCCESS;
  }
  int rollback(THD *thd, bool all) {
    return ha_rollback_low(thd, all);
  }
  int prepare(THD *thd, bool all) {
    return ha_prepare_low(thd, all);
  }
};

#ifdef HAVE_MMAP
class TC_LOG_MMAP: public TC_LOG
{
  public:                // only to keep Sun Forte on sol9x86 happy
  typedef enum {
    PS_POOL,                 // page is in pool
    PS_ERROR,                // last sync failed
    PS_DIRTY                 // new xids added since last sync
  } PAGE_STATE;

  private:
  typedef struct st_page {
    struct st_page *next; // page a linked in a fifo queue
    my_xid *start, *end;  // usable area of a page
    my_xid *ptr;          // next xid will be written here
    int size, free;       // max and current number of free xid slots on the page
    int waiters;          // number of waiters on condition
    PAGE_STATE state;     // see above
    mysql_mutex_t lock; // to access page data or control structure
    mysql_cond_t  cond; // to wait for a sync
  } PAGE;

  char logname[FN_REFLEN];
  File fd;
  my_off_t file_length;
  uint npages, inited;
  uchar *data;
  struct st_page *pages, *syncing, *active, *pool, *pool_last;
  /*
    note that, e.g. LOCK_active is only used to protect
    'active' pointer, to protect the content of the active page
    one has to use active->lock.
    Same for LOCK_pool and LOCK_sync
  */
  mysql_mutex_t LOCK_active, LOCK_pool, LOCK_sync;
  mysql_cond_t COND_pool, COND_active;

  public:
  TC_LOG_MMAP(): inited(0) {}
  int open(const char *opt_name);
  void close();
  enum_result commit(THD *thd, bool all);
  int rollback(THD *thd, bool all)      { return ha_rollback_low(thd, all); }
  int prepare(THD *thd, bool all)       { return ha_prepare_low(thd, all); }
  int recover();

private:
  int log_xid(THD *thd, my_xid xid);
  int unlog(ulong cookie, my_xid xid);
  void get_active_from_pool();
  int sync();
  int overflow();
};
#else
#define TC_LOG_MMAP TC_LOG_DUMMY
#endif

extern TC_LOG *tc_log;
extern TC_LOG_MMAP tc_log_mmap;
extern TC_LOG_DUMMY tc_log_dummy;

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8
#define LOG_INFO_EMFILE -9


/* bitmap to SQL_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4

/* 
  Maximum unique log filename extension.
  Note: setting to 0x7FFFFFFF due to atol windows 
        overflow/truncate.
 */
#define MAX_LOG_UNIQUE_FN_EXT 0x7FFFFFFF

/* 
   Number of warnings that will be printed to error log
   before extension number is exhausted.
*/
#define LOG_WARN_UNIQUE_FN_EXT_LEFT 1000

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_LOG_INFO_lock;
#endif

/*
  Note that we destroy the lock mutex in the desctructor here.
  This means that object instances cannot be destroyed/go out of scope,
  until we have reset thd->current_linfo to NULL;
 */
typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  int entry_index; //used in purge_logs(), calculatd in find_log_pos().
  mysql_mutex_t lock;
  st_log_info()
    : index_file_offset(0), index_file_start_offset(0),
      pos(0), fatal(0), entry_index(0)
    {
      log_file_name[0] = '\0';
      mysql_mutex_init(key_LOG_INFO_lock, &lock, MY_MUTEX_INIT_FAST);
    }
  ~st_log_info() { mysql_mutex_destroy(&lock);}
} LOG_INFO;

/*
  Currently we have only 3 kinds of logging functions: old-fashioned
  logs, stdout and csv logging routines.
*/
#define MAX_LOG_HANDLERS_NUM 3

/* log event handler flags */
#define LOG_NONE       1
#define LOG_FILE       2
#define LOG_TABLE      4

enum enum_log_type { LOG_UNKNOWN, LOG_NORMAL, LOG_BIN };
enum enum_log_state { LOG_OPENED, LOG_CLOSED, LOG_TO_BE_OPENED };

/*
  TODO use mmap instead of IO_CACHE for binlog
  (mmap+fsync is two times faster than write+fsync)
*/

class MYSQL_LOG
{
public:
  MYSQL_LOG();
  void init_pthread_objects();
  void cleanup();
  bool open(
#ifdef HAVE_PSI_INTERFACE
            PSI_file_key log_file_key,
#endif
            const char *log_name,
            enum_log_type log_type,
            const char *new_name,
            enum cache_type io_cache_type_arg);
  bool init_and_set_log_file_name(const char *log_name,
                                  const char *new_name,
                                  enum_log_type log_type_arg,
                                  enum cache_type io_cache_type_arg);
  void init(enum_log_type log_type_arg,
            enum cache_type io_cache_type_arg);
  void close(uint exiting);
  inline bool is_open() { return log_state != LOG_CLOSED; }
  const char *generate_name(const char *log_name, const char *suffix,
                            bool strip_ext, char *buff);
  int generate_new_name(char *new_name, const char *log_name);
 protected:
  /* LOCK_log is inited by init_pthread_objects() */
  mysql_mutex_t LOCK_log;
  char *name;
  char log_file_name[FN_REFLEN];
  char time_buff[20], db[NAME_LEN + 1];
  bool write_error, inited;
  IO_CACHE log_file;
  enum_log_type log_type;
  volatile enum_log_state log_state;
  enum cache_type io_cache_type;
  friend class Log_event;
#ifdef HAVE_PSI_INTERFACE
  /** Instrumentation key to use for file io in @c log_file */
  PSI_file_key m_log_file_key;
  /** The instrumentation key to use for @ LOCK_log. */
  PSI_mutex_key m_key_LOCK_log;
#endif
};


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


class MYSQL_QUERY_LOG: public MYSQL_LOG
{
public:
  MYSQL_QUERY_LOG() : last_time(0) {}
  bool reopen_file();
  bool write(time_t event_time, const char *user_host,
             uint user_host_len, my_thread_id thread_id,
             const char *command_type, uint command_type_len,
             const char *sql_text, uint sql_text_len);
  bool write(THD *thd, time_t current_time, time_t query_start_arg,
             const char *user_host, uint user_host_len,
             ulonglong query_utime, ulonglong lock_utime, bool is_command,
             const char *sql_text, uint sql_text_len);
  bool open_slow_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(
#ifdef HAVE_PSI_INTERFACE
                key_file_slow_log,
#endif
                generate_name(log_name, "-slow.log", 0, buf),
                LOG_NORMAL, 0, WRITE_CACHE);
  }
  bool open_query_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(
#ifdef HAVE_PSI_INTERFACE
                key_file_query_log,
#endif
                generate_name(log_name, ".log", 0, buf),
                LOG_NORMAL, 0, WRITE_CACHE);
  }

private:
  time_t last_time;
};

class Log_event_handler
{
public:
  Log_event_handler() {}
  virtual bool init()= 0;
  virtual void cleanup()= 0;

  virtual bool log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, uint sql_text_len)= 0;
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args)= 0;
  virtual bool log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, my_thread_id thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           const CHARSET_INFO *client_cs)= 0;
  virtual ~Log_event_handler() {}
};


int check_if_log_table(size_t db_len, const char *db, size_t table_name_len,
                       const char *table_name, bool check_if_opened);

class Log_to_csv_event_handler: public Log_event_handler
{
public:
  Log_to_csv_event_handler();
  ~Log_to_csv_event_handler();
  virtual bool init();
  virtual void cleanup();

  virtual bool log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, uint sql_text_len);
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual bool log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, my_thread_id thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           const CHARSET_INFO *client_cs);

  int activate_log(THD *thd, uint log_type);
};


/* type of the log table */
#define QUERY_LOG_SLOW 1
#define QUERY_LOG_GENERAL 2

class Log_to_file_event_handler: public Log_event_handler
{
  MYSQL_QUERY_LOG mysql_log;
  MYSQL_QUERY_LOG mysql_slow_log;
  bool is_initialized;
public:
  Log_to_file_event_handler(): is_initialized(FALSE)
  {}
  virtual bool init();
  virtual void cleanup();

  virtual bool log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, uint sql_text_len);
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual bool log_general(THD *thd, time_t event_time, const char *user_host,
                           uint user_host_len, my_thread_id thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           const CHARSET_INFO *client_cs);
  void flush();
  void init_pthread_objects();
  MYSQL_QUERY_LOG *get_mysql_slow_log() { return &mysql_slow_log; }
  MYSQL_QUERY_LOG *get_mysql_log() { return &mysql_log; }
};


/* Class which manages slow, general and error log event handlers */
class LOGGER
{
  mysql_rwlock_t LOCK_logger;
  /* flag to check whether logger mutex is initialized */
  uint inited;

  /* available log handlers */
  Log_to_csv_event_handler *table_log_handler;
  Log_to_file_event_handler *file_log_handler;

  /* NULL-terminated arrays of log handlers */
  Log_event_handler *error_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];
  Log_event_handler *slow_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];
  Log_event_handler *general_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];

public:

  bool is_log_tables_initialized;

  LOGGER() : inited(0), table_log_handler(NULL),
             file_log_handler(NULL), is_log_tables_initialized(FALSE)
  {}
  void lock_shared() { mysql_rwlock_rdlock(&LOCK_logger); }
  void lock_exclusive() { mysql_rwlock_wrlock(&LOCK_logger); }
  void unlock() { mysql_rwlock_unlock(&LOCK_logger); }
  bool is_log_table_enabled(uint log_table_type);
  bool log_command(THD *thd, enum enum_server_command command,
                   const char *query_str, size_t query_length);

  /*
    We want to initialize all log mutexes as soon as possible,
    but we cannot do it in constructor, as safe_mutex relies on
    initialization, performed by MY_INIT(). This why this is done in
    this function.
  */
  void init_base();
  void init_log_tables();
  bool flush_logs(THD *thd);
  bool flush_slow_log();
  bool flush_general_log();
  /* Perform basic logger cleanup. this will leave e.g. error log open. */
  void cleanup_base();
  /* Free memory. Nothing could be logged after this function is called */
  void cleanup_end();
  bool error_log_print(enum loglevel level, const char *format,
                      va_list args);
  bool slow_log_print(THD *thd, const char *query, uint query_length);
  bool general_log_print(THD *thd,enum enum_server_command command,
                         const char *format, va_list args);
  bool general_log_write(THD *thd, enum enum_server_command command,
                         const char *query, uint query_length);

  /* we use this function to setup all enabled log event handlers */
  int set_handlers(uint error_log_printer,
                   uint slow_log_printer,
                   uint general_log_printer);
  void init_error_log(uint error_log_printer);
  void init_slow_log(uint slow_log_printer);
  void init_general_log(uint general_log_printer);
  void deactivate_log_handler(THD* thd, uint log_type);
  bool activate_log_handler(THD* thd, uint log_type);
  MYSQL_QUERY_LOG *get_slow_log_file_handler() const
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_slow_log();
    return NULL;
  }
  MYSQL_QUERY_LOG *get_log_file_handler() const
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_log();
    return NULL;
  }
};

enum enum_binlog_row_image {
  /** PKE in the before image and changed columns in the after image */
  BINLOG_ROW_IMAGE_MINIMAL= 0,
  /** Whenever possible, before and after image contain all columns except blobs. */
  BINLOG_ROW_IMAGE_NOBLOB= 1,
  /** All columns in both before and after image. */
  BINLOG_ROW_IMAGE_FULL= 2
};

enum enum_binlog_format {
  BINLOG_FORMAT_MIXED= 0, ///< statement if safe, otherwise row - autodetected
  BINLOG_FORMAT_STMT=  1, ///< statement-based
  BINLOG_FORMAT_ROW=   2, ///< row-based
  BINLOG_FORMAT_UNSPEC=3  ///< thd_binlog_format() returns it when binlog is closed
};

void exec_binlog_error_action_abort(const char* err_string);
int query_error_code(THD *thd, bool not_killed);
uint purge_log_get_error_code(int res);

int vprint_msg_to_log(enum loglevel level, const char *format, va_list args);
void sql_print_error(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
void sql_print_warning(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
void sql_print_information(const char *format, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
typedef void (*sql_print_message_func)(const char *format, ...)
  ATTRIBUTE_FORMAT_FPTR(printf, 1, 2);
extern sql_print_message_func sql_print_message_handlers[];

int error_log_print(enum loglevel level, const char *format,
                    va_list args);

bool slow_log_print(THD *thd, const char *query, uint query_length);

bool general_log_print(THD *thd, enum enum_server_command command,
                       const char *format,...);

bool general_log_write(THD *thd, enum enum_server_command command,
                       const char *query, uint query_length);

void sql_perror(const char *message);
bool flush_error_log();

char *make_log_name(char *buff, const char *name, const char* log_ext);

/**
  Check given log name against certain blacklisted names/extensions.

  @param name     Log name to check
  @param len      Length of log name

  @returns true if name is valid, false otherwise.
*/
bool is_valid_log_name(const char *name, size_t len);

extern LOGGER logger;

#endif /* LOG_H */
