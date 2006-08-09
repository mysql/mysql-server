/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LOG_H
#define LOG_H

struct st_relay_log_info;

class Format_description_log_event;

/*
  Transaction Coordinator log - a base abstract class
  for two different implementations
*/
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() {}
  virtual ~TC_LOG() {}

  virtual int open(const char *opt_name)=0;
  virtual void close()=0;
  virtual int log(THD *thd, my_xid xid)=0;
  virtual void unlog(ulong cookie, my_xid xid)=0;
};

class TC_LOG_DUMMY: public TC_LOG // use it to disable the logging
{
public:
  TC_LOG_DUMMY() {}
  int open(const char *opt_name)        { return 0; }
  void close()                          { }
  int log(THD *thd, my_xid xid)         { return 1; }
  void unlog(ulong cookie, my_xid xid)  { }
};

#ifdef HAVE_MMAP
class TC_LOG_MMAP: public TC_LOG
{
  public:                // only to keep Sun Forte on sol9x86 happy
  typedef enum {
    POOL,                 // page is in pool
    ERROR,                // last sync failed
    DIRTY                 // new xids added since last sync
  } PAGE_STATE;

  private:
  typedef struct st_page {
    struct st_page *next; // page a linked in a fifo queue
    my_xid *start, *end;  // usable area of a page
    my_xid *ptr;          // next xid will be written here
    int size, free;       // max and current number of free xid slots on the page
    int waiters;          // number of waiters on condition
    PAGE_STATE state;     // see above
    pthread_mutex_t lock; // to access page data or control structure
    pthread_cond_t  cond; // to wait for a sync
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
  pthread_mutex_t LOCK_active, LOCK_pool, LOCK_sync;
  pthread_cond_t COND_pool, COND_active;

  public:
  TC_LOG_MMAP(): inited(0) {}
  int open(const char *opt_name);
  void close();
  int log(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover();

  private:
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

/* bitmap to SQL_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4

struct st_relay_log_info;

typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  pthread_mutex_t lock;
  st_log_info():fatal(0) { pthread_mutex_init(&lock, MY_MUTEX_INIT_FAST);}
  ~st_log_info() { pthread_mutex_destroy(&lock);}
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

class Log_event;
class Rows_log_event;

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
  bool open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
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
  pthread_mutex_t LOCK_log;
  char *name;
  char log_file_name[FN_REFLEN];
  char time_buff[20], db[NAME_LEN + 1];
  bool write_error, inited;
  IO_CACHE log_file;
  enum_log_type log_type;
  volatile enum_log_state log_state;
  enum cache_type io_cache_type;
  friend class Log_event;
};

class MYSQL_QUERY_LOG: public MYSQL_LOG
{
public:
  MYSQL_QUERY_LOG() : last_time(0) {}
  void reopen_file();
  bool write(time_t event_time, const char *user_host,
             uint user_host_len, int thread_id,
             const char *command_type, uint command_type_len,
             const char *sql_text, uint sql_text_len);
  bool write(THD *thd, time_t current_time, time_t query_start_arg,
             const char *user_host, uint user_host_len,
             longlong query_time, longlong lock_time, bool is_command,
             const char *sql_text, uint sql_text_len);
  bool open_slow_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(generate_name(log_name, "-slow.log", 0, buf), LOG_NORMAL, 0,
                WRITE_CACHE);
  }
  bool open_query_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(generate_name(log_name, ".log", 0, buf), LOG_NORMAL, 0,
                WRITE_CACHE);
  }
private:
  time_t last_time;
};

class MYSQL_BIN_LOG: public TC_LOG, private MYSQL_LOG
{
 private:
  /* LOCK_log and LOCK_index are inited by init_pthread_objects() */
  pthread_mutex_t LOCK_index;
  pthread_mutex_t LOCK_prep_xids;
  pthread_cond_t  COND_prep_xids;
  pthread_cond_t update_cond;
  ulonglong bytes_written;
  IO_CACHE index_file;
  char index_file_name[FN_REFLEN];
  /*
     The max size before rotation (usable only if log_type == LOG_BIN: binary
     logs and relay logs).
     For a binlog, max_size should be max_binlog_size.
     For a relay log, it should be max_relay_log_size if this is non-zero,
     max_binlog_size otherwise.
     max_size is set in init(), and dynamically changed (when one does SET
     GLOBAL MAX_BINLOG_SIZE|MAX_RELAY_LOG_SIZE) by fix_max_binlog_size and
     fix_max_relay_log_size).
  */
  ulong max_size;
  ulong prepared_xids; /* for tc log - number of xids to remember */
  // current file sequence number for load data infile binary logging
  uint file_id;
  uint open_count;				// For replication
  int readers_count;
  bool need_start_event;
  /*
    no_auto_events means we don't want any of these automatic events :
    Start/Rotate/Stop. That is, in 4.x when we rotate a relay log, we don't
    want a Rotate_log event to be written to the relay log. When we start a
    relay log etc. So in 4.x this is 1 for relay logs, 0 for binlogs.
    In 5.0 it's 0 for relay logs too!
  */
  bool no_auto_events;

  ulonglong m_table_map_version;

  int write_to_file(IO_CACHE *cache);
  /*
    This is used to start writing to a new log file. The difference from
    new_file() is locking. new_file_without_locking() does not acquire
    LOCK_log.
  */
  void new_file_without_locking();
  void new_file_impl(bool need_lock);

public:
  MYSQL_LOG::generate_name;
  MYSQL_LOG::is_open;
  /*
    These describe the log's format. This is used only for relay logs.
    _for_exec is used by the SQL thread, _for_queue by the I/O thread. It's
    necessary to have 2 distinct objects, because the I/O thread may be reading
    events in a different format from what the SQL thread is reading (consider
    the case of a master which has been upgraded from 5.0 to 5.1 without doing
    RESET MASTER, or from 4.x to 5.0).
  */
  Format_description_log_event *description_event_for_exec,
    *description_event_for_queue;

  MYSQL_BIN_LOG();
  /*
    note that there's no destructor ~MYSQL_BIN_LOG() !
    The reason is that we don't want it to be automatically called
    on exit() - but only during the correct shutdown process
  */

  int open(const char *opt_name);
  void close();
  int log(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover(IO_CACHE *log, Format_description_log_event *fdle);
#if !defined(MYSQL_CLIENT)
  bool is_table_mapped(TABLE *table) const
  {
    return table->s->table_map_version == table_map_version();
  }

  ulonglong table_map_version() const { return m_table_map_version; }
  void update_table_map_version() { ++m_table_map_version; }

  int flush_and_set_pending_rows_event(THD *thd, Rows_log_event* event);

#endif /* !defined(MYSQL_CLIENT) */
  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(ulonglong* counter)
  {
#ifndef DBUG_OFF
    char buf1[22],buf2[22];
#endif
    DBUG_ENTER("harvest_bytes_written");
    (*counter)+=bytes_written;
    DBUG_PRINT("info",("counter: %s  bytes_written: %s", llstr(*counter,buf1),
		       llstr(bytes_written,buf2)));
    bytes_written=0;
    DBUG_VOID_RETURN;
  }
  void set_max_size(ulong max_size_arg);
  void signal_update();
  void wait_for_update(THD* thd, bool master_or_slave);
  void set_need_start_event() { need_start_event = 1; }
  void init(bool no_auto_events_arg, ulong max_size);
  void init_pthread_objects();
  void cleanup();
  bool open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
	    enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg, ulong max_size,
            bool null_created);
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name);
  /* Use this to start writing a new log file */
  void new_file();

  bool write(Log_event* event_info); // binary log write
  bool write(THD *thd, IO_CACHE *cache, Log_event *commit_event);

  void start_union_events(THD *thd);
  void stop_union_events(THD *thd);
  bool is_query_in_union(THD *thd, query_id_t query_id_param);

  /*
    v stands for vector
    invoked as appendv(buf1,len1,buf2,len2,...,bufn,lenn,0)
  */
  bool appendv(const char* buf,uint len,...);
  bool append(Log_event* ev);

  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  int update_log_index(LOG_INFO* linfo, bool need_update_threads);
  void rotate_and_purge(uint flags);
  bool flush_and_sync();
  int purge_logs(const char *to_log, bool included,
                 bool need_mutex, bool need_update_threads,
                 ulonglong *decrease_log_space);
  int purge_logs_before_date(time_t purge_time);
  int purge_first_log(struct st_relay_log_info* rli, bool included);
  bool reset_logs(THD* thd);
  void close(uint exiting);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
		   bool need_mutex);
  int find_next_log(LOG_INFO* linfo, bool need_mutex);
  int get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline char* get_name() { return name; }
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
};

