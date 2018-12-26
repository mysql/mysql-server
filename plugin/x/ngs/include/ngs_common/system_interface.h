/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_COMMON_SYSTEM_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_COMMON_SYSTEM_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/memory.h"

struct addrinfo;

namespace ngs {

class System_interface {
 public:
  typedef ngs::shared_ptr<System_interface> Shared_ptr;

  virtual ~System_interface() {}

  virtual int unlink(const char *name) = 0;
  virtual int kill(int pid, int signal) = 0;

  virtual int get_ppid() = 0;
  virtual int get_errno() = 0;
  virtual int get_pid() = 0;

  virtual int get_socket_errno() = 0;
  virtual void set_socket_errno(const int err) = 0;
  virtual void get_socket_error_and_message(int &out_err,
                                            std::string &out_strerr) = 0;

  virtual void freeaddrinfo(addrinfo *ai) = 0;
  virtual int getaddrinfo(const char *node, const char *service,
                          const addrinfo *hints, addrinfo **res) = 0;
  virtual void sleep(uint32 seconds) = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_COMMON_SYSTEM_INTERFACE_H_
