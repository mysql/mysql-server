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
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <cstdarg>

#include "debugger/EventLogger.hpp"
#include "ndb_init.h"
#include "portlib/NdbDir.hpp"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_compiler.h"
#include "util/File.hpp"
#include "util/NdbOut.hpp"
#include "util/NodeCertificate.hpp"
#include "util/SocketClient.hpp"
#include "util/SocketServer.hpp"
#include "util/TlsKeyManager.hpp"
#include "util/ndb_openssl3_compat.h"
#include "util/ndb_opts.h"
#include "util/require.h"

int opt_port = 4400;
int opt_last_test = INT16_MAX;
const char *opt_cert_test_host = "www.kth.se";
bool opt_cert_test = true;

static struct my_option options[] = {
    NdbStdOpt::help,
    {"port", 'p', "server port number", &opt_port, nullptr, nullptr, GET_INT,
     REQUIRED_ARG, opt_port, 0, 0, nullptr, 0, nullptr},
    {"to", 'n', "run tests up to test number n", &opt_last_test, nullptr,
     nullptr, GET_INT, REQUIRED_ARG, opt_last_test, 0, 0, nullptr, 0, nullptr},
    {"cert-test", NDB_OPT_NOSHORT,
     "Run certificate test; use --skip-cert-test to skip", &opt_cert_test,
     nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"cert-test-hostname", NDB_OPT_NOSHORT,
     "hostname with a stable set of addresses for testing bound certificates",
     &opt_cert_test_host, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    NdbStdOpt::end_of_options};

/* This is a reimplementation of TAP that supports opt_last_test */
static struct {
  short run{0};
  short failed{0};
} globalTestInfo;

static short tests_run() { return globalTestInfo.run; }
static short tests_failed() { return globalTestInfo.failed; }
static void plan() { printf("1..%d\n", tests_run()); }
static int exit_status() {
  plan();
  return tests_failed();
}

static void emit(bool p, const char *dir, const char *fmt, std::va_list ap)
    ATTRIBUTE_FORMAT(printf, 3, 0);
static void ok(bool p, const char *fmt, ...) ATTRIBUTE_FORMAT(printf, 2, 3);
static void skip(int how_many, const char *fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);

void emit(bool p, const char *dir, const char *fmt, std::va_list ap) {
  globalTestInfo.run++;
  if (!p) globalTestInfo.failed++;
  cstrbuf<100> line;
  require(line.appendf("%s %d %s%s ", p ? "ok" : "not ok", globalTestInfo.run,
                       (dir ? "# " : "-"), (dir ? dir : "")) != -1);
  require(line.appendf(fmt, ap) != -1);
  line.replace_end_if_truncated("...");
  puts(line.c_str());
  if (globalTestInfo.run == opt_last_test) exit(exit_status());
}

void ok(bool p, const char *fmt, ...) {
  std::va_list ap;
  va_start(ap, fmt);
  emit(p, nullptr, fmt, ap);
  va_end(ap);
}

void skip(int how_many, const char *fmt, ...) {
  std::va_list ap;
  va_start(ap, fmt);
  for (int i = 0; i < how_many; i++) emit(true, "skip", fmt, ap);
  va_end(ap);
}

/* Infrastructure */

namespace Test {

class CertAuthority {
 public:
  CertAuthority(const char *ordinal = "First")
      : CA_lifetime(CertLifetime::CaDefaultDays) {
    CA_key = EVP_RSA_gen(2048);
    CA_cert = ClusterCertAuthority::create(CA_key, CA_lifetime, ordinal, false);
  }

  ~CertAuthority() {
    Certificate::free(CA_cert);
    PrivateKey::free(CA_key);
  }

  int sign(X509 *cert) {
    return ClusterCertAuthority::sign(CA_cert, CA_key, cert);
  }

  int sign(CertAuthority &leaf) { return sign(leaf.CA_cert); }

  EVP_PKEY *key() const { return CA_key; }
  X509 *cert() const { return CA_cert; }

