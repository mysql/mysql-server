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
#include <chrono>

#include "common.h"
#include "dest_round_robin.h"
#include "mysql/harness/logging/logging.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

IMPORT_LOG_FUNCTIONS()

using mysql_harness::TCPAddress;

// Timeout for trying to connect with quarantined servers
static constexpr std::chrono::milliseconds kQuarantinedConnectTimeout(1 * 1000);
// How long we pause before checking quarantined servers again (seconds)
static const int kQuarantineCleanupInterval = 3;
// Make sure Quarantine Manager Thread is run even with nothing in quarantine
static const int kTimeoutQuarantineConditional = 2;

void *DestRoundRobin::run_thread(void *context) {
  DestRoundRobin *dest_round_robin = static_cast<DestRoundRobin *>(context);
  dest_round_robin->quarantine_manager_thread();
  return nullptr;
}

void DestRoundRobin::start() { quarantine_thread_.run(&run_thread, this); }

int DestRoundRobin::get_server_socket(
    std::chrono::milliseconds connect_timeout, int *error,
    mysql_harness::TCPAddress *address) noexcept {
  size_t server_pos;

  const size_t num_servers = size();
  // Try at most num_servers times
  for (size_t i = 0; i < num_servers; i++) {
    try {
      server_pos = get_next_server();
    } catch (const std::runtime_error &) {
      log_warning("No destinations currently available for routing");
      return -1;
    }

    // If server is quarantined, skip
    {
      std::lock_guard<std::mutex> lock(mutex_quarantine_);
      if (is_quarantined(server_pos)) {
        continue;
      }
    }

    // Try server
    TCPAddress server_addr = destinations_[server_pos];
    log_debug("Trying server %s (index %lu)", server_addr.str().c_str(),
              static_cast<long unsigned>(server_pos));
    auto sock = get_mysql_socket(server_addr, connect_timeout);
    if (sock >= 0) {
      // Server is available
      if (address) *address = server_addr;
      return sock;
    } else {
#ifndef _WIN32
      *error = errno;
#else
      *error = WSAGetLastError();
#endif
      if (errno != ENFILE && errno != EMFILE) {
        // We failed to get a connection to the server; we quarantine.
        std::lock_guard<std::mutex> lock(mutex_quarantine_);
        add_to_quarantine(server_pos);
        if (quarantined_.size() == destinations_.size()) {
          log_debug("No more destinations: all quarantined");
          break;
        }
        continue;  // try another destination
      }
      break;
    }
  }

  return -1;  // no destination is available
}

DestRoundRobin::~DestRoundRobin() {
  stopping_ = true;
  quarantine_thread_.join();
}

void DestRoundRobin::add_to_quarantine(const size_t index) noexcept {
  assert(index < size());
  if (index >= size()) {
    log_debug("Impossible server being quarantined (index %lu)",
              static_cast<long unsigned>(index));  // 32bit Linux requires cast
    return;
  }
  if (!is_quarantined(index)) {
    log_debug("Quarantine destination server %s (index %lu)",
              destinations_.at(index).str().c_str(),
              static_cast<long unsigned>(index));  // 32bit Linux requires cast
    quarantined_.push_back(index);
    condvar_quarantine_.notify_one();
  }
}

void DestRoundRobin::cleanup_quarantine() noexcept {
  mutex_quarantine_.lock();
  // Nothing to do when nothing quarantined
  if (quarantined_.empty()) {
    mutex_quarantine_.unlock();
    return;
  }
  // We work on a copy; updating the original
  auto cpy_quarantined(quarantined_);
  mutex_quarantine_.unlock();

  for (auto it = cpy_quarantined.begin(); it != cpy_quarantined.end(); ++it) {
    if (stopping_) {
      return;
    }

    auto addr = destinations_.at(*it);
    auto sock = get_mysql_socket(addr, kQuarantinedConnectTimeout, false);

    if (sock >= 0) {
#ifndef _WIN32
      shutdown(sock, SHUT_RDWR);
      close(sock);
#else
      shutdown(sock, SD_BOTH);
      closesocket(sock);
#endif
      log_debug("Unquarantine destination server %s (index %lu)",
                addr.str().c_str(),
                static_cast<long unsigned>(*it));  // 32bit Linux requires cast
      std::lock_guard<std::mutex> lock(mutex_quarantine_);
      quarantined_.erase(
          std::remove(quarantined_.begin(), quarantined_.end(), *it));
    }
  }
}

void DestRoundRobin::quarantine_manager_thread() noexcept {
  mysql_harness::rename_thread(
      "RtQ:<unknown>");  // TODO change <unknown> to instance name

  std::unique_lock<std::mutex> lock(mutex_quarantine_manager_);
  while (!stopping_) {
    condvar_quarantine_.wait_for(
        lock, std::chrono::seconds(kTimeoutQuarantineConditional),
        [this] { return !quarantined_.empty(); });

    if (!stopping_) {
      cleanup_quarantine();
      // Temporize
      std::this_thread::sleep_for(
          std::chrono::seconds(kQuarantineCleanupInterval));
    }
  }
}

size_t DestRoundRobin::size_quarantine() {
  std::lock_guard<std::mutex> lock(mutex_quarantine_);
  return quarantined_.size();
}
