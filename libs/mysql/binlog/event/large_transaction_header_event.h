/* Copyright (c) 2026, Oracle and/or its affiliates.

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

/**
  @file large_transaction_header_event.h

  @brief Deserialization of the Large_transaction_header
         event. All serialization logic lives in the server class under
         sql/log_event.*
*/

#ifndef MYSQL_BINLOG_EVENT_LARGE_TRANSACTION_HEADER_EVENT_H
#define MYSQL_BINLOG_EVENT_LARGE_TRANSACTION_HEADER_EVENT_H

#include <cstdint>
#include <string>

#include "mysql/binlog/event/binlog_event.h"

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event {

/**
  @class Large_transaction_header_event

  Event needed for the large transaction optimization. It serves two
  purposes:

  1. It records the offset, type and body of the transaction's terminating
     event, so that binary log recovery can verify that event and seek
     directly past the transaction body instead of scanning it.
  2. Its variable-length padding fills the file's reserved header region
     exactly, so the transaction body starts at the offset that was
     assumed while events were being spilled.

  Always written with the LOG_EVENT_IGNORABLE_F flag set: replicas and
  binlog tools that do not recognize the type skip it.

  @section Large_transaction_header_event_binary_format
  Binary Format

  The post-header is empty. The Body has the following components:

  <table>
  <caption>Body for Large_transaction_header_event</caption>
  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>version</td>
    <td>1 byte unsigned integer</td>
    <td>Event format version; currently 1. Retained for future
        extensibility.</td>
  </tr>
  <tr>
    <td>terminating_event_offset</td>
    <td>8 byte unsigned little-endian integer</td>
    <td>Offset, within this binary log file, of the transaction's
        terminating event.</td>
  </tr>
  <tr>
    <td>terminating_event_type</td>
    <td>1 byte unsigned integer</td>
    <td>Binary log event type of the terminating event.</td>
  </tr>
  <tr>
    <td>terminating_event_body_length</td>
    <td>1 byte unsigned integer</td>
    <td>Length of terminating_event_body.</td>
  </tr>
  <tr>
    <td>terminating_event_body</td>
    <td>terminating_event_body_length bytes</td>
    <td>Copy of the terminating event's body, the bytes between its common
        header and its checksum: the internal XID of an XID_EVENT, or the
        one-phase flag and the external XID of an XA_PREPARE_LOG_EVENT; empty
        for any other event type. It identifies the transaction the
        terminating event ends.</td>
  </tr>
  <tr>
    <td>padding</td>
    <td>variable-length byte sequence</td>
    <td>Filler occupying the remainder of the reserved header region;
        contents are undefined and ignored on read.</td>
  </tr>
  </table>
*/
class Large_transaction_header_event : public Binary_log_event {
 public:
  /** Current version of the event's body format. The version field is
      retained for future extensibility; only this version exists today. */
  static constexpr uint8_t kVersion = 1;
  /** Body bytes before the terminating event's body and the padding: version
      (1) + offset (8) + event type (1) + terminating event body length (1). */
  static constexpr size_t kFixedBodyLength = 1 + 8 + 1 + 1;
  /** Longest terminating event body: an XA_PREPARE_LOG_EVENT's one-phase flag
      (1), format ID, gtrid and bqual lengths (4 each) and XID data (128). */
  static constexpr size_t kMaxTerminatingEventBodyLength = 1 + 4 + 4 + 4 + 128;

  /**
    Deserializing constructor.

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Large_transaction_header_event(const char *buf,
                                 const Format_description_event *fde);

  /**
    Creates an event with the given terminating-event metadata and
    padding size (used by the server when writing a promoted binary log
    file's header).

    @param terminating_event_offset  Offset of the transaction's
                                     terminating event in the file.
    @param terminating_event_type    Type code of the transaction's
                                     terminating event in the file.
    @param terminating_event_body    The terminating event's body, at most
                                     kMaxTerminatingEventBodyLength bytes.
    @param padding_size              Number of filler bytes to occupy the
                                     remainder of the reserved region.
  */
  Large_transaction_header_event(uint64_t terminating_event_offset,
                                 uint8_t terminating_event_type,
                                 std::string terminating_event_body,
                                 uint64_t padding_size);

  ~Large_transaction_header_event() override = default;

  /**
    @return Format version of this event's body, so a later server can tell
            which fields are present.
  */
  uint8_t get_version() const { return m_version; }

  /**
    @return Byte offset, from the start of the file, at which the
            transaction's terminating event begins.
  */
  uint64_t get_terminating_event_offset() const {
    return m_terminating_event_offset;
  }

  /** @return Type code of the transaction's terminating event. */
  uint8_t get_terminating_event_type() const {
    return m_terminating_event_type;
  }

  /**
    @return The terminating event's body (between its common header and its
            checksum), which identifies the transaction.
  */
  const std::string &get_terminating_event_body() const {
    return m_terminating_event_body;
  }

  /**
    @return Number of filler bytes this event carries in order to occupy the
            remainder of the file's reserved header region exactly.
  */
  uint64_t get_padding_size() const { return m_padding_size; }

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &info) override;
  void print_long_info(std::ostream &info) override;
#endif

 protected:
  /** Body format version read from (or to be written to) the wire. */
  uint8_t m_version{kVersion};
  /** Offset of the transaction's terminating event in this file. */
  uint64_t m_terminating_event_offset{0};
  /** Type code of the terminating event. */
  uint8_t m_terminating_event_type{0};
  /** Body of the terminating event. */
  std::string m_terminating_event_body;
  /** Number of filler bytes following the fixed body fields. */
  uint64_t m_padding_size{0};
};

}  // namespace mysql::binlog::event

/// @}

#endif  // MYSQL_BINLOG_EVENT_LARGE_TRANSACTION_HEADER_EVENT_H
