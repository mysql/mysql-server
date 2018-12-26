/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_BASE_PACKET_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_BASE_PACKET_INCLUDED

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "constants.h"
#include "harness_assert.h"

// GCC 4.8.4 requires all classes to be forward-declared before being used with
// "friend class <friendee>", if they're in a different namespace than the
// friender
#ifdef FRIEND_TEST
#include "mysqlrouter/utils.h"  // DECLARE_TEST
DECLARE_TEST(HandshakeResponseParseTest, server_does_not_support_PROTOCOL_41);
DECLARE_TEST(HandshakeResponseParseTest, no_PROTOCOL_41);
DECLARE_TEST(HandshakeResponseParseTest, bad_payload_length);
DECLARE_TEST(HandshakeResponseParseTest, bad_seq_number);
DECLARE_TEST(HandshakeResponseParseTest, max_packet_size);
DECLARE_TEST(HandshakeResponseParseTest, character_set);
DECLARE_TEST(HandshakeResponseParseTest, reserved);
DECLARE_TEST(HandshakeResponseParseTest, username);
DECLARE_TEST(HandshakeResponseParseTest, auth_response);
DECLARE_TEST(HandshakeResponseParseTest, database);
DECLARE_TEST(HandshakeResponseParseTest, auth_plugin);
DECLARE_TEST(HandshakeResponseParseTest, connection_attrs);
DECLARE_TEST(HandshakeResponseParseTest, all);
#endif

namespace mysql_protocol {

/** @class Packet
 * @brief Interface to MySQL packets
 *
 * This class is the base class for all the types of MySQL packets
 * such as ErrorPacket and HandshakeResponsePacket.
 *
 */
class MYSQL_PROTOCOL_API Packet : public std::vector<uint8_t> {
  /** @note This class exposes several types of methods for data manipulation.
   *
   * Packet buffer operations, they work like standard stream operations:
   *   seek()/tell()  - set/get buffer position
   *   write_*()      - write data at current buffer position
   *   read_*()       - read data at current buffer position
   *
   * Packet buffer operations with specified position:
   *   read_*_from()  - read data from specified buffer position
   *
   * Packet field setters/getters:
   *   get_*()   - return fields from this class (packet needs to be parsed
   * first) set_*()   - set fields in this class
   */

 public:
  using vector_t = std::vector<uint8_t>;

  ////////////////////////////////////////////////////////////////////////////////
  // constructors, destructors, assignment operators
  ////////////////////////////////////////////////////////////////////////////////

  /** @brief Header length of packets */
  static const unsigned int kHeaderSize{4};

  /** @brief Default of max_allowed_packet defined by the MySQL Server (2^30) */
  static const unsigned int kMaxAllowedSize{1073741824};

  /** @brief Constructor */
  Packet() : Packet(0, Capabilities::ALL_ZEROS) {}

  /** @overload
   *
   * This constructor takes a buffer, stores the data, and tries to get
   * information out of the buffer.
   *
   * When buffer is 4 or bigger, the payload size and sequence ID of the packet
   * is read from the first 4 bytes (packet header).
   *
   * When allow_partial is false, the payload size is not enforced and buffer
   * can be smaller than payload size given in the header. Allow partial packets
   * can be useful when all you need is to parse the heade
   *
   * @param buffer Vector of uint8_t
   * @param allow_partial Whether to allow buffers which have incomplete payload
   */
  explicit Packet(const vector_t &buffer, bool allow_partial = false)
      : Packet(buffer, Capabilities::ALL_ZEROS, allow_partial) {}

  /** @overload
   *
   * @param buffer Vector of uint8_t
   * @param capabilities Server or Client capability flags
   * @param allow_partial Whether to allow buffers which have incomplete payload
   */
  Packet(const vector_t &buffer, Capabilities::Flags capabilities,
         bool allow_partial = false);

