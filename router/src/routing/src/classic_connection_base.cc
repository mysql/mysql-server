/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_connection_base.h"

#include <chrono>
#include <cinttypes>  // PRIu64
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include "basic_protocol_splicer.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/channel.h"  // Channel, ClassicProtocolState
#include "mysqlrouter/classic_protocol_session_track.h"
#include "mysqlrouter/classic_protocol_state.h"
#include "mysqlrouter/connection_pool_component.h"
#include "processor.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

#undef DEBUG_IO

template <class T>
static constexpr uint8_t type_byte() {
  return classic_protocol::Codec<T>::type_byte();
}

stdx::expected<size_t, std::error_code>
MysqlRoutingClassicConnectionBase::encode_error_packet(
    std::vector<uint8_t> &error_frame, const uint8_t seq_id,
    const classic_protocol::capabilities::value_type caps,
    const uint16_t error_code, const std::string &msg,
    const std::string &sql_state) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::borrowed::message::server::Error>(
          seq_id, {error_code, msg, sql_state}),
      caps, net::dynamic_buffer(error_frame));
}

static stdx::expected<size_t, std::error_code>
encode_server_side_client_greeting(
    Channel::recv_buffer_type &send_buf, uint8_t seq_id,
    const classic_protocol::capabilities::value_type &shared_capabilities) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::borrowed::message::client::Greeting>(
          seq_id,
          {
              {},                                            // caps
              16 * 1024 * 1024,                              // max-packet-size
              classic_protocol::collation::Latin1SwedishCi,  // collation
              "ROUTER",                                      // username
              "",                                            // auth data
              "fake_router_login",                           // schema
              "mysql_native_password",                       // auth method
              ""                                             // attributes
          }),
      shared_capabilities, net::dynamic_buffer(send_buf));
}

static void log_fatal_error_code(const char *msg, std::error_code ec) {
  log_error("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
            ec.value());
}

void MysqlRoutingClassicConnectionBase::on_handshake_received() {
  auto &blocked_endpoints = this->context().blocked_endpoints();
  auto &client_conn = this->client_conn();

  const uint64_t old_value = client_conn.reset_error_count(blocked_endpoints);

  if (old_value != 0) {
    log_info("[%s] resetting error counter for %s (was %" PRIu64 ")",
             this->context().get_name().c_str(), client_conn.endpoint().c_str(),
             old_value);
  }
}

void MysqlRoutingClassicConnectionBase::on_handshake_aborted() {
  auto &blocked_endpoints = this->context().blocked_endpoints();
  auto &client_conn = this->client_conn();
  const uint64_t new_value =
      client_conn.increment_error_count(blocked_endpoints);

  if (new_value >= blocked_endpoints.max_connect_errors()) {
    log_warning("[%s] blocking client host for %s",
                this->context().get_name().c_str(),
                client_conn.endpoint().c_str());
  } else {
    log_info("[%s] incrementing error counter for host of %s (now %" PRIu64 ")",
             this->context().get_name().c_str(), client_conn.endpoint().c_str(),
             new_value);
  }
}

void MysqlRoutingClassicConnectionBase::send_server_failed(std::error_code ec,
                                                           bool call_finish) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r->s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec, call_finish);
}

void MysqlRoutingClassicConnectionBase::recv_server_failed(std::error_code ec,
                                                           bool call_finish) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r<-s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec, call_finish);
}

void MysqlRoutingClassicConnectionBase::send_client_failed(std::error_code ec,
                                                           bool call_finish) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c<-r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec, call_finish);
}

void MysqlRoutingClassicConnectionBase::recv_client_failed(std::error_code ec,
                                                           bool call_finish) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c->r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec, call_finish);
}

void MysqlRoutingClassicConnectionBase::server_socket_failed(std::error_code ec,
                                                             bool call_finish) {
  auto &server_conn = this->server_conn();

  if (server_conn.is_open()) {
    if (ec != net::stream_errc::eof) {
      (void)server_conn.shutdown(net::socket_base::shutdown_send);
    }
    (void)server_conn.close();
    trace(Tracer::Event()
              .stage("close::server")
              .direction(Tracer::Event::Direction::kServerClose));
  }

  if (call_finish) finish();
}

