/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#ifndef BINLOG_EVENT_INCLUDED
#define	BINLOG_EVENT_INCLUDED

#include "debug_vars.h"
/**
 The header contains functions macros for reading and storing in
 machine independent format (low byte first).
*/
#include "byteorder.h"
#include "wrapper_functions.h"
#include "cassert"
#include <zlib.h> //for checksum calculations
#include <stdint.h>
#ifdef min //definition of min() and max() in std and libmysqlclient
           //can be/are different
#undef min
#endif
#ifdef max
#undef max
#endif
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <vector>

/*
  The symbols below are a part of the common definitions between
  the MySQL server and the client. Since they should not be a part of
  this library but the server, these should be placed in a header file
  common to the library and the MySQL server code, so that if they are
  updated in the server code, it is reflected in the binlogapi also.

  TODO: Collect all the variables here and create a common header file,
  placing it in libbinlogapi/include.
*/
#ifndef SYSTEM_CHARSET_MBMAXLEN
#define SYSTEM_CHARSET_MBMAXLEN 3
#endif
#ifndef NAME_CHAR_LEN
#define NAME_CHAR_LEN   64                     /* Field/table name length */
#endif
#ifndef NAME_LEN
#define NAME_LEN (NAME_CHAR_LEN*SYSTEM_CHARSET_MBMAXLEN)
#endif

/**
   Enumeration of the incidents that can occur for the server.
 */
enum Incident {
  /** No incident */
  INCIDENT_NONE = 0,

  /** There are possibly lost events in the replication stream */
  INCIDENT_LOST_EVENTS = 1,

  /** Shall be last event of the enumeration */
  INCIDENT_COUNT
};

/*
   binlog_version 3 is MySQL 4.x; 4 is MySQL 5.0.0.
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
  Check if jump value is within buffer limits.

  @param jump         Number of positions we want to advance.
  @param buf_start    Pointer to buffer start.
  @param buf_current  Pointer to the current position on buffer.
  @param buf_len      Buffer length.

  @return             Number of bytes available on event buffer.
*/
template <class T> T available_buffer(const char* buf_start,
                                      const char* buf_current,
                                      T buf_len)
{
  return buf_len - (buf_current - buf_start);
}

/*
  Check if jump value is within buffer limits.

  @param jump         Number of positions we want to advance.
  @param buf_start    Pointer to buffer start
  @param buf_current  Pointer to the current position on buffer.
  @param buf_len      Buffer length.

  @return      True   If jump value is within buffer limits.
               False  Otherwise.
*/
template <class T> bool valid_buffer_range(T jump,
                                           const char* buf_start,
                                           const char* buf_current,
                                           T buf_len)
{
  return (jump <= available_buffer(buf_start, buf_current, buf_len));
}
/**
  Enumeration of group types formed while transactions.
  The structure of a group is as follows:
  Group {
        SID (16 byte UUID):         the source identifier for the group
        GNO (8 byte unsigned int):  the group number for the group
        COMMIT_FLAG (boolean):      true if this is the last group of the
                                    transaction
        LGID (8 byte unsigned int): local group identifier: this is 1 for the
                                    first group in the binary log, 2 for the
                                    next one, etc. This is like an auto_increment
                                    primary key on the binary log.
        }
*/
enum enum_group_type
{
  AUTOMATIC_GROUP= 0,
  GTID_GROUP,
  ANONYMOUS_GROUP,
  INVALID_GROUP,
  UNDEFINED_GROUP
};

/*
  G_COMMIT_TS status variable stores the logical timestamp when the transaction
  entered the commit phase. This wll be used to apply transactions in parallel
  on the slave.
 */
#define G_COMMIT_TS  1


/**
  These are flags and structs to handle all the LOAD DATA INFILE options (LINES
  TERMINATED etc).
  DUMPFILE_FLAG is probably not used (DUMPFILE is a clause of SELECT,
  not of LOAD DATA).
*/
#define DUMPFILE_FLAG           0x1
#define OPT_ENCLOSED_FLAG       0x2
#define REPLACE_FLAG            0x4
#define IGNORE_FLAG             0x8

#define FIELD_TERM_EMPTY        0x1
#define ENCLOSED_EMPTY          0x2
#define LINE_TERM_EMPTY         0x4
#define LINE_START_EMPTY        0x8
#define ESCAPED_EMPTY           0x10

/**
  @struct old_sql_ex

  This structure holds the single character field/line options in the
  LOAD_DATA_INFILE statement. It is used for server versions prior to
  5.0.3, where a single character separator was supported for
  LOAD_DATA_INFILE statements.

  The structure contains the foloowing components.
  <table>
  <caption>old_sql_ex members for Load_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>field_term</td>
    <td>a single character</td>
    <td>field terminating character spcified by the subclause
        'FIELDS TERMINATED BY'
    </td>
  </tr>

  <tr>
    <td>enclosed</td>
    <td>a single character</td>
    <td>character used for enclosing field data, specified by
        the subclause 'FIELDS ENCLOSED BY'
    </td>
  </tr>

  <tr>
    <td>line_term</td>
    <td>a single character</td>
    <td>line terminating character, specified by the subclause
        'LINES TERMINATED BY'
    </td>
  </tr>

  <tr>
    <td>line_start</td>
    <td>a single character</td>
    <td>character indicating the start of a line, specified by
       the subclause 'LINES STARTING BY'
    </td>
  </tr>

  <tr>
    <td>escaped</td>
    <td>a single character</td>
    <td>escape character for a field, specified by the subclause
       'FIELD ESCAPED BY'
    </td>
  </tr>

  <tr>
    <td>opt_flags</td>
    <td>8 bit bitfield value </td>
    <td>bitfield indicating the presence of the keywords REPLACE,
        IGNORE, and OPTIONALLY
    </td>
  </tr>

  <tr>
    <td>empty_flags</td>
    <td>8 bit bitfield value </td>
    <td>The low 5 bits of empty_flags indicate which of the five strings
        have length 0.  For each of the following flags that is set, the
        corresponding string has length 0; for the flags that are not set,
        the string has length 1: FIELD_TERM_EMPTY==0x1,
        ENCLOSED_EMPTY==0x2, LINE_TERM_EMPTY==0x4, LINE_START_EMPTY==0x8,
        ESCAPED_EMPTY==0x10.
    </td>
  </tr>
*/
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

/**
  Constants used by Query_event.
*/

/**
   The maximum number of updated databases that a status of
   Query-log-event can carry.  It can be redefined within a range
   [1.. OVER_MAX_DBS_IN_EVENT_MTS].
*/
#define MAX_DBS_IN_EVENT_MTS 16

/**
   When the actual number of databases exceeds MAX_DBS_IN_EVENT_MTS
   the value of OVER_MAX_DBS_IN_EVENT_MTS is is put into the
   mts_accessed_dbs status.
*/
#define OVER_MAX_DBS_IN_EVENT_MTS 254

/**
  size of prepare and commit sequence numbers in the status vars in bytes
*/
#define COMMIT_SEQ_LEN  8


/**
  Max number of possible extra bytes in a replication event compared to a
  packet (i.e. a query) sent from client to master;
  First, an auxiliary log_event status vars estimation:
*/
#define MAX_SIZE_LOG_EVENT_STATUS (1U + 4          /* type, flags2 */   + \
                                   1U + 8          /* type, sql_mode */ + \
                                   1U + 1 + 255    /* type, length, catalog */ + \
                                   1U + 4          /* type, auto_increment */ + \
                                   1U + 6          /* type, charset */ + \
                                   1U + 1 + 255    /* type, length, time_zone */ + \
                                   1U + 2          /* type, lc_time_names_number */ + \
                                   1U + 2          /* type, charset_database_number */ + \
                                   1U + 8          /* type, table_map_for_update */ + \
                                   1U + 4          /* type, master_data_written */ + \
                                                   /* type, db_1, db_2, ... */  \
                                   1U + (MAX_DBS_IN_EVENT_MTS * (1 + NAME_LEN)) + \
                                   3U +            /* type, microseconds */ + \
                                   1U + 16 + 1 + 60/* type, user_len, user, host_len, host */)


#define SEQ_UNINIT -1

