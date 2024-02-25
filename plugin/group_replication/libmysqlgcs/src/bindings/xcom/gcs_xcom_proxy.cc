/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_proxy.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_psi.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/leader_info_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/include/network_management_interface.h"
#include "xdr_gen/xcom_vp.h"

/*
  Time is defined in seconds.
*/
static const unsigned int WAITING_TIME = 30;

void Gcs_xcom_proxy_impl::delete_node_address(unsigned int n,
                                              node_address *na) {
  ::delete_node_address(n, na);
}

bool Gcs_xcom_proxy_impl::xcom_client_close_connection(
    connection_descriptor *fd) {
  return !static_cast<bool>(::close_open_connection(fd));
}

connection_descriptor *Gcs_xcom_proxy_impl::xcom_client_open_connection(
    std::string saddr, xcom_port port) {
  const char *addr = saddr.c_str();
  return ::open_new_connection(addr, port);
}

bool Gcs_xcom_proxy_impl::xcom_client_add_node(connection_descriptor *fd,
                                               node_list *nl, uint32_t gid) {
  MYSQL_GCS_LOG_INFO("Sending add_node request to a peer XCom node");
  bool const successful = (::xcom_client_add_node(fd, nl, gid) == 1);
  if (!successful) {
    MYSQL_GCS_LOG_INFO("Failed to send add_node request to a peer XCom node.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_remove_node(node_list *nl, uint32_t gid) {
  app_data_ptr data = new_app_data();
  data = init_config_with_group(data, nl, remove_node_type, gid);
  /* Takes ownership of data. */
  MYSQL_GCS_LOG_INFO(
      "xcom_client_remove_node: Try to push xcom_client_remove_node to XCom");
  bool const successful = xcom_input_try_push(data);
  if (!successful) {
    MYSQL_GCS_LOG_INFO("xcom_client_remove_node: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_remove_node(connection_descriptor *fd,
                                                  node_list *nl,
                                                  uint32_t group_id) {
  bool const successful = (::xcom_client_remove_node(fd, nl, group_id) == 1);
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_get_event_horizon(
    uint32_t gid, xcom_event_horizon &event_horizon) {
  bool successful = false;
  app_data_ptr data = new_app_data();
  data = init_get_event_horizon_msg(data, gid);
  /* Takes ownership of data. */
  Gcs_xcom_input_queue::future_reply future =
      xcom_input_try_push_and_get_reply(data);
  std::unique_ptr<Gcs_xcom_input_queue::Reply> reply = future.get();
  bool const processable_reply =
      (reply.get() != nullptr && reply->get_payload() != nullptr);
  if (processable_reply) {
    bool const reply_ok = (reply->get_payload()->cli_err == REQUEST_OK);
    if (reply_ok) {
      event_horizon = reply->get_payload()->event_horizon;
      successful = true;
    } else {
      MYSQL_GCS_LOG_DEBUG(
          "xcom_client_get_event_horizon: Couldn't fetch the event horizon. "
          "(cli_err=%d)",
          reply->get_payload()->cli_err);
    }
  } else {
    MYSQL_GCS_LOG_DEBUG(
        "xcom_client_get_event_horizon: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_set_event_horizon(
    uint32_t gid, xcom_event_horizon event_horizon) {
  app_data_ptr data = new_app_data();
  data = init_set_event_horizon_msg(data, gid, event_horizon);
  /* Takes ownership of data. */
  bool const successful = xcom_input_try_push(data);
  if (!successful) {
    MYSQL_GCS_LOG_DEBUG(
        "xcom_client_set_event_horizon: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_set_leaders(
    uint32_t gid, u_int nr_preferred_leaders, char const *preferred_leaders[],
    node_no max_nr_leaders) {
  app_data_ptr data = new_app_data();
  init_set_leaders(gid, data, nr_preferred_leaders, preferred_leaders,
                   new_app_data(), max_nr_leaders);
  /* Takes ownership of data. */
  Gcs_xcom_input_queue::future_reply future =
      xcom_input_try_push_and_get_reply(data);
  std::unique_ptr<Gcs_xcom_input_queue::Reply> reply = future.get();
  bool const processable_reply =
      (reply.get() != nullptr && reply->get_payload() != nullptr);

  bool successful = false;
  if (processable_reply) {
    successful = (reply->get_payload()->cli_err == REQUEST_OK);
  }

  if (!successful) {
    MYSQL_GCS_LOG_DEBUG("%s: Failed to push into XCom.", __func__);
  }

  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_get_leaders(uint32_t gid,
                                                  leader_info_data &leaders) {
  bool successful = false;
  app_data_ptr data = new_app_data();
  data = init_get_leaders_msg(data, gid);
  /* Takes ownership of data. */
  Gcs_xcom_input_queue::future_reply future =
      xcom_input_try_push_and_get_reply(data);
  std::unique_ptr<Gcs_xcom_input_queue::Reply> reply = future.get();
  bool const processable_reply =
      (reply.get() != nullptr && reply->get_payload() != nullptr);
  if (processable_reply) {
    bool const reply_ok = (reply->get_payload()->cli_err == REQUEST_OK);
    if (reply_ok) {
      leaders = steal_leader_info_data(
          reply->get_payload()->rd->reply_data_u.leaders);
      successful = true;
    } else {
      MYSQL_GCS_LOG_DEBUG(
          "xcom_client_get_leaders: Couldn't fetch the leader info. "
          "(cli_err=%d)",
          reply->get_payload()->cli_err);
    }
  } else {
    MYSQL_GCS_LOG_DEBUG("xcom_client_get_leaders: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_get_synode_app_data(
    connection_descriptor *fd, uint32_t gid, synode_no_array &synodes,
    synode_app_data_array &reply) {
  bool successful = false;

  successful =
      (::xcom_client_get_synode_app_data(fd, gid, &synodes, &reply) == 1);
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_set_cache_size(uint64_t size) {
  app_data_ptr data = new_app_data();
  data = init_set_cache_size_msg(data, size);
  /* Takes ownership of data. */
  bool const successful = xcom_input_try_push(data);
  if (!successful) {
    MYSQL_GCS_LOG_DEBUG(
        "xcom_client_set_cache_size: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_boot(node_list *nl, uint32_t gid) {
  app_data_ptr data = new_app_data();
  data = init_config_with_group(data, nl, unified_boot_type, gid);
  /* Takes ownership of data. */
  bool const successful = xcom_input_try_push(data);
  if (!successful) {
    MYSQL_GCS_LOG_DEBUG("xcom_client_boot: Failed to push into XCom.");
  }
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_send_data(unsigned long long len,
                                                char *data) {
  /* We own data. */
  bool successful = false;

  if (len <= std::numeric_limits<unsigned int>::max()) {
    assert(len > 0);
    app_data_ptr msg = new_app_data();
    /* Takes ownership of data. */
    msg = init_app_msg(msg, data, static_cast<uint32_t>(len));
    successful = xcom_input_try_push(msg);  // Takes ownership of msg.
    if (!successful) {
      MYSQL_GCS_LOG_DEBUG("xcom_client_send_data: Failed to push into XCom.");
    }
  } else {
    /*
      GCS's message length is defined as unsigned long long type, but
      XCOM can only accept packets length of which are in unsigned int range.
      So it throws an error when gcs message is too big.
    */
    MYSQL_GCS_LOG_ERROR(
        "The data is too big. Data length should not"
        << " exceed " << std::numeric_limits<unsigned int>::max() << " bytes.");
    free(data);  // We own it, so we free it.
  }
  return successful;
}

void Gcs_xcom_proxy_impl::xcom_init(xcom_port xcom_listen_port) {
  /* Init XCom */
  ::xcom_fsm(x_fsm_init, int_arg(0)); /* Basic xcom init */

  ::xcom_taskmain2(xcom_listen_port);
}

void Gcs_xcom_proxy_impl::xcom_exit() { this->set_should_exit(true); }

void Gcs_xcom_proxy_impl::xcom_set_cleanup() {
  xcom_set_ready(false);
  xcom_set_exit(false);
  xcom_set_comms_status(XCOM_COMM_STATUS_UNDEFINED);
}

int Gcs_xcom_proxy_impl::xcom_get_ssl_mode(const char *mode) {
  return ::get_network_management_interface()->xcom_get_ssl_mode(mode);
}

int Gcs_xcom_proxy_impl::xcom_get_ssl_fips_mode(const char *ssl_fips_mode) {
  return ::get_network_management_interface()->xcom_get_ssl_fips_mode(
      ssl_fips_mode);
}

int Gcs_xcom_proxy_impl::xcom_set_ssl_mode(int mode) {
  return ::get_network_management_interface()->xcom_set_ssl_mode(mode);
}

int Gcs_xcom_proxy_impl::xcom_set_ssl_fips_mode(int mode) {
  return ::get_network_management_interface()->xcom_set_ssl_fips_mode(mode);
}

bool Gcs_xcom_proxy_impl::xcom_init_ssl() {
  Network_configuration_parameters security_params;

  security_params.ssl_params.ssl_mode = m_ssl_mode;
  security_params.ssl_params.server_key_file = m_server_key_file;
  security_params.ssl_params.server_cert_file = m_server_cert_file;
  security_params.ssl_params.client_key_file = m_client_key_file;
  security_params.ssl_params.client_cert_file = m_client_cert_file;
  security_params.ssl_params.ca_file = m_ca_file;
  security_params.ssl_params.ca_path = m_ca_path;
  security_params.ssl_params.crl_file = m_crl_file;
  security_params.ssl_params.crl_path = m_crl_path;
  security_params.ssl_params.cipher = m_cipher;
  security_params.tls_params.tls_version = m_tls_version;
  security_params.tls_params.tls_ciphersuites = m_tls_ciphersuites;

  bool const successful =
      get_network_operations_interface()
          ->configure_active_provider_secure_connections(security_params);

  return successful;
}

void Gcs_xcom_proxy_impl::xcom_destroy_ssl() {
  ::get_network_management_interface()->finalize_secure_connections_context();
}

bool Gcs_xcom_proxy_impl::xcom_use_ssl() {
  bool const will_use =
      (get_network_management_interface()->is_xcom_using_ssl() == 1);
  return will_use;
}

void Gcs_xcom_proxy_impl::xcom_set_ssl_parameters(ssl_parameters ssl,
                                                  tls_parameters tls) {
  m_ssl_mode = ssl.ssl_mode;
  m_server_key_file = ssl.server_key_file;
  m_server_cert_file = ssl.server_cert_file;
  m_client_key_file = ssl.client_key_file;
  m_client_cert_file = ssl.client_cert_file;
  m_ca_file = ssl.ca_file;
  m_ca_path = ssl.ca_path;
  m_crl_file = ssl.crl_file;
  m_crl_path = ssl.crl_path;
  m_cipher = ssl.cipher;
  m_tls_version = tls.tls_version;
  m_tls_ciphersuites = tls.tls_ciphersuites;
}

/* purecov: begin deadcode */
Gcs_xcom_proxy_impl::Gcs_xcom_proxy_impl()
    : m_wait_time(WAITING_TIME),
      m_lock_xcom_ready(),
      m_cond_xcom_ready(),
      m_is_xcom_ready(false),
      m_lock_xcom_comms_status(),
      m_cond_xcom_comms_status(),
      m_xcom_comms_status(XCOM_COMM_STATUS_UNDEFINED),
      m_lock_xcom_exit(),
      m_cond_xcom_exit(),
      m_is_xcom_exit(false),
      m_socket_util(nullptr),
      m_ssl_mode(SSL_DISABLED),
      m_server_key_file(nullptr),
      m_server_cert_file(nullptr),
      m_client_key_file(nullptr),
      m_client_cert_file(nullptr),
      m_ca_file(nullptr),
      m_ca_path(nullptr),
      m_crl_file(nullptr),
      m_crl_path(nullptr),
      m_cipher(nullptr),
      m_tls_version(nullptr),
      m_tls_ciphersuites(nullptr),
      m_should_exit(false) {
  m_lock_xcom_ready.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
                         nullptr);
  m_cond_xcom_ready.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready);
  m_lock_xcom_comms_status.init(
      key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status, nullptr);
  m_cond_xcom_comms_status.init(
      key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status);
  m_lock_xcom_exit.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
                        nullptr);
  m_cond_xcom_exit.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_exit);

  m_socket_util = new My_xp_socket_util_impl();
}
/* purecov: begin end */

Gcs_xcom_proxy_impl::Gcs_xcom_proxy_impl(unsigned int wt)
    : m_wait_time(wt),
      m_lock_xcom_ready(),
      m_cond_xcom_ready(),
      m_is_xcom_ready(false),
      m_lock_xcom_comms_status(),
      m_cond_xcom_comms_status(),
      m_xcom_comms_status(XCOM_COMM_STATUS_UNDEFINED),
      m_lock_xcom_exit(),
      m_cond_xcom_exit(),
      m_is_xcom_exit(false),
      m_socket_util(nullptr),
      m_server_key_file(nullptr),
      m_server_cert_file(nullptr),
      m_client_key_file(nullptr),
      m_client_cert_file(nullptr),
      m_ca_file(nullptr),
      m_ca_path(nullptr),
      m_crl_file(nullptr),
      m_crl_path(nullptr),
      m_cipher(nullptr),
      m_tls_version(nullptr),
      m_tls_ciphersuites(nullptr),
      m_should_exit(false) {
  m_lock_xcom_ready.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
                         nullptr);
  m_cond_xcom_ready.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready);
  m_lock_xcom_comms_status.init(
      key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status, nullptr);
  m_cond_xcom_comms_status.init(
      key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status);
  m_lock_xcom_exit.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
                        nullptr);
  m_cond_xcom_exit.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_exit);

  m_socket_util = new My_xp_socket_util_impl();
}

Gcs_xcom_proxy_impl::~Gcs_xcom_proxy_impl() {
  m_lock_xcom_ready.destroy();
  m_cond_xcom_ready.destroy();
  m_lock_xcom_comms_status.destroy();
  m_cond_xcom_comms_status.destroy();
  m_lock_xcom_exit.destroy();
  m_cond_xcom_exit.destroy();

  delete m_socket_util;

  xcom_input_disconnect();
}

site_def const *Gcs_xcom_proxy_impl::find_site_def(synode_no synode) {
  return ::find_site_def(synode);
}

node_address *Gcs_xcom_proxy_impl::new_node_address_uuid(unsigned int n,
                                                         char const *names[],
                                                         blob uuids[]) {
  return ::new_node_address_uuid(n, names, uuids);
}

enum_gcs_error Gcs_xcom_proxy_impl::xcom_wait_for_condition(
    My_xp_cond_impl &condition, My_xp_mutex_impl &condition_lock,
    std::function<bool(void)> need_to_wait,
    std::function<const std::string(int res)> condition_event) {
  enum_gcs_error ret = GCS_OK;
  struct timespec ts;
  int res = 0;

  condition_lock.lock();

  if (need_to_wait()) {
    My_xp_util::set_timespec(&ts, m_wait_time);
    res = condition.timed_wait(condition_lock.get_native_mutex(), &ts);
  }

  condition_lock.unlock();

  if (res != 0) {
    // There was an error
    ret = GCS_NOK;
    std::string error_string = condition_event(res);
    if (res == ETIMEDOUT) {
      MYSQL_GCS_LOG_ERROR("Timeout while waiting for " << error_string << "!")
    } else if (res == EINVAL) {
      // invalid abstime or cond or mutex
      MYSQL_GCS_LOG_ERROR("Invalid parameter received by the timed wait for "
                          << error_string << "!")
    } else if (res == EPERM) {
      MYSQL_GCS_LOG_ERROR("Thread waiting for "
                          << error_string
                          << " does not own the mutex at the time of the call!")
    } else
      MYSQL_GCS_LOG_ERROR("Error while waiting for " << error_string << "!")
  }

  return ret;
}

enum_gcs_error Gcs_xcom_proxy_impl::xcom_wait_ready() {
  auto event_string = []([[maybe_unused]] int res) {
    return "the group communication engine to be ready";
  };
  return xcom_wait_for_condition(
      m_cond_xcom_ready, m_lock_xcom_ready,
      [this]() { return !m_is_xcom_ready; }, event_string);
}

bool Gcs_xcom_proxy_impl::xcom_is_ready() {
  bool retval;

  m_lock_xcom_ready.lock();
  retval = m_is_xcom_ready;
  m_lock_xcom_ready.unlock();

  return retval;
}

void Gcs_xcom_proxy_impl::xcom_set_ready(bool value) {
  m_lock_xcom_ready.lock();
  m_is_xcom_ready = value;
  m_lock_xcom_ready.unlock();
}

void Gcs_xcom_proxy_impl::xcom_signal_ready() {
  m_lock_xcom_ready.lock();
  m_is_xcom_ready = true;
  m_cond_xcom_ready.broadcast();
  m_lock_xcom_ready.unlock();
}

enum_gcs_error Gcs_xcom_proxy_impl::xcom_wait_exit() {
  auto event_string = [](int res) {
    if (res == ETIMEDOUT) {
      return "the group communication engine to exit";
    } else {
      return "group communication engine to exit";
    }
  };
  return xcom_wait_for_condition(
      m_cond_xcom_exit, m_lock_xcom_exit, [this]() { return !m_is_xcom_exit; },
      event_string);
}

bool Gcs_xcom_proxy_impl::xcom_is_exit() {
  bool retval;

  m_lock_xcom_exit.lock();
  retval = m_is_xcom_exit;
  m_lock_xcom_exit.unlock();

  return retval;
}

void Gcs_xcom_proxy_impl::xcom_set_exit(bool value) {
  m_lock_xcom_exit.lock();
  m_is_xcom_exit = value;
  m_lock_xcom_exit.unlock();
}

void Gcs_xcom_proxy_impl::xcom_signal_exit() {
  m_lock_xcom_exit.lock();
  m_is_xcom_exit = true;
  m_cond_xcom_exit.broadcast();
  m_lock_xcom_exit.unlock();
}

void Gcs_xcom_proxy_impl::xcom_wait_for_xcom_comms_status_change(int &status) {
  auto wait_cond = [this]() {
    return m_xcom_comms_status == XCOM_COMM_STATUS_UNDEFINED;
  };
  auto event_string = []([[maybe_unused]] int res) {
    return "the group communication engine's communications status to change";
  };

  enum_gcs_error res = xcom_wait_for_condition(m_cond_xcom_comms_status,
                                               m_lock_xcom_comms_status,
                                               wait_cond, event_string);

  m_lock_xcom_comms_status.lock();
  if (res != GCS_OK) {
    status = XCOM_COMMS_OTHER;
  } else {
    status = m_xcom_comms_status;
  }
  m_lock_xcom_comms_status.unlock();
}

bool Gcs_xcom_proxy_impl::xcom_has_comms_status_changed() {
  bool retval;

  m_lock_xcom_comms_status.lock();
  retval = (m_xcom_comms_status != XCOM_COMM_STATUS_UNDEFINED);
  m_lock_xcom_comms_status.unlock();

  return retval;
}

void Gcs_xcom_proxy_impl::xcom_set_comms_status(int value) {
  m_lock_xcom_comms_status.lock();
  m_xcom_comms_status = value;
  m_lock_xcom_comms_status.unlock();
}

void Gcs_xcom_proxy_impl::xcom_signal_comms_status_changed(int status) {
  m_lock_xcom_comms_status.lock();
  m_xcom_comms_status = status;
  m_cond_xcom_comms_status.broadcast();
  m_lock_xcom_comms_status.unlock();
}

bool Gcs_xcom_proxy_impl::get_should_exit() {
  return m_should_exit.load(std::memory_order_relaxed);
}

void Gcs_xcom_proxy_impl::set_should_exit(bool should_exit) {
  m_should_exit.store(should_exit, std::memory_order_relaxed);
}

bool Gcs_xcom_proxy_impl::xcom_input_connect(std::string const &address,
                                             xcom_port port) {
  m_xcom_input_queue.reset();
  xcom_input_disconnect();
  bool const successful =
      ::xcom_input_new_signal_connection(address.c_str(), port);

  return successful;
}

void Gcs_xcom_proxy_impl::xcom_input_disconnect() {
  ::xcom_input_free_signal_connection();
}

bool Gcs_xcom_proxy_impl::xcom_input_try_push(app_data_ptr data) {
  assert(data != nullptr);
  bool successful = false;
  bool const pushed =
      m_xcom_input_queue.push(data);  // Takes ownership of data.
  if (pushed) successful = ::xcom_input_signal();
  return successful;
}

Gcs_xcom_input_queue::future_reply
Gcs_xcom_proxy_impl::xcom_input_try_push_and_get_reply(app_data_ptr data) {
  assert(data != nullptr);
  Gcs_xcom_input_queue::future_reply future =
      m_xcom_input_queue.push_and_get_reply(data);  // Takes ownership of data.
  bool const pushed = future.valid();
  if (pushed) ::xcom_input_signal();
  return future;
}

xcom_input_request_ptr Gcs_xcom_proxy_impl::xcom_input_try_pop() {
  return m_xcom_input_queue.pop();
}

void Gcs_xcom_app_cfg::init() { ::init_cfg_app_xcom(); }

void Gcs_xcom_app_cfg::deinit() { ::deinit_cfg_app_xcom(); }

void Gcs_xcom_app_cfg::set_poll_spin_loops(unsigned int loops) {
  if (the_app_xcom_cfg) the_app_xcom_cfg->m_poll_spin_loops = loops;
}

void Gcs_xcom_app_cfg::set_xcom_cache_size(uint64_t size) {
  if (the_app_xcom_cfg) the_app_xcom_cfg->m_cache_limit = size;
}

void Gcs_xcom_app_cfg::set_network_namespace_manager(
    Network_namespace_manager *ns_mgr) {
  if (the_app_xcom_cfg) the_app_xcom_cfg->network_ns_manager = ns_mgr;
}

bool Gcs_xcom_app_cfg::set_identity(node_address *identity) {
  bool constexpr kError = true;
  bool constexpr kSuccess = false;

  if (identity == nullptr) return kError;

  ::cfg_app_xcom_set_identity(identity);
  return kSuccess;
}

bool Gcs_xcom_proxy_impl::xcom_client_force_config(node_list *nl,
                                                   uint32_t group_id) {
  app_data_ptr data = new_app_data();
  data = init_config_with_group(data, nl, force_config_type, group_id);

  /* Takes ownership of data. */
  Gcs_xcom_input_queue::future_reply future =
      xcom_input_try_push_and_get_reply(data);
  std::unique_ptr<Gcs_xcom_input_queue::Reply> reply = future.get();
  bool const processable_reply =
      (reply.get() != nullptr && reply->get_payload() != nullptr);

  bool successful = false;
  if (processable_reply) {
    successful = (reply->get_payload()->cli_err == REQUEST_OK);
  }

  if (!successful) {
    MYSQL_GCS_LOG_DEBUG("xcom_client_force_config: Failed to push into XCom.");
  }

  return successful;
}

bool Gcs_xcom_proxy_base::xcom_remove_nodes(Gcs_xcom_nodes &nodes,
                                            uint32_t group_id_hash) {
  node_list nl{0, nullptr};
  bool successful = false;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Removing %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    successful = xcom_client_remove_node(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return successful;
}

bool Gcs_xcom_proxy_base::xcom_remove_nodes(connection_descriptor &con,
                                            Gcs_xcom_nodes &nodes,
                                            uint32_t group_id_hash) {
  node_list nl{0, nullptr};
  bool successful = false;

  if (serialize_nodes_information(nodes, nl)) {
    successful = xcom_client_remove_node(&con, &nl, group_id_hash);
  }
  free_nodes_information(nl);

  return successful;
}

bool Gcs_xcom_proxy_base::xcom_remove_node(
    const Gcs_xcom_node_information &node, uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_remove;
  nodes_to_remove.add_node(node);

  return xcom_remove_nodes(nodes_to_remove, group_id_hash);
}

xcom_event_horizon Gcs_xcom_proxy_base::xcom_get_minimum_event_horizon() {
  return ::xcom_get_minimum_event_horizon();
}

xcom_event_horizon Gcs_xcom_proxy_base::xcom_get_maximum_event_horizon() {
  return ::xcom_get_maximum_event_horizon();
}

bool Gcs_xcom_proxy_base::xcom_get_event_horizon(
    uint32_t group_id_hash, xcom_event_horizon &event_horizon) {
  MYSQL_GCS_LOG_DEBUG("Retrieveing event horizon");
  return xcom_client_get_event_horizon(group_id_hash, event_horizon);
}

bool Gcs_xcom_proxy_base::xcom_set_event_horizon(
    uint32_t group_id_hash, xcom_event_horizon event_horizon) {
  MYSQL_GCS_LOG_DEBUG("Reconfiguring event horizon to %" PRIu32, event_horizon);
  return xcom_client_set_event_horizon(group_id_hash, event_horizon);
}

bool Gcs_xcom_proxy_base::xcom_set_leaders(uint32_t group_id_hash,
                                           u_int nr_preferred_leaders,
                                           char const *preferred_leaders[],
                                           node_no max_nr_leaders) {
  MYSQL_GCS_LOG_DEBUG(
      "Reconfiguring XCom's preferred leaders to nr_preferred_leaders=%" PRIu32
      " preferred_leaders[0]=%s max_nr_leaders=%" PRIu32,
      nr_preferred_leaders,
      nr_preferred_leaders > 0 ? preferred_leaders[0] : "n/a", max_nr_leaders);
  return xcom_client_set_leaders(group_id_hash, nr_preferred_leaders,
                                 preferred_leaders, max_nr_leaders);
}

bool Gcs_xcom_proxy_base::xcom_get_leaders(uint32_t group_id_hash,
                                           leader_info_data &leaders) {
  MYSQL_GCS_LOG_DEBUG("Retrieving leader information");
  return xcom_client_get_leaders(group_id_hash, leaders);
}

static bool convert_synode_set_to_synode_array(
    synode_no_array &to,
    std::unordered_set<Gcs_xcom_synode> const &synode_set) {
  bool constexpr SUCCESS = true;
  bool constexpr FAILURE = false;
  bool result = FAILURE;
  u_int const nr_synodes = synode_set.size();
  std::size_t index = 0;

  to.synode_no_array_len = 0;
  to.synode_no_array_val =
      static_cast<synode_no *>(malloc(nr_synodes * sizeof(synode_no)));
  if (to.synode_no_array_val == nullptr) goto end;
  to.synode_no_array_len = nr_synodes;

  for (auto &gcs_synod : synode_set) {
    to.synode_no_array_val[index] = gcs_synod.get_synod();
    index++;
  }

  result = SUCCESS;

end:
  return result;
}

bool Gcs_xcom_proxy_base::xcom_get_synode_app_data(
    Gcs_xcom_node_information const &xcom_instance, uint32_t group_id_hash,
    std::unordered_set<Gcs_xcom_synode> const &synode_set,
    synode_app_data_array &reply) {
  assert(!synode_set.empty());
  bool successful = false;
  synode_no_array synodes;

  /* Connect to XCom. */
  Gcs_xcom_node_address xcom_address(
      xcom_instance.get_member_id().get_member_id());
  connection_descriptor *con = xcom_client_open_connection(
      xcom_address.get_member_ip(), xcom_address.get_member_port());
  bool const connected_to_xcom = (con != nullptr);
  if (!connected_to_xcom) goto end;

  successful = convert_synode_set_to_synode_array(synodes, synode_set);
  if (!successful) goto end;

  /* Request the data decided at synodes.
   * synodes is passed with moved semantics, so no need to free afterwards. */
  successful =
      xcom_client_get_synode_app_data(con, group_id_hash, synodes, reply);

  /* Close the connection to XCom. */
  xcom_client_close_connection(con);

end:
  return successful;
}

bool Gcs_xcom_proxy_base::xcom_set_cache_size(uint64_t size) {
  MYSQL_GCS_LOG_DEBUG("Reconfiguring cache size limit to %luu", size);
  return xcom_client_set_cache_size(size);
}

bool Gcs_xcom_proxy_base::xcom_force_nodes(Gcs_xcom_nodes &nodes,
                                           uint32_t group_id_hash) {
  node_list nl{0, nullptr};
  bool successful = false;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Forcing %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    successful = xcom_client_force_config(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return successful;
}

bool Gcs_xcom_proxy_base::serialize_nodes_information(Gcs_xcom_nodes &nodes,
                                                      node_list &nl) {
  unsigned int len = 0;
  char const **addrs = nullptr;
  blob *uuids = nullptr;
  nl = {0, nullptr};

  if (nodes.get_size() == 0) {
    MYSQL_GCS_LOG_DEBUG("There aren't nodes to be reported.");
    return false;
  }

  if (!nodes.encode(&len, &addrs, &uuids)) {
    MYSQL_GCS_LOG_DEBUG("Could not encode %llu nodes.",
                        static_cast<long long unsigned>(nodes.get_size()));
    return false;
  }

  nl.node_list_len = len;
  nl.node_list_val = new_node_address_uuid(len, addrs, uuids);

  MYSQL_GCS_LOG_DEBUG("Prepared %u nodes at %p", nl.node_list_len,
                      nl.node_list_val);
  return true;
}

void Gcs_xcom_proxy_base::free_nodes_information(node_list &nl) {
  MYSQL_GCS_LOG_DEBUG("Unprepared %u nodes at %p", nl.node_list_len,
                      nl.node_list_val);
  delete_node_address(nl.node_list_len, nl.node_list_val);
}

bool Gcs_xcom_proxy_base::xcom_boot_node(Gcs_xcom_node_information &node,
                                         uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_boot;
  nodes_to_boot.add_node(node);
  node_list nl;
  bool successful = false;

  if (serialize_nodes_information(nodes_to_boot, nl)) {
    MYSQL_GCS_LOG_DEBUG("Booting up %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    successful = xcom_client_boot(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return successful;
}

bool Gcs_xcom_proxy_base::xcom_add_nodes(connection_descriptor &con,
                                         Gcs_xcom_nodes &nodes,
                                         uint32_t group_id_hash) {
  node_list nl;
  bool successful = false;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Adding up %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    successful = xcom_client_add_node(&con, &nl, group_id_hash);
  }
  free_nodes_information(nl);

  return successful;
}

bool Gcs_xcom_proxy_base::xcom_add_node(connection_descriptor &con,
                                        const Gcs_xcom_node_information &node,
                                        uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_add;
  nodes_to_add.add_node(node);

  return xcom_add_nodes(con, nodes_to_add, group_id_hash);
}

bool Gcs_xcom_proxy_base::test_xcom_tcp_connection(std::string &host,
                                                   xcom_port port) {
  connection_descriptor *con = xcom_client_open_connection(host, port);

  bool const could_connect_to_local_xcom = (con->fd != -1);
  bool could_disconnect_from_local_xcom = false;
  if (could_connect_to_local_xcom) {
    could_disconnect_from_local_xcom = xcom_client_close_connection(con);
  }

  free(con);

  return could_connect_to_local_xcom && could_disconnect_from_local_xcom;
}

bool Gcs_xcom_proxy_base::initialize_network_manager() {
  return get_network_management_interface()->initialize();
}

bool Gcs_xcom_proxy_base::finalize_network_manager() {
  return get_network_management_interface()->finalize();
}

bool Gcs_xcom_proxy_base::set_network_manager_active_provider(
    enum_transport_protocol new_value) {
  get_network_management_interface()->set_running_protocol(new_value);

  return false;
}
