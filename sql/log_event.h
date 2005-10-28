/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#ifndef _log_event_h
#define _log_event_h

#ifdef __EMX__
#undef write  // remove pthread.h macro definition, conflict with write() class member
#endif

#if defined(USE_PRAGMA_INTERFACE) && !defined(MYSQL_CLIENT)
#pragma interface			/* gcc class implementation */
#endif

#define LOG_READ_EOF    -1
#define LOG_READ_BOGUS  -2
#define LOG_READ_IO     -3
#define LOG_READ_MEM    -5
#define LOG_READ_TRUNC  -6
#define LOG_READ_TOO_LARGE -7

#define LOG_EVENT_OFFSET 4

/*
   3 is MySQL 4.x; 4 is MySQL 5.0.0.
   Compared to version 3, version 4 has:
   - a different Start_log_event, which includes info about the binary log
   (sizes of headers); this info is included for better compatibility if the
   master's MySQL version is different from the slave's.
   - all events have a unique ID (the triplet (server_id, timestamp at server
   start, other) to be sure an event is not executed more than once in a
   multimaster setup, example:
                M1
              /   \
             v     v
             M2    M3
             \     /
              v   v
                S
   if a query is run on M1, it will arrive twice on S, so we need that S
   remembers the last unique ID it has processed, to compare and know if the
   event should be skipped or not. Example of ID: we already have the server id
   (4 bytes), plus:
   timestamp_when_the_master_started (4 bytes), a counter (a sequence number
   which increments every time we write an event to the binlog) (3 bytes).
   Q: how do we handle when the counter is overflowed and restarts from 0 ?

   - Query and Load (Create or Execute) events may have a more precise timestamp
   (with microseconds), number of matched/affected/warnings rows
   and fields of session variables: SQL_MODE,
   FOREIGN_KEY_CHECKS, UNIQUE_CHECKS, SQL_AUTO_IS_NULL, the collations and
   charsets, the PASSWORD() version (old/new/...).
*/
#define BINLOG_VERSION    4

/*
 We could have used SERVER_VERSION_LENGTH, but this introduces an
 obscure dependency - if somebody decided to change SERVER_VERSION_LENGTH
 this would break the replication protocol
*/
#define ST_SERVER_VER_LEN 50

/*
  These are flags and structs to handle all the LOAD DATA INFILE options (LINES
  TERMINATED etc).
*/

/*
  These are flags and structs to handle all the LOAD DATA INFILE options (LINES
  TERMINATED etc).
  DUMPFILE_FLAG is probably useless (DUMPFILE is a clause of SELECT, not of LOAD
  DATA).
*/
#define DUMPFILE_FLAG		0x1
#define OPT_ENCLOSED_FLAG	0x2
#define REPLACE_FLAG		0x4
#define IGNORE_FLAG		0x8

#define FIELD_TERM_EMPTY	0x1
#define ENCLOSED_EMPTY		0x2
#define LINE_TERM_EMPTY		0x4
#define LINE_START_EMPTY	0x8
#define ESCAPED_EMPTY		0x10

/*****************************************************************************

  old_sql_ex struct

 ****************************************************************************/
struct old_sql_ex
{
  char field_term;
  char enclosed;
  char line_term;
  char line_start;
  char escaped;
  char opt_flags;
  char empty_flags;
};

#define NUM_LOAD_DELIM_STRS 5

/*****************************************************************************

  sql_ex_info struct

 ****************************************************************************/
struct sql_ex_info
{
  char* field_term;
  char* enclosed;
  char* line_term;
  char* line_start;
  char* escaped;
  int cached_new_format;
  uint8 field_term_len,enclosed_len,line_term_len,line_start_len, escaped_len;
  char opt_flags;
  char empty_flags;

  // store in new format even if old is possible
  void force_new_format() { cached_new_format = 1;}
  int data_size()
  {
    return (new_format() ?
	    field_term_len + enclosed_len + line_term_len +
	    line_start_len + escaped_len + 6 : 7);
  }
  bool write_data(IO_CACHE* file);
  char* init(char* buf,char* buf_end,bool use_new_format);
  bool new_format()
  {
    return ((cached_new_format != -1) ? cached_new_format :
	    (cached_new_format=(field_term_len > 1 ||
				enclosed_len > 1 ||
				line_term_len > 1 || line_start_len > 1 ||
				escaped_len > 1)));
  }
};

/*****************************************************************************

  MySQL Binary Log

  This log consists of events.  Each event has a fixed-length header,
  possibly followed by a variable length data body.

  The data body consists of an optional fixed length segment (post-header)
  and  an optional variable length segment.

  See the #defines below for the format specifics.

  The events which really update data are Query_log_event,
  Execute_load_query_log_event and old Load_log_event and
  Execute_load_log_event events (Execute_load_query is used together with
  Begin_load_query and Append_block events to replicate LOAD DATA INFILE.
  Create_file/Append_block/Execute_load (which includes Load_log_event)
  were used to replicate LOAD DATA before the 5.0.3).

 ****************************************************************************/

#define LOG_EVENT_HEADER_LEN 19     /* the fixed header length */
#define OLD_HEADER_LEN       13     /* the fixed header length in 3.23 */
/*
   Fixed header length, where 4.x and 5.0 agree. That is, 5.0 may have a longer
   header (it will for sure when we have the unique event's ID), but at least
   the first 19 bytes are the same in 4.x and 5.0. So when we have the unique
   event's ID, LOG_EVENT_HEADER_LEN will be something like 26, but
   LOG_EVENT_MINIMAL_HEADER_LEN will remain 19.
*/
#define LOG_EVENT_MINIMAL_HEADER_LEN 19

