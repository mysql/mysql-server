/*
   Copyright (C) 2003-2008 MySQL AB, 2009, 2010 Sun Microsystems, Inc.

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

#include <ndb_global.h>

#include <NdbOut.hpp>
#include <NdbTCP.h>
#include "CpcClient.hpp"

#define CPC_CMD(name, value, desc) \
 { (name), \
   0, \
   ParserRow_t::Cmd, \
   ParserRow_t::String, \
   ParserRow_t::Optional, \
   ParserRow_t::IgnoreMinMax, \
   0, 0, \
   0, \
   (desc), \
   (value) }

#define CPC_ARG(name, type, opt, desc) \
 { (name), \
   0, \
   ParserRow_t::Arg, \
   ParserRow_t::type, \
   ParserRow_t::opt, \
   ParserRow_t::IgnoreMinMax, \
   0, 0, \
   0, \
  (desc), 0 }

#define CPC_END() \
 { 0, \
   0, \
   ParserRow_t::Arg, \
   ParserRow_t::Int, \
   ParserRow_t::Optional, \
   ParserRow_t::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

#ifdef DEBUG_PRINT_PROPERTIES 
static void printprop(const Properties &p) {
  Properties::Iterator iter(&p);
  const char *name;
  while((name = iter.next()) != NULL) {
    PropertiesType t;
    Uint32 val_i;
    BaseString val_s;

    p.getTypeOf(name, &t);
    switch(t) {
    case PropertiesType_Uint32:
      p.get(name, &val_i);
      ndbout << name << " (Uint32): " << val_i << endl;
      break;
    case PropertiesType_char:
      p.get(name, val_s);
      ndbout << name << " (string): " << val_s << endl;
      break;
    default:
      ndbout << "Unknown type " << t << endl;
      break;
    }
  }
}
#endif


int
SimpleCpcClient::stop_process(Uint32 id, Properties& reply){
  const ParserRow_t stop_reply[] = {
    CPC_CMD("stop process", NULL, ""),
    CPC_ARG("status", Int, Mandatory, ""),
    CPC_ARG("id", Int, Optional, ""),
    CPC_ARG("errormessage", String, Optional, ""),
    
    CPC_END()
  };

  Properties args;
  args.put("id", id);
  
  const Properties* ret = cpc_call("stop process", args, stop_reply);
  if(ret == 0){
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if(status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }
  delete ret;
  return status;
}


int
SimpleCpcClient::start_process(Uint32 id, Properties& reply){
  const ParserRow_t start_reply[] = {
    CPC_CMD("start process", NULL, ""),
    CPC_ARG("status", Int, Mandatory, ""),
    CPC_ARG("id", Int, Optional, ""),
    CPC_ARG("errormessage", String, Optional, ""),
    
    CPC_END()
  };

  Properties args;
  args.put("id", id);
  
  const Properties* ret = cpc_call("start process", args, start_reply);
  if(ret == 0){
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if(status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  delete ret;

  return status;
}

int
SimpleCpcClient::undefine_process(Uint32 id, Properties& reply){
  const ParserRow_t stop_reply[] = {
    CPC_CMD("undefine process", NULL, ""),
    CPC_ARG("status", Int, Mandatory, ""),
    CPC_ARG("id", Int, Optional, ""),
    CPC_ARG("errormessage", String, Optional, ""),
    
    CPC_END()
  };

  Properties args;
  args.put("id", id);
  
  const Properties* ret = cpc_call("undefine process", args, stop_reply);
  if(ret == 0){
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if(status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  delete ret;

  return status;
}

#if 0
static void
printproc(SimpleCpcClient::Process & p) {
  ndbout.println("Name:                %s", p.m_name.c_str());
  ndbout.println("Id:                  %d", p.m_id);
  ndbout.println("Type:                %s", p.m_type.c_str());
  ndbout.println("Group:               %s", p.m_group.c_str());
  ndbout.println("Program path:        %s", p.m_path.c_str());
  ndbout.println("Arguments:           %s", p.m_args.c_str());
  ndbout.println("Environment:         %s", p.m_env.c_str());
  ndbout.println("Working directory:   %s", p.m_cwd.c_str());
  ndbout.println("Owner:               %s", p.m_owner.c_str());
  ndbout.println("Runas:               %s", p.m_runas.c_str());
  ndbout.println("Ulimit:              %s", p.m_ulimit.c_str());
  ndbout.println("%s", "");
}
#endif

static int
convert(const Properties & src, SimpleCpcClient::Process & dst){
  bool b = true;
  b &= src.get("id", (Uint32*)&dst.m_id);
  b &= src.get("name",   dst.m_name);
  b &= src.get("type",   dst.m_type);
  b &= src.get("status", dst.m_status);
  b &= src.get("owner",  dst.m_owner);
  b &= src.get("group",  dst.m_group);
  b &= src.get("path",   dst.m_path);
  b &= src.get("args",   dst.m_args);
  b &= src.get("env",    dst.m_env);
  b &= src.get("cwd",    dst.m_cwd);
  b &= src.get("runas",  dst.m_runas);

  b &= src.get("stdin",  dst.m_stdin);
  b &= src.get("stdout", dst.m_stdout);
  b &= src.get("stderr", dst.m_stderr);
  b &= src.get("ulimit", dst.m_ulimit);
  b &= src.get("shutdown", dst.m_shutdown_options);

  return b;
}

static int
convert(const SimpleCpcClient::Process & src, Properties & dst ){
  bool b = true;
  //b &= dst.put("id",     (Uint32)src.m_id);
  b &= dst.put("name",   src.m_name.c_str());
  b &= dst.put("type",   src.m_type.c_str());
  //b &= dst.put("status", src.m_status.c_str());
  b &= dst.put("owner",  src.m_owner.c_str());
  b &= dst.put("group",  src.m_group.c_str());
  b &= dst.put("path",   src.m_path.c_str());
  b &= dst.put("args",   src.m_args.c_str());
  b &= dst.put("env",    src.m_env.c_str());
  b &= dst.put("cwd",    src.m_cwd.c_str());
  b &= dst.put("runas",  src.m_runas.c_str());

  b &= dst.put("stdin",  src.m_stdin.c_str());
  b &= dst.put("stdout", src.m_stdout.c_str());
  b &= dst.put("stderr", src.m_stderr.c_str());
  b &= dst.put("ulimit", src.m_ulimit.c_str());
  b &= dst.put("shutdown", src.m_shutdown_options.c_str());
  
  return b;
}

int
SimpleCpcClient::define_process(Process & p, Properties& reply){
  const ParserRow_t define_reply[] = {
    CPC_CMD("define process", NULL, ""),
    CPC_ARG("status", Int, Mandatory, ""),
    CPC_ARG("id", Int, Optional, ""),
    CPC_ARG("errormessage", String, Optional, ""),
    
    CPC_END()
  };

  Properties args;
  convert(p, args);

  const Properties* ret = cpc_call("define process", args, define_reply);
  if(ret == 0){
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if(status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  Uint32 id;
  if(!ret->get("id", &id)){
    return -1;
  }

  p.m_id = id;
  delete ret;
  return status;
}

int
SimpleCpcClient::list_processes(Vector<Process> &procs, Properties& reply) {
  int start = 0, end = 0, entry; 
  const ParserRow_t list_reply[] = {
    CPC_CMD("start processes", &start, ""),
    CPC_CMD("end processes", &end, ""),

    CPC_CMD("process", &entry, ""),
    CPC_ARG("id",    Int,    Mandatory, "Id of process."),
    CPC_ARG("name",  String, Mandatory, "Name of process"),
    CPC_ARG("group", String, Mandatory, "Group of process"),
    CPC_ARG("env",   String, Mandatory, "Environment variables for process"),
    CPC_ARG("path",  String, Mandatory, "Path to binary"),
    CPC_ARG("args",  String, Mandatory, "Arguments to process"),
    CPC_ARG("type",  String, Mandatory, "Type of process"),
    CPC_ARG("cwd",   String, Mandatory, "Working directory of process"),
    CPC_ARG("owner", String, Mandatory, "Owner of process"),
    CPC_ARG("status",String, Mandatory, "Status of process"),
    CPC_ARG("runas", String, Mandatory, "Run as user"),
    CPC_ARG("stdin", String, Mandatory, "Redirect stdin"),
    CPC_ARG("stdout",String, Mandatory, "Redirect stdout"),
    CPC_ARG("stderr",String, Mandatory, "Redirect stderr"),
    CPC_ARG("ulimit",String, Mandatory, "ulimit"),    
    CPC_ARG("shutdown",String, Mandatory, "shutdown"),    
    
    CPC_END()
  };

  reply.clear();

  const Properties args;

  cpc_send("list processes", args);

  bool done = false;
  while(!done) {
    const Properties *proc;
    void *p;
    cpc_recv(list_reply, &proc, &p);

    end++;
    if(p == &start)
    {
      start = 1;
      /* do nothing */
    }
    else if(p == &end)
    {
      done = true;
    }
    else if(p == &entry)
    {
      if(proc != NULL)
      {
	Process p;
	convert(* proc, p);
	procs.push_back(p);
      }
      else
      {
        ndbout_c("JONAS: start: %d loop: %d cnt: %u got p == &entry with proc == NULL",
                 start, end, procs.size());
      }
    }
    else
    {
      ndbout_c("internal error: %d", __LINE__);
      return -1;
    }
    if (proc)
    {
      delete proc;
    }
  }
  return 0;
}


