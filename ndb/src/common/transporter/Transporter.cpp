/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

Transporter::Transporter(TransporterRegistry &t_reg,
			 const char *lHostName,
			 const char *rHostName, 
			 int r_port,
			 NodeId lNodeId,
			 NodeId rNodeId, 
			 int _byteorder, 
			 bool _compression, bool _checksum, bool _signalId)
  : m_r_port(r_port), remoteNodeId(rNodeId), localNodeId(lNodeId),
    isServer(lNodeId < rNodeId),
    m_packer(_signalId, _checksum),
    m_transporter_registry(t_reg)
{
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

  if (strlen(lHostName) > 0)
    Ndb_getInAddr(&localHostAddress, lHostName);

  byteOrder       = _byteorder;
  compressionUsed = _compression;
  checksumUsed    = _checksum;
  signalIdUsed    = _signalId;

  m_connected     = false;
  m_timeOutMillis = 1000;

  if (isServer)
    m_socket_client= 0;
  else
    m_socket_client= new SocketClient(remoteHostName, r_port,
				      new SocketAuthSimple("ndbd", "ndbd passwd"));
}

Transporter::~Transporter(){
  if (m_socket_client)
    delete m_socket_client;
}

bool
Transporter::connect_server(NDB_SOCKET_TYPE sockfd) {
  if(m_connected)
    return true; // TODO assert(0);
  
  bool res = connect_server_impl(sockfd);
  if(res){
    m_connected  = true;
    m_errorCount = 0;
  }

  return res;
}

bool
Transporter::connect_client() {
  if(m_connected)
    return true;
  NDB_SOCKET_TYPE sockfd = m_socket_client->connect();
  
  if (sockfd == NDB_INVALID_SOCKET)
    return false;

  // send info about own id 
  SocketOutputStream s_output(sockfd);
  s_output.println("%d", localNodeId);
  // get remote id
  int nodeId;
  SocketInputStream s_input(sockfd);
  char buf[256];
  if (s_input.gets(buf, 256) == 0) {
    NDB_CLOSE_SOCKET(sockfd);
    return false;
  }
  if (sscanf(buf, "%d", &nodeId) != 1) {
    NDB_CLOSE_SOCKET(sockfd);
    return false;
  }
  bool res = connect_client_impl(sockfd);
  if(res){
    m_connected  = true;
    m_errorCount = 0;
  }
  return res;
}

void
Transporter::doDisconnect() {

  if(!m_connected)
    return; //assert(0); TODO will fail

  m_connected= false;
  disconnectImpl();
}
