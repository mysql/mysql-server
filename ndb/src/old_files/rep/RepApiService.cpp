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
#include "RepApiService.hpp"
#include "RepApiInterpreter.hpp"
#include "repapi/repapi.h"
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

#define REP_CMD(name, fun, desc) \
 { name, \
   0, \
   ParserRow<RepApiSession>::Cmd, \
   ParserRow<RepApiSession>::String, \
   ParserRow<RepApiSession>::Optional, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   0, 0, \
   fun, \
   desc }

#define REP_ARG(name, type, opt, desc) \
 { name, \
   0, \
   ParserRow<RepApiSession>::Arg, \
   ParserRow<RepApiSession>::type, \
   ParserRow<RepApiSession>::opt, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
  desc }

#define REP_ARG2(name, type, opt, min, max, desc) \
 { name, \
   0, \
   ParserRow<RepApiSession>::Arg, \
   ParserRow<RepApiSession>::type, \
   ParserRow<RepApiSession>::opt, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   min, max, \
   0, \
  desc }

#define REP_END() \
 { 0, \
   0, \
   ParserRow<RepApiSession>::Arg, \
   ParserRow<RepApiSession>::Int, \
   ParserRow<RepApiSession>::Optional, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0 }

#define REP_CMD_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<RepApiSession>::CmdAlias, \
   ParserRow<RepApiSession>::Int, \
   ParserRow<RepApiSession>::Optional, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0 }

#define REP_ARG_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<RepApiSession>::ArgAlias, \
   ParserRow<RepApiSession>::Int, \
   ParserRow<RepApiSession>::Optional, \
   ParserRow<RepApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0 }


const
ParserRow<RepApiSession> commands[] = 
{
  
  REP_CMD("rep" , &RepApiSession::execCommand, ""),
    REP_ARG("request",     Int, Mandatory,  "Grep::Request."),
    REP_ARG("id",     Int, Mandatory,  "Replication id "),
    REP_ARG("epoch",     Int, Optional,  "Epoch. Used by stop epoch ..."),

  REP_CMD("rep status" , &RepApiSession::getStatus, ""),
    REP_ARG("request",     Int, Optional,  "Grep::Request."),

  REP_CMD("rep query" , &RepApiSession::query, ""),
  REP_ARG("id",     Int, Mandatory,  "Replication Id"),
  REP_ARG("counter",     Int, Mandatory,  "QueryCounter."),
  REP_ARG("request",     Int, Mandatory,  "Grep::Request."),
  
  REP_END()
};
RepApiSession::RepApiSession(NDB_SOCKET_TYPE sock,
			       class RepApiInterpreter & rep)
  : SocketServer::Session(sock)
  , m_rep(rep)
{
  m_input = new SocketInputStream(sock);
  m_output = new SocketOutputStream(sock);
  m_parser = new Parser<RepApiSession>(commands, *m_input, true, true, true);
}

RepApiSession::RepApiSession(FILE * f, class RepApiInterpreter & rep)
  : SocketServer::Session(1)
  , m_rep(rep)
{
  m_input = new FileInputStream(f);
  m_parser = new Parser<RepApiSession>(commands, *m_input, true, true, true);
}
  
RepApiSession::~RepApiSession() 
{
  delete m_input;
  delete m_parser;
}

void
RepApiSession::runSession()
{
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
RepApiSession::execCommand(Parser_t::Context & /* unused */, 
			   const class Properties & args)
{
  Uint32  err;
  Uint32  replicationId;
  args.get("id", &replicationId);
  Properties * result  = m_rep.execCommand(args);
  if(result == NULL) {
    m_output->println("global replication reply");
    m_output->println("result: %d", -1);
    m_output->println("id: %d",replicationId);
    m_output->println("");
    return;
  }
  result->get("err", &err);
  m_output->println("global replication reply");
  m_output->println("result: %d", err);
  m_output->println("id: %d", 0);
  m_output->println("");
  delete result;
}


void
RepApiSession::getStatus(Parser_t::Context & /* unused */, 
			 const class Properties & args)
{
  Uint32  err;
  Properties * result  = m_rep.getStatus();
  result->get("err", &err);
  Uint32 subId;
  result->get("subid", &subId);
  Uint32 subKey;
  result->get("subkey", &subKey);
  Uint32 connected_rep;
  result->get("connected_rep", &connected_rep);
  Uint32 connected_db;
  result->get("connected_db", &connected_db);
  Uint32 state;
  result->get("state", &state);
  Uint32 state_sub;
  result->get("state", &state_sub);

  m_output->println("global replication status reply");
  m_output->println("result: %d",0);
  m_output->println("id: %d",0);
  m_output->println("subid: %d", subId);
  m_output->println("subkey: %d", subKey);
  m_output->println("connected_rep: %d", connected_rep);
  m_output->println("connected_db: %d", connected_db);
  m_output->println("state_sub: %d", state_sub);
  m_output->println("state: %d", state);
  m_output->println("");
  delete result;
}


void
RepApiSession::query(Parser_t::Context & /* unused */, 
			const class Properties & args)
{
  Uint32  err;
  Uint32 counter, replicationId;
  args.get("counter", &counter);
  args.get("id", &replicationId);
  Properties * result  = m_rep.query(counter, replicationId);
  if(result == NULL) {
    m_output->println("global replication query reply");
    m_output->println("result: %s","Failed");
    m_output->println("id: %d",replicationId);
    m_output->println("");
    return;
  }
    
  BaseString first;
  BaseString last;
  Uint32 subid = 0, subkey = 0, no_of_nodegroups = 0;
  Uint32 connected_rep = 0, connected_db = 0;
  Uint32 state = 0 , state_sub = 0;
  result->get("err", &err);
  result->get("no_of_nodegroups", &no_of_nodegroups);
  result->get("subid", &subid);
  result->get("subkey", &subkey);
  result->get("connected_rep", &connected_rep);
  result->get("connected_db", &connected_db);
  result->get("first", first);
  result->get("last", last);
  result->get("state", &state);
  result->get("state_sub", &state_sub);
  m_output->println("global replication query reply");
  m_output->println("result: %s","Ok");
  m_output->println("id: %d",replicationId);
  m_output->println("no_of_nodegroups: %d",no_of_nodegroups);
  m_output->println("subid: %d", subid);
  m_output->println("subkey: %d", subkey);
  m_output->println("connected_rep: %d", connected_rep);
  m_output->println("connected_db: %d", connected_db);
  m_output->println("state_sub: %d", state_sub);
  m_output->println("state: %d", state);
  m_output->println("first: %s", first.c_str());
  m_output->println("last: %s", last.c_str());
  m_output->println("");
  delete result;
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
    snprintf(buf, sizeof buf, "%d", val);
    retval = buf;
    break;
  case PropertiesType_char:
    const char *str;
    prop->get(key, &str);
    retval = str;
    break;
  default:
    snprintf(buf, sizeof buf, "(unknown)");
    retval = buf;
  }
  return retval;
}

void
RepApiSession::printProperty(Properties *prop, const char *key) {
  m_output->println("%s: %s", key, propToString(prop, key));
}

void
RepApiSession::stopSession(){

}
