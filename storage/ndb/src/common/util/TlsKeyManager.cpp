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

#include <sys/stat.h>
#include <cinttypes>  // PRIuPTR
#include <cstdint>
#include <memory>

#include "openssl/err.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

#include "debugger/EventLogger.hpp"
#include "portlib/ndb_openssl_version.h"
#include "util/NodeCertificate.hpp"
#include "util/TlsKeyManager.hpp"
#include "util/cstrbuf.h"
#include "util/ndb_openssl3_compat.h"
#include "util/require.h"

TlsKeyManager::TlsKeyManager() { NdbMutex_Init(&m_cert_table_mutex); }

void TlsKeyManager::free_path_strings() {
  if (m_search_path) delete m_search_path;
  if (m_path_string) free(m_path_string);
  m_search_path = nullptr;
  m_path_string = nullptr;
}

TlsKeyManager::~TlsKeyManager() {
  if (m_ctx) SSL_CTX_free(m_ctx);
  free_path_strings();
  NdbMutex_Deinit(&m_cert_table_mutex);
}

void TlsKeyManager::log_error(TlsKeyError::code err) {
  m_error = err;
  log_error();
}

void TlsKeyManager::log_error() const {
  if (m_error < TlsKeyError::END_GENERIC_ERRORS)
    g_eventLogger->error("TLS key error: %s.\n", TlsKeyError::message(m_error));
  else
    g_eventLogger->error("TLS key error: %s (with path: %s).\n",
                         TlsKeyError::message(m_error), m_path_string);
}

#if OPENSSL_VERSION_NUMBER < NDB_TLS_MINIMUM_OPENSSL

void TlsKeyManager::init(int, const NodeCertificate *) {}

void TlsKeyManager::init(const char *, int, int) {}

void TlsKeyManager::init(const char *, int, Node::Type) {}

void TlsKeyManager::init(int, struct stack_st_X509 *, struct evp_pkey_st *) {}

#else

/* This is the list of allowed ciphers.
 * It includes all TLS 1.3 cipher suites, plus one TLS 1.2 cipher suite,
 * ECDHE-ECDSA-AES128-GCM-SHA256.
 */
static constexpr const char *cipher_list =
    "TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384:"
    "TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256:"
    "ECDHE-ECDSA-AES128-GCM-SHA256";

static int error_callback(const char *str, size_t, void *vp) {
  intptr_t r = reinterpret_cast<intptr_t>(vp);
  g_eventLogger->error("NDB TLS [%" PRIuPTR "]: %s", r, str);
  return 0;
}

static void log_openssl_errors(intptr_t r) {
  ERR_print_errors_cb(error_callback, reinterpret_cast<void *>(r));
}

void TlsKeyManager::init(const char *tls_search_path, int node_id,
                         int ndb_node_type) {
  require(ndb_node_type >= NODE_TYPE_DB && ndb_node_type <= NODE_TYPE_MGM);
  Node::Type nodeType = cert_type[ndb_node_type];
  init(tls_search_path, node_id, nodeType);
}

void TlsKeyManager::init(const char *tls_search_path, int node_id,
                         Node::Type node_type) {
  if (m_ctx) return;  // already initialized

  /* Set node id and type */
  m_node_id = node_id;
  m_type = node_type;

  /* Initialize Search Path */
  m_search_path = new TlsSearchPath(tls_search_path);
  m_path_string = m_search_path->expanded_path_string();

  /* Open active certificate; initialize NodeCertificate.  */
  if (!open_active_cert()) {
    free_path_strings();
    return;
  }

  initialize_context();

  if (m_ctx && (node_type != Node::Type::Client))
    g_eventLogger->info("NDB TLS 1.3 available using certificate file '%s'",
                        m_cert_file.c_str());
}

/* Versions of init() used by test harness */
void TlsKeyManager::init(int node_id, STACK_OF(X509) * certs, EVP_PKEY *key) {
  assert(m_ctx == nullptr);
  assert(certs);
  assert(key);

  /* Set node id */
  m_node_id = node_id;

  /* Initialize node cert and take a reference to the stack and key */
  m_node_cert.init_from_credentials(certs, key, true);

  initialize_context();
}

void TlsKeyManager::init(int node_id, const NodeCertificate *nc) {
  init(node_id, nc->all_certs(), nc->key());
}

class SSL_CTX_owner {
 public:
  SSL_CTX *ctx{nullptr};
  ~SSL_CTX_owner() {
    if (ctx) SSL_CTX_free(ctx);
  }
};

