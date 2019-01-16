/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.h"

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
  bool const successful = (::xcom_close_client_connection(fd) == 0);
  return successful;
}

connection_descriptor *Gcs_xcom_proxy_impl::xcom_client_open_connection(
    std::string saddr, xcom_port port) {
  const char *addr = saddr.c_str();
  return ::xcom_open_client_connection(addr, port);
}

bool Gcs_xcom_proxy_impl::xcom_client_add_node(connection_descriptor *fd,
                                               node_list *nl, uint32_t gid) {
  bool const successful = (::xcom_client_add_node(fd, nl, gid) == 1);
  return successful;
}

bool Gcs_xcom_proxy_impl::xcom_client_remove_node(node_list *nl, uint32_t gid) {
  app_data_ptr data = new_app_data();
  data = init_config_with_group(data, nl, remove_node_type, gid);
  /* Takes ownership of data. */
  bool const successful = xcom_input_try_push(data);
  if (!successful) {
    MYSQL_GCS_LOG_DEBUG("xcom_client_remove_node: Failed to push into XCom.");
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
  ::xcom_fsm(xa_init, int_arg(0)); /* Basic xcom init */

  ::xcom_taskmain2(xcom_listen_port);
}

bool Gcs_xcom_proxy_impl::xcom_exit(bool xcom_input_open) {
  bool successful = false;
  if (xcom_input_open) {
    /* Stop XCom */
    app_data_ptr data = new_app_data();
    data = init_terminate_command(data);
    /* Takes ownership of data. */
    successful = xcom_input_try_push(data);
    if (!successful) {
      MYSQL_GCS_LOG_DEBUG("xcom_exit: Failed to push into XCom.");
    }
  }
  if (!xcom_input_open || !successful) {
    /* The input channel was not yet open, or we failed to push, so use basic
       XCom stop. */
    this->set_should_exit(1);
    successful = true;
  }
  return successful;
}

void Gcs_xcom_proxy_impl::xcom_set_cleanup() {
  xcom_set_ready(false);
  xcom_set_exit(false);
  xcom_set_comms_status(XCOM_COMM_STATUS_UNDEFINED);
}

int Gcs_xcom_proxy_impl::xcom_get_ssl_mode(const char *mode) {
  return ::xcom_get_ssl_mode(mode);
}

int Gcs_xcom_proxy_impl::xcom_get_ssl_fips_mode(const char *ssl_fips_mode) {
  return ::xcom_get_ssl_fips_mode(ssl_fips_mode);
}

int Gcs_xcom_proxy_impl::xcom_set_ssl_mode(int mode) {
  return ::xcom_set_ssl_mode(mode);
}

int Gcs_xcom_proxy_impl::xcom_set_ssl_fips_mode(int mode) {
  return ::xcom_set_ssl_fips_mode(mode);
}

bool Gcs_xcom_proxy_impl::xcom_init_ssl() {
  bool const successful =
      (::xcom_init_ssl(m_server_key_file, m_server_cert_file, m_client_key_file,
                       m_client_cert_file, m_ca_file, m_ca_path, m_crl_file,
                       m_crl_path, m_cipher, m_tls_version) == 1);
  return successful;
}

void Gcs_xcom_proxy_impl::xcom_destroy_ssl() { ::xcom_destroy_ssl(); }

bool Gcs_xcom_proxy_impl::xcom_use_ssl() {
  bool const will_use = (::xcom_use_ssl() == 1);
  return will_use;
}

void Gcs_xcom_proxy_impl::xcom_set_ssl_parameters(
    const char *server_key_file, const char *server_cert_file,
    const char *client_key_file, const char *client_cert_file,
    const char *ca_file, const char *ca_path, const char *crl_file,
    const char *crl_path, const char *cipher, const char *tls_version) {
  m_server_key_file = server_key_file;
  m_server_cert_file = server_cert_file;
  m_client_key_file = client_key_file;
  m_client_cert_file = client_cert_file;
  m_ca_file = ca_file;
  m_ca_path = ca_path;
  m_crl_file = crl_file;
  m_crl_path = crl_path;
  m_cipher = cipher;
  m_tls_version = tls_version;
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
      m_socket_util(NULL),
      m_server_key_file(),
      m_server_cert_file(),
      m_client_key_file(),
      m_client_cert_file(),
      m_ca_file(),
      m_ca_path(),
      m_crl_file(),
      m_crl_path(),
      m_cipher(),
      m_tls_version(),
      m_should_exit(false) {
  m_lock_xcom_ready.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
                         NULL);
  m_cond_xcom_ready.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready);
  m_lock_xcom_comms_status.init(
      key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status, NULL);
  m_cond_xcom_comms_status.init(
      key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status);
  m_lock_xcom_exit.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
                        NULL);
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
      m_socket_util(NULL),
      m_server_key_file(),
      m_server_cert_file(),
      m_client_key_file(),
      m_client_cert_file(),
      m_ca_file(),
      m_ca_path(),
      m_crl_file(),
      m_crl_path(),
      m_cipher(),
      m_tls_version(),
      m_should_exit(false) {
  m_lock_xcom_ready.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_ready,
                         NULL);
  m_cond_xcom_ready.init(key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_ready);
  m_lock_xcom_comms_status.init(
      key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_comms_status, NULL);
  m_cond_xcom_comms_status.init(
      key_GCS_COND_Gcs_xcom_proxy_impl_m_cond_xcom_comms_status);
  m_lock_xcom_exit.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_exit,
                        NULL);
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

  ::xcom_input_free_signal_connection();
}

