/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/client/xconnection_impl.h"

#include <errno.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif  // HAVE_NETINET_IN_H
#include <openssl/x509v3.h>
#include <cassert>
#include <chrono>  // NOLINT(build/c++11)
#include <future>  // NOLINT(build/c++11)
#include <limits>
#include <sstream>
#include <string>

#include "my_openssl_fips.h"

#include "errmsg.h"       // NOLINT(build/include_subdir)
#include "my_config.h"    // NOLINT(build/include_subdir)
#include "my_dbug.h"      // NOLINT(build/include_subdir)
#include "my_macros.h"    // NOLINT(build/include_subdir)
#include "scope_guard.h"  // NOLINT(build/include_subdir)

#include "plugin/x/client/context/xconnection_config.h"
#include "plugin/x/client/context/xssl_config.h"
#include "plugin/x/generated/mysqlx_error.h"
#include "plugin/x/src/config/config.h"
#include "sql-common/net_ns.h"

#ifndef WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif  // WIN32
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif  // HAVE_SYS_UN_H
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef WIN32
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#endif

namespace xcl {

const char *const ER_TEXT_SERVER_GONE = "MySQL server has gone away";
const char *const ER_TEXT_CANT_SET_TIMEOUT_WHEN_NOT_CONNECTED =
    "Can't set a timeout, socket not connected.";
const char *const ER_TEXT_TLS_ALREADY_ACTIVATED = "TLS already activated";
const char *const ER_TEXT_TLS_NOT_CONFIGURATED = "TLS not configured";
const char *const ER_TEXT_INVALID_SOCKET = "Invalid socket";
const char *const ER_TEXT_CANT_TIMEOUT_WHILE_READING =
    "Read operation failed because of a timeout";
const char *const ER_TEXT_CANT_TIMEOUT_WHILE_WRITTING =
    "Write operation failed because of a timeout";

#if defined(HAVE_SYS_UN_H)
const char *const ER_TEXT_UN_SOCKET_FILE_NOT_SET =
    "UNIX Socket file was not specified";
#else
const char *const ER_TEXT_UN_SOCKET_NOT_SUPPORTED =
    "UNIX sockets aren't supported on current OS";
#endif  // defined(HAVE_SYS_UN_H)

namespace details {

class Connection_state : public XConnection::State {
 public:
  Connection_state(Vio *vio, const bool is_ssl_configured,
                   const bool is_ssl_active, const bool is_connected,
                   const Connection_type connection_type)
      : m_vio(vio),
        m_is_ssl_configured(is_ssl_configured),
        m_is_ssl_active(is_ssl_active),
        m_is_connected(is_connected),
        m_connection_type(connection_type) {}

  bool is_ssl_configured() const override { return m_is_ssl_configured; }
  bool is_ssl_activated() const override { return m_is_ssl_active; }
  bool is_connected() const override { return m_is_connected; }

  std::string get_ssl_version() const override {
    if (nullptr == m_vio->ssl_arg) return "";

    return SSL_get_version(reinterpret_cast<SSL *>(m_vio->ssl_arg));
  }

  std::string get_ssl_cipher() const override {
    if (nullptr == m_vio->ssl_arg) return "";

    return SSL_get_cipher(reinterpret_cast<SSL *>(m_vio->ssl_arg));
  }

  Connection_type get_connection_type() const override {
    return m_connection_type;
  }

  bool has_data() const override {
    bool res = m_vio->has_data(m_vio);
    if (res) return true;

    return vio_io_wait(m_vio, VIO_IO_EVENT_READ, 0) != 0;
  }