void MysqlRoutingClassicConnectionBase::client_socket_failed(std::error_code ec,
                                                             bool call_finish) {
  if (client_conn().is_open()) {
    // only log the connection-error, if the client started to send a handshake
    // and then aborted before the handshake finished.
    if (client_conn().protocol().handshake_state() ==
        ClassicProtocolState::HandshakeState::kClientGreeting) {
      log_info("[%s] %s closed connection before finishing handshake",
               this->context().get_name().c_str(),
               client_conn().endpoint().c_str());

      on_handshake_aborted();
    }

    if (ec != net::stream_errc::eof) {
      // the other side hasn't closed yet, shutdown our send-side.
      (void)client_conn().shutdown(net::socket_base::shutdown_send);
    }
    (void)client_conn().close();
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event()
                   .stage("close::client")
                   .direction(Tracer::Event::Direction::kClientClose));
    }
  }

  if (call_finish) finish();
}

void MysqlRoutingClassicConnectionBase::async_send_client(Function next) {
  auto &dst_conn = client_conn();
  auto &dst_channel = dst_conn.channel();

  if (disconnect_requested()) {
    return send_client_failed(make_error_code(std::errc::operation_canceled));
  }

  ++active_work_;
  dst_conn.async_send(
      [this, next, to_transfer = dst_channel.send_buffer().size()](
          std::error_code ec, size_t transferred) {
        --active_work_;
        if (ec) return send_client_failed(ec);

        this->transfered_to_client(transferred);

        if (transferred < to_transfer) {
          // send the rest
          return async_send_client(next);
        }

        return trace_and_call_function(
            Tracer::Event::Direction::kRouterToClient, "io::send", next);
      });
}

void MysqlRoutingClassicConnectionBase::async_recv_client(Function next) {
  if (disconnect_requested()) {
    return recv_client_failed(make_error_code(std::errc::operation_canceled));
  }

  auto &src_conn = client_conn();

  ++active_work_;
  src_conn.async_recv(
      [this, next](std::error_code ec, size_t transferred [[maybe_unused]]) {
        --active_work_;

        if (ec != std::errc::operation_canceled) {
          read_timer().cancel();
        }

        if (ec) return recv_client_failed(ec);

        return trace_and_call_function(
            Tracer::Event::Direction::kClientToRouter, "io::recv", next);
      });
}

void MysqlRoutingClassicConnectionBase::async_send_server(Function next) {
  if (disconnect_requested()) {
    return send_server_failed(make_error_code(std::errc::operation_canceled));
  }

  auto &dst_conn = server_conn();
  auto &dst_channel = dst_conn.channel();

  ++active_work_;
  dst_conn.async_send(
      [this, next, to_transfer = dst_channel.send_buffer().size()](
          std::error_code ec, size_t transferred) {
        --active_work_;
        if (ec) return send_server_failed(ec);

        this->transfered_to_server(transferred);

        if (transferred < to_transfer) {
          // send the rest
          return async_send_server(next);
        }

        return trace_and_call_function(
            Tracer::Event::Direction::kRouterToServer, "io::send", next);
      });
}

void MysqlRoutingClassicConnectionBase::async_recv_server(Function next) {
  if (disconnect_requested()) {
    return recv_server_failed(make_error_code(std::errc::operation_canceled));
  }

  auto &src_conn = server_conn();

  ++active_work_;
  src_conn.async_recv(
      [this, next](std::error_code ec, size_t transferred [[maybe_unused]]) {
        --active_work_;

        if (ec) return recv_server_failed(ec);

        return trace_and_call_function(
            Tracer::Event::Direction::kServerToRouter, "io::recv", next);
      });
}

