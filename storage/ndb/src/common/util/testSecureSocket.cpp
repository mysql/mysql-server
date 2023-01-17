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
#include <assert.h>
#include <stdio.h>
#include <vector>

#include "openssl/err.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"

#include "debugger/EventLogger.hpp"
#include "portlib/NdbGetRUsage.h"
#include "portlib/NdbMutex.h"
#include "portlib/NdbSleep.h"
#include "portlib/NdbTick.h"
#include "unittest/mytap/tap.h"
#include "util/ndb_openssl3_compat.h"
#include "util/ndb_opts.h"
#include "util/require.h"
#include "util/NdbSocket.h"
#include "util/SocketClient.hpp"
#include "util/SocketServer.hpp"

/* This can be run with no args as a TAP test. It will start echo servers on
   ports 3400 and 3401 and run the first 12 tests.

   With the -s option it will run the TCP and TLS echo servers.

   With the -c option and server hostname it will run the test client.

   With -s and --ack, run a sink service that sends back acknowledgement
   messages, rather than an echo server. A "-s --ack" server is compatible
   with a "-c --start=9" client.

   Tests 1 through 5 are send/recv tests. One thread reads data from a file
   and sends it to the echo server; the other thread reads the reply. These
   tests measure the total time required to receive a fixed amount of data.

   The default data source file is the testSecureSocket-t executable file
   itself. I have also used /dev/zero and /dev/random (which may impose a
   significant performance cost). Some tests will read the source file more
   than once if it is not as long as the data they require.

   Tests 6 and 7 are readline() tests. To run these, use --source to specify
   a text input file. With --dest=outfile, you can compare the output to the
   input.

   Test 8 attempts to perform a TLS 1.3 key update or TLS 1.2 renegotiation
   mid-transfer. The TLS 1.2 version can be run by supplying the --tls12 option,
   but is not supported and should crash.

   Tests 9 and 10 are send tests; these disregard what is received and measure
   just the time required to send all the data. Use -z to vary the per-send
   block size and -m to vary the total test size. Tests 11 and 12 are send tests
   that use writev(). Use -v to see the exact iovec buffer composition. Tests
   13 and 14 use writev() in a way that simulates many large send buffers.

   The two-thread design is based on the threaded echo server and test client
   from Richard Stevens "Unix Network Programming," 2nd ed., chapter 23.
*/

/* EL6 and EL7 have OpenSSL 1.0.2 (FIPS); this test will not compile */
#if OPENSSL_VERSION_NUMBER < 0x10003000L
int main() {
  printf("%s\n", OPENSSL_VERSION_TEXT);
  ok(1, "OpenSSL too old. Not testing.");
  return 0;
}
#else

#define POLL_TIMEOUT -3

static constexpr const int NetTimeoutMsec = 500;

int opt_buff_size = 8192;
int opt_port = 3400;
int opt_test_number = 0;
int opt_start_test_number = 0;
int opt_end_test_number = 100;
int opt_timeout = NetTimeoutMsec;
int opt_send_MB = 10;
int opt_tcp_no_delay = 1;
bool opt_server = false;
bool opt_sink = false;
bool opt_skip_warmup = false;
bool opt_tls12 = false;
bool opt_list = false;
int opt_verbose = 0;
const char * opt_remote_host = nullptr;
const char * opt_data_source = nullptr; // set in main()
const char * opt_data_dest = nullptr;