/* event-specific post-header sizes */
// where 3.23, 4.x and 5.0 agree
#define QUERY_HEADER_MINIMAL_LEN     (4 + 4 + 1 + 2)
// where 5.0 differs: 2 for len of N-bytes vars.
#define QUERY_HEADER_LEN     (QUERY_HEADER_MINIMAL_LEN + 2)
#define LOAD_HEADER_LEN      (4 + 4 + 4 + 1 +1 + 4)
#define START_V3_HEADER_LEN     (2 + ST_SERVER_VER_LEN + 4)
#define ROTATE_HEADER_LEN    8 // this is FROZEN (the Rotate post-header is frozen)
#define CREATE_FILE_HEADER_LEN 4
#define APPEND_BLOCK_HEADER_LEN 4
#define EXEC_LOAD_HEADER_LEN   4
#define DELETE_FILE_HEADER_LEN 4
#define FORMAT_DESCRIPTION_HEADER_LEN (START_V3_HEADER_LEN+1+LOG_EVENT_TYPES)
#define EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN (4 + 4 + 4 + 1)
#define EXECUTE_LOAD_QUERY_HEADER_LEN  (QUERY_HEADER_LEN + EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN)

/*
   Event header offsets;
   these point to places inside the fixed header.
*/

#define EVENT_TYPE_OFFSET    4
#define SERVER_ID_OFFSET     5
#define EVENT_LEN_OFFSET     9
#define LOG_POS_OFFSET       13
#define FLAGS_OFFSET         17

/* start event post-header (for v3 and v4) */

#define ST_BINLOG_VER_OFFSET  0
#define ST_SERVER_VER_OFFSET  2
#define ST_CREATED_OFFSET     (ST_SERVER_VER_OFFSET + ST_SERVER_VER_LEN)
#define ST_COMMON_HEADER_LEN_OFFSET (ST_CREATED_OFFSET + 4)

/* slave event post-header (this event is never written) */

#define SL_MASTER_PORT_OFFSET   8
#define SL_MASTER_POS_OFFSET    0
#define SL_MASTER_HOST_OFFSET   10

/* query event post-header */

#define Q_THREAD_ID_OFFSET	0
#define Q_EXEC_TIME_OFFSET	4
#define Q_DB_LEN_OFFSET		8
#define Q_ERR_CODE_OFFSET	9
#define Q_STATUS_VARS_LEN_OFFSET 11
#define Q_DATA_OFFSET		QUERY_HEADER_LEN
/* these are codes, not offsets; not more than 256 values (1 byte). */
#define Q_FLAGS2_CODE           0
#define Q_SQL_MODE_CODE         1
/*
  Q_CATALOG_CODE is catalog with end zero stored; it is used only by MySQL
  5.0.x where 0<=x<=3. We have to keep it to be able to replicate these
  old masters.
*/
#define Q_CATALOG_CODE          2
#define Q_AUTO_INCREMENT	3
#define Q_CHARSET_CODE          4
#define Q_TIME_ZONE_CODE        5
/*
  Q_CATALOG_NZ_CODE is catalog withOUT end zero stored; it is used by MySQL
  5.0.x where x>=4. Saves one byte in every Query_log_event in binlog,
  compared to Q_CATALOG_CODE. The reason we didn't simply re-use
  Q_CATALOG_CODE is that then a 5.0.3 slave of this 5.0.x (x>=4) master would
  crash (segfault etc) because it would expect a 0 when there is none.
*/
#define Q_CATALOG_NZ_CODE       6

/* Intvar event post-header */

#define I_TYPE_OFFSET        0
#define I_VAL_OFFSET         1

/* Rand event post-header */

#define RAND_SEED1_OFFSET 0
#define RAND_SEED2_OFFSET 8

/* User_var event post-header */

#define UV_VAL_LEN_SIZE        4
#define UV_VAL_IS_NULL         1
#define UV_VAL_TYPE_SIZE       1
#define UV_NAME_LEN_SIZE       4
#define UV_CHARSET_NUMBER_SIZE 4

/* Load event post-header */

#define L_THREAD_ID_OFFSET   0
#define L_EXEC_TIME_OFFSET   4
#define L_SKIP_LINES_OFFSET  8
#define L_TBL_LEN_OFFSET     12
#define L_DB_LEN_OFFSET      13
#define L_NUM_FIELDS_OFFSET  14
#define L_SQL_EX_OFFSET      18
#define L_DATA_OFFSET        LOAD_HEADER_LEN

/* Rotate event post-header */

#define R_POS_OFFSET       0
#define R_IDENT_OFFSET     8

/* CF to DF handle LOAD DATA INFILE */

/* CF = "Create File" */
#define CF_FILE_ID_OFFSET  0
#define CF_DATA_OFFSET     CREATE_FILE_HEADER_LEN

/* AB = "Append Block" */
#define AB_FILE_ID_OFFSET  0
#define AB_DATA_OFFSET     APPEND_BLOCK_HEADER_LEN

/* EL = "Execute Load" */
#define EL_FILE_ID_OFFSET  0

/* DF = "Delete File" */
#define DF_FILE_ID_OFFSET  0

/* ELQ = "Execute Load Query" */
#define ELQ_FILE_ID_OFFSET QUERY_HEADER_LEN
#define ELQ_FN_POS_START_OFFSET ELQ_FILE_ID_OFFSET + 4
#define ELQ_FN_POS_END_OFFSET ELQ_FILE_ID_OFFSET + 8
#define ELQ_DUP_HANDLING_OFFSET ELQ_FILE_ID_OFFSET + 12

/* 4 bytes which all binlogs should begin with */
#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

/*
  The 2 flags below were useless :
  - the first one was never set
  - the second one was set in all Rotate events on the master, but not used for
  anything useful.
  So they are now removed and their place may later be reused for other
  flags. Then one must remember that Rotate events in 4.x have
  LOG_EVENT_FORCED_ROTATE_F set, so one should not rely on the value of the
  replacing flag when reading a Rotate event.
  I keep the defines here just to remember what they were.
*/
#ifdef TO_BE_REMOVED
#define LOG_EVENT_TIME_F            0x1
#define LOG_EVENT_FORCED_ROTATE_F   0x2
#endif

