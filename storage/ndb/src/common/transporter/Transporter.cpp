/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include "Transporter.hpp"
#include <NdbSleep.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>
#include <SocketAuthenticator.hpp>
#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"
#include "ndb_config.h"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_sockaddr.h"
#include "util/cstrbuf.h"
#include "util/require.h"

#include <EventLogger.hpp>

#if 0
#define DEBUG_FPRINTF(arglist) \
  do {                         \
    fprintf arglist;           \
  } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

// #define DEBUG_MULTI_TRP 1

#ifdef DEBUG_MULTI_TRP
#define DEB_MULTI_TRP(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_MULTI_TRP(arglist) \
  do {                         \
  } while (0)
#endif

Transporter::Transporter(TransporterRegistry &t_reg, TrpId transporter_index,
                         TransporterType _type, const char *lHostName,
                         const char *rHostName, int s_port,
                         bool _isMgmConnection, NodeId lNodeId, NodeId rNodeId,
                         NodeId serverNodeId, int _byteorder, bool _compression,
                         bool _checksum, bool _signalId, Uint32 max_send_buffer,
                         bool _presend_checksum, Uint32 spintime)
    : m_s_port(s_port),
      m_spintime(spintime),
      remoteNodeId(rNodeId),
      localNodeId(lNodeId),
      m_transporter_index(transporter_index),
      isServer(lNodeId == serverNodeId),
      m_packer(_signalId, _checksum),
      m_max_send_buffer(max_send_buffer),
      m_overload_limit(0xFFFFFFFF),
      m_slowdown_limit(0xFFFFFFFF),
      m_bytes_sent(0),
      m_bytes_received(0),
      m_connect_count(0),
      m_overload_count(0),
      m_slowdown_count(0),
      m_send_buffer_alloc_bytes(0),
      m_send_buffer_max_alloc_bytes(0),
      m_send_buffer_used_bytes(0),
      m_send_buffer_max_used_bytes(0),
      isMgmConnection(_isMgmConnection),
      m_connected(false),
      m_type(_type),
      m_require_tls(false),
      m_encrypted(false),
      reportFreq(4096),
      receiveCount(0),
      receiveSize(0),
      sendCount(0),
      sendSize(0),
      m_transporter_registry(t_reg) {
  DBUG_ENTER("Transporter::Transporter");

  // Initialize member variables
  m_multi_transporter_instance = 0;
  m_recv_thread_idx = 0;
  m_is_active = true;

  assert(rHostName);
  if (rHostName && strlen(rHostName) > 0) {
    if (cstrbuf_copy(remoteHostName, rHostName) == 1) {
      ndbout << "Unable to setup transporter. Node " << rNodeId
             << " had a too long hostname '" << rHostName
             << "'. Update configuration." << endl;
      exit(-1);
    }
  } else {
    if (!isServer) {
      g_eventLogger->info(
          "Unable to setup transporter. Node %u must have hostname."
          " Update configuration.",
          rNodeId);
      exit(-1);
    }
    remoteHostName[0] = 0;
  }
  if (cstrbuf_copy(localHostName, lHostName) == 1) {
    ndbout << "Unable to setup transporter. Node " << lNodeId
           << " had a too long hostname '" << lHostName
           << "'. Update configuration." << endl;
    exit(-1);
  }

  DBUG_PRINT("info", ("rId=%d lId=%d isServer=%d rHost=%s lHost=%s s_port=%d",
                      remoteNodeId, localNodeId, isServer, remoteHostName,
                      localHostName, s_port));

  byteOrder = _byteorder;
  compressionUsed = _compression;
  checksumUsed = _checksum;
  check_send_checksum = _presend_checksum;
  signalIdUsed = _signalId;

  m_timeOutMillis = 3000;

  if (isServer)
    m_socket_client = nullptr;
  else {
    m_socket_client = new SocketClient(new SocketAuthSimple());
    m_socket_client->set_connect_timeout(m_timeOutMillis);
  }

  m_os_max_iovec = 16;
#if defined(_SC_IOV_MAX) && defined(HAVE_SYSCONF)
  long res = sysconf(_SC_IOV_MAX);
  if (res != (long)-1) {
    m_os_max_iovec = (Uint32)res;
  }
#endif

  DBUG_VOID_RETURN;
}