static struct my_option options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  { "client", 'c', "run test client: arg is remote server name or address",
    & opt_remote_host, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "list", 'l', "list client tests and exit",
    & opt_list, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "mb", 'm', "MB of data for client to send per test",
    & opt_send_MB, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_send_MB, 0, 0, nullptr, 0, nullptr },
  { "port", 'p', "server base port number (echo on p, TLS echo on p+1)",
    & opt_port, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_port, 0, 0, nullptr, 0, nullptr },
  { "server", 's', "run server",
    & opt_server, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "source", NDB_OPT_NOSHORT, "source of data for SendRecv tests",
    & opt_data_source, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "dest", NDB_OPT_NOSHORT, "file where received data will be written",
    & opt_data_dest, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "ack", NDB_OPT_NOSHORT, "server: do not echo, just send acknowledgements",
    & opt_sink, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "no-delay", NDB_OPT_NOSHORT, "value of TCP_NODELAY on client sockets",
    & opt_tcp_no_delay, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_tcp_no_delay, 0, 1, nullptr, 0, nullptr },
  { "test", 't', "run client test #n",
    & opt_test_number, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "start", NDB_OPT_NOSHORT, "start at test #n",
    & opt_start_test_number, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "stop", NDB_OPT_NOSHORT, "stop after test #n",
    & opt_end_test_number, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_end_test_number, 0, 0, nullptr, 0, nullptr },
  { "tls12", NDB_OPT_NOSHORT, "force client TLS version 1.2",
    & opt_tls12, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "timeout", NDB_OPT_NOSHORT, "client socket poll timeout in msec.",
    & opt_timeout, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_timeout, 0, 0, nullptr, 0, nullptr },
  { "buffer-size", 'z', "client network buffer size",
    & opt_buff_size, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    opt_buff_size, 0, 0, nullptr, 0, nullptr },
  { "skip-warmup", NDB_OPT_NOSHORT, "skip warmup",
    & opt_skip_warmup, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "verbose", 'v', "print more messages",
    & opt_verbose, nullptr, nullptr, GET_INT, OPT_ARG,
    0, 0, 4, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

/*** Server ****/

class EchoSession : public SocketServer::Session {
public:
  EchoSession(ndb_socket_t s, bool sink, SSL_CTX * ctx) :
    SocketServer::Session(s),
    m_sink(sink),
    m_ssl_ctx(ctx),
    m_secure_socket(s, NdbSocket::From::New) {}
  void runSession() override;
private:
  bool m_sink;
  SSL_CTX * m_ssl_ctx;
  NdbSocket m_secure_socket;
};

class PlainService : public SocketServer::Service {
public:
  PlainService(bool sink) : m_sink(sink) {}
  EchoSession * newSession(ndb_socket_t s) override {
    return new EchoSession(s, m_sink, nullptr);
  }
private:
  const bool m_sink;
};

class TlsService : public SocketServer::Service {
public:
  TlsService(bool sink);
  EchoSession * newSession(ndb_socket_t s) override {
    return new EchoSession(s, m_sink, m_ssl_ctx);
  }
  static int on_ssl_verify(int, X509_STORE_CTX *);

private:
  SSL_CTX * m_ssl_ctx;
  const bool m_sink;
};

TlsService::TlsService(bool sink) : m_sink(sink) {
  int r;
  static constexpr const char * cipher_list =
    "TLS_CHACHA20_POLY1305_SHA256:"
    "TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256:"
    "ECDHE-ECDSA-AES128-GCM-SHA256";
  static constexpr const char * common_name = "Test Certificate";

  /* Create a key and certificate */
  EVP_PKEY * tls_key = EVP_EC_generate("P-256");
  X509 * tls_cert = X509_new();
  X509_set_version(tls_cert, 2);
  X509_set_pubkey(tls_cert, tls_key);

  /* Set the names */
  X509_NAME * name = X509_get_subject_name(tls_cert);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (const unsigned char *) common_name, -1, -1, 0);
  X509_set_issuer_name(tls_cert, X509_get_subject_name(tls_cert));

  /* Set the expiration date */
  X509_gmtime_adj(X509_getm_notBefore(tls_cert), 0);
  X509_gmtime_adj(X509_getm_notAfter(tls_cert), 30 * 24 * 60 * 60);

  /* Sign the key (self-signed) */
  r = X509_sign(tls_cert, tls_key, EVP_sha256());
  assert(r);

  /* Get an SSL_CTX */
  m_ssl_ctx = SSL_CTX_new(TLS_method());
  assert(m_ssl_ctx);

  /* Set the active key and certificate in the context */
  r = SSL_CTX_use_certificate(m_ssl_ctx, tls_cert);
  assert(r);
  r = SSL_CTX_use_PrivateKey(m_ssl_ctx, tls_key);
  assert(r);

  /* Set the cipher list */
  r = SSL_CTX_set_cipher_list(m_ssl_ctx, cipher_list);
  require(r);
}

int TlsService::on_ssl_verify(int r, X509_STORE_CTX *) {
  assert(r);
  return r;
}

