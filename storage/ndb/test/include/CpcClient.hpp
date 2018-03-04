/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __CPCCLIENT_HPP_INCLUDED__
#define __CPCCLIENT_HPP_INCLUDED__

#include <Parser.hpp>
#include <SocketServer.hpp>
#include <util/InputStream.hpp>
#include <util/OutputStream.hpp>

/*
  Simple CPC client class.
*/
class SimpleCpcClient {
 public:
  SimpleCpcClient(const char *host, int port);
  ~SimpleCpcClient();

  int getPort() const { return port; }
  const char *getHost() const { return host; }

  struct Process {
    int m_id;
    BaseString m_name;

    BaseString m_owner;
    BaseString m_group;
    BaseString m_runas;
    BaseString m_cpuset;

    BaseString m_cwd;
    BaseString m_env;
    BaseString m_path;
    BaseString m_args;

    BaseString m_type;
    BaseString m_status;

    BaseString m_stdin;
    BaseString m_stdout;
    BaseString m_stderr;
    BaseString m_ulimit;
    BaseString m_shutdown_options;
  };

 private:
  class ParserDummy : SocketServer::Session {
   public:
    ParserDummy(NDB_SOCKET_TYPE sock);
  };

  typedef Parser<ParserDummy> Parser_t;
  typedef ParserRow<ParserDummy> ParserRow_t;

  char *host;
  int port;
  NDB_SOCKET_TYPE cpc_sock;

  enum { CPC_PROTOCOL_VERSION = 2 };

  Uint32 m_cpcd_protocol_version;

 public:
  int connect();
  int list_processes(Vector<Process> &, Properties &reply);
  int start_process(Uint32 id, Properties &reply);
  int stop_process(Uint32 id, Properties &reply);
  int undefine_process(Uint32 id, Properties &reply);
  int define_process(Process &p, Properties &reply);
  int show_version(Properties &reply);
  int select_protocol(Properties &reply);

 private:
  int open_connection();
  int negotiate_client_protocol();
  void close_connection();

  int cpc_send(const char *cmd, const Properties &args);

  Parser_t::ParserStatus cpc_recv(const ParserRow_t *syntax,
                                  const Properties **reply,
                                  void **user_data = NULL);

  const Properties *cpc_call(const char *cmd, const Properties &args,
                             const ParserRow_t *reply_syntax);
};

#endif /* !__CPCCLIENT_HPP_INCLUDED__ */