void Transporter::use_tls_client_auth() {
  delete m_socket_client;
  SocketAuthTls *authTls =
      new SocketAuthTls(&m_transporter_registry.m_tls_keys, m_require_tls);
  m_socket_client = new SocketClient(authTls);
  m_socket_client->set_connect_timeout(m_timeOutMillis);
}

Transporter::~Transporter() { delete m_socket_client; }

bool Transporter::start_disconnecting(int err, bool send_source) {
  DEB_MULTI_TRP(("Disconnecting trp_id %u for node %u", getTransporterIndex(),
                 remoteNodeId));
  return m_transporter_registry.start_disconnecting(getTransporterIndex(), err,
                                                    send_source);
}

bool Transporter::configure(const TransporterConfiguration *conf) {
  if (configure_derived(conf) && conf->s_port == m_s_port &&
      conf->requireTls == m_require_tls &&
      strcmp(conf->remoteHostName, remoteHostName) == 0 &&
      strcmp(conf->localHostName, localHostName) == 0 &&
      conf->remoteNodeId == remoteNodeId && conf->localNodeId == localNodeId &&
      (conf->serverNodeId == conf->localNodeId) == isServer &&
      conf->checksum == checksumUsed &&
      conf->preSendChecksum == check_send_checksum &&
      conf->signalId == signalIdUsed &&
      conf->isMgmConnection == isMgmConnection && conf->type == m_type)
    return true;  // No change
  return false;   // Can't reconfigure
}

void Transporter::update_connect_state(bool connected) {
  assert(connected != m_connected);
  m_connected = connected;
}

bool Transporter::connect_server(NdbSocket &&sockfd, BaseString &msg) {
  // all initial negotiation is done in TransporterRegistry::connect_server
  DBUG_ENTER("Transporter::connect_server");

  if (m_connected) {
    msg.assfmt("line: %u : already connected ??", __LINE__);
    DEBUG_FPRINTF((stderr, "Transporter already connected\n"));
    DBUG_RETURN(false);
  }

  // Cache the connect address
  ndb_socket_connect_address(sockfd.ndb_socket(), &m_connect_address);

  if (!connect_server_impl(std::move(sockfd))) {
    msg.assfmt("line: %u : connect_server_impl failed", __LINE__);
    DEBUG_FPRINTF((stderr, "connect_server_impl failed\n"));
    DBUG_RETURN(false);
  }

#ifdef DEBUG_FPRINTF
  if (isPartOfMultiTransporter()) {
    DEBUG_FPRINTF((stderr, "connect_server node_id: %u, trp_id: %u\n",
                   getRemoteNodeId(), getTransporterIndex()));
  }
#endif
  m_connect_count++;
  resetCounters();

  update_connect_state(true);
  DBUG_RETURN(true);
}

inline static void tls_error(int code) {
  g_eventLogger->error("TLS error %d '%s'", code, TlsKeyError::message(code));
}

bool Transporter::connect_client_mgm(int port) {
  require(!isPartOfMultiTransporter());
  NdbSocket secureSocket =
      m_transporter_registry.connect_ndb_mgmd(remoteHostName, port);
  return connect_client(std::move(secureSocket));
}