void EchoSession::runSession() {
  static constexpr int echo_buffer_size = 32 * 1024;
  char buffer[echo_buffer_size];
  char message[128];
  size_t total = 0;
  ssize_t n;

  if(m_ssl_ctx) {
    SSL * ssl = NdbSocket::get_server_ssl(m_ssl_ctx);
    if(! m_secure_socket.associate(ssl)) {
      NdbSocket::free_ssl(ssl);
      return;
    }
    if(! m_secure_socket.do_tls_handshake()) {
      return;
    }
  }

  while(! m_stop) {
    if( (n = m_secure_socket.read(50, buffer, echo_buffer_size)) < 0) {
      return;
    }
    total += n;
    if(m_sink && n) { /* Send acknowledgement */
      int sz = sprintf(message,"Sink ack: %zd\n", total);
      m_secure_socket.send(message, sz);
    }
    else {      /* Echo data back to client */
      m_secure_socket.send(buffer, n);
    }
  }
}


/*** Client ****/

/* Class Client simply manages two connections (plain and TLS)
*/
class Client : public SocketClient {
public:
  Client(const char * hostname);
  ~Client()                              {  SSL_CTX_free(m_ssl_ctx); }
  NdbSocket & connect_plain();
  NdbSocket & connect_tls();
  void disconnect();

private:
  SSL_CTX * m_ssl_ctx;
  const char * m_server_host;
  NdbSocket m_tls_socket;
  NdbSocket m_plain_socket;
};

Client::Client(const char * hostname) : SocketClient(nullptr),
                                        m_server_host(hostname)
{
  m_ssl_ctx = SSL_CTX_new(TLS_method());
  if(opt_tls12) SSL_CTX_set_max_proto_version(m_ssl_ctx, TLS1_2_VERSION);
}

NdbSocket & Client::connect_plain() {
  connect(m_plain_socket, m_server_host, opt_port);
  ndb_setsockopt(m_plain_socket.ndb_socket(), IPPROTO_TCP, TCP_NODELAY,
                 & opt_tcp_no_delay);
  return m_plain_socket;
}

NdbSocket & Client::connect_tls() {
  connect(m_tls_socket, m_server_host, opt_port + 1);

  if(! m_tls_socket.is_valid())
    puts("Could not connect to server");
  else {
    SSL * ssl = NdbSocket::get_client_ssl(m_ssl_ctx);
    if(m_tls_socket.associate(ssl)) {
      if(m_tls_socket.do_tls_handshake()) {
        return m_tls_socket;  // success
      }
    }
    else NdbSocket::free_ssl(ssl);
    puts("TLS connection failed.");
    m_tls_socket.invalidate();
  }
  ndb_setsockopt(m_tls_socket.ndb_socket(), IPPROTO_TCP, TCP_NODELAY,
                 & opt_tcp_no_delay);
  return m_tls_socket;
}

void Client::disconnect() {
  m_plain_socket.close();
  m_tls_socket.close();
}


/* Class ClientTest provides time-keeping and some thread scaffolding;
   functional tests are implemented in derived classes
*/
class ClientTest {
public:
  ClientTest(NdbSocket & s) : m_socket(s), m_timeout(opt_timeout)  { }

  virtual ~ClientTest() { }

  int run(int n);                        // n is ordinal in list of all tests
  bool verbose() const              { return opt_verbose > m_verbose_level; }
  void rusage12() const;
  void rusage13() const;

  /* Interface for derived test classes */
  virtual void printTestName(int n) = 0; // n is ordinal in list of all tests
  virtual void setup() { }
  virtual int runTest() = 0;             // returns 0 on success, or error code
  virtual int testSend() = 0;            // returns 0 on success, or error code
  virtual int testRecv() = 0;            // returns 0 on success, or error code
  virtual void printTestResult() {
    Uint64 elapsed_msec = NdbTick_Elapsed(t1, t2).milliSec();
    printf("elapsed msec.: %lld\n", elapsed_msec);
  }

protected:
  void runTestSend();
  void runTestRecv();
  friend void * runClientSendThread(void *);
  friend void * runClientRecvThread(void *);

  NDB_TICKS t1, t2, t3;
  int sendStatus, recvStatus;
  struct ndb_rusage ru1, ru2, ru3;
  NdbSocket & m_socket;
  const int m_timeout;
  int m_verbose_level{1};
};

void * runClientSendThread(void * p) {
  ClientTest * t = (ClientTest *) p;
  t->runTestSend();
  my_thread_exit(& t->sendStatus);
}

