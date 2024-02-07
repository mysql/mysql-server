/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <utility>  // std::move

#include "openssl/err.h"
#include "openssl/ssl.h"

#include "debugger/EventLogger.hpp"
#include "portlib/NdbTick.h"
#include "portlib/ndb_openssl_version.h"
#include "util/NdbSocket.h"
#include "util/ndb_openssl3_compat.h"
#include "util/require.h"
#include "util/socket_io.h"

static constexpr bool openssl_version_ok =
    (OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL);

/* Utility Functions */

#ifdef NDEBUG
#define Debug_Log(...)
#else
#define Debug_Log(...) g_eventLogger->debug(__VA_ARGS__)
#endif

static inline SSL *new_ssl(SSL_CTX *ctx) {
  if constexpr (!openssl_version_ok) {
    g_eventLogger->error("NDB TLS: OpenSSL version '%s' is not supported",
                         OPENSSL_VERSION_TEXT);
    return nullptr;
  }
  if (!ctx) return nullptr;
  return SSL_new(ctx);
}

/* Class Methods */

SSL *NdbSocket::get_client_ssl(SSL_CTX *ctx) {
  SSL *ssl = new_ssl(ctx);
  if (ssl) SSL_set_connect_state(ssl);  // client
  return ssl;
}

SSL *NdbSocket::get_server_ssl(SSL_CTX *ctx) {
  SSL *ssl = new_ssl(ctx);
  if (ssl) SSL_set_accept_state(ssl);  // server
  return ssl;
}

void NdbSocket::free_ssl(SSL *ssl) {
  if (ssl) SSL_free(ssl);
}

/* NdbSocket public instance methods */
int NdbSocket::readln(int timeout_msec, int *time, char *buf, int len,
                      NdbMutex *mutex) const {
  if (ssl) return ssl_readln(timeout_msec, time, buf, len, mutex);
  return readln_socket(s, timeout_msec, time, buf, len, mutex);
}

int NdbSocket::read(int timeout_msec, char *buf, int len) const {
  if (ssl) return ssl_read(timeout_msec, buf, len);
  return read_socket(s, timeout_msec, buf, len);
}

int NdbSocket::write(int timeout_msec, int *time, const char *buf,
                     int len) const {
  if (ssl) return ssl_write(timeout_msec, time, buf, len);
  return write_socket(s, timeout_msec, time, buf, len);
}

// associate fd with SSL
bool NdbSocket::associate(SSL *new_ssl) {
  if (ssl) return false;  // already associated
  if (new_ssl == nullptr) return false;
  if (!SSL_set_fd(new_ssl, s.s)) return false;
  ssl = new_ssl;

  // SSL will need mutex protection:
  assert(mutex == nullptr);
  mutex = NdbMutex_Create();
  return true;
}

X509 *NdbSocket::peer_certificate() const {
  if (!ssl) return nullptr;
  return SSL_get_peer_certificate(ssl);
}

int NdbSocket::set_nonblocking(int on) const {
  if (ssl) {
    if (on) {
      SSL_clear_mode(ssl, SSL_MODE_AUTO_RETRY);
      SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
      SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    } else {
      SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
      SSL_clear_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
      SSL_clear_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    }
  }
  return ndb_socket_nonblock(s, on);
}

/* NdbSocket private instance methods */

#if OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL

static void log_ssl_error(const char *fn_name) {
  const int code = ERR_peek_last_error();
  if (ERR_GET_REASON(code) == SSL_R_UNEXPECTED_EOF_WHILE_READING) {
    /*
     * "unexpected eof while reading" are in general expected to happen now and
     * then when network or peer breaks - logging is suppressed to limit
     * harmless noise.
     */
    while (ERR_get_error() != 0) /* clear queued errors */
      ;
    return;
  }
  char buffer[512];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  g_eventLogger->error("NDB TLS %s: %s", fn_name, buffer);
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
  /* Check that there is at least one error in queue. */
  require(ERR_get_error() != 0);
#endif
  while (ERR_get_error() != 0) /* clear queued errors */
    ;
}

/* This is only used by read & write routines */
static ssize_t handle_ssl_error(int err, const char *fn) {
  switch (err) {
    case SSL_ERROR_NONE:
      assert(false);  // we should not be here on success
      return 0;
    case SSL_ERROR_SSL:
      log_ssl_error(fn);  // OpenSSL knows more about the error
      return 0;           // caller should close the socket
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      // FIXME Bug#3569340:
      //
      // Note that for upper transporter layers we expect either 0 or -1
      // to be returned in case of failures.
      return TLS_BUSY_TRY_AGAIN;  // <- return -1;
    case SSL_ERROR_SYSCALL:
      return -1;  // caller should check errno and close the socket
    case SSL_ERROR_ZERO_RETURN:
      return 0;  // the peer has closed the SSL transport
    default:     // unexpected code
      log_ssl_error(fn);
      assert(false);
      return -1;
  }
}

