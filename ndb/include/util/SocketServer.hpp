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
    virtual void stopSession(){}
  protected:
    friend class SocketServer;
    friend void* sessionThread_C(void*);
    Session(NDB_SOCKET_TYPE sock): m_socket(sock){ m_stop = m_stopped = false;}
    
    bool m_stop;    // Has the session been ordered to stop?
    bool m_stopped; // Has the session stopped?
    
    NDB_SOCKET_TYPE m_socket;
  };
  
  /**
   * A service i.e. a session factory
   */
  class Service {
  public:
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
  SocketServer(int maxSessions = 32);
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
  bool setup(Service *, unsigned short port, const char * pinterface = 0);
  
  /**
   * start/stop the server
   */
  void startServer();
  void stopServer();
  
  /**
   * stop sessions
   *
   * Note: Implies stopServer
   */
  void stopSessions(bool wait = false);
  
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
  MutexVector<SessionInstance> m_sessions;
  MutexVector<ServiceInstance> m_services;
  unsigned m_maxSessions;
  
  void doAccept();
  void checkSessions();
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