namespace binary_log
{
/**
  Reads string from buf.

  Reads str from buf in the following format:
   1. Read length stored on buf first index, as it only has 1 byte values
      bigger than 255 where lost.
   2. Set str pointer to buf second index.
  Despite str contains the complete stored string, when it is read until
  len its value will be truncated if original length was bigger than 255.

  @param buf source pointer
  @param buf_end
  @param str destination pointer
  @param len length to which the buffer should be read

  @return 1 error
          0 success
*/
static inline int read_str_at_most_255_bytes(const char **buf,
                                             const char *buf_end,
                                             const char **str,
                                             uint8_t *len)
{
  if (*buf + ((unsigned int) (unsigned char) **buf) >= buf_end)
    return 1;
  *len= (unsigned char) **buf;
  *str= (*buf) + 1;
  (*buf)+= (unsigned int)*len + 1;
  return 0;
}

/**
   This flag only makes sense for Format_description_event. It is set
   when the event is written, and *reset* when a binlog file is
   closed (yes, it's the only case when MySQL modifies an already written
   part of the binlog).  Thus it is a reliable indicator that the binlog was
   closed correctly.  (Stop_log_event is not enough, there's always a
   small chance that mysqld crashes in the middle of insert and end of
   the binlog would look like a Stop_log_event).

   This flag is used to detect a restart after a crash, and to provide
   "unbreakable" binlog. The problem is that on a crash storage engines
   rollback automatically, while binlog does not.  To solve this we use this
   flag and automatically append ROLLBACK to every non-closed binlog (append
   virtually, on reading, file itself is not changed). If this flag is found,
   mysqlbinlog simply prints "ROLLBACK". Replication master does not abort on
   binlog corruption, but takes it as EOF, and replication slave forces a
   rollback in this case.

   Note, that old binlogs does not have this flag set, so we get a
   a backward-compatible behaviour.
*/

#define LOG_EVENT_BINLOG_IN_USE_F       0x1

/**
  @enum Log_event_type

  Enumeration type for the different types of log events.
*/
enum Log_event_type
{
  /**
    Every time you update this enum (when you add a type), you have to
    fix Format_description_event::Format_description_event().
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
  /**
    NEW_LOAD_EVENT is like LOAD_EVENT except that it has a longer
    sql_ex, allowing multibyte TERMINATED BY etc; both types share the
    same class (Load_event)
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
   These event numbers are used from 5.1.16 until mysql-trunk-xx
   */
  WRITE_ROWS_EVENT_V1 = 23,
  UPDATE_ROWS_EVENT_V1 = 24,
  DELETE_ROWS_EVENT_V1 = 25,

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
    In some situations, it is necessary to send over ignorable
    data to the slave: data that a slave can handle in case there
    is code for handling it, but which can be ignored if it is not
    recognized.
  */
  IGNORABLE_LOG_EVENT= 28,
  ROWS_QUERY_LOG_EVENT= 29,

  /* Version 2 of the Row events */
  WRITE_ROWS_EVENT = 30,
  UPDATE_ROWS_EVENT = 31,
  DELETE_ROWS_EVENT = 32,

  GTID_LOG_EVENT= 33,
  ANONYMOUS_GTID_LOG_EVENT= 34,

  PREVIOUS_GTIDS_LOG_EVENT= 35,

  /*
   * A user defined event
   */
  USER_DEFINED_EVENT= 36,
  /*
    Add new events here - right above this comment!
    Existing events (except ENUM_END_EVENT) should never change their numbers
  */
  ENUM_END_EVENT /* end marker */
};

/**
 We could have used SERVER_VERSION_LENGTH, but this introduces an
 obscure dependency - if somebody decided to change SERVER_VERSION_LENGTH
 this would break the replication protocol
 both of these are used to initialize the array server_version
 SERVER_VERSION_LENGTH is used for global array server_version
 and ST_SERVER_VER_LEN for the Start_event_v3 member server_version
*/
#define ST_SERVER_VER_LEN 50
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

#define LOG_EVENT_HEADER_LEN 19U    /* the fixed header length */
#define OLD_HEADER_LEN       13U    /* the fixed header length in 3.23 */
/*
   Fixed header length, where 4.x and 5.0 agree. That is, 5.0 may have a longer
   header (it will for sure when we have the unique event's ID), but at least
   the first 19 bytes are the same in 4.x and 5.0. So when we have the unique
   event's ID, LOG_EVENT_HEADER_LEN will be something like 26, but
   LOG_EVENT_MINIMAL_HEADER_LEN will remain 19.
*/
#define LOG_EVENT_MINIMAL_HEADER_LEN 19U

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
 * Convenience function to get the string representation of a binlog event.
 */
const char* get_event_type_str(Log_event_type type);
/*
  Calculate a long checksum for a memoryblock.

  SYNOPSIS
    checksum_crc32()
      crc       start value for crc
      pos       pointer to memory block
      length    length of the block
*/

inline uint32_t checksum_crc32(uint32_t crc, const unsigned char *pos, size_t length)
{
  return (uint32_t)crc32(crc, pos, length);
}
/*
  This method copies the string pointed to by src (including
  the terminating null byte ('\0')) to the array pointed to by dest.
  The strings may not overlap, and the destination string dest must be
  large enough to receive the copy.

  @param src  the source string
  @param dest the desctination string

  @return     pointer to the end of the string dest
*/
char *bapi_stpcpy(char *dst, const char *src);
/**
  bapi_strmake(dest,src,length) moves length characters, or until end, of src to
  dest and appends a closing NUL to dest.
  Note that if strlen(src) >= length then dest[length] will be set to \0
  bapi_strmake() returns pointer to closing null
*/
char *bapi_strmake(char *dst, const char *src, size_t length);

#define LOG_EVENT_HEADER_SIZE 20

/**
  @struct sql_ex_data_info

  This structure holds the multi character field/line options in the
  LOAD_DATA_INFILE statement. It is used for server versions newer than
  5.0.3, where multicharacter separators were supported for
  LOAD_DATA_INFILE SQL Query.

  The structure is simlar to old_sql_ex defined above.
  The new and old format differ in the way the five strings indicating the
  terminating characters in the query are stored.

  To know more, read comments in the class Load_event and struct
  old_sql_ex.
*/
struct sql_ex_data_info
{
  sql_ex_data_info() {}                            /* Remove gcc warning */
  const char* field_term;
  const char* enclosed;
  const char* line_term;
  const char* line_start;
  const char* escaped;
  uint8_t field_term_len, enclosed_len, line_term_len,
          line_start_len, escaped_len;

  char opt_flags;
  char empty_flags;

  int cached_new_format;

  /* store in new format even if old is possible */
  void force_new_format() { cached_new_format = 1;}
  int data_size()
  {
    return (new_format() ?
            field_term_len + enclosed_len + line_term_len +
            line_start_len + escaped_len + 6 : 7);
  }
  const char* init(const char* buf, const char* buf_end, bool use_new_format);
  bool new_format()
  {
    return ((cached_new_format != -1) ? cached_new_format :
            (cached_new_format= (field_term_len > 1 ||
                                 enclosed_len > 1 ||
                                 line_term_len > 1 || line_start_len > 1 ||
                                 escaped_len > 1)));
  }
};

/**
  Forward declaration of Format_description_event class to be used in class
  Log_event_header
*/
class Format_description_event;

/**
  @class Log_event_footer

  The footer, in the current version of the MySQL server, only contains
  the checksum algorithm descriptor. The descriptor is contained in the
  FDE of the binary log. This is common for all the events contained in
  that binary log, and defines the algorithm used to checksum
  the events contained in the binlog.

 @note checksum *value* is not stored in the event. On master's side, it
       is calculated before writing into the binary log, depending on the
       updated event data. On the slave, the checksum value is retrieved
       from a particular offset and checked for corruption, by computing
       a new value. It is not required after that. Therefore, it is not
       required to store the value in the instance as a class member.
*/
class Log_event_footer
{
public:

  enum_binlog_checksum_alg
  static get_checksum_alg(const char* buf, unsigned long len);

  bool static event_checksum_test(unsigned char* buf,
                                  unsigned long event_len,
                                  enum_binlog_checksum_alg alg);

  /* Constructors */
  Log_event_footer() : checksum_alg(BINLOG_CHECKSUM_ALG_UNDEF)
  {
  }

  Log_event_footer(enum_binlog_checksum_alg checksum_alg_arg)
  : checksum_alg(checksum_alg_arg)
  {
  }

