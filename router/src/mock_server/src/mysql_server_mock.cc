/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql_server_mock.h"
#include "duktape_statement_reader.h"
#include "json_statement_reader.h"
#include "mysql_protocol_utils.h"

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef _WIN32
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
constexpr socket_t kInvalidSocket = -1;
#endif

using namespace std::placeholders;

namespace server_mock {

constexpr char kAuthCachingSha2Password[] = "caching_sha2_password";
constexpr char kAuthNativePassword[] = "mysql_native_password";
constexpr size_t kReadBufSize =
    16 * 1024;  // size big enough to contain any packet we're likely to read

MySQLServerMock::MySQLServerMock(const std::string &expected_queries_file,
                                 const std::string &module_prefix,
                                 unsigned bind_port, bool debug_mode)
    : bind_port_{bind_port},
      debug_mode_{debug_mode},
      expected_queries_file_{expected_queries_file},
      module_prefix_{module_prefix} {
  if (debug_mode_)
    std::cout << "\n\nExpected SQL queries come from file '"
              << expected_queries_file << "'\n\n"
              << std::flush;
}

MySQLServerMock::~MySQLServerMock() {
  if (listener_ > 0) {
    close_socket(listener_);
  }
}

// close all active connections
void MySQLServerMock::close_all_connections() {
  std::lock_guard<std::mutex> active_fd_lock(active_fds_mutex_);
  for (auto it = active_fds_.begin(); it != active_fds_.end();) {
    close_socket(*it);
    it = active_fds_.erase(it);
  }
}

void MySQLServerMock::run(mysql_harness::PluginFuncEnv *env) {
  setup_service();
  handle_connections(env);
}

void MySQLServerMock::setup_service() {
  int err;
  struct addrinfo hints, *ainfo;

  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  err =
      getaddrinfo(nullptr, std::to_string(bind_port_).c_str(), &hints, &ainfo);
  if (err != 0) {
    throw std::runtime_error(std::string("getaddrinfo() failed: ") +
                             gai_strerror(err));
  }

  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { freeaddrinfo(ainfo); });

  listener_ = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
  if (listener_ < 0) {
    throw std::runtime_error("socket() failed: " + get_socket_errno_str());
  }

  int option_value = 1;
  if (setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char *>(&option_value),
                 static_cast<socklen_t>(sizeof(int))) == -1) {
    throw std::runtime_error("setsockopt() failed: " + get_socket_errno_str());
  }

  err = bind(listener_, ainfo->ai_addr, ainfo->ai_addrlen);
  if (err < 0) {
    throw std::runtime_error("bind('0.0.0.0', " + std::to_string(bind_port_) +
                             ") failed: " + strerror(get_socket_errno()) +
                             " (" + get_socket_errno_str() + ")");
  }

  err = listen(listener_, kListenQueueSize);
  if (err < 0) {
    throw std::runtime_error("listen() failed: " + get_socket_errno_str());
  }
}

void MySQLServerMockSession::send_handshake(
    socket_t client_socket,
    mysql_protocol::Capabilities::Flags our_capabilities) {
  constexpr const char *plugin_name = kAuthNativePassword;
  constexpr const char *plugin_data = "123456789|ABCDEFGHI|";  // 20 bytes

  std::vector<uint8_t> buf = protocol_encoder_.encode_greetings_message(
      0, "8.0.5", 1, plugin_data, our_capabilities, plugin_name);
  send_packet(client_socket, buf);
}

