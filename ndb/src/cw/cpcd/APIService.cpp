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


#include <Parser.hpp>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include <socket_io.h>

#include "APIService.hpp"
#include "CPCD.hpp"
#include <NdbMutex.h>
#include <OutputStream.hpp>

/**
   const char * name;
   const char * realName;
   const Type type;
   const ArgType argType;
   const ArgRequired argRequired;
   const ArgMinMax argMinMax;
   const int minVal;
   const int maxVal;
   void (T::* function)(const class Properties & args);
   const char * description;
*/

#define CPCD_CMD(name, fun, desc) \
 { name, \
   0, \
   ParserRow<CPCDAPISession>::Cmd, \
   ParserRow<CPCDAPISession>::String, \
   ParserRow<CPCDAPISession>::Optional, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   0, 0, \
   fun, \
   desc, 0 }

#define CPCD_ARG(name, type, opt, desc) \
 { name, \
   0, \
   ParserRow<CPCDAPISession>::Arg, \
   ParserRow<CPCDAPISession>::type, \
   ParserRow<CPCDAPISession>::opt, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   0, 0, \
   0, \
   desc, 0 }

#define CPCD_ARG2(name, type, opt, min, max, desc) \
 { name, \
   0, \
   ParserRow<CPCDAPISession>::Arg, \
   ParserRow<CPCDAPISession>::type, \
   ParserRow<CPCDAPISession>::opt, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   min, max, \
   0, \
   desc, 0 }

#define CPCD_END() \
 { 0, \
   0, \
   ParserRow<CPCDAPISession>::Arg, \
   ParserRow<CPCDAPISession>::Int, \
   ParserRow<CPCDAPISession>::Optional, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

#define CPCD_CMD_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<CPCDAPISession>::CmdAlias, \
   ParserRow<CPCDAPISession>::Int, \
   ParserRow<CPCDAPISession>::Optional, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

#define CPCD_ARG_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<CPCDAPISession>::ArgAlias, \
   ParserRow<CPCDAPISession>::Int, \
   ParserRow<CPCDAPISession>::Optional, \
   ParserRow<CPCDAPISession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

const
ParserRow<CPCDAPISession> commands[] = 
{
  CPCD_CMD("define process" , &CPCDAPISession::defineProcess, ""),
    CPCD_ARG("id",     Int,    Optional,  "Id of process."),
    CPCD_ARG("name",   String, Mandatory, "Name of process"),
    CPCD_ARG("group",  String, Mandatory, "Group of process"),
    CPCD_ARG("env",    String, Optional,  "Environment variables for process"),
    CPCD_ARG("path",   String, Mandatory, "Path to binary"),
    CPCD_ARG("args",   String, Optional,  "Arguments to process"),
    CPCD_ARG("type",   String, Mandatory, "Type of process"),
    CPCD_ARG("cwd",    String, Mandatory, "Working directory of process"),
    CPCD_ARG("owner",  String, Mandatory, "Owner of process"),
    CPCD_ARG("runas",  String, Optional,  "Run as user"),
    CPCD_ARG("stdout", String, Optional,  "Redirection of stdout"),
    CPCD_ARG("stderr", String, Optional,  "Redirection of stderr"),
    CPCD_ARG("stdin",  String, Optional,  "Redirection of stderr"),
    CPCD_ARG("ulimit", String, Optional,  "ulimit"),

  CPCD_CMD("undefine process", &CPCDAPISession::undefineProcess, ""),
    CPCD_CMD_ALIAS("undef", "undefine process", 0),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),
    CPCD_ARG_ALIAS("i", "id", 0),
    
  CPCD_CMD("start process", &CPCDAPISession::startProcess, ""),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),

  CPCD_CMD("stop process", &CPCDAPISession::stopProcess, ""),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),
  
  CPCD_CMD("list processes", &CPCDAPISession::listProcesses, ""),
  
  CPCD_END()
};
CPCDAPISession::CPCDAPISession(NDB_SOCKET_TYPE sock,
			       CPCD & cpcd)
  : SocketServer::Session(sock)
  , m_cpcd(cpcd)
{
  m_input = new SocketInputStream(sock);
  m_output = new SocketOutputStream(sock);
  m_parser = new Parser<CPCDAPISession>(commands, *m_input, true, true, true);
}

CPCDAPISession::CPCDAPISession(FILE * f, CPCD & cpcd)
  : SocketServer::Session(1)
  , m_cpcd(cpcd)
{
  m_input = new FileInputStream(f);
  m_parser = new Parser<CPCDAPISession>(commands, *m_input, true, true, true);
}
  
CPCDAPISession::~CPCDAPISession() {
  delete m_input;
  delete m_parser;
}

void
CPCDAPISession::runSession(){
  Parser_t::Context ctx;
  while(!m_stop){
    m_parser->run(ctx, * this); 
    if(ctx.m_currentToken == 0)
      break;

    switch(ctx.m_status){
    case Parser_t::Ok:
      for(size_t i = 0; i<ctx.m_aliasUsed.size(); i++)
	ndbout_c("Used alias: %s -> %s", 
		 ctx.m_aliasUsed[i]->name, ctx.m_aliasUsed[i]->realName);
      break;
    case Parser_t::NoLine:
    case Parser_t::EmptyLine:
      break;
    default:
      break;
    }
  }
  NDB_CLOSE_SOCKET(m_socket);
}

