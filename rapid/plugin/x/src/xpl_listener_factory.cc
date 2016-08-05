/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs/time_socket_events.h"
#include "ngs/interface/listener_interface.h"
#include "ngs_common/connection_vio.h"
#include "xpl_listener_factory.h"
#include "mysqlx_version.h"

using namespace xpl;

namespace details
{

class Tcp_listener: public ngs::Listener_interface
{
public:
  Tcp_listener(const unsigned short port, ngs::Time_and_socket_events &event, const uint32 backlog)
  : m_state(ngs::State_listener_initializing),
    m_port(port),
    m_event(event)
  {
    m_tcp_socket = ngs::Connection_vio::create_and_bind_socket(port, m_last_error, backlog);
  }

  ~Tcp_listener()
  {
    // close_listener() can be called multiple times, by user + from destructor
    close_listener();
  }

  Sync_variable_state &get_state()
  {
    return m_state;
  }

  bool is_handled_by_socket_event()
  {
    return true;
  }

  std::string get_last_error()
  {
    return m_last_error;
  }

  std::string get_name_and_configuration() const
  {
    char to_string_buffer[32];

    sprintf(to_string_buffer, "%i", m_port);

    std::string result = "TCP (port:";

    result += to_string_buffer;
    result += ")";

    return result;
  }

  std::string get_configuration_variable() const
  {
    return MYSQLX_SYSTEM_VARIABLE_PREFIX("port");
  }

  bool setup_listener(On_connection on_connection)
  {
    if (INVALID_SOCKET == m_tcp_socket)
      return false;

    if (m_event.listen(m_tcp_socket, on_connection))
    {
      m_state.set(ngs::State_listener_prepared);
      return true;
    }

    return false;
  }

  void close_listener()
  {
    // Connection_vio::close_socket can be called multiple times
    // it invalidates the content of m_tcp_socket thus at next call
    // it does nothing
    //
    // Same applies to close_listener()
    ngs::Connection_vio::close_socket(m_tcp_socket);
  }

  void loop()
  {
  }

private:
  Sync_variable_state m_state;
  const unsigned short m_port;
  my_socket m_tcp_socket;
  ngs::Time_and_socket_events &m_event;
  std::string m_last_error;
};


class Unix_socket_listener: public ngs::Listener_interface
{
public:
  Unix_socket_listener(const std::string &unix_socket_path, ngs::Time_and_socket_events &event, const uint32 backlog)
  : m_state(ngs::State_listener_initializing),
    m_unix_socket_path(unix_socket_path),
    m_unix_socket(INVALID_SOCKET),
    m_event(event)
  {
#if !defined(HAVE_SYS_UN_H)
    m_state.set(ngs::State_listener_stopped);
#else
    m_unix_socket = ngs::Connection_vio::create_and_bind_socket(unix_socket_path, m_last_error, backlog);
#endif // !defined(HAVE_SYS_UN_H)
  }

  ~Unix_socket_listener()
  {
    // close_listener() can be called multiple times, by user + from destructor
    close_listener();
  }

  Sync_variable_state &get_state()
  {
    return m_state;
  }

  bool is_handled_by_socket_event()
  {
    return true;
  }

  std::string get_name_and_configuration() const
  {
    std::string result = "UNIX socket (";

    result += m_unix_socket_path;
    result += ")";

    return result;
  }

  std::string get_configuration_variable() const
  {
    return MYSQLX_SYSTEM_VARIABLE_PREFIX("socket");
  }

  std::string get_last_error()
  {
    return m_last_error;
  }

  bool setup_listener(On_connection on_connection)
  {
    if (!m_state.is(ngs::State_listener_initializing))
      return false;

    if (INVALID_SOCKET == m_unix_socket)
      return false;

    if (m_event.listen(m_unix_socket, on_connection))
    {
      m_state.set(ngs::State_listener_prepared);
      return true;
    }

    return false;
  }

  void close_listener()
  {
    const bool should_unlink_unix_socket = INVALID_SOCKET != m_unix_socket;
    ngs::Connection_vio::close_socket(m_unix_socket);

    if (should_unlink_unix_socket)
      ngs::Connection_vio::unlink_unix_socket_file(m_unix_socket_path);
  }

  void loop()
  {
  }

private:
  Sync_variable_state m_state;
  const std::string m_unix_socket_path;
  my_socket m_unix_socket;
  ::ngs::Time_and_socket_events &m_event;
  std::string m_last_error;
  bool m_unlink_unix_socket;
};

} // namespace details


ngs::Listener_interface_ptr Listener_factory::create_unix_socket_listener(const std::string &unix_socket_path, ngs::Time_and_socket_events &event, const uint32 backlog)
{
  return ngs::Listener_interface_ptr(
      new details::Unix_socket_listener(unix_socket_path, event, backlog));
}

ngs::Listener_interface_ptr Listener_factory::create_tcp_socket_listener(const unsigned short port, ngs::Time_and_socket_events &event, const uint32 backlog)
{
  return ngs::Listener_interface_ptr(
      new details::Tcp_listener(port, event, backlog));
}
