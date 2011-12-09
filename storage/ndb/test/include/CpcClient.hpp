/*
   Copyright (C) 2003-2007 MySQL AB, 2009 Sun Microsystems, Inc.

   All rights reserved. Use is subject to license terms.

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

public:  
  int connect();
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