void TlsKeyManager::initialize_context() {
  SSL_CTX_owner g;
  SSL_CTX *&ctx = g.ctx;

  /* Initialize Context */
  g.ctx = SSL_CTX_new(TLS_method());
  if (!ctx) return log_openssl_errors(-3);

  /* Set the active key and certificate in the context */
  if (SSL_CTX_use_certificate(ctx, m_node_cert.cert()) != 1)
    return log_openssl_errors(-4);

  if (SSL_CTX_use_PrivateKey(ctx, m_node_cert.key()) != 1)
    return log_openssl_errors(-5);

  /* Create a Verify Store for use in the CTX, using the CAs starting
     from the 2nd certificate in the NodeCertificate stack.
  */
  X509_STORE *store = X509_STORE_new();
  if (!store) return log_openssl_errors(-6);

  /* For X509_STORE_set_depth() see X509_VERIFY_PARAM_SET_FLAGS(3)
     "With a depth limit of 1 there can be one intermediate CA certificate
     between the trust-anchor and the end-entity certificate."
  */
  X509_STORE_set_depth(store, 1);

  const STACK_OF(X509) *CAs = m_node_cert.all_certs();
  if (!CAs) {
    X509_STORE_free(store);
    return log_error(TlsKeyError::active_cert_invalid);
  }

  int ncerts = sk_X509_num(CAs);
  if (ncerts < 2) {
    X509_STORE_free(store);
    g_eventLogger->error("NDB TLS: No CA chain in active certificate: %s",
                         m_cert_file.c_str());
    return;
  }

  for (int i = 1; i < ncerts; i++) {
    X509 *CA_cert = sk_X509_value(CAs, i);
    X509_STORE_add_cert(store, CA_cert);
  }
  SSL_CTX_set1_cert_store(ctx, store);
  X509_STORE_free(store);

  /* Check the private key */
  if (SSL_CTX_check_private_key(ctx) != 1) return log_openssl_errors(-7);

  /* Set the cipher list */
  if (SSL_CTX_set_cipher_list(ctx, cipher_list) != 1)
    return log_openssl_errors(-8);

  /* Set the minimum protocol version */
  SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);

  /* Set the security level to level 2, requiring 112-bit security.
     RSA keys must be at least 2048 bits, and ECC keys must be at least
     256 bits.
  */
  if (SSL_CTX_get_security_level(ctx) < 2) SSL_CTX_set_security_level(ctx, 2);

  /* Never use the Subject Common Name for hostname checking, since we will
     often put something like "NDB Node 3" in it. Use Subject Alt Names instead.
  */
  X509_VERIFY_PARAM *vpm = SSL_CTX_get0_param(ctx);
  X509_VERIFY_PARAM_set_hostflags(vpm, X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);

  /* Set verification mode and callback. Require client certificates. */
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                     TlsKeyManager::on_verify);

  /* Store the SSL_CTX in m_ctx */
  if (SSL_CTX_up_ref(g.ctx) != 1) return log_openssl_errors(-9);
  m_ctx = g.ctx;

  /* Store our own NodeCertificate in the cert table */
  cert_table_set(m_node_id, m_node_cert.cert());
}

#endif

int TlsKeyManager::on_verify(int result, X509_STORE_CTX *store) {
  /* If result is 0, verification has failed, and this callback is
     our opportunity to write a log message.
  */
  if (!result) {
    NodeCertificate peer_node_cert;
    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    int err = X509_STORE_CTX_get_error(store);
    if (cert) {
      char name[128];
      Certificate::get_common_name(cert, name, 128);
      peer_node_cert.init_from_x509(cert);
      g_eventLogger->error("TLS AUTH: Rejected%s certificate '%s' (%s).",
                           peer_node_cert.name_is_conforming() ? " node" : "",
                           name, peer_node_cert.serial_number().c_str());
    }
    g_eventLogger->error("TLS AUTH: Rejected at eval depth %d, error %d: %s.",
                         X509_STORE_CTX_get_error_depth(store), err,
                         X509_verify_cert_error_string(err));
  }

  return result;
}

int TlsKeyManager::check_server_host_auth(const NdbSocket &socket,
                                          const char *hostname) {
  if (!socket.has_tls()) return TlsKeyError::no_error;

  /* Get TLS peer's X509 Certificate */
  X509 *x509 = socket.peer_certificate();
  if (!x509) return TlsKeyError::auth2_no_cert;
  int r = TlsKeyManager::check_server_host_auth(x509, hostname);
  Certificate::free(x509);
  return r;
}

