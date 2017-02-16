/* Copyright (c) 2000, 2014, Oracle and/or its affiliates.

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
  @addtogroup Replication
  @{

  @file
  
  @brief Binary log event definitions.  This includes generic code
  common to all types of log events, as well as specific code for each
  type of log event.
*/


#ifndef _log_event_h
#define _log_event_h

#if defined(USE_PRAGMA_INTERFACE) && defined(MYSQL_SERVER)
#pragma interface			/* gcc class implementation */
#endif

#include <my_bitmap.h>
#include "rpl_constants.h"

#ifdef MYSQL_CLIENT
#include "sql_const.h"
#include "rpl_utility.h"
#include "hash.h"
#include "rpl_tblmap.h"
#endif

#ifdef MYSQL_SERVER
#include "rpl_record.h"
#include "rpl_reporting.h"
#include "sql_class.h"                          /* THD */
#endif

/* Forward declarations */
class String;

#define PREFIX_SQL_LOAD "SQL_LOAD-"
#define LONG_FIND_ROW_THRESHOLD 60 /* seconds */

/**
   Either assert or return an error.

   In debug build, the condition will be checked, but in non-debug
   builds, the error code given will be returned instead.

   @param COND   Condition to check
   @param ERRNO  Error number to return in non-debug builds
*/
#ifdef DBUG_OFF
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) \
  do { if (!(COND)) return ERRNO; } while (0)
#else
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) \
  DBUG_ASSERT(COND)
#endif

#define LOG_READ_EOF    -1
#define LOG_READ_BOGUS  -2
#define LOG_READ_IO     -3
#define LOG_READ_MEM    -5
#define LOG_READ_TRUNC  -6
#define LOG_READ_TOO_LARGE -7
#define LOG_READ_CHECKSUM_FAILURE -8

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

   - Query and Load (Create or Execute) events may have a more precise
     timestamp (with microseconds), number of matched/affected/warnings rows
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
  sql_ex_info() {}                            /* Remove gcc warning */
  const char* field_term;
  const char* enclosed;
  const char* line_term;
  const char* line_start;
  const char* escaped;
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
  const char* init(const char* buf, const char* buf_end, bool use_new_format);
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
#define STOP_HEADER_LEN      0
#define LOAD_HEADER_LEN      (4 + 4 + 4 + 1 +1 + 4)
#define SLAVE_HEADER_LEN     0
#define START_V3_HEADER_LEN     (2 + ST_SERVER_VER_LEN + 4)
#define ROTATE_HEADER_LEN    8 // this is FROZEN (the Rotate post-header is frozen)
#define INTVAR_HEADER_LEN      0
#define CREATE_FILE_HEADER_LEN 4
#define APPEND_BLOCK_HEADER_LEN 4
#define EXEC_LOAD_HEADER_LEN   4
#define DELETE_FILE_HEADER_LEN 4
#define NEW_LOAD_HEADER_LEN    LOAD_HEADER_LEN
#define RAND_HEADER_LEN        0
#define USER_VAR_HEADER_LEN    0
#define FORMAT_DESCRIPTION_HEADER_LEN (START_V3_HEADER_LEN+1+LOG_EVENT_TYPES)
#define XID_HEADER_LEN         0
#define BEGIN_LOAD_QUERY_HEADER_LEN APPEND_BLOCK_HEADER_LEN
#define ROWS_HEADER_LEN        8
#define TABLE_MAP_HEADER_LEN   8
#define EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN (4 + 4 + 4 + 1)
#define EXECUTE_LOAD_QUERY_HEADER_LEN  (QUERY_HEADER_LEN + EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN)
#define INCIDENT_HEADER_LEN    2
#define HEARTBEAT_HEADER_LEN   0
#define ANNOTATE_ROWS_HEADER_LEN  0

/* 
  Max number of possible extra bytes in a replication event compared to a
  packet (i.e. a query) sent from client to master;
  First, an auxiliary log_event status vars estimation:
*/
#define MAX_SIZE_LOG_EVENT_STATUS (1 + 4          /* type, flags2 */   + \
                                   1 + 8          /* type, sql_mode */ + \
                                   1 + 1 + 255    /* type, length, catalog */ + \
                                   1 + 4          /* type, auto_increment */ + \
                                   1 + 6          /* type, charset */ + \
                                   1 + 1 + 255    /* type, length, time_zone */ + \
                                   1 + 2          /* type, lc_time_names_number */ + \
                                   1 + 2          /* type, charset_database_number */ + \
                                   1 + 8          /* type, table_map_for_update */ + \
                                   1 + 4          /* type, master_data_written */ + \
                                   1 + 3          /* type, sec_part of NOW() */ + \
                                   1 + 16 + 1 + 60/* type, user_len, user, host_len, host */)
#define MAX_LOG_EVENT_HEADER   ( /* in order of Query_log_event::write */ \
  LOG_EVENT_HEADER_LEN + /* write_header */ \
  QUERY_HEADER_LEN     + /* write_data */   \
  EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN + /*write_post_header_for_derived */ \
  MAX_SIZE_LOG_EVENT_STATUS + /* status */ \
  NAME_LEN + 1)

/*
  The new option is added to handle large packets that are sent from the master 
  to the slave. It is used to increase the thd(max_allowed) for both the
  DUMP thread on the master and the SQL/IO thread on the slave. 
*/
#define MAX_MAX_ALLOWED_PACKET 1024*1024*1024

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

#define Q_LC_TIME_NAMES_CODE    7

#define Q_CHARSET_DATABASE_CODE 8

#define Q_TABLE_MAP_FOR_UPDATE_CODE 9

#define Q_MASTER_DATA_WRITTEN_CODE 10

#define Q_INVOKER 11

#define Q_HRNOW 128

/* Intvar event post-header */

/* Intvar event data */
#define I_TYPE_OFFSET        0
#define I_VAL_OFFSET         1

/* Rand event data */
#define RAND_SEED1_OFFSET 0
#define RAND_SEED2_OFFSET 8

/* User_var event data */
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

/* TM = "Table Map" */
#define TM_MAPID_OFFSET    0
#define TM_FLAGS_OFFSET    6

/* RW = "RoWs" */
#define RW_MAPID_OFFSET    0
#define RW_FLAGS_OFFSET    6

/* ELQ = "Execute Load Query" */
#define ELQ_FILE_ID_OFFSET QUERY_HEADER_LEN
#define ELQ_FN_POS_START_OFFSET ELQ_FILE_ID_OFFSET + 4
#define ELQ_FN_POS_END_OFFSET ELQ_FILE_ID_OFFSET + 8
#define ELQ_DUP_HANDLING_OFFSET ELQ_FILE_ID_OFFSET + 12

/* 4 bytes which all binlogs should begin with */
#define BINLOG_MAGIC        (const uchar*) "\xfe\x62\x69\x6e"

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

/**
  @def LOG_EVENT_THREAD_SPECIFIC_F

  If the query depends on the thread (for example: TEMPORARY TABLE).
  Currently this is used by mysqlbinlog to know it must print
  SET @@PSEUDO_THREAD_ID=xx; before the query (it would not hurt to print it
  for every query but this would be slow).
*/
#define LOG_EVENT_THREAD_SPECIFIC_F 0x4

/**
  @def LOG_EVENT_SUPPRESS_USE_F

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
  Note: this is a place holder for the flag
  LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F (0x10), which is not used any
  more, please do not reused this value for other flags.
 */

/**
   @def LOG_EVENT_ARTIFICIAL_F
   
   Artificial events are created arbitarily and not written to binary
   log

   These events should not update the master log position when slave
   SQL thread executes them.
*/
#define LOG_EVENT_ARTIFICIAL_F 0x20

/**
   @def LOG_EVENT_RELAY_LOG_F
   
   Events with this flag set are created by slave IO thread and written
   to relay log
*/
#define LOG_EVENT_RELAY_LOG_F 0x40

/**
   @def LOG_EVENT_SKIP_REPLICATION_F

   Flag set by application creating the event (with @@skip_replication); the
   slave will skip replication of such events if
   --replicate-events-marked-for-skip is not set to REPLICATE.

   This is a MariaDB flag; we allocate it from the end of the available
   values to reduce risk of conflict with new MySQL flags.
*/
#define LOG_EVENT_SKIP_REPLICATION_F 0x8000


/**
  @def OPTIONS_WRITTEN_TO_BIN_LOG

  OPTIONS_WRITTEN_TO_BIN_LOG are the bits of thd->options which must
  be written to the binlog. OPTIONS_WRITTEN_TO_BIN_LOG could be
  written into the Format_description_log_event, so that if later we
  don't want to replicate a variable we did replicate, or the
  contrary, it's doable. But it should not be too hard to decide once
  for all of what we replicate and what we don't, among the fixed 32
  bits of thd->options.

  I (Guilhem) have read through every option's usage, and it looks
  like OPTION_AUTO_IS_NULL and OPTION_NO_FOREIGN_KEYS are the only
  ones which alter how the query modifies the table. It's good to
  replicate OPTION_RELAXED_UNIQUE_CHECKS too because otherwise, the
  slave may insert data slower than the master, in InnoDB.
  OPTION_BIG_SELECTS is not needed (the slave thread runs with
  max_join_size=HA_POS_ERROR) and OPTION_BIG_TABLES is not needed
  either, as the manual says (because a too big in-memory temp table
  is automatically written to disk).
*/
#define OPTIONS_WRITTEN_TO_BIN_LOG \
  (OPTION_AUTO_IS_NULL | OPTION_NO_FOREIGN_KEY_CHECKS |  \
   OPTION_RELAXED_UNIQUE_CHECKS | OPTION_NOT_AUTOCOMMIT)

/* Shouldn't be defined before */
#define EXPECTED_OPTIONS \
  ((ULL(1) << 14) | (ULL(1) << 26) | (ULL(1) << 27) | (ULL(1) << 19))

#if OPTIONS_WRITTEN_TO_BIN_LOG != EXPECTED_OPTIONS
#error OPTIONS_WRITTEN_TO_BIN_LOG must NOT change their values!
#endif
#undef EXPECTED_OPTIONS         /* You shouldn't use this one */

enum enum_binlog_checksum_alg {
  BINLOG_CHECKSUM_ALG_OFF= 0,    // Events are without checksum though its generator
                                 // is checksum-capable New Master (NM).
  BINLOG_CHECKSUM_ALG_CRC32= 1,  // CRC32 of zlib algorithm.
  BINLOG_CHECKSUM_ALG_ENUM_END,  // the cut line: valid alg range is [1, 0x7f].
  BINLOG_CHECKSUM_ALG_UNDEF= 255 // special value to tag undetermined yet checksum
                                 // or events from checksum-unaware servers
};

#define CHECKSUM_CRC32_SIGNATURE_LEN 4
/**
   defined statically while there is just one alg implemented
*/
#define BINLOG_CHECKSUM_LEN CHECKSUM_CRC32_SIGNATURE_LEN
#define BINLOG_CHECKSUM_ALG_DESC_LEN 1  /* 1 byte checksum alg descriptor */

/**
  @enum Log_event_type

  Enumeration type for the different types of log events.
*/
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

  TABLE_MAP_EVENT = 19,

  /*
    These event numbers were used for 5.1.0 to 5.1.15 and are
    therefore obsolete.
   */
  PRE_GA_WRITE_ROWS_EVENT = 20,
  PRE_GA_UPDATE_ROWS_EVENT = 21,
  PRE_GA_DELETE_ROWS_EVENT = 22,

  /*
    These event numbers are used from 5.1.16 and forward
   */
  WRITE_ROWS_EVENT = 23,
  UPDATE_ROWS_EVENT = 24,
  DELETE_ROWS_EVENT = 25,

  /*
    Something out of the ordinary happened on the master
   */
  INCIDENT_EVENT= 26,

  /*
    Heartbeat event to be send by master at its idle time 
    to ensure master's online status to slave 
  */
  HEARTBEAT_LOG_EVENT= 27,
  
  /*
    Add new events here - right above this comment!
    Existing events (except ENUM_END_EVENT) should never change their numbers
  */

  /* New MySQL/Sun events are to be added right above this comment */
  MYSQL_EVENTS_END,

  MARIA_EVENTS_BEGIN= 160,
  /* New Maria event numbers start from here */
  ANNOTATE_ROWS_EVENT= 160,

  /* Add new MariaDB events here - right above this comment!  */

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


