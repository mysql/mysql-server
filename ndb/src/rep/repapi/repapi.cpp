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
#include <NdbTCP.h>
#include "repapi.h"
//#include "mgmapi_debug.h"
#include <socket_io.h>

#include <NdbOut.hpp>
#include <SocketServer.hpp>
#include <Parser.hpp>
#include <OutputStream.hpp>
#include <InputStream.hpp>

#if defined  VM_TRACE && !defined NO_DEBUG_MESSAGES
#define DEBUG(x) ndbout << x << endl;
#elif defined NO_DEBUG_MESSAGES
#define DEBUG(x)
#endif

#ifdef NDB_WIN32
#define EBADMSG EFAULT
#endif



class ParserDummy2 : SocketServer::Session {
public:
  ParserDummy2(NDB_SOCKET_TYPE sock);
};

ParserDummy2::ParserDummy2(NDB_SOCKET_TYPE sock) : SocketServer::Session(sock) {

}

typedef Parser<ParserDummy2> Parser_t;


#define REP_CMD(name, fun, desc) \
 { name, \
   0, \
   ParserRow<ParserDummy2>::Cmd, \
   ParserRow<ParserDummy2>::String, \
   ParserRow<ParserDummy2>::Optional, \
   ParserRow<ParserDummy2>::IgnoreMinMax, \
   0, 0, \
   fun, \
   desc, 0 }

#define REP_ARG(name, type, opt, desc) \
 { name, \
   0, \
   ParserRow<ParserDummy2>::Arg, \
   ParserRow<ParserDummy2>::type, \
   ParserRow<ParserDummy2>::opt, \
   ParserRow<ParserDummy2>::IgnoreMinMax, \
   0, 0, \
   0, \
  desc, 0 }

#define REP_END() \
 { 0, \
   0, \
   ParserRow<ParserDummy2>::Arg, \
   ParserRow<ParserDummy2>::Int, \
   ParserRow<ParserDummy2>::Optional, \
   ParserRow<ParserDummy2>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

struct ndb_rep_handle {
  char * hostname;
  unsigned short port;
  
  int connected;
  int last_error;
  int last_error_line;
  int read_timeout;
  int write_timeout;

  NDB_SOCKET_TYPE socket;

#ifdef REPAPI_LOG
  FILE* logfile;
#endif
};

#define SET_ERROR(h, e) \
  h->last_error = e;  \
  h->last_error_line = __LINE__;

extern "C"
NdbRepHandle
ndb_rep_create_handle(){
  NdbRepHandle h   = (NdbRepHandle)malloc(sizeof(ndb_rep_handle));
  h->connected     = 0;
  h->last_error    = 0;
  h->last_error_line = 0;
  h->hostname      = 0;
  h->socket        = -1;
  h->read_timeout  = 50000;
  h->write_timeout = 100;
  
#ifdef REPAPI_LOG
  h->logfile = 0;
#endif

  return h;
}

/**
 * Destroy a handle
 */
extern "C"
void
ndb_rep_destroy_handle(NdbRepHandle * handle){
  if(!handle)
    return;
  if((* handle)->connected){
    ndb_rep_disconnect(* handle);
  }
  if((* handle)->hostname != 0){
    free((* handle)->hostname);
  }
#ifdef REPAPI_LOG
  if ((* handle)->logfile != 0){
    fclose((* handle)->logfile);
    (* handle)->logfile = 0;
  }
#endif
  free(* handle);
  * handle = 0;
}

/**
 * Get latest error associated with a handle
 */
extern "C"
int
ndb_rep_get_latest_error(const NdbRepHandle h){
  return h->last_error;
}

/**
 * Get latest error line associated with a handle
 */
extern "C"
int
ndb_rep_get_latest_error_line(const NdbRepHandle h){
  return h->last_error_line;
}

static
int
parse_connect_string(const char * connect_string,
		     NdbRepHandle handle){

  if(connect_string == 0){
    DEBUG("connect_string == 0");
    SET_ERROR(handle, EINVAL);
    return -1;
  }
  
  char * line = strdup(connect_string);
  if(line == 0){
    DEBUG("line == 0");
    SET_ERROR(handle, ENOMEM);
    return -1;
  }
  
  char * tmp = strchr(line, ':');
  if(tmp == 0){
    DEBUG("tmp == 0");
    free(line);
    SET_ERROR(handle, EINVAL);
    return -1;
  }
  * tmp = 0; tmp++;
  
  int port = 0;
  if(sscanf(tmp, "%d", &port) != 1){
    DEBUG("sscanf() != 1");
    free(line);
    SET_ERROR(handle, EINVAL);
    return -1;
  }
  
  if(handle->hostname != 0)
    free(handle->hostname);

  handle->hostname = strdup(line);
  handle->port = port;
  free(line);
  return 0;
}

/*
 * Call an operation, and return the reply
 */
static const Properties *
ndb_rep_call(NdbRepHandle handle,
	     const ParserRow<ParserDummy2> *command_reply,
	     const char *cmd,
	     const Properties *cmd_args) {
  SocketOutputStream out(handle->socket);
  SocketInputStream in(handle->socket, handle->read_timeout);

  out.println(cmd);
#ifdef REPAPI_LOG
  /** 
   * Print command to  log file
   */
  FileOutputStream f(handle->logfile);
  f.println("OUT: %s", cmd);
#endif

  if(cmd_args != NULL) {
    Properties::Iterator iter(cmd_args);
    const char *name;
    while((name = iter.next()) != NULL) {
      PropertiesType t;
      Uint32 val_i;
      BaseString val_s;

      cmd_args->getTypeOf(name, &t);
      switch(t) {
      case PropertiesType_Uint32:
	cmd_args->get(name, &val_i);
	out.println("%s: %d", name, val_i);
	break;
      case PropertiesType_char:
	cmd_args->get(name, val_s);
	out.println("%s: %s", name, val_s.c_str());
	break;
      default:
	/* Ignore */
	break;
      }
    }
#ifdef REPAPI_LOG
  /** 
   * Print arguments to  log file
   */
  cmd_args->print(handle->logfile, "OUT: ");
#endif
  }
  out.println("");

  Parser_t::Context ctx;
  ParserDummy2 session(handle->socket);
  Parser_t parser(command_reply, in, true, true, true);

#if 1
  const Properties* p = parser.parse(ctx, session);
  if (p == NULL){
    /**
     * Print some info about why the parser returns NULL
     */
    ndbout << " status=" << ctx.m_status << ", curr="<<ctx.m_currentToken << endl;
  } 
#ifdef REPAPI_LOG
  else {
    /** 
     * Print reply to log file
     */
    p->print(handle->logfile, "IN: ");
  }
#endif
  return p;
#else
   return parser.parse(ctx, session);
#endif
}

/**
 * Connect to a rep server
 *
 * Returns 0 if OK, sets ndb_rep_handle->last_error otherwise
 */
extern "C"
int
ndb_rep_connect(NdbRepHandle handle, const char * repsrv){

  if(handle == 0)
    return -1;
  
  if(parse_connect_string(repsrv, handle) != 0)
    return -1;


#ifdef REPAPI_LOG
  /**
  * Open the log file
  */
  char logname[64];
  snprintf(logname, 64, "repapi.log");
  handle->logfile = fopen(logname, "w");
#endif

  /**
   * Do connect
   */
  const NDB_SOCKET_TYPE sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == NDB_INVALID_SOCKET) {
    DEBUG("socket() == INVALID_SOCKET");
    return -1;
  }
  
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(handle->port);
  // Convert ip address presentation format to numeric format
  const int res1 = Ndb_getInAddr(&servaddr.sin_addr, handle->hostname);
  if (res1 != 0) {
    DEBUG("Ndb_getInAddr(...) == -1");
    return -1;
  }
  
  const int res2 = connect(sockfd, (struct sockaddr*) &servaddr, 
			   sizeof(servaddr));
  if (res2 == -1) {
    DEBUG("connect() == -1");
    NDB_CLOSE_SOCKET(sockfd);
    return -1;
  }
  
  handle->socket    = sockfd;
  handle->connected = 1;

  return 0;
}
  
