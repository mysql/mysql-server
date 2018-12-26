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

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

#include <cassert>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace mysql_protocol {

HandshakeResponsePacket::HandshakeResponsePacket(
    uint8_t sequence_id, const std::vector<unsigned char> &auth_response,
    const std::string &username, const std::string &password,
    const std::string &database, unsigned char char_set,
    const std::string &auth_plugin)
    : Packet(sequence_id),
      username_(username),
      password_(password),
      database_(database),
      char_set_(char_set),
      auth_plugin_(auth_plugin),
      auth_response_(auth_response) {
  prepare_packet();
}

/** @fn HandshakeResponsePacket::prepare_packet()
 *
 * @internal
 * Password is currently not used and 'incorrect' authentication data is
 * being set in this packet (making the packet currently unusable for
 * authentication. This is to satisfy fix for BUG22020088.
 * @endinternal
 */
void HandshakeResponsePacket::prepare_packet() {
  reset();
  position_ = size();

  reserve(size() + sizeof(uint32_t) +  // capabilities
          sizeof(uint32_t) +           // max packet size
          sizeof(uint8_t) +            // character set
          23 +                         // 23 0-byte filler
          username_.size() + 1 +       // username + nul-terminator
          sizeof(uint8_t) + 20 +       // auth-data-len + auth-data
          database_.size() + 1 +       // username + nul-terminator
          auth_plugin_.size() + 1      // auth-plugin + nul-terminator
  );

  // capabilities
  write_int<uint32_t>(kDefaultClientCapabilities.bits());

  // max packet size
  write_int<uint32_t>(kMaxAllowedSize);

  // Character set
  write_int<uint8_t>(char_set_);

  // Filler
  append_bytes(23, 0x0);

  // Username
  if (!username_.empty()) {
    write_string(username_);
  }
  write_int<uint8_t>(0);

  // Auth Data
  write_int<uint8_t>(20);
  append_bytes(20, 0x71);  // 0x71 is fake data; can be anything

  // Database
  if (!database_.empty()) {
    write_string(database_);
  }
  write_int<uint8_t>(0);

  // Authentication plugin name
  write_string(auth_plugin_);
  write_int<uint8_t>(0);

  update_packet_size();
}

////////////////////////////////////////////////////////////////////////////////
//
// HandshakeResponsePacket::Parser320 (unimplemented)
//
////////////////////////////////////////////////////////////////////////////////

/*static*/
bool HandshakeResponsePacket::Parser320::is_protocol320(
    const HandshakeResponsePacket &packet) {
  return !Parser41::is_protocol41(packet);
}

/*virtual*/
void HandshakeResponsePacket::Parser320::parse(
    Capabilities::Flags server_capabilities) /*override*/ {
  (void)server_capabilities;
  throw std::runtime_error(
      "Handshake response packet: Protocol is version 320, which is not "
      "implemented atm");
}

/*virtual*/
void HandshakeResponsePacket::Parser320::debug_dump() const {
  throw std::runtime_error("not implemented");
}

////////////////////////////////////////////////////////////////////////////////
//
// HandshakeResponsePacket::Parser41 (partial implementation - just essentials)
//
////////////////////////////////////////////////////////////////////////////////

/*static*/
bool HandshakeResponsePacket::Parser41::is_protocol41(
    const HandshakeResponsePacket &packet) {
  constexpr size_t kFlagsOffset = 4;  // vvvvvvvvv-- only low 16 bits are needed
  if (packet.size() < kFlagsOffset + sizeof(Capabilities::HalfFlags))
    throw std::runtime_error(
        "HandshakeResponsePacket: tried reading capability flags past EOF");

  Capabilities::Flags flags(
      packet.read_int_from<Capabilities::HalfFlags>(kFlagsOffset));
  return flags.test(Capabilities::PROTOCOL_41);
}

void HandshakeResponsePacket::Parser41::part1_max_packet_size() {
  /**
   * This function implements this part of the specification:
   *
   *   4              max-packet size
   */

  using MaxPacketSize = decltype(packet_.max_packet_size_);

  packet_.max_packet_size_ = packet_.read_int<MaxPacketSize>();
}