#ifdef MYSQL_SERVER
class String;
class MYSQL_BIN_LOG;
class THD;
#endif

class Format_description_log_event;
class Relay_log_info;

#ifdef MYSQL_CLIENT
enum enum_base64_output_mode {
  BASE64_OUTPUT_NEVER= 0,
  BASE64_OUTPUT_AUTO= 1,
  BASE64_OUTPUT_ALWAYS= 2,
  BASE64_OUTPUT_UNSPEC= 3,
  BASE64_OUTPUT_DECODE_ROWS= 4,
  /* insert new output modes here */
  BASE64_OUTPUT_MODE_COUNT
};

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
    that was printed.  We cache these so that we don't have to print
    them if they are unchanged.
  */
  // TODO: have the last catalog here ??
  char db[FN_REFLEN+1]; // TODO: make this a LEX_STRING when thd->db is
  bool flags2_inited;
  uint32 flags2;
  bool sql_mode_inited;
  ulonglong sql_mode;		/* must be same as THD.variables.sql_mode */
  ulong auto_increment_increment, auto_increment_offset;
  bool charset_inited;
  char charset[6]; // 3 variables, each of them storable in 2 bytes
  char time_zone_str[MAX_TIME_ZONE_NAME_LENGTH];
  uint lc_time_names_number;
  uint charset_database_number;
  uint thread_id;
  bool thread_id_printed;
  /*
    Track when @@skip_replication changes so we need to output a SET
    statement for it.
  */
  int skip_replication;

  st_print_event_info();

  ~st_print_event_info() {
    close_cached_file(&head_cache);
    close_cached_file(&body_cache);
  }
  bool init_ok() /* tells if construction was successful */
    { return my_b_inited(&head_cache) && my_b_inited(&body_cache); }


  /* Settings on how to print the events */
  bool short_form;
  enum_base64_output_mode base64_output_mode;
  /*
    This is set whenever a Format_description_event is printed.
    Later, when an event is printed in base64, this flag is tested: if
    no Format_description_event has been seen, it is unsafe to print
    the base64 event, so an error message is generated.
  */
  bool printed_fd_event;
  my_off_t hexdump_from;
  uint8 common_header_len;
  char delimiter[16];

  uint verbose;
  table_mapping m_table_map;
  table_mapping m_table_map_ignored;

  /*
     These two caches are used by the row-based replication events to
     collect the header information and the main body of the events
     making up a statement.
   */
  IO_CACHE head_cache;
  IO_CACHE body_cache;
} PRINT_EVENT_INFO;
#endif

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
  @class Log_event

  This is the abstract base class for binary log events.
  
  @section Log_event_binary_format Binary Format

  Any @c Log_event saved on disk consists of the following three
  components.

  - Common-Header
  - Post-Header
  - Body

  The Common-Header, documented in the table @ref Table_common_header
  "below", always has the same form and length within one version of
  MySQL.  Each event type specifies a format and length of the
  Post-Header.  The length of the Common-Header is the same for all
  events of the same type.  The Body may be of different format and
  length even for different events of the same type.  The binary
  formats of Post-Header and Body are documented separately in each
  subclass.  The binary format of Common-Header is as follows.

  <table>
  <caption>Common-Header</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>timestamp</td>
    <td>4 byte unsigned integer</td>
    <td>The time when the query started, in seconds since 1970.
    </td>
  </tr>

  <tr>
    <td>type</td>
    <td>1 byte enumeration</td>
    <td>See enum #Log_event_type.</td>
  </tr>

  <tr>
    <td>server_id</td>
    <td>4 byte unsigned integer</td>
    <td>Server ID of the server that created the event.</td>
  </tr>

  <tr>
    <td>total_size</td>
    <td>4 byte unsigned integer</td>
    <td>The total size of this event, in bytes.  In other words, this
    is the sum of the sizes of Common-Header, Post-Header, and Body.
    </td>
  </tr>

  <tr>
    <td>master_position</td>
    <td>4 byte unsigned integer</td>
    <td>The position of the next event in the master binary log, in
    bytes from the beginning of the file.  In a binlog that is not a
    relay log, this is just the position of the next event, in bytes
    from the beginning of the file.  In a relay log, this is
    the position of the next event in the master's binlog.
    </td>
  </tr>

  <tr>
    <td>flags</td>
    <td>2 byte bitfield</td>
    <td>See Log_event::flags.</td>
  </tr>
  </table>

  Summing up the numbers above, we see that the total size of the
  common header is 19 bytes.

  @subsection Log_event_format_of_atomic_primitives Format of Atomic Primitives

  - All numbers, whether they are 16-, 24-, 32-, or 64-bit numbers,
  are stored in little endian, i.e., the least significant byte first,
  unless otherwise specified.

  @anchor packed_integer
  - Some events use a special format for efficient representation of
  unsigned integers, called Packed Integer.  A Packed Integer has the
  capacity of storing up to 8-byte integers, while small integers
  still can use 1, 3, or 4 bytes.  The value of the first byte
  determines how to read the number, according to the following table:

  <table>
  <caption>Format of Packed Integer</caption>

  <tr>
    <th>First byte</th>
    <th>Format</th>
  </tr>

  <tr>
    <td>0-250</td>
    <td>The first byte is the number (in the range 0-250), and no more
    bytes are used.</td>
  </tr>

  <tr>
    <td>252</td>
    <td>Two more bytes are used.  The number is in the range
    251-0xffff.</td>
  </tr>

  <tr>
    <td>253</td>
    <td>Three more bytes are used.  The number is in the range
    0xffff-0xffffff.</td>
  </tr>

  <tr>
    <td>254</td>
    <td>Eight more bytes are used.  The number is in the range
    0xffffff-0xffffffffffffffff.</td>
  </tr>

  </table>

  - Strings are stored in various formats.  The format of each string
  is documented separately.
*/
class Log_event
{
public:
  /**
     Enumeration of what kinds of skipping (and non-skipping) that can
     occur when the slave executes an event.

     @see shall_skip
     @see do_shall_skip
   */
  enum enum_skip_reason {
    /**
       Don't skip event.
    */
    EVENT_SKIP_NOT,

    /**
       Skip event by ignoring it.

       This means that the slave skip counter will not be changed.
    */
    EVENT_SKIP_IGNORE,

    /**
       Skip event and decrease skip counter.
    */
    EVENT_SKIP_COUNT
  };

  enum enum_event_cache_type 
  {
    EVENT_INVALID_CACHE,
    /* 
      If possible the event should use a non-transactional cache before
      being flushed to the binary log. This means that it must be flushed
      right after its correspondent statement is completed.
    */
    EVENT_STMT_CACHE,
    /* 
      The event should use a transactional cache before being flushed to
      the binary log. This means that it must be flushed upon commit or 
      rollback. 
    */
    EVENT_TRANSACTIONAL_CACHE,
    /* 
      The event must be written directly to the binary log without going
      through a cache.
    */
    EVENT_NO_CACHE,
    /**
       If there is a need for different types, introduce them before this.
    */
    EVENT_CACHE_COUNT
  };

  /*
    The following type definition is to be used whenever data is placed 
    and manipulated in a common buffer. Use this typedef for buffers
    that contain data containing binary and character data.
  */
  typedef unsigned char Byte;

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
    TRUE <=> this event 'owns' temp_buf and should call my_free() when done
    with it
  */
  bool event_owns_temp_buf;

  /*
    Timestamp on the master(for debugging and replication of
    NOW()/TIMESTAMP).  It is important for queries and LOAD DATA
    INFILE. This is set at the event's creation time, except for Query
    and Load (et al.) events where this is set at the query's
    execution time, which guarantees good replication (otherwise, we
    could have a query and its event with different timestamps).
  */
  my_time_t when;
  ulong     when_sec_part;
  /* The number of seconds the query took to run on the master. */
  ulong exec_time;
  /* Number of bytes written by write() function */
  ulong data_written;

  /*
    The master's server id (is preserved in the relay log; used to
    prevent from infinite loops in circular replication).
  */
  uint32 server_id;

  /**
    Some 16 flags. See the definitions above for LOG_EVENT_TIME_F,
    LOG_EVENT_FORCED_ROTATE_F, LOG_EVENT_THREAD_SPECIFIC_F,
    LOG_EVENT_SUPPRESS_USE_F, and LOG_EVENT_SKIP_REPLICATION_F for notes.
  */
  uint16 flags;

  uint16 cache_type;

  /**
    A storage to cache the global system variable's value.
    Handling of a separate event will be governed its member.
  */
  ulong slave_exec_mode;

  /**
    Placeholder for event checksum while writing to binlog.
   */
  ha_checksum crc;

#ifdef MYSQL_SERVER
  THD* thd;

  Log_event();
  Log_event(THD* thd_arg, uint16 flags_arg, bool is_transactional);
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
                                   mysql_mutex_t* log_lock,
                                   const Format_description_log_event
                                   *description_event,
                                   my_bool crc_check);

  /**
    Reads an event from a binlog or relay log. Used by the dump thread
    this method reads the event into a raw buffer without parsing it.

    @Note If mutex is 0, the read will proceed without mutex.

    @Note If a log name is given than the method will check if the
    given binlog is still active.

    @param[in]  file                log file to be read
    @param[out] packet              packet to hold the event
    @param[in]  lock                the lock to be used upon read
    @param[in]  log_file_name_arg   the log's file name
    @param[out] is_binlog_active    is the current log still active

    @retval 0                   success
    @retval LOG_READ_EOF        end of file, nothing was read
    @retval LOG_READ_BOGUS      malformed event
    @retval LOG_READ_IO         io error while reading
    @retval LOG_READ_MEM        packet memory allocation failed
    @retval LOG_READ_TRUNC      only a partial event could be read
    @retval LOG_READ_TOO_LARGE  event too large
   */
  static int read_log_event(IO_CACHE* file, String* packet,
                            mysql_mutex_t* log_lock,
                            uint8 checksum_alg_arg,
                            const char *log_file_name_arg = NULL,
                            bool* is_binlog_active = NULL);
  /*
    init_show_field_list() prepares the column names and types for the
    output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
    EVENTS.
  */
  static void init_show_field_list(List<Item>* field_list);
#ifdef HAVE_REPLICATION
  int net_send(THD *thd, Protocol *protocol, const char* log_name,
               my_off_t pos);

  /*
    pack_info() is used by SHOW BINLOG EVENTS; as print() it prepares and sends
    a string to display to the user, so it resembles print().
  */

  virtual void pack_info(THD *thd, Protocol *protocol);

#endif /* HAVE_REPLICATION */
  virtual const char* get_db()
  {
    return thd ? thd->db : 0;
  }
#else
  Log_event() : temp_buf(0), flags(0) {}
    /* avoid having to link mysqlbinlog against libpthread */
  static Log_event* read_log_event(IO_CACHE* file,
                                   const Format_description_log_event
                                   *description_event, my_bool crc_check);
  /* print*() functions are used by mysqlbinlog */
  virtual void print(FILE* file, PRINT_EVENT_INFO* print_event_info) = 0;
  void print_timestamp(IO_CACHE* file, time_t *ts = 0);
  void print_header(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info,
                    bool is_more);
  void print_base64(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info,
                    bool is_more);
#endif
  /* 
     The value is set by caller of FD constructor and
     Log_event::write_header() for the rest.
     In the FD case it's propagated into the last byte 
     of post_header_len[] at FD::write().
     On the slave side the value is assigned from post_header_len[last] 
     of the last seen FD event.
  */
  uint8 checksum_alg;

  static void *operator new(size_t size)
  {
    return (void*) my_malloc((uint)size, MYF(MY_WME|MY_FAE));
  }

  static void operator delete(void *ptr, size_t)
  {
    my_free(ptr);
  }

  /* Placement version of the above operators */
  static void *operator new(size_t, void* ptr) { return ptr; }
  static void operator delete(void*, void*) { }
  bool wrapper_my_b_safe_write(IO_CACHE* file, const uchar* buf, ulong data_length);

