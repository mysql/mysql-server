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

#ifndef __CPCCLIENT_HPP_INCLUDED__
#define __CPCCLIENT_HPP_INCLUDED__

#include <Parser.hpp>
#include <SocketServer.hpp>
#include <util/InputStream.hpp>
#include <util/OutputStream.hpp>

/**
 * Simple CPC client class. The whole management client should be replaced
 * something smarter and more worked through.
 */
class SimpleCpcClient {
public:
  SimpleCpcClient(const char *host, int port);
  ~SimpleCpcClient();

  static void run(SimpleCpcClient &);

  int getPort() const { return port;}
  const char * getHost() const { return host;}
  
  struct Process {
    int m_id;
    BaseString m_name;

    BaseString m_owner;
    BaseString m_group;
    BaseString m_runas;

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
  InputStream *cpc_in;
  OutputStream *cpc_out;

public:  
  int connect();

  void cmd_list(char *arg);
  void cmd_start(char *arg);
  void cmd_stop(char *arg);
  void cmd_help(char *arg);

  int list_processes(Vector<Process>&, Properties &reply);
  int start_process(Uint32 id, Properties& reply);
  int stop_process(Uint32 id, Properties& reply);
  int undefine_process(Uint32 id, Properties& reply);
  int define_process(Process & p, Properties& reply);
  
private:
  int cpc_send(const char *cmd,
	       const Properties &args);


  Parser_t::ParserStatus cpc_recv(const ParserRow_t *syntax,
				  const Properties **reply,
				  void **user_data = NULL);

  const Properties *cpc_call(const char *cmd,
			     const Properties &args,
			     const ParserRow_t *reply_syntax);
};

#endif /* !__CPCCLIENT_HPP_INCLUDED__ */
