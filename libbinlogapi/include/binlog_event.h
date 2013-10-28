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

#include <stdint.h>
#ifdef min //definition of min() and max() in std and libmysqlclient
           //can be/are different
#undef min
#endif
#ifdef max
#undef max
#endif
#include <list>
#include <vector>
#include <cstring>
#include <sstream>

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

#define CHECKSUM_CRC32_SIGNATURE_LEN 4
/**
   defined statically while there is just one alg implemented
*/
#define BINLOG_CHECKSUM_LEN CHECKSUM_CRC32_SIGNATURE_LEN

namespace binary_log {
namespace system {
/**
 * Convenience function to get the string representation of a binlog event.
 */
const char* get_event_type_str(Log_event_type type);
} // end namespace system

#define LOG_EVENT_HEADER_SIZE 20
class Log_event_header
{
public:
  uint8_t  marker; // always 0 or 0xFF
  uint32_t timestamp;
  uint8_t  type_code;
  uint32_t server_id;
  uint32_t event_length;
  uint32_t next_position;
  uint16_t flags;

  ~Log_event_header() {}
};


class Binary_log_event;

/**
 * TODO Base class for events. Implementation is in body()
 */
class Binary_log_event
{
public:
    Binary_log_event()
    {
        /*
          An event length of 0 indicates that the header isn't initialized
         */
        m_header.event_length= 0;
        m_header.type_code=    0;
    }

    Binary_log_event(Log_event_header *header)
    {
        m_header= *header;
    }

    /**
      Returns short information about the event
    */
    virtual void print_event_info(std::ostream& info)=0;
    /**
      Returns detailed information about the event
    */
    virtual void print_long_info(std::ostream& info);
    virtual ~Binary_log_event();

    /**
     * Helper method
     */
    enum Log_event_type get_event_type() const
    {
      return (enum Log_event_type) m_header.type_code;
    }

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

    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Rotate_event: public Binary_log_event
{
public:
    Rotate_event(Log_event_header *header) : Binary_log_event(header) {}
    std::string binlog_file;
    uint64_t binlog_pos;

    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);};

class Format_event: public Binary_log_event
{
public:
    Format_event(Log_event_header *header) : Binary_log_event(header) {}
    uint16_t binlog_version;
    std::string master_version;
    uint32_t created_ts;
    uint8_t log_header_len;
    std::vector<uint8_t> post_header_len;

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

    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

class Xid: public Binary_log_event
{
public:
    Xid(Log_event_header *header) : Binary_log_event(header) {}
    uint64_t xid_id;

    void print_event_info(std::ostream& info);
    void print_long_info(std::ostream& info);
};

Binary_log_event *create_incident_event(unsigned int type,
                                        const char *message,
                                        unsigned long pos= 0);

} // end namespace binary_log

#endif	/* BINLOG_EVENT_INCLUDED */