#ifdef MYSQL_SERVER
  bool write_header(IO_CACHE* file, ulong data_length);
  bool write_footer(IO_CACHE* file);
  my_bool need_checksum();

  virtual bool write(IO_CACHE* file)
  {
    return(write_header(file, get_data_size()) ||
	   write_data_header(file) ||
	   write_data_body(file) ||
	   write_footer(file));
  }
  virtual bool write_data_header(IO_CACHE* file)
  { return 0; }
  virtual bool write_data_body(IO_CACHE* file __attribute__((unused)))
  { return 0; }
  inline my_time_t get_time()
  {
    THD *tmp_thd;
    if (when)
      return when;
    if (thd)
    {
      when= thd->start_time;
      when_sec_part= thd->start_time_sec_part;
      return when;
    }
    /* thd will only be 0 here at time of log creation */
    if ((tmp_thd= current_thd))
    {
      when= tmp_thd->start_time;
      when_sec_part= tmp_thd->start_time_sec_part;
      return when;
    }
    my_hrtime_t hrtime= my_hrtime();
    when= hrtime_to_my_time(hrtime);
    when_sec_part= hrtime_sec_part(hrtime);
    return when;
  }
#endif
  virtual Log_event_type get_type_code() = 0;
  virtual bool is_valid() const = 0;
  void set_artificial_event() { flags |= LOG_EVENT_ARTIFICIAL_F; }
  void set_relay_log_event() { flags |= LOG_EVENT_RELAY_LOG_F; }
  bool is_artificial_event() const { return flags & LOG_EVENT_ARTIFICIAL_F; }
  bool is_relay_log_event() const { return flags & LOG_EVENT_RELAY_LOG_F; }
  inline bool use_trans_cache() const
  { 
    return (cache_type == Log_event::EVENT_TRANSACTIONAL_CACHE);
  }
  inline void set_direct_logging()
  {
    cache_type = Log_event::EVENT_NO_CACHE;
  }
  inline bool use_direct_logging()
  {
    return (cache_type == Log_event::EVENT_NO_CACHE);
  }
  Log_event(const char* buf, const Format_description_log_event
            *description_event);
  virtual ~Log_event() { free_temp_buf();}
  void register_temp_buf(char* buf, bool must_free) 
  { 
    temp_buf= buf; 
    event_owns_temp_buf= must_free;
  }
  void free_temp_buf()
  {
    if (temp_buf)
    {
      if (event_owns_temp_buf)
        my_free(temp_buf);
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
                                   *description_event, my_bool crc_check);
  /**
    Returns the human readable name of the given event type.
  */
  static const char* get_type_str(Log_event_type type);
  /**
    Returns the human readable name of this event's type.
  */
  const char* get_type_str();

  /* Return start of query time or current time */

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
public:

  /**
     Apply the event to the database.

     This function represents the public interface for applying an
     event.

     @see do_apply_event
   */
  int apply_event(Relay_log_info const *rli)
  {
    return do_apply_event(rli);
  }


  /**
     Update the relay log position.

     This function represents the public interface for "stepping over"
     the event and will update the relay log information.

     @see do_update_pos
   */
  int update_pos(Relay_log_info *rli)
  {
    return do_update_pos(rli);
  }

  /**
     Decide if the event shall be skipped, and the reason for skipping
     it.

     @see do_shall_skip
   */
  enum_skip_reason shall_skip(Relay_log_info *rli)
  {
    return do_shall_skip(rli);
  }

protected:

  /**
     Helper function to ignore an event w.r.t. the slave skip counter.

     This function can be used inside do_shall_skip() for functions
     that cannot end a group. If the slave skip counter is 1 when
     seeing such an event, the event shall be ignored, the counter
     left intact, and processing continue with the next event.

     A typical usage is:
     @code
     enum_skip_reason do_shall_skip(Relay_log_info *rli) {
       return continue_group(rli);
     }
     @endcode

     @return Skip reason
   */
  enum_skip_reason continue_group(Relay_log_info *rli);

  /**
    Primitive to apply an event to the database.

    This is where the change to the database is made.

    @note The primitive is protected instead of private, since there
    is a hierarchy of actions to be performed in some cases.

    @see Format_description_log_event::do_apply_event()

    @param rli Pointer to relay log info structure

    @retval 0     Event applied successfully
    @retval errno Error code if event application failed
  */
  virtual int do_apply_event(Relay_log_info const *rli)
  {
    return 0;                /* Default implementation does nothing */
  }


  /**
     Advance relay log coordinates.

     This function is called to advance the relay log coordinates to
     just after the event.  It is essential that both the relay log
     coordinate and the group log position is updated correctly, since
     this function is used also for skipping events.

     Normally, each implementation of do_update_pos() shall:

     - Update the event position to refer to the position just after
       the event.

     - Update the group log position to refer to the position just
       after the event <em>if the event is last in a group</em>

     @param rli Pointer to relay log info structure

     @retval 0     Coordinates changed successfully
     @retval errno Error code if advancing failed (usually just
                   1). Observe that handler errors are returned by the
                   do_apply_event() function, and not by this one.
   */
  virtual int do_update_pos(Relay_log_info *rli);


  /**
     Decide if this event shall be skipped or not and the reason for
     skipping it.

     The default implementation decide that the event shall be skipped
     if either:

     - the server id of the event is the same as the server id of the
       server and <code>rli->replicate_same_server_id</code> is true,
       or

     - if <code>rli->slave_skip_counter</code> is greater than zero.

     @see do_apply_event
     @see do_update_pos

     @retval Log_event::EVENT_SKIP_NOT
     The event shall not be skipped and should be applied.

     @retval Log_event::EVENT_SKIP_IGNORE
     The event shall be skipped by just ignoring it, i.e., the slave
     skip counter shall not be changed. This happends if, for example,
     the originating server id of the event is the same as the server
     id of the slave.

     @retval Log_event::EVENT_SKIP_COUNT
     The event shall be skipped because the slave skip counter was
     non-zero. The caller shall decrease the counter by one.
   */
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
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

/**
  @class Query_log_event
   
  A @c Query_log_event is created for each query that modifies the
  database, unless the query is logged row-based.

  @section Query_log_event_binary_format Binary format

  See @ref Log_event_binary_format "Binary format for log events" for
  a general discussion and introduction to the binary format of binlog
  events.

  The Post-Header has five components:

  <table>
  <caption>Post-Header for Query_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>slave_proxy_id</td>
    <td>4 byte unsigned integer</td>
    <td>An integer identifying the client thread that issued the
    query.  The id is unique per server.  (Note, however, that two
    threads on different servers may have the same slave_proxy_id.)
    This is used when a client thread creates a temporary table local
    to the client.  The slave_proxy_id is used to distinguish
    temporary tables that belong to different clients.
    </td>
  </tr>

  <tr>
    <td>exec_time</td>
    <td>4 byte unsigned integer</td>
    <td>The time from when the query started to when it was logged in
    the binlog, in seconds.</td>
  </tr>

  <tr>
    <td>db_len</td>
    <td>1 byte integer</td>
    <td>The length of the name of the currently selected database.</td>
  </tr>

  <tr>
    <td>error_code</td>
    <td>2 byte unsigned integer</td>
    <td>Error code generated by the master.  If the master fails, the
    slave will fail with the same error code, except for the error
    codes ER_DB_CREATE_EXISTS == 1007 and ER_DB_DROP_EXISTS == 1008.
    </td>
  </tr>

  <tr>
    <td>status_vars_len</td>
    <td>2 byte unsigned integer</td>
    <td>The length of the status_vars block of the Body, in bytes. See
    @ref query_log_event_status_vars "below".
    </td>
  </tr>
  </table>

  The Body has the following components:

  <table>
  <caption>Body for Query_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>@anchor query_log_event_status_vars status_vars</td>
    <td>status_vars_len bytes</td>
    <td>Zero or more status variables.  Each status variable consists
    of one byte identifying the variable stored, followed by the value
    of the variable.  The possible variables are listed separately in
    the table @ref Table_query_log_event_status_vars "below".  MySQL
    always writes events in the order defined below; however, it is
    capable of reading them in any order.  </td>
  </tr>

  <tr>
    <td>db</td>
    <td>db_len+1</td>
    <td>The currently selected database, as a null-terminated string.

    (The trailing zero is redundant since the length is already known;
    it is db_len from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>query</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>The SQL query.</td>
  </tr>
  </table>

  The following table lists the status variables that may appear in
  the status_vars field.

  @anchor Table_query_log_event_status_vars
  <table>
  <caption>Status variables for Query_log_event</caption>

  <tr>
    <th>Status variable</th>
    <th>1 byte identifier</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>flags2</td>
    <td>Q_FLAGS2_CODE == 0</td>
    <td>4 byte bitfield</td>
    <td>The flags in @c thd->options, binary AND-ed with @c
    OPTIONS_WRITTEN_TO_BIN_LOG.  The @c thd->options bitfield contains
    options for "SELECT".  @c OPTIONS_WRITTEN identifies those options
    that need to be written to the binlog (not all do).  Specifically,
    @c OPTIONS_WRITTEN_TO_BIN_LOG equals (@c OPTION_AUTO_IS_NULL | @c
    OPTION_NO_FOREIGN_KEY_CHECKS | @c OPTION_RELAXED_UNIQUE_CHECKS |
    @c OPTION_NOT_AUTOCOMMIT), or 0x0c084000 in hex.

    These flags correspond to the SQL variables SQL_AUTO_IS_NULL,
    FOREIGN_KEY_CHECKS, UNIQUE_CHECKS, and AUTOCOMMIT, documented in
    the "SET Syntax" section of the MySQL Manual.

    This field is always written to the binlog in version >= 5.0, and
    never written in version < 5.0.
    </td>
  </tr>

  <tr>
    <td>sql_mode</td>
    <td>Q_SQL_MODE_CODE == 1</td>
    <td>8 byte bitfield</td>
    <td>The @c sql_mode variable.  See the section "SQL Modes" in the
    MySQL manual, and see sql_priv.h for a list of the possible
    flags. Currently (2007-10-04), the following flags are available:
    <pre>
    MODE_REAL_AS_FLOAT==0x1
    MODE_PIPES_AS_CONCAT==0x2
    MODE_ANSI_QUOTES==0x4
    MODE_IGNORE_SPACE==0x8
    MODE_IGNORE_BAD_TABLE_OPTIONS==0x10
    MODE_ONLY_FULL_GROUP_BY==0x20
    MODE_NO_UNSIGNED_SUBTRACTION==0x40
    MODE_NO_DIR_IN_CREATE==0x80
    MODE_POSTGRESQL==0x100
    MODE_ORACLE==0x200
    MODE_MSSQL==0x400
    MODE_DB2==0x800
    MODE_MAXDB==0x1000
    MODE_NO_KEY_OPTIONS==0x2000
    MODE_NO_TABLE_OPTIONS==0x4000
    MODE_NO_FIELD_OPTIONS==0x8000
    MODE_MYSQL323==0x10000
    MODE_MYSQL323==0x20000
    MODE_MYSQL40==0x40000
    MODE_ANSI==0x80000
    MODE_NO_AUTO_VALUE_ON_ZERO==0x100000
    MODE_NO_BACKSLASH_ESCAPES==0x200000
    MODE_STRICT_TRANS_TABLES==0x400000
    MODE_STRICT_ALL_TABLES==0x800000
    MODE_NO_ZERO_IN_DATE==0x1000000
    MODE_NO_ZERO_DATE==0x2000000
    MODE_INVALID_DATES==0x4000000
    MODE_ERROR_FOR_DIVISION_BY_ZERO==0x8000000
    MODE_TRADITIONAL==0x10000000
    MODE_NO_AUTO_CREATE_USER==0x20000000
    MODE_HIGH_NOT_PRECEDENCE==0x40000000
    MODE_PAD_CHAR_TO_FULL_LENGTH==0x80000000
    </pre>
    All these flags are replicated from the server.  However, all
    flags except @c MODE_NO_DIR_IN_CREATE are honored by the slave;
    the slave always preserves its old value of @c
    MODE_NO_DIR_IN_CREATE.  For a rationale, see comment in
    @c Query_log_event::do_apply_event in @c log_event.cc.

    This field is always written to the binlog.
    </td>
  </tr>

  <tr>
    <td>catalog</td>
    <td>Q_CATALOG_NZ_CODE == 6</td>
    <td>Variable-length string: the length in bytes (1 byte) followed
    by the characters (at most 255 bytes)
    </td>
    <td>Stores the client's current catalog.  Every database belongs
    to a catalog, the same way that every table belongs to a
    database.  Currently, there is only one catalog, "std".

    This field is written if the length of the catalog is > 0;
    otherwise it is not written.
    </td>
  </tr>

  <tr>
    <td>auto_increment</td>
    <td>Q_AUTO_INCREMENT == 3</td>
    <td>two 2 byte unsigned integers, totally 2+2=4 bytes</td>

    <td>The two variables auto_increment_increment and
    auto_increment_offset, in that order.  For more information, see
    "System variables" in the MySQL manual.

    This field is written if auto_increment > 1.  Otherwise, it is not
    written.
    </td>
  </tr>

  <tr>
    <td>charset</td>
    <td>Q_CHARSET_CODE == 4</td>
    <td>three 2 byte unsigned integers, totally 2+2+2=6 bytes</td>
    <td>The three variables character_set_client,
    collation_connection, and collation_server, in that order.
    character_set_client is a code identifying the character set and
    collation used by the client to encode the query.
    collation_connection identifies the character set and collation
    that the master converts the query to when it receives it; this is
    useful when comparing literal strings.  collation_server is the
    default character set and collation used when a new database is
    created.

    See also "Connection Character Sets and Collations" in the MySQL
    5.1 manual.

    All three variables are codes identifying a (character set,
    collation) pair.  To see which codes map to which pairs, run the
    query "SELECT id, character_set_name, collation_name FROM
    COLLATIONS".

    Cf. Q_CHARSET_DATABASE_CODE below.

    This field is always written.
    </td>
  </tr>

  <tr>
    <td>time_zone</td>
    <td>Q_TIME_ZONE_CODE == 5</td>
    <td>Variable-length string: the length in bytes (1 byte) followed
    by the characters (at most 255 bytes).
    <td>The time_zone of the master.

    See also "System Variables" and "MySQL Server Time Zone Support"
    in the MySQL manual.

    This field is written if the length of the time zone string is >
    0; otherwise, it is not written.
    </td>
  </tr>

  <tr>
    <td>lc_time_names_number</td>
    <td>Q_LC_TIME_NAMES_CODE == 7</td>
    <td>2 byte integer</td>
    <td>A code identifying a table of month and day names.  The
    mapping from codes to languages is defined in @c sql_locale.cc.

    This field is written if it is not 0, i.e., if the locale is not
    en_US.
    </td>
  </tr>

  <tr>
    <td>charset_database_number</td>
    <td>Q_CHARSET_DATABASE_CODE == 8</td>
    <td>2 byte integer</td>

    <td>The value of the collation_database system variable (in the
    source code stored in @c thd->variables.collation_database), which
    holds the code for a (character set, collation) pair as described
    above (see Q_CHARSET_CODE).

    collation_database was used in old versions (???WHEN).  Its value
    was loaded when issuing a "use db" query and could be changed by
    issuing a "SET collation_database=xxx" query.  It used to affect
    the "LOAD DATA INFILE" and "CREATE TABLE" commands.

    In newer versions, "CREATE TABLE" has been changed to take the
    character set from the database of the created table, rather than
    the character set of the current database.  This makes a
    difference when creating a table in another database than the
    current one.  "LOAD DATA INFILE" has not yet changed to do this,
    but there are plans to eventually do it, and to make
    collation_database read-only.

    This field is written if it is not 0.
    </td>
  </tr>
  <tr>
    <td>table_map_for_update</td>
    <td>Q_TABLE_MAP_FOR_UPDATE_CODE == 9</td>
    <td>8 byte integer</td>

    <td>The value of the table map that is to be updated by the
    multi-table update query statement. Every bit of this variable
    represents a table, and is set to 1 if the corresponding table is
    to be updated by this statement.

    The value of this variable is set when executing a multi-table update
    statement and used by slave to apply filter rules without opening
    all the tables on slave. This is required because some tables may
    not exist on slave because of the filter rules.
    </td>
  </tr>
  </table>

  @subsection Query_log_event_notes_on_previous_versions Notes on Previous Versions

  * Status vars were introduced in version 5.0.  To read earlier
  versions correctly, check the length of the Post-Header.

  * The status variable Q_CATALOG_CODE == 2 existed in MySQL 5.0.x,
  where 0<=x<=3.  It was identical to Q_CATALOG_CODE, except that the
  string had a trailing '\0'.  The '\0' was removed in 5.0.4 since it
  was redundant (the string length is stored before the string).  The
  Q_CATALOG_CODE will never be written by a new master, but can still
  be understood by a new slave.

  * See Q_CHARSET_DATABASE_CODE in the table above.

  * When adding new status vars, please don't forget to update the
  MAX_SIZE_LOG_EVENT_STATUS, and update function code_name

*/
class Query_log_event: public Log_event
{
  LEX_STRING user;
  LEX_STRING host;
protected:
  Log_event::Byte* data_buf;
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
    For events created by Query_log_event::do_apply_event (and
    Load_log_event::do_apply_event()) we need the *original* thread
    id, to be able to log the event with the original (=master's)
    thread id (fix for BUG#1686).
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
    (OPTIONS_WRITTEN_TO_BIN_LOG).
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
  ulonglong sql_mode;
  ulong auto_increment_increment, auto_increment_offset;
  char charset[6];
  uint time_zone_len; /* 0 means uninited */
  const char *time_zone_str;
  uint lc_time_names_number; /* 0 means en_US */
  uint charset_database_number;
  /*
    map for tables that will be updated for a multi-table update query
    statement, for other query statements, this will be zero.
  */
  ulonglong table_map_for_update;
  /*
    Holds the original length of a Query_log_event that comes from a
    master of version < 5.0 (i.e., binlog_version < 4). When the IO
    thread writes the relay log, it augments the Query_log_event with a
    Q_MASTER_DATA_WRITTEN_CODE status_var that holds the original event
    length. This field is initialized to non-zero in the SQL thread when
    it reads this augmented event. SQL thread does not write 
    Q_MASTER_DATA_WRITTEN_CODE to the slave's server binlog.
  */
  uint32 master_data_written;

#ifdef MYSQL_SERVER

  Query_log_event(THD* thd_arg, const char* query_arg, ulong query_length,
                  bool using_trans, bool direct, bool suppress_use, int error);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print_query_header(IO_CACHE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Query_log_event();
  Query_log_event(const char* buf, uint event_len,
                  const Format_description_log_event *description_event,
                  Log_event_type event_type);
  ~Query_log_event()
  {
    if (data_buf)
      my_free(data_buf);
  }
  Log_event_type get_type_code() { return QUERY_EVENT; }
#ifdef MYSQL_SERVER
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

public:        /* !!! Public in this patch to allow old usage */
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);

  int do_apply_event(Relay_log_info const *rli,
                       const char *query_arg,
                       uint32 q_len_arg);
#endif /* HAVE_REPLICATION */
  /*
    If true, the event always be applied by slave SQL thread or be printed by
    mysqlbinlog
   */
  bool is_trans_keyword()
  {
    /*
      Before the patch for bug#50407, The 'SAVEPOINT and ROLLBACK TO'
      queries input by user was written into log events directly.
      So the keywords can be written in both upper case and lower case
      together, strncasecmp is used to check both cases. they also could be
      binlogged with comments in the front of these keywords. for examples:
        / * bla bla * / SAVEPOINT a;
        / * bla bla * / ROLLBACK TO a;
      but we don't handle these cases and after the patch, both quiries are
      binlogged in upper case with no comments.
     */
    return !strncmp(query, "BEGIN", q_len) ||
      !strncmp(query, "COMMIT", q_len) ||
      !strncasecmp(query, "SAVEPOINT", 9) ||
      !strncasecmp(query, "ROLLBACK", 8);
  }
};


#ifdef HAVE_REPLICATION

/**
  @class Slave_log_event

  Note that this class is currently not used at all; no code writes a
  @c Slave_log_event (though some code in @c repl_failsafe.cc reads @c
  Slave_log_event).  So it's not a problem if this code is not
  maintained.

  @section Slave_log_event_binary_format Binary Format

  This event type has no Post-Header. The Body has the following
  four components.

  <table>
  <caption>Body for Slave_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>master_pos</td>
    <td>8 byte integer</td>
    <td>???TODO
    </td>
  </tr>

  <tr>
    <td>master_port</td>
    <td>2 byte integer</td>
    <td>???TODO</td>
  </tr>

  <tr>
    <td>master_host</td>
    <td>null-terminated string</td>
    <td>???TODO</td>
  </tr>

  <tr>
    <td>master_log</td>
    <td>null-terminated string</td>
    <td>???TODO</td>
  </tr>
  </table>
*/
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

#ifdef MYSQL_SERVER
  Slave_log_event(THD* thd_arg, Relay_log_info* rli);
  void pack_info(THD *thd, Protocol* protocol);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Slave_log_event(const char* buf,
                  uint event_len,
                  const Format_description_log_event *description_event);
  ~Slave_log_event();
  int get_data_size();
  bool is_valid() const { return master_host != 0; }
  Log_event_type get_type_code() { return SLAVE_EVENT; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const* rli);
#endif
};

#endif /* HAVE_REPLICATION */


/**
  @class Load_log_event

  This log event corresponds to a "LOAD DATA INFILE" SQL query on the
  following form:

  @verbatim
   (1)    USE db;
   (2)    LOAD DATA [CONCURRENT] [LOCAL] INFILE 'file_name'
   (3)    [REPLACE | IGNORE]
   (4)    INTO TABLE 'table_name'
   (5)    [FIELDS
   (6)      [TERMINATED BY 'field_term']
   (7)      [[OPTIONALLY] ENCLOSED BY 'enclosed']
   (8)      [ESCAPED BY 'escaped']
   (9)    ]
  (10)    [LINES
  (11)      [TERMINATED BY 'line_term']
  (12)      [LINES STARTING BY 'line_start']
  (13)    ]
  (14)    [IGNORE skip_lines LINES]
  (15)    (field_1, field_2, ..., field_n)@endverbatim

  @section Load_log_event_binary_format Binary Format

  The Post-Header consists of the following six components.

  <table>
  <caption>Post-Header for Load_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>slave_proxy_id</td>
    <td>4 byte unsigned integer</td>
    <td>An integer identifying the client thread that issued the
    query.  The id is unique per server.  (Note, however, that two
    threads on different servers may have the same slave_proxy_id.)
    This is used when a client thread creates a temporary table local
    to the client.  The slave_proxy_id is used to distinguish
    temporary tables that belong to different clients.
    </td>
  </tr>

  <tr>
    <td>exec_time</td>
    <td>4 byte unsigned integer</td>
    <td>The time from when the query started to when it was logged in
    the binlog, in seconds.</td>
  </tr>

  <tr>
    <td>skip_lines</td>
    <td>4 byte unsigned integer</td>
    <td>The number on line (14) above, if present, or 0 if line (14)
    is left out.
    </td>
  </tr>

  <tr>
    <td>table_name_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of 'table_name' on line (4) above.</td>
  </tr>

  <tr>
    <td>db_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of 'db' on line (1) above.</td>
  </tr>

  <tr>
    <td>num_fields</td>
    <td>4 byte unsigned integer</td>
    <td>The number n of fields on line (15) above.</td>
  </tr>
  </table>    

  The Body contains the following components.

  <table>
  <caption>Body of Load_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>sql_ex</td>
    <td>variable length</td>

    <td>Describes the part of the query on lines (3) and
    (5)&ndash;(13) above.  More precisely, it stores the five strings
    (on lines) field_term (6), enclosed (7), escaped (8), line_term
    (11), and line_start (12); as well as a bitfield indicating the
    presence of the keywords REPLACE (3), IGNORE (3), and OPTIONALLY
    (7).

    The data is stored in one of two formats, called "old" and "new".
    The type field of Common-Header determines which of these two
    formats is used: type LOAD_EVENT means that the old format is
    used, and type NEW_LOAD_EVENT means that the new format is used.
    When MySQL writes a Load_log_event, it uses the new format if at
    least one of the five strings is two or more bytes long.
    Otherwise (i.e., if all strings are 0 or 1 bytes long), the old
    format is used.

    The new and old format differ in the way the five strings are
    stored.

    <ul>
    <li> In the new format, the strings are stored in the order
    field_term, enclosed, escaped, line_term, line_start. Each string
    consists of a length (1 byte), followed by a sequence of
    characters (0-255 bytes).  Finally, a boolean combination of the
    following flags is stored in 1 byte: REPLACE_FLAG==0x4,
    IGNORE_FLAG==0x8, and OPT_ENCLOSED_FLAG==0x2.  If a flag is set,
    it indicates the presence of the corresponding keyword in the SQL
    query.

    <li> In the old format, we know that each string has length 0 or
    1.  Therefore, only the first byte of each string is stored.  The
    order of the strings is the same as in the new format.  These five
    bytes are followed by the same 1 byte bitfield as in the new
    format.  Finally, a 1 byte bitfield called empty_flags is stored.
    The low 5 bits of empty_flags indicate which of the five strings
    have length 0.  For each of the following flags that is set, the
    corresponding string has length 0; for the flags that are not set,
    the string has length 1: FIELD_TERM_EMPTY==0x1,
    ENCLOSED_EMPTY==0x2, LINE_TERM_EMPTY==0x4, LINE_START_EMPTY==0x8,
    ESCAPED_EMPTY==0x10.
    </ul>

    Thus, the size of the new format is 6 bytes + the sum of the sizes
    of the five strings.  The size of the old format is always 7
    bytes.
    </td>
  </tr>

  <tr>
    <td>field_lens</td>
    <td>num_fields 1 byte unsigned integers</td>
    <td>An array of num_fields integers representing the length of
    each field in the query.  (num_fields is from the Post-Header).
    </td>
  </tr>

  <tr>
    <td>fields</td>
    <td>num_fields null-terminated strings</td>
    <td>An array of num_fields null-terminated strings, each
    representing a field in the query.  (The trailing zero is
    redundant, since the length are stored in the num_fields array.)
    The total length of all strings equals to the sum of all
    field_lens, plus num_fields bytes for all the trailing zeros.
    </td>
  </tr>

  <tr>
    <td>table_name</td>
    <td>null-terminated string of length table_len+1 bytes</td>
    <td>The 'table_name' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the table_len is
    known from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>db</td>
    <td>null-terminated string of length db_len+1 bytes</td>
    <td>The 'db' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the db_len is known
    from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>file_name</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>The 'file_name' from the query.
    </td>
  </tr>

  </table>

  @subsection Load_log_event_notes_on_previous_versions Notes on Previous Versions

  This event type is understood by current versions, but only
  generated by MySQL 3.23 and earlier.
*/
class Load_log_event: public Log_event
{
private:
protected:
  int copy_log_event(const char *buf, ulong event_len,
                     int body_offset,
                     const Format_description_log_event* description_event);

public:
  void print_query(THD *thd, bool need_db, const char *cs, String *buf,
                   my_off_t *fn_start, my_off_t *fn_end,
                   const char *qualify_db);
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
  /**
    Indicates that this event corresponds to LOAD DATA CONCURRENT,

    @note Since Load_log_event event coming from the binary log
          lacks information whether LOAD DATA on master was concurrent
          or not, this flag is only set to TRUE for an auxiliary
          Load_log_event object which is used in mysql_load() to
          re-construct LOAD DATA statement from function parameters,
          for logging.
  */
  bool is_concurrent;

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

#ifdef MYSQL_SERVER
  String field_lens_buf;
  String fields_buf;

  Load_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
		 const char* table_name_arg,
		 List<Item>& fields_arg,
                 bool is_concurrent_arg,
                 enum enum_duplicates handle_dup, bool ignore,
		 bool using_trans);
  void set_fields(const char* db, List<Item> &fields_arg,
                  Name_resolution_context *context);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
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
#ifdef MYSQL_SERVER
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

public:        /* !!! Public in this patch to allow old usage */
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const* rli)
  {
    return do_apply_event(thd->slave_net,rli,0);
  }

  int do_apply_event(NET *net, Relay_log_info const *rli,
                     bool use_rli_only_for_errors);
#endif
};