class Log_event_handler
{
public:
  Log_event_handler() {}
  virtual bool init()= 0;
  virtual void cleanup()= 0;

  virtual bool log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, longlong query_time,
                        longlong lock_time, bool is_command,
                        const char *sql_text, uint sql_text_len)= 0;
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args)= 0;
  virtual bool log_general(time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           CHARSET_INFO *client_cs)= 0;
  virtual ~Log_event_handler() {}
};


class Log_to_csv_event_handler: public Log_event_handler
{
  /*
    We create artificial THD for each of the logs. This is to avoid
    locking issues: we don't want locks on the log tables reside in the
    THD's of the query. The reason is the locking order and duration.
  */
  THD *general_log_thd, *slow_log_thd;
  friend class LOGGER;
  TABLE_LIST general_log, slow_log;

private:
  bool open_log_table(uint log_type);

public:
  Log_to_csv_event_handler();
  ~Log_to_csv_event_handler();
  virtual bool init();
  virtual void cleanup();

  virtual bool log_slow(THD *thd, time_t current_time,
                        time_t query_start_arg, const char *user_host,
                        uint user_host_len, longlong query_time,
                        longlong lock_time, bool is_command,
                        const char *sql_text, uint sql_text_len);
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual bool log_general(time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                            CHARSET_INFO *client_cs);
  bool flush(THD *thd, TABLE_LIST *close_slow_Log,
             TABLE_LIST* close_general_log);
  void close_log_table(uint log_type, bool lock_in_use);
  bool reopen_log_table(uint log_type);
};


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
                        uint user_host_len, longlong query_time,
                        longlong lock_time, bool is_command,
                        const char *sql_text, uint sql_text_len);
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args);
  virtual bool log_general(time_t event_time, const char *user_host,
                           uint user_host_len, int thread_id,
                           const char *command_type, uint command_type_len,
                           const char *sql_text, uint sql_text_len,
                           CHARSET_INFO *client_cs);
  void flush();
  void init_pthread_objects();
  MYSQL_QUERY_LOG *get_mysql_slow_log() { return &mysql_slow_log; }
  MYSQL_QUERY_LOG *get_mysql_log() { return &mysql_log; }
};