void MysqlRoutingClassicConnectionBase::async_recv_both(Function next) {
  if (disconnect_requested()) {
    return recv_client_failed(make_error_code(std::errc::operation_canceled));
  }

  recv_from_either(MysqlRoutingClassicConnectionBase::FromEither::Started);

  ++active_work_;  // client
  ++active_work_;  // server

  client_conn().async_recv([this, next](std::error_code ec,
                                        size_t transferred [[maybe_unused]]) {
    --active_work_;

    if (ec == std::errc::operation_canceled) {
      // cancelled by:
      //
      // - request to shutdown
      // - timer
      // - read-from-client-xor-server
      if (recv_from_either() ==
          MysqlRoutingClassicConnectionBase::FromEither::RecvedFromServer) {
        recv_from_either(MysqlRoutingClassicConnectionBase::FromEither::None);

        return call_next_function(next);
      }
    }

    if (ec) return recv_client_failed(ec);

    if (recv_from_either() ==
        MysqlRoutingClassicConnectionBase::FromEither::Started) {
      recv_from_either(
          MysqlRoutingClassicConnectionBase::FromEither::RecvedFromClient);
    }

    return trace_and_call_function(Tracer::Event::Direction::kClientToRouter,
                                   "io::recv", next);
  });

  server_conn().async_recv([this, next](std::error_code ec,
                                        size_t transferred) {
    (void)transferred;

    --active_work_;

    if (ec == std::errc::operation_canceled) {
      // cancelled by:
      //
      // - request to shutdown
      // - timer
      // - read-from-client-xor-server
      if (recv_from_either() ==
          MysqlRoutingClassicConnectionBase::FromEither::RecvedFromClient) {
        recv_from_either(MysqlRoutingClassicConnectionBase::FromEither::None);

        return call_next_function(next);
      }
    }

    if (ec) return recv_server_failed(ec);

    if (recv_from_either() ==
        MysqlRoutingClassicConnectionBase::FromEither::Started) {
      recv_from_either(
          MysqlRoutingClassicConnectionBase::FromEither::RecvedFromServer);
    }

    return trace_and_call_function(Tracer::Event::Direction::kServerToRouter,
                                   "io::recv", next);
  });
}

void MysqlRoutingClassicConnectionBase::async_wait_send_server(Function next) {
  ++active_work_;
  server_conn().async_wait_send([this, next](std::error_code ec) {
    --active_work_;

    if (ec == std::errc::operation_canceled &&
        connect_error_code() != std::error_code{}) {
      ec = {};
    }

    if (ec) return send_server_failed(ec);

    return trace_and_call_function(Tracer::Event::Direction::kRouterToServer,
                                   "io::wait", next);
  });
}

void MysqlRoutingClassicConnectionBase::disconnect() {
  disconnect_request([this](auto &req) {
    auto &io_ctx = client_conn().connection()->io_ctx();

    if (io_ctx.stopped()) abort();

    req = true;

    // if disconnect is called from another thread,
    //
    // queue the cancel in the connections io-ctx to make it thread-safe.
    net::dispatch(io_ctx, [this, self = shared_from_this()]() {
      (void)client_conn().cancel();
      (void)server_conn().cancel();
    });
  });
}

// the client didn't send a Greeting before closing the connection.
//
// Generate a Greeting to be sent to the server, to ensure the router's IP
// isn't blocked due to the server's max_connect_errors.
void MysqlRoutingClassicConnectionBase::server_side_client_greeting() {
  auto encode_res = encode_server_side_client_greeting(
      this->server_conn().channel().send_buffer(), 1,
      client_conn().protocol().shared_capabilities());
  if (!encode_res) return send_server_failed(encode_res.error());

  return async_send_server(Function::kFinish);
}

// after a QUIT, we should wait until the client closed the connection.

