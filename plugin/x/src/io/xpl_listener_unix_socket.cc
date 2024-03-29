/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/io/xpl_listener_unix_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "my_config.h"  // NOLINT(build/include_subdir)
#include "my_io.h"      // NOLINT(build/include_subdir)

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/helper/string_formatter.h"
#include "plugin/x/src/operations_factory.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_performance_schema.h"

#ifdef HAVE_SYS_UN_H
#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#endif

namespace xpl {

class Unixsocket_creator {
 public:
  explicit Unixsocket_creator(iface::Operations_factory &operations_factory)
      : m_operations_factory(operations_factory),
        m_system_interface(operations_factory.create_system_interface()) {}

  void unlink_unixsocket_file(const std::string &unix_socket_file) {
    if (unix_socket_file.empty()) return;

    if (!m_system_interface) return;

    const std::string unix_socket_lockfile =
        get_unixsocket_lockfile_name(unix_socket_file);

    (void)m_system_interface->unlink(unix_socket_file.c_str());
    (void)m_system_interface->unlink(unix_socket_lockfile.c_str());
  }

  std::shared_ptr<iface::Socket> create_and_bind_unixsocket(
      const std::string &unix_socket_file, std::string &error_message,
      const uint32_t backlog) {
    std::shared_ptr<iface::Socket> listener_socket(
        m_operations_factory.create_socket(MYSQL_INVALID_SOCKET));

#if defined(HAVE_SYS_UN_H)
    struct sockaddr_un addr;
    int err;
    std::string errstr;

    log_debug("UNIX Socket is %s", unix_socket_file.c_str());

    if (unix_socket_file.empty()) {
      log_debug("UNIX socket not configured");
      error_message = "the socket file path is empty";

      return listener_socket;
    }

    // Check path length, probably move to set unix port?
    if (unix_socket_file.length() > (sizeof(addr.sun_path) - 1)) {
      error_message = String_formatter()
                          .append("the socket file path is too long (> ")
                          .append(sizeof(addr.sun_path) - 1)
                          .append(")")
                          .get_result();

      return listener_socket;
    }

    if (!create_unixsocket_lockfile(unix_socket_file, error_message)) {
      return listener_socket;
    }

    listener_socket = m_operations_factory.create_socket(
        KEY_socket_x_unix, AF_UNIX, SOCK_STREAM, 0);

    if (INVALID_SOCKET == listener_socket->get_socket_fd()) {
      m_system_interface->get_socket_error_and_message(&err, &errstr);
      error_message = String_formatter()
                          .append("can't create UNIX Socket: ")
                          .append(errstr)
                          .append(" (")
                          .append(err)
                          .append(")")
                          .get_result();

      return listener_socket;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    my_stpcpy(addr.sun_path, unix_socket_file.c_str());
    m_system_interface->unlink(unix_socket_file.c_str());

    // bind
    int old_mask = umask(0);
    if (listener_socket->bind(reinterpret_cast<struct sockaddr *>(&addr),
                              sizeof(addr)) < 0) {
      umask(old_mask);
      m_system_interface->get_socket_error_and_message(&err, &errstr);
      error_message = String_formatter()
                          .append("`bind()` on UNIX socket failed with error: ")
                          .append(errstr)
                          .append(" (")
                          .append(err)
                          .append("). ")
                          .append(
                              " Do you already have another mysqld server "
                              "running with Mysqlx ?")
                          .get_result();

      listener_socket->close();

      return listener_socket;
    }
    umask(old_mask);

    // listen
    if (listener_socket->listen(backlog) < 0) {
      m_system_interface->get_socket_error_and_message(&err, &errstr);

      error_message =
          String_formatter()
              .append("`listen()` on UNIX socket failed with error: ")
              .append(errstr)
              .append("(")
              .append(err)
              .append(")")
              .get_result();

      listener_socket->close();

      return listener_socket;
    }
    listener_socket->set_socket_thread_owner();
#endif  // defined(HAVE_SYS_UN_H)

    return listener_socket;
  }

 private:
  std::string get_unixsocket_lockfile_name(
      const std::string &unix_socket_file) {
    return unix_socket_file + ".lock";
  }

  bool create_unixsocket_lockfile(const std::string &unix_socket_file,
                                  std::string &error_message) {
    std::shared_ptr<iface::File> lockfile_fd;
#if !defined(HAVE_SYS_UN_H)
    return false;
#else
    char buffer[8];
    char *pid_begin = buffer;
    const char x_prefix = 'X';
    const pid_t cur_pid = m_system_interface->get_pid();
    const std::string lock_filename =
        get_unixsocket_lockfile_name(unix_socket_file);

    int retries = 3;
    while (true) {
      if (!retries--) {
        error_message = String_formatter()
                            .append("unable to create UNIX socket lock file ")
                            .append(lock_filename)
                            .append(" after ")
                            .append(retries)
                            .append(" retries")
                            .get_result();

        return false;
      }

      lockfile_fd = m_operations_factory.open_file(
          lock_filename.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);

      if (lockfile_fd->is_valid()) break;

      if (EEXIST != m_system_interface->get_errno()) {
        error_message = "can't create lock file ";
        error_message += lock_filename;

        return false;
      }

      lockfile_fd =
          m_operations_factory.open_file(lock_filename.c_str(), O_RDONLY, 0600);
      if (!lockfile_fd->is_valid()) {
        error_message = "can't open lock file ";
        error_message += lock_filename;

        return false;
      }

      ssize_t len = 0;
      ssize_t read_result = 1;

      while (read_result) {
        if ((read_result = lockfile_fd->read(buffer + len,
                                             sizeof(buffer) - 1 - len)) < 0) {
          error_message = "can't read lock file ";
          error_message += lock_filename;

          return false;
        }

        len += read_result;
      }

      lockfile_fd->close();

      if (len == 0) {
        error_message = "lock file is empty";

        return false;
      }
      buffer[len] = '\0';

      if (x_prefix == pid_begin[0]) {
        ++pid_begin;
      }

      pid_t parent_pid = m_system_interface->get_ppid();
      pid_t read_pid = atoi(pid_begin);

      if (read_pid <= 0) {
        error_message = "invalid PID in UNIX socket lock file ";
        error_message += lock_filename;

        return false;
      }

      if (read_pid != cur_pid && read_pid != parent_pid) {
        if (m_system_interface->kill(read_pid, 0) == 0) {
          error_message = String_formatter()
                              .append("another process with PID ")
                              .append(read_pid)
                              .append(" is using UNIX socket file")
                              .get_result();
          return false;
        }
      }

      /*
        Unlink the lock file as it is not associated with any process and
        retry.
      */
      if (m_system_interface->unlink(lock_filename.c_str()) < 0) {
        error_message = "can't remove UNIX socket lock file ";
        error_message += lock_filename;

        return false;
      }
    }

    snprintf(buffer, sizeof(buffer), "%d\n", static_cast<int>(cur_pid));
    if (lockfile_fd->write(buffer, strlen(buffer)) !=
        static_cast<signed>(strlen(buffer))) {
      error_message = String_formatter()
                          .append("can't write UNIX socket lock file ")
                          .append(lock_filename)
                          .append(", errno: ")
                          .append(errno)
                          .get_result();

      return false;
    }

    if (lockfile_fd->fsync() != 0) {
      error_message = String_formatter()
                          .append("can't sync UNIX socket lock file ")
                          .append(lock_filename)
                          .append(", errno: ")
                          .append(errno)
                          .get_result();

      return false;
    }

    if (lockfile_fd->close() != 0) {
      error_message = String_formatter()
                          .append("can't close UNIX socket lock file ")
                          .append(lock_filename)
                          .append(", errno: ")
                          .append(errno)
                          .get_result();

      return false;
    }

    return true;
#endif  // defined(HAVE_SYS_UN_H)
  }

  iface::Operations_factory &m_operations_factory;
  std::shared_ptr<iface::System> m_system_interface;
};

Listener_unix_socket::Listener_unix_socket(
    std::shared_ptr<iface::Operations_factory> operations_factory,
    const std::string &unix_socket_path, iface::Socket_events &event,
    const uint32_t backlog)
    : m_operations_factory(operations_factory),
      m_unix_socket_path(unix_socket_path),
      m_backlog(backlog),
      m_state(iface::Listener::State::k_initializing,
              KEY_mutex_x_listener_unix_socket_sync,
              KEY_cond_x_listener_unix_socket_sync),
      m_event(event) {}

Listener_unix_socket::~Listener_unix_socket() {
  // close_listener() can be called multiple times, by user + from destructor
  close_listener();
}

const Listener_unix_socket::Sync_variable_state &
Listener_unix_socket::get_state() const {
  return m_state;
}

std::string Listener_unix_socket::get_configuration_variable() const {
  return MYSQLX_SYSTEM_VARIABLE_PREFIX("socket");
}

bool Listener_unix_socket::setup_listener(On_connection on_connection) {
  Unixsocket_creator unixsocket_creator(*m_operations_factory);

  if (!m_state.is(iface::Listener::State::k_initializing)) {
    close_listener();
    return false;
  }

  m_unix_socket = unixsocket_creator.create_and_bind_unixsocket(
      m_unix_socket_path, m_last_error, m_backlog);

  if (INVALID_SOCKET == m_unix_socket->get_socket_fd()) {
    close_listener();
    return false;
  }

  if (!m_event.listen(m_unix_socket, on_connection)) {
    close_listener();
    return false;
  }

  m_state.set(iface::Listener::State::k_prepared);

  return true;
}

void Listener_unix_socket::close_listener() {
  if (iface::Listener::State::k_stopped ==
      m_state.set_and_return_old(iface::Listener::State::k_stopped))
    return;

  if (nullptr == m_unix_socket) return;

  const bool should_unlink_unix_socket =
      INVALID_SOCKET != m_unix_socket->get_socket_fd();
  m_unix_socket->close();

  if (!should_unlink_unix_socket) return;

  Unixsocket_creator unixsocket_creator(*m_operations_factory);
  unixsocket_creator.unlink_unixsocket_file(m_unix_socket_path);
}

void Listener_unix_socket::pre_loop() {
  if (m_unix_socket) m_unix_socket->set_socket_thread_owner();
  m_state.set(xpl::iface::Listener::State::k_running);
}

void Listener_unix_socket::loop() {}

void Listener_unix_socket::report_properties(On_report_properties on_prop) {
  switch (m_state.get()) {
    case State::k_initializing:
      on_prop(ngs::Server_property_ids::k_unix_socket, "");
      break;

    case State::k_prepared:
    case State::k_running:  // fall-through
      on_prop(ngs::Server_property_ids::k_unix_socket, m_unix_socket_path);
      break;

    case State::k_stopped:
      on_prop(ngs::Server_property_ids::k_unix_socket,
              ngs::PROPERTY_NOT_CONFIGURED);
      break;
  }
}

bool Listener_unix_socket::report_status() const {
  const std::string msg = "socket: '" + m_unix_socket_path + "'";

  if (m_state.is(State::k_prepared)) {
    log_info(ER_XPLUGIN_LISTENER_STATUS_MSG, msg.c_str());
    return true;
  }

  log_error(ER_XPLUGIN_LISTENER_SETUP_FAILED, msg.c_str(),
            m_last_error.c_str());
  return false;
}

}  // namespace xpl
