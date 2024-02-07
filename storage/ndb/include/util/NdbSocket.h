/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_UTIL_SECURE_SOCKET_H
#define NDB_UTIL_SECURE_SOCKET_H

#include <utility>  // std::swap
#include "portlib/NdbMutex.h"
#include "portlib/ndb_socket.h"
#include "portlib/ndb_socket_poller.h"

static constexpr int TLS_BUSY_TRY_AGAIN = -2;

class NdbSocket {
 public:
  NdbSocket() = default;
  NdbSocket(NdbSocket &&oth);
  // TODO: move ndb_socket_t into NdbSocket
  NdbSocket(ndb_socket_t ndbsocket) : s(ndbsocket) {}
  ~NdbSocket() {
    assert(ssl == nullptr);
    assert(!ndb_socket_valid(s));
    if (mutex != nullptr) NdbMutex_Destroy(mutex);
  }

  NdbSocket &operator=(NdbSocket &&);

  int is_valid() const { return ndb_socket_valid(s); }
  bool has_tls() const { return ssl; }
  ndb_socket_t ndb_socket() const { return s; }
  socket_t native_socket() const { return ndb_socket_get_native(s); }
  socket_t release_native_socket();
  std::string to_string() const;

  /* Get an SSL for client-side TLS.
   * Returns pointer to SSL. Returns null if CTX is null, or SSL_new() fails,
   * or if the OpenSSL version is not supported for NDB TLS.
   * When ready to switch over, call associate().
   */
  static struct ssl_st *get_client_ssl(struct ssl_ctx_st *);

  /* Get an SSL for for server-side TLS.
   * Returns pointer to SSL. Returns null if CTX is null, or SSL_new() fails,
   * or if the OpenSSL version is not supported for NDB TLS.
   * When ready to switch over, call associate().
   */
  static struct ssl_st *get_server_ssl(struct ssl_ctx_st *);

  /* Free an SSL returned by get_client_ssl() or get_server_ssl(), in the case
   * when an error has prevented it from being associated with a socket.
   */
  static void free_ssl(struct ssl_st *);

  /* Associate a socket with an SSL and create mutex for SSL-protection.
   * Returns true on success.
   * Returns false if socket already has an SSL association,
   *               or if SSL is null,
   *               or on failure from SSL_set_fd().
   */
  bool associate(struct ssl_st *);

  /* Run TLS handshake.
   * This must be done synchronously on a blocking socket.
   *
   * Returns:
   *    true on success.
   *    true if the socket does not have TLS enabled.
   *    false on failure; the socket has been invalidated and closed.
   *    false if socket is non-blocking (based on SSL mode flags)
   */
  bool do_tls_handshake() { return ssl ? ssl_handshake() : true; }

  /* Update keys (TLS 1.3).
   * If request_peer_update is set to true, request the peer to also update.
   * Returns true on success. If TLS 1.3 is not in use, returns false.
   */
  bool update_keys(bool request_peer_update = false) const;

  /* Renegotiate (TLS 1.2)
   * Like handshake, renegotiation is supported only as a synchronous action
   * on a blocking socket. Returns true if renegotiation has been scheduled;
   * caller should then call do_tls_handshake().
   * Returns false if connection is plaintext or is using TLS 1.3.
   */
  bool renegotiate();

  /* A single key_update_pending() function supports both TLS 1.2 and 1.3.
   * Returns true if a key update is pending (TLS 1.3).
   * Returns true if renegotiation is pending (TLS 1.2).
   */
  bool key_update_pending() const;

  /* Get peer's TLS certificate, and update the certificate reference count.
   *
   * The caller should free the returned pointer using Certificate::free().
   * Returns nullptr if no certificate is available.
   */
  struct x509_st *peer_certificate() const;

  /* Set socket behavior to blocking or non-blocking.
     For SSL sockets, this should be done after associate().
     This call also sets the appropriate TLS segment size and SSL mode flags
     for the socket.
      * blocking SSL sockets have mode flag SSL_MODE_AUTO_RETRY.
      * non-blocking SSL sockets have mode flags SSL_MODE_ENABLE_PARTIAL_WRITE
        and SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER.
     See SSL_set_mode(3) for reference on flags.
  */
  int set_nonblocking(int on) const;

  /* ndb_socket.h */
  ssize_t recv(char *buf, size_t len, int peek = 0) const;
  ssize_t send(const char *buf, size_t len) const;
  ssize_t writev(const struct iovec *vec, int nvec) const;

  /* socket_io.h */
  /* read() reads with a timeout. write() writes with a timeout.

     readln() reads a whole line, with timeout, and with optional unlocking
     of a held mutex while polling the socket. On success, it reads a whole
     null-terminated line, including the newline delimiter, into buf; it
     rewrites line \r\n to \n; and it resets *time to 0.
     It returns the length read, including the final null.
     readln() provides whole lines only; it returns -1 on timeout, end
     of file, or buffer full, even if a partial line would be available.
  */
  int read(int timeout, char *buf, int len) const;
  int readln(int timeout, int *time, char *buf, int len, NdbMutex *) const;
  int write(int timeout, int *, const char *buf, int len) const;

