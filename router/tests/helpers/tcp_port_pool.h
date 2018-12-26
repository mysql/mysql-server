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

#ifndef _TCP_PORT_POOL_H_
#define _TCP_PORT_POOL_H_

/** @class UniqueID
 *
 * Helper class allowing mechanism to retrieve system-level unique identifier.
 * Compatible with mysql-test MTR, see mysql-test/lib/mtr_unique.pm for details
 *
 **/
class UniqueId {
 public:
  UniqueId(unsigned start_from, unsigned range);
  UniqueId(UniqueId &&other);
  ~UniqueId();

  UniqueId(const UniqueId &) = delete;
  UniqueId &operator=(const UniqueId &) = delete;

  unsigned get() const { return id_; }

 private:
  bool lock_file(const std::string &file_name);
  std::string get_lock_file_dir() const;

  unsigned id_;
#ifndef _WIN32
  int lock_file_fd_;
#else
  HANDLE lock_file_fd_;
#endif
  std::string lock_file_name_;
};

/** @class TcpPortPool
 *
 * Helper class allowing mechanism to retrieve pool of the system-level unique
 *TCP port numbers. Compatible with mysql-test MTR, see
 *mysql-test/lib/mtr_unique.pm for details.
 *
 **/
class TcpPortPool {
 public:
  TcpPortPool(unsigned start_from = 1, unsigned range = 300)
      : unique_id_(start_from, range) {}

  TcpPortPool(const TcpPortPool &) = delete;
  TcpPortPool &operator=(const TcpPortPool &) = delete;
  TcpPortPool(TcpPortPool &&other) = default;

  unsigned get_next_available();

 private:
  UniqueId unique_id_;
  unsigned number_of_ids_used_{0};
  static const int kMaxPort{10};
};

#endif  // _TCP_PORT_POOL_H_