/*
   This flag only makes sense for Format_description_log_event. It is set
   when the event is written, and *reset* when a binlog file is
   closed (yes, it's the only case when MySQL modifies already written
   part of binlog).  Thus it is a reliable indicator that binlog was
   closed correctly.  (Stop_log_event is not enough, there's always a
   small chance that mysqld crashes in the middle of insert and end of
   the binlog would look like a Stop_log_event).

   This flag is used to detect a restart after a crash, and to provide
   "unbreakable" binlog. The problem is that on a crash storage engines
   rollback automatically, while binlog does not.  To solve this we use this
   flag and automatically append ROLLBACK to every non-closed binlog (append
   virtually, on reading, file itself is not changed). If this flag is found,
   mysqlbinlog simply prints "ROLLBACK" Replication master does not abort on
   binlog corruption, but takes it as EOF, and replication slave forces a
   rollback in this case.

   Note, that old binlogs does not have this flag set, so we get a
   a backward-compatible behaviour.
*/

#define LOG_EVENT_BINLOG_IN_USE_F       0x1

/*
   If the query depends on the thread (for example: TEMPORARY TABLE).
   Currently this is used by mysqlbinlog to know it must print
   SET @@PSEUDO_THREAD_ID=xx; before the query (it would not hurt to print it
   for every query but this would be slow).
*/
#define LOG_EVENT_THREAD_SPECIFIC_F 0x4

/*
  Suppress the generation of 'USE' statements before the actual
  statement. This flag should be set for any events that does not need
  the current database set to function correctly. Most notable cases
  are 'CREATE DATABASE' and 'DROP DATABASE'.

  This flags should only be used in exceptional circumstances, since
  it introduce a significant change in behaviour regarding the
  replication logic together with the flags --binlog-do-db and
  --replicated-do-db.
 */
#define LOG_EVENT_SUPPRESS_USE_F    0x8

/*
   OPTIONS_WRITTEN_TO_BIN_LOG are the bits of thd->options which must be
   written to the binlog. OPTIONS_WRITTEN_TO_BINLOG could be written
   into the Format_description_log_event, so that if later we don't want
   to replicate a variable we did replicate, or the contrary, it's
   doable. But it should not be too hard to decide once for all of what
   we replicate and what we don't, among the fixed 32 bits of
   thd->options.
   I (Guilhem) have read through every option's usage, and it looks like
   OPTION_AUTO_IS_NULL and OPTION_NO_FOREIGN_KEYS are the only ones
   which alter how the query modifies the table. It's good to replicate
   OPTION_RELAXED_UNIQUE_CHECKS too because otherwise, the slave may
   insert data slower than the master, in InnoDB.
   OPTION_BIG_SELECTS is not needed (the slave thread runs with
   max_join_size=HA_POS_ERROR) and OPTION_BIG_TABLES is not needed
   either, as the manual says (because a too big in-memory temp table is
   automatically written to disk).
*/
#define OPTIONS_WRITTEN_TO_BIN_LOG (OPTION_AUTO_IS_NULL | \
OPTION_NO_FOREIGN_KEY_CHECKS | OPTION_RELAXED_UNIQUE_CHECKS)

#if OPTIONS_WRITTEN_TO_BIN_LOG != ((1L << 14) | (1L << 26) | (1L << 27))
#error OPTIONS_WRITTEN_TO_BIN_LOG must NOT change their values!
#endif

enum Log_event_type
{
  /*
    Every time you update this enum (when you add a type), you have to
    fix Format_description_log_event::Format_description_log_event().
  */
  UNKNOWN_EVENT= 0,
  START_EVENT_V3= 1,
  QUERY_EVENT= 2,
  STOP_EVENT= 3,
  ROTATE_EVENT= 4,
  INTVAR_EVENT= 5,
  LOAD_EVENT= 6,
  SLAVE_EVENT= 7,
  CREATE_FILE_EVENT= 8,
  APPEND_BLOCK_EVENT= 9,
  EXEC_LOAD_EVENT= 10,
  DELETE_FILE_EVENT= 11,
  /*
    NEW_LOAD_EVENT is like LOAD_EVENT except that it has a longer
    sql_ex, allowing multibyte TERMINATED BY etc; both types share the
    same class (Load_log_event)
  */
  NEW_LOAD_EVENT= 12,
  RAND_EVENT= 13,
  USER_VAR_EVENT= 14,
  FORMAT_DESCRIPTION_EVENT= 15,
  XID_EVENT= 16,
  BEGIN_LOAD_QUERY_EVENT= 17,
  EXECUTE_LOAD_QUERY_EVENT= 18,

  /*
    Add new events here - right above this comment!
    Existing events (except ENUM_END_EVENT) should never change their numbers
  */

  ENUM_END_EVENT /* end marker */
};

/*
   The number of types we handle in Format_description_log_event (UNKNOWN_EVENT
   is not to be handled, it does not exist in binlogs, it does not have a
   format).
*/
#define LOG_EVENT_TYPES (ENUM_END_EVENT-1)

enum Int_event_type
{
  INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
};


#ifndef MYSQL_CLIENT
class String;
class MYSQL_LOG;
class THD;
#endif

class Format_description_log_event;

struct st_relay_log_info;

#ifdef MYSQL_CLIENT
/*
  A structure for mysqlbinlog to know how to print events

  This structure is passed to the event's print() methods,

  There are two types of settings stored here:
  1. Last db, flags2, sql_mode etc comes from the last printed event.
     They are stored so that only the necessary USE and SET commands
     are printed.
  2. Other information on how to print the events, e.g. short_form,
     hexdump_from.  These are not dependent on the last event.
*/
typedef struct st_print_event_info
{
  /*
    Settings for database, sql_mode etc that comes from the last event
    that was printed.
   */
  // TODO: have the last catalog here ??
  char db[FN_REFLEN+1]; // TODO: make this a LEX_STRING when thd->db is
  bool flags2_inited;
  uint32 flags2;
  bool sql_mode_inited;
  ulong sql_mode;		/* must be same as THD.variables.sql_mode */
  ulong auto_increment_increment, auto_increment_offset;
  bool charset_inited;
  char charset[6]; // 3 variables, each of them storable in 2 bytes
  char time_zone_str[MAX_TIME_ZONE_NAME_LENGTH];
  st_print_event_info()
    :flags2_inited(0), sql_mode_inited(0),
     auto_increment_increment(1),auto_increment_offset(1), charset_inited(0)
    {
      /*
        Currently we only use static PRINT_EVENT_INFO objects, so zeroed at
        program's startup, but these explicit bzero() is for the day someone
        creates dynamic instances.
      */
      bzero(db, sizeof(db));
      bzero(charset, sizeof(charset));
      bzero(time_zone_str, sizeof(time_zone_str));
    }

  /* Settings on how to print the events */
  bool short_form;
  my_off_t hexdump_from;
  uint8 common_header_len;

} PRINT_EVENT_INFO;
#endif


