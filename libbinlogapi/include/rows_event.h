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
/**
  @class Table_map_event

  In row-based mode, every row operation event is preceded by a
  Table_map_event which maps a table definition to a number.  The
  table definition consists of database name, table name, and column
  definitions.

  @section Table_map_event_binary_format Binary Format

  The Post-Header has the following components:

  <table>
  <caption>Post-Header for Table_map_event</caption>

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
  <caption>Body for Table_map_event</caption>

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
    Table_table_map_event_column_types "below" (along with
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
    in the table @ref Table_table_map_event_column_types
    "below".</td>
  </tr>

  <tr>
    <td>null_bits</td>
    <td>column_count bits, rounded up to nearest byte</td>
    <td>For each column, a bit indicating whether data in the column
    can be NULL or not.  The number of bytes needed for this is
    int((column_count + 7) / 8).  The flag for the first column from the
    left is in the least-significant bit of the first byte, the second
    is in the second least significant bit of the first byte, the
    ninth is in the least significant bit of the second byte, and so
    on.  </td>
  </tr>

  </table>

  The table below lists all column types, along with the numerical
  identifier for it and the size and interpretation of meta-data used
  to describe the type.

  @anchor Table_table_map_event_column_types
  <table>
  <caption>Table_map_event column types: numerical identifier and
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
    number of bytes is either int((length + 7) / 8) or int(length / 8).
    </td>
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
class Table_map_event: public Binary_log_event
{
public:
  /* Constants */
  enum Table_map_event_offset {
    /* TM = "Table Map" */
    TM_MAPID_OFFSET= 0,
    TM_FLAGS_OFFSET= 6
  };

  typedef uint16_t flag_set;
  Table_map_event(const char *buf, unsigned int event_len,
                  const Format_description_event *description_event);
  Table_map_event(unsigned long colcnt) : m_colcnt(colcnt) {}

  virtual ~Table_map_event();


  /* Event post header contents */
  Table_id m_table_id;
  flag_set m_flags;

  size_t   m_data_size;                         // event data size

  /* Event body contents */
  std::string m_dbnam;
  size_t m_dblen;
  std::string m_tblnam;
  size_t m_tbllen;
  unsigned long m_colcnt;
  std::vector<uint8_t> m_coltype;
  /*
    The size of field metadata buffer set by calling save_field_metadata()
  */
  unsigned long  m_field_metadata_size;
  std::vector<uint8_t> m_field_metadata;        // field metadata
  unsigned char* m_null_bits;

  Table_map_event()
  : m_null_bits(0)
  {
  }
  /*
    TODO: Remove this virtual method and replace by a member veriable in
    class Binary_log_event.
  */
  virtual Log_event_type get_type_code() { return TABLE_MAP_EVENT; }
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
};


/**
  @class Rows_event

 Common base class for all row-containing binary log events.

 RESPONSIBILITIES

   - Provide an interface for adding an individual row to the event.

  The Post-Header has the following components:

  <table>
  <caption>Post-Header for Rows_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>table_id</td>
    <td>6 bytes unsigned integer</td>
    <td>The number that identifies the table</td>
  </tr>

  <tr>
    <td>flags</td>
    <td>2 byte bitfield</td>
    <td>Reserved for future use; currently always 0.</td>
  </tr>

  </table>

  The Body has the following components:

  <table>
  <caption>Body for Rows_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>


  <tr>
    <td>var_header_len</td>
    <td>packed integer</td>
    <td>Represents the number of columns in the table</td>
  </tr>

  <tr>
    <td>width</td>
    <td>Bitfield, variable sized</td>
    <td>Indicates whether each column is used, one bit per column.
        For this field, the amount of storage required for N columns
        is INT((N + 7) / 8) bytes. </td>
  </tr>

  <tr>
    <td>extra_row_data</td>
    <td>unsigned char pointer</td>
    <td>Pointer to extra row data if any. If non null, first byte is length</td>
  </tr>

  <tr>
    <td>columns_before_image</td>
    <td>vector of elements of type unsigned char</td>
    <td>for UPDATE_ROWS_LOG_EVENT only. Bit-field indicating whether
        each column is used in the UPDATE_ROWS_LOG_EVENT after-image;
        one bit per column. For this field, the amount of storage
        required for N columns is INT((N + 7) / 8) bytes.</td>
  </tr>

  <tr>
    <td>columns_after_image</td>
    <td>vector of elements of type unsigned char</td>
    <td> Bit-field indicating whether  each column is used in the after-image;
        one bit per column. For this field, the amount of storage
        required for N columns is INT((N + 7) / 8) bytes.</td>
  </tr>

  <tr>
    <td>row</td>
    <td>vector of elements of type unsigned char</td>
    <td> A sequence of zero or more rows. The end is determined by the size
         of the event. Each row has the following format:
           - A Bit-field indicating whether each field in the row is NULL.
             Only columns that are "used" according to the second field in
             the variable data part are listed here. If the second field in
             the variable data part has N one-bits, the amount of storage
             required for this field is INT((N + 7) / 8) bytes.
           - The row-image, containing values of all table fields. This only
             lists table fields that are used (according to the second field
             of the variable data part) and non-NULL (according to the
             previous field). In other words, the number of values listed here
             is equal to the number of zero bits in the previous field.
             (not counting padding bits in the last byte).
    </td>
  </tr>
*/
class Rows_event: public virtual  Binary_log_event
{
public:
  /*
    These definitions allow to combine the flags into an
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

  Rows_event()
  : m_table_id(0), m_width(0), m_extra_row_data(0)
  {
  }

 Rows_event(const char *buf, unsigned int event_len,
             const Format_description_event *description_event);

  virtual ~Rows_event();

protected:
  Log_event_type  m_type;     /* Actual event type */

  /* Post header content */
  Table_id m_table_id;
  uint16_t m_flags;           /* Flags for row-level events */

  /* Body of the event */
  unsigned long m_width;      /* The width of the columns bitmap */
  uint32_t n_bits_len;        /* value determined by (m_width + 7) / 8 */
  uint16_t var_header_len;

  unsigned char* m_extra_row_data;

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
  @class Write_rows_event

  Log row insertions. The event contain several  insert/update rows
  for a table. Note that each event contains only  rows for one table.

  @section Write_rows_event_binary_format Binary Format
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
  @class Update_rows_event

  Log row updates with a before image. The event contain several
  update rows for a table. Note that each event contains only rows for
  one table.

  Also note that the row data consists of pairs of row data: one row
  for the old data and one row for the new data.

  @section Update_rows_event_binary_format Binary Format
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
  @class Delete_rows_event

  Log row deletions. The event contain several delete rows for a
  table. Note that each event contains only rows for one table.

  RESPONSIBILITIES

    - Act as a container for rows that has been deleted on the master
      and should be deleted on the slave.

   @section Delete_rows_event_binary_format Binary Format
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
#endif	/* ROWS_EVENT_INCLUDED */