void HandshakeResponsePacket::Parser41::part2_character_set() {
  /**
   * This function implements this part of the specification:
   *
   *   1              character set
   */

  using CharSet = decltype(packet_.char_set_);

  packet_.char_set_ = packet_.read_int<CharSet>();
}

void HandshakeResponsePacket::Parser41::part3_reserved() {
  /**
   * This function implements this part of the specification:
   *
   *   string[23]     reserved (all [0])
   */

  constexpr size_t kReservedBytes = 23;
  vector<uint8_t> reserved = packet_.read_bytes(kReservedBytes);

  // proper packet should have all of those set to 0
  if (!std::all_of(reserved.begin(), reserved.end(),
                   [](uint8_t c) { return c == 0; }))
    throw std::runtime_error(
        "Handshake response packet: found non-zero value in reserved 23-byte "
        "field");
}

void HandshakeResponsePacket::Parser41::part4_username() {
  /**
   * This function implements this part of the specification:
   *
   *   string[NUL]    username
   */

  packet_.username_ = packet_.read_string_nul();
}

void HandshakeResponsePacket::Parser41::part5_auth_response() {
  /**
   * This function implements this part of the specification:
   *
   *   if capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA {
   *     lenenc-int     length of auth-response
   *     string[n]      auth-response
   *   } else if capabilities & CLIENT_SECURE_CONNECTION {
   *     1              length of auth-response
   *     string[n]      auth-response
   *   } else {
   *     string[NUL]    auth-response
   *   }
   */

  if (effective_capability_flags_.test(
          Capabilities::PLUGIN_AUTH_LENENC_CLIENT_DATA)) {
    // get auth-response string length
    uint64_t len = packet_.read_lenenc_uint();  // length 0 is a valid value

    // get auth-response string
    packet_.auth_response_ = packet_.read_bytes(len);

  } else if (effective_capability_flags_.test(
                 Capabilities::SECURE_CONNECTION)) {
    // get auth-response string length
    uint64_t len = packet_.read_int<uint8_t>();

    // get auth-response string
    packet_.auth_response_ = packet_.read_bytes(len);

  } else {
    throw std::runtime_error(
        "Handshake response packet: capabilities "
        "PLUGIN_AUTH_LENENC_CLIENT_DATA and SECURE_CONNECTION both missing is "
        "not implemented atm");
  }
}

void HandshakeResponsePacket::Parser41::part6_database() {
  /**
   * This function implements this part of the specification:
   *
   *   if capabilities & CLIENT_CONNECT_WITH_DB {
   *     string[NUL]    database
   *   }
   */

  if (effective_capability_flags_.test(Capabilities::CONNECT_WITH_DB))
    packet_.database_ = packet_.read_string_nul();
}

void HandshakeResponsePacket::Parser41::part7_auth_plugin() {
  /**
   * This function implements this part of the specification:
   *
   *   if capabilities & CLIENT_PLUGIN_AUTH {
   *     string[NUL]    auth plugin name
   *   }
   */

  if (effective_capability_flags_.test(Capabilities::PLUGIN_AUTH))
    packet_.auth_plugin_ = packet_.read_string_nul();
}

void HandshakeResponsePacket::Parser41::part8_connection_attrs() {
  /**
   * This function implements this part of the specification:
   *
   *   if capabilities & CLIENT_CONNECT_ATTRS {
   *     lenenc-int     length of all key-values
   *     lenenc-str     key
   *     lenenc-str     value
   *     if-more data in 'length of all key-values', more keys and value pairs
   *   }
   */

  if (effective_capability_flags_.test(Capabilities::CONNECT_ATTRS))
    throw std::runtime_error(
        "Handshake response packet: capability CONNECT_ATTRS is not "
        "implemented atm");
}

