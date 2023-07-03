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

#ifndef SOCKET_SERVER_HPP
#define SOCKET_SERVER_HPP

#include "portlib/NdbMutex.h"
#include "portlib/NdbThread.h"
#include "portlib/ndb_socket_poller.h"

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
    Session(ndb_socket_t sock) :
      m_stop(false),
      m_refCount(0),
      m_socket(sock),
      m_thread_stopped(false)
      {
	DBUG_ENTER("SocketServer::Session");
	DBUG_PRINT("enter",("NDB_SOCKET: %s",
                            ndb_socket_to_string(m_socket).c_str()));
	DBUG_VOID_RETURN;
      }
    bool m_stop;    // Has the session been ordered to stop?
    unsigned m_refCount;
  private:
    ndb_socket_t m_socket;
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
    virtual Session * newSession(ndb_socket_t theSock) = 0;
    virtual void stopSessions(){}
  };
  
  /**
   * Constructor / Destructor
   */
  SocketServer(unsigned maxSessions = ~(unsigned)0);
  ~SocketServer();
  
  /**
   * Set up socket and bind it
   *  then  close the socket
   * Returns true if succeeding in binding
   */
  static bool tryBind(unsigned short port, const char* intface = nullptr,
                      char* error = nullptr, size_t error_size = 0);

  /**
   * Setup socket
   *   bind & listen
   * Returns false if no success
   */
  bool setup(Service *, unsigned short *port, 
             const char * pinterface = nullptr);
  
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
    ndb_socket_t m_socket;
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
