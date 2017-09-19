/*
* Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of the
* License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301  USA
*/

#include "ngs/protocol/row_builder.h"

#include "ngs/protocol/output_buffer.h"

#include "ngs_common/xdatetime.h"
#include "ngs_common/xdecimal.h"

#include "decimal.h"

#include "ngs_common/protocol_protobuf.h"
#include <iostream>
#include <string>
#include <limits>
#include <algorithm>
#include <cstring>

using namespace ngs;

#define ADD_FIELD_HEADER() \
  DBUG_ASSERT(m_row_processing); \
  google::protobuf::internal::WireFormatLite::WriteTag( \
    1, \
    google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED, \
    m_out_stream.get() \
  ); \
  ++m_num_fields;


Row_builder::Row_builder():
  m_row_processing(false)
{}

Row_builder::~Row_builder()
{
  abort_row();
}

void Row_builder::abort_row()
{
  if (m_row_processing)
  {
    m_out_stream.reset();
    m_out_buffer->rollback();
    m_row_processing = false;
  }
}

void Row_builder::start_row(Output_buffer* out_buffer)
{
  m_num_fields = 0;
  abort_row();

  Message_builder::start_message(out_buffer, Mysqlx::ServerMessages::RESULTSET_ROW);

  m_row_processing = true;
}

void Row_builder::end_row()
{
  if (m_row_processing)
  {
    Message_builder::end_message();

    m_row_processing = false;
  }
}

void Row_builder::add_null_field()
{
  ADD_FIELD_HEADER();

  m_out_stream->WriteVarint32(0);
}


void Row_builder::add_longlong_field(longlong value, my_bool unsigned_flag)
{
  ADD_FIELD_HEADER();

  if (unsigned_flag)
  {
    m_out_stream->WriteVarint32(CodedOutputStream::VarintSize64(value));
    m_out_stream->WriteVarint64(value);
  }
  else
  {
    google::protobuf::uint64 encoded = google::protobuf::internal::WireFormatLite::ZigZagEncode64(value);
    m_out_stream->WriteVarint32(CodedOutputStream::VarintSize64(encoded));
    m_out_stream->WriteVarint64(encoded);
  }
}

/** BEGIN TEMPORARY CODE */
typedef decimal_digit_t dec1;
typedef longlong      dec2;
#define DIG_PER_DEC1 9
#define DIG_MASK     100000000
#define DIG_BASE     1000000000
#define DIG_MAX      (DIG_BASE-1)
#define DIG_BASE2    ((dec2)DIG_BASE * (dec2)DIG_BASE)
#define ROUND_UP(X)  (((X)+DIG_PER_DEC1-1)/DIG_PER_DEC1)


static inline int count_leading_zeroes(int i, dec1 val)
{
  int ret = 0;
  switch (i)
  {
    /* @note Intentional fallthrough in all case labels */
  case 9: if (val >= 1000000000) break; ++ret;  // Fall through.
  case 8: if (val >= 100000000) break; ++ret;  // Fall through.
  case 7: if (val >= 10000000) break; ++ret;  // Fall through.
  case 6: if (val >= 1000000) break; ++ret;  // Fall through.
  case 5: if (val >= 100000) break; ++ret;  // Fall through.
  case 4: if (val >= 10000) break; ++ret;  // Fall through.
  case 3: if (val >= 1000) break; ++ret;  // Fall through.
  case 2: if (val >= 100) break; ++ret;  // Fall through.
  case 1: if (val >= 10) break; ++ret;  // Fall through.
  case 0: if (val >= 1) break; ++ret;  // Fall through.
  default: { DBUG_ASSERT(FALSE); }
  }
  return ret;
}

static dec1 *remove_leading_zeroes(const decimal_t *from, int *intg_result)
{
  int intg = from->intg, i;
  dec1 *buf0 = from->buf;
  i = ((intg - 1) % DIG_PER_DEC1) + 1;
  while (intg > 0 && *buf0 == 0)
  {
    intg -= i;
    i = DIG_PER_DEC1;
    buf0++;
  }
  if (intg > 0)
  {
    intg -= count_leading_zeroes((intg - 1) % DIG_PER_DEC1, *buf0);
    DBUG_ASSERT(intg > 0);
  }
  else
    intg = 0;
  *intg_result = intg;
  return buf0;
}

