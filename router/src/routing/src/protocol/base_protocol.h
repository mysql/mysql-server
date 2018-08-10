/*
Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_BASEPROTOCOL_INCLUDED
#define ROUTING_BASEPROTOCOL_INCLUDED

#include <cstdint>
#include <string>
#include <vector>
#include "mysqlrouter/mysql_protocol.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <winsock2.h>
#undef ERROR
#endif

using RoutingProtocolBuffer = mysql_protocol::Packet::vector_t;

namespace routing {
class RoutingSockOpsInterface;
}

class BaseProtocol {
 public:
  /** @brief supported protocols */
  enum class Type { kClassicProtocol, kXProtocol };

  BaseProtocol(routing::RoutingSockOpsInterface *routing_sock_ops)
      : routing_sock_ops_(routing_sock_ops) {}
  virtual ~BaseProtocol() {}

  /** @brief Function that gets called when the client is being blocked
   *
   * This function is called when the client is being blocked and should handle
   * any communication to the server required by the protocol in such case
   *
   * @param server Descriptor of the server
   * @param log_prefix prefix to be used by the function as a tag for logging
   *
   * @return true on success; false on error
   */
  virtual bool on_block_client_host(int server,
                                    const std::string &log_prefix) = 0;

  /** @brief Reads from sender and writes it back to receiver using select
   *
   * This function reads data from the sender socket and writes it back
   * to the receiver socket. It use `select`.
   *
   * @param sender Descriptor of the sender
   * @param receiver Descriptor of the receiver
   * @param sender_is_readable true if sender socket has data
   * @param buffer Buffer to use for storage
   * @param curr_pktnr Pointer to storage for sequence id of packet
   * @param handshake_done Whether handshake phase is finished or not
   * @param report_bytes_read Pointer to storage to report bytes read
   * @param from_server true if the message sender is the server, false
   *                    if it is a client
   *
   * @return 0 on success; -1 on error
   */
  virtual int copy_packets(int sender, int receiver, bool sender_is_readable,
                           RoutingProtocolBuffer &buffer, int *curr_pktnr,
                           bool &handshake_done, size_t *report_bytes_read,
                           bool from_server) = 0;

  /** @brief Sends error message to the provided receiver.
   *
   * This function sends protocol message containing MySQL error
   *
   * @param destination descriptor of the receiver
   * @param code general error code
   * @param message human readable error message
   * @param sql_state SQL state for the error
   * @param log_prefix prefix to be used by the function as a tag for logging
   *
   * @return true on success; false on error
   */
  virtual bool send_error(int destination, unsigned short code,
                          const std::string &message,
                          const std::string &sql_state,
                          const std::string &log_prefix) = 0;

  /** @brief Gets protocol type. */
  virtual Type get_type() = 0;

 protected:
  routing::RoutingSockOpsInterface *routing_sock_ops_;
};

#endif  // ROUTING_BASEPROTOCOL_INCLUDED