void * runClientRecvThread(void * p) {
  ClientTest * t = (ClientTest *) p;
  t->runTestRecv();
  my_thread_exit(& t->recvStatus);
}

void ClientTest::runTestSend() {
  t1 = NdbTick_getCurrentTicks();
  Ndb_GetRUsage(& ru1, false);
  sendStatus = testSend();
  Ndb_GetRUsage(& ru3, false);
  t3 = NdbTick_getCurrentTicks();
}

void ClientTest::runTestRecv() {
  recvStatus = testRecv();
  Ndb_GetRUsage(& ru2, false);
  t2 = NdbTick_getCurrentTicks();
}

int ClientTest::run(int n) {
  printTestName(n);
  fflush(stdout);

  /* Test-specific setup */
  setup();

  /* Test-specific run */
  int r = runTest();

  /* Report the result */
  printTestResult();
  return r;
}

void ClientTest::rusage12() const {
  printf("CPU user: %lld, system: %lld usec\n",
         ru2.ru_utime - ru1.ru_utime, ru2.ru_stime - ru1.ru_stime);
}

void ClientTest::rusage13() const {
  printf("CPU user: %lld, system: %lld usec\n",
         ru3.ru_utime - ru1.ru_utime, ru3.ru_stime - ru1.ru_stime);
}



/* Class SendRecvTest tests low-level NdbSocket send() and recv() calls,
   with both blocking and non-blocking sockets, with or without mutex locking.
*/
class SendRecvTest : public ClientTest {
public:
  SendRecvTest(NdbSocket & s, const char * name,
               bool blocking, bool locking, int buff_size = opt_buff_size) :
    ClientTest(s),  m_name(name), m_test_bytes(opt_send_MB * 1000000),
    m_buff_size(buff_size), m_block(blocking), m_locking(locking)
  {
    assert(! (blocking && locking));  // can result in deadlock
    m_send_buffer = (char *) malloc(m_buff_size);
    m_recv_buffer = (char *) malloc(m_buff_size);
    int d = m_test_bytes / m_buff_size;
    if(m_test_bytes % m_buff_size)
      m_test_bytes = (d+1) * m_buff_size; // round up
  }

  ~SendRecvTest() override
  {
    free(m_send_buffer);
    free(m_recv_buffer);
  }

protected:
  void assert_sent_received() {
    if(m_bytes_sent != m_bytes_received)
      BAIL_OUT("sent %llu != received %llu\n", m_bytes_sent, m_bytes_received);
  }

  int send(size_t sent, size_t total);
  int retry_send(size_t len);
  int recv(size_t len);
  bool writeOutputFile(FILE *, size_t) const;

  void setup() override;
  int runTest() override;
  int testSend() override;
  int testRecv() override;
  void printTestName(int) override;
  void printTestResult() override;

  const char * m_name;
  char * m_send_buffer, * m_recv_buffer;
  Uint64 m_test_bytes;
  Uint64 m_bytes_sent {0};
  Uint64 m_bytes_received {0};
  int m_buff_size;
  int m_update_keys {0};
  bool m_block;
  bool m_locking;
  bool m_repeat_input {true};
};

void SendRecvTest::setup() {
  m_socket.set_nonblocking(! m_block);
  if(m_locking) m_socket.enable_locking();
  else m_socket.disable_locking();
}

void SendRecvTest::printTestName(int n) {
  printf("(t%d) %s %s %s ", n, m_name,
         m_block   ? "  blocking  " : "non-blocking",
         m_locking ? "(w/ mutex)"   : "(no mutex)");
}

int SendRecvTest::runTest() {
  my_thread_handle sendThd;

  /* Start the send thread (start time = t1) */
  my_thread_create(& sendThd, nullptr, runClientSendThread, this);

  /* Receive in this thread (end time = t2) */
  runTestRecv();
  if(recvStatus != 0)
    my_thread_cancel(& sendThd);

  /* Wait for the send thread */
  my_thread_join(& sendThd, nullptr);

  return recvStatus;
}

void SendRecvTest::printTestResult() {
  assert_sent_received();
  Uint64 msec = NdbTick_Elapsed(t1, t2).milliSec();
  printf("received %llu bytes %llu msec\n", m_bytes_received, msec);
}