void HandshakeResponsePacket::Parser41::parse(
    Capabilities::Flags server_capabilities) /*override*/ {
  // full packet specification is here:
  // http://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::HandshakeResponse41

  // we only support PROTOCOL_41 for now, so server has to support it
  if (!server_capabilities.test(Capabilities::PROTOCOL_41))
    throw std::runtime_error(
        "Handshake response packet: server not supporting PROTOCOL_41 in not "
        "implemented atm");

  // header
  {
    // header should have already been parsed by Packet::parse_header(), which
    // is called by Packet constructor, so here we just skip it over
    packet_.seek(Packet::get_header_length());

    // correct handshake packet always has seq num = 1
    if (packet_.get_sequence_id() != 1)
      throw std::runtime_error(
          "Handshake response packet: sequence number different than 1");
  }

  // capabilities
  {
    // NOTE: in PROTOCOL_320, capabilities are expressed only in 2 bytes,
    // PROTOCOL_41 uses 4
    packet_.capability_flags_ =
        Capabilities::Flags(packet_.read_int<Capabilities::AllFlags>());

    // see @note in HandshakeResponsePacket ctor
    effective_capability_flags_ =
        packet_.capability_flags_ & server_capabilities;

    // ensure we're indeed parsing PROTOCOL_41
    assert(effective_capability_flags_.test(Capabilities::PROTOCOL_41));
  }

  // parse protocol-defined fields; all part*() throw std::runtime_error (or its
  // derivatives)
  {
    part1_max_packet_size();
    part2_character_set();
    part3_reserved();
    part4_username();
    part5_auth_response();
    part6_database();
    part7_auth_plugin();
    part8_connection_attrs();
  }

  // now let's verify packet payload length vs what we parsed
  if (packet_.tell() != Packet::get_header_length() + packet_.payload_size_)
    throw std::runtime_error(
        "Handshake response packet: parsed ok, but payload packet size (" +
        std::to_string(packet_.payload_size_) +
        " bytes) differs from what we parsed (" +
        std::to_string(packet_.tell()) + " bytes)");
}

////////////////////////////////////////////////////////////////////////////////
//
// Debug Tools
//
////////////////////////////////////////////////////////////////////////////////

/*static*/ std::string HandshakeResponsePacket::Parser::bytes2str(
    const uint8_t *bytes, size_t length,
    size_t bytes_per_group /* = 4 */) noexcept {
  assert(bytes_per_group > 0);

  const size_t space_after_modulus = bytes_per_group - 1;
  std::ostringstream buf;
  buf << std::hex;

  for (size_t i = 0; i < length; i++) {
    buf << (bytes[i] & 0xf0 >> 4) << (bytes[i] & 0x0f);
    if (i % bytes_per_group == space_after_modulus) buf << " ";
  }

  return buf.str();
}