/* Class which manages slow, general and error log event handlers */
class LOGGER
{
  pthread_mutex_t LOCK_logger;
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
  void lock() { (void) pthread_mutex_lock(&LOCK_logger); }
  void unlock() { (void) pthread_mutex_unlock(&LOCK_logger); }
  bool is_general_log_table_enabled()
  {
    return table_log_handler && table_log_handler->general_log.table != 0;
  }
  bool is_slow_log_table_enabled()
  {
    return table_log_handler && table_log_handler->slow_log.table != 0;
  }
  /*
    We want to initialize all log mutexes as soon as possible,
    but we cannot do it in constructor, as safe_mutex relies on
    initialization, performed by MY_INIT(). This why this is done in
    this function.
  */
  void init_base();
  void init_log_tables();
  bool flush_logs(THD *thd);
  THD *get_general_log_thd()
  {
    if (table_log_handler)
      return (THD *) table_log_handler->general_log_thd;
    else
      return NULL;
  }
  THD *get_slow_log_thd()
  {
    if (table_log_handler)
      return (THD *) table_log_handler->slow_log_thd;
    else
      return NULL;
  }
  /* Perform basic logger cleanup. this will leave e.g. error log open. */
  void cleanup_base();
  /* Free memory. Nothing could be logged after this function is called */
  void cleanup_end();
  bool error_log_print(enum loglevel level, const char *format,
                      va_list args);
  bool slow_log_print(THD *thd, const char *query, uint query_length,
                      time_t query_start_arg);
  bool general_log_print(THD *thd,enum enum_server_command command,
                         const char *format, va_list args);

  void close_log_table(uint log_type, bool lock_in_use);
  bool reopen_log_table(uint log_type);

  /* we use this function to setup all enabled log event handlers */
  int set_handlers(uint error_log_printer,
                   uint slow_log_printer,
                   uint general_log_printer);
  void init_error_log(uint error_log_printer);
  void init_slow_log(uint slow_log_printer);
  void init_general_log(uint general_log_printer);
  void deactivate_log_handler(THD* thd, uint log_type);
  bool activate_log_handler(THD* thd, uint log_type);
  MYSQL_QUERY_LOG *get_slow_log_file_handler()
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_slow_log();
    return NULL;
  }
  MYSQL_QUERY_LOG *get_log_file_handler()
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_log();
    return NULL;
  }
};

enum enum_binlog_format {
  BINLOG_FORMAT_STMT= 0, // statement-based
#ifdef HAVE_ROW_BASED_REPLICATION
  BINLOG_FORMAT_ROW= 1, // row_based
  /*
    statement-based except for cases where only row-based can work (UUID()
    etc):
  */
  BINLOG_FORMAT_MIXED= 2,
#endif
/*
  This value is last, after the end of binlog_format_typelib: it has no
  corresponding cell in this typelib. We use this value to be able to know if
  the user has explicitely specified a binlog format at startup or not.
*/
  BINLOG_FORMAT_UNSPEC= 3
};
extern TYPELIB binlog_format_typelib;

#endif /* LOG_H */
