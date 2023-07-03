/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>

#include <SocketServer.hpp>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include "ndb_socket.h"
#include <OwnProcessInfo.hpp>
#include <EventLogger.hpp>

#if 0
#define DEBUG_FPRINTF(arglist) do { fprintf arglist ; } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

SocketServer::SocketServer(unsigned maxSessions) :
  m_sessions(10),
  m_services(5),
  m_maxSessions(maxSessions),
  m_stopThread(false),
  m_thread(nullptr)
{
}

SocketServer::~SocketServer() {
  unsigned i;
  for(i = 0; i<m_sessions.size(); i++){
    Session* session= m_sessions[i].m_session;
    assert(session->m_refCount == 0);
    delete session;
  }
  for(i = 0; i<m_services.size(); i++){
    if(ndb_socket_valid(m_services[i].m_socket))
      ndb_socket_close(m_services[i].m_socket);
    delete m_services[i].m_service;
  }
}

bool SocketServer::tryBind(unsigned short port, const char* intface,
                           char* error, size_t error_size) {
  struct sockaddr_in6 servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_addr = in6addr_any;
  servaddr.sin6_port = htons(port);

  if(intface != nullptr){
    if(Ndb_getInAddr6(&servaddr.sin6_addr, intface))
      return false;
  }

  const ndb_socket_t sock =
      ndb_socket_create_dual_stack(SOCK_STREAM, 0);
  if (!ndb_socket_valid(sock))
    return false;

  DBUG_PRINT("info",("NDB_SOCKET: %s", ndb_socket_to_string(sock).c_str()));

  if (ndb_socket_configure_reuseaddr(sock, true) == -1)
  {
    ndb_socket_close(sock);
    return false;
  }

  if (ndb_bind_inet(sock, &servaddr) == -1) {
    if (error != nullptr) {
      int err_code = ndb_socket_errno();
      snprintf(error, error_size, "%d '%s'", err_code,
               ndb_socket_err_message(err_code).c_str());
    }
    ndb_socket_close(sock);
    return false;
  }

  ndb_socket_close(sock);
  return true;
}