  /**
     Master side:
     The value is set by caller of FD(Format Description) constructor
     In the FD case it's propagated into the last byte
     of post_header_len[].
     Slave side:
     On the slave side the value is assigned from post_header_len[last]
     of the last seen FD event.
     TODO: Revisit this comment when encoder is moved in libbinlogapi
  */
  enum_binlog_checksum_alg checksum_alg;
};

class Log_event_header
{
public:
  /**
    Timestamp on the master(for debugging and replication of
    NOW()/TIMESTAMP).  It is important for queries and LOAD DATA
    INFILE. This is set at the event's creation time, except for Query
    and Load (and other events) events where this is set at the query's
    execution time, which guarantees good replication (otherwise, we
    could have a query and its event with different timestamps).
  */
  struct timeval when;

  /**
    The server id read from the Binlog.
  */
  unsigned int unmasked_server_id;

  /* Length of an event, which will be written by write() function */
  unsigned long data_written;

  /**
    The offset in the log where this event originally appeared (it is
    preserved in relay logs, making SHOW SLAVE STATUS able to print
    coordinates of the event in the master's binlog). Note: when a
    transaction is written by the master to its binlog (wrapped in
    BEGIN/COMMIT) the log_pos of all the queries it contains is the
    one of the BEGIN (this way, when one does SHOW SLAVE STATUS it
    sees the offset of the BEGIN, which is logical as rollback may
    occur), except the COMMIT query which has its real offset.
  */
  unsigned long long log_pos;

  /**
    16 or less flags depending on the version of the binary log.
    See the definitions above for LOG_EVENT_TIME_F,
    LOG_EVENT_FORCED_ROTATE_F, LOG_EVENT_THREAD_SPECIFIC_F, and
    LOG_EVENT_SUPPRESS_USE_F for notes.
  */
  uint16_t flags;

  /**
    The following type definition is to be used whenever data is placed
    and manipulated in a common buffer. Use this typedef for buffers
    that contain data containing binary and character data.
  */
  typedef unsigned char Byte;

  /**
    Event type extracted from the header. In the server, it is decoded
    by read_log_event(), but adding here for complete decoding.
  */
  Log_event_type  type_code;
  Log_event_header():log_pos(0), flags(0)
  {
    when.tv_sec= 0;
    when.tv_usec= 0;
  }
  Log_event_header(const char* buf,
                   const Format_description_event *description_event);

  ~Log_event_header() {}
};

/**
 * TODO Base class for events. Implementation is in body()
 */
class Binary_log_event
{
public:

  /*
     The number of types we handle in Format_description_log_event (UNKNOWN_EVENT
     is not to be handled, it does not exist in binlogs, it does not have a
     format).
  */
  static const int LOG_EVENT_TYPES= (ENUM_END_EVENT - 2);

  /**
    The lengths for the fixed data part of each event.
    This is an enum that provides post-header lengths for all events.
  */
  enum enum_post_header_length{
    // where 3.23, 4.x and 5.0 agree
    QUERY_HEADER_MINIMAL_LEN= (4 + 4 + 1 + 2),
    // where 5.0 differs: 2 for length of N-bytes vars.
    QUERY_HEADER_LEN=(QUERY_HEADER_MINIMAL_LEN + 2),
    STOP_HEADER_LEN= 0,
    LOAD_HEADER_LEN= (4 + 4 + 4 + 1 +1 + 4),
    START_V3_HEADER_LEN= (2 + ST_SERVER_VER_LEN + 4),
    // this is FROZEN (the Rotate post-header is frozen)
    ROTATE_HEADER_LEN= 8,
    INTVAR_HEADER_LEN= 0,
    CREATE_FILE_HEADER_LEN= 4,
    APPEND_BLOCK_HEADER_LEN= 4,
    EXEC_LOAD_HEADER_LEN= 4,
    DELETE_FILE_HEADER_LEN= 4,
    NEW_LOAD_HEADER_LEN= LOAD_HEADER_LEN,
    RAND_HEADER_LEN= 0,
    USER_VAR_HEADER_LEN= 0,
    FORMAT_DESCRIPTION_HEADER_LEN= (START_V3_HEADER_LEN + 1 + LOG_EVENT_TYPES),
    XID_HEADER_LEN= 0,
    BEGIN_LOAD_QUERY_HEADER_LEN= APPEND_BLOCK_HEADER_LEN,
    ROWS_HEADER_LEN_V1= 8,
    TABLE_MAP_HEADER_LEN= 8,
    EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN= (4 + 4 + 4 + 1),
    EXECUTE_LOAD_QUERY_HEADER_LEN= (QUERY_HEADER_LEN +\
                                    EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN),
    INCIDENT_HEADER_LEN= 2,
    HEARTBEAT_HEADER_LEN= 0,
    IGNORABLE_HEADER_LEN= 0,
    ROWS_HEADER_LEN_V2= 10
  }; // end enum_post_header_length

  Binary_log_event()
  {
      /*
        An event length of 0 indicates that the header isn't initialized
       */
      //m_header.event_length= 0;
      //m_header.type_code=    0;
  }

  Binary_log_event(const char **buf, uint16_t binlog_version,
                   const char *server_version);
  // TODO: Uncomment when the dependency of log_event on this class in removed
  /**
    Returns short information about the event
  */
  //virtual void print_event_info(std::ostream& info)=0;
  /**
    Returns detailed information about the event
  */
  //virtual void print_long_info(std::ostream& info);
  virtual ~Binary_log_event() {};

  /**
   * Helper method
   */
  enum Log_event_type get_event_type() const
  {
    return (enum Log_event_type) m_header.type_code;
  }

  virtual Log_event_type get_type_code()= 0;
  virtual bool is_valid() const= 0;
  /**
   * Return a pointer to the header of the log event
   */
  Log_event_header *header() { return &m_header; }
  /**
   * Return a pointer to the footer of the log event
   */
  Log_event_footer *footer() { return &m_footer; }

private:
  Log_event_header m_header;
  Log_event_footer m_footer;
};


/**
  @class Query_event

  A @c Query_event is created for each query that modifies the
  database, unless the query is logged row-based.

  @section Query_event_binary_format Binary format

  //TODO: Add Documentation Binary format for the events
  See @ref Binary_log_event_binary_format "Binary format for log events" for
  a general discussion and introduction to the binary format of binlog
  events.

  The Post-Header has five components:

  <table>
  <caption>Post-Header for Query_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>thread_id</td>
    <td>4 byte unsigned integer</td>
    <td>The ID of the thread that issued this statement. It is needed for
        temporary tables.</td>
  </tr>

  <tr>
    <td>query_exec_time</td>
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
    <td>Error code generated by the master. If the master fails, the
    slave will fail with the same error code.
    </td>
  </tr>

  <tr>
    <td>status_vars_len</td>
    <td>2 byte unsigned integer</td>
    <td>The length of the status_vars block of the Body, in bytes. This is not
        present for binlog version 1 and 3. See
    @ref Query_event_status_vars "below".
    </td>
  </tr>
  </table>

  The Body has the following components:

  <table>
  <caption>Body for Query_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>@anchor Query_event_status_vars status_vars</td>
    <td>status_vars_len bytes</td>
    <td>Zero or more status variables.  Each status variable consists
    of one byte identifying the variable stored, followed by the value
    of the variable.  The possible variables are listed separately in
    the table @ref Table_query_event_status_vars "below".  MySQL
    always writes events in the order defined below; however, it is
    capable of reading them in any order.  </td>
  </tr>

  <tr>
    <td>m_db</td>
    <td>db_len + 1</td>
    <td>The currently selected database, as a null-terminated string.

    (The trailing zero is redundant since the length is already known;
    it is db_len from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>m_query</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>The SQL query.</td>
  </tr>
  </table>

  The following table lists the status variables that may appear in
  the status_vars field.

  @anchor Table_query_event_status_vars
  <table>
  <caption>Status variables for Query_event</caption>

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
    MODE_NOT_USED==0x10
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
    MODE_NO_DIR_IN_CREATE.

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
  <tr>
    <td>master_data_written</td>
    <td>Q_MASTER_DATA_WRITTEN_CODE == 10</td>
    <td>4 byte bitfield</td>

    <td>The value of the original length of a Query_event that comes from a
    master. Master's event is relay-logged with storing the original size of
    event in this field by the IO thread. The size is to be restored by reading
    Q_MASTER_DATA_WRITTEN_CODE-marked event from the relay log.

    This field is not written to slave's server binlog by the SQL thread.
    This field only exists in relay logs where master has binlog_version<4 i.e.
    server_version < 5.0 and the slave has binlog_version=4.
    </td>
  </tr>
  <tr>
    <td>binlog_invoker</td>
    <td>Q_INVOKER == 11</td>
    <td>2 Variable-length strings: the length in bytes (1 byte) followed
    by characters (user), again followed by length in bytes (1 byte) followed
    by characters(host)</td>

    <td>The value of boolean variable m_binlog_invoker is set TRUE if
    CURRENT_USER() is called in account management statements. SQL thread
    uses it as a default definer in CREATE/ALTER SP, SF, Event, TRIGGER or
    VIEW statements.

    The field Q_INVOKER has length of user stored in 1 byte followed by the
    user string which is assigned to 'user' and the length of host stored in
    1 byte followed by host string which is assigned to 'host'.
    </td>
  </tr>
  <tr>
    <td>mts_accessed_dbs</td>
    <td>Q_UPDATED_DB_NAMES == 12</td>
    <td>1 byte character, and a 2-D array, which  stores the total number
        and the names to of the databases accessed, be propagated to the
        slave in order to facilitate the parallel applying of the Query events.
    </td>
  </tr>
  <tr>
    <td>commit_seq_no</td>
    <td>Q_COMMIT_TS</td>
    <td>8 byte integer, stores the logical timestamp when the transaction
        entered the commit phase. This wll be used to apply transactions
        in parallel on the slave.  </td>
  </tr>
  </table>

  @subsection Query_event_notes_on_previous_versions Notes on Previous Versions

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

class Query_event: public Binary_log_event
{
public:
  /* query event post-header */
  enum Query_event_post_header_offset{
    Q_THREAD_ID_OFFSET= 0,
    Q_EXEC_TIME_OFFSET= 4,
    Q_DB_LEN_OFFSET= 8,
    Q_ERR_CODE_OFFSET= 9,
    Q_STATUS_VARS_LEN_OFFSET= 11,
    Q_DATA_OFFSET= QUERY_HEADER_LEN };