 private:
  CertLifetime CA_lifetime;
  EVP_PKEY *CA_key{nullptr};
  X509 *CA_cert{nullptr};
};

int finish_node_cert(const Test::CertAuthority *ca, NodeCertificate *nc) {
  nc->create_keys("P-256");
  return nc->finalise(ca->cert(), ca->key());
}

class Cluster {
 public:
  Cluster() {
    for (int i = 1; i < 256; i++) nc[i] = nullptr;

    nc[1] = new NodeCertificate(Node::Type::DB, 1);
    nc[2] = new NodeCertificate(Node::Type::DB, 2);

    nc[145] = new NodeCertificate(Node::Type::MGMD, 145);
    nc[145]->bind_hostname("abel");  // MGM server bound to hostname

    nc[151] = new NodeCertificate(Node::Type::Client, 151);  // no hostname

    nc[152] = new NodeCertificate(Node::Type::Client, 152);  // two hostnames
    nc[152]->bind_hostname("baker");
    nc[152]->bind_hostname("carlo");

    nc[153] = new NodeCertificate(Node::Type::Client, 153);
    nc[153]->bind_hostname("localhost");  // bound to "localhost"

    nc[154] = new NodeCertificate(Node::Type::Client, 154);
    require(nc[154]->bind_hostname("") == false);

    nc[155] = new NodeCertificate(Node::Type::Client, 155);
    nc[155]->set_lifetime(-5, 0);  // expired 5 days ago

    nc[200] = new NodeCertificate(Node::Type::Client, 0);  // client cert
  }

  Cluster(int start, int end, int) {
    EVP_PKEY *key = PrivateKey::create("P-256");  // all nodes share a key
    for (int i = 1; i < 256; i++) {
      nc[i] = nullptr;
      if (i >= start && i <= end) {
        X509 *cert = Certificate::create(key);
        nc[i] = new NodeCertificate(Node::Type::ANY, i);
        nc[i]->set_key(key);
        nc[i]->set_cert(cert);
        Certificate::free(cert);
      }
    }
    PrivateKey::free(key);
  }

  ~Cluster() {
    for (int i = 1; i < 256; i++)
      if (nc[i]) delete nc[i];
  }

  void finish_all_certs(Test::CertAuthority *ca) {
    int r = 0;
    for (int i = 1; i < 256; i++)
      if (nc[i] && !r) r = finish_node_cert(ca, nc[i]);
    require(r == 0);
  }

  void add_finalised_cert(int i, const NodeCertificate *cnc) {
    require(i < 256);
    require(nc[i] == nullptr);
    require(cnc->is_final());
    require(cnc->is_signed());
    nc[i] = const_cast<NodeCertificate *>(cnc);
  }

  NodeCertificate *nc[256];
};

/* Network tests from client to server port on localhost.
   For a test to succeed, both sides must complete the TLS handshake, then the
   server writes one byte of application data over the connection, and the
   client reads it.
*/
class Client : public SocketClient {
 public:
  Client(Test::Cluster *ndb, int id) {
    require(SocketClient::init(AF_INET));
    require(ndb->nc[id]);
    m_keyManager.init(id, ndb->nc[id]);
    m_ssl_ctx = m_keyManager.ctx();
  }
  Client(int id, STACK_OF(X509) * certs, EVP_PKEY *key) {
    require(SocketClient::init(AF_INET));
    m_keyManager.init(id, certs, key);
    m_ssl_ctx = m_keyManager.ctx();
  }
  Client(SSL_CTX *km) : m_ssl_ctx(km) {}

  ~Client() {
    if (m_socket.is_valid()) m_socket.close();
  }
  bool connect(int port, bool expectSuccess = true);
  bool connect(ndb_sockaddr &, bool);