// called when the connection should be closed.
//
// called multiple times (once per "active_work_").
void MysqlRoutingClassicConnectionBase::finish() {
  auto &client_socket = client_conn();
  auto &server_socket = server_conn();

  if (server_socket.is_open() && !client_socket.is_open()) {
    // client side closed while server side is still open ...
    if (server_socket.protocol().handshake_state() ==
        ClassicProtocolState::HandshakeState::kServerGreeting) {
      // client hasn't sent a greeting to the server. The server would track
      // this as "connection error" and block the router. Better send our own
      // client-greeting.
      server_socket.protocol().handshake_state(
          ClassicProtocolState::HandshakeState::kClientGreeting);
      return server_side_client_greeting();
    } else {
      // if the server is waiting on something, cancel it,
      // as client is already gone.
      (void)server_socket.cancel();
    }
  } else if (!server_socket.is_open() && client_socket.is_open()) {
    // if the client is waiting on something, as server is already gone.
    (void)client_socket.cancel();
  }

  if (active_work_ == 0) {
    log_connection_summary();
    if (server_socket.is_open()) {
      trace(Tracer::Event()
                .stage("close::server")
                .direction(Tracer::Event::Direction::kServerClose));
      (void)server_socket.shutdown(net::socket_base::shutdown_send);
      (void)server_socket.close();
    }
    if (client_socket.is_open()) {
      trace(Tracer::Event()
                .stage("close::client")
                .direction(Tracer::Event::Direction::kClientClose));
      (void)client_socket.shutdown(net::socket_base::shutdown_send);
      (void)client_socket.close();
    }

    done();
  }
}

// final state.
//
// removes the connection from the connection-container.
void MysqlRoutingClassicConnectionBase::done() { this->disassociate(); }

