/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_INTERFACE_SOCKET_H_
#define PLUGIN_X_SRC_INTERFACE_SOCKET_H_

#include "violite.h"  // NOLINT(build/include_subdir)

#include "mysql/psi/mysql_socket.h"
#include "mysql/psi/psi_socket.h"

namespace xpl {
namespace iface {

class Socket {
 public:
  virtual ~Socket() = default;

  virtual int bind(const struct sockaddr *addr, socklen_t len) = 0;
  virtual int listen(int backlog) = 0;
  virtual MYSQL_SOCKET accept(PSI_socket_key key, struct sockaddr *addr,
                              socklen_t *addr_len) = 0;
  virtual void close() = 0;

  virtual MYSQL_SOCKET get_socket_mysql() = 0;
  virtual my_socket get_socket_fd() = 0;

  virtual int set_socket_opt(int level, int optname, const SOCKBUF_T *optval,
                             socklen_t optlen) = 0;
  virtual void set_socket_thread_owner() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SOCKET_H_