  TlsKeyManager m_keyManager;
  SSL_CTX *m_ssl_ctx;
  const char *required_host{nullptr};
  NdbSocket m_socket;
};

bool Client::connect(int port, bool expectOk) {
  ndb_sockaddr addr;
  require(Ndb_getAddr(&addr, "localhost") == 0);
  addr.set_port(port);
  return connect(addr, expectOk);
}

bool Client::connect(ndb_sockaddr &addr, bool expectOk) {
  m_socket = SocketClient::connect(addr);
  if (!m_socket.is_valid()) {
    if (expectOk) {
      char buffer[256];
      Ndb_inet_ntop(&addr, buffer, 256);
      printf("Failed to connect to %s:%d\n", buffer, addr.get_port());
      perror("SocketClient::connect()");
    }
    return false;
  }

  /* Run SSL handshake and expect to receive 1 byte of data from the server */
  int r = -10;
  char buf[32];
  SSL *ssl = NdbSocket::get_client_ssl(m_ssl_ctx);
  if (required_host) {
    X509_VERIFY_PARAM *vpm = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set1_host(vpm, required_host, 0);
  }
  if (m_socket.associate(ssl)) {
    if (m_socket.do_tls_handshake()) r = m_socket.recv(buf, 32);
  } else
    NdbSocket::free_ssl(ssl);

  return (r == 1);
}

class Session : public SocketServer::Session {
  class Service *m_server;
  NdbSocket m_socket;

 public:
  Session(NdbSocket &&socket, class Service *server)
      : SocketServer::Session(m_socket),
        m_server(server),
        m_socket(std::move(socket)) {}
  void runSession() override;
};

class Service : public SocketServer::Service {
 public:
  Service(const TlsKeyManager &km) : m_keyManager(km) {}
  Session *newSession(NdbSocket &&s) override {
    return new Session(std::move(s), this);
  }

  const TlsKeyManager &m_keyManager;
};

void Session::runSession() {
  SSL_CTX *ctx = m_server->m_keyManager.ctx();
  SSL *ssl = NdbSocket::get_server_ssl(ctx);
  if (m_socket.associate(ssl)) {
    if (m_socket.do_tls_handshake()) {
      ClientAuthorization *auth = nullptr;
      int authResult = TlsKeyManager::check_socket_for_auth(m_socket, &auth);
      if (auth) authResult = TlsKeyManager::perform_client_host_auth(auth);
      if (authResult == 0) m_socket.send("M", 1);
    }
  } else
    NdbSocket::free_ssl(ssl);

  if (m_socket.is_valid()) m_socket.close();
}
}  // namespace Test

/* TestTlsKeyManager combines a TlsKeyManager with a server socket,
   and can create a client socket for each authentication test.
*/

class TestTlsKeyManager : public TlsKeyManager {
  SocketServer m_server;
  Test::Cluster *m_ndb;
  NdbThread *m_server_thd{nullptr};
  ndb_sockaddr m_addr;

 public:
  TestTlsKeyManager(Test::Cluster *ndb, int server_node_id, bool start = true)
      : m_ndb(ndb), m_addr(opt_port + server_node_id) {
    init(server_node_id, m_ndb->nc[server_node_id]);
    if (start && ctx()) {
      Test::Service *service = new Test::Service(*this);
      require(m_server.setup(service, &m_addr));
      m_server_thd = m_server.startServer();
      printf("TestTlsKeyManager listening on port %d\n", m_addr.get_port());
    }
  }

  ~TestTlsKeyManager() {
    if (m_server_thd) {
      m_server.stopServer();
      m_server.stopSessions(true, 100);
    }
  }

  int test_connection_from(int id, bool x = true) {
    Test::Client client(m_ndb, id);
    return test_connection_from(client, x);
  }

  int test_connection_from(Test::Client &client, bool x = true) {
    return client.connect(m_addr.get_port(), x) ? 1 : 0;
  }

  bool test_connect_ok(int id) { return (test_connection_from(id) == 1); }

  bool test_connect_ok(Test::Client &client) {
    return (test_connection_from(client) == 1);
  }

  bool test_connect_fail(int id) {
    return (test_connection_from(id, false) == 0);
  }

  bool test_connect_fail(Test::Client &client) {
    return (test_connection_from(client, false) == 0);
  }

