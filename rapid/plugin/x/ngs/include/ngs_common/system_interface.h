/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_SYSTEM_INTERFACE_H_
#define NGS_SYSTEM_INTERFACE_H_

#include "ngs/memory.h"


struct addrinfo;

namespace ngs {

class System_interface {
public:
  typedef ngs::shared_ptr<System_interface> Shared_ptr;

  virtual ~System_interface() {}

  virtual int unlink(const char* name) = 0;
  virtual int kill(int pid, int signal) = 0;

  virtual int get_ppid() = 0;
  virtual int get_errno() = 0;
  virtual int get_pid() = 0;

  virtual int  get_socket_errno() = 0;
  virtual void get_socket_error_and_message(int& err, std::string& strerr) = 0;

  virtual void freeaddrinfo(addrinfo *ai) = 0;
  virtual int getaddrinfo(const char *node,
                          const char *service,
                          const addrinfo *hints,
                          addrinfo **res) = 0;
  virtual void sleep(uint32 seconds) = 0;
};

} // namespace ngs

#endif // NGS_SYSTEM_INTERFACE_H_