  /* these are codes, not offsets; not more than 256 values (1 byte). */
  enum Query_event_status_vars
  {
    Q_FLAGS2_CODE= 0,
    Q_SQL_MODE_CODE,
    /*
      Q_CATALOG_CODE is catalog with end zero stored; it is used only by MySQL
      5.0.x where 0<=x<=3. We have to keep it to be able to replicate these
      old masters.
    */
    Q_CATALOG_CODE,
    Q_AUTO_INCREMENT,
    Q_CHARSET_CODE,
    Q_TIME_ZONE_CODE,
    /*
      Q_CATALOG_NZ_CODE is catalog withOUT end zero stored; it is used by MySQL
      5.0.x where x>=4. Saves one byte in every Query_event in binlog,
      compared to Q_CATALOG_CODE. The reason we didn't simply re-use
      Q_CATALOG_CODE is that then a 5.0.3 slave of this 5.0.x (x>=4)
      master would crash (segfault etc) because it would expect a 0 when there
      is none.
    */
    Q_CATALOG_NZ_CODE,
    Q_LC_TIME_NAMES_CODE,
    Q_CHARSET_DATABASE_CODE,
    Q_TABLE_MAP_FOR_UPDATE_CODE,
    Q_MASTER_DATA_WRITTEN_CODE,
    Q_INVOKER,
    /*
      Q_UPDATED_DB_NAMES status variable collects information of accessed
      databases i.e. the total number and the names to be propagated to the
      slave in order to facilitate the parallel applying of the Query events.
    */
    Q_UPDATED_DB_NAMES,
    Q_MICROSECONDS,
    /**
      Q_COMMIT_TS status variable stores the logical timestamp when the
      transaction entered the commit phase. This wll be used to apply
      transactions in parallel on the slave.
   */
   Q_COMMIT_TS
  };

private:
  std::string m_user;
  std::string m_host;
  std::string m_catalog;
  std::string m_time_zone_str;
  std::string m_db;
  std::string m_query;

protected:
  /* Required by the MySQL server class Log_event::Query_event */
  unsigned long data_len;
  /* Pointer to the end of the buffer shown below */
  unsigned long query_data_written;
  /*
    Copies data into the buffer in the following fashion
    +--------+-----------+------+------+---------+----+-------+----+
    | catlog | time_zone | user | host | db name | \0 | Query | \0 |
    +--------+-----------+------+------+---------+----+-------+----+
  */
  int fill_data_buf(unsigned char* dest);
  static char const *code_name(int code);

public:
  /* data members defined in order they are packed and written into the log */
  uint32_t thread_id;
  uint32_t query_exec_time;
  uint32_t db_len;
  uint16_t error_code;

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
    Query_event, so automatically benefit from the work already done for
    status variables in Query_event.
 */
  uint16_t status_vars_len;
  /*
    If we already know the length of the query string
    we pass it with q_len, so we would not have to call strlen()
    otherwise, set it to 0, in which case, we compute it with strlen()
  */
  uint32_t q_len;

  /* The members below represent the status variable block */

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

  uint32_t flags2;
  /* In connections sql_mode is 32 bits now but will be 64 bits soon */
  uint64_t sql_mode;
  uint16_t auto_increment_increment, auto_increment_offset;
  char charset[6];
  unsigned int time_zone_len; /* 0 means uninited */
  /*
    Binlog format 3 and 4 start to differ (as far as class members are
    concerned) from here.
  */
  unsigned int catalog_len;                    // <= 255 char; 0 means uninited
  uint16_t lc_time_names_number; /* 0 means en_US */
  uint16_t charset_database_number;
  /*
    map for tables that will be updated for a multi-table update query
    statement, for other query statements, this will be zero.
  */
  uint64_t table_map_for_update;
  /*
    Holds the original length of a Query_event that comes from a
    master of version < 5.0 (i.e., binlog_version < 4). When the IO
    thread writes the relay log, it augments the Query_event with a
    Q_MASTER_DATA_WRITTEN_CODE status_var that holds the original event
    length. This field is initialized to non-zero in the SQL thread when
    it reads this augmented event. SQL thread does not write
    Q_MASTER_DATA_WRITTEN_CODE to the slave's server binlog.
  */
  uint32_t master_data_written;

  /*
    number of updated databases by the query and their names. This info
    is requested by both Coordinator and Worker.
  */
  unsigned char mts_accessed_dbs;
  char mts_accessed_db_names[MAX_DBS_IN_EVENT_MTS][NAME_LEN];

  /**
    Prepare and commit sequence number. will be set to 0 if the event is not a
    transaction starter.
  */
  int64_t commit_seq_no;

  Query_event(const char* query_arg, const char* catalog_arg,
              const char* db_arg, uint32_t query_length,
              unsigned long thread_id_arg,
              unsigned long long sql_mode_arg,
              unsigned long auto_increment_increment_arg,
              unsigned long auto_increment_offset_arg,
              unsigned int number,
              unsigned long long table_map_for_update_arg,
              int errcode);

  Query_event(const char* buf, unsigned int event_len,
              const Format_description_event *description_event,
              Log_event_type event_type);
  Query_event();
  virtual ~Query_event()
  {
  }

  Log_event_type get_type_code() { return QUERY_EVENT;}
  bool is_valid() const {  return !m_query.empty(); }


  /*
    Define getters and setters for the string members
  */
  void set_user(const std::string &s) {m_user= s; }
  std::string get_user() const { return m_user;}
  void set_host(const std::string &s) {m_host= s; }
  std::string get_host() const { return m_host;}
  void set_time_zone_str(const std::string &s)
  {
    m_time_zone_str= s;
    time_zone_len= m_time_zone_str.length();
  }
  std::string get_time_zone_str() const { return m_time_zone_str;}
  void set_catalog(const std::string &s)
  {
    m_catalog= s;
    catalog_len= m_catalog.length();
  }
  std::string get_catalog() const { return m_catalog;}
  void set_db(const std::string &s) {m_db= s; db_len= m_db.length();}
  std::string get_db() const { return m_db;}
  void set_query(const std::string &s) {m_query= s; q_len= m_query.length();}
  std::string get_query() const { return m_query;}


  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
};