  Vio *m_vio;
  bool m_is_ssl_configured;
  bool m_is_ssl_active;
  bool m_is_connected;
  Connection_type m_connection_type;
};

const char *null_when_empty(const std::string &value) {
  if (value.empty()) return nullptr;

  return value.c_str();
}

int make_vio_timeout(const int64_t value) {
  if (value > 0) {
    if (std::numeric_limits<int>::max() < value)
      return std::numeric_limits<int>::max();

    return static_cast<int>(value);
  }

  return -1;
}

XError ssl_verify_server_cert(Vio *vio, const std::string &server_hostname) {
  SSL *ssl = reinterpret_cast<SSL *>(vio->ssl_arg);

  if (nullptr == ssl) {
    return XError{CR_SSL_CONNECTION_ERROR, "No SSL pointer found", true};
  }

  X509 *server_cert = SSL_get_peer_certificate(ssl);
  if (nullptr == server_cert) {
    return XError{CR_SSL_CONNECTION_ERROR, "Could not get server certificate",
                  true};
  }

  auto free_cert =
      create_scope_guard([server_cert]() { X509_free(server_cert); });

  if (X509_V_OK != SSL_get_verify_result(ssl)) {
    return XError{CR_SSL_CONNECTION_ERROR,
                  "Failed to verify the server certificate", true};
  }

  /*
    For OpenSSL 1.0.2 and up we already set certificate verification
    parameters in the new_VioSSLFd() to perform automatic checks.
  */
#if OPENSSL_VERSION_NUMBER < 0x10002000L
  /*
     OpenSSL prior to 1.0.2 do not support X509_check_host() function.
     Use deprecated X509_get_subject_name() instead.
  */

  X509_NAME *subject = X509_get_subject_name(server_cert);
  int cn_loc = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);

  if (cn_loc < 0) {
    return XError{CR_SSL_CONNECTION_ERROR,
                  "Failed to get CN location in the certificate subject", true};
  }

  // Get the CN entry for given location
  X509_NAME_ENTRY *cn_entry = X509_NAME_get_entry(subject, cn_loc);
  if (nullptr == cn_entry) {
    return XError{CR_SSL_CONNECTION_ERROR,
                  "Failed to get CN entry using CN location", true};
  }

  // Get CN from common name entry
  ASN1_STRING *cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
  if (nullptr == cn_asn1) {
    return XError{CR_SSL_CONNECTION_ERROR, "Failed to get CN from CN entry",
                  true};
  }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  const auto cn = reinterpret_cast<char *>(ASN1_STRING_data(cn_asn1));
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  const auto cn =
      reinterpret_cast<const char *>(ASN1_STRING_get0_data(cn_asn1));
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  const auto cn_len = static_cast<size_t>(ASN1_STRING_length(cn_asn1));

  // There should not be any NULL embedded in the CN
  if (cn_len != strlen(cn)) {
    return XError{CR_SSL_CONNECTION_ERROR,
                  "NULL embedded in the certificate CN", true};
  }

  if (server_hostname != cn) {
    return XError{CR_SSL_CONNECTION_ERROR, "SSL certificate validation failure",
                  true};
  }
#endif /* OPENSSL_VERSION_NUMBER < 0x10002000L */

  return {};
}

inline int get_shutdown_consts(const Shutdown_type type) {
  switch (type) {
    case Shutdown_type::Send:
      return SHUT_WR;
    case Shutdown_type::Recv:
      return SHUT_RD;
    case Shutdown_type::Both:
      return SHUT_RDWR;
  }

  return 0;
}

class Non_blocking {
 public:
  Non_blocking(Vio *vio) : m_vio(vio) { vio_set_blocking_flag(vio, false); }

  ~Non_blocking() {
    vio_set_blocking_flag(m_vio, true);
    // Setting blocking flag, sets the socket into the blocking/nonblocking
    // mode. Both the blocking flag and socket mode are in sync only at linux,
    // on these systems VIO sets blocking flag leaving socket in
    // non-blocking mode.
    //
    // The described behavior is a workaround in VIO, for constant
    // "MSG_DONTWAIT" which is not supported on FreeBSD, Solaris, MacOS.
    // This forces our code to write workaround below:
#if !defined(__linux__)
    vio_set_blocking(m_vio, false);
#endif  // !defined(__linux__)
  }