inline int try_again(int r) {
  return (    (r == TLS_BUSY_TRY_AGAIN)
           || (r == POLL_TIMEOUT)
           || (errno == EAGAIN));
}

inline int error_message(const char * prefix, int code) {
  if(code == POLL_TIMEOUT)
    printf("%s error: poll timeout\n", prefix);
  else
    printf("%s error: %d [%d] [%s]\n", prefix, code, errno, strerror(errno));
  return code;
}

/*
 * Sending
 */
int SendRecvTest::send(size_t sent, size_t len) {
  if(m_block || m_socket.poll_writable(m_timeout))
    return m_socket.send(m_send_buffer + sent, len - sent);
  return POLL_TIMEOUT;
}

int SendRecvTest::retry_send(size_t ndata) {
  size_t nsent = 0;

  while(nsent < ndata) {
    if(m_update_keys == 2) {
      printf("[size %d] UPDATING KEYS ", m_buff_size);
      bool ok = m_socket.update_keys();
      require(ok);
      m_update_keys = 0;
    }

    int r = send(nsent, ndata);
    if(verbose()) printf("SEND    .. %d .. %llu \n", r,
                         m_bytes_sent + nsent + (r > 0 ? r : 0));
    if(r > 0) nsent += r;
    else if (! try_again(r))
      return error_message("send()", r);
  }
  return nsent;
}

int SendRecvTest::testSend() {
  FILE * infp = fopen(opt_data_source, "r");
  if(infp == nullptr) return errno;
  int r = 0;

  do {
    m_bytes_sent += r;
    r = 0;
    Uint64 remaining = m_test_bytes - m_bytes_sent;
    int to_send = m_buff_size;
    if(remaining < (Uint64) to_send) to_send = (int) remaining;
    if((m_update_keys == 1) && (remaining < m_bytes_sent))
      m_update_keys = 2; // trigger update now
    if(to_send > 0) {
      int len = fread(m_send_buffer, 1, to_send, infp); // read from input
      if(len < 1 && feof(infp) && m_repeat_input) {
        fseek(infp, 0, SEEK_SET);
        len = fread(m_send_buffer, 1, to_send, infp);
      }
      if(len > 0) r = retry_send(len);
      else if(len == 0) r = 0;  // End of file
      else printf("fread() error: [%d] %d \n", len, ferror(infp));
    }
  } while(r > 0);

  fclose(infp);
  assert(! m_socket.key_update_pending());
  return (r > 0) ? 0 : r;
}


/*
 * Receiving
 */
int SendRecvTest::recv(size_t len) {
  if(m_block || m_socket.poll_readable(m_timeout))
    return m_socket.recv(m_recv_buffer, len);
  return POLL_TIMEOUT;
}

bool SendRecvTest::writeOutputFile(FILE * outfp, size_t r) const {
  if(outfp) {
    if(fwrite(m_recv_buffer, 1, r, outfp) != r) {
      printf("Error writing destination file: %s\n", strerror(errno));
      fclose(outfp);
      return false;
    }
  }
  return true;
}

int SendRecvTest::testRecv() {
  FILE * outfp = opt_data_dest ? fopen(opt_data_dest, "w") : nullptr;
  int r = 0;
  bool one_timeout = false;
  while(m_bytes_received < m_test_bytes) {
    do {
      r = recv(m_buff_size);
      if(verbose()) printf("                    RECV    .. %d .. %llu \n",
                           r, m_bytes_received + (r > 0 ? r : 0));
      bool this_timeout = (r == POLL_TIMEOUT);
      if(one_timeout && this_timeout) break; // two consecutive timeouts
      one_timeout = this_timeout;
    } while(try_again(r));
    if(r < 1) break;
    if(! writeOutputFile(outfp, r)) return -1;
    m_bytes_received += r;
  }

  if (outfp) fclose(outfp);
  return (r < 0) ? error_message("recv()", r) : 0;
}


/* Class SendTest measures just time spent in sending.
 * The receive thread runs only until it receives a timeout.
 * The remote end can be echo an server or data sink.
 */
class SendTest : public SendRecvTest {
public:
  SendTest(NdbSocket & s, const char * name, bool locking,
           size_t buff = opt_buff_size) :
    SendRecvTest(s, name, false, locking, buff)               {}