static int __decimal2string(const decimal_t *from, char *to, int *to_len,
  int fixed_precision, int fixed_decimals,
  char filler)
{
  /* {intg_len, frac_len} output widths; {intg, frac} places in input */
  int len, intg, frac = from->frac, i, intg_len, frac_len, fill;
  /* number digits before decimal point */
  int fixed_intg = (fixed_precision ?
    (fixed_precision - fixed_decimals) : 0);
  int error = E_DEC_OK;
  char *s = to;
  dec1 *buf, *buf0, tmp;

  DBUG_ASSERT(*to_len >= 2 + from->sign);

  /* removing leading zeroes */
  buf0 = remove_leading_zeroes(from, &intg);
  if (unlikely(intg + frac == 0))
  {
    intg = 1;
    tmp = 0;
    buf0 = &tmp;
  }

  if (!(intg_len = fixed_precision ? fixed_intg : intg))
    intg_len = 1;
  frac_len = fixed_precision ? fixed_decimals : frac;
  len = from->sign + intg_len + MY_TEST(frac) + frac_len;
  if (fixed_precision)
  {
    if (frac > fixed_decimals)
    {
      error = E_DEC_TRUNCATED;
      frac = fixed_decimals;
    }
    if (intg > fixed_intg)
    {
      error = E_DEC_OVERFLOW;
      intg = fixed_intg;
    }
  }
  else if (unlikely(len > --*to_len)) /* reserve one byte for \0 */
  {
    int j = len - *to_len;             /* excess printable chars */
    error = (frac && j <= frac + 1) ? E_DEC_TRUNCATED : E_DEC_OVERFLOW;

    /*
    If we need to cut more places than frac is wide, we'll end up
    dropping the decimal point as well.  Account for this.
    */
    if (frac && j >= frac + 1)
      j--;

    if (j > frac)
    {
      intg_len = intg -= j - frac;
      frac = 0;
    }
    else
      frac -= j;
    frac_len = frac;
    len = from->sign + intg_len + MY_TEST(frac) + frac_len;
  }
  *to_len = len;
  s[len] = 0;

  if (from->sign)
    *s++ = '-';

  if (frac)
  {
    char *s1 = s + intg_len;
    fill = frac_len - frac;
    buf = buf0 + ROUND_UP(intg);
    *s1++ = '.';
    for (; frac>0; frac -= DIG_PER_DEC1)
    {
      dec1 x = *buf++;
      for (i = MY_MIN(frac, DIG_PER_DEC1); i; i--)
      {
        dec1 y = x / DIG_MASK;
        *s1++ = '0' + (uchar)y;
        x -= y*DIG_MASK;
        x *= 10;
      }
    }
    for (; fill > 0; fill--)
      *s1++ = filler;
  }

  fill = intg_len - intg;
  if (intg == 0)
    fill--; /* symbol 0 before digital point */
  for (; fill > 0; fill--)
    *s++ = filler;
  if (intg)
  {
    s += intg;
    for (buf = buf0 + ROUND_UP(intg); intg>0; intg -= DIG_PER_DEC1)
    {
      dec1 x = *--buf;
      for (i = MY_MIN(intg, DIG_PER_DEC1); i; i--)
      {
        dec1 y = x / 10;
        *--s = '0' + (uchar)(x - y * 10);
        x = y;
      }
    }
  }
  else
    *s = '0';

  return error;
}
/* END TEMPORARY CODE */


void Row_builder::add_decimal_field(const decimal_t * value)
{
  ADD_FIELD_HEADER();

  /*TODO: inefficient, refactor to skip the string conversion*/
  std::string str_buf;
  int str_len = 200;
  str_buf.resize(str_len);
  __decimal2string(value, &(str_buf)[0], &str_len, 0, 0, 0);
  str_buf.resize(str_len);

  mysqlx::Decimal dec(str_buf);
  std::string dec_bytes = dec.to_bytes();

  m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(dec_bytes.length()));
  m_out_stream->WriteString(dec_bytes);
}

void Row_builder::add_decimal_field(const char * const value, size_t length)
{
  ADD_FIELD_HEADER();

  std::string dec_str(value, length);
  mysqlx::Decimal dec(dec_str);
  std::string dec_bytes = dec.to_bytes();

  m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(dec_bytes.length()));
  m_out_stream->WriteString(dec_bytes);
}

void Row_builder::add_double_field(double value)
{
  ADD_FIELD_HEADER();

  m_out_stream->WriteVarint32(sizeof(google::protobuf::uint64));
  m_out_stream->WriteLittleEndian64(google::protobuf::internal::WireFormatLite::EncodeDouble(value));
}

void Row_builder::add_float_field(float value)
{
  ADD_FIELD_HEADER();

  m_out_stream->WriteVarint32(sizeof(google::protobuf::uint32));
  m_out_stream->WriteLittleEndian32(google::protobuf::internal::WireFormatLite::EncodeFloat(value));
}

void Row_builder::add_date_field(const MYSQL_TIME * value)
{
  ADD_FIELD_HEADER();

  google::protobuf::uint32 size = CodedOutputStream::VarintSize64(value->year)
    + CodedOutputStream::VarintSize64(value->month)
    + CodedOutputStream::VarintSize64(value->day);

  m_out_stream->WriteVarint32(size);

  m_out_stream->WriteVarint64(value->year);
  m_out_stream->WriteVarint64(value->month);
  m_out_stream->WriteVarint64(value->day);
}