 private:
  Vio *m_vio;
};

enum Wait_flags {
  k_wait_flag_read = 1,
  k_wait_flag_write = 2,
};

#if !defined(_WIN32)
#define SOCKET_WAIT_INITIALIZE(FD, TIMEOUT)         \
  struct pollfd pfd;                                \
  int wait_timeout = (TIMEOUT >= 0 ? TIMEOUT : -1); \
  memset(&pfd, 0, sizeof(pfd));                     \
  pfd.fd = FD;
#define SOCKET_WAIT_READ() pfd.events |= POLLIN | POLLPRI;
#define SOCKET_WAIT_WRITE() pfd.events |= POLLOUT;
#define SOCKET_WAIT() poll(&pfd, 1, wait_timeout);
#define SOCKET_WAIT_CAN_READ() pfd.revents &POLLIN
#define SOCKET_WAIT_CAN_WRITE() pfd.revents &POLLOUT

#elif defined(_WIN32)
#define SOCKET_WAIT_INITIALIZE(FD, TIMEOUT)                       \
  fd_set sockwait_readfds, sockwait_writefds, sockwait_exceptfds; \
  struct timeval sockwait_tm;                                     \
  my_socket sockwait_socket = FD;                                 \
  auto sockwait_timeout = TIMEOUT;                                \
  if (TIMEOUT >= 0) {                                             \
    sockwait_tm.tv_sec = TIMEOUT / 1000;                          \
    sockwait_tm.tv_usec = (TIMEOUT % 1000) * 1000;                \
  }                                                               \
  FD_ZERO(&sockwait_readfds);                                     \
  FD_ZERO(&sockwait_writefds);                                    \
  FD_ZERO(&sockwait_exceptfds);                                   \
  FD_SET(sockwait_socket, &sockwait_exceptfds);
#define SOCKET_WAIT_READ() FD_SET(sockwait_socket, &sockwait_readfds);
#define SOCKET_WAIT_WRITE() FD_SET(sockwait_socket, &sockwait_writefds);
#define SOCKET_WAIT()                                                       \
  select((int)(sockwait_socket + 1), &sockwait_readfds, &sockwait_writefds, \
         &sockwait_exceptfds, (sockwait_timeout >= 0) ? &sockwait_tm : NULL);
#define SOCKET_WAIT_CAN_READ() FD_ISSET(sockwait_socket, &sockwait_readfds)
#define SOCKET_WAIT_CAN_WRITE() FD_ISSET(sockwait_socket, &sockwait_writefds)
#endif

int do_wait_for_socket(MYSQL_SOCKET sd, int wait_flags, const int timeout) {
  int ret;
  SOCKET_WAIT_INITIALIZE(sd.fd, timeout * 1000);

  MYSQL_SOCKET_WAIT_VARIABLES(locker, state) /* no ';' */
  MYSQL_START_SOCKET_WAIT(locker, &state, sd, PSI_SOCKET_SELECT, 0);

  while (wait_flags) {
    if (k_wait_flag_read & wait_flags) {
      SOCKET_WAIT_READ();
      wait_flags = wait_flags & (~k_wait_flag_read);
      continue;
    } else if (k_wait_flag_write & wait_flags) {
      SOCKET_WAIT_WRITE()
      wait_flags = wait_flags & (~k_wait_flag_write);
      continue;
    }
  }

  /*
    Wait for the I/O event and return early in case of
    error or timeout.
  */
  ret = SOCKET_WAIT();

  MYSQL_END_SOCKET_WAIT(locker, 0);

  switch (ret) {
    case -1:
      /* On error, -1 is returned. */
      return -1;
    case 0:
      errno = SOCKET_ETIMEDOUT;
      return 0;
  }

  if (SOCKET_WAIT_CAN_WRITE()) return k_wait_flag_write;

  if (SOCKET_WAIT_CAN_READ()) return k_wait_flag_read;

  return -1;
}

}  // namespace details

static bool should_retry(const int error) {
#if SOCKET_EAGAIN == SOCKET_EWOULDBLOCK
  return (error == SOCKET_EAGAIN);
#else
  return (error == SOCKET_EAGAIN || error == SOCKET_EWOULDBLOCK);
#endif
}

const int k_socket_want_write = static_cast<int>(VIO_SOCKET_WANT_WRITE);

