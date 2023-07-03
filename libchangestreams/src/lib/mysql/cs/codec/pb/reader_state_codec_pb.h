/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef CS_READER_CODEC_PROTOBUF_EXAMPLE_INCLUDED
#define CS_READER_CODEC_PROTOBUF_EXAMPLE_INCLUDED

#include <string>
#include "libchangestreams/include/mysql/cs/reader/state.h"

namespace cs::reader::codec::pb::example {

/**
 * @brief A stringstream class that encodes a State object using
 * according to the protobuf specification.
 *
 */
class stringstream : public std::basic_stringstream<char> {
 public:
  stringstream() = default;
  virtual ~stringstream() = default;

  /**
   * @brief Encodes the given state and puts the outcome on the given
   * buffer.
   *
   * @param to_encode_from The state to encode.
   * @return the reference to this stringstream.
   */
  stringstream &operator<<(cs::reader::State &to_encode_from);

  /**
   * @brief Decodes the state from the buffer in this stream and stores
   * it in the given state object.
   *
   * @param to_decode_into the state that is to be filled with the data decoded.
   * @return the reference to this stringstream.
   */
  stringstream &operator>>(cs::reader::State &to_decode_into);
};

}  // namespace cs::reader::codec::pb::example

#endif