/**
  @class Start_log_event_v3

  Start_log_event_v3 is the Start_log_event of binlog format 3 (MySQL 3.23 and
  4.x).

  Format_description_log_event derives from Start_log_event_v3; it is
  the Start_log_event of binlog format 4 (MySQL 5.0), that is, the
  event that describes the other events' Common-Header/Post-Header
  lengths. This event is sent by MySQL 5.0 whenever it starts sending
  a new binlog if the requested position is >4 (otherwise if ==4 the
  event will be sent naturally).

  @section Start_log_event_v3_binary_format Binary Format
*/
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
    We set this to 1 if we don't want to have the created time in the log,
    which is the case when we rollover to a new log.
  */
  bool dont_set_created;

#ifdef MYSQL_SERVER
  Start_log_event_v3();
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  Start_log_event_v3() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Start_log_event_v3(const char* buf, uint event_len,
                     const Format_description_log_event* description_event);
  ~Start_log_event_v3() {}
  Log_event_type get_type_code() { return START_EVENT_V3;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return server_version[0] != 0; }
  int get_data_size()
  {
    return START_V3_HEADER_LEN; //no variable-sized part
  }

protected:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info*)
  {
    /*
      Events from ourself should be skipped, but they should not
      decrease the slave skip counter.
     */
    if (this->server_id == ::server_id)
      return Log_event::EVENT_SKIP_IGNORE;
    else
      return Log_event::EVENT_SKIP_NOT;
  }
