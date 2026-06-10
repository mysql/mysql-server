/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

/// This was an experiment only. Do not use it.
///
/// Problems:
///
/// - This uses a non-standard serialization format for Gtid sets. This format
///   is inefficient, and a decoder cannot distingiush the format from other
///   formats.
///
/// - It has the function prototypes of a stream, but does not follow stream
///   idioms; in particular, it reads until the end of the stream.
///
/// - It does not sanity-check the input size; large enough input will make it
///   over-use memory.
///
/// - It inherits from std::basic_stringstream, which does not have a virtual
///   destructor. This should not even be a new class; we should just have
///   overloaded the operators on std::stringstream should have been overloaded.
///
/// - It duplicates the input, and uses a quadratic-time algorithm to do so.
///
/// - Not all error conditions have been tested.
///
/// The class also does not provide any new functionality. Use
/// `mysql::strconv::encode(Gtid_set)` and
/// `mysql::strconv::decode(Gtid_set)` instead.
///
/// It may be used by third parties, so we keep it until it has been deprecated
/// during a major version.
class [[deprecated("This class will be removed in the future.")]] stringstream
    : public std::basic_stringstream<char> {
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