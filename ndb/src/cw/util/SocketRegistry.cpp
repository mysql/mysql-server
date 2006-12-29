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

#include "SocketRegistry.hpp"
#include <Parser.hpp>

template<class T>
SocketRegistry<T>::SocketRegistry(Uint32 maxSocketClients) {

}


template<class T>
SocketRegistry<T>::~SocketRegistry() {
  delete [] m_socketClients;
}

template<class T>
bool 
SocketRegistry<T>::createSocketClient(const char * host, Uint16 port) {

  if(port == 0)
    return false;
  if(host==NULL)
    return false;
  
  SocketClient * socketClient = new SocketClient(host, port);

  if(socketClient->openSocket() < 0 || socketClient == NULL) {
    ndbout << "could not connect" << endl;
    delete socketClient;
    return false;
  }
  else {
    m_socketClients[m_nSocketClients] = socketClient;
    m_nSocketClients++;
  }
  return true;
}

template<class T>
int 
SocketRegistry<T>::pollSocketClients(Uint32 timeOutMillis) {



  // Return directly if there are no TCP transporters configured
  if (m_nSocketClients == 0){
    tcpReadSelectReply = 0;
    return 0;
  }
  struct timeval timeout;
  timeout.tv_sec  = timeOutMillis / 1000;
  timeout.tv_usec = (timeOutMillis % 1000) * 1000;


  NDB_SOCKET_TYPE maxSocketValue = 0;
  
  // Needed for TCP/IP connections
  // The read- and writeset are used by select
  
  FD_ZERO(&tcpReadset);

  // Prepare for sending and receiving
  for (Uint32 i = 0; i < m_nSocketClients; i++) {
    SocketClient * t = m_socketClients[i];
    
    // If the socketclient is connected
    if (t->isConnected()) {
      
      const NDB_SOCKET_TYPE socket = t->getSocket();
      // Find the highest socket value. It will be used by select
      if (socket > maxSocketValue)
	maxSocketValue = socket;
      
      // Put the connected transporters in the socket read-set 
      FD_SET(socket, &tcpReadset);
    }
  }
  
  // The highest socket value plus one
  maxSocketValue++; 
  
  tcpReadSelectReply = select(maxSocketValue, &tcpReadset, 0, 0, &timeout);  
#ifdef NDB_WIN32
  if(tcpReadSelectReply == SOCKET_ERROR)
  {
    NdbSleep_MilliSleep(timeOutMillis);
  }
#endif

  return tcpReadSelectReply;

}

template<class T>
bool 
SocketRegistry<T>::performSend(const char * buf, 
			       Uint32 len, 
			       const char * remotehost)
{
  SocketClient * socketClient;
  for(Uint32 i=0; i < m_nSocketClients; i++) {
    socketClient = m_socketClients[i];
    if(strcmp(socketClient->gethostname(), remotehost)==0) {
      if(socketClient->isConnected()) {
	if(socketClient->writeSocket(buf, len)>0)
	  return true;
	else
	  return false;
      }
    }
  }
  return false;
}

template<class T>
int 
SocketRegistry<T>::performReceive(T & t) {  
  char buf[255] ; //temp. just for testing. must fix better

  if(tcpReadSelectReply > 0){
    for (Uint32 i=0; i<m_nSocketClients; i++) {
      SocketClient *sc = m_socketClients[i];
      const NDB_SOCKET_TYPE socket    = sc->getSocket();
      if(sc->isConnected() && FD_ISSET(socket, &tcpReadset)) {
	t->runSession(socket,t);
      }
    }
    return 1;
  }
  return 0;
  
}



template<class T>
inline
int 
SocketRegistry<T>::syncPerformReceive(char * host,
				      T & t,
				      Uint32 timeOutMillis) {  
  char buf[255] ; //temp. just for testing. must fix better
  struct timeval timeout;
  timeout.tv_sec  = timeOutMillis / 1000;
  timeout.tv_usec = (timeOutMillis % 1000) * 1000;
  int reply;
  SocketClient * sc;
  for(Uint32 i=0; i < m_nSocketClients; i++) {
    sc = m_socketClients[i];
    if(strcmp(sc->gethostname(), remotehost)==0) {
      if(sc->isConnected()) {
	FD_ZERO(&tcpReadset);
	reply = select(sc->getSocket(), &tcpReadset, 0, 0, &timeout);
	if(reply > 0) {
	  return t->runSession(sc->getSocket(), t);
	}
      }
      
    }
  }
  return 0;
}



template<class T>
bool 
SocketRegistry<T>::reconnect(const char * host){
  for(Uint32 i=0; i < m_nSocketClients; i++) {
    SocketClient * socketClient = m_socketClients[i];
    if(strcmp(socketClient->gethostname(), host)==0) {
      if(!socketClient->isConnected()) {
	if(socketClient->openSocket() > 0)
	  return true;
	else return false;
      }
    }
  }
  return false;
}

template<class T>
bool 
SocketRegistry<T>::removeSocketClient(const char * host){
  for(Uint32 i=0; i < m_nSocketClients; i++) {
    SocketClient * socketClient = m_socketClients[i];
    if(strcmp(socketClient->gethostname(), host)==0) {
      if(!socketClient->isConnected()) {
	if(socketClient->closeSocket() > 0) {
	  delete socketClient;
	  return true;
	}
	else return false;
      }
    }
  }
  return false;
}
