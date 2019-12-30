/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "gr_notifications_listener.h"

#include "common.h"  // rename_thread()
#include "mysql/harness/logging/logging.h"
#include "mysqlxclient/xsession.h"
#include "socket_operations.h"

#include <algorithm>
#include <map>
#include <thread>

IMPORT_LOG_FUNCTIONS()

// 10 seconds (int64_t is required by the xclient API)
const int64_t kXSessionConnectTimeout = 10000;

// 8 hours - this is the session inactivity timer; it's a default but can be
// changed globally and we need to make sure that it is set to that value so
// that we knew how often we need to send a ping through the connection to
// prevent the server from closing the connection
const auto kXSesssionWaitTimeout = std::chrono::seconds(28800);
// to stay on the safe side we send a ping packet through the connection every
// half of mysqlx_connection_timeout to restart the timer
const auto kXSesssionPingTimeout = kXSesssionWaitTimeout / 2;

namespace {
struct NodeId {
  std::string host;
  uint16_t port;
  int fd;

  bool operator==(const NodeId &other) const {
    if (host != other.host) return false;
    if (port != other.port) return false;

    return fd == other.fd;
  }

  // it's needed to use NodeId as a key in the std::map
  bool operator<(const NodeId &other) const {
    if (host != other.host) return host < other.host;
    if (port != other.port) return port < other.port;

    return fd < other.fd;
  }
};

using NodeSession = std::shared_ptr<xcl::XSession>;
}  // namespace

using namespace std::chrono_literals;

struct GRNotificationListener::Impl {
  std::string user_name;
  std::string password;

  std::map<NodeId, NodeSession> sessions_;
  bool sessions_changed_{false};
  std::mutex configuration_data_mtx_;

  std::unique_ptr<std::thread> listener_thread;
  std::atomic<bool> terminate{false};
  NotificationClb notification_callback;
  std::string last_view_id;

  std::chrono::steady_clock::time_point last_ping_timepoint =
      std::chrono::steady_clock::now();

  Impl(const std::string &auth_user_name, const std::string &auth_password)
      : user_name(auth_user_name), password(auth_password) {}

  ~Impl();

  NodeSession connect(NodeId &node_id, xcl::XError &out_xerror);
  void listener_thread_func();
  bool read_from_session(const NodeId &node_id, NodeSession &session);

  xcl::XError enable_notices(xcl::XSession &session,
                             const NodeId &node_id) noexcept;
  xcl::XError set_mysqlx_wait_timeout(xcl::XSession &session,
                                      const NodeId &node_id) noexcept;
  void check_mysqlx_wait_timeout();
  xcl::XError ping(xcl::XSession &session) noexcept;
  void remove_node_session(const NodeId &node) noexcept;

  void reconfigure(
      const std::vector<metadata_cache::ManagedInstance> &instances,
      const NotificationClb &notification_clb);

  // handles the notice from the session
  xcl::Handler_result notice_handler(const xcl::XProtocol *,
                                     const bool /*is_global*/,
                                     const Mysqlx::Notice::Frame::Type type,
                                     const char *, const uint32_t);
};

xcl::Handler_result GRNotificationListener::Impl::notice_handler(
    const xcl::XProtocol *, const bool /*is_global*/,
    const Mysqlx::Notice::Frame::Type type, const char *payload,
    const uint32_t payload_size) {
  bool notify = false;
  if (type ==
      Mysqlx::Notice::Frame::Type::Frame_Type_GROUP_REPLICATION_STATE_CHANGED) {
    Mysqlx::Notice::GroupReplicationStateChanged change;
    change.ParseFromArray(payload, static_cast<int>(payload_size));
    log_debug("Got notification from the cluster. type=%d; view_id=%s; ",
              change.type(), change.view_id().c_str());

    const bool view_id_changed =
        change.view_id().empty() || (change.view_id() != last_view_id);
    if (view_id_changed) {
      log_debug(
          "Cluster notification: new view_id='%s'; previous view_id='%s'. "
          "Refreshing metadata.",
          change.view_id().c_str(), last_view_id.c_str());

      notify = true;
      last_view_id = change.view_id();
    }
  }

  if (notify && notification_callback) {
    notification_callback();
  }

  return xcl::Handler_result::Continue;
}

