/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include "Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <NdbSleep.h>
#include <SocketAuthenticator.hpp>
#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

Transporter::Transporter(TransporterRegistry &t_reg,
			 TransporterType _type,
			 const char *lHostName,
			 const char *rHostName, 
			 int s_port,
			 bool _isMgmConnection,
			 NodeId lNodeId,
			 NodeId rNodeId,
			 NodeId serverNodeId,
			 int _byteorder, 
			 bool _compression,
			 bool _checksum,
			 bool _signalId,
             Uint32 max_send_buffer,
             bool _presend_checksum)
  : m_s_port(s_port), remoteNodeId(rNodeId), localNodeId(lNodeId),
    isServer(lNodeId==serverNodeId),
    m_packer(_signalId, _checksum), m_max_send_buffer(max_send_buffer),
    m_overload_limit(0xFFFFFFFF), m_slowdown_limit(0xFFFFFFFF),
    m_bytes_sent(0), m_bytes_received(0),
    m_connect_count(0),
    m_overload_count(0), m_slowdown_count(0),
    isMgmConnection(_isMgmConnection),
    m_connected(false),
    m_type(_type),
    reportFreq(4096),
    receiveCount(0),
    receiveSize(0),
    sendCount(0),
    sendSize(0),
    m_transporter_registry(t_reg)
{
  DBUG_ENTER("Transporter::Transporter");

  // Initialize member variables
  my_socket_invalidate(&theSocket);

  if (rHostName && strlen(rHostName) > 0){
    strncpy(remoteHostName, rHostName, sizeof(remoteHostName));
  }
  else
  {
    if (!isServer) {
      ndbout << "Unable to setup transporter. Node " << rNodeId 
	     << " must have hostname. Update configuration." << endl; 
      exit(-1);
    }
    remoteHostName[0]= 0;
  }
  strncpy(localHostName, lHostName, sizeof(localHostName));

  DBUG_PRINT("info",("rId=%d lId=%d isServer=%d rHost=%s lHost=%s s_port=%d",
		     remoteNodeId, localNodeId, isServer,
		     remoteHostName, localHostName,
		     s_port));

  byteOrder       = _byteorder;
  compressionUsed = _compression;
  checksumUsed    = _checksum;
  check_send_checksum = _presend_checksum;
  signalIdUsed    = _signalId;

  m_timeOutMillis = 3000;

  m_connect_address.s_addr= 0;

  if (isServer)
    m_socket_client= 0;
  else
  {
    m_socket_client= new SocketClient(new SocketAuthSimple("ndbd",
                                                           "ndbd passwd"));

    m_socket_client->set_connect_timeout(m_timeOutMillis);
  }

  m_os_max_iovec = 16;
#if defined (_SC_IOV_MAX) && defined (HAVE_SYSCONF)
  long res = sysconf(_SC_IOV_MAX);
  if (res != (long)-1)
  {
    m_os_max_iovec = (Uint32)res;
  }
#endif
  
  DBUG_VOID_RETURN;
}

Transporter::~Transporter()
{
  delete m_socket_client;
}


bool
Transporter::configure(const TransporterConfiguration* conf)
{
  if (configure_derived(conf) &&
      conf->s_port == m_s_port &&
      strcmp(conf->remoteHostName, remoteHostName) == 0 &&
      strcmp(conf->localHostName, localHostName) == 0 &&
      conf->remoteNodeId == remoteNodeId &&
      conf->localNodeId == localNodeId &&
      (conf->serverNodeId == conf->localNodeId) == isServer &&
      conf->checksum == checksumUsed &&
      conf->preSendChecksum == check_send_checksum &&
      conf->signalId == signalIdUsed &&
      conf->isMgmConnection == isMgmConnection &&
      conf->type == m_type)
    return true; // No change
  return false; // Can't reconfigure
}

void
Transporter::update_connect_state(bool connected)
{
  assert(connected != m_connected);
  m_connected  = connected;
}

bool
Transporter::connect_server(NDB_SOCKET_TYPE sockfd,
                            BaseString& msg) {
  // all initial negotiation is done in TransporterRegistry::connect_server
  DBUG_ENTER("Transporter::connect_server");

  if (m_connected)
  {
    msg.assfmt("line: %u : already connected ??", __LINE__);
    DBUG_RETURN(false);
  }

  // Cache the connect address
  my_socket_connect_address(sockfd, &m_connect_address);

  if (!connect_server_impl(sockfd))
  {
    msg.assfmt("line: %u : connect_server_impl failed", __LINE__);
    DBUG_RETURN(false);
  }

  m_connect_count++;
  resetCounters();

  update_connect_state(true);
  DBUG_RETURN(true);
}


bool
Transporter::connect_client()
{
  NDB_SOCKET_TYPE sockfd;
  DBUG_ENTER("Transporter::connect_client");

  if(m_connected)
  {
    DBUG_RETURN(true);
  }

  int port = m_s_port;
  if (port<0)
  {
    // The port number is stored as negative to indicate it's a port number
    // which the server side setup dynamically and thus was communicated
    // to the client via the ndb_mgmd.
    // Reverse the negative port number to get the connectable port
    port= -port;
  }

  if(isMgmConnection)
  {
    sockfd= m_transporter_registry.connect_ndb_mgmd(remoteHostName,
                                                    port);
  }
  else
  {
    if (!m_socket_client->init())
    {
      DBUG_RETURN(false);
    }

    if (pre_connect_options(m_socket_client->m_sockfd) != 0)
    {
      DBUG_RETURN(false);
    }

    if (strlen(localHostName) > 0)
    {
      if (m_socket_client->bind(localHostName, 0) != 0)
      {
        DBUG_RETURN(false);
      }
    }

    sockfd= m_socket_client->connect(remoteHostName,
                                     port);
  }

  DBUG_RETURN(connect_client(sockfd));
}


