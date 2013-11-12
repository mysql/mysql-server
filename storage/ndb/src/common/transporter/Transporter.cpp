/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
			 bool _compression, bool _checksum, bool _signalId,
                         Uint32 max_send_buffer)
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
    m_transporter_registry(t_reg)
{
  DBUG_ENTER("Transporter::Transporter");
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
  signalIdUsed    = _signalId;

  m_timeOutMillis = 3000;

  m_connect_address.s_addr= 0;
  if(s_port<0)
    s_port= -s_port; // was dynamic

  if (isServer)
    m_socket_client= 0;
  else
  {
    m_socket_client= new SocketClient(remoteHostName, s_port,
				      new SocketAuthSimple("ndbd",
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

Transporter::~Transporter(){
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
      conf->signalId == signalIdUsed &&
      conf->isMgmConnection == isMgmConnection &&
      conf->type == m_type)
    return true; // No change
  return false; // Can't reconfigure
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

  m_connected  = true;

  DBUG_RETURN(true);
}


bool
Transporter::connect_client() {
  NDB_SOCKET_TYPE sockfd;
  DBUG_ENTER("Transporter::connect_client");

  if(m_connected)
    DBUG_RETURN(true);

  if(isMgmConnection)
  {
    sockfd= m_transporter_registry.connect_ndb_mgmd(m_socket_client);
  }
  else
  {
    if (!m_socket_client->init())
      DBUG_RETURN(false);

    if (pre_connect_options(m_socket_client->m_sockfd) != 0)
      DBUG_RETURN(false);

    if (strlen(localHostName) > 0)
    {
      if (m_socket_client->bind(localHostName, 0) != 0)
        DBUG_RETURN(false);
    }
    sockfd= m_socket_client->connect();
  }

  DBUG_RETURN(connect_client(sockfd));
}


bool
Transporter::connect_client(NDB_SOCKET_TYPE sockfd) {

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
    DBUG_RETURN(false);

  m_connect_count++;
  resetCounters();

  m_connected = true;

  DBUG_RETURN(true);
}

void
Transporter::doDisconnect() {

  if(!m_connected)
    return;

  m_connected = false;

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