// throws std::runtime_error
void HandshakeResponsePacket::Parser41::debug_dump() const noexcept {
  assert(packet_.size() >= get_header_length());

  // This function is likely to throw std::runtime_error just about anywhere,
  // if parsing the packet failed or would fail if ran.

  printf("\n--[BEGIN DUMP]----------------------------------------------\n");

  // raw bytes
  printf("\n  [RAW]\n");
  printf("    %s\n", bytes2str(packet_.data(), packet_.size()).c_str());

  // header
  size_t pos = 0;  // add space between size and seq nr --------v
  printf("\n  [HEADER] %s\n", bytes2str(packet_.data() + pos, 4, 3).c_str());
  pos += 4;
  printf("    size = %u\n", packet_.get_payload_size());
  printf("    seq_nr = %u\n", packet_.get_sequence_id());

  // flags
  {
    printf(
        "\n  [CAPABILITY FLAGS (all sent by client are listed, * = also sent "
        "by server)] %s\n",
        bytes2str(packet_.data() + pos, 4, 2).c_str());
    using namespace Capabilities;

    auto print_flag = [&](Flags flag, const char *name) {
      if (packet_.get_capabilities().test(flag))
        printf("  %c %s\n", effective_capability_flags_.test(flag) ? '*' : ' ',
               name);
    };

    print_flag(LONG_PASSWORD, "LONG_PASSWORD");
    print_flag(FOUND_ROWS, "FOUND_ROWS");
    print_flag(LONG_FLAG, "LONG_FLAG");
    print_flag(CONNECT_WITH_DB, "CONNECT_WITH_DB");

    print_flag(NO_SCHEMA, "NO_SCHEMA");
    print_flag(COMPRESS, "COMPRESS");
    print_flag(ODBC, "ODBC");
    print_flag(LOCAL_FILES, "LOCAL_FILES");

    print_flag(IGNORE_SPACE, "IGNORE_SPACE");
    print_flag(PROTOCOL_41, "PROTOCOL_41");
    print_flag(INTERACTIVE, "INTERACTIVE");
    print_flag(SSL, "SSL");

    print_flag(SIG_PIPE, "SIG_PIPE");
    print_flag(TRANSACTIONS, "TRANSACTIONS");
    print_flag(RESERVED_14, "RESERVED_14");
    print_flag(SECURE_CONNECTION, "SECURE_CONNECTION");

    print_flag(MULTI_STATEMENTS, "MULTI_STATEMENTS");
    print_flag(MULTI_RESULTS, "MULTI_RESULTS");
    print_flag(MULTI_PS_MULTO_RESULTS, "MULTI_PS_MULTO_RESULTS");
    print_flag(PLUGIN_AUTH, "PLUGIN_AUTH");

    print_flag(CONNECT_ATTRS, "CONNECT_ATTRS");
    print_flag(PLUGIN_AUTH_LENENC_CLIENT_DATA,
               "PLUGIN_AUTH_LENENC_CLIENT_DATA");
    print_flag(EXPIRED_PASSWORDS, "EXPIRED_PASSWORDS");
    print_flag(SESSION_TRACK, "SESSION_TRACK");

    print_flag(DEPRECATE_EOF, "DEPRECATE_EOF");

    pos += 4;
  }

  // max packet size
  printf("\n  [MAX PACKET SIZE] %s\n",
         bytes2str(packet_.data() + pos, 4).c_str());
  pos += 4;
  printf("    max_packet_size = %u\n", packet_.get_max_packet_size());

  // character set
  printf("\n  [CHARACTER SET] %s\n",
         bytes2str(packet_.data() + pos, 1).c_str());
  pos += 1;
  printf("    character_set = %u\n", packet_.get_character_set());

  // skip over 23 reserveed zero bytes
  printf("\n  [23 RESERVED ZERO BYTES] %s\n",
         bytes2str(packet_.data() + pos, 23).c_str());
  pos += 23;

  // rest of the fields
  printf("\n  [REST] %s\n",
         bytes2str(packet_.data() + pos, packet_.size() - pos).c_str());
  printf("    username = '%s'\n", packet_.get_username().c_str());
  {
    // find end of username (search for zero-terminator)
    size_t i = pos;
    while (packet_[i] && i < packet_.size()) i++;

    // advance to next field (which is auth_response)
    pos = i + 1;

    // if either is not set, 1st byte DOES NOT contain size length-encoded size
    assert(effective_capability_flags_.test(Capabilities::SECURE_CONNECTION) ||
           effective_capability_flags_.test(
               Capabilities::PLUGIN_AUTH_LENENC_CLIENT_DATA));

    size_t len = packet_[pos];  // assume length is encoded in only 1 byte
    pos += 1;                   // advance past auth_response length
    if (len > 0)
      printf("    auth_response = (%zu bytes) %s\n", len,
             bytes2str(packet_.data() + pos, len).c_str());
    else
      printf("    auth_response is empty\n");

    pos += len;  // advance past auth_response payload
  }
  printf("    database = '%s'\n", packet_.get_database().c_str());
  printf("    auth_plugin = '%s'\n", packet_.get_auth_plugin().c_str());
  // not implemented yet: printf("  connection_attrs = %u\n",
  // packet_.get_connection_attr());

  printf("\n--[END DUMP]------------------------------------------------\n\n");
}

}  // namespace mysql_protocol
