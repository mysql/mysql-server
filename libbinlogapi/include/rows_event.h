/*
Copyright (c) 2013, Oracle and/or its affiliates. All rights
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
#ifndef ROWS_EVENT_INCLUDED
#define	ROWS_EVENT_INCLUDED

#include "binlog_event.h"
#include "debug_vars.h"
/**
 The header contains functions macros for reading and storing in
 machine independent format (low byte first).
*/
#include "byteorder.h"
#include "wrapper_functions.h"
#include "table_id.h"
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
   1 byte length, 1 byte format
   Length is total length in bytes, including 2 byte header
   Length values 0 and 1 are currently invalid and reserved.
*/
#define EXTRA_ROW_INFO_LEN_OFFSET 0
#define EXTRA_ROW_INFO_FORMAT_OFFSET 1
#define EXTRA_ROW_INFO_HDR_BYTES 2
#define EXTRA_ROW_INFO_MAX_PAYLOAD (255 - EXTRA_ROW_INFO_HDR_BYTES)

/* RW = "RoWs" */
#define RW_MAPID_OFFSET    0
#define RW_FLAGS_OFFSET    6
#define RW_VHLEN_OFFSET    8
#define RW_V_TAG_LEN       1
#define RW_V_EXTRAINFO_TAG 0

namespace binary_log
{
class Table_map_event: public virtual Binary_log_event
{
public:
  /* Constants */
  enum Table_map_event_offset {
    /* TM = "Table Map" */
    TM_MAPID_OFFSET= 0,
    TM_FLAGS_OFFSET= 6
  };

  enum
  {
    TYPE_CODE= TABLE_MAP_EVENT
  };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error
  {
    ERR_OPEN_FAILURE= -1,               /**< Failure to open table */
    ERR_OK= 0,                                 /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED= 1,      /**< No more room for tables */
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

  typedef uint16_t flag_set;
  /* Special constants representing sets of flags */
  enum
  {
    TM_NO_FLAGS = 0U,
    TM_BIT_LEN_EXACT_F = (1U << 0),
    TM_REFERRED_FK_DB_F = (1U << 1)
  };

  flag_set get_flags(flag_set flag) const { return m_flags & flag; }

  Table_map_event(const char *buf, unsigned int event_len,
                  const Format_description_event *description_event);
  Table_map_event(unsigned long colcnt) : m_colcnt(colcnt) {}

  virtual ~Table_map_event();

  std::string m_dbnam;
  size_t m_dblen;
  std::string m_tblnam;
  size_t m_tbllen;
  unsigned long m_colcnt;
  std::vector<uint8_t> m_coltype;

  Table_id m_table_id;
  flag_set m_flags;
  size_t   m_data_size;

  std::vector<uint8_t> m_field_metadata;        // buffer for field metadata
  /*
    The size of field metadata buffer set by calling save_field_metadata()
  */
  unsigned long  m_field_metadata_size;
  unsigned char* m_null_bits;

  Table_map_event()
  :// Binary_log_event(TABLE_MAP_EVENT),
  m_null_bits(0) {}
  //TODO: Remove this virtual method and replace by a member veriable in
  // class Binary_log_event
  virtual Log_event_type get_type_code() { return TABLE_MAP_EVENT; }
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
};

class Rows_event: public virtual  Binary_log_event
{
public:
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

  virtual ~Rows_event();
  Rows_event()
   : m_table_id(0), m_extra_row_data(0)
   {}
  Rows_event(const char *buf, unsigned int event_len,
             const Format_description_event *description_event);
protected:
  Table_id m_table_id;
  uint16_t m_flags;           /* Flags for row-level events */
  unsigned long m_width;      /* The width of the columns bitmap */
  uint32_t n_bits_len;  //should be n_bits_len, determined by (m_width + 7) / 8
  uint16_t var_header_len;
  Log_event_type  m_type;     /* Actual event type */
  unsigned char* m_extra_row_data;
  // The three below are binary encoded.
  std::vector<uint8_t> columns_before_image;
  std::vector<uint8_t> columns_after_image;
  std::vector<uint8_t> row;

public:

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
  Log_event_type get_type_code() { return m_type; } /*Specific type (_V1 etc)*/
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
};


/**
  @class Write_rows_log_event

  Log row insertions and updates. The event contain several
  insert/update rows for a table. Note that each event contains only
  rows for one table.

  @section Write_rows_log_event_binary_format Binary Format
*/
class Write_rows_event : public virtual Rows_event
{
public:
  Write_rows_event(const char *buf, unsigned int event_len,
                   const Format_description_event *description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version)
  {
    this->header()->type_code= m_type;
  };
  Write_rows_event()
  {}
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
class Update_rows_event : public virtual Rows_event
{
public:
  Update_rows_event(const char *buf, unsigned int event_len,
                    const Format_description_event *description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version)
  {
    this->header()->type_code= m_type;
  };
  Update_rows_event()
  {}
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
class Delete_rows_event : public virtual Rows_event
{
public:
  Delete_rows_event(const char *buf, unsigned int event_len,
                    const Format_description_event *description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version)
  {
    this->header()->type_code= m_type;
  };
  Delete_rows_event()
  {}
};
}
#endif	/* ROWS:_EVENT_INCLUDED */
