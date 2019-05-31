/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_NETFWD_H_
#define MYSQL_HARNESS_NET_TS_NETFWD_H_

namespace net {
class execution_context;

template <class T, class Executor>
class executor_binder;
template <class Executor>
class executor_work_guard;

class system_executor;
class executor;

template <class Executor>
class strand;

template <class Clock>
struct wait_traits;

template <class Clock, class WaitTraits = wait_traits<Clock>>
class basic_waitable_timer;

template <class Protocol>
class basic_socket;

template <class Protocol>
class basic_socket_acceptor;

template <class Protocol>
class basic_stream_socket;

}  // namespace net
#endif
