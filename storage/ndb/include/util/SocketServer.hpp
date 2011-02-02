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

#ifndef SOCKET_SERVER_HPP
#define SOCKET_SERVER_HPP

#include <NdbTCP.h>
#include <NdbMutex.h>
#include <NdbThread.h>
#include <Vector.hpp>

extern "C" void* sessionThread_C(void*);
extern "C" void* socketServerThread_C(void*);

/**
 *  Socket Server
 */
class SocketServer {
public:
  /**
   * A Session 
   */
  class Session {
  public:
    virtual ~Session() {}
    virtual void runSession(){}
    virtual void stopSession(){ m_stop = true; }
  protected:
    friend class SocketServer;
    friend void* sessionThread_C(void*);
    Session(NDB_SOCKET_TYPE sock) :
      m_stop(false),
      m_socket(sock),
      m_refCount(0),
      m_thread_stopped(false)
      {
	DBUG_ENTER("SocketServer::Session");
	DBUG_PRINT("enter",("NDB_SOCKET: " MY_SOCKET_FORMAT,
                            MY_SOCKET_FORMAT_VALUE(m_socket)));
	DBUG_VOID_RETURN;
      }
    bool m_stop;    // Has the session been ordered to stop?
    NDB_SOCKET_TYPE m_socket;
    unsigned m_refCount;
  private:
    bool m_thread_stopped; // Has the session thread stopped?
  };
  
  /**
   * A service i.e. a session factory
   */
  class Service {
  public:
    Service() {}
    virtual ~Service(){}
    
    /**
     * Returned Session will be ran in own thread
     *
     * To manage threads self, just return NULL
     */
    virtual Session * newSession(NDB_SOCKET_TYPE theSock) = 0;
    virtual void stopSessions(){}
  };
  
  /**
   * Constructor / Destructor
   */
  SocketServer(unsigned maxSessions = ~(unsigned)0);
  ~SocketServer();
  
  /**
   * Setup socket and bind it
   *  then  close the socket
   * Returns true if succeding in binding
   */
  static bool tryBind(unsigned short port, const char * intface = 0);

  /**
   * Setup socket
   *   bind & listen
   * Returns false if no success
   */
  bool setup(Service *, unsigned short *port, const char * pinterface = 0);
  
  /**
   * start/stop the server
   */
  struct NdbThread* startServer();
  void stopServer();
  
  /**
   * stop sessions
   *
   * Note: Implies previous stopServer
   *
   * wait, wait until all sessions has stopped if true
   * wait_timeout - wait, but abort wait after this
   *                time(in milliseconds)
   *                0 = infinite
   *
   * returns false if wait was abandoned
   *
   */
  bool stopSessions(bool wait = false, unsigned wait_timeout = 0);
  
  void foreachSession(void (*f)(Session*, void*), void *data);
  void checkSessions();
  
private:
  struct SessionInstance {
    Service * m_service;
    Session * m_session;
    NdbThread * m_thread;
  };
  struct ServiceInstance {
    Service * m_service;
    NDB_SOCKET_TYPE m_socket;
  };
  NdbLockable m_session_mutex;
  Vector<SessionInstance> m_sessions;
  MutexVector<ServiceInstance> m_services;
  ndb_socket_poller m_services_poller;
  unsigned m_maxSessions;

  bool doAccept();
  void checkSessionsImpl();
  void startSession(SessionInstance &);
  
  /**
   * Note, this thread is only used when running interactive
   * 
   */
  bool m_stopThread;
  struct NdbThread * m_thread;
  NdbLockable m_threadLock;
  void doRun();
  friend void* socketServerThread_C(void*);
  friend void* sessionThread_C(void*);
};

#endif