bool NdbSocket::ssl_handshake() {
  /* Check for non-blocking socket (see set_nonblocking): */
  if (SSL_get_mode(ssl) & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER) return false;
  assert(SSL_get_mode(ssl) & SSL_MODE_AUTO_RETRY);

  int r = SSL_do_handshake(ssl);
  if (r == 1) return true;

  int err = SSL_get_error(ssl, r);
  require(err != SSL_ERROR_WANT_READ);  // always use blocking I/O for handshake
  require(err != SSL_ERROR_WANT_WRITE);
  const char *desc = SSL_is_server(ssl) ? "handshake failed in server"
                                        : "handshake failed in client";

  handle_ssl_error(err, desc);
  close();
  return false;
}

// ssl_close
void NdbSocket::ssl_close() {
  Guard guard(mutex);
  require(ssl != nullptr);
  if (SSL_is_init_finished(ssl)) {
    const int mode = SSL_get_shutdown(ssl);
    if (!(mode & SSL_SENT_SHUTDOWN)) {
      /*
       * Do not call SSL_shutdown again if it already been called in
       * NdbSocket::shutdown. In that case it could block waiting on
       * SSL_RECEIVED_SHUTDOWN.
       */
      int r = SSL_shutdown(ssl);
      if (r < 0) {
        // Clear errors
        int err = SSL_get_error(ssl, r);
        Debug_Log("SSL_shutdown(): ERR %d", err);
        handle_ssl_error(err, "SSL_close");
      }
    }
  }
  SSL_free(ssl);
  ssl = nullptr;
}

bool NdbSocket::update_keys(bool req_peer) const {
  if (ssl && (SSL_version(ssl) == TLS1_3_VERSION)) {
    Guard guard(mutex);
    int flag =
        req_peer ? SSL_KEY_UPDATE_REQUESTED : SSL_KEY_UPDATE_NOT_REQUESTED;
    return SSL_key_update(ssl, flag);
  }
  return false;
}

bool NdbSocket::renegotiate() {
  if (ssl && (SSL_version(ssl) != TLS1_3_VERSION)) return SSL_renegotiate(ssl);
  return false;
}

bool NdbSocket::key_update_pending() const {
  if (!ssl) return false;
  if (SSL_version(ssl) == TLS1_3_VERSION)
    return (SSL_get_key_update_type(ssl) != SSL_KEY_UPDATE_NONE);
  return (bool)SSL_renegotiate_pending(ssl);
}

int NdbSocket::ssl_shutdown() const {
  int err;
  {
    Guard guard(mutex);
    require(ssl != nullptr);
    /*
     * SSL_is_init_finished may return false if either TLS handshake have not
     * finished, or an unexpected eof was seen.
     */
    if (!SSL_is_init_finished(ssl)) return 0;
    const int mode = SSL_get_shutdown(ssl);
    assert(!(mode & SSL_SENT_SHUTDOWN));
    if (unlikely(mode & SSL_SENT_SHUTDOWN)) return 0;
    const int r = SSL_shutdown(ssl);
    if (r >= 0) return 0;
    err = SSL_get_error(ssl, r);
  }
  Debug_Log("SSL_shutdown(): ERR %d", err);
  return handle_ssl_error(err, "SSL_shutdown");
}

ssize_t NdbSocket::ssl_recv(char *buf, size_t len) const {
  bool r;
  size_t nread = 0;
  int err;
  {
    if (NdbMutex_Trylock(mutex)) {
      return TLS_BUSY_TRY_AGAIN;
    }
    if (unlikely(ssl == nullptr ||
                 SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN)) {
      NdbMutex_Unlock(mutex);
      return 0;
    }
    r = SSL_read_ex(ssl, buf, len, &nread);
    if (r) {
      NdbMutex_Unlock(mutex);
      return nread;
    }
    err = SSL_get_error(ssl, r);
    NdbMutex_Unlock(mutex);
  }

  Debug_Log("SSL_read(%zd): ERR %d", len, err);
  return handle_ssl_error(err, "SSL_read");
}

ssize_t NdbSocket::ssl_peek(char *buf, size_t len) const {
  bool r;
  size_t nread = 0;
  int err;
  {
    Guard guard(mutex);
    if (unlikely(ssl == nullptr ||
                 SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN))
      return 0;

    r = SSL_peek_ex(ssl, buf, len, &nread);
    if (r) return nread;
    err = SSL_get_error(ssl, r);
  }

  Debug_Log("SSL_peek(%zd): ERR %d", len, err);
  return handle_ssl_error(err, "SSL_peek");
}