stdx::expected<void, std::error_code>
MysqlRoutingClassicConnectionBase::track_session_changes(
    net::const_buffer session_trackers,
    classic_protocol::capabilities::value_type caps,
    bool ignore_some_state_changed) {
  std::bitset<5> set_names_sysvar{};

  do {
    auto decode_session_res = classic_protocol::decode<
        classic_protocol::borrowed::session_track::Field>(session_trackers,
                                                          caps);
    if (!decode_session_res) {
      return stdx::unexpected(decode_session_res.error());
    }

    const auto decoded_size = decode_session_res->first;

    if (decoded_size == 0) {
      return stdx::unexpected(make_error_code(std::errc::bad_message));
    }

    enum class Type {
      SystemVariable =
          type_byte<classic_protocol::session_track::SystemVariable>(),
      Schema = type_byte<classic_protocol::session_track::Schema>(),
      State = type_byte<classic_protocol::session_track::State>(),
      Gtid = type_byte<classic_protocol::session_track::Gtid>(),
      TransactionState =
          type_byte<classic_protocol::session_track::TransactionState>(),
      TransactionCharacteristics = type_byte<
          classic_protocol::session_track::TransactionCharacteristics>(),
    };

    switch (Type{decode_session_res->second.type()}) {
      case Type::SystemVariable: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::SystemVariable>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug(
              "decoding session_track::SystemVariable from server failed: %s",
              decode_value_res.error().message().c_str());
        } else {
          const auto kv = decode_value_res->second;

          if (kv.key() == "character_set_client") {
            set_names_sysvar.set(0);
          } else if (kv.key() == "character_set_connection") {
            set_names_sysvar.set(1);
          } else if (kv.key() == "character_set_results") {
            set_names_sysvar.set(2);
          } else if (kv.key() == "collation_connection") {
            set_names_sysvar.set(3);
          }

          auto value_from_kv = [](auto kv) -> Value {
            // the session tracker can't report NULL. Instead it reports "".
            //
            // In the case of 'character_set_results' setting "" leads to an
            // error which means "" needs to be converted back to NULL again.
            if (kv.value().empty() &&
                (kv.key() == "character_set_results" ||
                 kv.key() == "innodb_ft_user_stopword_table")) {
              return {std::nullopt};
            }
            return {std::string(kv.value())};
          };

          exec_ctx_.system_variables().set(std::string(kv.key()),
                                           value_from_kv(kv));

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< "
                << "SET @@SESSION." << kv.key() << " = " << quoted(kv.value())
                << ";";

            tr.trace(Tracer::Event().stage(oss.str()));
          }
        }
      } break;
      case Type::Schema: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::Schema>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug("decoding session_track::Schema from server failed: %s",
                    decode_value_res.error().message().c_str());
        } else {
          auto schema = std::string(decode_value_res->second.schema());

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< "
                << "USE " << schema;

            tr.trace(Tracer::Event().stage(oss.str()));
          }

          server_protocol().schema(schema);
          client_protocol().schema(schema);
        }
      } break;
      case Type::State: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::State>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug("decoding session_track::State from server failed: %s",
                    decode_value_res.error().message().c_str());
        } else {
          // .state() is always '1'

          if (!ignore_some_state_changed) {
            some_state_changed_ = true;
          }

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< "
                << "some session state changed.";

            tr.trace(Tracer::Event().stage(oss.str()));
          }
        }
      } break;
      case Type::Gtid: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::Gtid>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug("decoding session_track::Gtid from server failed: %s",
                    decode_value_res.error().message().c_str());
        } else {
          const auto gtid = decode_value_res->second;

          client_protocol().gtid_executed(std::string(gtid.gtid()));

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< "
                << "gtid: (spec: " << static_cast<int>(gtid.spec()) << ") "
                << gtid.gtid();

            tr.trace(Tracer::Event().stage(oss.str()));
          }
        }
      } break;
      case Type::TransactionState: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::TransactionState>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug(
              "decoding session_track::TransactionState from server failed: %s",
              decode_value_res.error().message().c_str());
        } else {
          auto trx_state = decode_value_res->second;

          // remember the last transaction-state
          trx_state_ = trx_state;

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< "
                << "trx-state: ";

            switch (trx_state.trx_type()) {
              case '_':
                oss << "no trx";
                break;
              case 'T':
                oss << "explicit trx";
                break;
              case 'I':
                oss << "implicit trx";
                break;
              default:
                oss << "(unknown trx-type)";
                break;
            }

            switch (trx_state.read_trx()) {
              case '_':
                break;
              case 'R':
                oss << ", read trx";
                break;
              default:
                oss << ", (unknown read-trx-type)";
                break;
            }

            switch (trx_state.read_unsafe()) {
              case '_':
                break;
              case 'r':
                oss << ", read trx (non-transactional)";
                break;
              default:
                oss << ", (unknown read-unsafe-type)";
                break;
            }

            switch (trx_state.write_trx()) {
              case '_':
                break;
              case 'W':
                oss << ", write trx";
                break;
              default:
                oss << ", (unknown write-trx-type)";
                break;
            }

            switch (trx_state.write_unsafe()) {
              case '_':
                break;
              case 'w':
                oss << ", write trx (non-transactional)";
                break;
              default:
                oss << ", (unknown write-unsafe-type)";
                break;
            }

            switch (trx_state.stmt_unsafe()) {
              case '_':
                break;
              case 's':
                oss << ", stmt unsafe (UUID(), RAND(), ...)";
                break;
              default:
                oss << ", (unknown stmt-unsafe-type)";
                break;
            }

            switch (trx_state.resultset()) {
              case '_':
                break;
              case 'S':
                oss << ", resultset sent";
                break;
              default:
                oss << ", (unknown resultset-type)";
                break;
            }

            switch (trx_state.locked_tables()) {
              case '_':
                break;
              case 'L':
                oss << ", LOCK TABLES";
                break;
              default:
                oss << ", (unknown locked-tables-type)";
                break;
            }

            tr.trace(Tracer::Event().stage(oss.str()));
          }
        }
      } break;
      case Type::TransactionCharacteristics: {
        auto decode_value_res =
            classic_protocol::decode<classic_protocol::borrowed::session_track::
                                         TransactionCharacteristics>(
                net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          log_debug(
              "decoding session_track::TransactionCharacteristics from server "
              "failed: %s",
              decode_value_res.error().message().c_str());
        } else {
          auto trx_characteristics = decode_value_res->second;

          trx_characteristics_ = {
              std::string(trx_characteristics.characteristics())};

          if (auto &tr = tracer()) {
            std::ostringstream oss;

            oss << "<< trx-stmt: " << trx_characteristics.characteristics();

            tr.trace(Tracer::Event().stage(oss.str()));
          }
        }
      } break;
    }

    // go to the next field.
    session_trackers += decoded_size;
  } while (session_trackers.size() > 0);

  if (set_names_sysvar.to_ulong() == 0b0111) {
    // character_set... are set, but not collation_connection.

    collation_connection_maybe_dirty_ = true;
  }

  return {};
}

