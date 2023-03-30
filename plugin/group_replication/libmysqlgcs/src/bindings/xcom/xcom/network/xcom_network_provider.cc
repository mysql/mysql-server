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

#ifndef _WIN32
#include <poll.h>
#endif

#ifdef WIN32
// In OpenSSL before 1.1.0, we need this first.
#include <winsock2.h>
#endif  // WIN32

#include "xcom/network/xcom_network_provider.h"
#include "xcom/network/xcom_network_provider_native_lib.h"

#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_transport.h"

void xcom_tcp_server_startup(Xcom_network_provider *net_provider) {
  xcom_port port = net_provider->get_port();

  result tcp_fd = {0, 0};
  tcp_fd = Xcom_network_provider_library::announce_tcp(port);
  if (tcp_fd.val < 0) {
    g_critical("Unable to announce tcp port %d. Port already in use?", port);
    net_provider->notify_provider_ready(true);
    return;
  }

  net_provider->notify_provider_ready();
  net_provider->set_open_server_socket(tcp_fd);

  G_INFO("XCom initialized and ready to accept incoming connections on port %d",
         port);

  int accept_fd = -1;
  struct sockaddr_storage sock_addr;
  socklen_t size = sizeof(struct sockaddr_storage);
  int funerr = 0;
  do {
    SET_OS_ERR(0);
    accept_fd = 0;
    funerr = 0;

    accept_fd = (int)accept(tcp_fd.val, (struct sockaddr *)&sock_addr, &size);
    funerr = to_errno(GET_OS_ERR);

    G_DEBUG("Accepting socket funerr=%d shutdown_tcp_server=%d", funerr,
            net_provider->should_shutdown_tcp_server());

    if (accept_fd < 0) {
      G_DEBUG("Error accepting socket funerr=%d shutdown_tcp_server=%d", funerr,
              net_provider->should_shutdown_tcp_server());

      continue;
    }

    /* Callback to check that the file descriptor is accepted. */
    if (!Xcom_network_provider_library::allowlist_socket_accept(
            accept_fd, get_site_def())) {
      net_provider->close_connection({accept_fd, nullptr});
      accept_fd = -1;
    }

    if (accept_fd == -1) {
      G_DEBUG("accept failed");
    } else {
      auto new_incoming_connection = new Network_connection(accept_fd);

#ifndef XCOM_WITHOUT_OPENSSL
      new_incoming_connection->ssl_fd = nullptr;
      if (::get_network_management_interface()->is_xcom_using_ssl()) {
        new_incoming_connection->ssl_fd = SSL_new(server_ctx);
        SSL_set_fd(new_incoming_connection->ssl_fd,
                   new_incoming_connection->fd);

        {
          int ret_ssl;
          int err;
          ERR_clear_error();
          ret_ssl = SSL_accept(new_incoming_connection->ssl_fd);
          err = SSL_get_error(new_incoming_connection->ssl_fd, ret_ssl);

          while (ret_ssl != SSL_SUCCESS) {
            /* Some other error, give up */
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
              break;
            }

            SET_OS_ERR(0);
            G_DEBUG("acceptor learner accept SSL retry fd %d",
                    new_incoming_connection->fd);
            ERR_clear_error();
            ret_ssl = SSL_accept(new_incoming_connection->ssl_fd);
            err = SSL_get_error(new_incoming_connection->ssl_fd, ret_ssl);
          }

          if (ret_ssl != SSL_SUCCESS) {
            G_DEBUG("acceptor learner accept SSL failed");
            net_provider->close_connection(*new_incoming_connection);
            accept_fd = -1;
          }
        }
      }
#endif
      if (accept_fd != -1) {
        new_incoming_connection->has_error = false;
        net_provider->set_new_connection(new_incoming_connection);
      } else {
        delete new_incoming_connection;
      }
    }
  } while (!net_provider->should_shutdown_tcp_server());

  net_provider->cleanup_secure_connections_context();

  return;
}

void ssl_shutdown_con(connection_descriptor *con) {
  if (con->fd >= 0 && con->ssl_fd != nullptr) {
    SSL_shutdown(con->ssl_fd);
    ssl_free_con(con);
  }
}

bool Xcom_network_provider::cleanup_secure_connections_context() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

  return false;
}

