/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CPCD_API_HPP
#define CPCD_API_HPP

#include <InputStream.hpp>
#include <Parser.hpp>
#include <SocketServer.hpp>

class CPCD;

class CPCDAPISession : public SocketServer::Session {
  typedef Parser<CPCDAPISession> Parser_t;

  class CPCD &m_cpcd;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;
  Uint32 m_protocol_version;

  Vector<int> m_temporaryProcesses;

  void printProperty(Properties *prop, const char *key);
  void printLongString(const char *key, const char *value);

 public:
  CPCDAPISession(NDB_SOCKET_TYPE, class CPCD &);
  CPCDAPISession(FILE *f, CPCD &cpcd);
  ~CPCDAPISession();

  virtual void runSession();
  virtual void stopSession();
  void loadFile();

  void defineProcess(Parser_t::Context &ctx, const class Properties &args);
  void undefineProcess(Parser_t::Context &ctx, const class Properties &args);
  void startProcess(Parser_t::Context &ctx, const class Properties &args);
  void stopProcess(Parser_t::Context &ctx, const class Properties &args);
  void showProcess(Parser_t::Context &ctx, const class Properties &args);
  void listProcesses(Parser_t::Context &ctx, const class Properties &args);
  void showVersion(Parser_t::Context &ctx, const class Properties &args);
  void selectProtocol(Parser_t::Context &ctx, const class Properties &args);

  bool may_print_process_cpuset() const { return m_protocol_version >= 2; }
};

class CPCDAPIService : public SocketServer::Service {
  class CPCD &m_cpcd;

 public:
  CPCDAPIService(class CPCD &cpcd) : m_cpcd(cpcd) {}

  CPCDAPISession *newSession(NDB_SOCKET_TYPE theSock) {
    return new CPCDAPISession(theSock, m_cpcd);
  }
};

#endif