void MysqlRoutingClassicConnectionBase::trace_and_call_function(
    Tracer::Event::Direction dir, std::string_view stage,
    MysqlRoutingClassicConnectionBase::Function func) {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage(stage).direction(dir));
  }

  call_next_function(func);
}

void MysqlRoutingClassicConnectionBase::loop() {
  while (!processors_.empty()) {
    auto res = processors_.back()->process();

    if (!res) {
      auto ec = res.error();

      log_fatal_error_code("classic::loop() processor failed", ec);

      // close the connection.
      break;
    }

    switch (*res) {
      case Processor::Result::Done:
        processors_.pop_back();
        break;
      case Processor::Result::RecvFromClient:
        return async_recv_client(Function::kLoop);
      case Processor::Result::RecvFromServer:
        return async_recv_server(Function::kLoop);
      case Processor::Result::RecvFromBoth:
        return async_recv_both(Function::kLoop);
      case Processor::Result::SendToClient:
        return async_send_client(Function::kLoop);
      case Processor::Result::SendToServer:
        return async_send_server(Function::kLoop);
      case Processor::Result::SendableToServer:
        return async_wait_send_server(Function::kLoop);
      case Processor::Result::Again:
        break;
      case Processor::Result::Suspend:
      case Processor::Result::Void:
        return;
    }
  }

  finish();
}

bool MysqlRoutingClassicConnectionBase::connection_sharing_possible() const {
  const auto &sysvars = exec_ctx_.system_variables();

  return context_.connection_sharing() &&             // config must allow it.
         client_protocol().password().has_value() &&  // a password is required
         sysvars.get("session_track_gtids") == "OWN_GTID" &&
         sysvars.get("session_track_state_change") == "ON" &&
         sysvars.get("session_track_system_variables") == "*" &&
         sysvars.get("session_track_transaction_info") == "CHARACTERISTICS";
}

static bool trx_characteristics_is_sharable(
    const std::optional<
        classic_protocol::session_track::TransactionCharacteristics>
        &trx_chars) {
  if (!trx_chars.has_value()) return false;

  auto stmt = trx_chars->characteristics();
  if (stmt.empty()) return true;

  std::string_view stmt_view(stmt);

  constexpr const std::string_view kSetTrx("SET TRANSACTION ");
  constexpr const std::string_view kSetTrxIsolationLevel(
      "SET TRANSACTION ISOLATION LEVEL ");
  constexpr const std::string_view kStartTrx("START TRANSACTION");

  if (stmt_view.substr(0, kSetTrxIsolationLevel.size()) ==
      kSetTrxIsolationLevel) {
    using namespace std::string_view_literals;

    stmt_view = stmt_view.substr(kSetTrxIsolationLevel.size());

    auto match_isolation_level =
        [](std::string_view stmt_view) -> std::optional<size_t> {
      for (auto isolation_level : {
               "READ COMMITTED"sv,
               "READ UNCOMMITTED"sv,
               "REPEATABLE READ"sv,
               "SERIALIZABLE"sv,
           }) {
        if (stmt_view.substr(0, isolation_level.size()) == isolation_level) {
          return isolation_level.size();
        }
      }
      return {};
    };

    auto isolation_level_res = match_isolation_level(stmt_view);
    if (!isolation_level_res) return false;

    // skip the isolation level.
    stmt_view = stmt_view.substr(isolation_level_res.value());

    auto semi = stmt_view.substr(0, 2);

    if (semi == ";") return true;  // end

    if (semi != "; ") return false;  // unexpected

    // SET TRANSACTION READ ... may follow
    stmt_view = stmt_view.substr(semi.size());
  }

  if (stmt_view.substr(0, kSetTrx.size()) == kSetTrx) {
    stmt_view = stmt_view.substr(kSetTrx.size());

    using namespace std::string_view_literals;
    for (auto suffix : {
             "READ ONLY;"sv,
             "READ WRITE;"sv,
             ";"sv,
         }) {
      if (stmt_view == suffix) return true;
    }
  } else if (stmt_view.substr(0, kStartTrx.size()) == kStartTrx) {
    stmt_view = stmt_view.substr(kStartTrx.size());

    using namespace std::string_view_literals;
    for (auto suffix : {
             " READ ONLY;"sv,
             " READ WRITE;"sv,
             ";"sv,
         }) {
      if (stmt_view == suffix) return true;
    }
  }

  return false;
}

