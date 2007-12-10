/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include "Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <NdbSleep.h>
#include <SocketAuthenticator.hpp>
#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

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
			 bool _compression, bool _checksum, bool _signalId)
  : m_s_port(s_port), remoteNodeId(rNodeId), localNodeId(lNodeId),
    isServer(lNodeId==serverNodeId),
    m_packer(_signalId, _checksum),  isMgmConnection(_isMgmConnection),
    m_type(_type),
    m_transporter_registry(t_reg)
{
  DBUG_ENTER("Transporter::Transporter");
  if (rHostName && strlen(rHostName) > 0){
    strncpy(remoteHostName, rHostName, sizeof(remoteHostName));
    Ndb_getInAddr(&remoteHostAddress, rHostName);
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

  m_connected     = false;
  m_timeOutMillis = 30000;

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

    m_socket_client->set_connect_timeout((m_timeOutMillis+999)/1000);
  }
  DBUG_VOID_RETURN;
}

Transporter::~Transporter(){
  if (m_socket_client)
    delete m_socket_client;
}

bool
Transporter::connect_server(NDB_SOCKET_TYPE sockfd) {
  // all initial negotiation is done in TransporterRegistry::connect_server
  DBUG_ENTER("Transporter::connect_server");

  if(m_connected)
  {
    DBUG_RETURN(false); // TODO assert(0);
  }
  
  {
    struct sockaddr_in addr;
    SOCKET_SIZE_TYPE addrlen= sizeof(addr);
    getpeername(sockfd, (struct sockaddr*)&addr, &addrlen);
    m_connect_address= (&addr)->sin_addr;
  }

  bool res = connect_server_impl(sockfd);
  if(res){
    m_connected  = true;
    m_errorCount = 0;
  }

  DBUG_RETURN(res);
}

bool
Transporter::connect_client() {
  NDB_SOCKET_TYPE sockfd;

  if(m_connected)
    return true;

  if(isMgmConnection)
  {
    sockfd= m_transporter_registry.connect_ndb_mgmd(m_socket_client);
  }
  else
  {
    if (!m_socket_client->init())
    {
      return false;
    }
    if (strlen(localHostName) > 0)
    {
      if (m_socket_client->bind(localHostName, 0) != 0)
	return false;
    }
    sockfd= m_socket_client->connect();
  }

  return connect_client(sockfd);
}

bool
Transporter::connect_client(NDB_SOCKET_TYPE sockfd) {

  if(m_connected)
    return true;

  if (sockfd == NDB_INVALID_SOCKET)
    return false;

  DBUG_ENTER("Transporter::connect_client");

  DBUG_PRINT("info",("port %d isMgmConnection=%d",m_s_port,isMgmConnection));

  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);

  // send info about own id
  // send info about own transporter type

  s_output.println("%d %d", localNodeId, m_type);
  // get remote id
  int nodeId, remote_transporter_type= -1;

  char buf[256];
  if (s_input.gets(buf, 256) == 0) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  int r= sscanf(buf, "%d %d", &nodeId, &remote_transporter_type);
  switch (r) {
  case 2:
    break;
  case 1:
    // we're running version prior to 4.1.9
    // ok, but with no checks on transporter configuration compatability
    break;
  default:
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("nodeId=%d remote_transporter_type=%d",
		      nodeId, remote_transporter_type));

  if (remote_transporter_type != -1)
  {
    if (remote_transporter_type != m_type)
    {
      DBUG_PRINT("error", ("Transporter types mismatch this=%d remote=%d",
			   m_type, remote_transporter_type));
      NDB_CLOSE_SOCKET(sockfd);
      g_eventLogger.error("Incompatible configuration: transporter type "
			  "mismatch with node %d", nodeId);
      DBUG_RETURN(false);
    }
  }
  else if (m_type == tt_SHM_TRANSPORTER)
  {
    g_eventLogger.warning("Unable to verify transporter compatability with node %d", nodeId);
  }

  {
    struct sockaddr_in addr;
    SOCKET_SIZE_TYPE addrlen= sizeof(addr);
    getpeername(sockfd, (struct sockaddr*)&addr, &addrlen);
    m_connect_address= (&addr)->sin_addr;
  }

  bool res = connect_client_impl(sockfd);
  if(res){
    m_connected  = true;
    m_errorCount = 0;
  }
  DBUG_RETURN(res);
}

void
Transporter::doDisconnect() {

  if(!m_connected)
    return; //assert(0); TODO will fail

  m_connected= false;
  disconnectImpl();
}