  /** @overload
   *
   * @param sequence_id Sequence ID of MySQL packet
   */
  explicit Packet(uint8_t sequence_id)
      : Packet(sequence_id, Capabilities::ALL_ZEROS) {}

  /** @overload
   *
   * @param sequence_id Sequence ID of MySQL packet
   * @param capabilities Server or Client capability flags
   */
  Packet(uint8_t sequence_id, Capabilities::Flags capabilities)
      : vector(),
        sequence_id_(sequence_id),
        payload_size_(0),
        capability_flags_(capabilities) {}

  /** @overload */
  Packet(std::initializer_list<uint8_t> ilist);

  /** @brief Destructor */
  virtual ~Packet() {}

  /** @brief Copy Constructor */
  Packet(const Packet &) = default;

  /** @brief Move Constructor */
  Packet(Packet &&other)
      : vector(std::move(other)),
        sequence_id_(other.get_sequence_id()),
        payload_size_(other.get_payload_size()),
        capability_flags_(other.get_capabilities()) {
    other.sequence_id_ = 0;
    other.capability_flags_ = Capabilities::ALL_ZEROS;
    other.payload_size_ = 0;
  }

  /** @brief Copy Assignment */
  Packet &operator=(const Packet &) = default;

  /** @brief Move Assigment */
  Packet &operator=(Packet &&other) {
    swap(other);
    sequence_id_ = other.sequence_id_;
    payload_size_ = other.payload_size_;
    capability_flags_ = other.get_capabilities();
    other.sequence_id_ = 0;
    other.capability_flags_ = Capabilities::ALL_ZEROS;
    other.payload_size_ = 0;
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // packet buffer operations: stream interface
  ////////////////////////////////////////////////////////////////////////////////

  /** @brief Sets current read/write position used by read_*()/write_*() calls
   */
  void seek(size_t position) const {
    if (position > size()) throw std::range_error("seek past EOF");
    position_ = position;
  }

  /** @brief Returns current read/write position used by read_*()/write_*()
   * calls */
  size_t tell() const { return position_; }

  /** @brief Gets an integral from given packet
   *
   * Gets an integral from packet buffer at the current position and advances it
   * by the length of the read. The size of the integral is deduced from the
   * give type but can be overwritten using the size parameter.
   *
   * Supported are integral of 1, 2, 3, 4, or 8 bytes. To retrieve an 24 bit
   * integral it is necessary to use a 32-bit integral type and
   * provided the size, for example:
   *
   *     auto id = Packet::read_int_from<uint32_t>(buffer, 0, 3);
   *
   * In MySQL packets, integrals are stored using little-endian format.
   *
   * @param length size of the integer to parse
   * @return integer type
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF
   *
   * @see read_int_from()
   */
  template <typename Type,
            typename = std::enable_if<std::is_integral<Type>::value>>
  Type read_int(size_t length = sizeof(Type)) const {
    Type res = read_int_from<Type>(position_,
                                   length);  // throws range_error/runtime_error
    position_ += length;
    return res;
  }

  /** @brief Gets a length encoded integer from given packet
   *
   * Gets a length encoded integer from packet buffer at the current position
   * and advances it by the length of the read. Function also returns the length
   * of the parsed integer token (you will need to advance your read position by
   * this value to get to next field in the packet)
   *
   * @return uint64_t
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF,
   *         std::runtime_error on bad first byte (which determines int length)
   *         (strong exception safety guarrantee)
   *
   * @see read_lenenc_uint_from()
   */
  uint64_t read_lenenc_uint() const;

  /** @brief Gets raw bytes from packet
   *
   * Gets raw byes from packet buffer at the current position and advances it
   * by the length of the read.
   *
   * @param length Number of bytes to read
   * @return std::vector<uint8_t>
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF
   *         (strong exception safety guarrantee)
   *
   * @see read_bytes_from()
   */
  std::vector<uint8_t> read_bytes(size_t length) const;

  /** @brief Gets raw bytes from packet using length encoded size
   *
   * Gets raw bytes with length encoded size from packet buffer at the current
   * position and advances it by the length of the read.
   *
   * @return std::vector<uint8_t>
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF,
   *         std::runtime_error on bad first byte (which determines int length)
   *         (strong exception safety guarrantee)
   *
   * @see read_lenenc_bytes_from()
   */
  std::vector<uint8_t> read_lenenc_bytes() const;

  /** @brief Gets zero-terminated string from packet
   *
   * Gets zero-terminated string from packet buffer at the current position and
   * advances it by the length of the read.
   *
   * @return std::string
   *
   * @see read_string_nul_from()
   */
  std::string read_string_nul() const;

  /** @brief Gets raw bytes from packet from position until EOF
   *
   * Gets raw bytes from packet buffer at the current position and advances it
   * by the length of the read.
   *
   * @return std::vector<uint8_t>
   *
   * @throws std::range_error (std::runtime_error) on start beyond EOF,
   *         std::runtime_error on zero-terminator not found
   *         (strong exception safety guarrantee)
   *
   * @see read_bytes_eof_from()
   */
  std::vector<uint8_t> read_bytes_eof() const;

  /** @brief Packs and adds an integral to the buffer
   *
   * Packs and adds an integral to the given buffer.
   *
   * @param value Integral to add to the packet
   * @param length Size of the integral (default: size of integral)
   *
   */
  template <class T, typename = std::enable_if<std::is_integral<T>::value>>
  void write_int(T value, size_t length = sizeof(T)) {
    reserve(size() + length);
    while (length-- > 0) {
      // Assignment to temporary variable `b` prevents too aggressive inlining
      // optimization in some compilers (e.g. GCC 4.9.2 on Solaris, with -O2).
      // Without it, `value` wasn't getting updated before push_back() under
      // certain conditions, and resulted in filling packet's buffer with
      // invalid data.
      uint8_t b = static_cast<uint8_t>(value);
      update_or_append(b);
      value = static_cast<T>(value >> CHAR_BIT);
    }
  }

  /** @brief Packs and adds a length-encoded integral to the buffer
   *
   * Packs and adds a length-encoded integral to the given buffer.
   *
   * @param value Integral to add to the packet
   * @return Size of the encoded integral (one of: 1, 3, 4 or 9 bytes)
   */
  size_t write_lenenc_uint(uint64_t value);

  /** @brief Adds bytes to the given packet
   *
   * Adds the given bytes to the buffer.
   *
   * @param bytes Bytes to add to the packet
   *
   */
  void write_bytes(const Packet::vector_t &bytes) {
    write_bytes_impl(bytes.data(), bytes.size());
  }

  /** @brief Adds a string to the given packet
   *
   * Adds the given string to the buffer. It does not append a zero-terminator
   * after this string.
   *
   * @param str String to add to the packet
   */
  void write_string(const std::string &str) {
    write_bytes_impl(reinterpret_cast<const uint8_t *>(str.data()), str.size());
  }  //               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ std::string contains
     //               signed chars

  /** @brief Adds bytes at the end of the buffer
   *
   * Appends a byte many times to the packet buffer at EOF and advances current
   * position by the length of the write (so that it points to EOF once again)
   *
   * @param count number of times to append the byte
   * @param byte byte to append
   *
   * @throws std::range_error (std::runtime_error) if current position is not
   *         currently at EOF
   */
  void append_bytes(size_t count, uint8_t byte);

  ////////////////////////////////////////////////////////////////////////////////
  // packet buffer operations: direct position interface
  ////////////////////////////////////////////////////////////////////////////////

  /** @brief Gets an integral from given packet
   *
   * Gets an integral from a buffer at the given position. The size of the
   * integral is deduced from the give type but can be overwritten using
   * the size parameter.
   *
   * Supported are integral of 1, 2, 3, 4, or 8 bytes. To retrieve an 24 bit
   * integral it is necessary to use a 32-bit integral type and
   * provided the size, for example:
   *
   *     auto id = Packet::read_int_from<uint32_t>(buffer, 0, 3);
   *
   * In MySQL packets, integrals are stored using little-endian format.
   *
   * @param position Position where to start reading
   * @param length size of the integer to parse
   * @return integer type
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF
   */
  template <typename Type,
            typename = std::enable_if<std::is_integral<Type>::value>>
  Type read_int_from(size_t position, size_t length = sizeof(Type)) const {
    harness_assert((length >= 1 && length <= 4) || length == 8);
    if (position + length > size())
      throw std::range_error("start or end beyond EOF");

    if (length == 1) {
      return static_cast<Type>((*this)[position]);
    }

    uint64_t result = 0;
    auto it = begin() + static_cast<long>(position + length);
    while (length-- > 0) {
      result <<= 8;
      result |= *--it;
    }

    return static_cast<Type>(result);
  }

  /** @brief Gets a length encoded integer from given packet
   *
   * Function also returns the length of the parsed integer token (you will need
   * to advance your read position by this value to get to next field in the
   * packet)
   *
   * @param position Position where to start reading
   * @return std::pair<uint64_t, size_t>
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF,
   *         std::runtime_error on bad first byte (which determines int length)
   *         (strong exception safety guarrantee)
   */
  std::pair<uint64_t, size_t> read_lenenc_uint_from(size_t position) const;

  /** @brief Gets a string from packet
   *
   * Gets a string from the given buffer at the given position. When size is
   * not given, we read until the end of the buffer.
   * When nil byte is found before we reach the requested size, the string will
   * be not be size long (if size is not 0).
   *
   * When pos is greater than the size of the buffer, an empty string is
   * returned.
   *
   * @param position Position from which to start reading
   * @param length Length of the string to read (default 0)
   * @return std::string
   */
  std::string read_string_from(unsigned long position,
                               unsigned long length = UINT_MAX) const;

  /** @brief Gets a zero-terminated string from packet
   *
   * @param position Position from which to start reading
   * @return std::string
   *
   * @throws std::range_error (std::runtime_error) on start beyond EOF,
   *         std::runtime_error on zero-terminator not found
   *         (strong exception safety guarrantee)
   */
  std::string read_string_nul_from(size_t position) const;

  /** @brief Gets raw bytes from packet
   *
   * @param position Position from which to start reading
   * @param length Number of bytes to read
   * @return std::vector<uint8_t>
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF
   *         (strong exception safety guarrantee)
   */
  std::vector<uint8_t> read_bytes_from(size_t position, size_t length) const;

  /** @brief Gets raw bytes from packet using length encoded size
   *
   * Function also returns the length of the parsed bytes token (you will need
   * to advance your read position by this value to get to next field in the
   * packet)
   *
   * @param position Position from which to start reading
   * @return std::pair<std::vector<uint8_t>, size_t>
   *
   * @throws std::range_error (std::runtime_error) on start or end beyond EOF,
   *         std::runtime_error on bad first byte (which determines int length)
   *         (strong exception safety guarrantee)
   */
  std::pair<std::vector<uint8_t>, size_t> read_lenenc_bytes_from(
      size_t position) const;

  /** @brief Gets raw bytes from packet from position until EOF
   *
   * @param position Position from which to start reading
   * @return std::vector<uint8_t>
   *
   * @throws std::range_error (std::runtime_error) on start beyond EOF,
   *         std::runtime_error on zero-terminator not found
   *         (strong exception safety guarrantee)
   */
  std::vector<uint8_t> read_bytes_eof_from(size_t position) const;

  ////////////////////////////////////////////////////////////////////////////////
  // packet buffer operations: static method interface
  ////////////////////////////////////////////////////////////////////////////////

  /** @brief Gets the packet sequence ID from supplied buffer
   *
   * @param header 4-byte header
   * @return uint8_t
   */
  static uint8_t read_sequence_id(const uint8_t header[4]) noexcept {
    return header[3];
  }

  /** @brief Gets the payload size from supplied buffer
   *
   * @param header 4-byte header
   * @return uint32_t payload size of the packet
   */
  static uint32_t read_payload_size(const uint8_t header[4]) noexcept {
    return header[0] + (header[1] << 8) + (header[2] << 16);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // packet field setter/getter interface
  ////////////////////////////////////////////////////////////////////////////////

  /** @brief Returns header length of MySQL Protocol packet
   *
   * @return header length (4 bytes)
   */
  static constexpr size_t get_header_length() noexcept { return 4; }

  /** @brief Gets the packet sequence ID
   *
   * @return uint8_t
   */
  uint8_t get_sequence_id() const noexcept { return sequence_id_; }

  /** @brief Sets the packet sequence ID
   *
   * @param id Sequence ID of the packet
   */
  void set_sequence_id(uint8_t id) noexcept { sequence_id_ = id; }

  /** @brief Gets server/client capabilities
   *
   * @return Capabilities
   */
  Capabilities::Flags get_capabilities() const noexcept {
    return capability_flags_;
  }

  /** @brief Gets the payload size
   *
   * Returns the payload size parsed retrieved from the packet header.
   *
   * @return uint32_t
   */
  uint32_t get_payload_size() const noexcept { return payload_size_; }

 protected:
  /** @brief Resets packet
   *
   * Resets the packet and sets the sequence id.
   */
  void reset() { this->assign({0x0, 0x0, 0x0, sequence_id_}); }

  /** @brief Updates payload size in packet header
   *
   * Updates the size of the payload storing it in the first 3 bytes
   * of the packet. This method is called after preparing the packet.
   */
  void update_packet_size();

  /** @brief MySQL packet sequence ID */
  uint8_t sequence_id_;

  /** @brief Payload of the packet */
  std::vector<uint8_t> payload_;

  /** @brief Payload size */
  uint32_t payload_size_;

  /** @brief Capability flags */
  Capabilities::Flags capability_flags_;

  /** @brief read/write position for stream operations */
  mutable size_t position_;

 private:
  void parse_header(bool allow_partial = false);

  void write_bytes_impl(const unsigned char *bytes, size_t length);

  static inline void update_or_append(std::vector<uint8_t> &vec,
                                      size_t &position, uint8_t value) {
    harness_assert(position <= vec.size());  // allow write before or at EOF

    if (position < vec.size())
      vec[position] = value;
    else
      vec.push_back(value);

    position++;
  }

  inline void update_or_append(size_t &position, uint8_t value) {
    update_or_append(*this, position, value);
  }

  inline void update_or_append(uint8_t value) {
    update_or_append(position_, value);
  }

#ifdef FRIEND_TEST
  FRIEND_TEST(::HandshakeResponseParseTest,
              server_does_not_support_PROTOCOL_41);
  FRIEND_TEST(::HandshakeResponseParseTest, no_PROTOCOL_41);
  FRIEND_TEST(::HandshakeResponseParseTest, bad_payload_length);
  FRIEND_TEST(::HandshakeResponseParseTest, bad_seq_number);
  FRIEND_TEST(::HandshakeResponseParseTest, max_packet_size);
  FRIEND_TEST(::HandshakeResponseParseTest, character_set);
  FRIEND_TEST(::HandshakeResponseParseTest, reserved);
  FRIEND_TEST(::HandshakeResponseParseTest, username);
  FRIEND_TEST(::HandshakeResponseParseTest, auth_response);
  FRIEND_TEST(::HandshakeResponseParseTest, database);
  FRIEND_TEST(::HandshakeResponseParseTest, auth_plugin);
  FRIEND_TEST(::HandshakeResponseParseTest, connection_attrs);
  FRIEND_TEST(::HandshakeResponseParseTest, all);
#endif
};

}  // namespace mysql_protocol

#endif  // MYSQLROUTER_MYSQLV10_BASE_PACKET_INCLUDED