  void add_finalised_cert(int i, const NodeCertificate *cnc) {
    m_ndb->add_finalised_cert(i, cnc);
  }

  X509 *cert(int id) { return m_ndb->nc[id]->cert(); }
};

const NodeCertificate *get_self_signed(int id) {
  NodeCertificate *nc = new NodeCertificate(Node::Type::Client, id);
  nc->create_keys("P-256");
  int r1 = nc->self_sign();
  require(r1 == 0);
  return nc;
}

/* Tests */

void test_connecting_api(TestTlsKeyManager &t) {
  ok(t.test_connect_ok(151), "    A connection from API to DB should succeed.");
}

void test_connecting_hostname_auth_fail(TestTlsKeyManager &t) {
  ok(t.test_connect_fail(152), "    Client with bound hostname should fail.");
}

void test_connecting_unrelated(TestTlsKeyManager &t) {
  Test::CertAuthority other_ca("Other");
  other_ca.sign(other_ca);

  NodeCertificate *nc = new NodeCertificate(Node::Type::Client, 14);
  nc->create_keys("P-256");
  nc->finalise(other_ca.cert(), other_ca.key());
  t.add_finalised_cert(14, nc);

  ok(t.test_connect_fail(14),
     "    A client with an unrelated cert rejects the server cert.");
}

void test_client_no_cert(TestTlsKeyManager &t) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_method());
  Test::Client client(ctx);
  require(ctx);
  ok(t.test_connect_fail(client), "   Server rejects a client with no cert.");
  SSL_CTX_free(ctx);
}

void test_connecting_self_signed(TestTlsKeyManager &t) {
  const NodeCertificate *nc = get_self_signed(15);
  t.add_finalised_cert(15, nc);
  ok(t.test_connect_fail(15),
     "    The server rejects the client's self-signed cert.");
}

void test_expired_client_cert(TestTlsKeyManager &t) {
  int r1 = t.test_connection_from(155);
  ok((r1 == 0), "    The server rejects the client's expired cert.");
}

void test_outside_cert(TestTlsKeyManager &t, Test::CertAuthority &ca) {
  EVP_PKEY *key = PrivateKey::create("P-256");
  X509 *cert = Certificate::create(key);
  Certificate::set_expire_time(cert, 90);
  Certificate::set_common_name(cert, "NDB Node");
  ca.sign(cert);

  STACK_OF(X509) *stack = sk_X509_new_null();
  sk_X509_push(stack, cert);
  sk_X509_push(stack, ca.cert());

  Test::Client client(167, stack, key);

  ok(t.test_connect_ok(client), "    Client with new valid cert can connect");
  int r1 = TlsKeyManager::check_server_host_auth(cert, "localmime");
  ok(r1 == 0, "New valid cert passes secondary auth");

  PrivateKey::free(key);
  Certificate::free(cert);
  sk_X509_free(stack);
}

void test_outside_certs(TestTlsKeyManager &t, Test::CertAuthority &ca) {
  Test::Cluster ndb(202, 203, 3);
  ok(ndb.nc[202]->finalise(ca.cert(), ca.key()) == 0, "finalise(202)");
  ok(ndb.nc[203]->finalise(ca.cert(), ca.key()) == 0, "finalise(203)");
  Test::Client client0(&ndb, 202);
  Test::Client client1(&ndb, 203);
  ok(t.test_connect_ok(client0), "    Client 0 with valid cert can connect");
  ok(t.test_connect_ok(client1), "    Client 1 with valid cert can connect");
  int r0 = TlsKeyManager::check_server_host_auth(ndb.nc[202]->cert(), "");
  int r1 = TlsKeyManager::check_server_host_auth(ndb.nc[203]->cert(), "");
  ok(r0 == 0, "client 0 valid cert passes secondary auth");
  ok(r1 == 0, "client 1 valid cert passes secondary auth");
}

// A set of basic tests that are run in various scenarios
void run_basic_tests(TestTlsKeyManager &t) {
  test_connecting_api(t);
  test_client_no_cert(t);
  test_connecting_unrelated(t);
  test_connecting_self_signed(t);
  test_expired_client_cert(t);
  test_connecting_hostname_auth_fail(t);
}

