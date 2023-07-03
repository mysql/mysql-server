/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include <algorithm>
#include <list>
#include <map>
#include <system_error>
#include <thread>

#include "my_thread.h"  // my_thread_self_setname
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysqld_error.h"
#include "mysqlx_error.h"
#include "mysqlxclient/xsession.h"

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
  using native_handle_type = net::impl::socket::native_handle_type;
  static const native_handle_type kInvalidSocket{
      net::impl::socket::kInvalidSocket};

  std::string host;
  uint16_t port;

  native_handle_type fd;

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
  mysqlrouter::UserCredentials user_credentials;

  std::map<NodeId, NodeSession> sessions_;
  bool sessions_changed_{false};
  std::mutex configuration_data_mtx_;
  std::atomic<bool> mysqlx_wait_timeout_set_{false};

  std::unique_ptr<std::thread> listener_thread;
  std::atomic<bool> terminate{false};
  NotificationClb notification_callback;

  std::chrono::steady_clock::time_point last_ping_timepoint =
      std::chrono::steady_clock::now();

  Impl(const mysqlrouter::UserCredentials &auth_user_credentials)
      : user_credentials(auth_user_credentials) {}

  ~Impl();

  xcl::XError connect(NodeSession &session, NodeId &node_id);
  void listener_thread_func();
  bool read_from_session(const NodeId &node_id, NodeSession &session);

  xcl::XError enable_notices(xcl::XSession &session, const NodeId &node_id,
                             const std::string &cluster_name) noexcept;
  void set_mysqlx_wait_timeout(xcl::XSession &session,
                               const NodeId &node_id) noexcept;
  void check_mysqlx_wait_timeout();
  xcl::XError ping(xcl::XSession &session) noexcept;
  void remove_node_session(const NodeId &node) noexcept;

  void reconfigure(const metadata_cache::ClusterTopology &cluster_topology,
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
    log_debug(
        "Got notification from the cluster. type=%d; view_id=%s; Refreshing "
        "metadata.",
        change.type(), change.view_id().c_str());

    notify = true;
  }

  if (notify && notification_callback) {
    notification_callback();
  }

  return xcl::Handler_result::Continue;
}

xcl::XError GRNotificationListener::Impl::connect(NodeSession &session,
                                                  NodeId &node_id) {
  session = xcl::create_session();
  xcl::XError err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Authentication_method, "FROM_CAPABILITIES");
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                  "PREFERRED");
  if (err) return err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Consume_all_notices, false);
  if (err) return err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Session_connect_timeout,
      kXSessionConnectTimeout);
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Connect_timeout,
                                  kXSessionConnectTimeout);
  if (err) return err;

  log_debug("Connecting GR Notices listener on %s:%d", node_id.host.c_str(),
            node_id.port);
  err = session->connect(node_id.host.c_str(), node_id.port,
                         user_credentials.username.c_str(),
                         user_credentials.password.c_str(), "");
  if (err) {
    log_warning(
        "Failed connecting GR Notices listener on %s:%d; (err_code=%d; "
        "err_msg='%s')",
        node_id.host.c_str(), node_id.port, err.error(), err.what());
    return err;
  }

  node_id.fd = session->get_protocol().get_connection().get_socket_fd();

  log_debug("Connected GR Notices listener on %s:%d", node_id.host.c_str(),
            node_id.port);

  return err;
}