mysql_protocol::HandshakeResponsePacket
MySQLServerMockSession::handle_handshake_response(
    socket_t client_socket,
    mysql_protocol::Capabilities::Flags our_capabilities) {
  typedef std::vector<uint8_t> MsgBuffer;
  using namespace mysql_protocol;

  uint8_t buf[kReadBufSize];
  size_t payload_size;
  constexpr size_t header_len = HandshakeResponsePacket::get_header_length();

  // receive handshake response packet
  {
    // reads all bytes or throws
    read_packet(client_socket, buf, header_len);

    if (HandshakeResponsePacket::read_sequence_id(buf) != 1)
      throw std::runtime_error(
          "Handshake response packet with incorrect sequence number: " +
          std::to_string(HandshakeResponsePacket::read_sequence_id(buf)));

    payload_size = HandshakeResponsePacket::read_payload_size(buf);
    assert(header_len + payload_size <= sizeof(buf));

    // reads all bytes or throws
    read_packet(client_socket, buf + header_len, payload_size);
  }

  // parse handshake response packet
  {
    HandshakeResponsePacket pkt(
        MsgBuffer(buf, buf + header_len + payload_size));
    try {
      pkt.parse_payload(our_capabilities);

#if 0  // enable if you need to debug
      pkt.debug_dump();
#endif
      return pkt;
    } catch (const std::runtime_error &e) {
      // Dump packet contents to stdout, so we can try to debug what went wrong.
      // Since parsing failed, this is also likely to throw. If it doesn't,
      // great, but we'll be happy to take whatever info the dump can give us
      // before throwing.
      try {
        pkt.debug_dump();
      } catch (...) {
      }

      throw;
    }
  }
}

void MySQLServerMockSession::handle_auth_switch(socket_t client_socket) {
  constexpr uint8_t seq_nr = 2;

  // send switch-auth request packet
  {
    constexpr const char *plugin_data = "123456789|ABCDEFGHI|";

    auto buf = protocol_encoder_.encode_auth_switch_message(
        seq_nr, kAuthCachingSha2Password, plugin_data);
    send_packet(client_socket, buf);
  }

  // receive auth-data packet
  {
    using namespace mysql_protocol;
    constexpr size_t header_len = HandshakeResponsePacket::get_header_length();

    uint8_t buf[kReadBufSize];

    // reads all bytes or throws
    read_packet(client_socket, buf, header_len);

    if (HandshakeResponsePacket::read_sequence_id(buf) != seq_nr + 1)
      throw std::runtime_error(
          "Auth-change response packet with incorrect sequence number: " +
          std::to_string(HandshakeResponsePacket::read_sequence_id(buf)));

    size_t payload_size = HandshakeResponsePacket::read_payload_size(buf);
    assert(header_len + payload_size <= sizeof(buf));

    // reads all bytes or throws
    read_packet(client_socket, buf + header_len, payload_size);

    // for now, we ignore the contents we just read, because we always
    // positively authenticate the client
  }
}

void MySQLServerMockSession::send_fast_auth(socket_t client_socket) {
  // a mysql-8 client will send us a cache-256-password-scramble
  // and expects a \x03 back (fast-auth) + a OK packet
  // Here we send the 1st of the two.

  // pretend we do cached_sha256 fast-auth
  constexpr uint8_t seq_nr = 4;
  constexpr uint8_t fast_auth_cmd = 3;
  constexpr uint8_t payload_size_bytes[] = {1, 0, 0};
  constexpr uint8_t switch_auth[] = {
      payload_size_bytes[0], payload_size_bytes[1], payload_size_bytes[2],
      seq_nr, fast_auth_cmd};

  send_packet(client_socket, switch_auth, sizeof(switch_auth));
}

void non_blocking(socket_t handle_, bool mode) {
#ifdef _WIN32
  u_long arg = mode ? 1 : 0;
  ioctlsocket(handle_, FIONBIO, &arg);
#else
  int flags = fcntl(handle_, F_GETFL, 0);
  fcntl(handle_, F_SETFL, (flags & ~O_NONBLOCK) | (mode ? O_NONBLOCK : 0));
#endif
}

class StatementReaderFactory {
 public:
  static StatementReaderBase *create(
      const std::string &filename, std::string &module_prefix,
      std::map<std::string, std::string> session_data,
      std::shared_ptr<MockServerGlobalScope> shared_globals) {
    if (filename.substr(filename.size() - 3) == ".js") {
      return new DuktapeStatementReader(filename, module_prefix, session_data,
                                        shared_globals);
    } else if (filename.substr(filename.size() - 5) == ".json") {
      return new QueriesJsonReader(filename);
    } else {
      throw std::runtime_error("can't create reader for " + filename);
    }
  }
};

MySQLServerMockSession::MySQLServerMockSession(
    socket_t client_sock,
    std::unique_ptr<StatementReaderBase> statement_processor, bool debug_mode)
    : client_socket_{client_sock},
      protocol_decoder_{&read_packet},
      json_reader_{std::move(statement_processor)},
      debug_mode_{debug_mode} {
  // if it doesn't work, no problem.
  int one = 1;
  setsockopt(client_socket_, IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char *>(&one), sizeof(one));

  non_blocking(client_socket_, false);
}