/*****************************************************************************

  Log_event class

  This is the abstract base class for binary log events.

 ****************************************************************************/
class Log_event
{
public:
  /*
    The offset in the log where this event originally appeared (it is
    preserved in relay logs, making SHOW SLAVE STATUS able to print
    coordinates of the event in the master's binlog). Note: when a
    transaction is written by the master to its binlog (wrapped in
    BEGIN/COMMIT) the log_pos of all the queries it contains is the
    one of the BEGIN (this way, when one does SHOW SLAVE STATUS it
    sees the offset of the BEGIN, which is logical as rollback may
    occur), except the COMMIT query which has its real offset.
  */
  my_off_t log_pos;
  /*
     A temp buffer for read_log_event; it is later analysed according to the
     event's type, and its content is distributed in the event-specific fields.
  */
  char *temp_buf;
  /*
    Timestamp on the master(for debugging and replication of
    NOW()/TIMESTAMP).  It is important for queries and LOAD DATA
    INFILE. This is set at the event's creation time, except for Query
    and Load (et al.) events where this is set at the query's
    execution time, which guarantees good replication (otherwise, we
    could have a query and its event with different timestamps).
  */
  time_t when;
  /* The number of seconds the query took to run on the master. */
  ulong exec_time;
  /* Number of bytes written by write() function */
  ulong data_written;

  /*
    The master's server id (is preserved in the relay log; used to
    prevent from infinite loops in circular replication).
  */
  uint32 server_id;

  /*
    Some 16 flags. Look above for LOG_EVENT_TIME_F,
    LOG_EVENT_FORCED_ROTATE_F, LOG_EVENT_THREAD_SPECIFIC_F, and
    LOG_EVENT_SUPPRESS_USE_F for notes.
  */
  uint16 flags;

  bool cache_stmt;

#ifndef MYSQL_CLIENT
  THD* thd;

  Log_event();
  Log_event(THD* thd_arg, uint16 flags_arg, bool cache_stmt);
  /*
    read_log_event() functions read an event from a binlog or relay
    log; used by SHOW BINLOG EVENTS, the binlog_dump thread on the
    master (reads master's binlog), the slave IO thread (reads the
    event sent by binlog_dump), the slave SQL thread (reads the event
    from the relay log).  If mutex is 0, the read will proceed without
    mutex.  We need the description_event to be able to parse the
    event (to know the post-header's size); in fact in read_log_event
    we detect the event's type, then call the specific event's
    constructor and pass description_event as an argument.
  */
  static Log_event* read_log_event(IO_CACHE* file,
				   pthread_mutex_t* log_lock,
                                   const Format_description_log_event *description_event);
  static int read_log_event(IO_CACHE* file, String* packet,
			    pthread_mutex_t* log_lock);
  /*
    init_show_field_list() prepares the column names and types for the
    output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
    EVENTS.
  */
  static void init_show_field_list(List<Item>* field_list);
#ifdef HAVE_REPLICATION
  int net_send(Protocol *protocol, const char* log_name, my_off_t pos);
  /*
    pack_info() is used by SHOW BINLOG EVENTS; as print() it prepares and sends
    a string to display to the user, so it resembles print().
  */
  virtual void pack_info(Protocol *protocol);
  /*
    The SQL slave thread calls exec_event() to execute the event; this is where
    the slave's data is modified.
  */
  virtual int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
  virtual const char* get_db()
  {
    return thd ? thd->db : 0;
  }
#else
  Log_event() : temp_buf(0) {}
    /* avoid having to link mysqlbinlog against libpthread */
  static Log_event* read_log_event(IO_CACHE* file,
                                   const Format_description_log_event *description_event);
  /* print*() functions are used by mysqlbinlog */
  virtual void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0) = 0;
  void print_timestamp(FILE* file, time_t *ts = 0);
  void print_header(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  static void *operator new(size_t size)
  {
    return (void*) my_malloc((uint)size, MYF(MY_WME|MY_FAE));
  }
  static void operator delete(void *ptr, size_t size)
  {
    my_free((gptr) ptr, MYF(MY_WME|MY_ALLOW_ZERO_PTR));
  }

#ifndef MYSQL_CLIENT
  bool write_header(IO_CACHE* file, ulong data_length);
  virtual bool write(IO_CACHE* file)
  {
    return (write_header(file, get_data_size()) ||
            write_data_header(file) ||
            write_data_body(file));
  }
  virtual bool write_data_header(IO_CACHE* file)
  { return 0; }
  virtual bool write_data_body(IO_CACHE* file __attribute__((unused)))
  { return 0; }
#endif
  virtual Log_event_type get_type_code() = 0;
  virtual bool is_valid() const = 0;
  virtual bool is_artificial_event() { return 0; }
  inline bool get_cache_stmt() { return cache_stmt; }
  Log_event(const char* buf, const Format_description_log_event* description_event);
  virtual ~Log_event() { free_temp_buf();}
  void register_temp_buf(char* buf) { temp_buf = buf; }
  void free_temp_buf()
  {
    if (temp_buf)
    {
      my_free(temp_buf, MYF(0));
      temp_buf = 0;
    }
  }
  /*
    Get event length for simple events. For complicated events the length
    is calculated during write()
  */
  virtual int get_data_size() { return 0;}
  static Log_event* read_log_event(const char* buf, uint event_len,
				   const char **error,
                                   const Format_description_log_event
                                   *description_event);
  /* returns the human readable name of the event's type */
  const char* get_type_str();
};

