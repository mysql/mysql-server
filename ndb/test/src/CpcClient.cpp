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

#include <ndb_global.h>
#include <editline/editline.h>

#include <netdb.h>

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
   (void *)(value) }

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

void
SimpleCpcClient::cmd_stop(char *arg) {
  Properties p;
  Vector<Process> proc_list;

  list_processes(proc_list, p);
  bool stopped = false;

  for(size_t i = 0; i < proc_list.size(); i++) {
    if(strcmp(proc_list[i].m_name.c_str(), arg) == 0) {
      stopped = true;
      Properties reply;
      stop_process(proc_list[i].m_id, reply);

      Uint32 status;
      reply.get("status", &status);
      if(status != 0) {
	BaseString msg;
	reply.get("errormessage", msg);
	ndbout << "Stop failed: " << msg << endl;
      }
    }
  }
  
  if(!stopped)
    ndbout << "No such process" << endl;
}

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

  return status;
}

void
SimpleCpcClient::cmd_start(char *arg) {
  Properties p;
  Vector<Process> proc_list;
  list_processes(proc_list, p);
  bool startped = false;

  for(size_t i = 0; i < proc_list.size(); i++) {
    if(strcmp(proc_list[i].m_name.c_str(), arg) == 0) {
      startped = true;

      Properties reply;
      start_process(proc_list[i].m_id, reply);

      Uint32 status;
      reply.get("status", &status);
      if(status != 0) {
	BaseString msg;
	reply.get("errormessage", msg);
	ndbout << "Start failed: " << msg << endl;
      }
    }
  }
  
  if(!startped)
    ndbout << "No such process" << endl;
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

  return status;
}

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
  ndbout.println("");
}

void
SimpleCpcClient::cmd_list(char *arg) {
  Properties p;
  Vector<Process> proc_list;
  list_processes(proc_list, p);

  for(size_t i = 0; i < proc_list.size(); i++) {
    printproc(proc_list[i]);
  }
}

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
  
  return status;
}

int
SimpleCpcClient::list_processes(Vector<Process> &procs, Properties& reply) {
  enum Proclist {
    Proclist_Start,
    Proclist_End,
    Proclist_Entry
  };
  const ParserRow_t list_reply[] = {
    CPC_CMD("start processes", Proclist_Start, ""),

    CPC_CMD("end processes", Proclist_End, ""),

    CPC_CMD("process", Proclist_Entry, ""),
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
    
    CPC_END()
  };

  reply.clear();

  const Properties args;

  cpc_send("list processes", args);

  bool done = false;
  while(!done) {
    const Properties *proc;
    enum Proclist p;
    cpc_recv(list_reply, &proc, (void **)&p);

    switch(p) {
    case Proclist_Start:
      /* do nothing */
      break;
    case Proclist_End:
      done = true;
      break;
    case Proclist_Entry:
      if(proc != NULL){
	Process p;
	convert(* proc, p);
	procs.push_back(p);
      }
      break;
    default:
      /* ignore */
      break;
    }
  }
  return 0;
}

void
SimpleCpcClient::cmd_help(char *arg) {
  ndbout
    << "HELP				Print help text" << endl
    << "LIST				List processes" << endl
    << "START				Start process" << endl
    << "STOP				Stop process" << endl;
}

SimpleCpcClient::SimpleCpcClient(const char *_host, int _port) {
  host = strdup(_host);
  port = _port;
  cpc_sock = -1;
  cpc_in = NULL;
  cpc_out = NULL;
}

SimpleCpcClient::~SimpleCpcClient() {
  if(host != NULL) {
    free(host);
    host = NULL;
  }

  port = 0;

  if(cpc_sock == -1) {
    close(cpc_sock);
    cpc_sock = -1;
  }

  if(cpc_in != NULL)
    delete cpc_in;

  if(cpc_out != NULL)
    delete cpc_out;
}

int
SimpleCpcClient::connect() {
  struct sockaddr_in sa;
  struct hostent *hp;

  /* Create socket */
  cpc_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(cpc_sock < 0)
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
  if (::connect(cpc_sock, (struct sockaddr*) &sa, sizeof(sa)) < 0)
    return -1;

  cpc_in = new SocketInputStream(cpc_sock, 60000);
  cpc_out = new SocketOutputStream(cpc_sock);
  
  return 0;
}

int
SimpleCpcClient::cpc_send(const char *cmd,
			  const Properties &args) {
  
  cpc_out->println(cmd);

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
      cpc_out->println("%s: %d", name, val_i);
      break;
    case PropertiesType_char:
      args.get(name, val_s);
      cpc_out->println("%s: %s", name, val_s.c_str());
      break;
    default:
      /* Silently ignore */
      break;
    }
  }
  cpc_out->println("");

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
  Parser_t::Context ctx;
  ParserDummy session(cpc_sock);
  Parser_t parser(syntax, *cpc_in, true, true, true);
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

#if 0
  Parser_t::Context ctx;
  ParserDummy session(cpc_sock);
  Parser_t parser(reply_syntax, *cpc_in, true, true, true);
  const Properties *ret = parser.parse(ctx, session);
  return ret;
#endif
  const Properties *ret;
  cpc_recv(reply_syntax, &ret);
  return ret;
}


SimpleCpcClient::ParserDummy::ParserDummy(NDB_SOCKET_TYPE sock)
  : SocketServer::Session(sock) {
}
 
template class Vector<SimpleCpcClient::Process>; 
template class Vector<ParserRow<SimpleCpcClient::ParserDummy> const*>;
