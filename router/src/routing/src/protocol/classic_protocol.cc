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

#include "classic_protocol.h"

#include "../utils.h"
#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"

#include <cstring>

using mysql_harness::get_strerror;
IMPORT_LOG_FUNCTIONS()

bool ClassicProtocol::on_block_client_host(int server,
                                           const std::string &log_prefix) {
  auto fake_response = mysql_protocol::HandshakeResponsePacket(
      1, {}, "ROUTER", "", "fake_router_login");
  if (routing_sock_ops_->so()->write_all(server, fake_response.data(),
                                         fake_response.size()) < 0) {
    log_debug("[%s] fd=%d write error: %s", log_prefix.c_str(), server,
              get_message_error(routing_sock_ops_->so()->get_errno()).c_str());
    return false;
  }
  return true;
}

int ClassicProtocol::copy_packets(int sender, int receiver,
                                  bool sender_is_readable,
                                  RoutingProtocolBuffer &buffer,
                                  int *curr_pktnr, bool &handshake_done,
                                  size_t *report_bytes_read,
                                  bool /*from_server*/) {
  assert(curr_pktnr);
  assert(report_bytes_read);
  ssize_t res = 0;
  int pktnr = 0;
  auto buffer_length = buffer.size();

  size_t bytes_read = 0;

  if (!handshake_done && *curr_pktnr == 2) {
    handshake_done = true;
  }

  mysql_harness::SocketOperationsBase *const so = routing_sock_ops_->so();
  if (sender_is_readable) {
    if ((res = so->read(sender, &buffer.front(), buffer_length)) <= 0) {
      if (res == -1) {
        const int last_errno = so->get_errno();

        log_debug("fd=%d read failed: (%d %s)", sender, last_errno,
                  get_message_error(last_errno).c_str());
      } else {
        // the caller assumes that errno == 0 on plain connection closes.
        so->set_errno(0);
      }
      return -1;
    }

    bytes_read += static_cast<size_t>(res);
    if (!handshake_done) {
      // Check packet integrity when handshaking. When packet number is 2, then
      // we assume handshaking is satisfied. For secure connections, we stop
      // when client asks to switch to SSL. The caller should set handshake_done
      // to true when packet number is 2.
      if (bytes_read < mysql_protocol::Packet::kHeaderSize) {
        // We need packet which is at least 4 bytes
        return -1;
      }
      pktnr = buffer[3];
      if (*curr_pktnr > 0 && pktnr != *curr_pktnr + 1) {
        log_debug("Received incorrect packet number; aborting (was %d)", pktnr);
        return -1;
      }

      if (buffer[4] == 0xff) {
        // We got error from MySQL Server while handshaking
        // We do not consider this a failed handshake

        // copy part of the buffer containing serialized error
        RoutingProtocolBuffer buffer_err(
            buffer.begin(),
            buffer.begin() +
                static_cast<RoutingProtocolBuffer::iterator::difference_type>(
                    bytes_read));

        auto server_error = mysql_protocol::ErrorPacket(buffer_err);
        if (so->write_all(receiver, server_error.data(), server_error.size()) <
            0) {
          log_debug("fd=%d write error: %s", receiver,
                    get_message_error(so->get_errno()).c_str());
        }
        // receiver socket closed by caller
        *curr_pktnr =
            2;  // we assume handshaking is done though there was an error
        *report_bytes_read = bytes_read;
        return 0;
      }

      // We are dealing with the handshake response from client
      if (pktnr == 1) {
        // if client is switching to SSL, we are not continuing any checks
        mysql_protocol::Capabilities::Flags capabilities;
        try {
          auto pkt = mysql_protocol::Packet(buffer);
          capabilities = mysql_protocol::Capabilities::Flags(
              pkt.read_int_from<uint32_t>(4));
        } catch (const mysql_protocol::packet_error &exc) {
          log_debug("%s", exc.what());
          return -1;
        }
        if (capabilities.test(mysql_protocol::Capabilities::SSL)) {
          pktnr =
              2;  // Setting to 2, we tell the caller that handshaking is done
        }
      }
    }

    if (so->write_all(receiver, &buffer[0], bytes_read) < 0) {
      const int last_errno = so->get_errno();

      log_debug("fd=%d write error: %s", receiver,
                get_message_error(last_errno).c_str());
      return -1;
    }
  }

  *curr_pktnr = pktnr;
  *report_bytes_read = bytes_read;

  return 0;
}

bool ClassicProtocol::send_error(int destination, unsigned short code,
                                 const std::string &message,
                                 const std::string &sql_state,
                                 const std::string &log_prefix) {
  auto server_error = mysql_protocol::ErrorPacket(0, code, message, sql_state);

  mysql_harness::SocketOperationsBase *const so = routing_sock_ops_->so();
  if (so->write_all(destination, server_error.data(), server_error.size()) <
      0) {
    log_debug("[%s] fd=%d write error: %s", log_prefix.c_str(), destination,
              get_message_error(so->get_errno()).c_str());

    return false;
  }
  return true;
}