/*
   One class for each type of event.
   Two constructors for each class:
   - one to create the event for logging (when the server acts as a master),
   called after an update to the database is done,
   which accepts parameters like the query, the database, the options for LOAD
   DATA INFILE...
   - one to create the event from a packet (when the server acts as a slave),
   called before reproducing the update, which accepts parameters (like a
   buffer). Used to read from the master, from the relay log, and in
   mysqlbinlog. This constructor must be format-tolerant.
*/

/*****************************************************************************

  Query Log Event class

  Logs SQL queries

 ****************************************************************************/
class Query_log_event: public Log_event
{
protected:
  char* data_buf;
public:
  const char* query;
  const char* catalog;
  const char* db;
  /*
    If we already know the length of the query string
    we pass it with q_len, so we would not have to call strlen()
    otherwise, set it to 0, in which case, we compute it with strlen()
  */
  uint32 q_len;
  uint32 db_len;
  uint16 error_code;
  ulong thread_id;
  /*
    For events created by Query_log_event::exec_event (and
    Load_log_event::exec_event()) we need the *original* thread id, to be able
    to log the event with the original (=master's) thread id (fix for
    BUG#1686).
  */
  ulong slave_proxy_id;

  /*
    Binlog format 3 and 4 start to differ (as far as class members are
    concerned) from here.
  */

  uint catalog_len;			// <= 255 char; 0 means uninited

  /*
    We want to be able to store a variable number of N-bit status vars:
    (generally N=32; but N=64 for SQL_MODE) a user may want to log the number
    of affected rows (for debugging) while another does not want to lose 4
    bytes in this.
    The storage on disk is the following:
    status_vars_len is part of the post-header,
    status_vars are in the variable-length part, after the post-header, before
    the db & query.
    status_vars on disk is a sequence of pairs (code, value) where 'code' means
    'sql_mode', 'affected' etc. Sometimes 'value' must be a short string, so
    its first byte is its length. For now the order of status vars is:
    flags2 - sql_mode - catalog - autoinc - charset
    We should add the same thing to Load_log_event, but in fact
    LOAD DATA INFILE is going to be logged with a new type of event (logging of
    the plain text query), so Load_log_event would be frozen, so no need. The
    new way of logging LOAD DATA INFILE would use a derived class of
    Query_log_event, so automatically benefit from the work already done for
    status variables in Query_log_event.
 */
  uint16 status_vars_len;

  /*
    'flags2' is a second set of flags (on top of those in Log_event), for
    session variables. These are thd->options which is & against a mask
    (OPTIONS_WRITTEN_TO_BINLOG).
    flags2_inited helps make a difference between flags2==0 (3.23 or 4.x
    master, we don't know flags2, so use the slave server's global options) and
    flags2==0 (5.0 master, we know this has a meaning of flags all down which
    must influence the query).
  */
  bool flags2_inited;
  bool sql_mode_inited;
  bool charset_inited;

  uint32 flags2;
  /* In connections sql_mode is 32 bits now but will be 64 bits soon */
  ulong sql_mode;
  ulong auto_increment_increment, auto_increment_offset;
  char charset[6];
  uint time_zone_len; /* 0 means uninited */
  const char *time_zone_str;

#ifndef MYSQL_CLIENT

  Query_log_event(THD* thd_arg, const char* query_arg, ulong query_length,
		  bool using_trans, bool suppress_use);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
  int exec_event(struct st_relay_log_info* rli, const char *query_arg,
                 uint32 q_len_arg);
#endif /* HAVE_REPLICATION */
#else
  void print_query_header(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Query_log_event(const char* buf, uint event_len,
                  const Format_description_log_event *description_event,
                  Log_event_type event_type);
  ~Query_log_event()
  {
    if (data_buf)
      my_free((gptr) data_buf, MYF(0));
  }
  Log_event_type get_type_code() { return QUERY_EVENT; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
  virtual bool write_post_header_for_derived(IO_CACHE* file) { return FALSE; }
#endif
  bool is_valid() const { return query != 0; }

  /*
    Returns number of bytes additionaly written to post header by derived
    events (so far it is only Execute_load_query event).
  */
  virtual ulong get_post_header_size_for_derived() { return 0; }
  /* Writes derived event-specific part of post header. */
};

#ifdef HAVE_REPLICATION

/*****************************************************************************

  Slave Log Event class
  Note that this class is currently not used at all; no code writes a
  Slave_log_event (though some code in repl_failsafe.cc reads Slave_log_event).
  So it's not a problem if this code is not maintained.

 ****************************************************************************/
class Slave_log_event: public Log_event
{
protected:
  char* mem_pool;
  void init_from_mem_pool(int data_size);
public:
  my_off_t master_pos;
  char* master_host;
  char* master_log;
  int master_host_len;
  int master_log_len;
  uint16 master_port;

#ifndef MYSQL_CLIENT
  Slave_log_event(THD* thd_arg, struct st_relay_log_info* rli);
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Slave_log_event(const char* buf, uint event_len);
  ~Slave_log_event();
  int get_data_size();
  bool is_valid() const { return master_host != 0; }
  Log_event_type get_type_code() { return SLAVE_EVENT; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
};

#endif /* HAVE_REPLICATION */


/*****************************************************************************

  Load Log Event class

 ****************************************************************************/
class Load_log_event: public Log_event
{
private:
  uint get_query_buffer_length();
  void print_query(bool need_db, char *buf, char **end,
                   char **fn_start, char **fn_end);
protected:
  int copy_log_event(const char *buf, ulong event_len,
                     int body_offset, const Format_description_log_event* description_event);

public:
  ulong thread_id;
  ulong slave_proxy_id;
  uint32 table_name_len;
  /*
    No need to have a catalog, as these events can only come from 4.x.
    TODO: this may become false if Dmitri pushes his new LOAD DATA INFILE in
    5.0 only (not in 4.x).
  */
  uint32 db_len;
  uint32 fname_len;
  uint32 num_fields;
  const char* fields;
  const uchar* field_lens;
  uint32 field_block_len;

  const char* table_name;
  const char* db;
  const char* fname;
  uint32 skip_lines;
  sql_ex_info sql_ex;
  bool local_fname;

  /* fname doesn't point to memory inside Log_event::temp_buf  */
  void set_fname_outside_temp_buf(const char *afname, uint alen)
  {
    fname= afname;
    fname_len= alen;
    local_fname= TRUE;
  }
  /* fname doesn't point to memory inside Log_event::temp_buf  */
  int  check_fname_outside_temp_buf()
  {
    return local_fname;
  }

#ifndef MYSQL_CLIENT
  String field_lens_buf;
  String fields_buf;

  Load_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
		 const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup, bool ignore,
		 bool using_trans);
  void set_fields(const char* db, List<Item> &fields_arg,
                  Name_resolution_context *context);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli)
  {
    return exec_event(thd->slave_net,rli,0);
  }
  int exec_event(NET* net, struct st_relay_log_info* rli,
		 bool use_rli_only_for_errors);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info = 0);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info, bool commented);
