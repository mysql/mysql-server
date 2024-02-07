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

#ifndef PLUGIN_X_SRC_INTERFACE_SYSTEM_H_
#define PLUGIN_X_SRC_INTERFACE_SYSTEM_H_

#include <string>

struct addrinfo;

namespace xpl {
namespace iface {

class System {
 public:
  virtual ~System() = default;

  virtual int32_t unlink(const char *name) = 0;
  virtual int32_t kill(int32_t pid, int32_t signal) = 0;

  virtual int32_t get_ppid() = 0;
  virtual int32_t get_errno() = 0;
  virtual int32_t get_pid() = 0;

  virtual int32_t get_socket_errno() = 0;
  virtual void set_socket_errno(const int32_t err) = 0;
  virtual void get_socket_error_and_message(int32_t *out_err,
                                            std::string *out_strerr) = 0;

  virtual void freeaddrinfo(addrinfo *ai) = 0;
  virtual int32_t getaddrinfo(const char *node, const char *service,
                              const addrinfo *hints, addrinfo **res) = 0;
  virtual void sleep(uint32_t seconds) = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SYSTEM_H_