/**
 * Disconnect from a rep server
 */
extern "C"
void
ndb_rep_disconnect(NdbRepHandle handle){
  if(handle == 0)
    return;
  
  if(handle->connected != 1){
    return;
  }

  NDB_CLOSE_SOCKET(handle->socket);
  handle->socket = -1;
  handle->connected = 0;

  return;
}



/******************************************************************************
 * Global Replication
 ******************************************************************************/
extern "C"
int ndb_rep_command(NdbRepHandle handle,
		    unsigned int request,
		    unsigned int* replication_id,
		    struct ndb_rep_reply* /*reply*/,
		    unsigned int epoch) {
  
  *replication_id = 0;

  const ParserRow<ParserDummy2> replication_reply[] = {
    REP_CMD("global replication reply", NULL, ""),
    REP_ARG("result", Int, Mandatory, "Error message"),
    REP_ARG("id", Int, Optional, "Id of global replication"),
    REP_END()
  };
  
  if (handle == 0) {
    return -1;
  }
  
  if (handle->connected != 1) {
    handle->last_error = EINVAL;
    return -1;
  }

  Properties args;
  args.put("request", request);
  args.put("id", *replication_id);
  if(epoch > 0) 
    args.put("epoch",epoch);
  else
    args.put("epoch",(unsigned int)0);

  const Properties *reply;
  reply = ndb_rep_call(handle, replication_reply, "rep", &args);

  if(reply == NULL) {
    handle->last_error = EIO;
    return -1;
  }

  reply->get("id", replication_id);
  Uint32 result;
  reply->get("result", &result);
  delete reply;
  return result;
}

extern "C"
int convert2int(char * first, char * last, unsigned int f[], unsigned int  l[])
{
  char * ftok = strtok(first, ",");
  char * ltok = strtok(last, ",");
  Uint32 i=0;
  while(ftok!=NULL && ltok!=NULL) 
  {
    f[i] = atoi(ftok);
    l[i] = atoi(ltok);
    ftok = strtok(NULL, ",");
    ltok = strtok(NULL, ",");
    i++;
  }
 
  return 0;
}