  /* Do the socket have pending data to be read immediately? */
  bool has_pending() const;

  int shutdown() const;
  int close();
  void close_with_reset();

  /* ndb_socket_poller.h */
  int poll_readable(int timeout_msec) const;
  int poll_writable(int timeout_msec) const;
  bool check_hup() const;

 private:
  void invalidate_socket_handle();

  bool ssl_has_pending() const;
  ssize_t ssl_recv(char *buf, size_t len) const;
  ssize_t ssl_peek(char *buf, size_t len) const;
  ssize_t ssl_send(const char *buf, size_t len) const;
  ssize_t ssl_writev(const struct iovec *vec, int nvec) const;

  int ssl_read(int timeout, char *buf, int len) const;
  int ssl_readln(int timeout, int *, char *buf, int len, NdbMutex *) const;
  int ssl_write(int timeout, int *, const char *buf, int len) const;

  bool ssl_handshake();
  int ssl_shutdown() const;
  void ssl_close();

  friend class TlsLineReader;

 private:
  /**
   * SSL functions required a mutex as its libraries are not multithread safe.
   * The mutex is created by associate(), when a ssl object is assigned to the
   * NdbSocket. As the mutex is also required for shutdown() and close(),
   * it is only released by the destructor, or the move c'tors when we
   * move into an NdbSocket already having a mutex assigned.
   */
  NdbMutex *mutex{nullptr};  // Protects ssl
  struct ssl_st *ssl{nullptr};
  ndb_socket_t s{INVALID_SOCKET};
};

/*
 * There must not be any concurrent operations on the source object (oth) while
 * calling the move constructor.
 */
inline NdbSocket::NdbSocket(NdbSocket &&oth) {
  assert(ssl == nullptr);
  assert(!ndb_socket_valid(s));

  // All members are default member initialized.
  // Using swap for move.
  // Source (oth) will become like default initialized.
  std::swap(ssl, oth.ssl);
  std::swap(mutex, oth.mutex);
  std::swap(s, oth.s);
}

/*
 * There must not be any concurrent operations on either the source object
 * (oth) or destination object (this) while calling the move assignment.
 */
inline NdbSocket &NdbSocket::operator=(NdbSocket &&oth) {
  // Only allow move assignment to default NdbSocket object
  assert(ssl == nullptr);
  assert(!ndb_socket_valid(s));

  // Using swap for move.
  // Source (oth) will become like default initialized.
  std::swap(ssl, oth.ssl);
  std::swap(mutex, oth.mutex);
  std::swap(s, oth.s);
  return *this;
}

inline void NdbSocket::invalidate_socket_handle() {
  // call close() before invalidate_socket_handle()
  assert(ssl == nullptr);
  ndb_socket_invalidate(&s);
}

inline socket_t NdbSocket::release_native_socket() {
  assert(ssl == nullptr);
  socket_t sock = s.s;
  invalidate_socket_handle();
  return sock;
}

inline std::string NdbSocket::to_string() const {
  std::string str = ndb_socket_to_string(s);
  if (ssl) str += " [ssl]";
  return str;
}

/* ndb_socket.h */
inline ssize_t NdbSocket::recv(char *buf, size_t len, int peek) const {
  if (ssl) {
    assert(!peek);  // could be supported but is not expected
    return ssl_recv(buf, len);
  }
  return ndb_recv(s, buf, len, peek);
}

inline ssize_t NdbSocket::send(const char *buf, size_t len) const {
  if (ssl) return ssl_send(buf, len);
  return ndb_send(s, buf, len, 0);
}

inline ssize_t NdbSocket::writev(const struct iovec *vec, int nvec) const {
  if (ssl) return ssl_writev(vec, nvec);
  return ndb_socket_writev(s, vec, nvec);
}

inline int NdbSocket::shutdown() const {
  if (ssl) ssl_shutdown();
  int r = ndb_socket_shutdown_both(s);
  return r;
}

inline int NdbSocket::close() {
  assert(is_valid());
  if (ssl) ssl_close();
  int r = ndb_socket_close(s);
  invalidate_socket_handle();
  return r;
}

inline void NdbSocket::close_with_reset() {
  if (ssl) ssl_close();
  constexpr bool with_reset = true;
  ndb_socket_close_with_reset(s, with_reset);
  invalidate_socket_handle();
}

inline bool NdbSocket::has_pending() const {
  if (ssl != nullptr) return ssl_has_pending();
  return false;
}

/* ndb_socket_poller.h */
inline int NdbSocket::poll_readable(int timeout) const {
  if (!is_valid()) return -1;
  if (has_pending()) return 1;
  ndb_socket_poller poller;
  poller.add_readable(s);
  return poller.poll(timeout);
}

inline int NdbSocket::poll_writable(int timeout) const {
  if (!is_valid()) return -1;
  ndb_socket_poller poller;
  poller.add_writable(s);
  return poller.poll(timeout);
}

inline bool NdbSocket::check_hup() const {
  if (!is_valid()) return true;
  ndb_socket_poller poller;
  poller.add_readable(s);
  if (poller.poll_unsafe(0) > 0 && poller.has_hup(0)) return true;
  return false;
}

#endif