NodeSession GRNotificationListener::Impl::connect(NodeId &node_id,
                                                  xcl::XError &out_xerror) {
  NodeSession session{xcl::create_session()};

  out_xerror = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Authentication_method, "FROM_CAPABILITIES");
  if (out_xerror) return nullptr;

  out_xerror = session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                         "PREFERRED");
  if (out_xerror) return nullptr;

  out_xerror = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Consume_all_notices, false);
  if (out_xerror) return nullptr;

  out_xerror = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Session_connect_timeout,
      kXSessionConnectTimeout);
  if (out_xerror) return nullptr;

  out_xerror = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Connect_timeout, kXSessionConnectTimeout);
  if (out_xerror) return nullptr;

  log_debug("Connecting GR Notices listener on %s:%d", node_id.host.c_str(),
            node_id.port);
  out_xerror = session->connect(node_id.host.c_str(), node_id.port,
                                user_name.c_str(), password.c_str(), "");
  if (out_xerror) {
    log_warning(
        "Failed connecting GR Notices listener on %s:%d; (err_code=%d; "
        "err_msg='%s')",
        node_id.host.c_str(), node_id.port, out_xerror.error(),
        out_xerror.what());
    return nullptr;
  }

  log_debug("Connected GR Notices listener on %s:%d", node_id.host.c_str(),
            node_id.port);

  out_xerror = set_mysqlx_wait_timeout(*session, node_id);
  if (out_xerror) return nullptr;

  out_xerror = enable_notices(*session, node_id);
  if (out_xerror) return nullptr;

  node_id.fd = session->get_protocol().get_connection().get_socket_fd();
  return session;
}

void GRNotificationListener::Impl::listener_thread_func() {
  const auto kPollTimeout = 50ms;
  size_t sessions_qty{0};
  std::unique_ptr<pollfd[]> fds;

  mysql_harness::rename_thread("GR Notify");

  while (!terminate) {
    // first check if the set of fds did not change and we shouldn't reconfigure
    {
      std::lock_guard<std::mutex> lock(configuration_data_mtx_);
      if (sessions_changed_) {
        sessions_qty = sessions_.size();
        fds.reset(new pollfd[sessions_qty]);

        size_t i = 0;
        auto fds_array = fds.get();
        for (const auto &session : sessions_) {
          const auto fd = static_cast<int>(
              session.second->get_protocol().get_connection().get_socket_fd());
          fds_array[i].fd = fd;
          fds_array[i].events = POLLIN;
          ++i;
        }
        sessions_changed_ = false;
      }
    }

    if (sessions_qty == 0) {
      std::this_thread::sleep_for(kPollTimeout);
      continue;
    }

    // check if we're not due for a ping to the server to avoid inactivity timer
    // disconnect
    check_mysqlx_wait_timeout();

    const int poll_res = mysql_harness::SocketOperations::instance()->poll(
        fds.get(), sessions_qty, kPollTimeout);
    if (poll_res <= 0) {
      // poll has timed out or failed
      const int err_no =
          mysql_harness::SocketOperations::instance()->get_errno();
      // if this is timeout or EINTR just sleep and go to the next iteration
      if (poll_res == 0 || EINTR == err_no) {
        std::this_thread::sleep_for(kPollTimeout);
      } else {
        // any other error is fatal
        log_error(
            "poll() failed with error: %d, clearing all the sessions in the GR "
            "Notice thread",
            err_no);
        sessions_.clear();
        sessions_changed_ = true;
      }
      continue;
    }

    // read from the nodes that sent something
    for (size_t i = 0; i < sessions_qty; ++i) {
      const pollfd poll_res = fds.get()[i];
      if ((poll_res.revents & (POLLIN | POLLHUP)) == 0) continue;

      std::pair<NodeId, NodeSession> session;
      {
        std::lock_guard<std::mutex> lock(configuration_data_mtx_);
        auto session_iter = std::find_if(
            sessions_.begin(), sessions_.end(),
            [&poll_res](const std::pair<const NodeId, NodeSession> &s) {
              return s.first.fd == poll_res.fd;
            });
        if (session_iter == sessions_.end()) continue;
        // copy the session before releasing the mutex
        session = *session_iter;
      }

      log_debug(
          "GR notification listen thread has read sth from %s:%d on fd=%d",
          session.first.host.c_str(), session.first.port, session.first.fd);

      do {
        bool read_ok = read_from_session(session.first, session.second);
        if (!read_ok) {
          remove_node_session(session.first);
          break;
        }
      } while (
          session.second->get_protocol().get_connection().state().has_data());
    }
  }
}