MySQLServerMockSession::~MySQLServerMockSession() {}

void MySQLServerMockSession::run() {
  try {
    ////////////////////////////////////////////////////////////////////////////////
    //
    // This is the handshake packet that my server v8.0.5 emits:
    //
    //        <header >   v10  <--- server version
    //  0000: 6c00 0000   0a   38 2e30 2e35 2d65 6e74 6572 7072 6973 652d 636f
    //  6d6d 6572 6369 616c            l....8.0.5-enterprise-commercial
    //
    //                         server version -> <conn id> <-- auth data 1 -->
    //                         zero cap.low char status
    //  0020: 2d61 6476 616e 6365 642d 6c6f 6700 0800 0000 5b09 4e78 3d48 0a11
    //  00   ff ff   ff   0200       -advanced-log.....[.Nx=H........
    //
    //      cap.hi  auth-len  <- reserved 10 0-bytes ->   <SECURE_CONN &&
    //      auth-data 2    >   <PLUGIN_AUTH && auth-plugin name
    //  0040: ffc3     15     00 0000 0000 0000 0000 00   64 1242 070c 5263 2d01
    //  710c 4100   6361 6368 696e   .............d.B..Rc-.q.A.cachin
    //
    //        auth-plugin name --------------------->
    //  0060: 675f 7368 6132 5f70 6173 7377 6f72 6400 g_sha2_password.
    //
    //
    //  client v8.0.5 reponds with capability flags: 05ae ff01
    //
    ////////////////////////////////////////////////////////////////////////////////

    using namespace mysql_protocol;

    constexpr Capabilities::Flags our_capabilities =
        Capabilities::PROTOCOL_41 | Capabilities::PLUGIN_AUTH |
        Capabilities::SECURE_CONNECTION;

    send_handshake(client_socket_, our_capabilities);
    HandshakeResponsePacket handshake_response =
        handle_handshake_response(client_socket_, our_capabilities);

    uint8_t packet_seq = 2u;
    if (handshake_response.get_auth_plugin() == kAuthCachingSha2Password) {
      // typically, client >= 8.0.4 will trigger this branch

      handle_auth_switch(client_socket_);
      send_fast_auth(client_socket_);
      packet_seq += 3;  // 2 from auth-switch + 1 from fast-auth

    } else if (handshake_response.get_auth_plugin() == kAuthNativePassword) {
      // typically, client <= 5.7 will trigger this branch; do nothing, we're
      // good
    } else {
      // unexpected auth-plugin name
      assert(0);
    }

    send_ok(client_socket_, packet_seq);

    bool res = process_statements(client_socket_);
    if (!res) {
      std::cout << "Error processing statements with client: " << client_socket_
                << std::endl;
    }
  } catch (const std::exception &e) {
    log_warning("Exception caught in connection loop: %s", e.what());
  }
}

struct Work {
  socket_t client_socket;
  std::string expected_queries_file;
  std::string module_prefix;
  bool debug_mode;
};

template <typename Data>
class concurrent_queue {
 public:
  Data pop() {
    std::unique_lock<std::mutex> mlock(mutex_);

    while (queue_.empty()) {
      cond_.wait(mlock);
    }

    auto item = queue_.front();
    queue_.pop();
    return item;
  }

  void push(Data &&item) {
    std::unique_lock<std::mutex> mlock(mutex_);

    queue_.push(std::move(item));

    mlock.unlock();
    cond_.notify_one();
  }

