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

#ifndef ROUTING_MOCKS_INCLUDED
#define ROUTING_MOCKS_INCLUDED

#ifdef _WIN32
#include "Winsock2.h"
#endif

// ignore GMock warnings
#ifdef __clang__
#ifndef __has_warning
#define __has_warning(x) 0
#endif
#pragma clang diagnostic push
#if __has_warning("-Winconsistent-missing-override")
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
#if __has_warning("-Wsign-conversion")
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#endif
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <atomic>

class MockSocketOperations : public mysql_harness::SocketOperationsBase {
 public:
  MOCK_METHOD3(read, ssize_t(int, void *, size_t));
  MOCK_METHOD3(write, ssize_t(int, void *, size_t));
  MOCK_METHOD1(close, void(int));
  MOCK_METHOD1(shutdown, void(int));
  MOCK_METHOD1(freeaddrinfo, void(addrinfo *ai));
  MOCK_METHOD4(getaddrinfo,
               int(const char *, const char *, const addrinfo *, addrinfo **));
  MOCK_METHOD3(bind, int(int, const struct sockaddr *, socklen_t));
  MOCK_METHOD3(socket, int(int, int, int));
  MOCK_METHOD5(setsockopt, int(int, int, int, const void *, socklen_t));
  MOCK_METHOD2(listen, int(int fd, int n));
  MOCK_METHOD3(poll, int(struct pollfd *, nfds_t, std::chrono::milliseconds));
  MOCK_METHOD2(connect_non_blocking_wait,
               int(socket_t sock, std::chrono::milliseconds timeout));
  MOCK_METHOD2(connect_non_blocking_status, int(int sock, int &so_error));
  MOCK_METHOD0(get_local_hostname, std::string());
  MOCK_METHOD4(inetntop, const char *(int af, void *, char *, socklen_t));
  MOCK_METHOD3(getpeername, int(int, struct sockaddr *, socklen_t *));

  void set_errno(int err) override {
  // set errno/Windows equivalent. At the time of writing, unit tests
  // will pass just fine without this, as they are too low-level and the errno
  // is checked at higher level. But to do an accurate mock, we should set
  // this.
#ifdef _WIN32
    WSASetLastError(err);
#else
    errno = err;
#endif
  }

  int get_errno() override {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }
};

class MockRoutingSockOps : public routing::RoutingSockOpsInterface {
 public:
  MockRoutingSockOps() : so_(new MockSocketOperations) {}

  MockSocketOperations *so() const override { return so_.get(); }

  int get_mysql_socket(mysql_harness::TCPAddress addr,
                       std::chrono::milliseconds,
                       bool = true) noexcept override {
    get_mysql_socket_call_cnt_++;
    if (get_mysql_socket_fails_todo_) {
      so()->set_errno(ECONNREFUSED);
      get_mysql_socket_fails_todo_--;
      return -1;  // -1 means server is unavailable
    } else {
      so()->set_errno(0);

      // if addr string starts with a number, this will return it. Therefore
      // it's recommended that addr.addr is set to something like "42"
      return atoi(addr.addr.c_str());
    }
  }

  int get_mysql_socket_call_cnt() {
    int cc = get_mysql_socket_call_cnt_;
    get_mysql_socket_call_cnt_ = 0;
    return cc;
  }

  void get_mysql_socket_fail(int fail_cnt) {
    get_mysql_socket_fails_todo_ = fail_cnt;
  }

 private:
  std::atomic_int get_mysql_socket_fails_todo_{0};
  std::atomic_int get_mysql_socket_call_cnt_{0};

  std::unique_ptr<MockSocketOperations> so_;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif  // ROUTING_MOCKS_INCLUDED
