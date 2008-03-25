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


#include <ndb_global.h>
#include <my_pthread.h>

#include <SocketServer.hpp>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>

#define DEBUG(x) ndbout << x << endl;

SocketServer::SocketServer(unsigned maxSessions) :
  m_sessions(10),
  m_services(5)
{
  m_thread = 0;
  m_stopThread = false;
  m_maxSessions = maxSessions;
}

SocketServer::~SocketServer() {
  unsigned i;
  for(i = 0; i<m_sessions.size(); i++){
    delete m_sessions[i].m_session;
  }
  for(i = 0; i<m_services.size(); i++){
    if(m_services[i].m_socket)
      NDB_CLOSE_SOCKET(m_services[i].m_socket);
    delete m_services[i].m_service;
  }
}

bool
SocketServer::tryBind(unsigned short port, const char * intface) {
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  
  if(intface != 0){
    if(Ndb_getInAddr(&servaddr.sin_addr, intface))
      return false;
  }
  
  const NDB_SOCKET_TYPE sock  = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == NDB_INVALID_SOCKET) {
    return false;
  }
  
  DBUG_PRINT("info",("NDB_SOCKET: %d", sock));

  const int on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		 (const char*)&on, sizeof(on)) == -1) {
    NDB_CLOSE_SOCKET(sock);
    return false;
  }
  
  if (bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr)) == -1) {
    NDB_CLOSE_SOCKET(sock);
    return false;
  }

  NDB_CLOSE_SOCKET(sock);
  return true;
}

bool
SocketServer::setup(SocketServer::Service * service, 
		    unsigned short * port,
		    const char * intface){
  DBUG_ENTER("SocketServer::setup");
  DBUG_PRINT("enter",("interface=%s, port=%u", intface, *port));
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(*port);
  
  if(intface != 0){
    if(Ndb_getInAddr(&servaddr.sin_addr, intface))
      DBUG_RETURN(false);
  }
  
  const NDB_SOCKET_TYPE sock  = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == NDB_INVALID_SOCKET) {
    DBUG_PRINT("error",("socket() - %d - %s",
			errno, strerror(errno)));
    DBUG_RETURN(false);
  }
  
  DBUG_PRINT("info",("NDB_SOCKET: %d", sock));
 
  const int on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		 (const char*)&on, sizeof(on)) == -1) {
    DBUG_PRINT("error",("setsockopt() - %d - %s",
			errno, strerror(errno)));
    NDB_CLOSE_SOCKET(sock);
    DBUG_RETURN(false);
  }
  
  if (bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr)) == -1) {
    DBUG_PRINT("error",("bind() - %d - %s",
			errno, strerror(errno)));
    NDB_CLOSE_SOCKET(sock);
    DBUG_RETURN(false);
  }

  /* Get the port we bound to */
  SOCKET_SIZE_TYPE sock_len = sizeof(servaddr);
  if(getsockname(sock,(struct sockaddr*)&servaddr,&sock_len)<0) {
    ndbout_c("An error occurred while trying to find out what"
	     " port we bound to. Error: %s",strerror(errno));
    NDB_CLOSE_SOCKET(sock);
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info",("bound to %u",ntohs(servaddr.sin_port)));
  if (listen(sock, m_maxSessions > 32 ? 32 : m_maxSessions) == -1){
    DBUG_PRINT("error",("listen() - %d - %s",
			errno, strerror(errno)));
    NDB_CLOSE_SOCKET(sock);
    DBUG_RETURN(false);
  }
  
  ServiceInstance i;
  i.m_socket = sock;
  i.m_service = service;
  m_services.push_back(i);

  *port = ntohs(servaddr.sin_port);

  DBUG_RETURN(true);
}

void
SocketServer::doAccept(){
  fd_set readSet, exceptionSet;
  FD_ZERO(&readSet);
  FD_ZERO(&exceptionSet);
  
  m_services.lock();
  int maxSock = 0;
  for (unsigned i = 0; i < m_services.size(); i++){
    const NDB_SOCKET_TYPE s = m_services[i].m_socket;
    FD_SET(s, &readSet);
    FD_SET(s, &exceptionSet);
    maxSock = (maxSock > s ? maxSock : s);
  }
  struct timeval timeout;
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;
  
  if(select(maxSock + 1, &readSet, 0, &exceptionSet, &timeout) > 0){
    for (unsigned i = 0; i < m_services.size(); i++){
      ServiceInstance & si = m_services[i];
      
      if(FD_ISSET(si.m_socket, &readSet)){
	NDB_SOCKET_TYPE childSock = accept(si.m_socket, 0, 0);
	if(childSock == NDB_INVALID_SOCKET){
	  continue;
	}
	
	SessionInstance s;
	s.m_service = si.m_service;
	s.m_session = si.m_service->newSession(childSock);
	if(s.m_session != 0)
	{
	  m_session_mutex.lock();
	  m_sessions.push_back(s);
	  startSession(m_sessions.back());
	  m_session_mutex.unlock();
	}
	
	continue;
      }      
      
      if(FD_ISSET(si.m_socket, &exceptionSet)){
	DEBUG("socket in the exceptionSet");
	continue;
      }
    }
  }
  m_services.unlock();
}