  void printTestName(int n) override {
    printf("(t%d) SendTest: %s ", n, m_name);
  }
  int testRecv() final;
  int runTest() final;
  void printTestResult() override;
};

int SendTest::runTest() {
  my_thread_handle recvThd;

  /* Start the receive thread */
  my_thread_create(& recvThd, nullptr, runClientRecvThread, this);

  /* Send from this thread; start time is t1, end time is t3 */
  runTestSend();
  if(sendStatus != 0)
    my_thread_cancel(& recvThd);

  /* Wait for the receive thread to time out */
  my_thread_join(& recvThd, nullptr);

  return sendStatus;
}

int SendTest::testRecv() {
  while(1) {
    int r = recv(m_buff_size);
    if(verbose()) printf("                    RECV    .. %d \n", r);
    if(r < 1 && r != TLS_BUSY_TRY_AGAIN) return r;
  }
}

void SendTest::printTestResult() {
  Uint64 msec = NdbTick_Elapsed(t1, t3).milliSec();
  printf("sent %llu bytes %llu msec [Buf: %d] ",
         m_bytes_sent, msec, m_buff_size);
  rusage13();
}

/* Class WarmupTest
*/
class WarmupTest : public SendRecvTest {
public:
  WarmupTest(NdbSocket &s) :
    SendRecvTest(s, "", false, true, 4096)
  {
    m_test_bytes = 2000000;
    m_verbose_level = 2;
  }

  void printTestName(int n) override
  {
    printf("Warm up %s connection: ", n ? "TLS" : "plain");
  }

  void printTestResult() override {
    puts("complete.");
  }
};

/* Class ReadLineTest
*/
class ReadLineTest : public SendRecvTest {
public:
  ReadLineTest(NdbSocket & s, const char * name) :
    SendRecvTest(s, name, false, true)
  {
    m_repeat_input = false; // don't read the file more than once
    m_test_bytes = 1000000; // don't send more than this
  }

  int testRecv() final;
  void printTestResult() final;

private:
  int m_lines_received {0};
};

int ReadLineTest::testRecv() {
  FILE * outfp = opt_data_dest ? fopen(opt_data_dest, "w") : nullptr;
  if(opt_data_dest && ! outfp) return error_message("Destination file", -1);
  int r = 0;
  int elapsed_time;
  while(1) {
    elapsed_time = 0;
    r = m_socket.readln(m_timeout, & elapsed_time,
                        m_recv_buffer, m_buff_size, nullptr);
    if(elapsed_time >= m_timeout) break;
    if(r == -1) continue; // buffer full, no line found
    assert(r > 0);
    assert(m_recv_buffer[r] == '\0');
    assert(m_recv_buffer[r-1] == '\n');
    if(! writeOutputFile(outfp, r)) return -1;
    m_lines_received++;
  };

  if (outfp) fclose(outfp);

  if(r > 0 || elapsed_time >= m_timeout) return 0;
  return r;
}

void ReadLineTest::printTestResult() {
  Uint64 elapsed = NdbTick_Elapsed(t1, t2).milliSec();
  printf("%d lines in %lld msec\n", m_lines_received, elapsed);
}

/* Class IovList
*/
class IovList {
public:
  IovList() : nbuf(0) {}
  IovList(const IovList &) = default;
  void set_count(int n)                                           { nbuf = n; }
  int count() const                                            { return nbuf; }
  struct iovec & iov(int n)                              { return m_iovec[n]; }
  void free_all()     { for(int n = 0; n < nbuf ; n++) free(iov(n).iov_base); }
  int writev(const NdbSocket &, int);
  int adjust(size_t);

  static constexpr int MaxBuffers = 8;
private:
  int nbuf;
  struct iovec m_iovec[MaxBuffers];
};

int IovList::writev(const NdbSocket &s, int timeout) {
  if(s.poll_writable(timeout))
    return s.writev(m_iovec, nbuf);
  return POLL_TIMEOUT;
}

int IovList::adjust(size_t x) {
  for(int n = 0; n < nbuf ; n++) {
    size_t a = m_iovec[n].iov_len;
    if(x > a) {
      m_iovec[n].iov_len = 0;
      x -= a;
    } else {
      m_iovec[n].iov_base = ((char *) m_iovec[n].iov_base + x);
      m_iovec[n].iov_len -= x;
      return n;
    }
  }
  assert(false); // only called after a partial send
  return -1;
}