ssize_t NdbSocket::ssl_send(const char *buf, size_t len) const {
  size_t nwrite = 0;
  int err = 0;

  /* Locked section */
  {
    if (NdbMutex_Trylock(mutex)) {
      return TLS_BUSY_TRY_AGAIN;
    }
    if (unlikely(ssl == nullptr || SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN)) {
      NdbMutex_Unlock(mutex);
      return -1;
    }
    if (SSL_write_ex(ssl, buf, len, &nwrite)) {
      NdbMutex_Unlock(mutex);
      return nwrite;
    }
    err = SSL_get_error(ssl, 0);
    NdbMutex_Unlock(mutex);
  }

  require(err != SSL_ERROR_WANT_READ);
  return handle_ssl_error(err, "SSL_write");
}

#else
static constexpr ssize_t too_old = NDB_OPENSSL_TOO_OLD;
bool NdbSocket::ssl_handshake() { return false; }
bool NdbSocket::update_keys(bool) const { return false; }
bool NdbSocket::renegotiate() { return false; }
bool NdbSocket::key_update_pending() const { return false; }
ssize_t NdbSocket::ssl_recv(char *, size_t) const { return too_old; }
ssize_t NdbSocket::ssl_peek(char *, size_t) const { return too_old; }
ssize_t NdbSocket::ssl_send(const char *, size_t) const { return too_old; }
void NdbSocket::ssl_close() {}
int NdbSocket::ssl_shutdown() const { return -1; }
#endif

/*
 * writev()
 */

/* MaxTlsRecord is set to some small amount less than 16KB.

   MaxSingleBuffer is set to some size point where a record is so large that
   it is not worth the cost of the in-memory copy to 'pack' it.
   12KB here is just a guess.
*/
static constexpr const int MaxTlsRecord = 16000;
static constexpr size_t MaxSingleBuffer = 12 * 1024;

static_assert(MaxSingleBuffer <= MaxTlsRecord);

ssize_t NdbSocket::ssl_writev(const struct iovec *vec, int nvec) const {
  if (unlikely(nvec <= 0)) return 0;

  if (nvec == 1 || vec[0].iov_len > MaxSingleBuffer) {
    return ssl_send((const char *)vec[0].iov_base, vec[0].iov_len);
  } else {
    // Pack iovec's into buff[], first iovec will always fit.
    char buff[MaxTlsRecord];
    size_t buff_len = vec[0].iov_len;
    memcpy(buff, vec[0].iov_base, buff_len);

    // Pack more iovec's into remaining buff[] space
    for (int i = 1; i < nvec && buff_len < MaxTlsRecord; i++) {
      const struct iovec &v = vec[i];
      size_t cpy_len = v.iov_len;
      if (buff_len + cpy_len > MaxTlsRecord) {
        // Do a partial copy of last iovec[]
        cpy_len = MaxTlsRecord - buff_len;
      }
      memcpy(buff + buff_len, v.iov_base, cpy_len);
      buff_len += cpy_len;
    }
    return ssl_send(buff, buff_len);
  }
}

/* Functions for socket_io.cpp
   Used in InputStream / OutputStream
*/

namespace {
class Timer {
  int *m_elapsed;
  const NDB_TICKS m_start;

 public:
  explicit Timer(int *elapsed)
      : m_elapsed(elapsed), m_start(NdbTick_getCurrentTicks()) {}

  ~Timer() {  // record the elapsed time when the Timer goes out of scope
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    *m_elapsed = *m_elapsed + NdbTick_Elapsed(m_start, now).milliSec();
  }
};
}  // namespace

bool NdbSocket::ssl_has_pending() const {
  Guard guard(mutex);
  if (unlikely(ssl == nullptr)) return false;
  return SSL_pending(ssl);
}

/* Read with timeout
 */
int NdbSocket::ssl_read(int timeout, char *buf, int len) const {
  if (len < 1) return 0;

  int res;
  do {
    int elapsed = 0;
    {
      Timer t(&elapsed);
      res = poll_readable(timeout);
    }
    if (res > 0) {
      timeout -= elapsed;
      res = ssl_recv(buf, len);
      if (res >= 0) return res;
    }
  } while (timeout > 0 && res == TLS_BUSY_TRY_AGAIN);

  return 0;  // timed out
}

class TlsLineReader {
  const NdbSocket &m_socket;
  NdbMutex *m_held_mutex;
  char *m_buf;
  int m_buf_len;
  int m_bytes_read{0};
  bool m_complete{false};
  bool m_error{false};