#endif

  /*
    Note that for all the events related to LOAD DATA (Load_log_event,
    Create_file/Append/Exec/Delete, we pass description_event; however as
    logging of LOAD DATA is going to be changed in 4.1 or 5.0, this is only used
    for the common_header_len (post_header_len will not be changed).
  */
  Load_log_event(const char* buf, uint event_len,
                 const Format_description_log_event* description_event);
  ~Load_log_event()
  {}
  Log_event_type get_type_code()
  {
    return sql_ex.new_format() ? NEW_LOAD_EVENT: LOAD_EVENT;
  }
#ifndef MYSQL_CLIENT
  bool write_data_header(IO_CACHE* file);
  bool write_data_body(IO_CACHE* file);
#endif
  bool is_valid() const { return table_name != 0; }
  int get_data_size()
  {
    return (table_name_len + db_len + 2 + fname_len
	    + LOAD_HEADER_LEN
	    + sql_ex.data_size() + field_block_len + num_fields);
  }
};

extern char server_version[SERVER_VERSION_LENGTH];

/*****************************************************************************

  Start Log Event_v3 class

  Start_log_event_v3 is the Start_log_event of binlog format 3 (MySQL 3.23 and
  4.x).
  Format_description_log_event derives from Start_log_event_v3; it is the
  Start_log_event of binlog format 4 (MySQL 5.0), that is, the event that
  describes the other events' header/postheader lengths. This event is sent by
  MySQL 5.0 whenever it starts sending a new binlog if the requested position
  is >4 (otherwise if ==4 the event will be sent naturally).

 ****************************************************************************/

class Start_log_event_v3: public Log_event
{
public:
  /*
    If this event is at the start of the first binary log since server
    startup 'created' should be the timestamp when the event (and the
    binary log) was created.  In the other case (i.e. this event is at
    the start of a binary log created by FLUSH LOGS or automatic
    rotation), 'created' should be 0.  This "trick" is used by MySQL
    >=4.0.14 slaves to know whether they must drop stale temporary
    tables and whether they should abort unfinished transaction.

    Note that when 'created'!=0, it is always equal to the event's
    timestamp; indeed Start_log_event is written only in log.cc where
    the first constructor below is called, in which 'created' is set
    to 'when'.  So in fact 'created' is a useless variable. When it is
    0 we can read the actual value from timestamp ('when') and when it
    is non-zero we can read the same value from timestamp
    ('when'). Conclusion:
     - we use timestamp to print when the binlog was created.
     - we use 'created' only to know if this is a first binlog or not.
     In 3.23.57 we did not pay attention to this identity, so mysqlbinlog in
     3.23.57 does not print 'created the_date' if created was zero. This is now
     fixed.
  */
  time_t created;
  uint16 binlog_version;
  char server_version[ST_SERVER_VER_LEN];
  /*
    artifical_event is 1 in the case where this is a generated event that
    should not case any cleanup actions. We handle this in the log by
    setting log_event == 0 (for now).
  */
  bool artificial_event;

#ifndef MYSQL_CLIENT
  Start_log_event_v3();
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  Start_log_event_v3() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Start_log_event_v3(const char* buf,
                     const Format_description_log_event* description_event);
  ~Start_log_event_v3() {}
  Log_event_type get_type_code() { return START_EVENT_V3;}
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
  int get_data_size()
  {
    return START_V3_HEADER_LEN; //no variable-sized part
  }
  virtual bool is_artificial_event() { return artificial_event; }
};


/*
   For binlog version 4.
   This event is saved by threads which read it, as they need it for future
   use (to decode the ordinary events).
*/

class Format_description_log_event: public Start_log_event_v3
{
public:
  /*
     The size of the fixed header which _all_ events have
     (for binlogs written by this version, this is equal to
     LOG_EVENT_HEADER_LEN), except FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT
     (those have a header of size LOG_EVENT_MINIMAL_HEADER_LEN).
  */
  uint8 common_header_len;
  uint8 number_of_event_types;
  /* The list of post-headers' lengthes */
  uint8 *post_header_len;

  Format_description_log_event(uint8 binlog_ver, const char* server_ver=0);

#ifndef MYSQL_CLIENT
#ifdef HAVE_REPLICATION
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#endif