extern "C"
void* 
socketServerThread_C(void* _ss){
  SocketServer * ss = (SocketServer *)_ss;
  ss->doRun();
  return 0;
}

void
SocketServer::startServer(){
  m_threadLock.lock();
  if(m_thread == 0 && m_stopThread == false){
    m_thread = NdbThread_Create(socketServerThread_C,
				(void**)this,
				32768,
				"NdbSockServ",
				NDB_THREAD_PRIO_LOW);
  }
  m_threadLock.unlock();
}

void
SocketServer::stopServer(){
  m_threadLock.lock();
  if(m_thread != 0){
    m_stopThread = true;
    
    void * res;
    NdbThread_WaitFor(m_thread, &res);
    NdbThread_Destroy(&m_thread);
    m_thread = 0;
  }
  m_threadLock.unlock();
}

void
SocketServer::doRun(){

  while(!m_stopThread){
    m_session_mutex.lock();
    checkSessionsImpl();
    if(m_sessions.size() < m_maxSessions){
      m_session_mutex.unlock();
      doAccept();
    } else {
      m_session_mutex.unlock();
      NdbSleep_MilliSleep(200);
    }
  }
}

void
SocketServer::startSession(SessionInstance & si){
  si.m_thread = NdbThread_Create(sessionThread_C,
				 (void**)si.m_session,
				 32768,
				 "NdbSock_Session",
				 NDB_THREAD_PRIO_LOW);
}

void
SocketServer::foreachSession(void (*func)(SocketServer::Session*, void *), void *data)
{
  m_session_mutex.lock();
  for(int i = m_sessions.size() - 1; i >= 0; i--){
    (*func)(m_sessions[i].m_session, data);
  }
  m_session_mutex.unlock();
}

void
SocketServer::checkSessions()
{
  m_session_mutex.lock();
  checkSessionsImpl();
  m_session_mutex.unlock();  
}

void
SocketServer::checkSessionsImpl()
{
  for(int i = m_sessions.size() - 1; i >= 0; i--)
  {
    if(m_sessions[i].m_session->m_stopped)
    {
      if(m_sessions[i].m_thread != 0)
      {
	void* ret;
	NdbThread_WaitFor(m_sessions[i].m_thread, &ret);
	NdbThread_Destroy(&m_sessions[i].m_thread);
      } 
      m_sessions[i].m_session->stopSession();
      delete m_sessions[i].m_session;
      m_sessions.erase(i);
    }
  }
}

void
SocketServer::stopSessions(bool wait){
  int i;
  m_session_mutex.lock();
  for(i = m_sessions.size() - 1; i>=0; i--)
  {
    m_sessions[i].m_session->stopSession();
    m_sessions[i].m_session->m_stop = true; // to make sure
  }
  m_session_mutex.unlock();
  
  for(i = m_services.size() - 1; i>=0; i--)
    m_services[i].m_service->stopSessions();
  
  if(wait){
    m_session_mutex.lock();
    while(m_sessions.size() > 0){
      checkSessionsImpl();
      m_session_mutex.unlock();
      NdbSleep_MilliSleep(100);
      m_session_mutex.lock();
    }
    m_session_mutex.unlock();
  }
}

/***** Session code ******/

extern "C"
void* 
sessionThread_C(void* _sc){
  SocketServer::Session * si = (SocketServer::Session *)_sc;

  /**
   * may have m_stopped set if we're transforming a mgm
   * connection into a transporter connection.
   */
  if(!si->m_stopped)
  {
    if(!si->m_stop){
      si->m_stopped = false;
      si->runSession();
    } else {
      NDB_CLOSE_SOCKET(si->m_socket);
    }
  }
  
  si->m_stopped = true;
  return 0;
}

template class MutexVector<SocketServer::ServiceInstance>;
template class Vector<SocketServer::SessionInstance>;