bool test_2nd_auth(TestTlsKeyManager &t) {
  // 145 is bound to abel
  int r1 = TlsKeyManager::check_server_host_auth(t.cert(145), "abel");
  return (r1 == 0);
}

bool test_2nd_auth_bad_anon(TestTlsKeyManager &t) {
  // 152 is bound to baker and carlo
  int r1 = TlsKeyManager::check_server_host_auth(t.cert(152), "");
  return (r1 == TlsKeyError::auth2_bad_hostname);
}

bool test_2nd_auth_localhost(TestTlsKeyManager &t) {
  // 153 is bound to an empty hostname
  int r1 = TlsKeyManager::check_server_host_auth(t.cert(153), "localhost");
  int r2 = TlsKeyManager::check_server_host_auth(t.cert(153), "");
  int r3 = TlsKeyManager::check_server_host_auth(t.cert(153), "freddy");
  return ((r1 == 0) && (r2 == 0) && (r3 != 0));
}

bool test_2nd_auth_bad_hostname(TestTlsKeyManager &t) {
  // 152 is bound to baker and carlo
  int r1 = TlsKeyManager::check_server_host_auth(t.cert(152), "abel");
  return (r1 == TlsKeyError::auth2_bad_hostname);
}

bool test_2nd_auth_unbound_name(TestTlsKeyManager &t) {
  // 2 does not have a bound hostname
  int r1 =
      TlsKeyManager::check_server_host_auth(t.cert(2), "dominique.mysql.fr");
  return (r1 == 0);
}

bool test_mgmclient_to_mgmd(Test::CertAuthority &ca) {
  Test::Cluster ndb;
  ndb.finish_all_certs(&ca);
  TestTlsKeyManager t(&ndb, 145);      // MGM Server
  return t.test_connection_from(200);  // MGM Client
}

void test_primary_hostname_auth(Test::CertAuthority &ca) {
  /* Node 145 is our server. It is bound to the name "abel" */
  Test::Cluster ndb;
  ndb.finish_all_certs(&ca);
  TestTlsKeyManager t(&ndb, 145);

  {
    Test::Client c(&ndb, 151);
    ok(t.test_connect_ok(c), "No hostname checks by default");
  }

  {
    Test::Client c(&ndb, 151);
    c.required_host = "abel";
    ok(t.test_connect_ok(c), "Client checks server hostname; check succeeds");
  }

  {
    Test::Client c(&ndb, 151);
    c.required_host = "baker";
    ok(t.test_connect_fail(c), "Client checks server hostname; check fails");
  }
}

// Run basic tests with self-signed CA
void test_cluster_ca_self_signed(Test::CertAuthority &ca) {
  Test::Cluster ndb;
  ndb.finish_all_certs(&ca);
  TestTlsKeyManager t(&ndb, 1);  // server node 1
  printf("\nTests with self-signed Cluster CA:\n");
  run_basic_tests(t);
  test_outside_cert(t, ca);
  ok(test_mgmclient_to_mgmd(ca), "MGM Client connects to mgmd");
  test_outside_certs(t, ca);
}

// Run basic tests.
// In this test the cluster CA is not self-signed. Each node certificate
// requires the whole chain back to the root.
void test_cluster_ca_not_self_signed() {
  printf("\nTests with all NCs signed by intermediate CA:\n");
  Test::CertAuthority rootCa("1st");
  Test::CertAuthority intCa("2nd");
  rootCa.sign(rootCa);
  rootCa.sign(intCa);

  Test::Cluster ndb;
  ndb.finish_all_certs(&intCa);

  /* When node 1 does not have the a copy of the root cert,
     Node 151 cannot connect to it. */
  {
    TestTlsKeyManager t(&ndb, 1);
    ok(t.test_connect_fail(151), "    Cannot connect without extra cert.");
  }

  /* Give both nodes a copy of the root certificate, then re-test */
  ndb.nc[1]->push_extra_ca_cert(rootCa.cert());
  ndb.nc[151]->push_extra_ca_cert(rootCa.cert());
  {
    TestTlsKeyManager t(&ndb, 1);
    run_basic_tests(t);
  }
}