/* Class WritevTest
*/
class WritevTest : public SendTest {
public:
  WritevTest(NdbSocket &, const char *, bool, size_t buff, std::vector<int>
             dist = { 25, 60, 250, 600, 2500, 6000, 25000, -1 });
 ~WritevTest() override  { m_iov.free_all(); }

  int testSend() override;
  int retry_writev();

private:
  std::vector<int> m_buffer_dist;
  IovList m_iov;
};

WritevTest::WritevTest(NdbSocket & s, const char * name,
                       bool locking, size_t buff, std::vector<int> dist) :
  SendTest(s, name, locking, buff), m_buffer_dist(dist)
{
  size_t size = m_buff_size;
  int n = 0;
  for(n = 0 ; n < IovList::MaxBuffers ; n++) m_iov.iov(n).iov_len = 0;
  for(n = 0 ; size ; n++) {
    int vec_size = m_buffer_dist[n];
    if((vec_size > (int) size) || (vec_size == -1)) vec_size = size;
    if(vec_size) m_iov.iov(n).iov_base = malloc(vec_size);
    m_iov.iov(n).iov_len = vec_size;
    size -= vec_size;
  }
  assert(size == 0);
  if(opt_verbose)
    printf("WRITEV %d buffers: %lu %lu %lu %lu %lu %lu %lu %lu\n", n,
           m_iov.iov(0).iov_len, m_iov.iov(1).iov_len,m_iov.iov(2).iov_len,
           m_iov.iov(3).iov_len, m_iov.iov(4).iov_len, m_iov.iov(5).iov_len,
           m_iov.iov(6).iov_len, m_iov.iov(7).iov_len);
  m_iov.set_count(n);
}

int WritevTest::retry_writev() {
  IovList iov(m_iov);
  int nsent = 0;
  while(1) {
    int r = iov.writev(m_socket, m_timeout);
    if(r > 0) nsent += r;
    if(verbose())
      printf("WRITEV  .. %d .. %llu \n", r, m_bytes_sent + nsent);
    if(nsent == m_buff_size) break;  // all sent
    assert(nsent < m_buff_size);
    if(r > 0) iov.adjust(r);  // partially sent
    else if( ! try_again(r))
      return error_message("writev()", r);
  }
  return nsent;
}

int WritevTest::testSend() {
  FILE * infp = fopen(opt_data_source, "r");
  int sent = 0;

  while(1) {
    m_bytes_sent += sent;
    if(m_bytes_sent >= m_test_bytes) break;

    /* Read data from source into buffers */
    for(int i = 0; i < m_iov.count() ; i++) {
      size_t len = m_iov.iov(i).iov_len;
      len = fread(m_iov.iov(i).iov_base, 1, len, infp);
    }

    /* Write to socket */
    sent = retry_writev();
    if((sent == 0) || (sent == -1) || feof(infp)) break;
  }

  fclose(infp);
  return 0;
}

/* Class BigWritevTest
*/
class BigWritevTest : public WritevTest {
public:
  BigWritevTest(NdbSocket & s, const char * name, bool locking) :
    WritevTest(s, name, locking, 262144,
               { 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768 }) {}
};

/* Class KeyUpdateTest
*/
class KeyUpdateTest : public SendRecvTest {
public:
  KeyUpdateTest(NdbSocket &s, const char * name) :
    SendRecvTest(s, name, false, true)
  {
    m_update_keys = 1;
  }
};