int TlsKeyManager::check_server_host_auth(X509 *peer_x509,
                                          const char *hostname) {
  NodeCertificate peer_cert;
  peer_cert.init_from_x509(peer_x509);
  if (!peer_cert.name_is_conforming())
    return TlsKeyError::auth2_bad_common_name;

  return check_server_host_auth(peer_cert, hostname);
}

int TlsKeyManager::check_server_host_auth(const NodeCertificate &nc,
                                          const char *hostname) {
  /* If the certificate is not bound to a hostname, auth has succeeded */
  int n_bound_hosts = nc.bound_hostnames();
  if (n_bound_hosts == 0) return 0;

  /* If the server's certificate is bound to the name "localhost",
     the server's configured HostName must be either "" or "localhost" */
  if (nc.bound_localhost()) {
    if (strlen(hostname) && strcmp(hostname, "localhost"))
      return TlsKeyError::auth2_bad_hostname;
    return 0;
  }

  /* Check configured hostname against certificate hostname */
  for (int n = 0; n < n_bound_hosts; n++)
    if (nc.bound_hostname(n) == hostname) return 0;

  return TlsKeyError::auth2_bad_hostname;
}

class ClientAuthorization {
 public:
  ndb_sockaddr m_sockaddr;
  std::unique_ptr<const NodeCertificate> m_cert;

  ClientAuthorization(X509 *x509) : m_cert(NodeCertificate::for_peer(x509)) {}

  ClientAuthorization(const struct addrinfo *ai, X509 *x509)
      : m_sockaddr(ai->ai_addr, ai->ai_addrlen),
        m_cert(NodeCertificate::for_peer(x509)) {}

  int run();
  int run_check_name(int);
  bool compare_list(const addrinfo *) const;
};

int TlsKeyManager::check_socket_for_auth(const NdbSocket &socket,
                                         ClientAuthorization **pAuth) {
  *pAuth = nullptr;
  if (!socket.has_tls()) return 0;

  X509 *cert = socket.peer_certificate();
  if (!cert) return TlsKeyError::auth2_no_cert;

  ClientAuthorization *auth = new ClientAuthorization(cert);
  Certificate::free(cert);

  if (auth->m_cert->bound_hostnames() == 0) {
    delete auth;  // Hostname auth is not needed
    return 0;
  }

  /* Get peer address from socket */
  if (ndb_getpeername(socket.ndb_socket(), &(auth->m_sockaddr))) {
    delete auth;
    return TlsKeyError::auth2_bad_socket;
  }

  /* Check for localhost certificate with loopback address */
  if (auth->m_cert->bound_localhost() && auth->m_sockaddr.is_loopback()) {
    delete auth;
    return 0;
  }

  *pAuth = auth;
  return 0;
}

ClientAuthorization *TlsKeyManager::test_client_auth(
    X509 *cert, const struct addrinfo *ai) {
  return new ClientAuthorization(ai, cert);
}

int ClientAuthorization::run() {
  /* The certificate contains one or more names. The socket is connected to
     exactly one peer.

     A PTR lookup of the socket address should return the canonical hostname,
     but an attacker who owns some IP address space can easily craft a PTR
     record for the attack host that matches the name in the certificate.
     So verification should proceed by looking up the names from the cert.

     Ideally we would like to send out a batch of requests, asking for
     addresses for every name in the cert. As the replies come back, one by
     one, each reply might resolve the authorization positively (allowing us
     to cancel the rest of the requests and return immediately).

     Iterate over the list in the cert, making a blocking call for each name.
  */
  int ck = 0;
  for (int i = 0; i < m_cert->bound_hostnames(); i++) {
    ck = run_check_name(i);
    if (ck == 0) break;
  }

  return ck;
}

int ClientAuthorization::run_check_name(int n) {
  BaseString name = m_cert->bound_hostname(n);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo *ai_list = nullptr;
  int r = getaddrinfo(name.c_str(), nullptr, &hints, &ai_list);
  if (r) {
    g_eventLogger->error(
        "TLS Authorization failure checking host name '%s': '%s'", name.c_str(),
        gai_strerror(r));
    return TlsKeyError::auth2_resolver_error;
  }

  bool cmp = compare_list(ai_list);
  freeaddrinfo(ai_list);
  if (cmp == true) return 0;
  return TlsKeyError::auth2_bad_hostname;
}