Connection_impl::Connection_impl(std::shared_ptr<Context> context)
    : m_vioSslFd(nullptr),
      m_vio(nullptr),
      m_ssl_active(false),
      m_connected(false),
      m_ssl_init_error(SSL_INITERR_NOERROR),
      m_context(context) {}

Connection_impl::~Connection_impl() { close(); }

XError Connection_impl::connect_to_localhost(const std::string &unix_socket) {
  m_connection_type = Connection_type::Unix_socket;
  m_hostname = "localhost";
#if defined(HAVE_SYS_UN_H)
  sockaddr_un addr;

  if (unix_socket.empty())
    return XError(CR_UNKNOWN_HOST, ER_TEXT_UN_SOCKET_FILE_NOT_SET, true);

  if (unix_socket.length() > (sizeof(addr.sun_path) - 1)) {
    std::stringstream stream;

    stream << "UNIX Socket file name too long, size should be less or equal "
           << sizeof(addr.sun_path) - 1;
    return XError(CR_UNKNOWN_HOST, stream.str(), true);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, unix_socket.c_str(), sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  auto error = connect(reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

  if (error) {
    return XError(
        CR_CONNECTION_ERROR,
        std::string(error.what()) + ", while connecting to " + unix_socket,
        true);
  }

  m_connected = true;
  return {};
#else
  return XError(CR_SOCKET_CREATE_ERROR, ER_TEXT_UN_SOCKET_NOT_SUPPORTED, true);
#endif  // defined(HAVE_SYS_UN_H)
}

XError Connection_impl::connect(const std::string &host, const uint16_t port,
                                const Internet_protocol ip_mode) {
  m_connection_type = Connection_type::Tcp;
  m_hostname = host;

  char port_buf[NI_MAXSERV];
  snprintf(port_buf, NI_MAXSERV, "%d", port);

  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_family = AF_UNSPEC;

  if (Internet_protocol::V6 == ip_mode)
    hints.ai_family = AF_INET6;
  else if (Internet_protocol::V4 == ip_mode)
    hints.ai_family = AF_INET;

  auto addr_future =
      std::async(std::launch::async, [&host, &port_buf, &hints]() {
        auto addr_cleanup = [](addrinfo *addr) {
          if (addr) freeaddrinfo(addr);
        };
        std::shared_ptr<addrinfo> result_list(nullptr, addr_cleanup);
        addrinfo *temp_res_lst;
        auto gai_errno =
            getaddrinfo(host.c_str(), port_buf, &hints, &temp_res_lst);
        if (gai_errno == 0) result_list.reset(temp_res_lst, addr_cleanup);
        return result_list;
      });

  const auto timeout = m_context->m_connection_config.m_timeout_session_connect;
  const auto delay = std::chrono::milliseconds(
      timeout > 0 ? timeout : std::numeric_limits<int32_t>::max());
  if (addr_future.wait_for(delay) == std::future_status::timeout) {
    return XError(CR_X_SESSION_CONNECT_TIMEOUT,
                  "Session_connect_timeout limit exceeded", true);
  }

  auto resolved_addr_list_ptr = addr_future.get();
  if (!resolved_addr_list_ptr)
    return XError(CR_UNKNOWN_HOST, "No such host is known '" + host + "'",
                  true);

  XError error;
  const auto &ns = m_context->m_connection_config.m_network_namespace;
  if (!ns.empty()) {
#ifdef HAVE_SETNS
    if (set_network_namespace(ns)) {
      return XError(CR_SOCKET_CREATE_ERROR,
                    "Failed to set active network namespace " + ns, true);
    }
#else
    return XError(CR_SOCKET_CREATE_ERROR,
                  "Network namespace not supported by the platform", true);
#endif
  }
  for (const auto *t_res = resolved_addr_list_ptr.get(); t_res;
       t_res = t_res->ai_next) {
    error = connect(reinterpret_cast<sockaddr *>(t_res->ai_addr),
                    t_res->ai_addrlen);

    if (!error) break;
  }

#ifdef HAVE_SETNS
  if (!ns.empty() && restore_original_network_namespace() && !error) {
    /*
      Report about error during restoring network namespace only
      in case an error not happened on connecting to a server.
      In other words, error got during establishing a connection
      has higher priority.
    */
    return XError(CR_SOCKET_CREATE_ERROR,
                  "Fails to restore original network namespace " + ns, true);
  }
#endif

  if (error) {
    std::string error_description = error.what();
    return XError(CR_CONNECTION_ERROR,
                  error_description + " connecting to " + host + ":" + port_buf,
                  true);
  }

  m_connected = true;

  return XError();
}

XError Connection_impl::connect(sockaddr *addr, const std::size_t addr_size) {
  enum_vio_type type = VIO_TYPE_TCPIP;
  int err = 0;
  const bool is_unix_socket = AF_UNIX == addr->sa_family;
  my_socket s =
      ::socket(addr->sa_family, SOCK_STREAM, is_unix_socket ? 0 : IPPROTO_TCP);

  if (is_unix_socket) type = VIO_TYPE_SOCKET;

  if (INVALID_SOCKET == s)
    return XError(CR_SOCKET_CREATE_ERROR, ER_TEXT_INVALID_SOCKET);

  auto vio = vio_new(s, type, 0);
  auto error =
      vio_socket_connect(vio, addr, static_cast<socklen_t>(addr_size), false,
                         details::make_vio_timeout(
                             m_context->m_connection_config.m_timeout_connect));

  if (error) {
    err = socket_errno;
    vio_delete(vio);

    return get_socket_error(err);
  }

  m_vio = vio;
  // Enable TCP_NODELAY
  vio_fastsend(m_vio);

  const auto read_timeout = m_context->m_connection_config.m_timeout_read;
  set_read_timeout(
      details::make_vio_timeout((read_timeout < 0) ? -1 : read_timeout / 1000));
  const auto write_timeout = m_context->m_connection_config.m_timeout_write;
  set_write_timeout(details::make_vio_timeout(
      (write_timeout < 0) ? -1 : write_timeout / 1000));

  m_buffer.reset(
      new Cyclic_buffer(m_context->m_connection_config.m_buffer_receive_size));

  return XError();
}

my_socket Connection_impl::get_socket_fd() {
  if (nullptr == m_vio) return INVALID_SOCKET;

  return vio_fd(m_vio);
}

std::string Connection_impl::get_socket_error_description(const int error_id) {
  std::string strerr;
#ifdef _WIN32
  char *s = nullptr;
  if (0 == FormatMessage(
               FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
               nullptr, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
               (LPSTR)&s, 0, nullptr)) {
    char text[256];
    snprintf(text, sizeof(text), "Error %i", error_id);
    strerr = text;
  } else {
    strerr = s;
    LocalFree(s);

    while (strerr.size() && (strerr[strerr.size() - 1] == '\n' ||
                             strerr[strerr.size() - 1] == '\r')) {
      strerr.erase(strerr.size() - 1);
    }

    if (strerr.size() && strerr[strerr.size() - 1] == '.') {
      strerr.erase(strerr.size() - 1);
    }
  }
#else
  strerr = strerror(error_id);
#endif

  return strerr;
}

XError Connection_impl::get_ssl_init_error(const int init_error_id) {
  return XError(CR_SSL_CONNECTION_ERROR,
                sslGetErrString((enum_ssl_init_error)init_error_id), true);
}

#define SOCKET_EPIPE IF_WIN(ERROR_BROKEN_PIPE, EPIPE)
#define SOCKET_ECONNABORTED IF_WIN(WSAECONNABORTED, ECONNABORTED)

XError Connection_impl::get_socket_error(const int error_id) {
  switch (error_id) {
#if defined(__APPLE__)
    // OSX return this undocumented error in case of kernel race-condition
    // lets ignore it and next call to any 'IO' function should return correct
    // error
    case EPROTOTYPE:
      return XError();
#endif  // defined(__APPLE__)

    case SOCKET_ECONNABORTED:
    case SOCKET_ECONNRESET:
      return XError(CR_SERVER_GONE_ERROR, ER_TEXT_SERVER_GONE, true);
    // For now pipe is not fatal, still in future this might change
    // in some cases
    case SOCKET_EPIPE:
      return XError(CR_SERVER_GONE_ERROR, ER_TEXT_SERVER_GONE);
    default:
      return XError(CR_UNKNOWN_ERROR, get_socket_error_description(error_id),
                    true);
  }
}

XError Connection_impl::get_ssl_error(const int error_id) {
  const unsigned int buffer_size = 1024;
  std::string r;

  r.resize(buffer_size);

  char *buffer = &r[0];

  ERR_error_string_n(error_id, buffer, buffer_size);
  return XError(CR_SSL_CONNECTION_ERROR, buffer);
}

XError Connection_impl::activate_tls() {
  if (nullptr == m_vio) return get_socket_error(SOCKET_ECONNRESET);

  if (nullptr != m_vioSslFd)
    return XError{CR_SSL_CONNECTION_ERROR, ER_TEXT_TLS_ALREADY_ACTIVATED, true};

  if (!m_context->m_ssl_config.is_configured())
    return XError{CR_SSL_CONNECTION_ERROR, ER_TEXT_TLS_NOT_CONFIGURATED, true};

  char err_string[OPENSSL_ERROR_LENGTH] = {'\0'};
  if (set_fips_mode(
          static_cast<uint32_t>(m_context->m_ssl_config.m_ssl_fips_mode),
          err_string)) {
    return XError{CR_SSL_CONNECTION_ERROR, err_string, true};
  }
  auto ssl_ctx_flags = process_tls_version(
      details::null_when_empty(m_context->m_ssl_config.m_tls_version));

  const bool verify_identity =
      Ssl_config::Mode::Ssl_verify_identity == m_context->m_ssl_config.m_mode;
  m_vioSslFd = new_VioSSLConnectorFd(
      details::null_when_empty(m_context->m_ssl_config.m_key),
      details::null_when_empty(m_context->m_ssl_config.m_cert),
      details::null_when_empty(m_context->m_ssl_config.m_ca),
      details::null_when_empty(m_context->m_ssl_config.m_ca_path),
      details::null_when_empty(m_context->m_ssl_config.m_cipher), nullptr,
      &m_ssl_init_error,
      details::null_when_empty(m_context->m_ssl_config.m_crl),
      details::null_when_empty(m_context->m_ssl_config.m_crl_path),
      ssl_ctx_flags, verify_identity ? m_hostname.c_str() : nullptr);

  if (nullptr == m_vioSslFd) return get_ssl_init_error(m_ssl_init_error);

  // When mode it set to Ssl_config::Mode_ssl_verify_ca
  // then lower layers are going to verify it
  unsigned long error{false};  // NOLINT
  if (0 != sslconnect(m_vioSslFd, m_vio, 60, nullptr, &error, nullptr)) {
    return get_ssl_error(error);
  }

  if (verify_identity) {
    auto error = details::ssl_verify_server_cert(m_vio, m_hostname);

    if (error) return error;
  }

  m_ssl_active = true;

  return XError();
}

XError Connection_impl::shutdown(const Shutdown_type how_to_shutdown) {
  if (0 !=
      ::shutdown(vio_fd(m_vio), details::get_shutdown_consts(how_to_shutdown)))
    return get_socket_error(socket_errno);

  m_connected = false;
  return XError();
}

XError Connection_impl::wait_for_socket_and_read_to_buffer() {
  const auto wait_flag = details::do_wait_for_socket(
      m_vio->mysql_socket,
      details::k_wait_flag_read | details::k_wait_flag_write, m_write_timeout);

  switch (wait_flag) {
    case 0:
      return XError{CR_X_WRITE_TIMEOUT, ER_TEXT_CANT_TIMEOUT_WHILE_WRITTING};
    case details::k_wait_flag_write:
      break;
    case details::k_wait_flag_read: {
      uint8_t *out_ptr;
      uint64_t out_size;

      while (m_buffer->begin_direct_update(&out_ptr, &out_size)) {
        auto result = static_cast<int>(vio_read(m_vio, out_ptr, out_size));

        if (0 >= result) break;
        m_buffer->end_direct_update(result);
      }
    } break;
    default: {
      const int vio_error = vio_errno(m_vio);
      return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET);
    }
  }

  return {};
}
XError Connection_impl::write(const uint8_t *data,
                              const std::size_t data_length) {
  std::size_t left_data_to_write = data_length;
  const unsigned char *data_to_send = (const unsigned char *)data;

  if (nullptr == m_vio) return get_socket_error(SOCKET_ECONNRESET);

  details::Non_blocking non_blocking(m_vio);

  do {
    const int number_of_byte_send =
        static_cast<int>(vio_write(m_vio, data_to_send, left_data_to_write));

    const bool should_wait_for_read_or_write =
        number_of_byte_send == k_socket_want_write ||
        (-1 == number_of_byte_send && should_retry(vio_errno(m_vio)));

    if (0 == number_of_byte_send) {
      return get_socket_error(SOCKET_ECONNRESET);
    }

    if (should_wait_for_read_or_write) {
      const auto error = wait_for_socket_and_read_to_buffer();

      if (error) return error;
      continue;
    }

    if (number_of_byte_send < 0) {
      const int vio_error = vio_errno(m_vio);

      if (SOCKET_EWOULDBLOCK == vio_error || vio_was_timeout(m_vio))
        return XError{CR_X_WRITE_TIMEOUT, ER_TEXT_CANT_TIMEOUT_WHILE_WRITTING};

      return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET);
    }

    left_data_to_write -= number_of_byte_send;
    data_to_send += number_of_byte_send;
  } while (left_data_to_write > 0);

  return XError();
}

