/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"

#include <assert.h>
#include <errno.h>
#include <time.h>
#ifndef _WIN32
#include <netdb.h>
#endif
#include <algorithm>
#include <climits>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_net.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_os.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.h"

/**
  6 is the recommended value. Too large numbers
  here hinder testing and do not bring too much
  performance improvement as confirmed by our
  benchmarks.
 */
static const int XCOM_MAX_HANDLERS = 6;

/*
  Time is defined in seconds.
*/
static const unsigned int WAITING_TIME = 30;

/*
  Number of attempts to join a group.
*/
static const unsigned int JOIN_ATTEMPTS = 0;

/*
  Sleep time between attempts defined in seconds.
*/
static const uint64_t JOIN_SLEEP_TIME = 5;

Gcs_xcom_utils::~Gcs_xcom_utils() {}

u_long Gcs_xcom_utils::build_xcom_group_id(Gcs_group_identifier &group_id) {
  std::string group_id_str = group_id.get_group_id();
  return mhash((unsigned char *)group_id_str.c_str(), group_id_str.size());
}

void Gcs_xcom_utils::process_peer_nodes(
    const std::string *peer_nodes, std::vector<std::string> &processed_peers) {
  std::string peer_init(peer_nodes->c_str());
  std::string delimiter = ",";

  // Clear all whitespace in the string
  peer_init.erase(std::remove(peer_init.begin(), peer_init.end(), ' '),
                  peer_init.end());

  // Skip delimiter at beginning.
  std::string::size_type lastPos = peer_init.find_first_not_of(delimiter, 0);

  // Find first "non-delimiter".
  std::string::size_type pos = peer_init.find_first_of(delimiter, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    std::string peer(peer_init.substr(lastPos, pos - lastPos));
    processed_peers.push_back(peer);

    // Skip delimiter
    lastPos = peer_init.find_first_not_of(delimiter, pos);

    // Find next "non-delimiter"
    pos = peer_init.find_first_of(delimiter, lastPos);
  }
}