/*
  Elements of this enum describe how LOAD DATA handles duplicates.
*/
enum enum_load_dup_handling { LOAD_DUP_ERROR= 0, LOAD_DUP_IGNORE,
                              LOAD_DUP_REPLACE };
/**
  @class Execute_load_query_event

  Event responsible for LOAD DATA execution, it similar to Query_log_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

  The first 13 bytes of the Post-Header for this event are the same as for
  Query_event, as is the initial status variable block in the Body.
  The additional members of the events are the following:

   <table>
   <caption>Body for Execute_load_query_event</caption>

   <tr>
     <th>Name</th>
     <th>Format</th>
     <th>Description</th>
   </tr>

   <tr>
     <td>file_id</td>
     <td>4 byte unsigned integer</td>
     <td>ID of the temporary file to load</td>
   </tr>

   <tr>
     <td>fn_pos_start</td>
     <td>4 byte unsigned integer</td>
     <td>The start position within the statement for filename substitution</td>
   </tr>
   <tr>

     <td>fn_pos_end</td>
     <td>4 byte unsigned integer</td>
     <td>The end position within the statement for filename substitution</td>
   </tr>

   <tr>
     <td>dup_handling</td>
     <td>enum_load_dup_handling</td>
     <td>Represents information on how to handle duplicates:
          LOAD_DUP_ERROR= 0, LOAD_DUP_IGNORE= 1, LOAD_DUP_REPLACE= 2</td>
   </tr>
  @section Execute_load_query_event_binary_format Binary Format
*/
class Execute_load_query_event : public virtual Query_event
{
public:
  enum Execute_load_query_event_offset{
   /* ELQ = "Execute Load Query" */
   ELQ_FILE_ID_OFFSET= QUERY_HEADER_LEN,
   ELQ_FN_POS_START_OFFSET= ELQ_FILE_ID_OFFSET + 4,
   ELQ_FN_POS_END_OFFSET= ELQ_FILE_ID_OFFSET + 8,
   ELQ_DUP_HANDLING_OFFSET= ELQ_FILE_ID_OFFSET + 12
  };

  uint32_t file_id;       // file_id of temporary file
  uint32_t fn_pos_start;  // pointer to the part of the query that should
                          // be substituted
  uint32_t fn_pos_end;    // pointer to the end of this part of query
  /*
    We have to store type of duplicate handling explicitly, because
    for LOAD DATA it also depends on LOCAL option. And this part
    of query will be rewritten during replication so this information
    may be lost...
  */
  enum_load_dup_handling dup_handling;

  Execute_load_query_event() {} //TODO: required by the (THD* arg cons...)
  Execute_load_query_event(const char* buf, unsigned int event_len,
                           const Format_description_event *description_event);
  ~Execute_load_query_event() {}

  Log_event_type get_type_code() { return EXECUTE_LOAD_QUERY_EVENT; }
  bool is_valid() const { return Query_event::is_valid() && file_id != 0; }
};


/**
  @class Rotate_event

  When a binary log file exceeds a size limit, a ROTATE_EVENT is written
  at the end of the file that points to the next file in the squence.
  This event is information for the slave to know the name of the next
  binary log it is going to receive.

  ROTATE_EVENT is generated locally and written to the binary log
  on the master. It is written to the relay log on the slave when FLUSH LOGS
  occurs, and when receiving a ROTATE_EVENT from the master.
  In the latter case, there will be two rotate events in total originating
  on different servers.

  @section Rotate_event_binary_format Binary Format

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
    <td>The position within the binary log to rotate to.</td>
  </tr>

  </table>

  The Body has one component:

  <table>
  <caption>Body for Rotate_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>new_log_ident</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>Name of the binlog to rotate to.</td>
  </tr>

  </table>
*/
class Rotate_event: public Binary_log_event
{
public:
  const char* new_log_ident;
  unsigned int ident_len;
  unsigned int flags;
  uint64_t pos;

  enum {
    /* Values taken by the flag member variable */
    DUP_NAME= 2, // if constructor should dup the string argument
    RELAY_LOG= 4 // rotate event for the relay log
  };

  enum {
    /* Rotate event post_header */
    R_POS_OFFSET= 0,
    R_IDENT_OFFSET= 8
  };

  Rotate_event() {}
  Rotate_event(const char* buf, unsigned int event_len,
               const Format_description_event *description_event);

  Log_event_type get_type_code() { return ROTATE_EVENT; }
  //TODO: is_valid() is to be handled as a separate patch
  bool is_valid() const { return new_log_ident != 0; }

  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);

  ~Rotate_event()
  {
    if (flags & DUP_NAME)
      bapi_free((void*) new_log_ident);
  }
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
  The Post-Header has four components:

  <table>
  <caption>Post-Header for Start_event_v3</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>created</td>
    <td>4 byte unsigned integer</td>
    <td>The creation timestamp, if non-zero,
        is the time in seconds when this event was created</td>
  </tr>
  <tr>
    <td>binlog_version</td>
    <td>2 byte unsigned integer</td>
    <td>This is 1 in MySQL 3.23 and 3 in MySQL 4.0 and 4.1
        (In MySQL 5.0 and up, FORMAT_DESCRIPTION_EVENT is
        used instead of START_EVENT_V3 and for them its 4).</td>
  </tr>
  <tr>
    <td>server_version</td>
    <td>char array of 50 bytes</td>
    <td>The MySQL server's version (example: 4.0.14-debug-log),
        padded with 0x00 bytes on the right</td>
  </tr>
  <tr>
    <td>dont_set_created</td>
    <td>type bool</td>
    <td>Set to 1 when you dont want to have created time in the log</td>
  </table>
  @section Start_log_event_v3_binary_format Binary Format
*/

class Start_event_v3: public Binary_log_event
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
  uint16_t binlog_version;
  char server_version[ST_SERVER_VER_LEN];
   /*
    We set this to 1 if we don't want to have the created time in the log,
    which is the case when we rollover to a new log.
  */
  bool dont_set_created;
  Log_event_type get_type_code() { return START_EVENT_V3;}
  protected:
  /**
     The constructor below will be  used only by Format_description_event
     constructor
  */
  Start_event_v3();
  public:
  Start_event_v3(const char* buf,
                 const Format_description_event* description_event);
  //TODO: Add definition for them
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
};

/**
  @class Format_description_evenit
  For binlog version 4.
  This event is saved by threads which read it, as they need it for future
  use (to decode the ordinary events).

  The Post-Header has six components:

  <table>
  <caption>Post-Header for Format_description_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>created_ts</td>
    <td>4 byte unsigned integer</td>
    <td>The creation timestamp, if non-zero,
        is the time in seconds when this event was created</td>
  </tr>

  <tr>
    <td>common_header_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of the event header. This value includes the extra_headers
        field, so this header length - 19 yields the size
        of the extra_headers field.</td>
  </tr>
  <tr>
    <td>post_header_len</td>
    <td>array of type 1 byte unsigned integer</td>
    <td>The lengths for the fixed data part of each event</td>
  </tr>
  <tr>
    <td>server_version_split</td>
    <td>unsigned char array</td>
    <td>Stores the server version of the server
        and splits them in three parts</td>
  </tr>
  <tr>
    <td>event_type_permutation</td>
    <td>const array of type 1 byte unsigned integer</td>
    <td>Provides mapping between the event types of
        some previous versions > 5.1 GA to current event_types</td>
  </tr>
    <tr>
    <td>number_of_event_types</td>
    <td>1 byte unsigned integer</td>
    <td>number of event types present in the server</td>
  </tr>
  </table>
  @section Format_description_event_binary_format Binary Format
*/
class Format_description_event: public virtual Start_event_v3
{
public:
    uint32_t created_ts;
    /**
     The size of the fixed header which _all_ events have
     (for binlogs written by this version, this is equal to
     LOG_EVENT_HEADER_LEN), except FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT
     (those have a header of size LOG_EVENT_MINIMAL_HEADER_LEN).
    */
    uint8_t common_header_len;
    /*
     The list of post-headers' lengths followed
     by the checksum alg decription byte
  */
    uint8_t *post_header_len;
    unsigned char server_version_split[3];
    /**
     In some previous version > 5.1 GA event types are assigned
     different event id numbers than in the present version, so we
     must map those id's to the our current event id's. This
     mapping is done using event_type_permutation
    */
    const uint8_t *event_type_permutation;
    Format_description_event(uint8_t binlog_ver,
                             const char* server_ver);
    Format_description_event(const char* buf, unsigned int event_len,
                             const Format_description_event *description_event);

