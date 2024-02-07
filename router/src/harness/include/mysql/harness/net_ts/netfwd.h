/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

class io_context;

template <class Clock>
struct wait_traits;

template <class Clock, class WaitTraits = wait_traits<Clock>>
class basic_waitable_timer;

template <class Protocol>
class basic_socket;

template <class Protocol>
class basic_datagram_socket;

template <class Protocol>
class basic_stream_socket;

template <class Protocol>
class basic_socket_acceptor;

#if 0
// not implemented yet
template <class Protocol, class Clock = std::chrono::steady_clock,
          class WaitTraits = wait_traits<Clock>>
class basic_socket_streambuf;

template <class Protocol, class Clock = std::chrono::steady_clock,
          class WaitTraits = wait_traits<Clock>>
class basic_socket_iostream;
#endif

namespace ip {
class address;
class address_v4;
class address_v6;

template <class Address>
class basic_address_iterator;

using address_v4_iterator = basic_address_iterator<address_v4>;
using address_v6_iterator = basic_address_iterator<address_v6>;

template <class Address>
class basic_address_range;
using address_v4_range = basic_address_range<address_v4>;
using address_v6_range = basic_address_range<address_v6>;

class network_v4;
class network_v6;

template <class InternetProtocol>
class basic_endpoint;

template <class InternetProtocol>
class basic_resolver_entry;

template <class InternetProtocol>
class basic_resolver_results;

template <class InternetProtocol>
class basic_resolver;

class tcp;
class udp;
}  // namespace ip

}  // namespace net
#endif