void
CPCDAPISession::stopSession(){
  CPCD::RequestStatus rs;
  for(size_t i = 0; i<m_temporaryProcesses.size(); i++){
    Uint32 id = m_temporaryProcesses[i];
    m_cpcd.undefineProcess(&rs, id);
  }
}

void
CPCDAPISession::loadFile(){
  Parser_t::Context ctx;
  while(!m_stop){
    m_parser->run(ctx, * this); 
    if(ctx.m_currentToken == 0)
      break;

    switch(ctx.m_status){
    case Parser_t::Ok:
      for(size_t i = 0; i<ctx.m_aliasUsed.size(); i++)
	ndbout_c("Used alias: %s -> %s", 
		 ctx.m_aliasUsed[i]->name, ctx.m_aliasUsed[i]->realName);
      break;
    case Parser_t::NoLine:
    case Parser_t::EmptyLine:
      break;
    default:
      break;
    }
  }
}

static const int g_TimeOut = 1000;

void
CPCDAPISession::defineProcess(Parser_t::Context & /* unused */, 
			      const class Properties & args){

  CPCD::Process * p = new CPCD::Process(args, &m_cpcd);
  
  CPCD::RequestStatus rs;

  bool ret = m_cpcd.defineProcess(&rs, p);
  if(!m_cpcd.loadingProcessList) {
    m_output->println("define process");
    m_output->println("status: %d", rs.getStatus());
    if(ret == true){
      m_output->println("id: %d", p->m_id);
      if(p->m_processType == TEMPORARY){
	m_temporaryProcesses.push_back(p->m_id);
      }
    } else {
      m_output->println("errormessage: %s", rs.getErrMsg());
    }
    m_output->println("");
  }
}

void
CPCDAPISession::undefineProcess(Parser_t::Context & /* unused */, 
				const class Properties & args){
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  bool ret = m_cpcd.undefineProcess(&rs, id);

  m_output->println("undefine process");
  m_output->println("id: %d", id);
  m_output->println("status: %d", rs.getStatus());
  if(!ret)
    m_output->println("errormessage: %s", rs.getErrMsg());

  m_output->println("");
}

void
CPCDAPISession::startProcess(Parser_t::Context & /* unused */, 
			     const class Properties & args){
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  const int ret = m_cpcd.startProcess(&rs, id);

  if(!m_cpcd.loadingProcessList) {
    m_output->println("start process");
    m_output->println("id: %d", id);
    m_output->println("status: %d", rs.getStatus());
    if(!ret)
      m_output->println("errormessage: %s", rs.getErrMsg());
    m_output->println("");
  }
}

void
CPCDAPISession::stopProcess(Parser_t::Context & /* unused */, 
			    const class Properties & args){
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  int ret = m_cpcd.stopProcess(&rs, id);

  m_output->println("stop process");
  m_output->println("id: %d", id);
  m_output->println("status: %d", rs.getStatus());
  if(!ret)
    m_output->println("errormessage: %s", rs.getErrMsg());
  
  m_output->println("");
}

static const char *
propToString(Properties *prop, const char *key) {
  static char buf[32];
  const char *retval = NULL;
  PropertiesType pt;

  prop->getTypeOf(key, &pt);
  switch(pt) {
  case PropertiesType_Uint32:
    Uint32 val;
    prop->get(key, &val);
    BaseString::snprintf(buf, sizeof buf, "%d", val);
    retval = buf;
    break;
  case PropertiesType_char:
    const char *str;
    prop->get(key, &str);
    retval = str;
    break;
  default:
    BaseString::snprintf(buf, sizeof buf, "(unknown)");
    retval = buf;
  }
  return retval;
}

void
CPCDAPISession::printProperty(Properties *prop, const char *key) {
  m_output->println("%s: %s", key, propToString(prop, key));
}

void
CPCDAPISession::listProcesses(Parser_t::Context & /* unused */, 
			      const class Properties & /* unused */){
  m_cpcd.m_processes.lock();
  MutexVector<CPCD::Process *> *proclist = m_cpcd.getProcessList();

  m_output->println("start processes");
  m_output->println("");
  

  for(size_t i = 0; i < proclist->size(); i++) {
    CPCD::Process *p = (*proclist)[i];

    m_output->println("process");
  
    m_output->println("id: %d", p->m_id);
    m_output->println("name: %s", p->m_name.c_str());
    m_output->println("path: %s", p->m_path.c_str());
    m_output->println("args: %s", p->m_args.c_str());
    m_output->println("type: %s", p->m_type.c_str());
    m_output->println("cwd: %s", p->m_cwd.c_str());
    m_output->println("env: %s", p->m_env.c_str());
    m_output->println("owner: %s", p->m_owner.c_str());
    m_output->println("group: %s", p->m_group.c_str());
    m_output->println("runas: %s", p->m_runas.c_str());
    m_output->println("stdin: %s", p->m_stdin.c_str());
    m_output->println("stdout: %s", p->m_stdout.c_str());
    m_output->println("stderr: %s", p->m_stderr.c_str());    
    m_output->println("ulimit: %s", p->m_ulimit.c_str());    
    switch(p->m_status){
    case STOPPED:
      m_output->println("status: stopped");
      break;
    case STARTING:
      m_output->println("status: starting");
      break;
    case RUNNING:
      m_output->println("status: running");
      break;
    case STOPPING:
      m_output->println("status: stopping");
      break;
    }
    
    m_output->println("");
    
  }

  m_output->println("end processes");
  m_output->println("");

  m_cpcd.m_processes.unlock();
}

template class Vector<ParserRow<CPCDAPISession> const*>;
