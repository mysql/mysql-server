/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_SOCKET_EVENTS_INTERFACE_H_
#define NGS_SOCKET_EVENTS_INTERFACE_H_

#include <vector>
#include <list>
#include <ngs/thread.h>
#include "ngs_common/socket_interface.h"


namespace ngs {

class Connection_acceptor_interface;

class Socket_events_interface {
public:
  virtual ~Socket_events_interface() {}

  virtual bool listen(Socket_interface::Shared_ptr s, ngs::function<void (Connection_acceptor_interface &)> callback) = 0;

  virtual void add_timer(const std::size_t delay_ms, ngs::function<bool ()> callback) = 0;
  virtual void loop() = 0;
  virtual void break_loop() = 0;
};

} // namespace ngs

#endif // NGS_SOCKET_EVENTS_INTERFACE_H_