    uint8_t number_of_event_types;
    Log_event_type get_type_code() { return FORMAT_DESCRIPTION_EVENT; }
    unsigned long get_version_product() const;
    bool is_version_before_checksum() const;
    void calc_server_version_split();
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
    ~Format_description_event();

};
/**
  @class Stop_event

  A stop event is written to the log files under these circumstances:
  - A master writes the event to the binary log when it shuts down.
  - A slave writes the event to the relay log when it shuts down or
    when a RESET SLAVE statement is executed.

  @section Stop_log_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/

class Stop_event: public Binary_log_event
{
public:
  Stop_event() : Binary_log_event()
  {
  }
  Stop_event(const char* buf,
             const Format_description_event *description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version)
  {
  }

  Log_event_type get_type_code() { return STOP_EVENT; }
  bool is_valid() const { return 1; }

  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info);
};



/**
  @class Load_event

  This event corresponds to a "LOAD DATA INFILE" SQL query in the
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

  @section Load_event_binary_format Binary Format

  The Post-Header consists of the following six components.

  <table>
  <caption>Post-Header for Load_event</caption>

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
    <td>load_exec_time</td>
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
  <caption>Body of Load_event</caption>

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
    When MySQL writes a Load_event, it uses the new format if at
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
    <td>null-terminated string of length table_len + 1 bytes</td>
    <td>The 'table_name' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the table_len is
    known from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>db</td>
    <td>null-terminated string of length db_len + 1 bytes</td>
    <td>The 'db' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the db_len is known
    from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>fname</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>The 'file_name' from the query.
    </td>
  </tr>

  </table>

  @subsection Load_event_notes_on_previous_versions Notes on Previous Versions

  This event type is understood by current versions, but only
  generated by MySQL 3.23 and earlier.
*/
class Load_event: public Binary_log_event
{
protected:
 int copy_load_event(const char *buf, unsigned long event_len,
                     int body_offset,
                     const Format_description_event* description_event);
 // Required by Load_event(THD* ...) in the server
 Load_event() : num_fields(0), fields(0), field_lens(0), field_block_len(0)
 {}
public:
  enum Load_event_offset {
    L_THREAD_ID_OFFSET= 0,
    L_EXEC_TIME_OFFSET= 4,
    L_SKIP_LINES_OFFSET= 8,
    L_TBL_LEN_OFFSET= 12,
    L_DB_LEN_OFFSET= 13,
    L_NUM_FIELDS_OFFSET= 14,
    L_SQL_EX_OFFSET= 18,
    L_DATA_OFFSET= LOAD_HEADER_LEN
  };

  /**
   This is the execution time stored in the post header.
   Make sure to use it to set the exec_time variable in class Log_event
  */
  uint32_t load_exec_time;

  uint32_t slave_proxy_id;
  uint32_t table_name_len;

  /**
    No need to have a catalog, as these events can only come from 4.x.
  */
  uint32_t db_len;
  uint32_t fname_len;
  uint32_t num_fields;

  const char* fields;
  const unsigned char* field_lens;
  uint32_t field_block_len;

  const char* table_name;
  const char* db;
  const char* fname;
  uint32_t skip_lines;
  sql_ex_data_info sql_ex_data;
  bool local_fname;

  /**
    Indicates that this event corresponds to LOAD DATA CONCURRENT,

    @note Since Load_event event coming from the binary log
          lacks information whether LOAD DATA on master was concurrent
          or not, this flag is only set to TRUE for an auxiliary
          Load_event object which is used in mysql_load() to
          re-construct LOAD DATA statement from function parameters,
          for logging.
  */
  bool is_concurrent;

  /**
    Note that for all the events related to LOAD DATA (Load_event,
    Create_file/Append/Exec/Delete, we pass description_event; however as
    logging of LOAD DATA is changed, this is only used
    for the common_header_len (post_header_len is not changed).
  */
  Load_event(const char* buf, unsigned int event_len,
             const Format_description_event* description_event);
  ~Load_event()
  {}

  Log_event_type get_type_code()
  {
    return sql_ex_data.new_format() ? NEW_LOAD_EVENT: LOAD_EVENT;
  }

  virtual bool is_valid() const { return table_name != 0; }
  int get_data_size()
  {
    return (table_name_len + db_len + 2 + fname_len
            + LOAD_HEADER_LEN
            + sql_ex_data.data_size() + field_block_len + num_fields);
  }

  //TODO: Define the methods when required
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};

};

/* the classes below are for the new LOAD DATA INFILE logging */

/**
  @class Create_file_event

  The Create_file_event contains the options to LOAD DATA INFILE.
  This was a design flaw since the file cannot be loaded until the
  Exec_load_event is seen. The use of this event was deprecated from
  MySQL server version 5.0.3 and above.

  @section Create_file_event_binary_format Binary Format
*/

class Create_file_event: public virtual Load_event
{
protected:
  /**
    Pretend we are Load event, so we can write out just
    our Load part - used on the slave when writing event out to
    SQL_LOAD-*.info file
  */
  bool fake_base;
public:
  enum Create_file_offset {
   /* CF = "Create File" */
   CF_FILE_ID_OFFSET= 0,
   CF_DATA_OFFSET= CREATE_FILE_HEADER_LEN
  };

  unsigned char* block;
  const char *event_buf;
  unsigned int block_len;
  unsigned int file_id;
  bool inited_from_old;

  Create_file_event(const char* buf, unsigned int event_len,
                    const Format_description_event* description_event);

  Create_file_event(unsigned char* block_arg,
                    unsigned int  block_len_arg, unsigned int file_id_arg);
  ~Create_file_event()
  {
    bapi_free((void*) event_buf);
  }
  Log_event_type get_type_code()
  {
    return fake_base ? Load_event::get_type_code() : CREATE_FILE_EVENT;
  }
  int get_data_size()
  {
    return (fake_base ? Load_event::get_data_size() :
            Load_event::get_data_size() +
            4 + 1 + block_len);
  }

  bool is_valid() const { return inited_from_old || block != 0; }

};

/**
  @class Delete_file_event

  DELETE_FILE_EVENT occurs when the LOAD DATA failed on the master.
  This event notifies the slave not to do the load and to delete
  the temporary file.

  @section Delete_file_event_binary_format Binary Format
*/
class Delete_file_event: public Binary_log_event
{
protected:
  // Required by Delete_file_log_event(THD* ..)
  Delete_file_event(uint32_t file_id_arg, const char* db_arg)
  : file_id(file_id_arg), db(db_arg)
 {
 }
public:
  enum Delete_file_offset {
    /* DF = "Delete File" */
    DF_FILE_ID_OFFSET= 0
  };

  uint32_t file_id;
  const char* db; /* see comment in Append_block_event */

  Delete_file_event(const char* buf, uint event_len,
                    const Format_description_event* description_event);
  ~Delete_file_event() {}
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  bool is_valid() const { return file_id != 0; }
};

/**
  @class Execute_load_event

  Execute_load_event is created when the LOAD_DATA query succeeds on
  the master. The slave should be notified to load the temporary file into
  the table. For server versions > 5.0.3, the temporary files that stores
  the parameters to LOAD DATA INFILE is not needed anymore, since they are
  stored in this event. There is still a temp file containing all the data
  to be loaded.

  @section Delete_file_event_binary_format Binary Format
*/
class Execute_load_event: public Binary_log_event
{
protected:
  //TODO: Remove if not required, used by Execute_load_log_event(THD* ...)
  Execute_load_event() {}
public:
  enum Execute_load_offset {
    /* EL = "Execute Load" */
    EL_FILE_ID_OFFSET= 0
  };

  uint32_t file_id;
  const char* db; /* see comment in Append_block_event */

  Execute_load_event(const char* buf, uint event_len,
                     const Format_description_event *description_event);

  ~Execute_load_event() {}
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  bool is_valid() const { return file_id != 0; }
};