void GRNotificationListener::Impl::check_mysqlx_wait_timeout() {
  const auto seconds_since_last_ping =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - last_ping_timepoint);
  if (seconds_since_last_ping >= kXSesssionPingTimeout) {
    log_debug(
        "Sending ping on x protocol connections to reset inactivity timer");
    decltype(sessions_) sessions_copy;
    {
      std::lock_guard<std::mutex> lock(configuration_data_mtx_);
      sessions_copy = sessions_;
    }
    for (auto &session : sessions_copy) {
      auto error = ping(*session.second.get());
      if (error) {
        log_warning(
            "Failed sending ping on connection to %s:%d; (err_code=%d; "
            "err_msg='%s')",
            session.first.host.c_str(), session.first.port, error.error(),
            error.what());
      } else {
        log_debug("Successfully sent ping on connection to %s:%d",
                  session.first.host.c_str(), session.first.port);
      }
    }
    last_ping_timepoint = std::chrono::steady_clock::now();
  }
}

bool GRNotificationListener::Impl::read_from_session(const NodeId &node_id,
                                                     NodeSession &session) {
  auto &protocol = session->get_protocol();
  bool notify = false, result = true;

  xcl::XProtocol::Server_message_type_id msg_id;
  xcl::XError recv_err;
  auto msg = protocol.recv_single_message(&msg_id, &recv_err);
  if (recv_err) {
    log_warning(
        "Cluster notification connection: error reading from the server %s:%d; "
        "(err_code=%d; err_msg='%s')",
        node_id.host.c_str(), node_id.port, recv_err.error(), recv_err.what());

    result = false;
    notify = true;
  } else {
    log_debug("Got message from cluster: %d", static_cast<int>(msg_id));
    // we do not really care about the message, we just had to read it to remove
    // from the socket; if it was a notice, the notice handler should have
    // handled it
  }

  {
    std::lock_guard<std::mutex> lock(configuration_data_mtx_);
    if (notify && notification_callback) {
      notification_callback();
    }
  }

  return result;
}

void GRNotificationListener::Impl::remove_node_session(
    const NodeId &node) noexcept {
  bool do_log_warning = false;
  {
    std::lock_guard<std::mutex> lock(configuration_data_mtx_);
    auto session_iter =
        std::find_if(sessions_.begin(), sessions_.end(),
                     [&node](const std::pair<const NodeId, NodeSession> &s) {
                       return s.first == node;
                     });
    if (session_iter != sessions_.end()) {
      sessions_.erase(session_iter);
      sessions_changed_ = true;
      do_log_warning = true;
    }
  }

  if (do_log_warning)
    log_warning("Removing the node %s:%d from the notification thread",
                node.host.c_str(), node.port);
}

GRNotificationListener::Impl::~Impl() {
  terminate = true;
  if (listener_thread) listener_thread->join();
}

/**
 * Replication events are delivered to the client only when client explicitly
 * request them. This function uses enable_notice to request notifications
 * for all 4 event types:
 * - quorum lost
 * - view
 * - role_changed
 * - state_changed
 */
xcl::XError GRNotificationListener::Impl::enable_notices(
    xcl::XSession &session, const NodeId &node_id) noexcept {
  log_info("Enabling notices for cluster changes");
  xcl::XError out_error;

  xcl::Argument_value::Object arg_obj;
  xcl::Argument_value arg_value;
  arg_value = arg_obj;

  arg_obj["notice"] = xcl::Argument_value::Arguments{
      xcl::Argument_value("group_replication/membership/quorum_loss",
                          xcl::Argument_value::String_type::k_string),
      xcl::Argument_value("group_replication/membership/view",
                          xcl::Argument_value::String_type::k_string),
      xcl::Argument_value("group_replication/status/role_change",
                          xcl::Argument_value::String_type::k_string),
      xcl::Argument_value("group_replication/status/state_change",
                          xcl::Argument_value::String_type::k_string)};

  auto stmt_result = session.execute_stmt(
      "mysqlx", "enable_notices", {xcl::Argument_value(arg_obj)}, &out_error);

  if (!out_error) {
    log_debug("Enabled notices for cluster changes on connection to node %s:%d",
              node_id.host.c_str(), node_id.port);
  } else {
    log_warning(
        "Failed sending ping to node %s:%d; (err_code=%d; err_msg='%s')",
        node_id.host.c_str(), node_id.port, out_error.error(),
        out_error.what());
  }
  return out_error;
}