bool ClientAuthorization::compare_list(const addrinfo *ai_list) const {
  for (const addrinfo *ai = ai_list; ai != nullptr; ai = ai->ai_next) {
    ndb_sockaddr addr(ai->ai_addr, ai->ai_addrlen);
    if (m_sockaddr.has_same_addr(addr)) return true;
  }
  return false;
}

int TlsKeyManager::perform_client_host_auth(ClientAuthorization *auth) {
  int r = auth->run();
  delete auth;
  return r;
}

/*
 * Certificate table routines
 */
void TlsKeyManager::describe_cert(cert_record &entry, struct x509_st *cert) {
  SerialNumber::print(entry.serial, cert_record::SN_buf_len,
                      X509_get0_serialNumber(cert));
  Certificate::get_common_name(cert, entry.name, cert_record::CN_buf_len);
  entry.expires = 0;
  if (ASN1_TIME_to_tm(X509_get0_notAfter(cert), &entry.exp_tm))
    entry.expires = mktime(&entry.exp_tm);
}

void TlsKeyManager::cert_table_set(int node_id, X509 *cert) {
  Guard mutex_guard(&m_cert_table_mutex);
  assert(node_id < MAX_NODES);
  if (node_id == 0) return;  // Client certs do not go into table

  /* In the case of a multi-transporter, the entry may already be active */
  struct cert_record &entry = m_cert_table[node_id];
  if (!entry.active) {
    describe_cert(entry, cert);
    entry.active = true;
  }
}

void TlsKeyManager::cert_table_clear(int node_id) {
  Guard mutex_guard(&m_cert_table_mutex);
  assert(node_id < MAX_NODES);

  struct cert_record &entry = m_cert_table[node_id];
  entry.serial[0] = '\0';
  entry.name[0] = '\0';
  entry.expires = 0;
  entry.active = false;
}

bool TlsKeyManager::cert_table_get(const cert_record &row,
                                   cert_table_entry *client_row) const {
  assert(row.active);
  client_row->expires = row.expires;
  client_row->name = &row.name[0];
  client_row->serial = &row.serial[0];
  return true;
}

bool TlsKeyManager::iterate_cert_table(int &node, cert_table_entry *client) {
  Guard mutex_guard(&m_cert_table_mutex);

  if (node < 0) node = 0;
  if (m_ctx) {
    while (node < MAX_NODES_ID) {
      node += 1;
      const cert_record &row = m_cert_table[node];
      if (row.active) return cert_table_get(row, client);
    }
  }
  return false;
}

bool TlsKeyManager::open_active_cert() {
  if (ActivePrivateKey::find(m_search_path, m_node_id, m_type, m_key_file)) {
    EVP_PKEY *key = PrivateKey::open(m_key_file, nullptr);
    if (key) {
      if (ActiveCertificate::find(m_search_path, m_node_id, m_type,
                                  m_cert_file)) {
        STACK_OF(X509) *certs = Certificate::open(m_cert_file);
        if (certs) {
          if (EVP_PKEY_eq(key, X509_get0_pubkey(sk_X509_value(certs, 0)))) {
            m_node_cert.init_from_credentials(certs, key);
            if (m_node_cert.is_signed()) {
              if (check_replace_date(1.0)) return true;
              log_error(TlsKeyError::active_cert_expired);
            } else
              log_error(TlsKeyError::active_cert_invalid);
          } else
            log_error(TlsKeyError::active_cert_mismatch);
        } else
          log_error(TlsKeyError::cannot_read_active_cert);
      } else
        log_error(TlsKeyError::active_cert_not_found);
    } else
      log_error(TlsKeyError::cannot_read_active_key);
  }
  return false;
}

bool TlsKeyManager::check_replace_date(float pct) {
  assert(m_node_cert.is_final());
  time_t current_time = time(nullptr);
  time_t replace_time = m_node_cert.replace_time(pct);

  return ((replace_time > 0) && (current_time < replace_time));
}

#ifdef TEST_TLSKEYMANAGER
/* This test is intended only to check for memory leaks.
   usage: TlsKeyManager-t search-path
*/

/* */
int main(int argc, const char *argv[]) {
  TlsKeyManager t;
  t.init_mgm_client(argc > 1 ? argv[1] : nullptr, Node::Type::ANY);
  puts(t.ctx() ? "Loaded a certificate." : "Did not load a certificate.");
  return 0;
}

#endif