#endif
};


/**
  @class Format_description_log_event

  For binlog version 4.
  This event is saved by threads which read it, as they need it for future
  use (to decode the ordinary events).

  @section Format_description_log_event_binary_format Binary Format
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
  /* 
     The list of post-headers' lengths followed 
     by the checksum alg decription byte
  */
  uint8 *post_header_len;
  struct master_version_split {
    enum {KIND_MYSQL, KIND_MARIADB};
    int kind;
    uchar ver[3];
  };
  master_version_split server_version_split;
  const uint8 *event_type_permutation;

  Format_description_log_event(uint8 binlog_ver, const char* server_ver=0);
  Format_description_log_event(const char* buf, uint event_len,
                               const Format_description_log_event
                               *description_event);
  ~Format_description_log_event()
  {
    my_free(post_header_len);
  }
  Log_event_type get_type_code() { return FORMAT_DESCRIPTION_EVENT;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool header_is_valid() const
  {
    return ((common_header_len >= ((binlog_version==1) ? OLD_HEADER_LEN :
                                   LOG_EVENT_MINIMAL_HEADER_LEN)) &&
            (post_header_len != NULL));
  }

  bool version_is_valid() const
  {
    /* It is invalid only when all version numbers are 0 */
    return !(server_version_split.ver[0] == 0 &&
             server_version_split.ver[1] == 0 &&
             server_version_split.ver[2] == 0);
  }

  bool is_valid() const
  {
    return header_is_valid() && version_is_valid();
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

  void calc_server_version_split();
  static bool is_version_before_checksum(const master_version_split *version_split);
protected:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Intvar_log_event

  An Intvar_log_event will be created just before a Query_log_event,
  if the query uses one of the variables LAST_INSERT_ID or INSERT_ID.
  Each Intvar_log_event holds the value of one of these variables.

  @section Intvar_log_event_binary_format Binary Format

  The Post-Header for this event type is empty.  The Body has two
  components:

  <table>
  <caption>Body for Intvar_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>type</td>
    <td>1 byte enumeration</td>
    <td>One byte identifying the type of variable stored.  Currently,
    two identifiers are supported:  LAST_INSERT_ID_EVENT==1 and
    INSERT_ID_EVENT==2.
    </td>
  </tr>

  <tr>
    <td>value</td>
    <td>8 byte unsigned integer</td>
    <td>The value of the variable.</td>
  </tr>

  </table>
*/
class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;

#ifdef MYSQL_SERVER
Intvar_log_event(THD* thd_arg,uchar type_arg, ulonglong val_arg,
                 bool using_trans, bool direct)
    :Log_event(thd_arg,0,using_trans),val(val_arg),type(type_arg)
  {
    if (direct)
      cache_type= Log_event::EVENT_NO_CACHE;
  }
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Intvar_log_event(const char* buf,
                   const Format_description_log_event *description_event);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  const char* get_var_type_name();
  int get_data_size() { return  9; /* sizeof(type) + sizeof(val) */;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Rand_log_event

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.0.
  4.1.1 does not need it (it's repeatable again) so this event needn't be
  written in 4.1.1 for PASSWORD() (but the fact that it is written is just a
  waste, it does not cause bugs).

  The state of the random number generation consists of 128 bits,
  which are stored internally as two 64-bit numbers.

  @section Rand_log_event_binary_format Binary Format  

  The Post-Header for this event type is empty.  The Body has two
  components:

  <table>
  <caption>Body for Rand_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>seed1</td>
    <td>8 byte unsigned integer</td>
    <td>64 bit random seed1.</td>
  </tr>

  <tr>
    <td>seed2</td>
    <td>8 byte unsigned integer</td>
    <td>64 bit random seed2.</td>
  </tr>
  </table>
*/

class Rand_log_event: public Log_event
{
 public:
  ulonglong seed1;
  ulonglong seed2;

#ifdef MYSQL_SERVER
  Rand_log_event(THD* thd_arg, ulonglong seed1_arg, ulonglong seed2_arg,
                 bool using_trans, bool direct)
    :Log_event(thd_arg,0,using_trans),seed1(seed1_arg),seed2(seed2_arg)
  {
    if (direct)
      cache_type= Log_event::EVENT_NO_CACHE;
  }
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Rand_log_event(const char* buf,
                 const Format_description_log_event *description_event);
  ~Rand_log_event() {}
  Log_event_type get_type_code() { return RAND_EVENT;}
  int get_data_size() { return 16; /* sizeof(ulonglong) * 2*/ }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};

/**
  @class Xid_log_event

  Logs xid of the transaction-to-be-committed in the 2pc protocol.
  Has no meaning in replication, slaves ignore it.

  @section Xid_log_event_binary_format Binary Format  
*/
#ifdef MYSQL_CLIENT
typedef ulonglong my_xid; // this line is the same as in handler.h
#endif

class Xid_log_event: public Log_event
{
 public:
   my_xid xid;

#ifdef MYSQL_SERVER
  Xid_log_event(THD* thd_arg, my_xid x, bool direct):
   Log_event(thd_arg, 0, TRUE), xid(x)
   {
     if (direct)
       cache_type= Log_event::EVENT_NO_CACHE;
   }
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Xid_log_event(const char* buf,
                const Format_description_log_event *description_event);
  ~Xid_log_event() {}
  Log_event_type get_type_code() { return XID_EVENT;}
  int get_data_size() { return sizeof(xid); }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};

/**
  @class User_var_log_event

  Every time a query uses the value of a user variable, a User_var_log_event is
  written before the Query_log_event, to set the user variable.

  @section User_var_log_event_binary_format Binary Format  
*/

class User_var_log_event: public Log_event
{
public:
  enum {
    UNDEF_F= 0,
    UNSIGNED_F= 1
  };
  char *name;
  uint name_len;
  char *val;
  ulong val_len;
  Item_result type;
  uint charset_number;
  bool is_null;
  uchar flags;
#ifdef MYSQL_SERVER
  bool deferred;
  query_id_t query_id;
  User_var_log_event(THD* thd_arg, char *name_arg, uint name_len_arg,
                     char *val_arg, ulong val_len_arg, Item_result type_arg,
		     uint charset_number_arg, uchar flags_arg,
                     bool using_trans, bool direct)
    :Log_event(thd_arg, 0, using_trans),
    name(name_arg), name_len(name_len_arg), val(val_arg),
    val_len(val_len_arg), type(type_arg), charset_number(charset_number_arg),
    flags(flags_arg), deferred(false)
    {
      is_null= !val;
      if (direct)
        cache_type= Log_event::EVENT_NO_CACHE;
    }
  void pack_info(THD *thd, Protocol* protocol);
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  User_var_log_event(const char* buf, uint event_len,
                     const Format_description_log_event *description_event);
  ~User_var_log_event() {}
  Log_event_type get_type_code() { return USER_VAR_EVENT;}
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  /* 
     Getter and setter for deferred User-event. 
     Returns true if the event is not applied directly 
     and which case the applier adjusts execution path.
  */
  bool is_deferred() { return deferred; }
  /*
    In case of the deffered applying the variable instance is flagged
    and the parsing time query id is stored to be used at applying time.
  */
  void set_deferred(query_id_t qid) { deferred= true; query_id= qid; }
#endif
  bool is_valid() const { return name != 0; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/**
  @class Stop_log_event

  @section Stop_log_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/
class Stop_log_event: public Log_event
{
public:
#ifdef MYSQL_SERVER
  Stop_log_event() :Log_event()
  {}
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Stop_log_event(const char* buf,
                 const Format_description_log_event *description_event):
    Log_event(buf, description_event)
  {}
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  bool is_valid() const { return 1; }

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli)
  {
    /*
      Events from ourself should be skipped, but they should not
      decrease the slave skip counter.
     */
    if (this->server_id == ::server_id)
      return Log_event::EVENT_SKIP_IGNORE;
    else
      return Log_event::EVENT_SKIP_NOT;
  }
#endif
};

/**
  @class Rotate_log_event

  This will be deprecated when we move to using sequence ids.

  @section Rotate_log_event_binary_format Binary Format

  The Post-Header has one component:

  <table>
  <caption>Post-Header for Rotate_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>position</td>
    <td>8 byte integer</td>
    <td>The position within the binlog to rotate to.</td>
  </tr>

  </table>

  The Body has one component:

  <table>
  <caption>Body for Rotate_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>new_log</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>Name of the binlog to rotate to.</td>
  </tr>

  </table>
*/

class Rotate_log_event: public Log_event
{
public:
  enum {
    DUP_NAME= 2, // if constructor should dup the string argument
    RELAY_LOG=4  // rotate event for relay log
  };
  const char* new_log_ident;
  ulonglong pos;
  uint ident_len;
  uint flags;
#ifdef MYSQL_SERVER
  Rotate_log_event(const char* new_log_ident_arg,
		   uint ident_len_arg,
		   ulonglong pos_arg, uint flags);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Rotate_log_event(const char* buf, uint event_len,
                   const Format_description_log_event* description_event);
  ~Rotate_log_event()
  {
    if (flags & DUP_NAME)
      my_free((void*) new_log_ident);
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len + ROTATE_HEADER_LEN;}
  bool is_valid() const { return new_log_ident != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/* the classes below are for the new LOAD DATA INFILE logging */

/**
  @class Create_file_log_event

  @section Create_file_log_event_binary_format Binary Format
*/

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
  uchar* block;
  const char *event_buf;
  uint block_len;
  uint file_id;
  bool inited_from_old;

#ifdef MYSQL_SERVER
  Create_file_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			const char* table_name_arg,
			List<Item>& fields_arg,
                        bool is_concurrent_arg,
			enum enum_duplicates handle_dup, bool ignore,
			uchar* block_arg, uint block_len_arg,
			bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
             bool enable_local);
#endif

  Create_file_log_event(const char* buf, uint event_len,
                        const Format_description_log_event* description_event);
  ~Create_file_log_event()
  {
    my_free((void*) event_buf);
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
#ifdef MYSQL_SERVER
  bool write_data_header(IO_CACHE* file);
  bool write_data_body(IO_CACHE* file);
  /*
    Cut out Create_file extentions and
    write it as Load event - used on the slave
  */
  bool write_base(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Append_block_log_event

  @section Append_block_log_event_binary_format Binary Format
*/

class Append_block_log_event: public Log_event
{
public:
  uchar* block;
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

#ifdef MYSQL_SERVER
  Append_block_log_event(THD* thd, const char* db_arg, uchar* block_arg,
			 uint block_len_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
  virtual int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Append_block_log_event(const char* buf, uint event_len,
                         const Format_description_log_event
                         *description_event);
  ~Append_block_log_event() {}
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  int get_data_size() { return  block_len + APPEND_BLOCK_HEADER_LEN ;}
  bool is_valid() const { return block != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Delete_file_log_event

  @section Delete_file_log_event_binary_format Binary Format
*/

class Delete_file_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */

#ifdef MYSQL_SERVER
  Delete_file_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
             bool enable_local);
#endif

  Delete_file_log_event(const char* buf, uint event_len,
                        const Format_description_log_event* description_event);
  ~Delete_file_log_event() {}
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  int get_data_size() { return DELETE_FILE_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Execute_load_log_event

  @section Delete_file_log_event_binary_format Binary Format
*/

class Execute_load_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */

#ifdef MYSQL_SERVER
  Execute_load_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
#endif

  Execute_load_log_event(const char* buf, uint event_len,
                         const Format_description_log_event
                         *description_event);
  ~Execute_load_log_event() {}
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  int get_data_size() { return  EXEC_LOAD_HEADER_LEN ;}
  bool is_valid() const { return file_id != 0; }
#ifdef MYSQL_SERVER
  bool write(IO_CACHE* file);
  const char* get_db() { return db; }
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


/**
  @class Begin_load_query_log_event

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

  @section Begin_load_query_log_event_binary_format Binary Format
*/
class Begin_load_query_log_event: public Append_block_log_event
{
public:
#ifdef MYSQL_SERVER
  Begin_load_query_log_event(THD* thd_arg, const char *db_arg,
                             uchar* block_arg, uint block_len_arg,
                             bool using_trans);
#ifdef HAVE_REPLICATION
  Begin_load_query_log_event(THD* thd);
  int get_create_or_append() const;
#endif /* HAVE_REPLICATION */
#endif
  Begin_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_log_event
                             *description_event);
  ~Begin_load_query_log_event() {}
  Log_event_type get_type_code() { return BEGIN_LOAD_QUERY_EVENT; }
private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};


/*
  Elements of this enum describe how LOAD DATA handles duplicates.
*/
enum enum_load_dup_handling { LOAD_DUP_ERROR= 0, LOAD_DUP_IGNORE,
                              LOAD_DUP_REPLACE };

/**
  @class Execute_load_query_log_event

  Event responsible for LOAD DATA execution, it similar to Query_log_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

  @section Execute_load_query_log_event_binary_format Binary Format
*/
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

#ifdef MYSQL_SERVER
  Execute_load_query_log_event(THD* thd, const char* query_arg,
                               ulong query_length, uint fn_pos_start_arg,
                               uint fn_pos_end_arg,
                               enum_load_dup_handling dup_handling_arg,
                               bool using_trans, bool direct,
                               bool suppress_use, int errcode);
#ifdef HAVE_REPLICATION
  void pack_info(THD *thd, Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  /* Prints the query as LOAD DATA LOCAL and with rewritten filename */
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info,
	     const char *local_fname);
#endif
  Execute_load_query_log_event(const char* buf, uint event_len,
                               const Format_description_log_event
                               *description_event);
  ~Execute_load_query_log_event() {}

  Log_event_type get_type_code() { return EXECUTE_LOAD_QUERY_EVENT; }
  bool is_valid() const { return Query_log_event::is_valid() && file_id != 0; }

  ulong get_post_header_size_for_derived();
#ifdef MYSQL_SERVER
  bool write_post_header_for_derived(IO_CACHE* file);
#endif

private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif
};


#ifdef MYSQL_CLIENT
/**
  @class Unknown_log_event

  @section Unknown_log_event_binary_format Binary Format
*/
class Unknown_log_event: public Log_event
{
public:
  /*
    Even if this is an unknown event, we still pass description_event to
    Log_event's ctor, this way we can extract maximum information from the
    event's header (the unique ID for example).
  */
  Unknown_log_event(const char* buf,
                    const Format_description_log_event *description_event):
    Log_event(buf, description_event)
  {}
  ~Unknown_log_event() {}
  void print(FILE* file, PRINT_EVENT_INFO* print_event_info);
  Log_event_type get_type_code() { return UNKNOWN_EVENT;}
  bool is_valid() const { return 1; }
};
#endif
char *str_to_hex(char *to, const char *from, uint len);

/**
  @class Annotate_rows_log_event

  In row-based mode, if binlog_annotate_row_events = ON, each group of
  Table_map_log_events is preceded by an Annotate_rows_log_event which
  contains the query which caused the subsequent rows operations.

  The Annotate_rows_log_event has no post-header and its body contains
  the corresponding query (without trailing zero). Note. The query length
  is to be calculated as a difference between the whole event length and
  the common header length.
*/
class Annotate_rows_log_event: public Log_event
{
public:
#ifndef MYSQL_CLIENT
  Annotate_rows_log_event(THD*, bool using_trans, bool direct);
#endif
  Annotate_rows_log_event(const char *buf, uint event_len,
                          const Format_description_log_event*);
  ~Annotate_rows_log_event();

  virtual int get_data_size();
  virtual Log_event_type get_type_code();
  virtual bool is_valid() const;

#ifndef MYSQL_CLIENT
  virtual bool write_data_header(IO_CACHE*);
  virtual bool write_data_body(IO_CACHE*);
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  virtual void pack_info(THD *thd, Protocol*);
#endif

#ifdef MYSQL_CLIENT
  virtual void print(FILE*, PRINT_EVENT_INFO*);
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
private:
  virtual int do_apply_event(Relay_log_info const*);
  virtual int do_update_pos(Relay_log_info*);
  virtual enum_skip_reason do_shall_skip(Relay_log_info*);
#endif

private:
  char *m_query_txt;
  uint  m_query_len;
  char *m_save_thd_query_txt;
  uint  m_save_thd_query_len;
};

/**
  @class Table_map_log_event

  In row-based mode, every row operation event is preceded by a
  Table_map_log_event which maps a table definition to a number.  The
  table definition consists of database name, table name, and column
  definitions.

  @section Table_map_log_event_binary_format Binary Format

  The Post-Header has the following components:

  <table>
  <caption>Post-Header for Table_map_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>table_id</td>
    <td>6 bytes unsigned integer</td>
    <td>The number that identifies the table.</td>
  </tr>

  <tr>
    <td>flags</td>
    <td>2 byte bitfield</td>
    <td>Reserved for future use; currently always 0.</td>
  </tr>

  </table>

  The Body has the following components:

  <table>
  <caption>Body for Table_map_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>database_name</td>
    <td>one byte string length, followed by null-terminated string</td>
    <td>The name of the database in which the table resides.  The name
    is represented as a one byte unsigned integer representing the
    number of bytes in the name, followed by length bytes containing
    the database name, followed by a terminating 0 byte.  (Note the
    redundancy in the representation of the length.)  </td>
  </tr>

  <tr>
    <td>table_name</td>
    <td>one byte string length, followed by null-terminated string</td>
    <td>The name of the table, encoded the same way as the database
    name above.</td>
  </tr>

  <tr>
    <td>column_count</td>
    <td>@ref packed_integer "Packed Integer"</td>
    <td>The number of columns in the table, represented as a packed
    variable-length integer.</td>
  </tr>

  <tr>
    <td>column_type</td>
    <td>List of column_count 1 byte enumeration values</td>
    <td>The type of each column in the table, listed from left to
    right.  Each byte is mapped to a column type according to the
    enumeration type enum_field_types defined in mysql_com.h.  The
    mapping of types to numbers is listed in the table @ref
    Table_table_map_log_event_column_types "below" (along with
    description of the associated metadata field).  </td>
  </tr>

  <tr>
    <td>metadata_length</td>
    <td>@ref packed_integer "Packed Integer"</td>
    <td>The length of the following metadata block</td>
  </tr>

  <tr>
    <td>metadata</td>
    <td>list of metadata for each column</td>
    <td>For each column from left to right, a chunk of data who's
    length and semantics depends on the type of the column.  The
    length and semantics for the metadata for each column are listed
    in the table @ref Table_table_map_log_event_column_types
    "below".</td>
  </tr>

  <tr>
    <td>null_bits</td>
    <td>column_count bits, rounded up to nearest byte</td>
    <td>For each column, a bit indicating whether data in the column
    can be NULL or not.  The number of bytes needed for this is
    int((column_count+7)/8).  The flag for the first column from the
    left is in the least-significant bit of the first byte, the second
    is in the second least significant bit of the first byte, the
    ninth is in the least significant bit of the second byte, and so
    on.  </td>
  </tr>

  </table>

  The table below lists all column types, along with the numerical
  identifier for it and the size and interpretation of meta-data used
  to describe the type.

  @anchor Table_table_map_log_event_column_types
  <table>
  <caption>Table_map_log_event column types: numerical identifier and
  metadata</caption>
  <tr>
    <th>Name</th>
    <th>Identifier</th>
    <th>Size of metadata in bytes</th>
    <th>Description of metadata</th>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DECIMAL</td><td>0</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TINY</td><td>1</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_SHORT</td><td>2</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_LONG</td><td>3</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_FLOAT</td><td>4</td>
    <td>1 byte</td>
    <td>1 byte unsigned integer, representing the "pack_length", which
    is equal to sizeof(float) on the server from which the event
    originates.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DOUBLE</td><td>5</td>
    <td>1 byte</td>
    <td>1 byte unsigned integer, representing the "pack_length", which
    is equal to sizeof(double) on the server from which the event
    originates.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_NULL</td><td>6</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TIMESTAMP</td><td>7</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_LONGLONG</td><td>8</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_INT24</td><td>9</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DATE</td><td>10</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TIME</td><td>11</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_DATETIME</td><td>12</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_YEAR</td><td>13</td>
    <td>0</td>
    <td>No column metadata.</td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_NEWDATE</i></td><td><i>14</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_VARCHAR</td><td>15</td>
    <td>2 bytes</td>
    <td>2 byte unsigned integer representing the maximum length of
    the string.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_BIT</td><td>16</td>
    <td>2 bytes</td>
    <td>A 1 byte unsigned int representing the length in bits of the
    bitfield (0 to 64), followed by a 1 byte unsigned int
    representing the number of bytes occupied by the bitfield.  The
    number of bytes is either int((length+7)/8) or int(length/8).</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_NEWDECIMAL</td><td>246</td>
    <td>2 bytes</td>
    <td>A 1 byte unsigned int representing the precision, followed
    by a 1 byte unsigned int representing the number of decimals.</td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_ENUM</i></td><td><i>247</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_SET</i></td><td><i>248</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_TINY_BLOB</td><td>249</td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_MEDIUM_BLOB</i></td><td><i>250</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td><i>MYSQL_TYPE_LONG_BLOB</i></td><td><i>251</i></td>
    <td>&ndash;</td>
    <td><i>This enumeration value is only used internally and cannot
    exist in a binlog.</i></td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_BLOB</td><td>252</td>
    <td>1 byte</td>
    <td>The pack length, i.e., the number of bytes needed to represent
    the length of the blob: 1, 2, 3, or 4.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_VAR_STRING</td><td>253</td>
    <td>2 bytes</td>
    <td>This is used to store both strings and enumeration values.
    The first byte is a enumeration value storing the <i>real
    type</i>, which may be either MYSQL_TYPE_VAR_STRING or
    MYSQL_TYPE_ENUM.  The second byte is a 1 byte unsigned integer
    representing the field size, i.e., the number of bytes needed to
    store the length of the string.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_STRING</td><td>254</td>
    <td>2 bytes</td>
    <td>The first byte is always MYSQL_TYPE_VAR_STRING (i.e., 253).
    The second byte is the field size, i.e., the number of bytes in
    the representation of size of the string: 3 or 4.</td>
  </tr>

  <tr>
    <td>MYSQL_TYPE_GEOMETRY</td><td>255</td>
    <td>1 byte</td>
    <td>The pack length, i.e., the number of bytes needed to represent
    the length of the geometry: 1, 2, 3, or 4.</td>
  </tr>

  </table>
*/
class Table_map_log_event : public Log_event
{
public:
  /* Constants */
  enum
  {
    TYPE_CODE = TABLE_MAP_EVENT
  };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error
  {
    ERR_OPEN_FAILURE = -1,               /**< Failure to open table */
    ERR_OK = 0,                                 /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1,      /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,                         /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,     /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4  /**< daisy-chanining RBR to SBR not allowed */
  };

  enum enum_flag
  {
    /* 
       Nothing here right now, but the flags support is there in
       preparation for changes that are coming.  Need to add a
       constant to make it compile under HP-UX: aCC does not like
       empty enumerations.
    */
    ENUM_FLAG_COUNT
  };

  typedef uint16 flag_set;

  /* Special constants representing sets of flags */
  enum 
  {
    TM_NO_FLAGS = 0U,
    TM_BIT_LEN_EXACT_F = (1U << 0)
  };

  flag_set get_flags(flag_set flag) const { return m_flags & flag; }

#ifdef MYSQL_SERVER
  Table_map_log_event(THD *thd, TABLE *tbl, ulong tid, bool is_transactional);
#endif
#ifdef HAVE_REPLICATION
  Table_map_log_event(const char *buf, uint event_len, 
                      const Format_description_log_event *description_event);
#endif

  ~Table_map_log_event();

#ifdef MYSQL_CLIENT
  table_def *create_table_def()
  {
    return new table_def(m_coltype, m_colcnt, m_field_metadata,
                         m_field_metadata_size, m_null_bits, m_flags);
  }
  int rewrite_db(const char* new_name, size_t new_name_len,
                 const Format_description_log_event*);
#endif
  ulong get_table_id() const        { return m_table_id; }
  const char *get_table_name() const { return m_tblnam; }
  const char *get_db_name() const    { return m_dbnam; }

  virtual Log_event_type get_type_code() { return TABLE_MAP_EVENT; }
  virtual bool is_valid() const { return m_memory != NULL; /* we check malloc */ }

  virtual int get_data_size() { return (uint) m_data_size; } 
#ifdef MYSQL_SERVER
  virtual int save_field_metadata();
  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);
  virtual const char *get_db() { return m_dbnam; }
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual void pack_info(THD *thd, Protocol *protocol);
#endif

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif


private:
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif

#ifdef MYSQL_SERVER
  TABLE         *m_table;
#endif
  char const    *m_dbnam;
  size_t         m_dblen;
  char const    *m_tblnam;
  size_t         m_tbllen;
  ulong          m_colcnt;
  uchar         *m_coltype;

  uchar         *m_memory;
  ulong          m_table_id;
  flag_set       m_flags;

  size_t         m_data_size;

  uchar          *m_field_metadata;        // buffer for field metadata
  /*
    The size of field metadata buffer set by calling save_field_metadata()
  */
  ulong          m_field_metadata_size;   
  uchar         *m_null_bits;
  uchar         *m_meta_memory;
};


/**
  @class Rows_log_event

 Common base class for all row-containing log events.

 RESPONSIBILITIES

   Encode the common parts of all events containing rows, which are:
   - Write data header and data body to an IO_CACHE.
   - Provide an interface for adding an individual row to the event.

  @section Rows_log_event_binary_format Binary Format
*/


class Rows_log_event : public Log_event
{
public:
  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error
  {
    ERR_OPEN_FAILURE = -1,               /**< Failure to open table */
    ERR_OK = 0,                                 /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1,      /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,                         /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,     /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4  /**< daisy-chanining RBR to SBR not allowed */
  };

  /*
    These definitions allow you to combine the flags into an
    appropriate flag set using the normal bitwise operators.  The
    implicit conversion from an enum-constant to an integer is
    accepted by the compiler, which is then used to set the real set
    of flags.
  */
  enum enum_flag
  {
    /* Last event of a statement */
    STMT_END_F = (1U << 0),

    /* Value of the OPTION_NO_FOREIGN_KEY_CHECKS flag in thd->options */
    NO_FOREIGN_KEY_CHECKS_F = (1U << 1),

    /* Value of the OPTION_RELAXED_UNIQUE_CHECKS flag in thd->options */
    RELAXED_UNIQUE_CHECKS_F = (1U << 2),

    /** 
      Indicates that rows in this event are complete, that is contain
      values for all columns of the table.
     */
    COMPLETE_ROWS_F = (1U << 3)
  };

  typedef uint16 flag_set;

  /* Special constants representing sets of flags */
  enum 
  {
      RLE_NO_FLAGS = 0U
  };

  virtual ~Rows_log_event();

  void set_flags(flag_set flags_arg) { m_flags |= flags_arg; }
  void clear_flags(flag_set flags_arg) { m_flags &= ~flags_arg; }
  flag_set get_flags(flag_set flags_arg) const { return m_flags & flags_arg; }

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual void pack_info(THD *thd, Protocol *protocol);
#endif

#ifdef MYSQL_CLIENT
  /* not for direct call, each derived has its own ::print() */
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info)= 0;
  void print_verbose(IO_CACHE *file,
                     PRINT_EVENT_INFO *print_event_info);
  size_t print_verbose_one_row(IO_CACHE *file, table_def *td,
                               PRINT_EVENT_INFO *print_event_info,
                               MY_BITMAP *cols_bitmap,
                               const uchar *ptr, const uchar *prefix);
#endif

#ifdef MYSQL_SERVER
  int add_row_data(uchar *data, size_t length)
  {
    return do_add_row_data(data,length); 
  }
#endif

  /* Member functions to implement superclass interface */
  virtual int get_data_size();

  MY_BITMAP const *get_cols() const { return &m_cols; }
  size_t get_width() const          { return m_width; }
  ulong get_table_id() const        { return m_table_id; }

#ifdef MYSQL_SERVER
  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);
  virtual const char *get_db() { return m_table->s->db.str; }
#endif
  /*
    Check that malloc() succeeded in allocating memory for the rows
    buffer and the COLS vector. Checking that an Update_rows_log_event
    is valid is done in the Update_rows_log_event::is_valid()
    function.
  */
  virtual bool is_valid() const
  {
    return m_rows_buf && m_cols.bitmap;
  }

  uint     m_row_count;         /* The number of rows added to the event */

protected:
  /* 
     The constructors are protected since you're supposed to inherit
     this class, not create instances of this class.
  */
#ifdef MYSQL_SERVER
  Rows_log_event(THD*, TABLE*, ulong table_id, 
		 MY_BITMAP const *cols, bool is_transactional);
#endif
  Rows_log_event(const char *row_data, uint event_len, 
		 Log_event_type event_type,
		 const Format_description_log_event *description_event);

#ifdef MYSQL_CLIENT
  void print_helper(FILE *, PRINT_EVENT_INFO *, char const *const name);
#endif

#ifdef MYSQL_SERVER
  virtual int do_add_row_data(uchar *data, size_t length);
#endif

#ifdef MYSQL_SERVER
  TABLE *m_table;		/* The table the rows belong to */
#endif
  ulong       m_table_id;	/* Table ID */
  MY_BITMAP   m_cols;		/* Bitmap denoting columns available */
  ulong       m_width;          /* The width of the columns bitmap */
  /*
    Bitmap for columns available in the after image, if present. These
    fields are only available for Update_rows events. Observe that the
    width of both the before image COLS vector and the after image
    COLS vector is the same: the number of columns of the table on the
    master.
  */
  MY_BITMAP   m_cols_ai;

  ulong       m_master_reclength; /* Length of record on master side */

  /* Bit buffers in the same memory as the class */
  uint32    m_bitbuf[128/(sizeof(uint32)*8)];
  uint32    m_bitbuf_ai[128/(sizeof(uint32)*8)];

  uchar    *m_rows_buf;		/* The rows in packed format */
  uchar    *m_rows_cur;		/* One-after the end of the data */
  uchar    *m_rows_end;		/* One-after the end of the allocated space */

  flag_set m_flags;		/* Flags for row-level events */

  /* helper functions */

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  const uchar *m_curr_row;     /* Start of the row being processed */
  const uchar *m_curr_row_end; /* One-after the end of the current row */
  uchar    *m_key;      /* Buffer to keep key value during searches */
  KEY      *m_key_info; /* Pointer to KEY info for m_key_nr */
  uint      m_key_nr;   /* Key number */

  int find_key(); // Find a best key to use in find_row()
  int find_row(const Relay_log_info *const);
  int write_row(const Relay_log_info *const, const bool);

  // Unpack the current row into m_table->record[0]
  int unpack_current_row(const Relay_log_info *const rli)
  {
    DBUG_ASSERT(m_table);

    ASSERT_OR_RETURN_ERROR(m_curr_row < m_rows_end, HA_ERR_CORRUPT_EVENT);
    return ::unpack_row(rli, m_table, m_width, m_curr_row, &m_cols,
                                   &m_curr_row_end, &m_master_reclength, m_rows_end);
  }

  /**
    Helper function to check whether there is an auto increment
    column on the table where the event is to be applied.

    @return true if there is an autoincrement field on the extra
            columns, false otherwise.
   */
  inline bool is_auto_inc_in_extra_columns()
  {
    DBUG_ASSERT(m_table);
    return (m_table->next_number_field &&
            m_table->next_number_field->field_index >= m_width);
  }
#endif

private:

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
  virtual int do_update_pos(Relay_log_info *rli);
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);

  /*
    Primitive to prepare for a sequence of row executions.

    DESCRIPTION

      Before doing a sequence of do_prepare_row() and do_exec_row()
      calls, this member function should be called to prepare for the
      entire sequence. Typically, this member function will allocate
      space for any buffers that are needed for the two member
      functions mentioned above.

    RETURN VALUE

      The member function will return 0 if all went OK, or a non-zero
      error code otherwise.
  */
  virtual 
  int do_before_row_operations(const Slave_reporting_capability *const log) = 0;

  /*
    Primitive to clean up after a sequence of row executions.

    DESCRIPTION
    
      After doing a sequence of do_prepare_row() and do_exec_row(),
      this member function should be called to clean up and release
      any allocated buffers.
      
      The error argument, if non-zero, indicates an error which happened during
      row processing before this function was called. In this case, even if 
      function is successful, it should return the error code given in the argument.
  */
  virtual 
  int do_after_row_operations(const Slave_reporting_capability *const log,
                              int error) = 0;

  /*
    Primitive to do the actual execution necessary for a row.

    DESCRIPTION
      The member function will do the actual execution needed to handle a row.
      The row is located at m_curr_row. When the function returns, 
      m_curr_row_end should point at the next row (one byte after the end
      of the current row).    

    RETURN VALUE
      0 if execution succeeded, 1 if execution failed.
      
  */
  virtual int do_exec_row(const Relay_log_info *const rli) = 0;
#endif /* defined(MYSQL_SERVER) && defined(HAVE_REPLICATION) */

  friend class Old_rows_log_event;
};

/**
  @class Write_rows_log_event

  Log row insertions and updates. The event contain several
  insert/update rows for a table. Note that each event contains only
  rows for one table.

  @section Write_rows_log_event_binary_format Binary Format
*/
class Write_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = WRITE_ROWS_EVENT
  };

#if defined(MYSQL_SERVER)
  Write_rows_log_event(THD*, TABLE*, ulong table_id, 
		       MY_BITMAP const *cols, bool is_transactional);
#endif
#ifdef HAVE_REPLICATION
  Write_rows_log_event(const char *buf, uint event_len, 
                       const Format_description_log_event *description_event);
#endif
#if defined(MYSQL_SERVER) 
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          MY_BITMAP *cols,
                                          uint fields,
                                          const uchar *before_record
                                          __attribute__((unused)),
                                          const uchar *after_record)
  {
    return thd->binlog_write_row(table, is_transactional,
                                 cols, fields, after_record);
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif
};


/**
  @class Update_rows_log_event

  Log row updates with a before image. The event contain several
  update rows for a table. Note that each event contains only rows for
  one table.

  Also note that the row data consists of pairs of row data: one row
  for the old data and one row for the new data.

  @section Update_rows_log_event_binary_format Binary Format
*/
class Update_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = UPDATE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Update_rows_log_event(THD*, TABLE*, ulong table_id,
			MY_BITMAP const *cols_bi,
			MY_BITMAP const *cols_ai,
                        bool is_transactional);

  Update_rows_log_event(THD*, TABLE*, ulong table_id,
			MY_BITMAP const *cols,
                        bool is_transactional);

  void init(MY_BITMAP const *cols);
#endif

  virtual ~Update_rows_log_event();

#ifdef HAVE_REPLICATION
  Update_rows_log_event(const char *buf, uint event_len, 
			const Format_description_log_event *description_event);
#endif

#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          MY_BITMAP *cols,
                                          uint fields,
                                          const uchar *before_record,
                                          const uchar *after_record)
  {
    return thd->binlog_update_row(table, is_transactional,
                                  cols, fields, before_record, after_record);
  }
#endif

  virtual bool is_valid() const
  {
    return Rows_log_event::is_valid() && m_cols_ai.bitmap;
  }

protected:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif /* defined(MYSQL_SERVER) && defined(HAVE_REPLICATION) */
};