xcl::XError GRNotificationListener::Impl::set_mysqlx_wait_timeout(
    xcl::XSession &session, const NodeId &node_id) noexcept {
  xcl::XError out_error;
  const std::string sql_stmt = "set @@mysqlx_wait_timeout = " +
                               std::to_string(kXSesssionWaitTimeout.count());
  session.execute_sql(sql_stmt, &out_error);

  if (!out_error) {
    log_debug(
        "Successfully set mysqlx_wait_timeout on connection to node %s:%d",
        node_id.host.c_str(), node_id.port);
  } else {
    log_warning(
        "Failed setting mysqlx_wait_timeout on connection to node %s:%d; "
        "(err_code=%d; err_msg='%s')",
        node_id.host.c_str(), node_id.port, out_error.error(),
        out_error.what());
  }
  return out_error;
}

xcl::XError GRNotificationListener::Impl::ping(
    xcl::XSession &session) noexcept {
  xcl::XError out_error;
  session.execute_stmt("xplugin", "ping", {}, &out_error);

  return out_error;
}

void GRNotificationListener::Impl::reconfigure(
    const std::vector<metadata_cache::ManagedInstance> &instances,
    const NotificationClb &notification_clb) {
  std::lock_guard<std::mutex> lock(configuration_data_mtx_);

  notification_callback = notification_clb;

  // if there are connections to the nodes that are no longer required, remove
  // them first
  for (auto it = sessions_.cbegin(); it != sessions_.cend();) {
    if (std::find_if(instances.begin(), instances.end(),
                     [&it](const metadata_cache::ManagedInstance &i) {
                       return it->first.host == i.host &&
                              it->first.port == i.xport;
                     }) != instances.end()) {
      sessions_.erase(it++);
      sessions_changed_ = true;
    } else {
      ++it;
    }
  }

  // check if there are some new nodes that we should connect to
  for (const auto &instance : instances) {
    NodeId node_id{instance.host, instance.xport, -1};
    if (std::find_if(
            sessions_.begin(), sessions_.end(),
            [&node_id](const std::pair<const NodeId, NodeSession> &node) {
              return node.first.host == node_id.host &&
                     node.first.port == node_id.port;
            }) == sessions_.end()) {
      NodeId node_id{instance.host, instance.xport, -1};
      xcl::XError xerror;
      auto session = connect(node_id, xerror);

      // If we could not connect it's not fatal, we only log it and live with
      // the node not being monitored for GR notifications.
      if (!session) {
        log_warning(
            "Could not create notification connection to the node %s:%d. "
            "(err_code=%d; err_msg='%s')",
            node_id.host.c_str(), node_id.port, xerror.error(), xerror.what());
        continue;
      }

      session->get_protocol().add_notice_handler(
          [this](const xcl::XProtocol *protocol, const bool is_global,
                 const Mysqlx::Notice::Frame::Type type, const char *data,
                 const uint32_t data_length) -> xcl::Handler_result {
            return notice_handler(protocol, is_global, type, data, data_length);
          });

      sessions_[node_id] = std::move(session);
      sessions_changed_ = true;
    }
  }

  if (!listener_thread)
    listener_thread.reset(
        new std::thread([this]() { listener_thread_func(); }));
}

GRNotificationListener::GRNotificationListener(const std::string &user_name,
                                               const std::string &password)
    : impl_(new Impl(user_name, password)) {}

GRNotificationListener::~GRNotificationListener() = default;

void GRNotificationListener::setup(
    const std::vector<metadata_cache::ManagedInstance> &instances,
    const NotificationClb &notification_clb) {
  impl_->reconfigure(instances, notification_clb);
}
