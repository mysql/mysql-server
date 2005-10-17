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

#ifndef Transporter_H
#define Transporter_H

#include <ndb_global.h>

#include <SocketClient.hpp>

#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include "TransporterDefinitions.hpp"
#include "Packer.hpp"

#include <NdbMutex.h>
#include <NdbThread.h>

class Transporter {
  friend class TransporterRegistry;
public:
  virtual bool initTransporter() = 0;

  /**
   * Destructor
   */
  virtual ~Transporter();

  /**
   * None blocking
   *    Use isConnected() to check status
   */
  bool connect_client();
  bool connect_client(NDB_SOCKET_TYPE sockfd);
  bool connect_server(NDB_SOCKET_TYPE socket);

  /**
   * Blocking
   */
  virtual void doDisconnect();

  virtual Uint32 * getWritePtr(Uint32 lenBytes, Uint32 prio) = 0;
  virtual void updateWritePtr(Uint32 lenBytes, Uint32 prio) = 0;
  
  /**
   * Are we currently connected
   */
  bool isConnected() const;
  
  /**
   * Remote Node Id
   */
  NodeId getRemoteNodeId() const;

  /**
   * Local (own) Node Id
   */
  NodeId getLocalNodeId() const;

  /**
   * Get port we're connecting to (signed)
   */
  int get_s_port() { return m_s_port; };
  
  /**
   * Set port to connect to (signed)
   */
  void set_s_port(int port) {
    m_s_port = port;
    if(port<0)
      port= -port;
    if(m_socket_client)
      m_socket_client->set_port(port);
  };

  virtual Uint32 get_free_buffer() const = 0;
  
protected:
  Transporter(TransporterRegistry &,
	      TransporterType,
	      const char *lHostName,
	      const char *rHostName, 
	      int s_port,
	      bool isMgmConnection,
	      NodeId lNodeId,
	      NodeId rNodeId,
	      NodeId serverNodeId,
	      int byteorder, 
	      bool compression, 
	      bool checksum, 
	      bool signalId);

  /**
   * Blocking, for max timeOut milli seconds
   *   Returns true if connect succeded
   */
  virtual bool connect_server_impl(NDB_SOCKET_TYPE sockfd) = 0;
  virtual bool connect_client_impl(NDB_SOCKET_TYPE sockfd) = 0;
  
  /**
   * Blocking
   */
  virtual void disconnectImpl() = 0;
  
  /**
   * Remote host name/and address
   */
  char remoteHostName[256];
  char localHostName[256];
  struct in_addr remoteHostAddress;
  struct in_addr localHostAddress;

  int m_s_port;

  const NodeId remoteNodeId;
  const NodeId localNodeId;
  
  const bool isServer;

  unsigned createIndex;
  
  int byteOrder;
  bool compressionUsed;
  bool checksumUsed;
  bool signalIdUsed;
  Packer m_packer;  

private:

  /**
   * means that we transform an MGM connection into
   * a transporter connection
   */
  bool isMgmConnection;

  SocketClient *m_socket_client;
  struct in_addr m_connect_address;

protected:
  Uint32 getErrorCount();
  Uint32 m_errorCount;
  Uint32 m_timeOutMillis;

protected:
  bool m_connected;     // Are we connected
  TransporterType m_type;

  TransporterRegistry &m_transporter_registry;
  void *get_callback_obj() { return m_transporter_registry.callbackObj; };
  void report_disconnect(int err){m_transporter_registry.report_disconnect(remoteNodeId,err);};
  void report_error(enum TransporterError err, const char *info = 0)
    { reportError(get_callback_obj(), remoteNodeId, err, info); };
};

inline
bool
Transporter::isConnected() const {
  return m_connected;
}

inline
NodeId
Transporter::getRemoteNodeId() const {
  return remoteNodeId;
}

inline
NodeId
Transporter::getLocalNodeId() const {
  return localNodeId;
}

inline
Uint32
Transporter::getErrorCount()
{ 
  return m_errorCount;
}

#endif // Define of Transporter_H