 public:
  TlsLineReader(const NdbSocket &s, char *buf, int buf_len, NdbMutex *m)
      : m_socket(s), m_held_mutex(m), m_buf(buf), m_buf_len(buf_len) {}
  void read(int timeout, int *elapsed);
  bool error() { return m_error; }  // true on timeout, eof, buffer full
  bool complete() {
    return m_complete;
  }                                      // true if a complete line is available
  int length() { return m_bytes_read; }  // length read
};

/* Unlock a mutex on entry, then lock it at end of scope. */
class UnlockGuard {
  NdbMutex *m_mtx;

 public:
  UnlockGuard(NdbMutex *m) : m_mtx(m) {
    if (m_mtx) NdbMutex_Unlock(m_mtx);
  }
  ~UnlockGuard() {
    if (m_mtx) NdbMutex_Lock(m_mtx);
  }
};

/*
  Read to newline, with timeout. Optionally unlock and relock a held mutex.
  Return a null-terminated whole line, including the newline character.
  Rewrite \r\n to \n. Reset *elapsed on success.
 */
int NdbSocket::ssl_readln(int timeout, int *elapsed, char *buf, int len,
                          NdbMutex *heldMutex) const {
  assert(*elapsed == 0);
  if (len < 1) return 0;  // no room in the buffer for the content

  /* Initial poll, with unlocking of mutex. */
  int result;
  {
    UnlockGuard guard(heldMutex);
    Timer t(elapsed);
    result = poll_readable(timeout);
  }
  if (result == 0) return 0;  // timeout
  if (result < 0) return -1;

  /* Read until a complete line is available, eof, or timeout */
  TlsLineReader reader(*this, buf, len, heldMutex);

  do {
    reader.read(timeout, elapsed);

    if (reader.complete()) {
      *elapsed = 0;
      Debug_Log("ssl_readln => %d", reader.length());
      return reader.length();
    }

  } while (!(reader.error() || (*elapsed >= timeout)));

  Debug_Log("ssl_readln => -1 [ELAPSED: %d]", *elapsed);
  if (*elapsed >= timeout) return 0;
  return -1;
}

void TlsLineReader::read(int timeout, int *elapsed) {
  int peek_len = 0;
  {
    UnlockGuard guard(m_held_mutex);
    peek_len = m_socket.ssl_peek(m_buf, m_buf_len - 1);
  }

  while ((peek_len == TLS_BUSY_TRY_AGAIN) && (*elapsed < timeout)) {
    UnlockGuard guard(m_held_mutex);
    Timer pollTimer(elapsed);
    m_socket.poll_readable(timeout - *elapsed);
    peek_len = m_socket.ssl_peek(m_buf, m_buf_len - 1);
  }

  m_error = (peek_len < 1);
  if (m_error) return;

  /* Find the first newline */
  char *ptr = m_buf;
  int t;
  int i = 0;
  for (t = peek_len; i < t; i++) {
    if (ptr[i] == '\n') {
      m_complete = true;
      i++;
      break;
    }
  }

  /* Consume characters from the buffer */
  for (int len = i; len;) {
    t = m_socket.ssl_recv(ptr, len);
    m_error = (t < 1);
    if (m_error) return;

    ptr += t;
    len -= t;
  }

  /* If a complete line was read, return */
  if (m_complete) {
    assert(ptr[-1] == '\n');

    /* rewrite line endings */
    if (t > 1 && ptr[-2] == '\r') {
      ptr[-2] = '\n';
      ptr--;
    }

    /* append null terminator */
    ptr[0] = '\0';
    m_bytes_read += (int)(ptr - m_buf);
    return;
  }

  /* A partial line has been read */
  m_bytes_read += i;  // record the length so far
  m_buf = ptr;        // reset past the partial line
  m_buf_len -= i;
  m_error = (m_buf_len < 1);  // buffer full
}

int NdbSocket::ssl_write(int timeout, int *time, const char *buf,
                         int len) const {
  {
    Timer t(time);
    if (poll_writable(timeout) != 1) return -1;
  }

#ifndef NDEBUG
  /* Assert that this is a blocking socket; see set_nonblocking() above. */
  long ssl_mode = SSL_get_mode(ssl);
  assert(!(ssl_mode & SSL_MODE_ENABLE_PARTIAL_WRITE));
  assert(!(ssl_mode & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER));
#endif

  ssize_t r = ssl_send(buf, len);
  Debug_Log("NdbSocket::ssl_write(%d) => %zd", len, r);

  assert(r != TLS_BUSY_TRY_AGAIN);

  return r < 0 ? r : 0;
}