bool
Transporter::connect_client(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("Transporter::connect_client(sockfd)");

  if(m_connected)
  {
    DBUG_PRINT("error", ("Already connected"));
    DBUG_RETURN(true);
  }

  if (!my_socket_valid(sockfd))
  {
    DBUG_PRINT("error", ("Socket " MY_SOCKET_FORMAT " is not valid",
                         MY_SOCKET_FORMAT_VALUE(sockfd)));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info",("server port: %d, isMgmConnection: %d",
                     m_s_port, isMgmConnection));

  // Send "hello"
  DBUG_PRINT("info", ("Sending own nodeid: %d and transporter type: %d",
                      localNodeId, m_type));
  SocketOutputStream s_output(sockfd);
  if (s_output.println("%d %d", localNodeId, m_type) < 0)
  {
    DBUG_PRINT("error", ("Send of 'hello' failed"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  // Read reply
  DBUG_PRINT("info", ("Reading reply"));
  char buf[256];
  SocketInputStream s_input(sockfd);
  if (s_input.gets(buf, 256) == 0)
  {
    DBUG_PRINT("error", ("Failed to read reply"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  // Parse reply
  int nodeId, remote_transporter_type= -1;
  int r= sscanf(buf, "%d %d", &nodeId, &remote_transporter_type);
  switch (r) {
  case 2:
    break;
  case 1:
    // we're running version prior to 4.1.9
    // ok, but with no checks on transporter configuration compatability
    break;
  default:
    DBUG_PRINT("error", ("Failed to parse reply"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("nodeId=%d remote_transporter_type=%d",
		      nodeId, remote_transporter_type));

  // Check nodeid
  if (nodeId != remoteNodeId)
  {
    g_eventLogger->error("Connected to wrong nodeid: %d, expected: %d",
                         nodeId, remoteNodeId);
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  // Check transporter type
  if (remote_transporter_type != -1 &&
      remote_transporter_type != m_type)
  {
    g_eventLogger->error("Connection to node: %d uses different transporter "
                         "type: %d, expected type: %d",
                         nodeId, remote_transporter_type, m_type);
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  // Cache the connect address
  my_socket_connect_address(sockfd, &m_connect_address);

  if (!connect_client_impl(sockfd))
  {
    DBUG_RETURN(false);
  }

  m_connect_count++;
  resetCounters();

  update_connect_state(true);
  DBUG_RETURN(true);
}

void
Transporter::doDisconnect()
{
  if(!m_connected)
  {
    return;
  }
  update_connect_state(false);
  disconnectImpl();
}

void
Transporter::resetCounters()
{
  m_bytes_sent = 0;
  m_bytes_received = 0;
  m_overload_count = 0;
  m_slowdown_count = 0;
};

void
Transporter::checksum_state::dumpBadChecksumInfo(Uint32 inputSum,
                                                 Uint32 badSum,
                                                 size_t offset,
                                                 Uint32 sig_remaining,
                                                 const void* buf,
                                                 size_t len) const
{
  /* Timestamped event showing issue, followed by details */
  /* As eventLogger and stderr may not be in-sync, put details together */
  g_eventLogger->error("Transporter::checksum_state::compute() failed");
  fprintf(stderr,
          "checksum_state::compute() failed "
          "with sum 0x%x.\n"
          "Input sum 0x%x compute offset %llu len %u "
          "bufflen %llu\n",
          badSum,
          inputSum,
          Uint64(offset),
          sig_remaining,
          Uint64(len));
  /* Next dump buf content, with word alignment
   * Buffer is a byte aligned window on signals made of words
   * remaining bytes to end of multiple-of-word sized signal
   * indicates where word alignmnent boundaries are
   */
  {
    Uint32 pos = 0;
    Uint32 buf_remain = Uint32(len);
    const char* data = (const char*) buf;
    const Uint32 firstWordBytes = Uint32((offset + sig_remaining) & 3);
    if (firstWordBytes && (buf_remain >= firstWordBytes))
    {
      /* Partial first word */
      Uint32 word = 0;
      memcpy(&word, data, firstWordBytes);
      fprintf(stderr, "\n-%4x  : 0x%08x\n", 4 - firstWordBytes, word);
      buf_remain -= firstWordBytes;
      pos += firstWordBytes;
    }

    if (buf_remain)
      fprintf(stderr, "\n %4x  : ", pos);

    while (buf_remain > 4)
    {
      Uint32 word;
      memcpy(&word, data+pos, 4);
      pos += 4;
      buf_remain -= 4;
      fprintf(stderr, "0x%08x ", word);
      if (((pos + firstWordBytes) % 24) == 0)
        fprintf(stderr, "\n %4x  : ", pos);
    }
    if (buf_remain > 0)
    {
      /* Partial last word */
      Uint32 word = 0;
      memcpy(&word, data + pos, buf_remain);
      fprintf(stderr, "0x%08x\n", word);
    }
    fprintf(stderr, "\n\n");
  }
}

void
Transporter::set_get(NDB_SOCKET_TYPE fd,
                     int level,
                     int optval,
                     const char *optname, 
                     int val)
{
  int actual = 0, defval = 0;
  socket_len_t len = sizeof(actual);

  my_getsockopt(fd, level, optval, (char*)&defval, &len);

  if (my_setsockopt(fd, level, optval,
                    (char*)&val, sizeof(val)) < 0)
  {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) errno: %d %s",
                         optname, val, errno, strerror(errno));
#endif
  }
  
  len = sizeof(actual);
  if ((my_getsockopt(fd, level, optval,
                     (char*)&actual, &len) == 0) &&
      actual != val)
  {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) - actual %d default: %d",
                         optname, val, actual, defval);
#endif
  }
}