/**
  @class Append_block_event

  This event is created to contain the file data. One LOAD_DATA_INFILE
  can have 0 or more instances of this event written to the binary log
  depending on the size of the file. If the file to be loaded is greater
  than the threshold value, which is roughly 2^17 bytes, the file is
  divided into blocks of size equal to the threshold, and each block
  is sent across as a separate event.

  @section Append_block_event_binary_format Binary Format
*/

class Append_block_event: public Binary_log_event
{
protected:
  /**
    This constructor is used by the MySQL server.
  */
  Append_block_event(const char* db_arg,
                     unsigned char* block_arg,
                     unsigned int block_len_arg,
                     uint32_t file_id_arg)
 : block(block_arg), block_len(block_len_arg),
   file_id(file_id_arg), db(db_arg)
 {
 }
 Append_block_event() {}
public:
  enum Append_block_offset
  {
    /* AB = "Append Block" */
    AB_FILE_ID_OFFSET= 0,
    AB_DATA_OFFSET=  APPEND_BLOCK_HEADER_LEN
  };

  unsigned char* block;
  unsigned int block_len;
  uint32_t file_id;
  /*
    'db' is filled when the event is created in mysql_load() (the
    event needs to have a 'db' member to be well filtered by
    binlog-*-db rules). 'db' is not written to the binlog (it's not
    used by Append_block_log_event::write()), so it can't be read in
    the Append_block_log_event(const char* buf, int event_len)
    constructor.  In other words, 'db' is used only for filtering by
    binlog-*-db rules.  Create_file_log_event is different: it's 'db'
    (which is inherited from Load_event) is written to the binlog
    and can be re-read.
  */
  const char* db;

  Append_block_event(const char* buf, unsigned int event_len,
                     const Format_description_event *description_event);
  ~Append_block_event() {}
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  virtual bool is_valid() const { return block != 0; }
};

/**
  @class Begin_load_query_event

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

  @section Begin_load_query_event_binary_format Binary Format
*/
class Begin_load_query_event: public virtual Append_block_event
{
protected:
  //TODO: Remove. Right now required by Begin_load_query_log_event(THD*...)
  Begin_load_query_event(): Append_block_event() {}
public:
  Begin_load_query_event(const char* buf, unsigned int event_len,
                         const Format_description_event *description_event);
  ~Begin_load_query_event() {}
  Log_event_type get_type_code() { return BEGIN_LOAD_QUERY_EVENT; }
};


//TODO: Add comments for this class
class User_var_event: public Binary_log_event
{
public:
  enum Value_type {
    STRING_TYPE,
    REAL_TYPE,
    INT_TYPE,
    ROW_TYPE,
    DECIMAL_TYPE,
    VALUE_TYPE_COUNT
    };
  enum {
    UNDEF_F,
    UNSIGNED_F
  };
  enum User_var_event_data
  {
    UV_VAL_LEN_SIZE= 4,
    UV_VAL_IS_NULL= 1,
    UV_VAL_TYPE_SIZE= 1,
    UV_NAME_LEN_SIZE= 4,
    UV_CHARSET_NUMBER_SIZE= 4
  };
  User_var_event(const char *name_arg, unsigned int name_len_arg, char *val_arg,
                 unsigned long val_len_arg, Value_type type_arg,
                 unsigned int charset_number_arg, unsigned char flags_arg)
  {
    name= name_arg;
    name_len= name_len_arg;
    val= val_arg;
    val_len= val_len_arg;
    type=(Value_type) type_arg;
    charset_number= charset_number_arg;
    flags= flags_arg;
    is_null= !val;
  }

  User_var_event(const char* buf, unsigned int event_len,
                 const Format_description_event *description_event);
  const char *name;
  unsigned int name_len;
  char *val;
  uint32_t val_len;
  Value_type type;
  unsigned int charset_number;
  bool is_null;
  unsigned char flags;
  Log_event_type get_type_code() {return USER_VAR_EVENT; }
  bool is_valid() const { return 1; }
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
  std::string static get_value_type_string(enum Value_type type)
  {
    switch(type)
    {
      case STRING_TYPE:return "String";
      case REAL_TYPE:return "Real";
      case INT_TYPE:return "Integer";
      case ROW_TYPE:return "Row";
      case DECIMAL_TYPE:return "Decimal";
      case VALUE_TYPE_COUNT:return "Value type count";
      default:return "Unknown";
    }
  }
};

/**
  @class Ignorable_event

  Base class for ignorable log events. Events deriving from
  this class can be safely ignored by slaves that cannot
  recognize them. Newer slaves, will be able to read and
  handle them. This has been designed to be an open-ended
  architecture, so adding new derived events shall not harm
  the old slaves that support ignorable log event mechanism
  (they will just ignore unrecognized ignorable events).

  @note The only thing that makes an event ignorable is that it has
  the LOG_EVENT_IGNORABLE_F flag set.  It is not strictly necessary
  that ignorable event types derive from Ignorable_event; they may
  just as well derive from Binary_log_event and Log_event and pass
  LOG_EVENT_IGNORABLE_F as argument to the Log_event constructor.
*/

class Ignorable_event: public Binary_log_event
{
public:
  Ignorable_event(const char *buf, const Format_description_event *descr_event)
  :Binary_log_event(&buf, descr_event->binlog_version,
                    descr_event->server_version)
  { }
  Ignorable_event() { }; // For the thd ctor of Ignorable_log_event
  virtual Log_event_type get_type_code() { return IGNORABLE_LOG_EVENT; }
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
};

/**
  @class Rows_query_event

  Rows query event type, which is a subclass
  of the ignorable_event, to record the original query for the rows
  events in RBR. This event can be used to display the original query as
  comments by SHOW BINLOG EVENTS query, or mysqlbinlog client when the
  --verbose option is given twice
  @section Int_var_event_binary_format Binary Format

  The Post-Header for this event type is empty. The Body has one
  components:

  <table>
  <caption>Body for Intvar_log_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>m_rows_query</td>
    <td>char array</td>
    <td>Records the original quesry executed in RBR </td>
  </tr>
  </table>

*/
class Rows_query_event: public virtual Ignorable_event
{
public:
  Rows_query_event(const char *buf, unsigned int event_len,
                   const Format_description_event *descr_event);
  Rows_query_event()
  {
  }
  virtual ~Rows_query_event();
protected:
  char *m_rows_query;
};

/**
  @class Int_var_event

  An Intvar_log_event will be created just before a Query_event,
  if the query uses one of the variables LAST_INSERT_ID or INSERT_ID.
  Each Int_var_event holds the value of one of these variables.


  The Post-Header for this event type is empty. The Body has two
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
    two identifiers are supported: LAST_INSERT_ID_EVENT == 1 and
    INSERT_ID_EVENT == 2.
    </td>
  </tr>

  <tr>
    <td>val</td>
    <td>8 byte unsigned integer</td>
    <td>The value of the variable.</td>
  </tr>

  </table>
  @section Int_var_event_binary_format Binary Format
*/
class Int_var_event: public Binary_log_event
{
public:
    unsigned char  type;
    uint64_t  val;

    enum Int_event_type
    {
      INVALID_INT_EVENT,
      LAST_INSERT_ID_EVENT,
      INSERT_ID_EVENT
    };

    /**
      moving from pre processor symbols from global scope in log_event.h
      to an enum inside the class, since these are used only by
       members of this class itself.
    */
    enum Intvar_event_offset
    {
      I_TYPE_OFFSET= 0,
      I_VAL_OFFSET= 1
    };

    /**
      This method returns the string representing the type of the variable
      used in the event. Changed the definition to be similar to that
      previously defined in log_event.cc.
    */
    std::string get_var_type_string()
    {
      switch(type)
      {
      case INVALID_INT_EVENT:
        return "INVALID_INT";
      case LAST_INSERT_ID_EVENT:
        return "LAST_INSERT_ID";
      case INSERT_ID_EVENT:
        return "INSERT_ID";
      default: /* impossible */
        return "UNKNOWN";
      }
    }

    Int_var_event(const char* buf,
                  const Format_description_event *description_event);
    Int_var_event(unsigned char type_arg, uint64_t val_arg)
                 :type(type_arg), val(val_arg) {}
    ~Int_var_event() {}

    Log_event_type get_type_code() { return INTVAR_EVENT; }

