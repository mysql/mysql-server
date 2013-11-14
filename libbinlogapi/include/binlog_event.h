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
/*
 *The header contains functions macros for reading and storing in
 *machine independent format (low byte first).
 */
#include "byteorder.h"
#include "wrapper_functions.h"
#include "cassert"
#include <zlib.h> //for checksum calculations
#include "m_string.h"//for strmov used in Format_description_event's constructor
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


#ifndef SERVER_VERSION_LENGTH
#define SERVER_VERSION_LENGTH 60
#endif
extern char server_version[SERVER_VERSION_LENGTH];

namespace binary_log
{
/*
   This flag only makes sense for Format_description_log_event. It is set
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
  mysqlbinlog simply prints "ROLLBACK" Replication master does not abort on
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

/*
 We could have used SERVER_VERSION_LENGTH, but this introduces an
 obscure dependency - if somebody decided to change SERVER_VERSION_LENGTH
 this would break the replication protocol
+*/
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

enum_binlog_checksum_alg get_checksum_alg(const char* buf, unsigned long len);
bool event_checksum_test(unsigned char* buf, unsigned long event_len,
                         enum_binlog_checksum_alg alg);

#define LOG_EVENT_HEADER_SIZE 20

/**
  *forward declaration of Format_description_event class to be used in class
  *Log_event_header
*/
class Format_description_event;
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

  /* event-specific post-header sizes */
  enum enum_post_header_length{
    // where 3.23, 4.x and 5.0 agree
    QUERY_HEADER_MINIMAL_LEN= (4 + 4 + 1 + 2),
    // where 5.0 differs: 2 for len of N-bytes vars.
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
    FORMAT_DESCRIPTION_HEADER_LEN= (START_V3_HEADER_LEN+1+LOG_EVENT_TYPES),
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

  Binary_log_event(Log_event_header *header)
  {
      m_header= *header;
  }

  // TODO: Uncomment when the dependency of log_event on this class in removed
  /**
    Returns short information about the event
  */
  //virtual void print_event_info(std::ostream& info)=0;
  /**
    Returns detailed information about the event
  */
  // virtual void print_long_info(std::ostream& info);
  virtual ~Binary_log_event();
  /*
     The value is set by caller of FD constructor and
     Log_event::write_header() for the rest.
     In the FD case it's propagated into the last byte
     of post_header_len[] at FD::write().
     On the slave side the value is assigned from post_header_len[last]
     of the last seen FD event.
  */
  enum_binlog_checksum_alg checksum_alg;
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

private:
  Log_event_header m_header;
};

class Unknown_event: public Binary_log_event
{
public:
    Unknown_event(Log_event_header *header) : Binary_log_event(header) {}

    bool is_valid() const { return 1; }
    Log_event_type get_type_code() { return UNKNOWN_EVENT;}
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Query_event: public Binary_log_event
{
public:
    Query_event(Log_event_header *header) : Binary_log_event(header) {}
    uint32_t thread_id;
    uint32_t exec_time;
    uint16_t error_code;
    std::vector<uint8_t > variables;

    std::string db_name;
    std::string query;

    Log_event_type get_type_code() { return QUERY_EVENT;}
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
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
class Rotate_event: public virtual Binary_log_event
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
               const Format_description_event *description_event,
               Log_event_header *head);

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
  @class Stop_event

  A stop event is written to the log files under these circumstances:
  - A master writes the event to the binary log when it shuts down.
  - A slave writes the event to the relay log when it shuts down or
    when a RESET SLAVE statement is executed.

  @section Stop_log_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/

class Stop_event: public virtual Binary_log_event
{
public:
  Stop_event() : Binary_log_event()
  {
  }
  Stop_event(const char* buf,
             const Format_description_event *description_event,
             Log_event_header *header)
  : Binary_log_event(header)
  {
  }