int ndb_rep_query(NdbRepHandle           handle,
		  QueryCounter           counter,
		  unsigned int*          replicationId,
		  struct ndb_rep_reply*  /*reply*/,
		  struct rep_state * state)
{
  *replicationId = 0; // not used currently.

  if(state == 0)   
    return -1;

  const ParserRow<ParserDummy2> replication_reply[] = {
    REP_CMD("global replication query reply", NULL, ""),
    REP_ARG("result", String, Mandatory, "Error message"),
    REP_ARG("id", Int, Mandatory, "replicationId"),
    REP_ARG("no_of_nodegroups", Int, Optional, "number of nodegroups"),
    REP_ARG("subid", Int, Optional, "Id of subscription"),
    REP_ARG("subkey", Int, Optional, "Key of subscription"),
    REP_ARG("connected_rep", Int, Optional, "connected to rep"),
    REP_ARG("connected_db", Int, Optional, "connected to db"),
    REP_ARG("first", String, Optional, ""),
    REP_ARG("last", String, Optional, ""),
    REP_ARG("state_sub", Int, Optional, "state of subsription"),
    REP_ARG("state", Int, Optional, "state"),
    REP_END()
  };
  
  if (handle == 0) {
    return -1;
  }
  
  if (handle->connected != 1) {
    handle->last_error = EINVAL;
    return -1;
  }

  const Properties *props;
  Properties args;
  Uint32 request = 0;
  args.put("request", request);
  args.put("id", *replicationId);
  args.put("counter" , (Uint32)counter);
  props = ndb_rep_call(handle, replication_reply, "rep query", &args);

  BaseString result;
  props->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) 
  {
    delete props;
    return 1;
  }
  state->queryCounter = counter;
  unsigned int no_of_nodegroups;
  props->get("no_of_nodegroups", &no_of_nodegroups);
  state->no_of_nodegroups = no_of_nodegroups;  

  if(counter >= 0) 
  {
    BaseString first, last;
    props->get("first", first);
    props->get("last", last);
    convert2int((char*)first.c_str(), (char*)last.c_str(),
	      state->first , state->last );
  } else 
  {
    for(Uint32 i = 0; i<REPAPI_MAX_NODE_GROUPS; i++) {
    state->first[i] = 0;
    state->last[i] = 0;
    }
  }

  unsigned int connected_rep = 0;
  props->get("connected_rep", &connected_rep);
  state->connected_rep = connected_rep;
  
  unsigned int connected_db = 0;
  props->get("connected_rep", &connected_db);
  state->connected_db = connected_db;      
  
  unsigned int subid;
  props->get("subid", &subid);
  state->subid = subid;

  unsigned int subkey;
  props->get("subkey", &subkey);
  state->subkey = subkey;

  unsigned int _state;
  props->get("state", &_state);
  state->state = _state;

  unsigned int state_sub;
  props->get("state_sub", &state_sub);
  state->state_sub = state_sub;

  if(props == NULL) {
    handle->last_error = EIO;
    return -1;
  }
  delete props;

  return 0;
}


extern "C"
int  
ndb_rep_get_status(NdbRepHandle handle,
			unsigned int* replication_id,
			struct ndb_rep_reply* /*reply*/,
			struct rep_state * repstate) {
  
  const ParserRow<ParserDummy2> replication_reply[] = {
    REP_CMD("global replication status reply", NULL, ""),
    REP_ARG("result", String, Mandatory, "Error message"),
    REP_ARG("id", Int, Optional, "Error message"),
    REP_ARG("subid", Int, Optional, "Id of subscription"),
    REP_ARG("subkey", Int, Optional, "Key of subscription"),
    REP_ARG("connected_rep", Int, Optional, "connected to rep"),
    REP_ARG("connected_db", Int, Optional, "connected to db"),
    REP_ARG("state_sub", Int, Optional, "state of subsription"),
    REP_ARG("state", Int, Optional, "state"),
    REP_END()
  };
  
  if (handle == 0) {
    return -1;
  }
  
  if (handle->connected != 1) {
    handle->last_error = EINVAL;
    return -1;
  }

  const Properties *reply;
  Properties args;
  Uint32 request = 0;
  args.put("request", request);
  reply = ndb_rep_call(handle, replication_reply, "rep status", &args);

  if(reply == NULL) {
    handle->last_error = EIO;
    return -1;
  }
  
  Uint32 result;
  reply->get("result", &result);
  reply->get("id", replication_id);
  reply->get("subid", (Uint32*)&repstate->subid);
  reply->get("subkey", (Uint32*)&repstate->subkey);
  reply->get("connected_rep", (Uint32*)&repstate->connected_rep);
  reply->get("connected_db", (Uint32*)&repstate->connected_db);
  reply->get("state", (Uint32*)&repstate->state);
  reply->get("state_sub", (Uint32*)&repstate->state_sub);

  delete reply;
  return result;
}