  Format_description_log_event(const char* buf, uint event_len,
                               const Format_description_log_event* description_event);
  ~Format_description_log_event() { my_free((gptr)post_header_len, MYF(0)); }
  Log_event_type get_type_code() { return FORMAT_DESCRIPTION_EVENT;}
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const
  {
    return ((common_header_len >= ((binlog_version==1) ? OLD_HEADER_LEN :
                                   LOG_EVENT_MINIMAL_HEADER_LEN)) &&
            (post_header_len != NULL));
  }
  int get_data_size()
  {
    /*
      The vector of post-header lengths is considered as part of the
      post-header, because in a given version it never changes (contrary to the
      query in a Query_log_event).
    */
    return FORMAT_DESCRIPTION_HEADER_LEN;
  }
};


/*****************************************************************************

  Intvar Log Event class

  Logs special variables such as auto_increment values

 ****************************************************************************/

class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;

#ifndef MYSQL_CLIENT
  Intvar_log_event(THD* thd_arg,uchar type_arg, ulonglong val_arg)
    :Log_event(thd_arg,0,0),val(val_arg),type(type_arg)
  {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Intvar_log_event(const char* buf, const Format_description_log_event* description_event);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  const char* get_var_type_name();
  int get_data_size() { return  9; /* sizeof(type) + sizeof(val) */;}
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
};


/*****************************************************************************

  Rand Log Event class

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.0.
  4.1.1 does not need it (it's repeatable again) so this event needn't be
  written in 4.1.1 for PASSWORD() (but the fact that it is written is just a
  waste, it does not cause bugs).

 ****************************************************************************/

class Rand_log_event: public Log_event
{
 public:
  ulonglong seed1;
  ulonglong seed2;

#ifndef MYSQL_CLIENT
  Rand_log_event(THD* thd_arg, ulonglong seed1_arg, ulonglong seed2_arg)
    :Log_event(thd_arg,0,0),seed1(seed1_arg),seed2(seed2_arg)
  {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Rand_log_event(const char* buf, const Format_description_log_event* description_event);
  ~Rand_log_event() {}
  Log_event_type get_type_code() { return RAND_EVENT;}
  int get_data_size() { return 16; /* sizeof(ulonglong) * 2*/ }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
};

/*****************************************************************************

  Xid Log Event class

  Logs xid of the transaction-to-be-committed in the 2pc protocol.
  Has no meaning in replication, slaves ignore it.

 ****************************************************************************/
#ifdef MYSQL_CLIENT
typedef ulonglong my_xid; // this line is the same as in handler.h
#endif

class Xid_log_event: public Log_event
{
 public:
   my_xid xid;

#ifndef MYSQL_CLIENT
  Xid_log_event(THD* thd_arg, my_xid x): Log_event(thd_arg,0,0), xid(x) {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Xid_log_event(const char* buf, const Format_description_log_event* description_event);
  ~Xid_log_event() {}
  Log_event_type get_type_code() { return XID_EVENT;}
  int get_data_size() { return sizeof(xid); }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
};

/*****************************************************************************

  User var Log Event class

  Every time a query uses the value of a user variable, a User_var_log_event is
  written before the Query_log_event, to set the user variable.

 ****************************************************************************/

class User_var_log_event: public Log_event
{
public:
  char *name;
  uint name_len;
  char *val;
  ulong val_len;
  Item_result type;
  uint charset_number;
  bool is_null;
#ifndef MYSQL_CLIENT
  User_var_log_event(THD* thd_arg, char *name_arg, uint name_len_arg,
                     char *val_arg, ulong val_len_arg, Item_result type_arg,
		     uint charset_number_arg)
    :Log_event(), name(name_arg), name_len(name_len_arg), val(val_arg),
    val_len(val_len_arg), type(type_arg), charset_number(charset_number_arg)
    { is_null= !val; }
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  User_var_log_event(const char* buf, const Format_description_log_event* description_event);
  ~User_var_log_event() {}
  Log_event_type get_type_code() { return USER_VAR_EVENT;}
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }
};


/*****************************************************************************

  Stop Log Event class

 ****************************************************************************/
class Stop_log_event: public Log_event
{
public:
#ifndef MYSQL_CLIENT
  Stop_log_event() :Log_event()
  {}
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Stop_log_event(const char* buf, const Format_description_log_event* description_event):
    Log_event(buf, description_event)
  {}
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  bool is_valid() const { return 1; }
};

/*****************************************************************************

  Rotate Log Event class

  This will be deprecated when we move to using sequence ids.

 ****************************************************************************/

class Rotate_log_event: public Log_event
{
public:
  enum {
    DUP_NAME= 2 // if constructor should dup the string argument
  };
  const char* new_log_ident;
  ulonglong pos;
  uint ident_len;
  uint flags;
#ifndef MYSQL_CLIENT
  Rotate_log_event(THD* thd_arg, const char* new_log_ident_arg,
		   uint ident_len_arg,
		   ulonglong pos_arg, uint flags);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Rotate_log_event(const char* buf, uint event_len,
                   const Format_description_log_event* description_event);
  ~Rotate_log_event()
  {
    if (flags & DUP_NAME)
      my_free((gptr) new_log_ident, MYF(MY_ALLOW_ZERO_PTR));
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len + ROTATE_HEADER_LEN;}
  bool is_valid() const { return new_log_ident != 0; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
};


/* the classes below are for the new LOAD DATA INFILE logging */

/*****************************************************************************
  Create File Log Event class
 ****************************************************************************/

class Create_file_log_event: public Load_log_event
{
protected:
  /*
    Pretend we are Load event, so we can write out just
    our Load part - used on the slave when writing event out to
    SQL_LOAD-*.info file
  */
  bool fake_base;
public:
  char* block;
  const char *event_buf;
  uint block_len;
  uint file_id;
  bool inited_from_old;

#ifndef MYSQL_CLIENT
  Create_file_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			const char* table_name_arg,
			List<Item>& fields_arg,
			enum enum_duplicates handle_dup, bool ignore,
			char* block_arg, uint block_len_arg,
			bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info, bool enable_local);
#endif

  Create_file_log_event(const char* buf, uint event_len,
                        const Format_description_log_event* description_event);
  ~Create_file_log_event()
  {
    my_free((char*) event_buf, MYF(MY_ALLOW_ZERO_PTR));
  }

