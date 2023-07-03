/* Copyright (c) 2007, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_RECORD_H
#define RPL_RECORD_H

#include <stddef.h>
#include <sys/types.h>

#include "my_inttypes.h"

class Relay_log_info;
struct TABLE;

struct MY_BITMAP;

class table_def;
class Bit_reader;

enum class enum_row_image_type { WRITE_AI, UPDATE_BI, UPDATE_AI, DELETE_BI };

#if defined(MYSQL_SERVER)
size_t pack_row(TABLE *table, MY_BITMAP const *cols, uchar *row_data,
                const uchar *data, enum_row_image_type row_image_type,
                ulonglong value_options = 0);

/**
  Unpack a row image (either before-image or after-image) into @c
  table->record[0].

  The row is assumed to only consist of the fields for which the
  corresponding bit in bitset @c column_image is set; the other parts
  of the record are left alone.

  If the replica table has more columns than the source table, then the
  extra columns are not touched by this function. If the source table
  has more columns than the replica table, then the position is moved to
  after the extra columns, but the values are not used.

  If the replica has a GIPK and the source does not, then the extra column
  is not touched by this function. If the source table has a GIPK and the
  replica does not, then the position is shifted forward by 1.

  - The layout of a row is:

    For WRITE_ROWS_EVENT:
        +--------------+
        | after-image |
        +--------------+

    For DELETE_ROWS_EVENT:
        +--------------+
        | before-image |
        +--------------+

    For UPDATE_ROWS_EVENT:
        +--------------+-------------+
        | before-image | after-image |
        +--------------+-------------+

    For PARTIAL_UPDATE_ROWS_EVENT:
        +--------------+--------------+-------------+
        | before-image | shared-image | after-image |
        +--------------+--------------+-------------+

  - Each of before-image and after-image has the following format:
        +--------+-------+-------+     +-------+
        | length | col_1 | col_2 | ... | col_N |
        +--------+-------+-------+     +-------+
    length is a 4-byte integer in little-endian format, equal to the
    total length in bytes of col_1, col_2, ..., col_N.

  - The shared-image has one of the following formats:
        +-----------------+
        | value_options=0 |
        +-----------------+
    or
        +-----------------+--------------+
        | value_options=1 | partial_bits |
        +-----------------+--------------+
    where:

    - value_options is a bitmap, stored as an integer, in the format
      of net_field_length. Currently only one bit is allowed:
      1=PARTIAL_JSON_UPDATES (so therefore the integer is always 0 or
      1, so in reality value_options is only one byte).  When
      PARTIAL_JSON_UPDATES=0, there is nothing else in the
      shared-image.  When PARTIAL_JSON_UPDATES=1, there is a
      partial_bits field.

    - partial_bits has one bit for each *JSON* column in the table
      (regardless of whether it is included in the before-image and/or
      after-image).  The bit is 0 if the JSON update is stored as a
      full document in the after-image, and 1 if the JSON update in
      partial form in the after-image.

    - Both when reading the before-image and when reading the
      after-image it is necessary to know the partialness of JSON
      columns: when reading the before-image, before looking up the
      row in the table, we need to set the column in the table's
      'read_set' (even if the column was not in the before-image), in
      order to guarantee that the storage engine reads that column, so
      that there is any base document that the diff can be applied
      on. When reading the after-image, we need to know which columns
      are partial so that we can correctly parse the data for that
      column.

      Therefore, when this function parses the before-image of a
      PARTIAL_UPDATE_ROWS_LOG_EVENT, it reads both the before-image
      and the shared-image, but leaves the read position after the
      before-image.  So when it parses the after-image of a
      PARTIAL_UPDATE_ROWS_LOG_EVENT, the read position is at the
      beginning of the shared-image, so it parses both the
      shared-image and the after-image.

  @param[in] rli Applier execution context

  @param[in,out] table Table to unpack into

  @param[in] source_column_count Number of columns that the source had
  in its table

  @param[in] row_data Beginning of the row image

  @param[in] column_image Pointer to a bit vector where the N'th bit
  is 0 for columns that are not included in the event, and 1 for
  columns that are included in the event.

  @param[out] row_image_end_p If this function returns successfully, it
  sets row_image_end to point to the next byte after the row image
  that it has read.

  @param[in] event_end Pointer to the end of the event.

  @param[in] row_image_type The type of row image that we are going to
  read: WRITE_AI, UPDATE_BI, UPDATE_AI, or DELETE_BI.

  @param[in] event_has_value_options true for PARTIAL_UPDATE_ROWS_EVENT,
  false for UPDATE_ROWS_EVENT.

  @param only_seek If true, this is a seek operation rather than a
  read operation.  It will only compute the row_image_end_p pointer,
  and not read anything into the table and not apply any JSON diffs.
  (This is used in slave_rows_search_algorithms=HASH_SCAN, which (1)
  unpacks and hashes the before-image for all rows in the event, (2)
  scans the table, and for each matching row it (3) unpacks the
  after-image and applies on the table. In step (1) it needs to unpack
  the after-image too, in order to move the read position forwards,
  and then it should use only_seek=true.  This is an optimization, but
  more importantly, when the after-image contains partial JSON, the
  partial JSON cannot be applied in step (1) since there is no JSON
  document to apply it on.)

  @returns false on success, true on error.
 */