/**
  @class Delete_rows_log_event

  Log row deletions. The event contain several delete rows for a
  table. Note that each event contains only rows for one table.

  RESPONSIBILITIES

    - Act as a container for rows that has been deleted on the master
      and should be deleted on the slave. 

  COLLABORATION

    Row_writer
      Create the event and add rows to the event.
    Row_reader
      Extract the rows from the event.

  @section Delete_rows_log_event_binary_format Binary Format
*/
class Delete_rows_log_event : public Rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = DELETE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Delete_rows_log_event(THD*, TABLE*, ulong, 
			MY_BITMAP const *cols, bool is_transactional);
#endif
#ifdef HAVE_REPLICATION
  Delete_rows_log_event(const char *buf, uint event_len, 
			const Format_description_log_event *description_event);
#endif
#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          MY_BITMAP *cols,
                                          uint fields,
                                          const uchar *before_record,
                                          const uchar *after_record
                                          __attribute__((unused)))
  {
    return thd->binlog_delete_row(table, is_transactional,
                                  cols, fields, before_record);
  }
#endif
  
protected:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#ifdef MYSQL_CLIENT
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_before_row_operations(const Slave_reporting_capability *const);
  virtual int do_after_row_operations(const Slave_reporting_capability *const,int);
  virtual int do_exec_row(const Relay_log_info *const);