void Gcs_xcom_utils::validate_peer_nodes(
    std::vector<std::string> &peers, std::vector<std::string> &invalid_peers) {
  std::vector<std::string>::iterator it;
  for (it = peers.begin(); it != peers.end();) {
    std::string server_and_port = *it;
    if (!is_valid_hostname(server_and_port)) {
      invalid_peers.push_back(server_and_port);
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

uint32_t Gcs_xcom_utils::mhash(unsigned char *buf, size_t length) {
  size_t i = 0;
  uint32_t sum = 0;
  for (i = 0; i < length; i++) {
    sum += 0x811c9dc5 * (uint32_t)buf[i];
  }

  return sum;
}

int Gcs_xcom_utils::init_net() { return ::init_net(); }

int Gcs_xcom_utils::deinit_net() { return ::deinit_net(); }

void Gcs_xcom_proxy_impl::delete_node_address(unsigned int n,
                                              node_address *na) {
  ::delete_node_address(n, na);
}

int Gcs_xcom_proxy_impl::xcom_client_close_connection(
    connection_descriptor *fd) {
  return ::xcom_close_client_connection(fd);
}

connection_descriptor *Gcs_xcom_proxy_impl::xcom_client_open_connection(
    std::string saddr, xcom_port port) {
  char *addr = (char *)saddr.c_str();
  return ::xcom_open_client_connection(addr, port);
}

int Gcs_xcom_proxy_impl::xcom_client_add_node(connection_descriptor *fd,
                                              node_list *nl, uint32_t gid) {
  return ::xcom_client_add_node(fd, nl, gid);
}

int Gcs_xcom_proxy_impl::xcom_client_remove_node(connection_descriptor *fd,
                                                 node_list *nl, uint32_t gid) {
  return ::xcom_client_remove_node(fd, nl, gid);
}

int Gcs_xcom_proxy_impl::xcom_client_remove_node(node_list *nl, uint32_t gid) {
  int index = xcom_acquire_handler();
  int res = true;

  if (index != -1) {
    connection_descriptor *fd = m_xcom_handlers[index]->get_fd();

    /*
      XCOM will return 1 if the request is successfully processed or
      0 otherwise.
    */
    if (fd != NULL) res = ::xcom_client_remove_node(fd, nl, gid) ? false : true;
  }
  xcom_release_handler(index);
  return res;
}

int Gcs_xcom_proxy_impl::xcom_client_boot(node_list *nl, uint32_t gid) {
  int index = xcom_acquire_handler();
  int res = true;

  if (index != -1) {
    connection_descriptor *fd = m_xcom_handlers[index]->get_fd();

    if (fd != NULL) res = ::xcom_client_boot(fd, nl, gid);
  }
  xcom_release_handler(index);
  return res;
}

int Gcs_xcom_proxy_impl::xcom_client_send_data(unsigned long long len,
                                               char *data) {
  int res = true;

  if (len <= std::numeric_limits<unsigned int>::max()) {
    int index = xcom_acquire_handler();
    if (index != -1) {
      connection_descriptor *fd = m_xcom_handlers[index]->get_fd();
      /*
        XCOM will write all requested bytes or return -1 if there is
        an error. However, the wrapper will return 1 if connections
        to XCOM are not configured.

        Having said that, it should be enough to check whether data
        size was written and report false if so and true otherwise.
      */
      if (fd != NULL) {
        assert(len > 0);
        int64_t written =
            ::xcom_client_send_data(static_cast<uint32_t>(len), data, fd);
        if (static_cast<unsigned int>(written) >= len) res = false;
      }
    }
    xcom_release_handler(index);
  } else {
    /*
      GCS's message length is defined as unsigned long long type, but
      XCOM can only accept packets length of which are in unsigned int range.
      So it throws an error when gcs message is too big.
    */

    MYSQL_GCS_LOG_ERROR(
        "The data is too big. Data length should not"
        << " exceed " << std::numeric_limits<unsigned int>::max() << " bytes.");
  }
  return res;
}

int Gcs_xcom_proxy_impl::xcom_init(xcom_port xcom_listen_port) {
  /* Init XCom */
  ::xcom_fsm(xa_init, int_arg(0)); /* Basic xcom init */

  ::xcom_taskmain2(xcom_listen_port);

  return 0;
}

int Gcs_xcom_proxy_impl::xcom_exit(bool xcom_handlers_open) {
  int index = xcom_acquire_handler();
  int res = true;

  if (index != -1) {
    connection_descriptor *fd = m_xcom_handlers[index]->get_fd();

    /* Stop XCom */
    if (fd != NULL) {
      res = ::xcom_client_terminate_and_exit(fd);
    }

    xcom_release_handler(index);
  } else if (!xcom_handlers_open) {
    /* The handlers were not yet open, so use basic xcom stop */
    this->set_should_exit(1);

    res = false;
  }

  return res;
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

int Gcs_xcom_proxy_impl::xcom_init_ssl() {
  return ::xcom_init_ssl(m_server_key_file, m_server_cert_file,
                         m_client_key_file, m_client_cert_file, m_ca_file,
                         m_ca_path, m_crl_file, m_crl_path, m_cipher,
                         m_tls_version);
}

void Gcs_xcom_proxy_impl::xcom_destroy_ssl() { ::xcom_destroy_ssl(); }

int Gcs_xcom_proxy_impl::xcom_use_ssl() { return ::xcom_use_ssl(); }

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

bool Gcs_xcom_proxy_impl::xcom_open_handlers(std::string saddr,
                                             xcom_port port) {
  bool success = true;
  char *addr = (char *)saddr.c_str();
  int n = 0;

  m_lock_xcom_cursor.lock();
  if (m_xcom_handlers_cursor == -1 && addr != NULL) {
    for (int i = 0; i < m_xcom_handlers_size && success; i++) {
      connection_descriptor *con = NULL;

      while ((con = xcom_client_open_connection(addr, port)) == NULL &&
             n < Gcs_xcom_proxy::connection_attempts) {
        My_xp_util::sleep_seconds(1);
        n++;
      }

      n = 0;

      if (con == NULL) {
        success = false;
        break;
      } else {
        if (m_socket_util->disable_nagle_in_socket(con->fd) < 0) {
          success = false;
        }

        // This is a hack. It forces a protocol negotiation in
        // the current connection with the local xcom, so that
        // it does not happen later on.
        if ((::xcom_client_enable_arbitrator(con) <= 0) ||
            (::xcom_client_disable_arbitrator(con) <= 0))
          success = false;
      }

      m_xcom_handlers[i]->set_fd(con);
    }

    if (!success) {
      for (int i = 0; i < m_xcom_handlers_size; i++) {
        if (m_xcom_handlers[i]->get_fd() != NULL) {
          xcom_close_client_connection(m_xcom_handlers[i]->get_fd());
          m_xcom_handlers[i]->set_fd(NULL);
        }
      }
      m_xcom_handlers_cursor = -1;
    } else
      m_xcom_handlers_cursor = 0;
  } else {
    success = false;
  }
  m_lock_xcom_cursor.unlock();

  return success ? false : true;
}

bool Gcs_xcom_proxy_impl::xcom_close_handlers() {
  m_lock_xcom_cursor.lock();
  // Prevent that any other thread gets a new handler.
  m_xcom_handlers_cursor = -1;
  m_lock_xcom_cursor.unlock();

  /* Close the file descriptors */
  for (int i = 0; i < m_xcom_handlers_size; i++) {
    Xcom_handler *handler = m_xcom_handlers[i];
    if (handler && handler->get_fd() != NULL) {
      handler->lock();
      xcom_close_client_connection(handler->get_fd());
      handler->unlock();
    }
  }

#ifdef XCOM_HAVE_OPENSSL
  ::xcom_cleanup_ssl();
#endif

  return false;
}

void Gcs_xcom_proxy_impl::xcom_release_handler(int index) {
  if (index < m_xcom_handlers_size && index >= 0)
    m_xcom_handlers[index]->unlock();
}

int Gcs_xcom_proxy_impl::xcom_acquire_handler() {
  int res = -1;
  m_lock_xcom_cursor.lock();

  if (m_xcom_handlers_cursor != -1) {
    res = m_xcom_handlers_cursor;
    m_xcom_handlers[res]->lock();
    m_xcom_handlers_cursor =
        (m_xcom_handlers_cursor + 1) % m_xcom_handlers_size;
  }
  m_lock_xcom_cursor.unlock();

  return res;
}

/* purecov: begin deadcode */
Gcs_xcom_proxy_impl::Gcs_xcom_proxy_impl()
    : m_xcom_handlers_cursor(-1),
      m_lock_xcom_cursor(),
      m_xcom_handlers_size(XCOM_MAX_HANDLERS),
      m_wait_time(WAITING_TIME),
      m_xcom_handlers(NULL),
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
  m_xcom_handlers = new Xcom_handler *[m_xcom_handlers_size];

  for (int i = 0; i < m_xcom_handlers_size; i++)
    m_xcom_handlers[i] = new Xcom_handler();

  m_lock_xcom_cursor.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_cursor,
                          NULL);
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
    : m_xcom_handlers_cursor(-1),
      m_lock_xcom_cursor(),
      m_xcom_handlers_size(XCOM_MAX_HANDLERS),
      m_wait_time(wt),
      m_xcom_handlers(NULL),
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
  m_xcom_handlers = new Xcom_handler *[m_xcom_handlers_size];

  for (int i = 0; i < m_xcom_handlers_size; i++)
    m_xcom_handlers[i] = new Xcom_handler();

  m_lock_xcom_cursor.init(key_GCS_MUTEX_Gcs_xcom_proxy_impl_m_lock_xcom_cursor,
                          NULL);
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
  for (int i = 0; i < m_xcom_handlers_size; i++) delete m_xcom_handlers[i];

  delete[] m_xcom_handlers;
  m_lock_xcom_cursor.destroy();
  m_lock_xcom_ready.destroy();
  m_cond_xcom_ready.destroy();
  m_lock_xcom_comms_status.destroy();
  m_cond_xcom_comms_status.destroy();
  m_lock_xcom_exit.destroy();
  m_cond_xcom_exit.destroy();

  delete m_socket_util;
}

site_def const *Gcs_xcom_proxy_impl::find_site_def(synode_no synode) {
  return ::find_site_def(synode);
}

Gcs_xcom_proxy_impl::Xcom_handler::Xcom_handler() : m_lock(), m_fd(NULL) {
  m_lock.init(key_GCS_MUTEX_Xcom_handler_m_lock, NULL);
}

Gcs_xcom_proxy_impl::Xcom_handler::~Xcom_handler() { m_lock.destroy(); }

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

void Gcs_xcom_app_cfg::init() { ::init_cfg_app_xcom(); }

void Gcs_xcom_app_cfg::deinit() { ::deinit_cfg_app_xcom(); }

void Gcs_xcom_app_cfg::set_poll_spin_loops(unsigned int loops) {
  if (the_app_xcom_cfg) the_app_xcom_cfg->m_poll_spin_loops = loops;
}

int Gcs_xcom_proxy_impl::xcom_client_force_config(connection_descriptor *fd,
                                                  node_list *nl,
                                                  uint32_t group_id) {
  return ::xcom_client_force_config(fd, nl, group_id);
}

int Gcs_xcom_proxy_impl::xcom_client_force_config(node_list *nl,
                                                  uint32_t group_id) {
  int index = xcom_acquire_handler();
  int res = true;

  if (index != -1) {
    connection_descriptor *fd = m_xcom_handlers[index]->get_fd();

    if (fd != NULL) res = ::xcom_client_force_config(fd, nl, group_id);
  }
  xcom_release_handler(index);
  return res;
}

int Gcs_xcom_proxy_base::xcom_remove_nodes(Gcs_xcom_nodes &nodes,
                                           uint32_t group_id_hash) {
  node_list nl{0, nullptr};
  int ret = 1;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Removing %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    ret = xcom_client_remove_node(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return ret;
}

int Gcs_xcom_proxy_base::xcom_remove_node(const Gcs_xcom_node_information &node,
                                          uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_remove;
  nodes_to_remove.add_node(node);

  return xcom_remove_nodes(nodes_to_remove, group_id_hash);
}

int Gcs_xcom_proxy_base::xcom_force_nodes(Gcs_xcom_nodes &nodes,
                                          uint32_t group_id_hash) {
  node_list nl{0, nullptr};
  int ret = 1;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Forcing %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    ret = xcom_client_force_config(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return ret;
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

int Gcs_xcom_proxy_base::xcom_boot_node(Gcs_xcom_node_information &node,
                                        uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_boot;
  nodes_to_boot.add_node(node);
  node_list nl;
  int ret = 1;

  if (serialize_nodes_information(nodes_to_boot, nl)) {
    MYSQL_GCS_LOG_DEBUG("Booting up %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    ret = xcom_client_boot(&nl, group_id_hash);
  }
  free_nodes_information(nl);

  return ret;
}

int Gcs_xcom_proxy_base::xcom_add_nodes(connection_descriptor &con,
                                        Gcs_xcom_nodes &nodes,
                                        uint32_t group_id_hash) {
  node_list nl;
  int ret = 1;

  if (serialize_nodes_information(nodes, nl)) {
    MYSQL_GCS_LOG_DEBUG("Adding up %u nodes at %p", nl.node_list_len,
                        nl.node_list_val);
    ret = xcom_client_add_node(&con, &nl, group_id_hash);
  }
  free_nodes_information(nl);

  return ret;
}

int Gcs_xcom_proxy_base::xcom_add_node(connection_descriptor &con,
                                       const Gcs_xcom_node_information &node,
                                       uint32_t group_id_hash) {
  Gcs_xcom_nodes nodes_to_add;
  nodes_to_add.add_node(node);

  return xcom_add_nodes(con, nodes_to_add, group_id_hash);
}

bool is_valid_hostname(const std::string &server_and_port) {
  std::string::size_type delim_pos = server_and_port.find_last_of(":");
  std::string s_port =
      server_and_port.substr(delim_pos + 1, server_and_port.length());
  std::string hostname = server_and_port.substr(0, delim_pos);
  int port;
  bool error = false;
  struct addrinfo *addr = NULL;

  if ((error = (delim_pos == std::string::npos))) goto end;

  /* handle hostname*/
  error = (checked_getaddrinfo(hostname.c_str(), 0, NULL, &addr) != 0);
  if (error) goto end;

  /* handle port */
  if ((error = !is_number(s_port))) goto end;

  port = atoi(s_port.c_str());
  if ((error = port > USHRT_MAX)) goto end;

end:
  if (addr) freeaddrinfo(addr);
  return error == false;
}

void fix_parameters_syntax(Gcs_interface_parameters &interface_params) {
  std::string *compression_str =
      const_cast<std::string *>(interface_params.get_parameter("compression"));
  std::string *compression_threshold_str = const_cast<std::string *>(
      interface_params.get_parameter("compression_threshold"));
  std::string *wait_time_str =
      const_cast<std::string *>(interface_params.get_parameter("wait_time"));
  std::string *ip_whitelist_str =
      const_cast<std::string *>(interface_params.get_parameter("ip_whitelist"));
  std::string *join_attempts_str = const_cast<std::string *>(
      interface_params.get_parameter("join_attempts"));
  std::string *join_sleep_time_str = const_cast<std::string *>(
      interface_params.get_parameter("join_sleep_time"));

  // sets the default value for compression (ON by default)
  if (!compression_str) {
    interface_params.add_parameter("compression", "on");
  }

  // sets the default threshold if no threshold has been set
  if (!compression_threshold_str) {
    std::stringstream ss;
    ss << Gcs_message_stage_lz4::DEFAULT_THRESHOLD;
    interface_params.add_parameter("compression_threshold", ss.str());
  }

  // sets the default waiting time for timed_waits
  if (!wait_time_str) {
    std::stringstream ss;
    ss << WAITING_TIME;
    interface_params.add_parameter("wait_time", ss.str());
  }

  // sets the default ip whitelist
  if (!ip_whitelist_str) {
    std::stringstream ss;
    std::string iplist;
    std::map<std::string, int> out;

    // add local private networks that one has an IP on by default
    get_ipv4_local_private_addresses(out);

    if (out.empty())
      ss << "127.0.0.1/32,::1/128,";
    else {
      std::map<std::string, int>::iterator it;
      for (it = out.begin(); it != out.end(); it++) {
        ss << (*it).first << "/" << (*it).second << ",";
      }
    }

    iplist = ss.str();
    iplist.erase(iplist.end() - 1);  // remove trailing comma

    MYSQL_GCS_LOG_INFO("Added automatically IP ranges " << iplist
                                                        << " to the whitelist");

    interface_params.add_parameter("ip_whitelist", iplist);
  }

  // sets the default join attempts
  if (!join_attempts_str) {
    std::stringstream ss;
    ss << JOIN_ATTEMPTS;
    interface_params.add_parameter("join_attempts", ss.str());
  }

  // sets the default sleep time between join attempts
  if (!join_sleep_time_str) {
    std::stringstream ss;
    ss << JOIN_SLEEP_TIME;
    interface_params.add_parameter("join_sleep_time", ss.str());
  }
}

static enum_gcs_error is_valid_flag(const std::string param,
                                    std::string &flag) {
  enum_gcs_error error = GCS_OK;

  // transform to lower case
  std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);

  if (flag.compare("on") && flag.compare("off") && flag.compare("true") &&
      flag.compare("false")) {
    std::stringstream ss;
    ss << "Invalid parameter set to " << param << ". ";
    ss << "Valid values are either \"on\" or \"off\".";
    MYSQL_GCS_LOG_ERROR(ss.str());
    error = GCS_NOK;
  }
  return error;
}

bool is_parameters_syntax_correct(
    const Gcs_interface_parameters &interface_params) {
  enum_gcs_error error = GCS_OK;

  // get the parameters
  const std::string *group_name_str =
      interface_params.get_parameter("group_name");
  const std::string *local_node_str =
      interface_params.get_parameter("local_node");
  const std::string *peer_nodes_str =
      interface_params.get_parameter("peer_nodes");
  const std::string *bootstrap_group_str =
      interface_params.get_parameter("bootstrap_group");
  const std::string *poll_spin_loops_str =
      interface_params.get_parameter("poll_spin_loops");
  const std::string *compression_threshold_str =
      interface_params.get_parameter("compression_threshold");
  const std::string *compression_str =
      interface_params.get_parameter("compression");
  const std::string *wait_time_str =
      interface_params.get_parameter("wait_time");
  const std::string *join_attempts_str =
      interface_params.get_parameter("join_attempts");
  const std::string *join_sleep_time_str =
      interface_params.get_parameter("join_sleep_time");
  const std::string *suspicions_timeout_str =
      interface_params.get_parameter("suspicions_timeout");
  const std::string *suspicions_processing_period_str =
      interface_params.get_parameter("suspicions_processing_period");

  /*
    -----------------------------------------------------
    Checks
    -----------------------------------------------------
   */
  // validate group name
  if (group_name_str != NULL && group_name_str->size() == 0) {
    MYSQL_GCS_LOG_ERROR("The group_name parameter (" << group_name_str << ")"
                                                     << " is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate bootstrap string
  // accepted values: true, false, on, off
  if (bootstrap_group_str != NULL) {
    std::string &flag = const_cast<std::string &>(*bootstrap_group_str);
    error = is_valid_flag("bootstrap_group", flag);
    if (error == GCS_NOK) goto end;
  }

  // validate peer addresses addresses
  if (peer_nodes_str != NULL) {
    /*
     Parse and validate hostname and ports.
     */
    std::vector<std::string> hostnames_and_ports;
    std::vector<std::string> invalid_hostnames_and_ports;
    Gcs_xcom_utils::process_peer_nodes(peer_nodes_str, hostnames_and_ports);
    Gcs_xcom_utils::validate_peer_nodes(hostnames_and_ports,
                                        invalid_hostnames_and_ports);

    if (!invalid_hostnames_and_ports.empty()) {
      std::vector<std::string>::iterator invalid_hostnames_and_ports_it;
      for (invalid_hostnames_and_ports_it = invalid_hostnames_and_ports.begin();
           invalid_hostnames_and_ports_it != invalid_hostnames_and_ports.end();
           ++invalid_hostnames_and_ports_it) {
        MYSQL_GCS_LOG_WARN("Peer address \""
                           << (*invalid_hostnames_and_ports_it).c_str()
                           << "\" is not valid.");
      }
    }

    /*
     This means that none of the provided hosts is valid and that
     hostnames_and_ports had some sort of value
     */
    if (!invalid_hostnames_and_ports.empty() && hostnames_and_ports.empty()) {
      MYSQL_GCS_LOG_ERROR("None of the provided peer address is valid.");
      error = GCS_NOK;
      goto end;
    }
  }

  // local peer address
  if (local_node_str != NULL) {
    bool matches_local_ip = false;
    std::map<std::string, int> ips;
    std::map<std::string, int>::iterator it;

    std::string::size_type delim_pos = (*local_node_str).find_last_of(":");
    std::string host = (*local_node_str).substr(0, delim_pos);
    std::string ip;

    // first validate hostname
    if (!is_valid_hostname(*local_node_str)) {
      MYSQL_GCS_LOG_ERROR("Invalid hostname or IP address ("
                          << *local_node_str << ") assigned to the parameter "
                          << "local_node!");

      error = GCS_NOK;
      goto end;
    }

    // hostname was validated already, lets find the IP
    if (resolve_ip_addr_from_hostname(host, ip)) {
      MYSQL_GCS_LOG_ERROR("Unable to translate hostname " << host
                                                          << " to IP address!");
      error = GCS_NOK;
      goto end;
    }

    if (ip.compare(host) != 0)
      MYSQL_GCS_LOG_INFO("Translated '" << host << "' to " << ip);

    // second check that this host has that IP assigned
    if (get_ipv4_local_addresses(ips, true)) {
      MYSQL_GCS_LOG_ERROR(
          "Unable to get the list of local IP addresses for "
          "the server!");
      error = GCS_NOK;
      goto end;
    }

    // see if any IP matches
    for (it = ips.begin(); it != ips.end() && !matches_local_ip; it++)
      matches_local_ip = (*it).first.compare(ip) == 0;
    if (!matches_local_ip) {
      MYSQL_GCS_LOG_ERROR(
          "There is no local IP address matching the one "
          "configured for the local node ("
          << *local_node_str << ").");
      error = GCS_NOK;
      goto end;
    }
  }

  // poll spin loops
  if (poll_spin_loops_str &&
      (poll_spin_loops_str->size() == 0 || !is_number(*poll_spin_loops_str))) {
    MYSQL_GCS_LOG_ERROR("The poll_spin_loops parameter (" << poll_spin_loops_str
                                                          << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate compression
  if (compression_str != NULL) {
    std::string &flag = const_cast<std::string &>(*compression_str);
    error = is_valid_flag("compression", flag);
    if (error == GCS_NOK) goto end;
  }

  if (compression_threshold_str && (compression_threshold_str->size() == 0 ||
                                    !is_number(*compression_threshold_str))) {
    MYSQL_GCS_LOG_ERROR("The compression_threshold parameter ("
                        << compression_threshold_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (wait_time_str &&
      (wait_time_str->size() == 0 || !is_number(*wait_time_str))) {
    MYSQL_GCS_LOG_ERROR("The wait_time parameter (" << wait_time_str
                                                    << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (join_attempts_str &&
      (join_attempts_str->size() == 0 || !is_number(*join_attempts_str))) {
    MYSQL_GCS_LOG_ERROR("The join_attempts parameter (" << join_attempts_str
                                                        << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate suspicions parameters
  if (suspicions_timeout_str && (suspicions_timeout_str->size() == 0 ||
                                 !is_number(*suspicions_timeout_str))) {
    MYSQL_GCS_LOG_ERROR("The suspicions_timeout parameter ("
                        << suspicions_timeout_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (join_sleep_time_str &&
      (join_sleep_time_str->size() == 0 || !is_number(*join_sleep_time_str))) {
    MYSQL_GCS_LOG_ERROR("The join_sleep_time parameter (" << join_sleep_time_str
                                                          << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (suspicions_processing_period_str &&
      (suspicions_processing_period_str->size() == 0 ||
       !is_number(*suspicions_processing_period_str))) {
    MYSQL_GCS_LOG_ERROR("The suspicions_processing_period parameter ("
                        << suspicions_processing_period_str
                        << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

end:
  return error == GCS_NOK ? false : true;
}