XError Connection_impl::read(uint8_t *data_head,
                             const std::size_t data_length) {
  int result = 0;
  std::size_t data_to_send = data_length;
  char *data = reinterpret_cast<char *>(data_head);

  if (nullptr == m_vio) return get_socket_error(SOCKET_ECONNRESET);

  do {
    if (m_buffer->space_used()) {
      result = m_buffer->get(data_head, data_length);
    } else {
      result = static_cast<int>(
          vio_read(m_vio, reinterpret_cast<uint8_t *>(data), data_to_send));
    }

    if (-1 == result) {
      int vio_error = vio_errno(m_vio);

      if (SOCKET_EWOULDBLOCK == vio_error || vio_was_timeout(m_vio)) {
        if (!vio_is_connected(m_vio))
          return get_socket_error(SOCKET_ECONNRESET);

        return XError{CR_X_READ_TIMEOUT, ER_TEXT_CANT_TIMEOUT_WHILE_READING};
      }

      vio_error = vio_error == 0 ? SOCKET_ECONNRESET : vio_error;
      return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET);
    } else if (0 == result) {
      return get_socket_error(SOCKET_ECONNRESET);
    }

    data_to_send -= result;
    data += result;
  } while (data_to_send != 0);

  return XError();
}

XError Connection_impl::set_read_timeout(const int deadline_seconds) {
  if (nullptr == m_vio) {
    return XError{CR_INVALID_CONN_HANDLE,
                  ER_TEXT_CANT_SET_TIMEOUT_WHEN_NOT_CONNECTED, true};
  }

  vio_timeout(m_vio, 0, deadline_seconds);
  return {};
}

XError Connection_impl::set_write_timeout(const int deadline_seconds) {
  if (nullptr == m_vio) {
    return XError{CR_INVALID_CONN_HANDLE,
                  ER_TEXT_CANT_SET_TIMEOUT_WHEN_NOT_CONNECTED, true};
  }

  m_write_timeout = deadline_seconds;
  vio_timeout(m_vio, 1, deadline_seconds);

  return {};
}

const XConnection::State &Connection_impl::state() {
  m_state.reset(new details::Connection_state(
      m_vio, m_context->m_ssl_config.is_configured(), m_ssl_active, m_connected,
      m_connection_type));
  return *m_state;
}

void Connection_impl::close() {
  if (m_vio) {
    vio_delete(m_vio);
    m_connected = false;
    m_ssl_active = false;
    m_vio = nullptr;
  }

  if (m_vioSslFd) {
    free_vio_ssl_acceptor_fd(m_vioSslFd);
    m_vioSslFd = nullptr;
  }
}

}  // namespace xcl