#endif
};


#include "log_event_old.h"

/**
  @class Incident_log_event

   Class representing an incident, an occurance out of the ordinary,
   that happened on the master.

   The event is used to inform the slave that something out of the
   ordinary happened on the master that might cause the database to be
   in an inconsistent state.

   <table id="IncidentFormat">
   <caption>Incident event format</caption>
   <tr>
     <th>Symbol</th>
     <th>Format</th>
     <th>Description</th>
   </tr>
   <tr>
     <td>INCIDENT</td>
     <td align="right">2</td>
     <td>Incident number as an unsigned integer</td>
   </tr>
   <tr>
     <td>MSGLEN</td>
     <td align="right">1</td>
     <td>Message length as an unsigned integer</td>
   </tr>
   <tr>
     <td>MESSAGE</td>
     <td align="right">MSGLEN</td>
     <td>The message, if present. Not null terminated.</td>
   </tr>
   </table>

  @section Delete_rows_log_event_binary_format Binary Format
*/
class Incident_log_event : public Log_event {
public:
#ifdef MYSQL_SERVER
  Incident_log_event(THD *thd_arg, Incident incident)
    : Log_event(thd_arg, 0, FALSE), m_incident(incident)
  {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("m_incident: %d", m_incident));
    m_message.str= NULL;                    /* Just as a precaution */
    m_message.length= 0;
    set_direct_logging();
    /* Replicate the incident irregardless of @@skip_replication. */
    flags&= ~LOG_EVENT_SKIP_REPLICATION_F;
    DBUG_VOID_RETURN;
  }

  Incident_log_event(THD *thd_arg, Incident incident, LEX_STRING const msg)
    : Log_event(thd_arg, 0, FALSE), m_incident(incident)
  {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("m_incident: %d", m_incident));
    m_message= msg;
    set_direct_logging();
    /* Replicate the incident irregardless of @@skip_replication. */
    flags&= ~LOG_EVENT_SKIP_REPLICATION_F;
    DBUG_VOID_RETURN;
  }