bool Transporter::connect_client() {
  NdbSocket secureSocket;
  DBUG_ENTER("Transporter::connect_client");

  if (m_connected) {
    DBUG_RETURN(true);
  }

  int port = m_s_port;
  if (port < 0) {
    // The port number is stored as negative to indicate it's a port number
    // which the server side setup dynamically and thus was communicated
    // to the client via the ndb_mgmd.
    // Reverse the negative port number to get the connectable port
    port = -port;
  }

  if (isMgmConnection) {
    DBUG_RETURN(connect_client_mgm(port));
  } else {
    ndb_sockaddr local;
    if (strlen(localHostName) > 0) {
      if (Ndb_getAddr(&local, localHostName)) {
        DEBUG_FPRINTF((stderr, "connect_client lookup '%s' failed, node: %u\n",
                       localHostName, getRemoteNodeId()));
        DBUG_RETURN(false);
      }
    }

    ndb_sockaddr remote_addr;
    if (Ndb_getAddr(&remote_addr, remoteHostName)) {
      DEBUG_FPRINTF((stderr,
                     "connect_client lookup remote '%s' failed, node: %u\n",
                     remoteHostName, getRemoteNodeId()));
      DBUG_RETURN(false);
    }
    remote_addr.set_port(port);
    if (!m_socket_client->init(remote_addr.get_address_family())) {
      DEBUG_FPRINTF((stderr, "m_socket_client->init failed, node: %u\n",
                     getRemoteNodeId()));
      DBUG_RETURN(false);
    }

    if (pre_connect_options(m_socket_client->m_sockfd) != 0) {
      DEBUG_FPRINTF((stderr, "pre_connect_options failed, node: %u\n",
                     getRemoteNodeId()));
      DBUG_RETURN(false);
    }

    if (strlen(localHostName) > 0) {
      if (m_socket_client->bind(local) != 0) {
        DEBUG_FPRINTF((stderr, "m_socket_client->bind failed, node: %u\n",
                       getRemoteNodeId()));
        DBUG_RETURN(false);
      }
    }
    secureSocket = m_socket_client->connect(remote_addr);
    if (!secureSocket.is_valid()) {
      DBUG_RETURN(false);
    }

    /** Socket Authentication */
    int auth = m_socket_client->authenticate(secureSocket);
    g_eventLogger->debug("Transporter client auth result: %d [%s]", auth,
                         SocketAuthenticator::error(auth));
    if (auth < SocketAuthenticator::AuthOk) {
      secureSocket.close();
      DBUG_RETURN(false);
    }

    if (auth == SocketAuthTls::negotiate_tls_ok)  // Initiate TLS
    {
      struct ssl_ctx_st *ctx = m_transporter_registry.m_tls_keys.ctx();
      struct ssl_st *ssl = NdbSocket::get_client_ssl(ctx);
      if (ssl == nullptr) {
        tls_error(TlsKeyError::no_local_cert);
        secureSocket.close();
        DBUG_RETURN(false);
      }
      if (!secureSocket.associate(ssl)) {
        tls_error(TlsKeyError::openssl_error);
        NdbSocket::free_ssl(ssl);
        secureSocket.close();
        DBUG_RETURN(false);
      }
      if (!secureSocket.do_tls_handshake()) {
        tls_error(TlsKeyError::authentication_failure);
        // secureSocket closed by do_tls_handshake
        DBUG_RETURN(false);
      }

      /* Certificate Authorization */
      auth =
          TlsKeyManager::check_server_host_auth(secureSocket, remoteHostName);
      if (auth) {
        tls_error(auth);
        secureSocket.close();
        DBUG_RETURN(false);
      }
    }
  }

  DBUG_RETURN(connect_client(std::move(secureSocket)));
}