// In this test the CA cert has been rotated. The old CA signed the new one.
// Some node certs are signed with the old CA, some with the new.
void test_old_and_new_ca(Test::CertAuthority &oldCa) {
  printf("\nTests with old and new cluster CA:\n");
  Test::CertAuthority newCa("2nd");
  oldCa.sign(newCa);

  Test::Cluster ndb;

  finish_node_cert(&oldCa, ndb.nc[1]);
  finish_node_cert(&newCa, ndb.nc[2]);
  finish_node_cert(&oldCa, ndb.nc[151]);
  finish_node_cert(&newCa, ndb.nc[153]);

  require(X509_verify(ndb.nc[1]->cert(), oldCa.key()) == 1);
  require(X509_verify(ndb.nc[2]->cert(), newCa.key()) == 1);
  require(X509_verify(ndb.nc[151]->cert(), oldCa.key()) == 1);
  require(X509_verify(ndb.nc[153]->cert(), newCa.key()) == 1);

  /* Only nodes with the same CA can connect to each other */
  {
    TestTlsKeyManager t(&ndb, 1);
    ok(t.test_connect_ok(151), "    151 connecting to 1");
    ok(t.test_connect_fail(153), "    153 cannot connect to 1");
  }

  /* Nodes signed by the new CA also need the old one */
  ndb.nc[2]->push_extra_ca_cert(oldCa.cert());
  ndb.nc[153]->push_extra_ca_cert(oldCa.cert());

  TestTlsKeyManager t1(&ndb, 1);
  TestTlsKeyManager t2(&ndb, 2);

  ok(t1.test_connect_ok(2), "      2 connecting to 1");
  ok(t1.test_connect_ok(151), "    151 connecting to 1");
  ok(t1.test_connect_ok(153), "    153 connecting to 1");
  ok(t2.test_connect_ok(1), "      1 connecting to 2");
  ok(t2.test_connect_ok(151), "    151 connecting to 2");
  ok(t2.test_connect_ok(153), "    153 connecting to 2");
}

void test_secondary_auth(Test::CertAuthority &ca) {
  Test::Cluster ndb;
  ndb.finish_all_certs(&ca);
  TestTlsKeyManager t(&ndb, 1, false);

  printf("\nTest client authorization of server hostname:\n");
  ok(test_2nd_auth(t), "    Secondary auth should succeed");
  ok(test_2nd_auth_bad_anon(t), "    2nd auth should fail (bad anon)");
  ok(test_2nd_auth_bad_hostname(t), "    2nd auth should fail (bad hostname)");
  ok(test_2nd_auth_unbound_name(t),
     "    2nd auth should succeed (cert not bound to name)");
  ok(test_2nd_auth_localhost(t), "    2nd auth should succeed (localhost)");
}

static void test_iterate(TlsKeyManager &keyMgr, int expect, int last_row) {
  int node_id = 0;
  int count = 0;
  int last = 0;
  cert_table_entry row;
  while (keyMgr.iterate_cert_table(node_id, &row)) {
    count++;
    printf("Node: %3d   Expires:  %d    Name: %s \n", node_id, (int)row.expires,
           row.name);
    last = node_id;
  }
  ok((count == expect), "    Count of rows in certificate table");
  ok((last == last_row), "    Node ID of last row in certificate table");
}