#endif

#ifdef MYSQL_SERVER
  void pack_info(THD *thd, Protocol*);
#endif

  Incident_log_event(const char *buf, uint event_len,
                     const Format_description_log_event *descr_event);

  virtual ~Incident_log_event();

#ifdef MYSQL_CLIENT
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info);
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  virtual int do_apply_event(Relay_log_info const *rli);
#endif

  virtual bool write_data_header(IO_CACHE *file);
  virtual bool write_data_body(IO_CACHE *file);

  virtual Log_event_type get_type_code() { return INCIDENT_EVENT; }

  virtual bool is_valid() const
  {
    return m_incident > INCIDENT_NONE && m_incident < INCIDENT_COUNT;
  }
  virtual int get_data_size() {
    return INCIDENT_HEADER_LEN + 1 + (uint) m_message.length;
  }

private:
  const char *description() const;

  Incident m_incident;
  LEX_STRING m_message;
};

static inline bool copy_event_cache_to_file_and_reinit(IO_CACHE *cache,
                                                       FILE *file)
{
  return         
    my_b_copy_to_file(cache, file) ||
    reinit_io_cache(cache, WRITE_CACHE, 0, FALSE, TRUE);
}

#ifdef MYSQL_SERVER
/*****************************************************************************

  Heartbeat Log Event class

  Replication event to ensure to slave that master is alive.
  The event is originated by master's dump thread and sent straight to
  slave without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class log_ident and Log_event::log_pos comprise 
  @see the event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event master has sent from
  its binlog.

 ****************************************************************************/
class Heartbeat_log_event: public Log_event
{
public:
  Heartbeat_log_event(const char* buf, uint event_len,
                      const Format_description_log_event* description_event);
  Log_event_type get_type_code() { return HEARTBEAT_LOG_EVENT; }
  bool is_valid() const
    {
      return (log_ident != NULL &&
              log_pos >= BIN_LOG_HEADER_SIZE);
    }
  const char * get_log_ident() { return log_ident; }
  uint get_ident_len() { return ident_len; }
  
private:
  const char* log_ident;
  uint ident_len;
};

/**
   The function is called by slave applier in case there are
   active table filtering rules to force gathering events associated
   with Query-log-event into an array to execute
   them once the fate of the Query is determined for execution.
*/
bool slave_execute_deferred_events(THD *thd);
#endif

int append_query_string(THD *thd, CHARSET_INFO *csinfo,
                        String const *from, String *to);

bool rpl_get_position_info(const char **log_file_name, ulonglong *log_pos,
                           const char **group_relay_log_name,
                           ulonglong *relay_log_pos);

bool event_checksum_test(uchar *buf, ulong event_len, uint8 alg);
uint8 get_checksum_alg(const char* buf, ulong len);
extern TYPELIB binlog_checksum_typelib;

#ifndef MYSQL_CLIENT
/**
   The function is called by slave applier in case there are
   active table filtering rules to force gathering events associated
   with Query-log-event into an array to execute
   them once the fate of the Query is determined for execution.
*/
bool slave_execute_deferred_events(THD *thd);
#endif

/**
  @} (end of group Replication)
*/

#endif /* _log_event_h */