void GRNotificationListener::Impl::listener_thread_func() {
  const auto kPollTimeout = 50ms;
  size_t sessions_qty{0};
  std::unique_ptr<pollfd[]> fds;

  my_thread_self_setname("GR Notify");

  while (!terminate) {
    std::list<NodeSession> used_sessions;
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

      // we use the fds so we need to keep the session objects alive to prevent
      // the fds being released to the OS and reused while the poll is using
      // those fds. For that we copy the sessions shared pointers to our list,
      // it will be cleared at the end of the loop
      std::for_each(sessions_.begin(), sessions_.end(),
                    [&used_sessions](const auto &s) {
                      used_sessions.push_back(s.second);
                    });
    }

    if (sessions_qty == 0) {
      std::this_thread::sleep_for(kPollTimeout);
      continue;
    }

    if (mysqlx_wait_timeout_set_) {
      // check if we're not due for a ping to the server to avoid inactivity
      // timer disconnect
      check_mysqlx_wait_timeout();
    }

    const auto poll_res =
        net::impl::poll::poll(fds.get(), sessions_qty, kPollTimeout);
    if (!poll_res) {
      // poll has failed
      if (poll_res.error() == make_error_condition(std::errc::interrupted)) {
        // got interrupted. Sleep a bit more
        std::this_thread::sleep_for(kPollTimeout);
      } else if (poll_res.error() == make_error_code(std::errc::timed_out)) {
        // poll has timed out, sleep time already passed.
      } else {
        // any other error is fatal
        log_error(
            "poll() failed with error: %d, clearing all the sessions in the GR "
            "Notice thread",
            poll_res.error().value());
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
    xcl::XSession &session, const NodeId &node_id,
    const std::string &cluster_name) noexcept {
  log_info("Enabling GR notices for cluster '%s' changes on node %s:%u",
           cluster_name.c_str(), node_id.host.c_str(), node_id.port);
  xcl::XError err;

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

  auto stmt_result = session.execute_stmt("mysqlx", "enable_notices",
                                          {xcl::Argument_value(arg_obj)}, &err);

  if (!err) {
    log_debug(
        "Enabled GR notices for cluster changes on connection to node %s:%d",
        node_id.host.c_str(), node_id.port);
  } else if (err.error() == ER_X_BAD_NOTICE) {
    log_warning(
        "Failed enabling GR notices on the node %s:%d. This MySQL server "
        "version does not support GR notifications (err_code=%d; err_msg='%s')",
        node_id.host.c_str(), node_id.port, err.error(), err.what());
  } else {
    log_warning(
        "Failed enabling GR notices on the node %s:%d; (err_code=%d; "
        "err_msg='%s')",
        node_id.host.c_str(), node_id.port, err.error(), err.what());
  }

  return err;
}

void GRNotificationListener::Impl::set_mysqlx_wait_timeout(
    xcl::XSession &session, const NodeId &node_id) noexcept {
  xcl::XError err;
  const std::string sql_stmt = "set @@mysqlx_wait_timeout = " +
                               std::to_string(kXSesssionWaitTimeout.count());
  session.execute_sql(sql_stmt, &err);

  if (!err) {
    log_debug(
        "Successfully set mysqlx_wait_timeout on connection to node %s:%d",
        node_id.host.c_str(), node_id.port);
    mysqlx_wait_timeout_set_ = true;
  } else if (err.error() == ER_UNKNOWN_SYSTEM_VARIABLE) {
    // This version of mysqlxplugin does not support mysqlx_wait_timeout,
    // that's ok, we do not need to worry about it then
  } else {
    log_warning(
        "Failed setting mysqlx_wait_timeout on connection to node %s:%d; "
        "(err_code=%d; err_msg='%s')",
        node_id.host.c_str(), node_id.port, err.error(), err.what());
  }
}

xcl::XError GRNotificationListener::Impl::ping(
    xcl::XSession &session) noexcept {
  xcl::XError out_error;
  session.execute_stmt("mysqlx", "ping", {}, &out_error);

  return out_error;
}

void GRNotificationListener::Impl::reconfigure(
    const metadata_cache::ClusterTopology &cluster_topology,
    const NotificationClb &notification_clb) {
  std::lock_guard<std::mutex> lock(configuration_data_mtx_);

  notification_callback = notification_clb;

  const auto all_nodes = cluster_topology.get_all_members();
  // if there are connections to the nodes that are no longer required, remove
  // them first
  for (auto it = sessions_.cbegin(); it != sessions_.cend();) {
    if (std::find_if(all_nodes.begin(), all_nodes.end(),
                     [&it](const metadata_cache::ManagedInstance &i) {
                       return it->first.host == i.host &&
                              it->first.port == i.xport;
                     }) == all_nodes.end()) {
      log_info("Removing unused GR notification session to '%s:%d'",
               it->first.host.c_str(), it->first.port);
      sessions_.erase(it++);
      sessions_changed_ = true;
    } else {
      ++it;
    }
  }

  // check if there are some new nodes that we should connect to
  for (const auto &cluster : cluster_topology.clusters_data) {
    for (const auto &instance : cluster.members) {
      NodeId node_id{instance.host, instance.xport, NodeId::kInvalidSocket};
      if (std::find_if(
              sessions_.begin(), sessions_.end(),
              [&node_id](const std::pair<const NodeId, NodeSession> &node) {
                return node.first.host == node_id.host &&
                       node.first.port == node_id.port;
              }) == sessions_.end()) {
        NodeId node_id{instance.host, instance.xport, NodeId::kInvalidSocket};
        NodeSession session;
        // If we could not connect it's not fatal, we only log it and live with
        // the node not being monitored for GR notifications.
        if (connect(session, node_id)) continue;

        set_mysqlx_wait_timeout(*session, node_id);

        if (enable_notices(*session, node_id, cluster.name)) continue;

        session->get_protocol().add_notice_handler(
            [this](const xcl::XProtocol *protocol, const bool is_global,
                   const Mysqlx::Notice::Frame::Type type, const char *data,
                   const uint32_t data_length) -> xcl::Handler_result {
              return notice_handler(protocol, is_global, type, data,
                                    data_length);
            });

        sessions_[node_id] = std::move(session);
        sessions_changed_ = true;
      }
    }
  }

  if (!listener_thread)
    listener_thread.reset(
        new std::thread([this]() { listener_thread_func(); }));
}

GRNotificationListener::GRNotificationListener(
    const mysqlrouter::UserCredentials &user_credentials)
    : impl_(new Impl(user_credentials)) {}

GRNotificationListener::~GRNotificationListener() = default;

void GRNotificationListener::setup(
    const metadata_cache::ClusterTopology &cluster_topology,
    const NotificationClb &notification_clb) {
  impl_->reconfigure(cluster_topology, notification_clb);
}
