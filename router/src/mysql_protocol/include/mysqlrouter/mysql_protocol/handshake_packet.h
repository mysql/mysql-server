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

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED

#include <memory>
#include "base_packet.h"

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

/** @brief Default capability flags
 *
 * Default capability flags
 *
 * NOTE: do not put inside the class, this makes SunStudio on
 *       Solaris to generate invalid code
 *
 */
static constexpr Capabilities::Flags kDefaultClientCapabilities =
    Capabilities::LONG_PASSWORD | Capabilities::LONG_FLAG |
    Capabilities::CONNECT_WITH_DB | Capabilities::LOCAL_FILES |
    Capabilities::PROTOCOL_41 | Capabilities::TRANSACTIONS |
    Capabilities::SECURE_CONNECTION | Capabilities::MULTI_STATEMENTS |
    Capabilities::MULTI_RESULTS;

/** @class HandshakeResponsePacket
 * @brief Creates a MySQL handshake response packet
 *
 * This class creates a MySQL handshake response packet which is send by
 * the MySQL client after receiving the server's handshake packet.
 *
 */
class MYSQL_PROTOCOL_API HandshakeResponsePacket final : public Packet {
 public:
  /** @brief Constructor
   *
   * This version of constructor just creates an uninitialized packet
   */
  HandshakeResponsePacket()
      : Packet(0),
        username_(""),
        password_(""),
        char_set_(8),
        auth_plugin_("mysql_native_password"),
        auth_response_({}) {
    prepare_packet();
  }

  /** @overload
   *
   * This version of constructor takes in packet bytes, parses it and writes
   * results in object's fields
   *
   * @param buffer Packet payload (including packet header)
   * @param server_capabilities Capabilities sent by the server in Handshake
   * Packet; see note in parse_payload()
   * @param auto_parse_payload Disables automatic parsing of payload if set to
   * false Note that header is still parsed (sequence_id_ and payload_size_ are
   * set)
   *
   * @throws std::runtime_error on unrecognised or invalid packet, when parsing
   */
  HandshakeResponsePacket(
      const std::vector<uint8_t> &buffer, bool auto_parse_payload = false,
      Capabilities::Flags server_capabilities = Capabilities::ALL_ZEROS)
      : Packet(buffer) {
    if (auto_parse_payload) parse_payload(server_capabilities);
  }

  /** @overload
   *
   * This version of constructor takes in fields, and generates packet bytes
   *
   * @param sequence_id MySQL Packet number
   * @param auth_response Authentication data from the MySQL server handshake
   * @param username MySQL username to use
   * @param password MySQL password to use
   * @param database MySQL database to use when connecting (default is empty)
   * @param char_set MySQL character set code (default 8, latin1)
   * @param auth_plugin MySQL authentication plugin name (default
   * 'mysql_native_password')
   */
  HandshakeResponsePacket(
      uint8_t sequence_id, const std::vector<unsigned char> &auth_response,
      const std::string &username, const std::string &password,
      const std::string &database = "", unsigned char char_set = 8,
      const std::string &auth_plugin = "mysql_native_password");

  /** @brief Parses packet payload, results written to object's field
   *
   * @param server_capabilities Capabilities sent by the server in Handshake
   * Packet, see note below
   *
   * @throws std::runtime_error on unrecognised or invalid packet
   *
   * @note MySQL Protocol has a quirk: In the Handshake Packet, server sends to
   * client its capability flags, then in Handshake Response Packet, client
   * sends its own, possibly including some that the server did not advertise.
   * Despite advertising these flags unique to client, it does not actually use
   * them. This is vital in understanding packets. If data chunk dataX depeneded
   * on capability X, then how should a packet be parsed when it comes in?
   *         {data1, data2, dataX, data3, data4}
   *       or
   *         {data1, data2, data3, data4}
   *       Apparently the latter
   */
  void parse_payload(Capabilities::Flags server_capabilities) {
    init_parser_if_not_initialized();
    parser_->parse(server_capabilities);
  }

  /** @brief returns username specified in the packet */
  const std::string &get_username() const { return username_; }

  /** @brief returns database name specified in the packet */
  const std::string &get_database() const { return database_; }

  /** @brief returns character set specified in the packet */
  uint8_t get_character_set() const { return char_set_; }

  /** @brief returns auth-plugin-name specified in the packet */
  const std::string &get_auth_plugin() const { return auth_plugin_; }

