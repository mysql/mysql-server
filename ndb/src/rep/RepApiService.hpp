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

#ifndef REP_APISERVICE_HPP
#define REP_APISERVICE_HPP

#include <Parser.hpp>
#include <InputStream.hpp>
#include <SocketServer.hpp>

class RepApiInterpreter;

class RepApiSession : public SocketServer::Session {
  typedef Parser<RepApiSession> Parser_t;

  class RepApiInterpreter & m_rep;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;
  
void printProperty(Properties *prop, const char *key);
public:
  RepApiSession(NDB_SOCKET_TYPE, class RepApiInterpreter &);
  RepApiSession(FILE * f, class RepApiInterpreter & rep);
  ~RepApiSession();

  virtual void runSession();
  virtual void stopSession();
  
  void execCommand(Parser_t::Context & ctx, const class Properties & args);
  void getStatus(Parser_t::Context & ctx, const class Properties & args);
  void query(Parser_t::Context & ctx, const class Properties & args);

};

class RepApiService : public SocketServer::Service {
  class RepApiInterpreter & m_rep;
public:
  RepApiService(class RepApiInterpreter & rep) : m_rep(rep) {}
  
  RepApiSession * newSession(NDB_SOCKET_TYPE theSock){
    return new RepApiSession(theSock, m_rep);
  }
};

#endif