#define MAX_SOCKET_SERVER_TCP_BACKLOG 64
bool
SocketServer::setup(SocketServer::Service * service,
        unsigned short * port,
        const char * intface){
  DBUG_ENTER("SocketServer::setup");
  DBUG_PRINT("enter",("interface=%s, port=%u", intface, *port));
  struct sockaddr_in6 servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_addr = in6addr_any;
  servaddr.sin6_port = htons(*port);

  if(intface != nullptr){
    if(Ndb_getInAddr6(&servaddr.sin6_addr, intface))
      DBUG_RETURN(false);
  }

  const ndb_socket_t sock =
      ndb_socket_create_dual_stack(SOCK_STREAM, 0);
  if (!ndb_socket_valid(sock))
  {
    DBUG_PRINT("error",("socket() - %d - %s",
      socket_errno, strerror(socket_errno)));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info",("NDB_SOCKET: %s", ndb_socket_to_string(sock).c_str()));

  if (ndb_socket_reuseaddr(sock, true) == -1)
  {
    DBUG_PRINT("error",("setsockopt() - %d - %s",
      errno, strerror(errno)));
    ndb_socket_close(sock);
    DBUG_RETURN(false);
  }

  if (ndb_bind_inet(sock, &servaddr) == -1) {
    DBUG_PRINT("error",("bind() - %d - %s",
      socket_errno, strerror(socket_errno)));
    ndb_socket_close(sock);
    DBUG_RETURN(false);
  }

  /* Get the address and port we bound to */
  struct sockaddr_in6 serv_addr;
  if(ndb_getsockname(sock, &serv_addr))
  {
    g_eventLogger->info(
        "An error occurred while trying to find out what port we bound to."
        " Error: %d - %s",
        ndb_socket_errno(), strerror(ndb_socket_errno()));
    ndb_socket_close(sock);
    DBUG_RETURN(false);
  }
  *port = ntohs(serv_addr.sin6_port);
  setOwnProcessInfoServerAddress((sockaddr*)& serv_addr);

  DBUG_PRINT("info",("bound to %u", *port));

  if (ndb_listen(sock, m_maxSessions > MAX_SOCKET_SERVER_TCP_BACKLOG ?
                      MAX_SOCKET_SERVER_TCP_BACKLOG : m_maxSessions) == -1)
  {
    DBUG_PRINT("error",("listen() - %d - %s",
      socket_errno, strerror(socket_errno)));
    ndb_socket_close(sock);
    DBUG_RETURN(false);
  }

  DEBUG_FPRINTF((stderr, "Listening on port: %u\n",
                (Uint32)*port));

  ServiceInstance i;
  i.m_socket = sock;
  i.m_service = service;
  m_services.push_back(i);

  // Increase size to allow polling all listening ports
  m_services_poller.set_max_count(m_services.size());

  DBUG_RETURN(true);
}


bool
SocketServer::doAccept()
{
  m_services.lock();

  m_services_poller.clear();
  for (unsigned i = 0; i < m_services.size(); i++)
  {
    m_services_poller.add_readable(m_services[i].m_socket); // Need error ??
  }
  assert(m_services.size() == m_services_poller.count());

  const int accept_timeout_ms = 1000;
  const int ret = m_services_poller.poll(accept_timeout_ms);
  if (ret < 0)
  {
    // Error occurred, indicate error to caller by returning false
    m_services.unlock();
    return false;
  }

  if (ret == 0)
  {
    // Timeout occurred
    m_services.unlock();
    return true;
  }

  bool result = true;
  for (unsigned i = 0; i < m_services_poller.count(); i++)
  {
    const bool has_read = m_services_poller.has_read(i);

    if (!has_read)
      continue; // Ignore events where read flag wasn't set

    ServiceInstance & si = m_services[i];
    assert(m_services_poller.is_socket_equal(i, si.m_socket));

    const ndb_socket_t childSock = ndb_accept(si.m_socket, nullptr, nullptr);
    if (!ndb_socket_valid(childSock))
    {
      // Could not 'accept' socket(maybe at max fds), indicate error
      // to caller by returning false
      result = false;
      continue;
    }

    SessionInstance s;
    s.m_service = si.m_service;
    s.m_session = si.m_service->newSession(childSock);
    if (s.m_session != nullptr)
    {
      m_session_mutex.lock();
      m_sessions.push_back(s);
      startSession(m_sessions.back());
      m_session_mutex.unlock();
    }
  }

  m_services.unlock();
  return result;
}

extern "C"
void* 
socketServerThread_C(void* _ss){
  SocketServer * ss = (SocketServer *)_ss;
  ss->doRun();
  return nullptr;
}

struct NdbThread*
SocketServer::startServer()
{
  m_threadLock.lock();
  if(m_thread == nullptr && m_stopThread == false)
  {
    m_thread = NdbThread_Create(socketServerThread_C,
				(void**)this,
                                0, // default stack size
				"NdbSockServ",
				NDB_THREAD_PRIO_LOW);
  }
  m_threadLock.unlock();
  return m_thread;
}

void
SocketServer::stopServer(){
  m_threadLock.lock();
  if(m_thread != nullptr){
    m_stopThread = true;
    
    void * res;
    NdbThread_WaitFor(m_thread, &res);
    NdbThread_Destroy(&m_thread);
    m_thread = nullptr;
  }
  m_threadLock.unlock();
}

void
SocketServer::doRun(){

  while(!m_stopThread){
    m_session_mutex.lock();
    checkSessionsImpl();
    m_session_mutex.unlock();

    if(m_sessions.size() >= m_maxSessions){
      // Don't accept more connections yet
      DEBUG_FPRINTF((stderr, "Too many connections\n"));
      NdbSleep_MilliSleep(200);
      continue;
    }

    if (!doAccept()){
      // accept failed, step back
      DEBUG_FPRINTF((stderr, "Accept failed\n"));
      NdbSleep_MilliSleep(200);
    }
  }
}

void
SocketServer::startSession(SessionInstance & si){
  si.m_thread = NdbThread_Create(sessionThread_C,
				 (void**)si.m_session,
                                 0, // default stack size
				 "NdbSock_Session",
				 NDB_THREAD_PRIO_LOW);
}

void
SocketServer::foreachSession(void (*func)(SocketServer::Session*, void *),
                             void *data)
{
  // Build a list of pointers to all active sessions
  // and increase refcount on the sessions
  m_session_mutex.lock();
  Vector<Session*> session_pointers(m_sessions.size());
  for(unsigned i= 0; i < m_sessions.size(); i++){
    Session* session= m_sessions[i].m_session;
    session_pointers.push_back(session);
    session->m_refCount++;
  }
  m_session_mutex.unlock();

  // Call the function on each session
  for(unsigned i= 0; i < session_pointers.size(); i++){
    (*func)(session_pointers[i], data);
  }

  // Release the sessions pointers and any stopped sessions
  m_session_mutex.lock();
  for(unsigned i= 0; i < session_pointers.size(); i++){
    Session* session= session_pointers[i];
    assert(session->m_refCount > 0);
    session->m_refCount--;
  }
  checkSessionsImpl();
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
    if(m_sessions[i].m_session->m_thread_stopped &&
       (m_sessions[i].m_session->m_refCount == 0))
    {
      if(m_sessions[i].m_thread != nullptr)
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

bool
SocketServer::stopSessions(bool wait, unsigned wait_timeout){
  int i;
  m_session_mutex.lock();
  for(i = m_sessions.size() - 1; i>=0; i--)
  {
    m_sessions[i].m_session->stopSession();
  }
  m_session_mutex.unlock();
  
  for(i = m_services.size() - 1; i>=0; i--)
    m_services[i].m_service->stopSessions();
  
  if(!wait)
    return false; // No wait

  const NDB_TICKS start = NdbTick_getCurrentTicks();
  m_session_mutex.lock();
  while(m_sessions.size() > 0){
    checkSessionsImpl();
    m_session_mutex.unlock();

    if (wait_timeout > 0 &&
        NdbTick_Elapsed(start,NdbTick_getCurrentTicks()).milliSec() > wait_timeout)
      return false; // Wait abandoned

    NdbSleep_MilliSleep(100);
    m_session_mutex.lock();
  }
  m_session_mutex.unlock();
  return true; // All sessions gone
}


/***** Session code ******/

extern "C"
void* 
sessionThread_C(void* _sc){
  SocketServer::Session * si = (SocketServer::Session *)_sc;

  assert(si->m_thread_stopped == false);

  if(!si->m_stop)
    si->runSession();
  else
  {
    ndb_socket_close(si->m_socket);
    ndb_socket_invalidate(&si->m_socket);
  }

  // Mark the thread as stopped to allow the
  // session resources to be released
  si->m_thread_stopped = true;
  return nullptr;
}

template class MutexVector<SocketServer::ServiceInstance>;
template class Vector<SocketServer::SessionInstance>;
template class Vector<SocketServer::Session*>;