 private:
  std::queue<Data> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

void MySQLServerMock::handle_connections(mysql_harness::PluginFuncEnv *env) {
  struct sockaddr_storage client_addr;
  socklen_t addr_size = sizeof(client_addr);

  log_info("Starting to handle connections on port: %d", bind_port_);

  concurrent_queue<Work> work_queue;
  concurrent_queue<socket_t> socket_queue;

  auto connection_handler = [&]() -> void {
    while (true) {
      auto work = work_queue.pop();

      // exit
      if (work.client_socket == kInvalidSocket) break;

      try {
        sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (-1 == getsockname(work.client_socket,
                              reinterpret_cast<sockaddr *>(&addr), &addr_len)) {
          throw std::system_error(get_socket_errno(), std::system_category(),
                                  "getsockname() failed");
        }
        std::unique_ptr<StatementReaderBase> statement_reader{
            StatementReaderFactory::create(
                work.expected_queries_file, work.module_prefix,
                // expose session data json-encoded string
                {
                    {"port", std::to_string(ntohs(addr.sin_port))},
                },
                shared_globals_)};

        MySQLServerMockSession session(
            work.client_socket, std::move(statement_reader), work.debug_mode);
        try {
          session.run();
        } catch (const std::exception &e) {
          log_error("%s", e.what());
        }
      } catch (const std::exception &e) {
        // close the connection before Session took over.
        send_packet(work.client_socket,
                    MySQLProtocolEncoder().encode_error_message(
                        0, 1064, "", "reader error: " + std::string(e.what())),
                    0);
        log_error("%s", e.what());
      }

      // first remove the book-keeping, then close the socket
      // to avoid a race between the acceptor and the worker thread
      {
        // socket is done, unregister it
        std::lock_guard<std::mutex> active_fd_lock(active_fds_mutex_);
        auto it = active_fds_.find(work.client_socket);
        if (it != active_fds_.end()) {
          // it should always be there
          active_fds_.erase(it);
        }
      }
      close_socket(work.client_socket);
    }
  };

  non_blocking(listener_, true);

  std::deque<std::thread> worker_threads;
  for (size_t ndx = 0; ndx < 4; ndx++) {
    worker_threads.emplace_back(connection_handler);
  }

  while (is_running(env)) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listener_, &fds);

    // timeval is initialized in loop because value of timeval may be overriden
    // by calling select.
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    int err = select(listener_ + 1, &fds, NULL, NULL, &tv);

    if (err < 0) {
      std::cerr << "select() failed: " << strerror(errno) << "\n";
      break;
    } else if (err == 0) {
      // timeout
      continue;
    }

    if (FD_ISSET(listener_, &fds)) {
      while (true) {
        socket_t client_socket =
            accept(listener_, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == kInvalidSocket) {
          auto accept_errno = get_socket_errno();

          // if we got interrupted at shutdown, just leave
          if (!is_running(env)) break;

          if (accept_errno == EAGAIN) break;
          if (accept_errno == EWOULDBLOCK) break;
#ifdef _WIN32
          if (accept_errno == WSAEWOULDBLOCK) break;
          if (accept_errno == WSAEINTR) continue;
#endif
          if (accept_errno == EINTR) continue;

          std::cerr << "accept() failed: errno=" << accept_errno << std::endl;
          return;
        }

        {
          // socket is new, register it
          std::lock_guard<std::mutex> active_fd_lock(active_fds_mutex_);
          active_fds_.emplace(client_socket);
        }

        // std::cout << "Accepted client " << client_socket << std::endl;
        work_queue.push(Work{client_socket, expected_queries_file_,
                             module_prefix_, debug_mode_});
      }
    }
  }

  // beware, this closes all sockets that are either in the work-queue or
  // currently handled by worker-threads. As long as we don't reuse the
  // file-handles for anything else before we leave this function, this approach
  // is safe.
  close_all_connections();

  // std::cerr << "sending death-signal to threads" << std::endl;
  for (size_t ndx = 0; ndx < worker_threads.size(); ndx++) {
    work_queue.push(Work{kInvalidSocket, "", "", 0});
  }
  // std::cerr << "joining threads" << std::endl;
  for (size_t ndx = 0; ndx < worker_threads.size(); ndx++) {
    worker_threads[ndx].join();
  }
  // std::cerr << "done" << std::endl;
}