static bool trx_state_is_sharable(
    const std::optional<classic_protocol::session_track::TransactionState>
        &trx_state) {
  // at the start trx_state is not set.
  if (!trx_state.has_value()) return true;

  auto st = *trx_state;

  // trx-type: _|T|I are "no", "explicit", "implicit" started transactions
  //
  // they have been started, but nothing has been executed in them yet which
  // allows to replay the statements via session-tracker.trx_characteristics.
  return (st.trx_type() == '_' || st.trx_type() == 'T' ||
          st.trx_type() == 'I') &&
         st.read_unsafe() == '_' && st.read_trx() == '_' &&    //
         st.write_unsafe() == '_' && st.write_trx() == '_' &&  //
         st.stmt_unsafe() == '_' && st.resultset() == '_' &&   //
         st.locked_tables() == '_';
}

bool MysqlRoutingClassicConnectionBase::connection_sharing_allowed() const {
  return connection_sharing_possible() &&                          //
         trx_state_is_sharable(trx_state_) &&                      //
         trx_characteristics_is_sharable(trx_characteristics_) &&  //
         !some_state_changed_;
}

std::string MysqlRoutingClassicConnectionBase::connection_sharing_blocked_by()
    const {
  const auto &sysvars = exec_ctx_.system_variables();

  // "possible"
  if (!context_.connection_sharing()) return "config";
  if (!client_protocol().password().has_value()) return "no-password";
  if (sysvars.get("session_track_gtids") != Value("OWN_GTID"))
    return "session-track-gtids";
  if (sysvars.get("session_track_state_change") != Value("ON"))
    return "session-track-state-change";
  if (sysvars.get("session_track_system_variables") != Value("*"))
    return "session-track-system-variables";
  if (sysvars.get("session_track_transaction_info") != Value("CHARACTERISTICS"))
    return "session-track-transaction-info";

  // "allowed"
  if (!(!trx_state_.has_value() ||
        (*trx_state_ == classic_protocol::session_track::TransactionState{
                            '_', '_', '_', '_', '_', '_', '_', '_'}))) {
    return "trx-state";
  }

  if (!(trx_characteristics_.has_value() &&
        trx_characteristics_->characteristics().empty())) {
    return "trx-characteristics";
  }
  if (some_state_changed_) return "some-state-changed";

  return "";  // not blocked.
}

void MysqlRoutingClassicConnectionBase::connection_sharing_allowed_reset() {
  trx_state_.reset();
  trx_characteristics_.reset();
  some_state_changed_ = false;
}

void MysqlRoutingClassicConnectionBase::reset_to_initial() {
  auto &src_protocol = client_protocol();

  // allow connection sharing again.
  connection_sharing_allowed_reset();

  // clear the warnings
  execution_context().diagnostics_area().warnings().clear();

  // clear the tracked-system-vars like sql_mode, ...
  execution_context().system_variables().clear();

  // clear the prepared statements.
  src_protocol.prepared_statements().clear();

  // back to 'auto'
  src_protocol.access_mode({});

  // disable the tracer.
  src_protocol.trace_commands(false);
  events().active(false);

  // reset to initial values.
  src_protocol.gtid_executed({});

  src_protocol.wait_for_my_writes(context().wait_for_my_writes());
  src_protocol.wait_for_my_writes_timeout(
      context().wait_for_my_writes_timeout());

  diagnostic_area_changed(false);
}

void MysqlRoutingClassicConnectionBase::stash_server_conn() {
  // no-op
}