bool Transporter::connect_client(NdbSocket &&socket) {
  DBUG_ENTER("Transporter::connect_client(sockfd)");

  if (m_connected) {
    DBUG_PRINT("error", ("Already connected"));
    DEBUG_FPRINTF((stderr, "Already connected\n"));
    socket.close();
    DBUG_RETURN(true);
  }

  if (!socket.is_valid()) {
    DBUG_PRINT("error", ("Socket %s is not valid", socket.to_string().c_str()));
    DEBUG_FPRINTF((stderr, "Socket not valid\n"));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("server port: %d, isMgmConnection: %d", m_s_port,
                      isMgmConnection));

  /**
   * Send "hello"
   *
   * We can add more optional parameters here, so long as the
   * receiver can safely ignore them, and the string does
   * not get longer than the max size allowed by supported
   * receivers - see below.
   *
   * Currently have
   *   nodeId      0..255   :  3 chars
   *   space                :  1 char
   *   type          0..4   :  1 char
   *   space                :  1 char
   *   nodeId      0..255   :  3 chars
   *   space                :  1 char
   *   instance id  0..32   :  2 chars
   *   ------------------------------
   *   total                : 12 chars
   */
  char helloBuf[256];
  const int helloLen = BaseString::snprintf(
      helloBuf, sizeof(helloBuf), "%d %d %d %d", localNodeId, m_type,
      remoteNodeId, m_multi_transporter_instance);
  if (helloLen < 0) {
    DBUG_PRINT("error", ("Failed to buffer hello %d", helloLen));
    socket.close();
    DBUG_RETURN(false);
  }
  /**
   * Received in TransporterRegistry::connect_server()
   * with tight limit up to 8.0.20.
   * When servers older than 8.0.20 are no longer supported
   * the higher limit can be used.
   */
  const int OldMaxHandshakeBytesLimit = 23; /* 24 - 1 for \n */
  if (unlikely(helloLen > OldMaxHandshakeBytesLimit)) {
    /* Cannot send this many bytes to older versions */
    g_eventLogger->info("Failed handshake string length %u : \"%s\"", helloLen,
                        helloBuf);
    abort();
  }

  DBUG_PRINT("info", ("Sending hello : %s", helloBuf));
  DEBUG_FPRINTF((stderr, "Sending hello : %s\n", helloBuf));

  SocketOutputStream s_output(socket);
  if (s_output.println("%s", helloBuf) < 0) {
    DBUG_PRINT("error", ("Send of 'hello' failed"));
    socket.close();
    DBUG_RETURN(false);
  }

  // Read reply
  DBUG_PRINT("info", ("Reading reply"));
  char buf[256];
  SocketInputStream s_input(socket);
  if (s_input.gets(buf, 256) == nullptr) {
    DBUG_PRINT("error", ("Failed to read reply"));
    socket.close();
    DBUG_RETURN(false);
  }

  // Parse reply
  int nodeId, remote_transporter_type = -1;
  int r = sscanf(buf, "%d %d", &nodeId, &remote_transporter_type);
  switch (r) {
    case 2:
      break;
    default:
      DBUG_PRINT("error", ("Failed to parse reply"));
      socket.close();
      DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("nodeId=%d remote_transporter_type=%d", nodeId,
                      remote_transporter_type));

  // Check nodeid
  if (nodeId != remoteNodeId) {
    g_eventLogger->error("Connected to wrong nodeid: %d, expected: %d", nodeId,
                         remoteNodeId);
    socket.close();
    DBUG_RETURN(false);
  }

  // Check transporter type
  if (remote_transporter_type != -1 && remote_transporter_type != m_type) {
    g_eventLogger->error(
        "Connection to node: %d uses different transporter "
        "type: %d, expected type: %d",
        nodeId, remote_transporter_type, m_type);
    socket.close();
    DBUG_RETURN(false);
  }

  // Cache the connect address
  ndb_socket_connect_address(socket.ndb_socket(), &m_connect_address);

  if (!connect_client_impl(std::move(socket))) {
    DBUG_RETURN(false);
  }

  m_connect_count++;
  resetCounters();

#ifdef DEBUG_FPRINTF
  if (isPartOfMultiTransporter()) {
    DEBUG_FPRINTF(
        (stderr, "connect_client multi trp node: %u\n", getRemoteNodeId()));
  }
#endif
  update_connect_state(true);
  DBUG_RETURN(true);
}

void Transporter::doDisconnect() {
  if (!m_connected) {
    return;
  }
  disconnectImpl();             // Do the disconnect
  update_connect_state(false);  // Announce disconnect
}

