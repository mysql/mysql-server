/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "openssl/ssl.h"
#include "openssl/err.h"

#include "debugger/EventLogger.hpp"
#include "portlib/NdbTick.h"

#include "util/require.h"
#include "util/NdbSocket.h"
#include "util/socket_io.h"

#include "portlib/ndb_openssl_version.h"

static constexpr bool openssl_version_ok =
  ((OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL) ||
   (OPENSSL_VERSION_NUMBER == UBUNTU18_OPENSSL_VER_ID));

/* Utility Functions */

#ifdef NDEBUG
#define Debug_Log(...)
#else
#define Debug_Log(...) g_eventLogger->debug(__VA_ARGS__)
#endif

static inline
SSL * new_ssl(SSL_CTX * ctx) {
  if constexpr(! openssl_version_ok) {
    g_eventLogger->error("NDB TLS: OpenSSL version '%s' is not supported",
                         OPENSSL_VERSION_TEXT);
    return nullptr;
  }
  if(! ctx) return nullptr;
  return SSL_new(ctx);
}

/* Class Methods */

SSL * NdbSocket::get_client_ssl(SSL_CTX *ctx) {
  SSL * ssl = new_ssl(ctx);
  if(ssl) SSL_set_connect_state(ssl);       // client
  return ssl;
}

SSL * NdbSocket::get_server_ssl(SSL_CTX *ctx) {
  SSL * ssl = new_ssl(ctx);
  if(ssl) SSL_set_accept_state(ssl);       // server
  return ssl;
}

void NdbSocket::free_ssl(SSL *ssl) {
  if(ssl) SSL_free(ssl);
}

/* NdbSocket public instance methods */
int NdbSocket::readln(int timeout_msec, int *time, char *buf,
                      int len, NdbMutex * mutex) const {
  if(ssl) return ssl_readln(timeout_msec, time, buf, len, mutex);
  return readln_socket(s, timeout_msec, time, buf, len, mutex);
}

int NdbSocket::read(int timeout_msec, char * buf, int len) const {
  if(ssl) return ssl_read(timeout_msec, buf, len);
  return read_socket(s, timeout_msec, buf, len);
}

int NdbSocket::write(int timeout_msec, int *time,
                     const char *buf, int len) const {
  if(ssl) return ssl_write(timeout_msec, time, buf, len);
  return write_socket(s, timeout_msec, time, buf, len);
}

// associate fd with SSL
bool NdbSocket::associate(SSL * new_ssl)
{
  if(ssl) return false; // already associated
  if(new_ssl == nullptr) return false;
  if(! SSL_set_fd(new_ssl, s.s)) return false;
  socket_table_set_ssl(s.s, new_ssl);
  ssl = new_ssl;
  return true;
}

X509 * NdbSocket::peer_certificate() const {
  if(! ssl) return nullptr;
  return SSL_get_peer_certificate(ssl);
}