bool Xcom_network_provider::finalize_secure_connections_context() {
  Xcom_network_provider_ssl_library::xcom_destroy_ssl();

  return false;
}

int Xcom_network_provider::close_connection(
    const Network_connection &connection) {
#ifndef XCOM_WITHOUT_OPENSSL
  connection_descriptor temp_con;
  temp_con.fd = connection.fd;
  temp_con.ssl_fd = connection.ssl_fd;
  if (connection.has_error && temp_con.fd >= 0 && temp_con.ssl_fd != nullptr) {
    ssl_free_con(&temp_con);
  } else {
    ssl_shutdown_con(&temp_con);
  }
#endif

  int fd = connection.fd;
  result shut_close_result = xcom_shut_close_socket(&fd);

  return shut_close_result.val;
}

std::unique_ptr<Network_connection> Xcom_network_provider::open_connection(
    const std::string &address, const unsigned short port,
    const Network_security_credentials &security_credentials,
    int connection_timeout) {
  result fd = {0, 0};
  result ret = {0, 0};

  (void)security_credentials;

  auto cd = std::make_unique<Network_connection>(-1);
  cd->has_error = true;

  char buf[SYS_STRERROR_SIZE];

  G_DEBUG("connecting to %s %d", address.c_str(), port);

  struct addrinfo *addr = nullptr, *from_ns = nullptr;

  char buffer[20];
  sprintf(buffer, "%d", port);

  checked_getaddrinfo(address.c_str(), buffer, nullptr, &from_ns);

  if (from_ns == nullptr) {
    /* purecov: begin inspected */
    G_ERROR("Error retrieving server information.");
    goto end;
    /* purecov: end */
  }

  addr = Xcom_network_provider_library::does_node_have_v4_address(from_ns);

  /* Create socket after knowing the family that we are dealing with
     getaddrinfo returns a list of possible addresses. We will alays
     default to the first one in the list, which is V4 if applicable.
   */
  if ((fd = Xcom_network_provider_library::checked_create_socket(
           addr->ai_family, SOCK_STREAM, IPPROTO_TCP))
          .val < 0) {
    /* purecov: begin inspected */
    G_ERROR("Error creating socket in local GR->GCS connection to address %s",
            address.c_str());
    goto end;
    /* purecov: end */
  }

  /* Connect socket to address */

  SET_OS_ERR(0);

  if (Xcom_network_provider_library::timed_connect_msec(
          fd.val, addr->ai_addr, addr->ai_addrlen, connection_timeout) == -1) {
    fd.funerr = to_errno(GET_OS_ERR);
    G_DEBUG(
        "Connecting socket to address %s in port %d failed with error "
        "%d-%s.",
        address.c_str(), port, fd.funerr,
        strerr_msg(buf, sizeof(buf), fd.funerr));
    xcom_close_socket(&fd.val);
    goto end;
  }
  {
    int peer = 0;
    /* Sanity check before return */
    SET_OS_ERR(0);

    socklen_t addr_size =
        static_cast<socklen_t>(sizeof(struct sockaddr_storage));
    struct sockaddr_storage another_addr;
    ret.val = peer =
        xcom_getpeername(fd.val, (struct sockaddr *)&another_addr, &addr_size);
    ret.funerr = to_errno(GET_OS_ERR);
    if (peer >= 0) {
      ret = set_nodelay(fd.val);
      if (ret.val < 0) {
        this->close_connection({fd.val
#ifndef XCOM_WITHOUT_OPENSSL
                                ,
                                nullptr
#endif
        });
#if defined(_WIN32)
        G_DEBUG(
            "Setting node delay failed  while connecting to %s with error "
            "%d.",
            address.c_str(), ret.funerr);
#else
        G_DEBUG(
            "Setting node delay failed  while connecting to %s with error %d "
            "- "
            "%s.",
            address.c_str(), ret.funerr, strerror(ret.funerr));
#endif
        goto end;
      }
      G_DEBUG("client connected to %s %d fd %d", address.c_str(), port, fd.val);
    } else {
      /* Something is wrong */
      socklen_t errlen = sizeof(ret.funerr);

      getsockopt(fd.val, SOL_SOCKET, SO_ERROR, (char *)&ret.funerr, &errlen);
      if (ret.funerr == 0) {
        ret.funerr = to_errno(SOCK_ECONNREFUSED);
      }
      this->close_connection({fd.val
#ifndef XCOM_WITHOUT_OPENSSL
                              ,
                              nullptr
#endif
      });
      goto end;
#if defined(_WIN32)
      G_DEBUG(
          "Getting the peer name failed while connecting to server %s with  "
          "error %d.",
          address.c_str(), ret.funerr);
#else
      G_DEBUG(
          "Getting the peer name failed while connecting to server %s with "
          "error % d - %s.",
          address.c_str(), ret.funerr, strerror(ret.funerr));
#endif
      goto end;
    }

#ifndef XCOM_WITHOUT_OPENSSL
    if (::get_network_management_interface()->is_xcom_using_ssl()) {
      SSL *ssl = SSL_new(client_ctx);
      G_DEBUG("Trying to connect using SSL.")
      SSL_set_fd(ssl, fd.val);

      ERR_clear_error();
      ret.val = SSL_connect(ssl);
      ret.funerr = to_ssl_err(SSL_get_error(ssl, ret.val));

      if (ret.val != SSL_SUCCESS) {
        G_INFO("Error connecting using SSL %d %d", ret.funerr,
               SSL_get_error(ssl, ret.val));
        task_dump_err(ret.funerr);

        this->close_connection({fd.val
#ifndef XCOM_WITHOUT_OPENSSL
                                ,
                                ssl
#endif
                                ,
                                true});
        goto end;
      }

      if (Xcom_network_provider_ssl_library::ssl_verify_server_cert(
              ssl, address.c_str())) {
        G_MESSAGE("Error validating certificate and peer.");
        task_dump_err(ret.funerr);
        this->close_connection({fd.val
#ifndef XCOM_WITHOUT_OPENSSL
                                ,
                                ssl
#endif
                                ,
                                true});
        goto end;
      }

      cd->fd = fd.val;
      cd->ssl_fd = ssl;
      cd->has_error = false;

      G_DEBUG("Success connecting using SSL.")

      goto end;
    } else {
      cd->fd = fd.val;
      cd->ssl_fd = nullptr;
      cd->has_error = false;

      goto end;
    }
#else
    {
      cd->fd = fd.val;
      cd->has_error = false;

      goto end;
    }
#endif
  }

end:
  if (from_ns) freeaddrinfo(from_ns);

  return cd;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */

bool Xcom_network_provider::wait_for_provider_ready() {
  std::unique_lock<std::mutex> lck(m_init_lock);

  bool left_wait_ok = m_init_cond_var.wait_for(
      lck, std::chrono::seconds(10), [this] { return m_initialized; });

  if (!left_wait_ok) {
    G_DEBUG("wait_for_provider_ready is leaving with a timeout!")
    m_init_error = true;
  }

  return m_init_error;
}

void Xcom_network_provider::notify_provider_ready(bool init_error) {
  std::lock_guard<std::mutex> lck(m_init_lock);
  m_initialized = true;
  m_init_error = init_error;
  m_init_cond_var.notify_one();
}

std::pair<bool, int> Xcom_network_provider::start() {
  if (is_provider_initialized()) {
    return std::make_pair(true, -1);
  }

  set_shutdown_tcp_server(false);

  bool init_error = false;
  if (!(init_error = (m_port == 0))) {
    m_network_provider_tcp_server = std::thread(xcom_tcp_server_startup, this);

    init_error = wait_for_provider_ready();
  }

  if (init_error) {
    G_ERROR("Error initializing the group communication engine.")
    set_shutdown_tcp_server(true);
    if (m_network_provider_tcp_server.joinable())
      m_network_provider_tcp_server.join();

    std::unique_lock<std::mutex> lck(m_init_lock);
    m_initialized = false;
    lck.unlock();
  }

  return std::make_pair(init_error, init_error ? -1 : 0);
}

std::pair<bool, int> Xcom_network_provider::stop() {
  if (!is_provider_initialized()) {
    return std::make_pair(true, -1);
  }

  set_shutdown_tcp_server(true);

  Xcom_network_provider_library::gcs_shut_close_socket(
      &m_open_server_socket.val);

  std::lock_guard<std::mutex> lck(m_init_lock);
  m_initialized = false;

  this->reset_new_connection();

  if (m_network_provider_tcp_server.joinable())
    m_network_provider_tcp_server.join();

  return std::make_pair(false, 0);
}
