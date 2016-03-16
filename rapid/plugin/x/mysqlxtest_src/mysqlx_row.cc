/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "mysqlx_row.h"
#include "ngs_common/xdatetime.h"
#include "ngs_common/xdecimal.h"
#include "ngs_common/protocol_protobuf.h"

#include <iostream>
#include <string>
#include <limits>
#include <algorithm>
#include <sstream>
using namespace mysqlx;

int64_t Row_decoder::s64_from_buffer(const std::string& buffer)
{
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint64 value;
  bool ret = input_buffer.ReadVarint64(&value);
  if (!ret)
  {
    throw std::invalid_argument("error reading value");
  }

  return google::protobuf::internal::WireFormatLite::ZigZagDecode64(value);
}

uint64_t Row_decoder::u64_from_buffer(const std::string& buffer)
{
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint64 value;
  bool ret = input_buffer.ReadVarint64(&value);
  if (!ret)
  {
    throw std::invalid_argument("error reading value");
  }

  return value;
}

const char *Row_decoder::string_from_buffer(const std::string& buffer, size_t &rlength)
{
  /*Last byte contains trailing '\0' that we want to skip here*/
  rlength = buffer.length() - 1;
  return &buffer[0];
}

void Row_decoder::set_from_buffer(const std::string& buffer, std::set<std::string>& result)
{
  result.clear();

  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);
  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint64 len;
  std::string elem;
  bool has_next = true;
  while (true)
  {
    has_next = input_buffer.ReadVarint64(&len);
    if (has_next && len > 0)
    {
      bool ok = input_buffer.ReadString(&elem, static_cast<int>(len));
      if (!ok)
      {
        if (result.empty() && (0x01 == len))
        {
          /*special case for empty set*/
          break;
        }
        else
          throw std::invalid_argument("error reading value");
      }
      result.insert(elem);
    }
    else
      break;
  }
}

std::string Row_decoder::set_from_buffer_as_str(const std::string& buffer)
{
  std::string result;

  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);
  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint64 len;
  std::string elem;
  bool has_next = true;
  while (true)
  {
    has_next = input_buffer.ReadVarint64(&len);
    if (has_next && len > 0)
    {
      bool ok = input_buffer.ReadString(&elem, static_cast<int>(len));
      if (!ok)
      {
        if (result.empty() && (0x01 == len))
        {
          /*special case for empty set*/
          break;
        }
        else
          throw std::invalid_argument("error reading value");
      }
      if (!result.empty())
        result.append(",");
      result.append(elem);
    }
    else
      break;
  }

  return result;
}


float Row_decoder::float_from_buffer(const std::string& buffer)
{
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint32 value;
  bool ret = input_buffer.ReadLittleEndian32(&value);
  if (!ret)
  {
    throw std::invalid_argument("error reading value");
  }

  return google::protobuf::internal::WireFormatLite::DecodeFloat(value);
}


double Row_decoder::double_from_buffer(const std::string& buffer)
{
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint64 value;
  bool ret = input_buffer.ReadLittleEndian64(&value);
  if (!ret)
  {
    throw std::invalid_argument("error reading value");
  }

  return google::protobuf::internal::WireFormatLite::DecodeDouble(value);
}


void Row_decoder::read_required_uint64(
  google::protobuf::io::CodedInputStream& input_buffer,
  google::protobuf::uint64& result
  )
{
  google::protobuf::uint64 uint64;
  bool ret = input_buffer.ReadVarint64(&uint64);
  if (!ret)
  {
    throw std::invalid_argument("error reading value");
  }
  result = uint64;
}

DateTime Row_decoder::datetime_from_buffer(const std::string& buffer)
{
  google::protobuf::uint64 year, month, day, hour = 0, minutes = 0, seconds = 0, useconds = 0, tmp = 0;
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  read_required_uint64(input_buffer, year);
  read_required_uint64(input_buffer, month);
  read_required_uint64(input_buffer, day);

  bool has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  hour = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  minutes = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  seconds = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  useconds = tmp;

RETURN:
  return DateTime(
    static_cast<uint16_t>(year),
    static_cast<uint8_t>(month),
    static_cast<uint8_t>(day),
    static_cast<uint8_t>(hour),
    static_cast<uint8_t>(minutes),
    static_cast<uint8_t>(seconds),
    static_cast<uint32_t>(useconds));
}


Time Row_decoder::time_from_buffer(const std::string& buffer)
{
  google::protobuf::uint64 hour = 0, minutes = 0, seconds = 0, useconds = 0, tmp = 0;
  std::string& _buf = const_cast<std::string&>(buffer);
  google::protobuf::uint8* field_buff = reinterpret_cast<google::protobuf::uint8*>(&_buf[0]);

  google::protobuf::io::CodedInputStream input_buffer(field_buff, static_cast<int>(buffer.length()));

  google::protobuf::uint8 sign;
  input_buffer.ReadRaw(&sign, 1);
  bool has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  hour = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  minutes = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  seconds = tmp;
  has_more = input_buffer.ReadVarint64(&tmp);
  if (!has_more) goto RETURN;
  useconds = tmp;

RETURN:
  return Time((sign != 0x00),
    static_cast<uint32_t>(hour),
    static_cast<uint8_t>(minutes),
    static_cast<uint8_t>(seconds),
    static_cast<uint32_t>(useconds));
}

Decimal Row_decoder::decimal_from_buffer(const std::string& buffer)
{
  const std::string& _buf = buffer;
  Decimal dec = Decimal::from_bytes(_buf);

  return dec;
}


//--------------------------------------------------------------
