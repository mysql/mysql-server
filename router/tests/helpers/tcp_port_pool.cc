/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/filesystem.h"

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <fcntl.h>
#include <string.h>

#include "mysqlrouter/utils.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"

using mysql_harness::Path;
using mysqlrouter::get_socket_errno;

#ifndef _WIN32
bool UniqueId::lock_file(const std::string &file_name) {
  lock_file_fd_ = open(file_name.c_str(), O_RDWR | O_CREAT, 0666);

  if (lock_file_fd_ >= 0) {
#ifdef __sun
    struct flock fl;

    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    int lock = fcntl(lock_file_fd_, F_SETLK, &fl);
#else
    int lock = flock(lock_file_fd_, LOCK_EX | LOCK_NB);
#endif
    if (lock) {
      // no lock so no luck, try the next one
      close(lock_file_fd_);
      return false;
    }

    // obtained the lock
    return true;
  }

  return false;
}

std::string UniqueId::get_lock_file_dir() const {
  // this is what MTR uses, see mysql-test/lib/mtr_unique.pm for details
  return "/tmp/mysql-unique-ids";
}

#else

bool UniqueId::lock_file(const std::string &file_name) {
  lock_file_fd_ = ::CreateFile(file_name.c_str(), GENERIC_READ, 0, NULL,
                               OPEN_ALWAYS, 0, NULL);
  if (lock_file_fd_ != NULL && lock_file_fd_ != INVALID_HANDLE_VALUE) {
    return true;
  }

  return false;
}

std::string UniqueId::get_lock_file_dir() const {
  // this are env variables that MTR uses, see mysql-test/lib/mtr_unique.pm for
  // details
  DWORD buff_size = 65535;
  std::vector<char> buffer;
  buffer.resize(buff_size);
  buff_size = GetEnvironmentVariableA("ALLUSERSPROFILE", &buffer[0], buff_size);
  if (!buff_size) {
    buff_size = GetEnvironmentVariableA("TEMP", &buffer[0], buff_size);
  }

  if (!buff_size) {
    throw std::runtime_error("Could not get directory for lock files.");
  }

  std::string result(buffer.begin(), buffer.begin() + buff_size);
  result.append("\\mysql-unique-ids");
  return result;
}

#endif

UniqueId::UniqueId(unsigned start_from, unsigned range) {
  const std::string lock_file_dir = get_lock_file_dir();
  mysqlrouter::mkdir(lock_file_dir, 0777);

  for (unsigned i = 0; i < range; i++) {
    id_ = start_from + i;
    Path lock_file_path(lock_file_dir);
    lock_file_path.append(std::to_string(id_));
    lock_file_name_ = lock_file_path.str();

    if (lock_file(lock_file_name_.c_str())) {
      // obtained the lock, we are good to go
      return;
    }
  }

  throw std::runtime_error("Could not get uniqe id from the given range");
}

UniqueId::~UniqueId() {
#ifndef _WIN32
  if (lock_file_fd_ > 0) {
    close(lock_file_fd_);
  }

    /*
     * Removing lock file may result in race condition, both fcntl and flock are
     * affected by this issue, consider the following scenario.
     *
     *           process A           process B
     *     1. fd_a = open(file)                     // process A opens file
     *     2. fcntl(fd_a) == 0                      // process A acquires lock
     * on file
     *     3.                    fd_b = open(file)  // process B opens file
     *     4.                    fcntl(fd_b) == -1  // process B fails to
     * acquire lock
     *     5. close(fd_a)                           // process A closes file
     *     6. unlink(file)                          // process A removes file
     * name
     *     7. fd_a = open(file)                     // process A opens file once
     * again
     *     8. fcntl(fd_a) == 0                      // process A acquires lock
     * on the file
     *     9.                    close(fd_b)        // process B closes file
     *    10.                    unlink(file)       // process B removes file
     * name
     *    11.                    fd_b = open(file)  // process B opens file
     *    12.                    fcntl(fd_b) == 0   // process B acquires lock
     * on file
     *
     *    At this point both process A and process B have lock on the same file.
     */

#else
  if (lock_file_fd_ != NULL && lock_file_fd_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(lock_file_fd_);
  }

  if (!lock_file_name_.empty()) {
    mysql_harness::delete_file(lock_file_name_);
  }
#endif
}

UniqueId::UniqueId(UniqueId &&other) {
  id_ = other.id_;
  lock_file_fd_ = other.lock_file_fd_;
  lock_file_name_ = other.lock_file_name_;

  // mark moved object as no longer owning of the resources
#ifndef _WIN32
  other.lock_file_fd_ = -1;
#else
  other.lock_file_fd_ = INVALID_HANDLE_VALUE;
#endif

  other.lock_file_name_ = "";
}

/*
 * Check whether we can connect on a given port.
 * It returns false if the connect returns any error (ECONNREFUSED, ENETUNREACH,
 * EACCESS etc.)
 * */
static bool try_to_connect(uint16_t port,
                           const std::string &hostname = "127.0.0.1") {
  struct addrinfo hints, *ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hostname.c_str(), std::to_string(port).c_str(),
                           &hints, &ainfo);
  if (status != 0) {
    throw std::runtime_error(
        std::string("try_to_connect(): getaddrinfo() failed: ") +
        gai_strerror(status));
  }
  std::shared_ptr<void> exit_freeaddrinfo(nullptr,
                                          [&](void *) { freeaddrinfo(ainfo); });

  auto sock_id =
      socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
  if (sock_id < 0) {
    throw std::runtime_error("try_to_connect(): socket() failed: " +
                             std::to_string(get_socket_errno()));
  }
  std::shared_ptr<void> exit_close_socket(nullptr, [&](void *) {
    mysql_harness::SocketOperations::instance()->close(sock_id);
  });

  status = connect(sock_id, ainfo->ai_addr, ainfo->ai_addrlen);
  return status >= 0;
}

unsigned TcpPortPool::get_next_available() {
  while (true) {
    if (number_of_ids_used_ >= kMaxPort) {
      throw std::runtime_error("No more available ports from UniquePortsGroup");
    }

    // this is the formula that mysql-test also uses to map lock filename to
    // actual port number
    unsigned result =
        10000 + unique_id_.get() * kMaxPort + number_of_ids_used_++;

    // there is no lock file for a given port but let's also check if there
    // really is nothing that will accept our connection attempt on that port
    if (!try_to_connect(result)) return result;

    std::cerr << "get_next_available(): port " << result
              << " seems busy, not using\n";
  }
}
