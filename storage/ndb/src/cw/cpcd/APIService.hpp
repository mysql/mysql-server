/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include "util/NdbSocket.h"

class CPCD;

class CPCDAPISession : public SocketServer::Session {
  typedef Parser<CPCDAPISession> Parser_t;

  class CPCD &m_cpcd;
  NdbSocket m_secure_socket;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;
  Uint32 m_protocol_version;

  Vector<int> m_temporaryProcesses;

  void printProperty(Properties *prop, const char *key);
  void printLongString(const char *key, const char *value);

 public:
  CPCDAPISession(NdbSocket &&sock, class CPCD &);
  CPCDAPISession(FILE *f, CPCD &cpcd);
  ~CPCDAPISession() override;

  void runSession() override;
  void stopSession() override;
  void loadFile();

  uintptr_t getSessionid() const { return reinterpret_cast<uintptr_t>(this); }

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

  CPCDAPISession *newSession(NdbSocket &&theSock) override {
    return new CPCDAPISession(std::move(theSock), m_cpcd);
  }
};

#endif
