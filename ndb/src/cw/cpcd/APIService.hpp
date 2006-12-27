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

#ifndef CPCD_API_HPP
#define CPCD_API_HPP

#include <Parser.hpp>
#include <InputStream.hpp>
#include <SocketServer.hpp>

class CPCD;

class CPCDAPISession : public SocketServer::Session {
  typedef Parser<CPCDAPISession> Parser_t;

  class CPCD & m_cpcd;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;

  Vector<int> m_temporaryProcesses;
  
  void printProperty(Properties *prop, const char *key);
public:
  CPCDAPISession(NDB_SOCKET_TYPE, class CPCD &);
  CPCDAPISession(FILE * f, CPCD & cpcd);
  ~CPCDAPISession();

  virtual void runSession();
  virtual void stopSession();
  void loadFile();
  
  void defineProcess(Parser_t::Context & ctx, const class Properties & args);
  void undefineProcess(Parser_t::Context & ctx, const class Properties & args);
  void startProcess(Parser_t::Context & ctx, const class Properties & args);
  void stopProcess(Parser_t::Context & ctx, const class Properties & args);
  void showProcess(Parser_t::Context & ctx, const class Properties & args);
  void listProcesses(Parser_t::Context & ctx, const class Properties & args);
  void showVersion(Parser_t::Context & ctx, const class Properties & args);
};

class CPCDAPIService : public SocketServer::Service {
  class CPCD & m_cpcd;
public:
  CPCDAPIService(class CPCD & cpcd) : m_cpcd(cpcd) {}

  CPCDAPISession * newSession(NDB_SOCKET_TYPE theSock){
    return new CPCDAPISession(theSock, m_cpcd);
  }
};

#endif