/* Client */
/* Returns true if tests were run */
int run_client(const char * server_host) {
  if(! server_host) {
    ok(false, "server hostname on command line");
    return -10;
  }

  Client client(server_host);

  NdbSocket & plain_socket = client.connect_plain();
  bool r = plain_socket.is_valid();
  ok(r, "client connection to plain port %d on server %s",
     opt_port, server_host);
  if(!r ) return -11;

  NdbSocket & tls_socket = client.connect_tls();
  r = tls_socket.is_valid();
  ok(r, "client connection to  TLS  port %d on server %s",
     opt_port + 1, server_host);
  if(!r ) return -12;

  printf("Client reading data from %s\n", opt_data_source);

  /* Test definitions.
     It is best to run all SendRecv tests before any Send tests; otherwise a
     SendRecv test might receive spurious data from an earlier test.
   */
  std::vector<ClientTest *> tests =
  {
    new SendRecvTest(plain_socket, "plain", true,  false),
    new SendRecvTest(tls_socket,   " TLS ", true,  false),
    new SendRecvTest(plain_socket, "plain", false, false),
    new SendRecvTest(plain_socket, "plain", false, true),
    new SendRecvTest(tls_socket,   " TLS ", false, true),

    new ReadLineTest(plain_socket, "plain readline"),
    new ReadLineTest(tls_socket,   " TLS  readline"),

    new KeyUpdateTest(tls_socket,
                      opt_tls12 ? "TLS 1.2 renegotiate" : "TLS 1.3 key update"),

    new SendTest(plain_socket, " plain basic", false),
    new SendTest(tls_socket, " TLS  basic", true),

    new WritevTest(plain_socket,   "plain writev", false, opt_buff_size),
    new WritevTest(tls_socket,     " TLS  writev", true, opt_buff_size),
    new BigWritevTest(plain_socket, "plain big writev", false),
    new BigWritevTest(tls_socket,   " TLS  big writev", true)
  };

  /* Print list of tests and exit */
  if(opt_list) {
    for(int t = 1 ; t <= (int) tests.size(); t++) {
      tests[t-1]->printTestName(t);
      printf("\n");
    }
    return 0;
  }

  /* "Warm up" each socket past TCP's slow start phase */
  if(! opt_skip_warmup) {
    if(WarmupTest(plain_socket).run(0))
      return -13;
    if(WarmupTest(tls_socket).run(1))
      return -14;
    puts("");
  }

  if(opt_test_number)
    opt_start_test_number = opt_end_test_number = opt_test_number;

  int rft = 0;   // result of final test
  for(int t = 1 ; t <= (int) tests.size(); t++)
    if(t >= opt_start_test_number && t <= opt_end_test_number)
      rft = tests[t-1]->run(t);

  for(ClientTest * t : tests) delete t;

  client.disconnect();
  return rft;
}


/* Server */
void run_server(bool standalone = false) {
  SocketServer server;
  PlainService * s1 = new PlainService(opt_sink);
  TlsService * s2 = new TlsService(opt_sink);
  unsigned short port = opt_port;
  const char * srvType = opt_sink ? "sink" : "echo";

  server.setup(s1, &port);
  printf("Plain %s server running on port %d\n", srvType, port);

  port++;
  server.setup(s2, &port);
  printf("  TLS %s server running on port %d\n", srvType, port);

  NdbThread * thd = server.startServer();

  if(standalone) {
    int r = run_client("localhost");
    ok((r == 0), "client tests (%d)", r);
    server.stopServer();
    server.stopSessions(true, 100);
    return;
  }

  NdbThread_WaitFor(thd, nullptr);
}

#ifdef _WIN32
void platform_specific(char * exePath, char **) {
  GetModuleFileNameA(nullptr, exePath, PATH_MAX);
}
#else

void sigpipe_handler(int) {
  printf("\n SIGPIPE received \n");
  exit(-1);
}

void platform_specific(char * exePath, char ** argv) {
  signal(SIGPIPE, sigpipe_handler);
  char * source = getenv("_");
  if(source == nullptr) source = argv[0];
  require(source);
  require(strlen(source) < PATH_MAX);
  strncpy(exePath, source, PATH_MAX);
}
#endif

/* Main */
int main(int argc, char** argv) {
  char exePath[PATH_MAX];
  NDB_INIT("testSecureSocket-t");
  Ndb_opts opts(argc, argv, options);

  printf("%s\n", OPENSSL_VERSION_TEXT);

  /* This executable program file is also the default data source */
  platform_specific(exePath, argv);
  opt_data_source = exePath;

  int r = opts.handle_options();
  ok(!r, "options ok");
  if(r) return r;

  g_eventLogger->createConsoleHandler();
  if(opt_verbose > 3)
    g_eventLogger->enable(Logger::LL_DEBUG);

  if(opt_server) run_server();
  else if(opt_remote_host)
    return run_client(opt_remote_host);
  else {
    /* stand-alone mode: run server and client both */

    if(! (opt_start_test_number || opt_test_number))
      opt_end_test_number = 12;

    run_server(true);
  }

  return 0;
}

#endif  /* OPENSSL_VERSION_NUMBER < x */

