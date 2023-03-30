/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_UTIL_SECURE_SOCKET_H
#define NDB_UTIL_SECURE_SOCKET_H

#include "portlib/ndb_socket.h"
#include "portlib/ndb_socket_poller.h"
#include "portlib/NdbMutex.h"
#include "util/ssl_socket_table.h"

static constexpr int TLS_BUSY_TRY_AGAIN = -2;

class NdbSocket {
public:
  enum class From { New, Existing };

  NdbSocket() = default;
  NdbSocket(ndb_socket_t ndbsocket, From fromType) {
    ssl = socket_table_get_ssl(ndbsocket.s, (fromType == From::Existing));
    init_from_native(ndbsocket.s);
  }
  ~NdbSocket()                       { disable_locking(); }

 /* The standard copy constructor and copy operator are private.
  * NdbSockets should be copied using NdbSocket::transfer(), which
  * invalidates the original, and transfers ownership of its ssl and
  * mutex.
  *
  * NdbSocket::copy() should only be used when the original is going
  * out of scope.
  */
  static void transfer(NdbSocket & newSocket, NdbSocket & original);
  static NdbSocket transfer(NdbSocket & original);
  static NdbSocket copy(const NdbSocket &s) { return s; }

  void init_from_new(ndb_socket_t);
  void init_from_native(socket_t fd) { ndb_socket_init_from_native(s, fd); }
  void invalidate();
  int is_valid() const               { return ndb_socket_valid(s); }
  bool has_tls() const               { return ssl; }
  ndb_socket_t ndb_socket() const    { return s; }
  socket_t native_socket() const     { return ndb_socket_get_native(s); }
  std::string to_string() const;

  /* Get an SSL for client-side TLS.
   * Returns pointer to SSL. Returns null if CTX is null, or SSL_new() fails,
   * or if the OpenSSL version is not supported for NDB TLS.
   * When ready to switch over, call associate().
   */
  static struct ssl_st * get_client_ssl(struct ssl_ctx_st *);

  /* Get an SSL for for server-side TLS.
   * Returns pointer to SSL. Returns null if CTX is null, or SSL_new() fails,
   * or if the OpenSSL version is not supported for NDB TLS.
   * When ready to switch over, call associate().
   */
  static struct ssl_st * get_server_ssl(struct ssl_ctx_st *);

  /* Free an SSL returned by get_client_ssl() or get_server_ssl(), in the case
   * when an error has prevented it from being associated with a socket.
   */
  static void free_ssl(struct ssl_st *);

  /* Associate a socket with an SSL.
   * Returns true on success.
   * Returns false if socket already has an SSL association,
   *               or if SSL is null,
   *               or on failure from the SSL socket table,
   *               or on failure from SSL_set_fd().
   */
  bool associate(struct ssl_st *);

  /* Enable or disable mutex locking around SSL read and write calls.
   * Return true on success.
   */
  bool enable_locking();
  bool disable_locking();

  /* Run TLS handshake.
   * This must be done synchronously on a blocking socket.
   *
   * Returns:
   *    true on success.
   *    true if the socket does not have TLS enabled.
   *    false on failure; the socket has been invalidated and closed.
   *    false if socket is non-blocking (based on SSL mode flags)
   */
  bool do_tls_handshake()            { return ssl ? ssl_handshake() : true; }

  /* Update keys (TLS 1.3).
   * If request_peer_update is set to true, request the peer to also update.
   * Returns true on success. If TLS 1.3 is not in use, returns false.
   */
  bool update_keys(bool request_peer_update=false) const;

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

  /* Get peer's TLS certificate
   *
   * Returns nullptr if socket does not have TLS enabled.
   */
  struct x509_st * peer_certificate() const;

  /* Set socket behavior to blocking or non-blocking.
     For SSL sockets, this should be done after associate().
     This call also sets the appropriate TLS segment size and SSL mode flags
     for the socket.
      * blocking SSL sockets have mode flag SSL_MODE_AUTO_RETRY.
      * non-blocking SSL sockets have mode flags SSL_MODE_ENABLE_PARTIAL_WRITE
        and SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER.
     See SSL_set_mode(3) for reference on flags.
  */
  int set_nonblocking(int on);

  /* ndb_socket.h */
  ssize_t recv(char * buf, size_t len, int peek = 0) const;
  ssize_t send(const char * buf, size_t len) const;
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
  int write(int timeout, int *, const char * buf, int len) const;