size_t Row_builder::get_time_size(const MYSQL_TIME * value)
{
  size_t result = 0;
  if (value->hour != 0 || value->minute != 0 || value->second != 0 || value->second_part != 0)
  {
    result += CodedOutputStream::VarintSize64(value->hour);
  }
  if (value->minute != 0 || value->second != 0 || value->second_part != 0)
  {
    result += CodedOutputStream::VarintSize64(value->minute);
  }
  if (value->second != 0 || value->second_part != 0)
  {
    result += CodedOutputStream::VarintSize64(value->second);
  }
  if (value->second_part != 0)
  {
    result += CodedOutputStream::VarintSize64(value->second_part);
  }

  return result;
}

void Row_builder::append_time_values(const MYSQL_TIME * value, CodedOutputStream* out_stream)
{
  // optimize the output size skipping the right-most 0's
  if (value->hour != 0 || value->minute != 0 || value->second != 0 || value->second_part != 0)
  {
    out_stream->WriteVarint64(value->hour);
  }
  if (value->minute != 0 || value->second != 0 || value->second_part != 0)
  {
    out_stream->WriteVarint64(value->minute);
  }
  if (value->second != 0 || value->second_part != 0)
  {
    out_stream->WriteVarint64(value->second);
  }
  if (value->second_part != 0)
  {
    out_stream->WriteVarint64(value->second_part);
  }
}

void Row_builder::add_time_field(const MYSQL_TIME * value, uint decimals)
{
  ADD_FIELD_HEADER();

  m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(get_time_size(value) + 1)); // +1 for sign

  /*sign*/
  google::protobuf::uint8 neg = (value->neg) ? 0x01 : 0x00;
  m_out_stream->WriteRaw(&neg, 1);

  append_time_values(value, m_out_stream.get());
}

void Row_builder::add_datetime_field(const MYSQL_TIME * value, uint decimals)
{
  ADD_FIELD_HEADER();

  google::protobuf::uint32 size = CodedOutputStream::VarintSize64(value->year)
    + CodedOutputStream::VarintSize64(value->month)
    + CodedOutputStream::VarintSize64(value->day)
    + static_cast<int>(get_time_size(value));

  m_out_stream->WriteVarint32(size);

  m_out_stream->WriteVarint64(value->year);
  m_out_stream->WriteVarint64(value->month);
  m_out_stream->WriteVarint64(value->day);

  append_time_values(value, m_out_stream.get());
}

void Row_builder::add_string_field(const char * const value, size_t length,
  const CHARSET_INFO * const valuecs)
{
  ADD_FIELD_HEADER();

  m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(length + 1)); // 1 byte for thre trailing '\0'

  m_out_stream->WriteRaw(value, static_cast<int>(length));
  char zero = '\0';
  m_out_stream->WriteRaw(&zero, 1);
}

void Row_builder::add_set_field(const char * const value, size_t length,
  const CHARSET_INFO * const valuecs)
{
  ADD_FIELD_HEADER();

  // special case: empty SET
  if (0 == length)
  {
    // write length=0x01 to the buffer and we're done here
    m_out_stream->WriteVarint32(1);
    m_out_stream->WriteVarint64(0x01);
    return;
  }

  std::vector<std::string> set_vals;
  const char *comma, *p_value = value;
  unsigned int elem_len;
  do
  {
    comma = std::strchr(p_value, ',');
    if (comma != NULL)
    {
      elem_len = static_cast<unsigned int>(comma - p_value);
      set_vals.push_back(std::string(p_value, elem_len));
      p_value = comma + 1;
    }
  } while (comma != NULL);

  // still sth left to store
  if ((size_t)(p_value - value) < length)
  {
    elem_len = static_cast<unsigned int>(length - (p_value - value));
    set_vals.push_back(std::string(p_value, elem_len));
  }

  // calculate size needed for all lengths and values
  google::protobuf::uint32 size = 0;
  for (size_t i = 0; i < set_vals.size(); ++i)
  {
    size += CodedOutputStream::VarintSize64(set_vals[i].length());
    size += static_cast<google::protobuf::uint32>(set_vals[i].length());
  }

  // write total size to the buffer
  m_out_stream->WriteVarint32(size);

  // write all lengths and values to the buffer
  for (size_t i = 0; i < set_vals.size(); ++i)
  {
    m_out_stream->WriteVarint64(set_vals[i].length());
    m_out_stream->WriteString(set_vals[i]);
  }
}

void Row_builder::add_bit_field(const char * const value, size_t length,
  const CHARSET_INFO * const valuecs)
{
  ADD_FIELD_HEADER();
  DBUG_ASSERT(length <= 8);

  google::protobuf::uint64 binary_value = 0;
  for (size_t i = 0; i < length; ++i)
  {
    binary_value += ((static_cast<google::protobuf::uint64>(value[i]) & 0xff) << ((length - i - 1) * 8));
  }
  m_out_stream->WriteVarint32(CodedOutputStream::VarintSize64(binary_value));
  m_out_stream->WriteVarint64(binary_value);
}