  /** @brief returns auth-plugin-data specified in the packet */
  const std::vector<uint8_t> &get_auth_response() const {
    return auth_response_;
  }

  /** @brief returns max packet size specified in the packet */
  uint32_t get_max_packet_size() const { return max_packet_size_; }

  /** @brief (debug tool) parse packet contents and print this info on stdout */
  void debug_dump() {
    init_parser_if_not_initialized();
    parser_->debug_dump();
  }

 private:
  class Parser;

  /** @brief Prepares the packet
   *
   * Prepares the actual MySQL Error packet and stores it. The header is
   * created using the sequence id and the size of the payload.
   */
  void prepare_packet();

  /** @brief Initializes Parser needed to parse the packet payload */
  void init_parser_if_not_initialized() {
    if (!parser_) {
      if (Parser41::is_protocol41(*this)) {
        parser_.reset(new Parser41(*this));
      } else if (Parser320::is_protocol320(*this)) {
        parser_.reset(new Parser320(*this));
      } else {
        assert(0);
      }
    }
  }

  /** @brief MySQL username */
  std::string username_;

  /** @brief MySQL password */
  std::string password_;

  /** @brief MySQL database */
  std::string database_;

  /** @brief MySQL character set */
  unsigned char char_set_;

  /** @brief MySQL authentication plugin name */
  std::string auth_plugin_;

  /** @brief MySQL auth-response */
  std::vector<unsigned char> auth_response_;

  /** @brief Max size that of a command packet that the client wants to send to
   * the server */
  uint32_t max_packet_size_;

  /** @brief Parser used to parse this packet */
  std::unique_ptr<Parser> parser_;

  class MYSQL_PROTOCOL_API Parser {
   public:
    virtual ~Parser() = default;
    virtual void parse(Capabilities::Flags server_capabilities) = 0;

    // debug tools
    static std::string bytes2str(const uint8_t *bytes, size_t length,
                                 size_t bytes_per_group = 4) noexcept;
    virtual void debug_dump() const = 0;
  };

  class MYSQL_PROTOCOL_API Parser41 : public Parser {
   public:
    Parser41(HandshakeResponsePacket &packet) : packet_(packet) {}

    /** @brief Tests if handshake response packet has PROTOCOL_41 flag set
     *
     * This is a very simple method, it only checks that single flag and does
     * nothing else (in particular, it doesn't perform any kind of validation)
     */
    static bool is_protocol41(const HandshakeResponsePacket &packet);

    /** @brief Parses handshake response packet
     *
     * This method assumes that the current packet is a PROTOCOL41 handshake
     * response.
     *
     * @param server_capabilities Capability flags of the server. Client's flags
     * will be &-ed with them before applying rules for packet parsing.
     * @throws std::runtime_error on unrecognised or invalid packet
     */
    void parse(Capabilities::Flags server_capabilities) override;

    // debug tools
    void debug_dump() const noexcept override;

   private:
    /** @brief Helper functions called by parse()
     *
     * All these methods throw std::runtime_error on parse errors; in
     * particular, std::range_error (std::runtime_error specialization) is
     * thrown on EOF
     * */
    void part1_max_packet_size();
    void part2_character_set();
    void part3_reserved();
    void part4_username();
    void part5_auth_response();
    void part6_database();
    void part7_auth_plugin();
    void part8_connection_attrs();

    HandshakeResponsePacket &packet_;
    Capabilities::Flags effective_capability_flags_;

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

  class MYSQL_PROTOCOL_API Parser320 : public Parser {
   public:
    Parser320(HandshakeResponsePacket &packet) : packet_(packet) {}

    /** @brief Tests if handshake response packet DOES NOT have PROTOCOL_41 flag
     * set
     *
     * This is a very simple method, it only checks that single flag and does
     * nothing else (in particular, it doesn't perform any kind of validation)
     */
    static bool is_protocol320(const HandshakeResponsePacket &packet);

    /** @brief Parses handshake response packet
     *
     * Currently not implemented
     */
    void parse(Capabilities::Flags server_capabilities) override;
    void debug_dump() const override;

    HandshakeResponsePacket &packet_;
    Capabilities::Flags effective_capability_flags_;
  };

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

};  // class HandshakeResponsePacket

}  // namespace mysql_protocol

#endif  // MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED
