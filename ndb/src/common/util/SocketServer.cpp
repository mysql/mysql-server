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


#include <ndb_global.h>
#include <my_pthread.h>

#include <SocketServer.hpp>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>

#define DEBUG(x) ndbout << x << endl;

SocketServer::SocketServer(int maxSessions) :
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
		    unsigned short port, 
		    const char * intface){
  DBUG_ENTER("SocketServer::setup");
  DBUG_PRINT("enter",("interface=%s, port=%d", intface, port));
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  
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
  
  const int on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		 (const char*)&on, sizeof(on)) == -1) {
    DBUG_PRINT("error",("getsockopt() - %d - %s",
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
  
  if (listen(sock, m_maxSessions) == -1){
    DBUG_PRINT("error",("listen() - %d - %s",
			errno, strerror(errno)));
    NDB_CLOSE_SOCKET(sock);
    DBUG_RETURN(false);
  }
  
  ServiceInstance i;
  i.m_socket = sock;
  i.m_service = service;
  m_services.push_back(i);
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
	if(s.m_session != 0){
	  m_sessions.push_back(s);
	  startSession(m_sessions.back());
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
  
  my_thread_init();
  ss->doRun();
  my_thread_end();  
  NdbThread_Exit(0);
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
    checkSessions();
    if(m_sessions.size() < m_maxSessions){
      doAccept();
    } else {
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

static
bool 
transfer(NDB_SOCKET_TYPE sock){
#if defined NDB_OSE || defined NDB_SOFTOSE
  const PROCESS p = current_process();
  const size_t ps = sizeof(PROCESS);
  int res = setsockopt(sock, SOL_SOCKET, SO_OSEOWNER, &p, ps);
  if(res != 0){
    ndbout << "Failed to transfer ownership of socket" << endl;
    return false;
  }
#endif
  return true;
}

void
SocketServer::foreachSession(void (*func)(SocketServer::Session*, void *), void *data)
{
  for(int i = m_sessions.size() - 1; i >= 0; i--){
    (*func)(m_sessions[i].m_session, data);
  }
  checkSessions();
}

void
SocketServer::checkSessions(){
  for(int i = m_sessions.size() - 1; i >= 0; i--){
    if(m_sessions[i].m_session->m_stopped){
      if(m_sessions[i].m_thread != 0){
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
  for(i = m_sessions.size() - 1; i>=0; i--)
  {
    m_sessions[i].m_session->stopSession();
    m_sessions[i].m_session->m_stop = true; // to make sure
  }
  for(i = m_services.size() - 1; i>=0; i--)
    m_services[i].m_service->stopSessions();
  
  if(wait){
    while(m_sessions.size() > 0){
      checkSessions();
      NdbSleep_MilliSleep(100);
    }
  }
}

/***** Session code ******/

extern "C"
void* 
sessionThread_C(void* _sc){
  SocketServer::Session * si = (SocketServer::Session *)_sc;

  my_thread_init();
  if(!transfer(si->m_socket)){
    si->m_stopped = true;
    my_thread_end();
    NdbThread_Exit(0);
    return 0;
  }
  
  if(!si->m_stop){
    si->m_stopped = false;
    si->runSession();
  } else {
    NDB_CLOSE_SOCKET(si->m_socket);
  }
  
  si->m_stopped = true;
  my_thread_end();
  NdbThread_Exit(0);
  return 0;
}

template class MutexVector<SocketServer::ServiceInstance>;
template class MutexVector<SocketServer::SessionInstance>;