bool MySQLServerMockSession::process_statements(socket_t client_socket) {
  using mysql_protocol::Command;

  while (!killed_) {
    protocol_decoder_.read_message(client_socket);
    auto cmd = protocol_decoder_.get_command_type();
    switch (cmd) {
      case Command::QUERY: {
        std::string statement_received = protocol_decoder_.get_statement();

        try {
          handle_statement(client_socket, protocol_decoder_.packet_seq(),
                           json_reader_->handle_statement(statement_received));
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          uint8_t packet_seq =
              protocol_decoder_.packet_seq() + 1;  // rollover to 0 is ok
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          send_error(client_socket, packet_seq, 1064,
                     std::string("executing statement failed: ") + e.what());

          // assume the connection is broken
          return true;
        }
      } break;
      case Command::QUIT:
        // std::cout << "received QUIT command from the client" << std::endl;
        return true;
      default:
        std::cerr << "received unsupported command from the client: "
                  << static_cast<int>(cmd) << "\n";
        uint8_t packet_seq =
            protocol_decoder_.packet_seq() + 1;  // rollover to 0 is ok
        std::this_thread::sleep_for(json_reader_->get_default_exec_time());
        send_error(client_socket, packet_seq, 1064,
                   "Unsupported command: " + std::to_string(cmd));
    }
  }

  return true;
}

static void debug_trace_result(const ResultsetResponse *resultset) {
  std::cout << "QUERY RESULT:\n";
  for (size_t i = 0; i < resultset->rows.size(); ++i) {
    for (const auto &cell : resultset->rows[i])
      std::cout << "  |  " << (cell.first ? cell.second : "NULL");
    std::cout << "  |\n";
  }
  std::cout << "\n\n\n" << std::flush;
}

void MySQLServerMockSession::handle_statement(
    socket_t client_socket, uint8_t seq_no,
    const StatementAndResponse &statement) {
  using StatementResponseType = StatementAndResponse::StatementResponseType;

  switch (statement.response_type) {
    case StatementResponseType::STMT_RES_OK: {
      if (debug_mode_) std::cout << std::endl;  // visual separator
      OkResponse *response =
          dynamic_cast<OkResponse *>(statement.response.get());
      std::this_thread::sleep_for(statement.exec_time);
      send_ok(client_socket, static_cast<uint8_t>(seq_no + 1), 0,
              response->last_insert_id, 0, response->warning_count);
    } break;
    case StatementResponseType::STMT_RES_RESULT: {
      ResultsetResponse *response =
          dynamic_cast<ResultsetResponse *>(statement.response.get());
      if (debug_mode_) {
        debug_trace_result(response);
      }
      seq_no = static_cast<uint8_t>(seq_no + 1);
      auto buf = protocol_encoder_.encode_columns_number_message(
          seq_no++, response->columns.size());
      std::this_thread::sleep_for(statement.exec_time);
      send_packet(client_socket, buf);
      for (const auto &column : response->columns) {
        auto col_buf =
            protocol_encoder_.encode_column_meta_message(seq_no++, column);
        send_packet(client_socket, col_buf);
      }
      buf = protocol_encoder_.encode_eof_message(seq_no++);
      send_packet(client_socket, buf);

      for (size_t i = 0; i < response->rows.size(); ++i) {
        auto res_buf = protocol_encoder_.encode_row_message(
            seq_no++, response->columns, response->rows[i]);
        send_packet(client_socket, res_buf);
      }
      buf = protocol_encoder_.encode_eof_message(seq_no++);
      send_packet(client_socket, buf);
    } break;
    case StatementResponseType::STMT_RES_ERROR: {
      if (debug_mode_) std::cout << std::endl;  // visual separator
      ErrorResponse *response =
          dynamic_cast<ErrorResponse *>(statement.response.get());
      send_error(client_socket, static_cast<uint8_t>(seq_no + 1),
                 response->code, response->msg);
    } break;
    default:;
      throw std::runtime_error("Unsupported command in handle_statement(): " +
                               std::to_string((int)statement.response_type));
  }
}

void MySQLServerMockSession::send_error(socket_t client_socket, uint8_t seq_no,
                                        uint16_t error_code,
                                        const std::string &error_msg,
                                        const std::string &sql_state) {
  auto buf = protocol_encoder_.encode_error_message(seq_no, error_code,
                                                    sql_state, error_msg);
  send_packet(client_socket, buf);
}

void MySQLServerMockSession::send_ok(socket_t client_socket, uint8_t seq_no,
                                     uint64_t affected_rows,
                                     uint64_t last_insert_id,
                                     uint16_t server_status,
                                     uint16_t warning_count) {
  auto buf = protocol_encoder_.encode_ok_message(
      seq_no, affected_rows, last_insert_id, server_status, warning_count);
  send_packet(client_socket, buf);
}

}  // namespace server_mock