int NdbSocket::set_nonblocking(int on) {
  if(ssl) {
    if(on) {
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

bool NdbSocket::enable_locking() {
  if(! mutex) mutex = NdbMutex_Create();
  return (bool) mutex;
}

bool NdbSocket::disable_locking() {
  int r = mutex ? NdbMutex_Destroy(mutex) : 0;
  mutex = nullptr;
  return (r == 0);
}

/* NdbSocket private instance methods */

// ssl_close
void NdbSocket::ssl_close() {
  Guard2 guard(mutex);     // acquire mutex if non-null
  set_nonblocking(false);  // set to blocking
  SSL_shutdown(ssl);       // wait for close
  socket_table_clear_ssl(s.s);
  SSL_free(ssl);
  ssl = nullptr;
}

#if OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL

static void log_ssl_error(const char * fn_name)
{
  char buffer[512];
  int code;
  while((code = ERR_get_error()) != 0) {
    ERR_error_string_n(code, buffer, sizeof(buffer));
    g_eventLogger->error("NDB TLS %s: %s", fn_name, buffer);
  }
}

bool NdbSocket::ssl_handshake() {
  /* Check for non-blocking socket (see set_nonblocking): */
  if(SSL_get_mode(ssl) & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER) return false;
  assert(SSL_get_mode(ssl) & SSL_MODE_AUTO_RETRY);

  int r = SSL_do_handshake(ssl);
  if(r == 1) return true;

  int err = SSL_get_error(ssl, r);
  require(err != SSL_ERROR_WANT_READ); // always use blocking I/O for handshake
  require(err != SSL_ERROR_WANT_WRITE);
  const char * desc = SSL_is_server(ssl) ?
    "handshake failed in server" : "handshake failed in client";

  log_ssl_error(desc);
  close();
  invalidate();
  return false;
}

/* This is only used by read & write routines */
static ssize_t handle_ssl_error(int err, const char * fn) {
  switch(err) {
    case SSL_ERROR_NONE:
      assert(false);      // we should not be here on success
      return 0;
    case SSL_ERROR_SSL:
      log_ssl_error(fn); // OpenSSL knows more about the error
      return 0;           // caller should close the socket
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return TLS_BUSY_TRY_AGAIN;
    case SSL_ERROR_SYSCALL:
      return -1;          // caller should check errno and close the socket
    case SSL_ERROR_ZERO_RETURN:
      return 0;           // the peer has closed the SSL transport
    default:              // unexpected code
      log_ssl_error(fn);
      assert(false);
      return -1;
  }
}

bool NdbSocket::update_keys(bool req_peer) const {
  if(ssl && (SSL_version(ssl) == TLS1_3_VERSION)) {
    Guard2 guard(mutex); // acquire mutex if non-null
    int flag = req_peer ? SSL_KEY_UPDATE_REQUESTED
                        : SSL_KEY_UPDATE_NOT_REQUESTED;
    return SSL_key_update(ssl, flag);
  }
  return false;
}

bool NdbSocket::renegotiate() {
  if(ssl && (SSL_version(ssl) != TLS1_3_VERSION))
    return SSL_renegotiate(ssl);
  return false;
}

bool NdbSocket::key_update_pending() const {
  if(!ssl) return false;
  if(SSL_version(ssl) == TLS1_3_VERSION)
    return (SSL_get_key_update_type(ssl) != SSL_KEY_UPDATE_NONE);
  return (bool) SSL_renegotiate_pending(ssl);
}

ssize_t NdbSocket::ssl_recv(char *buf, size_t len) const
{
  bool r;
  size_t nread = 0;
  {
    Guard2 guard(mutex); // acquire mutex if non-null
    r = SSL_read_ex(ssl, buf, len, &nread);
  }

  if(r) return nread;
  int err = SSL_get_error(ssl, r);
  Debug_Log("SSL_read(%zd): ERR %d", len, err);
  return handle_ssl_error(err, "SSL_read");
}

ssize_t NdbSocket::ssl_peek(char *buf, size_t len) const
{
  bool r;
  size_t nread = 0;
  {
    Guard2 guard(mutex); // acquire mutex if non-null
    r = SSL_peek_ex(ssl, buf, len, &nread);
  }

  if(r) return nread;
  int err = SSL_get_error(ssl, r);
  Debug_Log("SSL_peek(%zd): ERR %d", len, err);
  return handle_ssl_error(err, "SSL_peek");
}

ssize_t NdbSocket::ssl_send(const char * buf, size_t len) const
{
  size_t nwrite = 0;
  int err = 0;

  /* Locked section */
  {
    Guard2 guard(mutex);
    if(unlikely(ssl == nullptr)) return -1; // conn. closed by another thread
    if(SSL_write_ex(ssl, buf, len, &nwrite))
      return nwrite;
    err = SSL_get_error(ssl, 0);
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
#endif

/*
 * writev()
 */

/* consolidate() returns the number of consecutive iovec send buffers
   that can be combined and sent together with total size < a 16KB TLS record.

   MaxTlsRecord is set to some small amount less than 16KB.

   MaxSingleBuffer is set to some size point where a record is so large that
   consolidation is not worth the cost of the in-memory copy required.
   12KB here is just a guess.
*/
static constexpr const int MaxTlsRecord = 16000;
static constexpr size_t MaxSingleBuffer = 12 * 1024;

int NdbSocket::consolidate(const struct iovec *vec,
                           const int nvec) const
{
  int n = 0;
  size_t total = 0;
  for(int i = 0; i < nvec ; i++) {
    size_t len = vec[i].iov_len;
    if(len > MaxSingleBuffer) break;
    total += len;
    if(total > MaxTlsRecord) break;
    n++;
  }
  if(n == 0) n = 1;
  return n;
}

ssize_t NdbSocket::ssl_writev(const struct iovec *vec, int nvec) const
{
  while(nvec > 0 && vec->iov_len == 0) {
    vec++;
    nvec--;
  }

  ssize_t total = 0;
  while(nvec > 0) {
    int sent;
    int n = consolidate(vec, nvec);
    if(n > 1) {
      sent = send_several_iov(vec, n);
    } else {
      sent = ssl_send((const char *) vec[0].iov_base, vec[0].iov_len);
    }

    if(sent > 0) {
      vec += n;
      nvec -= n;
      total += sent;
    } else if(total > 0) {
      break;  // return total bytes sent prior to error
    } else {
      return sent; // no data has been sent; return error code
    }
  }
  return total;
}

int NdbSocket::send_several_iov(const struct iovec * vec, int n) const {
  size_t len = 0;
  char buff[MaxTlsRecord];

  for(int i = 0 ; i < n ; i++) {
    const struct iovec & v = vec[i];
    memcpy(buff + len, v.iov_base, v.iov_len);
    len += v.iov_len;
    assert(len <= MaxTlsRecord);
  }

  return ssl_send(buff, len);
}


/* Functions for socket_io.cpp
   Used in InputStream / OutputStream
*/

namespace {
class Timer {
  int * m_elapsed;
  const NDB_TICKS m_start;

public:
  explicit Timer(int * elapsed) :
    m_elapsed(elapsed), m_start(NdbTick_getCurrentTicks())  {}

  ~Timer() {  // record the elapsed time when the Timer goes out of scope
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    *m_elapsed = *m_elapsed + NdbTick_Elapsed(m_start,now).milliSec();
  }
};
}

/* Read with timeout
*/
int NdbSocket::ssl_read(int timeout, char *buf, int len) const {
  if(len < 1) return 0;

  int res;
  do {
    int elapsed = 0;
    {
      Timer t(& elapsed);
      res = poll_readable(timeout);
    }
    if(res > 0) {
      timeout -= elapsed;
      res = ssl_recv(buf, len);
      if(res >= 0) return res;
    }
  } while(timeout > 0 && res == TLS_BUSY_TRY_AGAIN);

  return 0; // timed out
}

class TlsLineReader {
  const NdbSocket & m_socket;
  NdbMutex * m_held_mutex;
  char * m_buf;
  int m_buf_len;
  int m_bytes_read {0};
  bool m_complete {false};
  bool m_error {false};

public:
  TlsLineReader(const NdbSocket &s, char * buf, int buf_len, NdbMutex * m) :
    m_socket(s), m_held_mutex(m), m_buf(buf), m_buf_len(buf_len) {}
  void read(int timeout, int * elapsed);
  bool error()    { return m_error; }    // true on timeout, eof, buffer full
  bool complete() { return m_complete; } // true if a complete line is available
  int length()    { return m_bytes_read; } // length read
};

/* Unlock a mutex on entry, then lock it at end of scope. */
class UnlockGuard {
  NdbMutex * m_mtx;
public:
  UnlockGuard(NdbMutex *m) : m_mtx(m) { if(m_mtx) NdbMutex_Unlock(m_mtx); }
  ~UnlockGuard()                      { if(m_mtx) NdbMutex_Lock(m_mtx); }
};

/*
  Read to newline, with timeout. Optionally unlock and relock a held mutex.
  Return a null-terminated whole line, including the newline character.
  Rewrite \r\n to \n. Reset *elapsed on success.
 */
int NdbSocket::ssl_readln(int timeout, int * elapsed,
                          char * buf, int len, NdbMutex * heldMutex) const {
  assert(*elapsed == 0);
  if(len < 1) return 0; // no room in the buffer for the content

  /* Initial poll, with unlocking of mutex. */
  int result;
  {
    UnlockGuard guard(heldMutex);
    Timer t(elapsed);
    result = poll_readable(timeout);
  }
  if(result <= 0) return -1;

  /* Read until a complete line is available, eof, or timeout */
  TlsLineReader reader(*this, buf, len, heldMutex);

  do {
    reader.read(timeout, elapsed);

    if(reader.complete()) {
      *elapsed = 0;
      Debug_Log("ssl_readln => %d", reader.length());
      return reader.length();
    }

  } while(! (reader.error() || (*elapsed >= timeout)));

  Debug_Log("ssl_readln => -1 [ELAPSED: %d]", *elapsed);
  return -1;
}

void TlsLineReader::read(int timeout, int * elapsed) {
  int peek_len = 0;
  {
    UnlockGuard guard(m_held_mutex);
    peek_len = m_socket.ssl_peek(m_buf, m_buf_len - 1);
  }

  while((peek_len == TLS_BUSY_TRY_AGAIN) && (*elapsed < timeout)) {
    UnlockGuard guard(m_held_mutex);
    Timer pollTimer(elapsed);
    m_socket.poll_readable(timeout - *elapsed);
    peek_len = m_socket.ssl_peek(m_buf, m_buf_len - 1);
  }

  m_error = (peek_len < 1);
  if(m_error) return;

  /* Find the first newline */
  char * ptr = m_buf;
  int t;
  int i = 0;
  for(t = peek_len; i < t; i++) {
    if(ptr[i] == '\n') {
      m_complete = true;
      i++;
      break;
    }
  }

  /* Consume characters from the buffer */
  for (int len = i; len; ) {
    t = m_socket.ssl_recv(ptr, len);
    m_error = (t < 1);
    if(m_error) return;

    ptr += t;
    len -= t;
  }

  /* If a complete line was read, return */
  if(m_complete) {
    assert(ptr[-1] == '\n');

    /* rewrite line endings */
    if (t > 1 && ptr[-2] == '\r') {
      ptr[-2] = '\n';
      ptr--;
    }

    /* append null terminator */
    ptr[0]= '\0';
    m_bytes_read += (int)(ptr - m_buf);
    return;
  }

  /* A partial line has been read */
  m_bytes_read += i;   // record the length so far
  m_buf = ptr;         // reset past the partial line
  m_buf_len -= i;
  m_error = (m_buf_len < 1);  // buffer full
}


int NdbSocket::ssl_write(int timeout, int *time,
                         const char * buf, int len) const
{
  {
    Timer t(time);
    if(poll_writable(timeout) != 1) return -1;
  }

#ifndef NDEBUG
  /* Assert that this is a blocking socket; see set_nonblocking() above. */
  long ssl_mode = SSL_get_mode(ssl);
  assert(! ( ssl_mode & SSL_MODE_ENABLE_PARTIAL_WRITE));
  assert(! ( ssl_mode & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER));
#endif

  ssize_t r = ssl_send(buf, len);
  Debug_Log("NdbSocket::ssl_write(%d) => %zd", len, r);

  assert(r != TLS_BUSY_TRY_AGAIN);

  return r < 0 ? r : 0;
}