site_def const *Gcs_xcom_proxy_impl::find_site_def(synode_no synode) {
  return ::find_site_def(synode);
}

node_address *Gcs_xcom_proxy_impl::new_node_address_uuid(unsigned int n,
                                                         char *names[],
                                                         blob uuids[]) {
  return ::new_node_address_uuid(n, names, uuids);
}

enum_gcs_error Gcs_xcom_proxy_impl::xcom_wait_ready() {
  enum_gcs_error ret = GCS_OK;
  struct timespec ts;
  int res = 0;

  m_lock_xcom_ready.lock();

  if (!m_is_xcom_ready) {
    My_xp_util::set_timespec(&ts, m_wait_time);
    res =
        m_cond_xcom_ready.timed_wait(m_lock_xcom_ready.get_native_mutex(), &ts);
  }

  if (res != 0) {
    ret = GCS_NOK;
    // There was an error
    if (res == ETIMEDOUT) {
      // timeout
      MYSQL_GCS_LOG_ERROR("Timeout while waiting for the group"
                          << " communication engine to be ready!");
    } else if (res == EINVAL) {
      // invalid abstime or cond or mutex
      MYSQL_GCS_LOG_ERROR("Invalid parameter received by the timed wait for"
                          << " the group communication engine to be ready.");
    } else if (res == EPERM) {
      // mutex isn't owned by the current thread at the time of the call
      MYSQL_GCS_LOG_ERROR("Thread waiting for the group communication"
                          << " engine to be ready does not own the mutex at the"
                          << " time of the call!");
    } else
      MYSQL_GCS_LOG_ERROR("Error while waiting for the group"
                          << "communication engine to be ready!");
  }

  m_lock_xcom_ready.unlock();

  return ret;
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
  enum_gcs_error ret = GCS_OK;
  struct timespec ts;
  int res = 0;

  m_lock_xcom_exit.lock();

  if (!m_is_xcom_exit) {
    My_xp_util::set_timespec(&ts, m_wait_time);
    res = m_cond_xcom_exit.timed_wait(m_lock_xcom_exit.get_native_mutex(), &ts);
  }

  if (res != 0) {
    ret = GCS_NOK;
    // There was an error
    if (res == ETIMEDOUT) {
      // timeout
      MYSQL_GCS_LOG_ERROR(
          "Timeout while waiting for the group communication engine to exit!")
    } else if (res == EINVAL) {
      // invalid abstime or cond or mutex
      MYSQL_GCS_LOG_ERROR(
          "Timed wait for group communication engine to exit received an "
          "invalid parameter!")
    } else if (res == EPERM) {
      // mutex isn't owned by the current thread at the time of the call
      MYSQL_GCS_LOG_ERROR(
          "Timed wait for group communication engine to exit using mutex that "
          "isn't owned by the current thread at the time of the call!")
    } else {
      MYSQL_GCS_LOG_ERROR(
          "Error while waiting for group communication to exit!")
    }
  }

  m_lock_xcom_exit.unlock();

  return ret;
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
  struct timespec ts;
  int res = 0;

  m_lock_xcom_comms_status.lock();

  if (m_xcom_comms_status == XCOM_COMM_STATUS_UNDEFINED) {
    My_xp_util::set_timespec(&ts, m_wait_time);
    res = m_cond_xcom_comms_status.timed_wait(
        m_lock_xcom_comms_status.get_native_mutex(), &ts);
  }

  if (res != 0) {
    // There was an error while retrieving the latest status change.
    status = XCOM_COMMS_OTHER;

    if (res == ETIMEDOUT) {
      // timeout
      MYSQL_GCS_LOG_ERROR("Timeout while waiting for the group communication"
                          << " engine's communications status to change!");
    } else if (res == EINVAL) {
      // invalid abstime or cond or mutex
      MYSQL_GCS_LOG_ERROR("Invalid parameter received by the timed wait for"
                          << " the group communication engine's communications"
                          << " status to change.");
    } else if (res == EPERM) {
      // mutex isn't owned by the current thread at the time of the call
      MYSQL_GCS_LOG_ERROR("Thread waiting for the group communication"
                          << " engine's communications status to change does"
                          << " not own the mutex at the time of the call!");
    } else
      MYSQL_GCS_LOG_ERROR("Error while waiting for the group communication"
                          << " engine's communications status to change!");
  } else
    status = m_xcom_comms_status;

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

bool Gcs_xcom_proxy_impl::xcom_input_connect() {
  m_xcom_input_queue.reset();
  ::xcom_input_free_signal_connection();
  bool const successful = ::xcom_input_new_signal_connection();
  return successful;
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
  char **addrs = NULL;
  blob *uuids = NULL;
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

  bool const could_connect_to_local_xcom = (con != nullptr);
  bool could_disconnect_from_local_xcom = false;
  if (could_connect_to_local_xcom) {
    could_disconnect_from_local_xcom = xcom_client_close_connection(con);
  }

  return could_connect_to_local_xcom && could_disconnect_from_local_xcom;
}