SimpleCpcClient::SimpleCpcClient(const char *_host, int _port) {
  host = strdup(_host);
  port = _port;
  my_socket_invalidate(&cpc_sock);
}

SimpleCpcClient::~SimpleCpcClient() {
  if(host != NULL) {
    free(host);
    host = NULL;
  }

  port = 0;

  if(my_socket_valid(cpc_sock)) {
    my_socket_close(cpc_sock);
    my_socket_invalidate(&cpc_sock);
  }
}

int
SimpleCpcClient::connect() {
  struct sockaddr_in sa;
  struct hostent *hp;

  /* Create socket */
  cpc_sock = my_socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(!my_socket_valid(cpc_sock))
    return -1;

  /* Connect socket */
  sa.sin_family = AF_INET;
  hp = gethostbyname(host);
  if(hp == NULL) {
    errno = ENOENT;
    return -1;
  }

  memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
  sa.sin_port = htons(port);
  if (my_connect_inet(cpc_sock, &sa) < 0)
    return -1;

  return 0;
}

int
SimpleCpcClient::cpc_send(const char *cmd,
			  const Properties &args) {
  SocketOutputStream cpc_out(cpc_sock);

  cpc_out.println(cmd);

  Properties::Iterator iter(&args);
  const char *name;
  while((name = iter.next()) != NULL) {
    PropertiesType t;
    Uint32 val_i;
    BaseString val_s;

    args.getTypeOf(name, &t);
    switch(t) {
    case PropertiesType_Uint32:
      args.get(name, &val_i);
      cpc_out.println("%s: %d", name, val_i);
      break;
    case PropertiesType_char:
      args.get(name, val_s);
      if (strlen(val_s.c_str()) > Parser_t::Context::MaxParseBytes)
      {
        ndbout << "Argument " << name << " at " 
               << strlen(val_s.c_str())
               << " longer than max of "
               << Parser_t::Context::MaxParseBytes
               << endl;
        abort();
      }
      cpc_out.println("%s: %s", name, val_s.c_str());
      break;
    default:
      /* Silently ignore */
      break;
    }
  }
  cpc_out.println("%s", "");

  return 0;
}