    /*
      is_valid() is event specific sanity checks to determine that the
      object is correctly initialized. This is redundant here, because
      no new allocation is done in the constructor of the event.
      Else, they contain the value indicating whether the event was
      correctly initialized.
    */
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

/**
  @class Incident_event

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

  @section Incident_event_binary_format Binary Format
*/
class Incident_event: public Binary_log_event
{
public:
  Incident_event(Incident incident)
  : Binary_log_event(), m_incident(incident), m_message(NULL),
    m_message_length(0)
  {
  }
  Incident_event(const char *buf, unsigned int event_len,
                 const Format_description_event *description_event);

  Log_event_type get_type_code() { return INCIDENT_EVENT; }
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
protected:
  Incident m_incident;
  char *m_message;
  size_t m_message_length;
};

/**
  @class Xid_log_event

  Logs xid of the transaction-to-be-committed in the 2pc protocol.
  Has no meaning in replication, slaves ignore it.

  @section Xid_log_event_binary_format Binary Format
*/
class Xid_event: public Binary_log_event
{
public:
    Xid_event() {}
    Xid_event(const char *buf, const Format_description_event *fde);
    uint64_t xid;
    Log_event_type get_type_code() { return XID_EVENT; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};
/**
  @class Rand_event

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.0.
  4.1.1 does not need it (it's repeatable again) so this event needn't be
  written in 4.1.1 for PASSWORD() (but the fact that it is written is just a
  waste, it does not cause bugs).

  The state of the random number generation consists of 128 bits,
  which are stored internally as two 64-bit numbers.


  The Post-Header for this event type is empty.  The Body has two
  components:

  <table>
  <caption>Body for Rand_event</caption>

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
  @section Rand_event_binary_format Binary Format
*/
class Rand_event: public Binary_log_event
{
  public:
  unsigned long long seed1;
  unsigned long long seed2;
  enum Rand_event_data
  {
    RAND_SEED1_OFFSET= 0,
    RAND_SEED2_OFFSET= 8
  };
  Rand_event(unsigned long long seed1_arg, unsigned long long seed2_arg)
  {
    seed1= seed1_arg;
    seed2= seed2_arg;
  }
  Rand_event(const char* buf,
             const Format_description_event *description_event);
  Log_event_type get_type_code() { return RAND_EVENT ; }
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
};

/**
  @struct  gtid_info
  Structure to hold the members declared in the class Gtid_log_event
  those member are objects of classes defined in server(rpl_gtid.h).
  As we can not move all the classes defined there(in rpl_gtid.h) in libbinlogapi
  so this structure was created, to provide a way to map the decoded
  value in Gtid_event ctor and the class members defined in rpl_gtid.h,
  these classes are also the members of Gtid_log_event(subclass of this in server code)

  The structure contains the following components.
  <table>
  <caption>Structure gtid_info</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <th>type</th>
    <th>enum_group_type</th>
    <th>Group type of the groups created while transaction</th>
  </tr>
  <tr>
    <th>bytes_to_copy</th>
    <th>size_t</th>
    <th>Number of bytes to copy from the buffer, this is
        used as the size of array uuid_buf</th>
  </tr>
  <tr>
    <th>uuid_buf</th>
    <th>unsigned char array</th>
    <th>This stores the Uuid of the server on which transaction
        is happening</th>
  </tr>
  <tr>
    <th>rpl_gtid_sidno</th>
    <th>4 bytes integer</th>
    <th>SIDNO (source ID number, first component of GTID)</th>
  </tr>
  <tr>
    <th>rpl_gtid_gno</th>
    <th>8 bytes integer</th>
    <th>GNO (group number, second component of GTID)</th>
  </tr>
  </table>
*/
struct gtid_info
{
  uint8_t type;
  //TODO Replace 16 with BYTE_LENGTH defined in struct Uuid in rpl_gtid.h
  const static size_t bytes_to_copy= 16;
  unsigned char uuid_buf[bytes_to_copy];
  int32_t  rpl_gtid_sidno;
  int64_t  rpl_gtid_gno;
};

/**
  @class Gtid_event
  GTID stands for Global Transaction IDentifier
  It is composed of two part
     SID for Source Identifier, and
     GNO for Group Number.
  The basic idea is to
     1. Associate an identifier, the Global Transaction IDentifier or GTID,
        to every transaction.
     2. When a transaction is copied to a slave, re-executed on the slave,
        and written to the slave's binary log, the GTID is preserved.
     3. When a  slave connects to a master, the slave uses GTIDs instead of
        (file, offset)
  The Body has five components:

  <table>
  <caption>Body for Gtid_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>commit_seq_no</td>
    <td>8 bytes signed integer</td>
    <td>Prepare and commit sequence number. will be set to 0 if the event is
        not a transaction starter</td>
  </tr>
  <tr>
    <td>ENCODED_FLAG_LENGTH</td>
    <td>4 bytes static const integer</td>
    <td>Length of the commit_flag in event encoding</td>
  </tr>
  <tr>
    <td>ENCODED_SID_LENGTH</td>
    <td>4 bytes static const integer</td>
    <td>Length of SID in event encoding</td>
  </tr>
  <tr>
    <td>ENCODED_GNO_LENGTH</td>
    <td>4 bytes static const integer</td>
    <td>Length of GNO in event encoding.</td>
  </tr>
  <tr>
    <td>commit_flag</td>
    <td>bool</td>
    <td>True if this is the last group of the transaction</td>
  </tr>
  </table>

  @section Gtid_event_binary_format Binary Format
*/
class Gtid_event: public Binary_log_event
{
public:
  int64_t commit_seq_no;
  Gtid_event(const char *buffer, uint32_t event_len,
             const Format_description_event *descr_event);
  Gtid_event(bool commit_flag_arg): commit_flag(commit_flag_arg) {}
  Log_event_type get_type_code()
  {
    Log_event_type ret= (gtid_info_struct.type == 2 ?
                         ANONYMOUS_GTID_LOG_EVENT : GTID_LOG_EVENT);
    return ret;
  }
  //TODO: Add definitions for these methods
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
protected:
  static const int ENCODED_FLAG_LENGTH= 1;
  static const int ENCODED_SID_LENGTH= 16;// Uuid::BYTE_LENGTH;
  static const int ENCODED_GNO_LENGTH= 8;
  gtid_info gtid_info_struct;
  bool commit_flag;
};

/**
  @class Previous_gtids_event
  The Post-Header for this event type is empty.  The Body has two
  components:

  <table>
  <caption>Body for Previous_gtids_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>buf</td>
    <td>unsigned char array</td>
    <td>It contains the Gtids executed in the
        last binary log file.</td>
  </tr>

  <tr>
    <td>buf_size</td>
    <td>4 byte integer</td>
    <td>Size of the above buffer</td>
  </tr>
  </table>
  @section Previous_gtids_event_binary_format Binary Format
*/
class Previous_gtids_event : public Binary_log_event
{
public:
  Previous_gtids_event(const char *buf, unsigned int event_len,
                       const Format_description_event *descr_event);
  Previous_gtids_event() { } //called by the ctor with Gtid_set parameter
  Log_event_type get_type_code() { return PREVIOUS_GTIDS_LOG_EVENT ; }
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
protected:
  int buf_size;
  const unsigned char *buf;
};


/**
  @class Heartbeat_event

  Replication event to ensure to slave that master is alive.
  The event is originated by master's dump thread and sent straight to
  slave without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class log_ident and Binary_log_event::log_pos comprise
  @see the rpl_event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event master has sent from
  its binlog.
*/
class Heartbeat_event: public Binary_log_event
{
public:
  Heartbeat_event(const char* buf, unsigned int event_len,
                  const Format_description_event *description_event);

  const char* get_log_ident() { return log_ident; }
  unsigned int get_ident_len() { return ident_len; }

protected:
  const char* log_ident;
  unsigned int ident_len;
};

/**
  @class Unknown_event
*/
class Unknown_event: public Binary_log_event
{
public:
    Unknown_event() {}
    Unknown_event(const char* buf,
                  const Format_description_event *description_event)
   : Binary_log_event(&buf,
                      description_event->binlog_version,
                      description_event->server_version) {}

    Log_event_type get_type_code() { return UNKNOWN_EVENT;}
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};
} // end namespace binary_log
#endif	/* BINLOG_EVENT_INCLUDED */
