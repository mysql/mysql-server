/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"  // rename_thread()
#include "duktape_statement_reader.h"
#include "mock_session.h"
#include "mysql_protocol_utils.h"
#include "socket_operations.h"

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()
#include "mysql/harness/mpmc_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
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
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace std::placeholders;

namespace server_mock {

void non_blocking(socket_t handle_, bool mode) noexcept {
#ifdef _WIN32
  u_long arg = mode ? 1 : 0;
  ioctlsocket(handle_, FIONBIO, &arg);
#else
  int flags = fcntl(handle_, F_GETFL, 0);
  fcntl(handle_, F_SETFL, (flags & ~O_NONBLOCK) | (mode ? O_NONBLOCK : 0));
#endif
}

MySQLServerMock::MySQLServerMock(const std::string &expected_queries_file,
                                 const std::string &module_prefix,
                                 unsigned bind_port,
                                 const std::string &protocol, bool debug_mode)
    : bind_port_{bind_port},
      debug_mode_{debug_mode},
      expected_queries_file_{expected_queries_file},
      module_prefix_{module_prefix},
      protocol_(protocol) {
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
  mysql_harness::rename_thread("SM Main");

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
    throw std::system_error(get_last_socket_error_code(), "socket() failed");
  }

  int option_value = 1;
  if (setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char *>(&option_value),
                 static_cast<socklen_t>(sizeof(int))) == -1) {
    throw std::system_error(get_last_socket_error_code(),
                            "setsockopt() failed");
  }

  err = bind(listener_, ainfo->ai_addr, ainfo->ai_addrlen);
  if (err < 0) {
    throw std::system_error(
        get_last_socket_error_code(),
        "bind('0.0.0.0', " + std::to_string(bind_port_) + ") failed");
  }

  err = listen(listener_, kListenQueueSize);
  if (err < 0) {
    throw std::system_error(get_last_socket_error_code(), "listen() failed");
  }
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
    } else {
      throw std::runtime_error("can't create reader for " + filename);
    }
  }
};

struct Work {
  socket_t client_socket;
  std::string expected_queries_file;
  std::string module_prefix;
  bool debug_mode;
};

void MySQLServerMock::handle_connections(mysql_harness::PluginFuncEnv *env) {
  struct sockaddr_storage client_addr;
  socklen_t addr_size = sizeof(client_addr);

  log_info("Starting to handle connections on port: %d", bind_port_);

  mysql_harness::WaitingMPMCQueue<Work> work_queue;
  mysql_harness::WaitingMPMCQueue<socket_t> socket_queue;

  auto connection_handler = [&]() -> void {
    mysql_harness::rename_thread("SM Worker");

    while (true) {
      auto work = work_queue.pop();

      // exit
      if (work.client_socket == mysql_harness::kInvalidSocket) break;

      try {
        sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (-1 == getsockname(work.client_socket,
                              reinterpret_cast<sockaddr *>(&addr), &addr_len)) {
          throw std::system_error(get_last_socket_error_code(),
                                  "getsockname() failed");
        }
        std::unique_ptr<StatementReaderBase> statement_reader{
            StatementReaderFactory::create(
                work.expected_queries_file, work.module_prefix,
                // expose session data json-encoded string
                {
                    {"port", std::to_string(ntohs(addr.sin_port))},
                },
                MySQLServerSharedGlobals::get())};

        std::unique_ptr<MySQLServerMockSession> session(
            MySQLServerMockSession::create_session(
                protocol_, work.client_socket, std::move(statement_reader),
                work.debug_mode));
        try {
          session->run();
        } catch (const std::exception &e) {
          log_error("%s", e.what());
        }
      } catch (const std::exception &e) {
        // close the connection before Session took over.
        try {
          send_packet(
              work.client_socket,
              MySQLProtocolEncoder().encode_error_message(
                  0, 1064, "", "reader error: " + std::string(e.what())),
              0);
        } catch (const std::system_error &send_e) {
          // it may get EBADF due to the close_all_connections() later.
          if (send_e.code() != std::errc::bad_file_descriptor) {
            log_error("sending error-msg failed: %s", send_e.what());
          }
        }
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
      std::cerr << std::system_error(get_last_socket_error_code(),
                                     "select() failed")
                       .what()
                << "\n";
      break;
    } else if (err == 0) {
      // timeout
      continue;
    }

    if (FD_ISSET(listener_, &fds)) {
      while (true) {
        socket_t client_socket =
            accept(listener_, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == mysql_harness::kInvalidSocket) {
          auto accept_ec = get_last_socket_error_code();

          // if we got interrupted at shutdown, just leave
          if (!is_running(env)) break;

          if (accept_ec == std::errc::resource_unavailable_try_again) break;
          if (accept_ec == std::errc::operation_would_block) break;

          if (accept_ec == std::errc::interrupted) continue;

          std::cerr << std::system_error(accept_ec, "accept() failed").what()
                    << std::endl;
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
    work_queue.push(Work{mysql_harness::kInvalidSocket, "", "", 0});
  }
  // std::cerr << "joining threads" << std::endl;
  for (size_t ndx = 0; ndx < worker_threads.size(); ndx++) {
    worker_threads[ndx].join();
  }
  // std::cerr << "done" << std::endl;
}

std::shared_ptr<MockServerGlobalScope>
    MySQLServerSharedGlobals::shared_globals_;

std::mutex MySQLServerSharedGlobals::mtx_;

}  // namespace server_mock