/**
 * Receive a response from the CPCD. The argument reply will point
 * to a Properties object describing the reply. Note that the caller
 * is responsible for deleting the Properties object returned.
 */
SimpleCpcClient::Parser_t::ParserStatus
SimpleCpcClient::cpc_recv(const ParserRow_t *syntax,
			  const Properties **reply,
			  void **user_value) {
  SocketInputStream cpc_in(cpc_sock);

  Parser_t::Context ctx;
  ParserDummy session(cpc_sock);
  Parser_t parser(syntax, cpc_in, true, true, true);
  *reply = parser.parse(ctx, session);
  if(user_value != NULL)
    *user_value = ctx.m_currentCmd->user_value;
  return ctx.m_status;
}

const Properties *
SimpleCpcClient::cpc_call(const char *cmd,
			  const Properties &args,
			  const ParserRow_t *reply_syntax) {
  cpc_send(cmd, args);

  const Properties *ret;
  cpc_recv(reply_syntax, &ret);
  return ret;
}


SimpleCpcClient::ParserDummy::ParserDummy(NDB_SOCKET_TYPE sock)
  : SocketServer::Session(sock) {
}
 
template class Vector<SimpleCpcClient::Process>; 
template class Vector<ParserRow<SimpleCpcClient::ParserDummy> const*>;