void Transporter::disconnectImpl() {
  assert(theSocket.is_valid());
  if (theSocket.is_valid()) {
    DEB_MULTI_TRP(("Shutdown socket for trp %u", getTransporterIndex()));
    if (theSocket.shutdown() < 0) {
      // Do we care about shutdown failures? It might fail due to e.g.
      // connection already terminated by other peer.
      report_error(TE_ERROR_CLOSING_SOCKET);
    }
  }
}

/**
 * releaseAfterDisconnect() is assumed to be called when TR has this
 * Transporter in the DISCONNECTED state -> There are no other concurrent
 * send/receive activity on it, thus held resources can be released without
 * lock and concerns for thread safety.
 */
void Transporter::releaseAfterDisconnect() {
  assert(!isConnected());
  theSocket.close();
}

void Transporter::resetCounters() {
  m_bytes_sent = 0;
  m_bytes_received = 0;
  m_overload_count = 0;
  m_slowdown_count = 0;
  m_send_buffer_alloc_bytes = 0;
  m_send_buffer_max_alloc_bytes = 0;
  m_send_buffer_used_bytes = 0;
  m_send_buffer_max_used_bytes = 0;
}

void Transporter::checksum_state::dumpBadChecksumInfo(
    Uint32 inputSum, Uint32 badSum, size_t offset, Uint32 sig_remaining,
    const void *buf, size_t len) const {
  /* Timestamped event showing issue, followed by details */
  g_eventLogger->error(
      "Transporter::checksum_state::compute() failed with sum 0x%x", badSum);
  g_eventLogger->info("Input sum 0x%x compute offset %llu len %u  bufflen %llu",
                      inputSum, Uint64(offset), sig_remaining, Uint64(len));
  /* Next dump buf content, with word alignment
   * Buffer is a byte aligned window on signals made of words
   * remaining bytes to end of multiple-of-word sized signal
   * indicates where word alignmnent boundaries are
   */
  {
    Uint32 pos = 0;
    Uint32 buf_remain = Uint32(len);
    const char *data = (const char *)buf;
    const Uint32 firstWordBytes = Uint32((offset + sig_remaining) & 3);
    if (firstWordBytes && (buf_remain >= firstWordBytes)) {
      /* Partial first word */
      Uint32 word = 0;
      memcpy(&word, data, firstWordBytes);
      g_eventLogger->info("-%4x  : 0x%08x", 4 - firstWordBytes, word);
      buf_remain -= firstWordBytes;
      pos += firstWordBytes;
    }

    char logbuf[MAX_LOG_MESSAGE_SIZE] = "";

    if (buf_remain)
      BaseString::snappend(logbuf, sizeof(logbuf), " %4x  : ", pos);

    while (buf_remain > 4) {
      Uint32 word;
      memcpy(&word, data + pos, 4);
      pos += 4;
      buf_remain -= 4;
      BaseString::snappend(logbuf, sizeof(logbuf), "0x%08x ", word);
      if (((pos + firstWordBytes) % 24) == 0) {
        g_eventLogger->info("%s", logbuf);

        logbuf[0] = '\0';
        BaseString::snappend(logbuf, sizeof(logbuf), " %4x  : ", pos);
      }
    }
    if (buf_remain > 0) {
      /* Partial last word */
      Uint32 word = 0;
      memcpy(&word, data + pos, buf_remain);
      g_eventLogger->info("%s 0x%08x", logbuf, word);
    }
  }
}

void Transporter::set_get(ndb_socket_t fd, int level, int optval,
                          const char *optname [[maybe_unused]], int val) {
  int actual = 0, defval = 0;

  ndb_getsockopt(fd, level, optval, &defval);

  if (ndb_setsockopt(fd, level, optval, &val) < 0) {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) errno: %d %s", optname, val, errno,
                         strerror(errno));
#endif
  }

  if ((ndb_getsockopt(fd, level, optval, &actual) == 0) && actual != val) {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) - actual %d default: %d", optname,
                         val, actual, defval);
#endif
  }
}