  Log_event_type get_type_code() { return STOP_EVENT; }
  bool is_valid() const { return 1; }

  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info);
};

class Format_description_event: public Binary_log_event
{
public:
    Format_description_event(Log_event_header *header) : Binary_log_event(header) {}
    Format_description_event(uint16_t binlog_ver, const char* server_ver=0);
    uint16_t binlog_version;
    std::string master_version;
    uint32_t created_ts;
    uint8_t log_header_len;
    uint8_t common_header_len;
    char server_version[ST_SERVER_VER_LEN]; // This will be moved from here to Start_event_v3

    /* making post_header_len a uint8_t *, because it is done in that way
     *  in server, can be changed later if required.
    */
    uint8_t *post_header_len;
    uint8_t number_of_event_types;
    const uint8_t *event_type_permutation;
    Log_event_type get_type_code() { return FORMAT_DESCRIPTION_EVENT; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);


};

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

    User_var_event(Log_event_header *header) : Binary_log_event(header) {}
    std::string name;
    uint8_t is_null;
    uint8_t type;
    uint32_t charset; /* charset of the string */
    std::string value; /* encoded in binary speak, depends on .type */

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

class Table_map_event: public Binary_log_event
{
public:
    Table_map_event(Log_event_header *header) : Binary_log_event(header) {}
    uint64_t table_id;
    uint16_t flags;
    std::string db_name;
    std::string table_name;
    std::vector<uint8_t> columns;
    std::vector<uint8_t> metadata;
    std::vector<uint8_t> null_bits;

    Log_event_type get_type_code() { return TABLE_MAP_EVENT;}
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Row_event: public Binary_log_event
{
public:
    //TODO: Use the enum defined in log_event.h instead
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

    Row_event(Log_event_header *header) : Binary_log_event(header) {}
    uint64_t table_id;
    uint16_t flags;
    uint64_t columns_len;
    uint32_t null_bits_len;
    uint16_t var_header_len;
    Log_event_type  m_type;     /* Actual event type */
    std::vector<uint8_t> extra_header_data;
    std::vector<uint8_t> columns_before_image;
    std::vector<uint8_t> used_columns;
    std::vector<uint8_t> row;
    static std::string get_flag_string(enum_flag flag)
    {
      std::string str= "";
      if (flag & STMT_END_F)
        str.append(" Last event of the statement");
      if (flag & NO_FOREIGN_KEY_CHECKS_F)
        str.append(" No foreign Key checks");
      if (flag & RELAXED_UNIQUE_CHECKS_F)
        str.append(" No unique key checks");
      if (flag & COMPLETE_ROWS_F)
        str.append(" Complete Rows");
      if (flag & ~(STMT_END_F | NO_FOREIGN_KEY_CHECKS_F |
                   RELAXED_UNIQUE_CHECKS_F | COMPLETE_ROWS_F))
        str.append("Unknown Flag");
      return str;
    }
    Log_event_type get_type_code() { return m_type; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Int_var_event: public Binary_log_event
{
public:
    Int_var_event(Log_event_header *header) : Binary_log_event(header) {}
    uint8_t  type;
    uint64_t value;
    enum Int_event_type
    {
      INVALID_INT_EVENT,
      LAST_INSERT_ID_EVENT,
      INSERT_ID_EVENT
    };

    static std::string get_type_string(enum Int_event_type type)
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

    Log_event_type get_type_code() { return INTVAR_EVENT; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Incident_event: public Binary_log_event
{
public:
    Incident_event() : Binary_log_event() {}
    Incident_event(Log_event_header *header) : Binary_log_event(header) {}
    uint8_t type;
    std::string message;

    Log_event_type get_type_code() { return INCIDENT_EVENT; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Xid: public Binary_log_event
{
public:
    Xid(Log_event_header *header) : Binary_log_event(header) {}
    uint64_t xid_id;

    Log_event_type get_type_code() { return XID_EVENT; }
    bool is_valid() const { return 1; }
    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

Binary_log_event *create_incident_event(unsigned int type,
                                        const char *message,
                                        unsigned long pos= 0);

} // end namespace binary_log

#endif	/* BINLOG_EVENT_INCLUDED */
