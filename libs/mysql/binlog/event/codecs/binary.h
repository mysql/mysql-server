/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_BINLOG_EVENT_CODECS_BINARY_H
#define MYSQL_BINLOG_EVENT_CODECS_BINARY_H

#include "mysql/binlog/event/binary_log.h"
#include "mysql/binlog/event/codecs/base.h"

namespace mysql::binlog::event::codecs::binary {

/**
  This is the abstract and base class for binary log BINARY codecs.
 */
class Base_codec : public mysql::binlog::event::codecs::Codec {
 protected:
  Event_reader *m_reader = nullptr;
  inline Event_reader &reader() { return *m_reader; }

 public:
  std::pair<std::size_t, bool> decode(const unsigned char *from,
                                      std::size_t size,
                                      Binary_log_event &to) const override = 0;
  std::pair<std::size_t, bool> encode(const Binary_log_event &from,
                                      unsigned char *to,
                                      std::size_t size) const override = 0;

  Base_codec() = default;

  // copy-move semantics
  Base_codec(Base_codec &&) noexcept = default;
  Base_codec &operator=(Base_codec &&) noexcept = default;
  Base_codec(const Base_codec &) noexcept = default;
  Base_codec &operator=(const Base_codec &) = default;

  ~Base_codec() override = default;
};

/**
  Binary codec for the transaction payload log event.
 */
class Transaction_payload : public Base_codec {
 public:
  /**
    The on-the-wire fields
   */
  enum fields {
    /** Marks the end of the payload header. */
    OTW_PAYLOAD_HEADER_END_MARK = 0,

    /** The payload field */
    OTW_PAYLOAD_SIZE_FIELD = 1,

    /** The compression type field */
    OTW_PAYLOAD_COMPRESSION_TYPE_FIELD = 2,

    /** The uncompressed size field */
    OTW_PAYLOAD_UNCOMPRESSED_SIZE_FIELD = 3,

    /** Other fields are appended here. */
  };

  Transaction_payload() = default;
  /**
     This member function shall decode the contents of the buffer provided and
     fill in the event referenced. Note that the event provided needs to be of
     type TRANSACTION_PAYLOAD_EVENT.

     @param from the buffer to decode
     @param size the size of the buffer to decode.
     @param to the event to store the decoded information into.

     @return a pair containing the amount of bytes decoded and whether there was
             an error or not. False if no error, true otherwise.
   */
  std::pair<std::size_t, bool> decode(const unsigned char *from,
                                      std::size_t size,
                                      Binary_log_event &to) const override;

  /**
     This member function shall encode the contents of the event referenced and
     store the result in the buffer provided. Note that the event referenced
     needs to be of type TRANSACTION_PAYLOAD_EVENT.

     @param from the event to encode.
     @param to the buffer where to store the encoded event.
     @param size the size of the buffer.

     @return a pair containing the amount of bytes encoded and whether there was
             an error or not.
  */
  std::pair<std::size_t, bool> encode(const Binary_log_event &from,
                                      unsigned char *to,
                                      std::size_t size) const override;
};

/**
  Binary codec for the heartbeat log event.
 */
class Heartbeat : public Base_codec {
 public:
  /**
    The on-the-wire fields
   */
  enum fields {
    /** Marks the end of the fields. */
    OTW_HB_HEADER_END_MARK = 0,

    /** The log file name */
    OTW_HB_LOG_FILENAME_FIELD = 1,

    /** The log position field */
    OTW_HB_LOG_POSITION_FIELD = 2,

    /** Other fields are appended here. */
  };

  Heartbeat() {}
  /**
     This member function shall decode the contents of the buffer provided and
     fill in the event referenced. Note that the event provided needs to be of
     type HEARTBEAT_EVENT_V2.

     @param from the buffer to decode
     @param size the size of the buffer to decode.
     @param to the event to store the decoded information into.

     @return a pair containing the amount of bytes decoded and whether there was
             an error or not. False if no error, true otherwise.
   */
  std::pair<std::size_t, bool> decode(const unsigned char *from,
                                      std::size_t size,
                                      Binary_log_event &to) const override;

  /**
     This member function shall encode the contents of the event referenced and
     store the result in the buffer provided. Note that the event referenced
     needs to be of type HEARTBEAT_EVENT_V2.

     @param from the event to encode.
     @param to the buffer where to store the encoded event.
     @param size the size of the buffer.

     @return a pair containing the amount of bytes encoded and whether there was
             an error or not.
  */
  std::pair<std::size_t, bool> encode(const Binary_log_event &from,
                                      unsigned char *to,
                                      std::size_t size) const override;
};

}  // namespace mysql::binlog::event::codecs::binary

namespace [[deprecated]] binary_log {
namespace [[deprecated]] codecs {
namespace [[deprecated]] binary {
using namespace mysql::binlog::event::codecs::binary;
}  // namespace binary
}  // namespace codecs
}  // namespace binary_log

#endif  // MYSQL_BINLOG_EVENT_CODECS_BINARY_H
