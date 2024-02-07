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

#include "mysql/binlog/event/codecs/binary.h"
#include <string>
#include "my_byteorder.h"
#include "mysql_com.h"  // net_store_length, net_length_size

namespace mysql::binlog::event::codecs::binary {

/**
  This is the decoder member function for the TLV encoded set of
  values for the Transaction payload event. Note that the log event
  must have the appropriate type code: TRANSACTION_PAYLOAD_EVENT to
  be compatible with this specific decoder. If it is not, an error
  shall be returned

  @param from the buffer to decode from.
  @param size the buffer size.
  @param to The log event to decode to.

  @return a pair containing the number of bytes decoded and whether
          there was an error or not. False if no error, true
          otherwise.
 */
std::pair<std::size_t, bool> Transaction_payload::decode(
    const unsigned char *from, size_t size, Binary_log_event &to) const {
  BAPI_TRACE;
  Event_reader reader(reinterpret_cast<const char *>(from), size);
  bool have_payload_size = false;
  bool have_compression_type = false;

  // Compute a return status for the function
  auto get_return_status = [&](bool error) {
    return std::make_pair(reader.position(), error);
  };

  // Wrong use of this function.
  if (to.header()->type_code !=
      mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT) {
    BAPI_ASSERT(false);
    return get_return_status(true);
  }

  Transaction_payload_event &ev = down_cast<Transaction_payload_event &>(to);

  while (reader.available_to_read()) {
    uint64_t type{0};
    uint64_t length{0};
    uint64_t value{0};

    // Decode the type of the field.
    type = reader.net_field_length_ll();
    if (reader.get_error()) return get_return_status(true);

    // We have reached the end of the payload data header.
    if (type == OTW_PAYLOAD_HEADER_END_MARK)
      // For success, the function should have read both payload size
      // and compression type, and payload size should not be bigger
      // than the remainder of the event. It's OK to not have
      // uncompressed size, since that is informational only.
      return get_return_status(!have_payload_size || !have_compression_type ||
                               ev.get_payload_size() >
                                   reader.available_to_read());

    // Decode the size of the field.
    length = reader.net_field_length_ll();
    if (reader.get_error()) return get_return_status(true);

    switch (type) {
      case OTW_PAYLOAD_SIZE_FIELD:
        // Decode the payload size.
        value = reader.net_field_length_ll();
        if (reader.get_error()) return get_return_status(true);
        ev.set_payload_size(value);
        have_payload_size = true;
        break;

      case OTW_PAYLOAD_COMPRESSION_TYPE_FIELD:
        // Decode the compression type.
        value = reader.net_field_length_ll();
        if (reader.get_error()) return get_return_status(true);
        if (!mysql::binlog::event::compression::type_is_valid(value))
          return get_return_status(true);
        ev.set_compression_type(
            static_cast<mysql::binlog::event::compression::type>(value));
        have_compression_type = true;
        break;

      case OTW_PAYLOAD_UNCOMPRESSED_SIZE_FIELD:
        // Decode the uncompressed size.
        value = reader.net_field_length_ll();
        if (reader.get_error()) return get_return_status(true);
        ev.set_uncompressed_size(value);
        break;

      default:
        // Ignore unrecognized fields.
        reader.forward(length);
        break;
    }
  }
  // Reached end of event without seeing an END_MARK
  return get_return_status(true);
}

/**
  This is the decoder member function for decoding heartbeats. Note
  that the log event  must have the appropriate type code: HEARTBEAT_LOG_EVENT
  to be compatible with this specific decoder. If it is not, an error shall be
  returned

  @param from the buffer to decode from.
  @param size the buffer size.
  @param to The log event to decode to.

  @return a pair containing the number of bytes decoded and whether
          there was an error or not. False if no error, true
          otherwise.
 */

std::pair<std::size_t, bool> Heartbeat::decode(const unsigned char *from,
                                               size_t size,
                                               Binary_log_event &to) const {
  bool result =
      (to.header()->type_code != mysql::binlog::event::HEARTBEAT_LOG_EVENT_V2);
  Event_reader reader(reinterpret_cast<const char *>(from), size);
  BAPI_ASSERT(!result);
  if (result)
    return std::make_pair<std::size_t, bool &>(reader.position(), result);

  Heartbeat_event_v2 &ev = down_cast<Heartbeat_event_v2 &>(to);

  while (reader.available_to_read()) {
    uint64_t type{0};
    uint64_t length{0};
    uint64_t value{0};

    /* read the type of the field. */
    value = reader.net_field_length_ll();
    if ((result = reader.get_error())) return {reader.position(), true};
    type = value;

    // we have reached the end of the header
    if (type == OTW_HB_HEADER_END_MARK) break;

    /* read the size of the field. */
    value = reader.net_field_length_ll();
    if ((result = reader.get_error())) return {reader.position(), true};
    length = value;

    switch (type) {
      case OTW_HB_LOG_FILENAME_FIELD: {
        // read the string
        std::string name{reader.ptr(), static_cast<size_t>(length)};
        // advance the cursor
        reader.ptr(length);
        if ((result = reader.get_error())) return {reader.position(), true};
        ev.set_log_filename(name);
      } break;

      case OTW_HB_LOG_POSITION_FIELD:
        value = reader.net_field_length_ll();
        if ((result = reader.get_error())) return {reader.position(), true};
        ev.set_log_position(value);
        break;

      /* purecov: begin inspected */
      default:
        /* ignore unrecognized field. */
        reader.go_to(length);
        break;
        /* purecov: end */
    }
  }

  return std::make_pair<std::size_t, bool &>(reader.position(), result);
}

/**
  This is the encoder member function for the heartbeat event. Note
  that the log event must have the appropriate type code: HEARTBEAT_LOG_EVENT
  to be compatible with this specific enccoder. If it is not, an error
  shall be returned

  @param from The log event to decode to.
  @param to the buffer to decode from.
  @param size the buffer size.


  @return a pair containing the number of bytes decoded and whether
          there was an error or not. False if no error, true
          otherwise.
 */
std::pair<std::size_t, bool> Heartbeat::encode(const Binary_log_event &from,
                                               unsigned char *to,
                                               size_t size) const {
  bool result =
      from.header()->type_code != mysql::binlog::event::HEARTBEAT_LOG_EVENT_V2;
  uchar *ptr = to;
  BAPI_ASSERT(!result);
  if (result) return std::make_pair<std::size_t, bool &>(ptr - to, result);

  uint64_t type{0};
  uint64_t length{0};
  const Heartbeat_event_v2 &ev = down_cast<const Heartbeat_event_v2 &>(from);
  const auto filename = ev.get_log_filename();
  const auto position = ev.get_log_position();

  // encode the string in the TLV section
  type = static_cast<uint64_t>(OTW_HB_LOG_FILENAME_FIELD);
  length = filename.size();
  if (ptr + net_length_size(type) + net_length_size(length) + length >=
      to + size)
    return {ptr - to, true};
  ptr = net_store_length(ptr, type);
  ptr = net_store_length(ptr, length);
  memcpy(ptr, filename.c_str(), length);
  ptr = ptr + length;

  // encode the log position in network encoding
  type = static_cast<uint64_t>(OTW_HB_LOG_POSITION_FIELD);
  length = net_length_size(position);
  if (ptr + net_length_size(type) + net_length_size(length) + length >=
      to + size)
    return {ptr - to, true};

  ptr = net_store_length(ptr, type);
  ptr = net_store_length(ptr, length);
  ptr = net_store_length(ptr, position);

  // new fields go here -----------------------

  // write the end of event header marker
  type = static_cast<uint64_t>(OTW_HB_HEADER_END_MARK);
  if (ptr + net_length_size(type) >= to + size)
    return std::make_pair<std::size_t, bool &>(ptr - to, (result = true));
  ptr = net_store_length(ptr, type);

  return std::make_pair<std::size_t, bool &>(ptr - to, result);
}

/**
  This is the encoder member function for the TLV encoded set of
  values for the Transaction payload event. Note that the log event
  must have the appropriate type code: TRANSACTION_PAYLOAD_EVENT to
  be compatible with this specific enccoder. If it is not, an error
  shall be returned

  @param from The log event to decode to.
  @param to the buffer to decode from.
  @param size the buffer size.

  @return a pair containing the number of bytes decoded and whether
          there was an error or not. False if no error, true
          otherwise.
 */
std::pair<std::size_t, bool> Transaction_payload::encode(
    const Binary_log_event &from, unsigned char *to,
    size_t size [[maybe_unused]]) const {
  bool result = from.header()->type_code !=
                mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT;
  uchar *ptr = to;
  BAPI_ASSERT(!result);
  if (result == false) {
    uint64_t type{0};
    uint64_t length{0};
    uint64_t value{0};
    const Transaction_payload_event &ev =
        down_cast<const Transaction_payload_event &>(from);

    // encode the compression type
    type = static_cast<uint64_t>(OTW_PAYLOAD_COMPRESSION_TYPE_FIELD);
    value = static_cast<uint64_t>(ev.get_compression_type());
    length = net_length_size(value);
    ptr = net_store_length(ptr, type);
    ptr = net_store_length(ptr, length);
    ptr = net_store_length(ptr, value);

    // encode uncompressed size
    if (ev.get_compression_type() !=
        mysql::binlog::event::compression::type::NONE) {
      type = static_cast<uint64_t>(OTW_PAYLOAD_UNCOMPRESSED_SIZE_FIELD);
      value = ev.get_uncompressed_size();
      length = net_length_size(value);
      ptr = net_store_length(ptr, type);
      ptr = net_store_length(ptr, length);
      ptr = net_store_length(ptr, value);
    }

    // encode payload size
    type = static_cast<uint64_t>(OTW_PAYLOAD_SIZE_FIELD);
    value = ev.get_payload_size();
    length = net_length_size(value);
    ptr = net_store_length(ptr, type);
    ptr = net_store_length(ptr, length);
    ptr = net_store_length(ptr, value);

    // write the end of event header marker
    type = static_cast<uint64_t>(OTW_PAYLOAD_HEADER_END_MARK);
    ptr = net_store_length(ptr, type);
  }

  return std::make_pair<std::size_t, bool &>(ptr - to, result);
}

}  // namespace mysql::binlog::event::codecs::binary
