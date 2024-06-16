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

#ifndef NDB_UTIL_TLS_KEY_MANAGER_H
#define NDB_UTIL_TLS_KEY_MANAGER_H

#include "ndb_limits.h"  // MAX_NODES

#include "portlib/NdbMutex.h"
#include "util/NdbSocket.h"
#include "util/NodeCertificate.hpp"
#include "util/TlsKeyErrors.h"

struct cert_table_entry {
  time_t expires;
  const char *name;
  const char *serial;
};

class ClientAuthorization;

class TlsKeyManager {
 public:
  struct cert_record {
    static constexpr size_t SN_buf_len = 65;
    static constexpr size_t CN_buf_len = 65;

    char serial[SN_buf_len];
    char name[CN_buf_len];
    struct tm exp_tm;
    time_t expires{0};
    bool active{false};
  };

  TlsKeyManager();
  ~TlsKeyManager();

  /* TlsKeyManager::init() for NDB nodes.
     All error and info messages are logged to g_EventLogger.
     You can test whether init() has succeeded by calling ctx().
  */
  void init(const char *tls_search_path, int node_id, int node_type);

  /* init() for MGM Clients that will not have a node ID */
  void init_mgm_client(const char *tls_search_path,
                       Node::Type type = Node::Type::Client);

  /* Alternate versions of init() used for authentication testing */
  void init(int node_id, const NodeCertificate *);
  void init(int node_id, struct stack_st_X509 *, struct evp_pkey_st *);

  /* Returns the path name of the active TLS certificate file
   */
  const char *cert_path() const {
    return m_ctx ? m_cert_file.c_str() : nullptr;
  }

  /* Get SSL_CTX */
  struct ssl_ctx_st *ctx() const { return m_ctx; }

  /* Certificate table routines */
  void cert_table_set(int node_id, struct x509_st *);
  void cert_table_clear(int node_id);
  bool iterate_cert_table(int &node_id, cert_table_entry *);
  static void describe_cert(cert_record &, struct x509_st *);

  /* Check replacement date of our own node certificate.
     pct should be a number between 0.0 and 1.0, where 0 represents the
     not-valid-before date 1 represents the not-valid-after date.
     Returns true if the current time is strictly less than pct.
  */
  bool check_replace_date(float pct);

  /* Class method: TLS verification callback */
  static int on_verify(int result, struct x509_store_ctx_st *);

  /* Class methods: certificate hostname authorization checks.

     The check of a server's certificate is a simple comparison between the
     hostnames in the cert and the name the client used in order to reach the
     server.

     The check of a client's certificate requires a DNS lookup. It is divided
     into a "fast" part, in check_socket_for_auth(), and a "slow" (blocking)
     part, in perform_client_host_auth(). The API is designed to allow the
     slow part to run asynchronously if needed.
  */

  // Client-side checks of server cert:
  static int check_server_host_auth(const NdbSocket &, const char *name);
  static int check_server_host_auth(struct x509_st *, const char *name);
  static int check_server_host_auth(const NodeCertificate &, const char *);

  /* Server-side check of client cert:

     check_socket_for_auth() can return a non-zero TlsKeyError error code:
        auth2_no_cert if socket has no client certificate;
        auth2_bad_common_name if cert CN is not valid for NDB;
        auth2_bad_socket if getpeername() fails.

     Otherwise it returns zero, and the caller should check the contents
     of pAuth. If *pAuth is null, the certificate is not bound to a
     hostname, so authorization is complete. If *pAuth is non-null,
     hostname authorization is required, and the user should call
     perform_client_host_auth(*pAuth).
  */
  static int check_socket_for_auth(const NdbSocket &socket,
                                   ClientAuthorization **pAuth);

  /* test harness */
  static ClientAuthorization *test_client_auth(struct x509_st *,
                                               const struct addrinfo *);

  /* perform_client_host_auth() checks the socket peer against the certificate
     hostname, using DNS lookup. It will block, synchronously waiting for
     DNS. On return, the supplied ClientAuthorization will have been
     deleted. Returns a TlsKeyError code.
  */
  static int perform_client_host_auth(ClientAuthorization *);

 protected:
  void initialize_context();

  void log_error(TlsKeyError::code);
  void log_error() const;

  bool open_active_cert();

  static constexpr Node::Type cert_type[3] = {
      /* indexed to NODE_TYPE_DB, NODE_TYPE_API, NODE_TYPE_MGM */
      Node::Type::DB, Node::Type::Client, Node::Type::MGMD};

 private:
  PkiFile::PathName m_key_file, m_cert_file;
  char *m_path_string{nullptr};
  TlsSearchPath *m_search_path{nullptr};
  cert_record m_cert_table[MAX_NODES];
  NodeCertificate m_node_cert;
  NdbMutex m_cert_table_mutex;
  int m_error{0};
  int m_node_id;
  Node::Type m_type;
  struct ssl_ctx_st *m_ctx{nullptr};

  void init(const char *, int, Node::Type);

  bool cert_table_get(const cert_record &, cert_table_entry *) const;
  void free_path_strings();
};

inline void TlsKeyManager::init_mgm_client(const char *tls_search_path,
                                           Node::Type type) {
  init(tls_search_path, 0, type);
}

#endif