bool unpack_row(Relay_log_info const *rli, TABLE *table,
                uint const source_column_count, uchar const *const row_data,
                MY_BITMAP const *column_image,
                uchar const **const row_image_end_p,
                uchar const *const event_end,
                enum_row_image_type row_image_type,
                bool event_has_value_options, bool only_seek);

/**
  Return a pointer within a row event's row data, to the data of the first
  column that exists on the replica.

  This skips the 'null bits' field, which precedes the column definitions in the
  row image. In case a GIPK exists in the event but not in this replica's table
  definition, it skips the GIPK too.

  @param raw_data The data received from the source
  @param column_image The column bitmap
  @param column_count The number of column in the image bitmap
  @param null_bits    The bits that are null
  @param tabledef     The source table definition structure
  @param source_has_gipk  If the source table has a GIPK
  @param replica_has_gipk If the replica table has a GIPK

  @return const uchar* The adjusted point to the source data
 */
const uchar *translate_beginning_of_raw_data(
    const uchar *raw_data, MY_BITMAP const *column_image, size_t column_count,
    Bit_reader &null_bits, table_def *tabledef, bool source_has_gipk,
    bool replica_has_gipk);

// Fill table's record[0] with default values.
int prepare_record(TABLE *const table, const MY_BITMAP *cols, const bool check);
#endif

/**
  Template base class of Bit_reader / Bit_writer.
*/
template <typename T, typename UT>
class Bit_stream_base {
 protected:
  /// Pointer to beginning of buffer where bits are read or written.
  T *m_ptr;
  /// Current position in buffer.
  uint m_current_bit;

 public:
  /**
    Construct a new Bit_stream (either reader or writer).
    @param ptr Pointer where bits will be read or written.
  */
  Bit_stream_base(T *ptr) : m_ptr(ptr), m_current_bit(0) {}

  /**
    Set the buffer pointer.
    @param ptr Pointer where bits will be read or written.
  */
  void set_ptr(T *ptr) { m_ptr = ptr; }
  /**
    Set the buffer pointer, using an unsigned datatype.
    @param ptr Pointer where bits will be read or written.
  */
  void set_ptr(UT *ptr) { m_ptr = (T *)ptr; }

  /// @return the current position.
  uint tell() const { return m_current_bit; }

  /**
    Print all the bits before the current position to the debug trace.
    @param str Descriptive text that will be prefixed before the bit string.
  */
  void dbug_print(const char *str [[maybe_unused]]) const;
};

/**
  Auxiliary class to write a stream of bits to a memory location.

  Call set() to write a bit and move the position one bit forward.
*/
class Bit_writer : public Bit_stream_base<char, uchar> {
 public:
  Bit_writer(char *ptr = nullptr) : Bit_stream_base<char, uchar>(ptr) {}
  Bit_writer(uchar *ptr) : Bit_writer((char *)ptr) {}

  /**
    Write the next bit and move the write position one bit forward.
    @param set_to_on If true, set the bit to 1, otherwise set it to 0.
  */
  void set(bool set_to_on) {
    uint byte = m_current_bit / 8;
    uint bit_within_byte = m_current_bit % 8;
    m_current_bit++;
    if (bit_within_byte == 0)
      m_ptr[byte] = set_to_on ? 1 : 0;
    else if (set_to_on)
      m_ptr[byte] |= 1 << bit_within_byte;
  }
};

/**
  Auxiliary class to read or write a stream of bits to a memory location.

  Call get() to read a bit and move the position one bit forward.
*/
class Bit_reader : public Bit_stream_base<const char, const uchar> {
 public:
  Bit_reader(const char *ptr = nullptr)
      : Bit_stream_base<const char, const uchar>(ptr) {}
  Bit_reader(const uchar *ptr) : Bit_reader((const char *)ptr) {}

  /**
    Read the next bit and move the read position one bit forward.
    @return true if the bit was 1, false if the bit was 0.
  */
  bool get() {
    uint byte = m_current_bit / 8;
    uint bit_within_byte = m_current_bit % 8;
    m_current_bit++;
    return (m_ptr[byte] & (1 << bit_within_byte)) != 0;
  }
};

#endif  // ifdef RPL_RECORD_H