  /* For SSL sockets, close() removes the SSL from the SSL Socket Table */
  int close();
  void close_with_reset(bool with_reset);

  /* ndb_socket_poller.h */
  uint add_readable(ndb_socket_poller *) const;
  uint add_writable(ndb_socket_poller *) const;
  int poll_readable(int timeout_msec) const;
  int poll_writable(int timeout_msec) const;
  bool check_hup() const;

private:
  NdbSocket & operator= (const NdbSocket &) = default;
  NdbSocket(const NdbSocket &) = default;

  ssize_t ssl_recv(char * buf, size_t len) const;
  ssize_t ssl_peek(char * buf, size_t len) const;
  ssize_t ssl_send(const char * buf, size_t len) const;
  ssize_t ssl_writev(const struct iovec *vec, int nvec) const;

  int ssl_read(int timeout, char *buf, int len) const;
  int ssl_readln(int timeout, int *, char * buf, int len, NdbMutex *) const;
  int ssl_write(int timeout, int *, const char * buf, int len) const;

  bool ssl_handshake();
  void ssl_close();

  int consolidate(const struct iovec *, const int) const;
  int send_several_iov(const struct iovec *, int) const;

  friend class TlsLineReader;

private:
  struct ssl_st * ssl{nullptr};
  NdbMutex * mutex{nullptr};
  ndb_socket_t s{INVALID_SOCKET};
};

inline
void NdbSocket::init_from_new(ndb_socket_t ndbsocket) {
  assert(! socket_table_get_ssl(ndbsocket.s, false));
  init_from_native(ndbsocket.s);
}

inline
void NdbSocket::transfer(NdbSocket & newSocket, NdbSocket & original) {
  assert(! newSocket.is_valid());
  newSocket = original;           // invokes the private copy operator
  original.ssl = nullptr;         // transfer ownership
  original.mutex = nullptr;       // transfer ownership
  original.invalidate();
}

inline
NdbSocket NdbSocket::transfer(NdbSocket & original) {
  NdbSocket newSocket;
  transfer(newSocket, original);
  return newSocket;               // invokes the private copy constructor
}

inline
void NdbSocket::invalidate() {
  assert(ssl == nullptr);         // call close() before invalidate()
  assert(mutex == nullptr);       // call close() before invalidate()
  ndb_socket_invalidate(&s);
}

inline
std::string NdbSocket::to_string() const {
  std::string str = ndb_socket_to_string(s);
  if(ssl) str += " [ssl]";
  return str;
}

/* ndb_socket.h */
inline
ssize_t NdbSocket::recv(char * buf, size_t len, int peek) const {
  if(ssl) {
    assert(!peek);  // could be supported but is not expected
    return ssl_recv(buf, len);
  }
  return ndb_recv(s, buf, len, peek);
}

inline
ssize_t NdbSocket::send(const char * buf, size_t len) const {
  if(ssl) return ssl_send(buf, len);
  return ndb_send(s, buf, len, 0);
}

inline
ssize_t NdbSocket::writev(const struct iovec *vec, int nvec) const {
  if(ssl) return ssl_writev(vec, nvec);
  return ndb_socket_writev(s, vec, nvec);
}

inline
int NdbSocket::close() {
  if(ssl) ssl_close();
  disable_locking();
  return ndb_socket_close(s);
}

inline
void NdbSocket::close_with_reset(bool with_reset) {
  if(ssl) ssl_close();
  ndb_socket_close_with_reset(s, with_reset);
}

/* ndb_socket_poller.h */
inline
uint NdbSocket::add_readable(ndb_socket_poller * poller) const {
  return poller->add_readable(s, ssl);
}

inline
uint NdbSocket::add_writable(ndb_socket_poller * poller) const {
  return poller->add_writable(s);
}

inline
int NdbSocket::poll_readable(int timeout) const {
  ndb_socket_poller poller;
  poller.add_readable(s, ssl);
  return poller.poll(timeout);
}

inline
int NdbSocket::poll_writable(int timeout) const {
  ndb_socket_poller poller;
  poller.add_writable(s);
  return poller.poll(timeout);
}

inline
bool NdbSocket::check_hup() const {
  ndb_socket_poller poller;
  poller.add_readable(s);
  if(poller.poll_unsafe(0) > 0 && poller.has_hup(0))
    return true;
  return false;
}

#endif