  Log_event_type get_type_code()
  {
    return fake_base ? Load_log_event::get_type_code() : CREATE_FILE_EVENT;
  }
  int get_data_size()
  {
    return (fake_base ? Load_log_event::get_data_size() :
	    Load_log_event::get_data_size() +
	    4 + 1 + block_len);
  }
  bool is_valid() const { return inited_from_old || block != 0; }
#ifndef MYSQL_CLIENT
  bool write_data_header(IO_CACHE* file);
  bool write_data_body(IO_CACHE* file);
  /*
    Cut out Create_file extentions and
    write it as Load event - used on the slave
  */
  bool write_base(IO_CACHE* file);
#endif
};


/*****************************************************************************

  Append Block Log Event class

 ****************************************************************************/

class Append_block_log_event: public Log_event
{
public:
  char* block;
  uint block_len;
  uint file_id;
  /*
    'db' is filled when the event is created in mysql_load() (the
    event needs to have a 'db' member to be well filtered by
    binlog-*-db rules). 'db' is not written to the binlog (it's not
    used by Append_block_log_event::write()), so it can't be read in
    the Append_block_log_event(const char* buf, int event_len)
    constructor.  In other words, 'db' is used only for filtering by
    binlog-*-db rules.  Create_file_log_event is different: it's 'db'
    (which is inherited from Load_log_event) is written to the binlog
    and can be re-read.
  */
  const char* db;

#ifndef MYSQL_CLIENT
  Append_block_log_event(THD* thd, const char* db_arg, char* block_arg,
			 uint block_len_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  int exec_event(struct st_relay_log_info* rli);
  void pack_info(Protocol* protocol);
  virtual int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Append_block_log_event(const char* buf, uint event_len,
                         const Format_description_log_event* description_event);
  ~Append_block_log_event() {}
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  int get_data_size() { return  block_len + APPEND_BLOCK_HEADER_LEN ;}
  bool is_valid() const { return block != 0; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  const char* get_db() { return db; }
};


/*****************************************************************************

  Delete File Log Event class

 ****************************************************************************/

class Delete_file_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */

#ifndef MYSQL_CLIENT
  Delete_file_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info, bool enable_local);
#endif

  Delete_file_log_event(const char* buf, uint event_len,
                        const Format_description_log_event* description_event);
  ~Delete_file_log_event() {}
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  int get_data_size() { return DELETE_FILE_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  const char* get_db() { return db; }
};


/*****************************************************************************

  Execute Load Log Event class

 ****************************************************************************/

class Execute_load_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */

#ifndef MYSQL_CLIENT
  Execute_load_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
#endif

  Execute_load_log_event(const char* buf, uint event_len,
                         const Format_description_log_event* description_event);
  ~Execute_load_log_event() {}
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  int get_data_size() { return  EXEC_LOAD_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifndef MYSQL_CLIENT
  bool write(IO_CACHE* file);
#endif
  const char* get_db() { return db; }
};


/***************************************************************************

  Begin load query Log Event class

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

****************************************************************************/
class Begin_load_query_log_event: public Append_block_log_event
{
public:
#ifndef MYSQL_CLIENT
  Begin_load_query_log_event(THD* thd_arg, const char *db_arg,
                             char* block_arg, uint block_len_arg,
                             bool using_trans);
#ifdef HAVE_REPLICATION
  Begin_load_query_log_event(THD* thd);
  int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#endif
  Begin_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_log_event* description_event);
  ~Begin_load_query_log_event() {}
  Log_event_type get_type_code() { return BEGIN_LOAD_QUERY_EVENT; }
};


/*
  Elements of this enum describe how LOAD DATA handles duplicates.
*/
enum enum_load_dup_handling { LOAD_DUP_ERROR= 0, LOAD_DUP_IGNORE,
                              LOAD_DUP_REPLACE };

/****************************************************************************

  Execute load query Log Event class

  Event responsible for LOAD DATA execution, it similar to Query_log_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

****************************************************************************/
class Execute_load_query_log_event: public Query_log_event
{
public:
  uint file_id;       // file_id of temporary file
  uint fn_pos_start;  // pointer to the part of the query that should
                      // be substituted
  uint fn_pos_end;    // pointer to the end of this part of query
  /*
    We have to store type of duplicate handling explicitly, because
    for LOAD DATA it also depends on LOCAL option. And this part
    of query will be rewritten during replication so this information
    may be lost...
  */
  enum_load_dup_handling dup_handling;

#ifndef MYSQL_CLIENT
  Execute_load_query_log_event(THD* thd, const char* query_arg,
                       ulong query_length, uint fn_pos_start_arg,
                       uint fn_pos_end_arg,
                       enum_load_dup_handling dup_handling_arg,
                       bool using_trans, bool suppress_use);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
  /* Prints the query as LOAD DATA LOCAL and with rewritten filename */
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
	     const char *local_fname);
#endif
  Execute_load_query_log_event(const char* buf, uint event_len,
                               const Format_description_log_event *description_event);
  ~Execute_load_query_log_event() {}

  Log_event_type get_type_code() { return EXECUTE_LOAD_QUERY_EVENT; }
  bool is_valid() const { return Query_log_event::is_valid() && file_id != 0; }

  ulong get_post_header_size_for_derived();
#ifndef MYSQL_CLIENT
  bool write_post_header_for_derived(IO_CACHE* file);
#endif
 };


#ifdef MYSQL_CLIENT
class Unknown_log_event: public Log_event
{
public:
  /*
    Even if this is an unknown event, we still pass description_event to
    Log_event's ctor, this way we can extract maximum information from the
    event's header (the unique ID for example).
  */
  Unknown_log_event(const char* buf, const Format_description_log_event* description_event):
    Log_event(buf, description_event)
  {}
  ~Unknown_log_event() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info= 0);
  Log_event_type get_type_code() { return UNKNOWN_EVENT;}
  bool is_valid() const { return 1; }
};
#endif
char *str_to_hex(char *to, const char *from, uint len);
#endif /* _log_event_h */