void test_cert_table(Test::CertAuthority &ca) {
  Test::Cluster ndb;

  ndb.nc[255] = new NodeCertificate(Node::Type::Client, 255);
  ndb.finish_all_certs(&ca);

  printf("\nTesting certificate table:\n");

  // Table initially has 1 row for its own cert
  TestTlsKeyManager test(&ndb, 1, false);
  test_iterate(test, 1, 1);

  test.cert_table_set(2, ndb.nc[2]->cert());      // row 2
  test.cert_table_set(151, ndb.nc[151]->cert());  // row 3
  test_iterate(test, 3, 151);

  test.cert_table_set(255, ndb.nc[255]->cert());  // row 4
  test_iterate(test, 4, 255);                     // get 4 rows

  test.cert_table_clear(151);
  test.cert_table_clear(255);
  test_iterate(test, 2, 2);  // get 2 rows
}

void test_key_replace(Test::CertAuthority &ca) {
  Test::Cluster ndb;
  ndb.finish_all_certs(&ca);

  printf("\nTesting certificate replacement:\n");
  {
    TlsKeyManager km1;
    km1.init(154, ndb.nc[154]);
    bool r = km1.check_replace_date(.85F);
    ok((r == true), "    Cert 154 should not be replaced");
  }
  {
    TlsKeyManager km2;
    km2.init(155, ndb.nc[155]);
    bool r = km2.check_replace_date(.85F);
    ok((r == false), "    Cert 155 should be replaced");
  }
}

void test_affirm_client_auth(Test::CertAuthority &ca) {
  if (!opt_cert_test) {
    skip(1, "certificate authorization test");
    return;
  }
  printf("\nTest server authorization of client hostname:\n");
  auto nc = std::make_unique<NodeCertificate>(Node::Type::Client, 15);
  nc->create_keys("P-256");
  nc->bind_hostname(opt_cert_test_host);
  nc->finalise(ca.cert(), ca.key());

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = 0;
  hints.ai_flags = 0;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  struct addrinfo *ai_list = nullptr;
  int gaierr = getaddrinfo(opt_cert_test_host, nullptr, &hints, &ai_list);
  if (gaierr == EAI_NONAME) {
    skip(1, "Could not find addresses for %s", opt_cert_test_host);
    return;
  }
  ok(gaierr == 0, "getaddrinfo(%s) success", opt_cert_test_host);

  /* We cannot actually connect from the test host, but we can test
     each address that came from the resolver as if it belonged to a
     connected socket. The cert should be valid for each listed address. */
  ClientAuthorization *auth;
  for (struct addrinfo *ai = ai_list; ai != nullptr; ai = ai->ai_next) {
    auth = TlsKeyManager::test_client_auth(nc->cert(), ai);
    int r = TlsKeyManager::perform_client_host_auth(auth);
    char buff[INET6_ADDRSTRLEN];
    ndb_sockaddr addr(ai->ai_addr, ai->ai_addrlen);
    Ndb_inet_ntop(&addr, buff, sizeof(buff));
    ok(r == 0, "Client cert with address %s for test hostname %s is OK", buff,
       opt_cert_test_host);
    if (r) {
      printf(" >>> Test of address %s for %s returned error %s\n", buff,
             opt_cert_test_host, TlsKeyError::message(r));
    }
  }

  freeaddrinfo(ai_list);
}

/* Main */

int main(int argc, char **argv) {
  NDB_INIT("testTlsKeyManager-t");
  Ndb_opts opts(argc, argv, options);

  g_eventLogger->createConsoleHandler();

  int r = opts.handle_options();
  if (r) return r;

#if OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL

  Test::CertAuthority ca;
  ca.sign(ca);  // self-signed

  test_cluster_ca_self_signed(ca);

  // test_primary_hostname_auth(): Use X509_VERIFY_PARAM_set1_host to set
  // the expected server hostname in client's SSL Verify ctx.
  test_primary_hostname_auth(ca);

  // test_secondary_auth(): Test TlsKeyManager server hostname auth checks
  test_secondary_auth(ca);

  // test_affirm_client_auth(): Test client hostname auth checks that succeed
  test_affirm_client_auth(ca);

  test_cluster_ca_not_self_signed();

  test_old_and_new_ca(ca);

  test_cert_table(ca);

  test_key_replace(ca);

#else

  printf("Test disabled: OpenSSL version too old.\n");

#endif

  ndb_end(0);
  return exit_status();
}
