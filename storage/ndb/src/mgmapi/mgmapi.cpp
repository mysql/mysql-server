 /* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <my_sys.h>

#include <LocalConfig.hpp>
#include <NdbAutoPtr.hpp>

#include <NdbSleep.h>
#include <NdbTCP.h>
#include <mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi_debug.h>
#include "mgmapi_configuration.hpp"
#include <socket_io.h>
#include <version.h>

#include <NdbOut.hpp>
#include <SocketServer.hpp>
#include <SocketClient.hpp>
#include <Parser.hpp>
#include <OutputStream.hpp>
#include <InputStream.hpp>

#include <base64.h>

#define MGM_CMD(name, fun, desc) \
 { name, \
   0, \
   ParserRow<ParserDummy>::Cmd, \
   ParserRow<ParserDummy>::String, \
   ParserRow<ParserDummy>::Optional, \
   ParserRow<ParserDummy>::IgnoreMinMax, \
   0, 0, \
   fun, \
   desc, 0 }

#define MGM_ARG(name, type, opt, desc) \
 { name, \
   0, \
   ParserRow<ParserDummy>::Arg, \
   ParserRow<ParserDummy>::type, \
   ParserRow<ParserDummy>::opt, \
   ParserRow<ParserDummy>::IgnoreMinMax, \
   0, 0, \
   0, \
   desc, 0 }

#define MGM_END() \
 { 0, \
   0, \
   ParserRow<ParserDummy>::Arg, \
   ParserRow<ParserDummy>::Int, \
   ParserRow<ParserDummy>::Optional, \
   ParserRow<ParserDummy>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

class ParserDummy : private SocketServer::Session 
{
public:
  ParserDummy(NDB_SOCKET_TYPE sock);
};

ParserDummy::ParserDummy(NDB_SOCKET_TYPE sock) : SocketServer::Session(sock) 
{
}

typedef Parser<ParserDummy> Parser_t;

#define NDB_MGM_MAX_ERR_DESC_SIZE 256

struct ndb_mgm_handle {
  int cfg_i;
  
  int connected;
  int last_error;
  int last_error_line;
  char last_error_desc[NDB_MGM_MAX_ERR_DESC_SIZE];
  unsigned int timeout;

  NDB_SOCKET_TYPE socket;

  LocalConfig cfg;

#ifdef MGMAPI_LOG
  FILE* logfile;
#endif
  FILE *errstream;
  char *m_name;
  int mgmd_version_major;
  int mgmd_version_minor;
  int mgmd_version_build;
  char * m_bindaddress;
};

#define SET_ERROR(h, e, s) setError(h, e, __LINE__, s)

static
void
setError(NdbMgmHandle h, int error, int error_line, const char * msg, ...){

  h->last_error = error;  \
  h->last_error_line = error_line;

  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(h->last_error_desc, sizeof(h->last_error_desc), msg, ap);
  va_end(ap);
}

#define CHECK_HANDLE(handle, ret) \
  if(handle == 0) { \
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_HANDLE, ""); \
    return ret; \
  } 

#define CHECK_CONNECTED(handle, ret) \
  if (handle->connected != 1) { \
    SET_ERROR(handle, NDB_MGM_SERVER_NOT_CONNECTED , ""); \
    return ret; \
  }

#define CHECK_REPLY(handle, reply, ret) \
  if(reply == NULL) { \
    if(!handle->last_error) \
      SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, ""); \
    return ret; \
  }

#define DBUG_CHECK_REPLY(handle, reply, ret) \
  if (reply == NULL) { \
    if(!handle->last_error) \
      SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, ""); \
    DBUG_RETURN(ret);                                    \
  }

#define CHECK_TIMEDOUT(in, out) \
  if(in.timedout() || out.timedout()) \
    SET_ERROR(handle, ETIMEDOUT, \
              "Time out talking to management server");

#define CHECK_TIMEDOUT_RET(h, in, out, ret) \
  if(in.timedout() || out.timedout()) { \
    SET_ERROR(handle, ETIMEDOUT, \
              "Time out talking to management server"); \
    ndb_mgm_disconnect_quiet(h); \
    return ret; \
  }

#define DBUG_CHECK_TIMEDOUT_RET(h, in, out, ret) \
  if(in.timedout() || out.timedout()) { \
    SET_ERROR(handle, ETIMEDOUT, \
              "Time out talking to management server"); \
    ndb_mgm_disconnect_quiet(h); \
    DBUG_RETURN(ret); \
  }

/*****************************************************************************
 * Handles
 *****************************************************************************/

extern "C"
NdbMgmHandle
ndb_mgm_create_handle()
{
  DBUG_ENTER("ndb_mgm_create_handle");
  NdbMgmHandle h     =
    (NdbMgmHandle)my_malloc(sizeof(ndb_mgm_handle),MYF(MY_WME));
  h->connected       = 0;
  h->last_error      = 0;
  h->last_error_line = 0;
  h->socket          = NDB_INVALID_SOCKET;
  h->timeout         = 60000;
  h->cfg_i           = -1;
  h->errstream       = stdout;
  h->m_name          = 0;
  h->m_bindaddress   = 0;

  strncpy(h->last_error_desc, "No error", NDB_MGM_MAX_ERR_DESC_SIZE);

  new (&(h->cfg)) LocalConfig;
  h->cfg.init(0, 0);

#ifdef MGMAPI_LOG
  h->logfile = 0;
#endif

  h->mgmd_version_major= -1;
  h->mgmd_version_minor= -1;
  h->mgmd_version_build= -1;

  DBUG_PRINT("info", ("handle: 0x%lx", (long) h));
  DBUG_RETURN(h);
}

extern "C"
void
ndb_mgm_set_name(NdbMgmHandle handle, const char *name)
{
  my_free(handle->m_name);
  handle->m_name= my_strdup(name, MYF(MY_WME));
}

extern "C"
int
ndb_mgm_set_connectstring(NdbMgmHandle handle, const char * mgmsrv)
{
  DBUG_ENTER("ndb_mgm_set_connectstring");
  DBUG_PRINT("info", ("handle: 0x%lx", (long) handle));
  handle->cfg.~LocalConfig();
  new (&(handle->cfg)) LocalConfig;
  if (!handle->cfg.init(mgmsrv, 0) ||
      handle->cfg.ids.size() == 0)
  {
    handle->cfg.~LocalConfig();
    new (&(handle->cfg)) LocalConfig;
    handle->cfg.init(0, 0); /* reset the LocalConfig */
    SET_ERROR(handle, NDB_MGM_ILLEGAL_CONNECT_STRING, mgmsrv ? mgmsrv : "");
    DBUG_RETURN(-1);
  }
  handle->cfg_i= -1;
  DBUG_RETURN(0);
}

extern "C"
int
ndb_mgm_set_bindaddress(NdbMgmHandle handle, const char * arg)
{
  DBUG_ENTER("ndb_mgm_set_bindaddress");
  if (handle->m_bindaddress)
    free(handle->m_bindaddress);

  if (arg)
    handle->m_bindaddress = strdup(arg);
  else
    handle->m_bindaddress = 0;

  DBUG_RETURN(0);
}

/**
 * Destroy a handle
 */
extern "C"
void
ndb_mgm_destroy_handle(NdbMgmHandle * handle)
{
  DBUG_ENTER("ndb_mgm_destroy_handle");
  if(!handle)
    DBUG_VOID_RETURN;
  DBUG_PRINT("info", ("handle: 0x%lx", (long) (* handle)));
  /**
   * important! only disconnect if connected
   * other code relies on this
   */
  if((* handle)->connected){
    ndb_mgm_disconnect(* handle);
  }
#ifdef MGMAPI_LOG
  if ((* handle)->logfile != 0){
    fclose((* handle)->logfile);
    (* handle)->logfile = 0;
  }
#endif
  (*handle)->cfg.~LocalConfig();
  my_free((*handle)->m_name);
  if ((*handle)->m_bindaddress)
    free((*handle)->m_bindaddress);
  my_free(* handle);
  * handle = 0;
  DBUG_VOID_RETURN;
}

extern "C" 
void
ndb_mgm_set_error_stream(NdbMgmHandle handle, FILE * file)
{
  handle->errstream = file;
}

/*****************************************************************************
 * Error handling
 *****************************************************************************/

/**
 * Get latest error associated with a handle
 */
extern "C"
int
ndb_mgm_get_latest_error(const NdbMgmHandle h)
{
  return h->last_error;
}

extern "C"
const char *
ndb_mgm_get_latest_error_desc(const NdbMgmHandle h){
  return h->last_error_desc;
}

extern "C"
int
ndb_mgm_get_latest_error_line(const NdbMgmHandle h)
{
  return h->last_error_line;
}

extern "C"
const char *
ndb_mgm_get_latest_error_msg(const NdbMgmHandle h)
{
  for (int i=0; i<ndb_mgm_noOfErrorMsgs; i++) {
    if (ndb_mgm_error_msgs[i].code == h->last_error)
      return ndb_mgm_error_msgs[i].msg;
  }

  return "Error"; // Unknown Error message
}

/*
 * Call an operation, and return the reply
 */
static const Properties *
ndb_mgm_call(NdbMgmHandle handle, const ParserRow<ParserDummy> *command_reply,
	     const char *cmd, const Properties *cmd_args) 
{
  DBUG_ENTER("ndb_mgm_call");
  DBUG_PRINT("enter",("handle->socket: %d, cmd: %s",
		      handle->socket, cmd));
  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);

  out.println(cmd);
#ifdef MGMAPI_LOG
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
      Uint64 val_64;
      BaseString val_s;

      cmd_args->getTypeOf(name, &t);
      switch(t) {
      case PropertiesType_Uint32:
	cmd_args->get(name, &val_i);
	out.println("%s: %d", name, val_i);
	break;
      case PropertiesType_Uint64:
	cmd_args->get(name, &val_64);
	out.println("%s: %Ld", name, val_64);
	break;
      case PropertiesType_char:
	cmd_args->get(name, val_s);
	out.println("%s: %s", name, val_s.c_str());
	break;
      case PropertiesType_Properties:
	DBUG_PRINT("info",("Ignoring PropertiesType_Properties."));
	/* Ignore */
	break;
      default:
	DBUG_PRINT("info",("Ignoring PropertiesType: %d.",t));
      }
    }
#ifdef MGMAPI_LOG
  /** 
   * Print arguments to  log file
   */
  cmd_args->print(handle->logfile, "OUT: ");
#endif
  }
  out.println("");

  DBUG_CHECK_TIMEDOUT_RET(handle, in, out, NULL);

  Parser_t::Context ctx;
  ParserDummy session(handle->socket);
  Parser_t parser(command_reply, in, true, true, true);

  const Properties* p = parser.parse(ctx, session);
  if (p == NULL){
    if(!ndb_mgm_is_connected(handle)) {
      DBUG_CHECK_TIMEDOUT_RET(handle, in, out, NULL);
      DBUG_RETURN(NULL);
    }
    else
    {
      DBUG_CHECK_TIMEDOUT_RET(handle, in, out, NULL);
      if(ctx.m_status==Parser_t::Eof
	 || ctx.m_status==Parser_t::NoLine)
      {
	ndb_mgm_disconnect(handle);
        DBUG_CHECK_TIMEDOUT_RET(handle, in, out, NULL);
	DBUG_RETURN(NULL);
      }
      /**
       * Print some info about why the parser returns NULL
       */
      fprintf(handle->errstream,
	      "Error in mgm protocol parser. cmd: >%s< status: %d curr: %s\n",
	      cmd, (Uint32)ctx.m_status,
              (ctx.m_currentToken)?ctx.m_currentToken:"NULL");
      DBUG_PRINT("info",("ctx.status: %d, ctx.m_currentToken: %s",
		         ctx.m_status, ctx.m_currentToken));
    }
  }
#ifdef MGMAPI_LOG
  else {
    /** 
     * Print reply to log file
     */
    p->print(handle->logfile, "IN: ");
  }
#endif

  if(p && (in.timedout() || out.timedout()))
    delete p;
  DBUG_CHECK_TIMEDOUT_RET(handle, in, out, NULL);
  DBUG_RETURN(p);
}

/**
 * Returns true if connected
 */
extern "C"
int ndb_mgm_is_connected(NdbMgmHandle handle)
{
  if(!handle)
    return 0;

  if(handle->connected)
  {
    if(Ndb_check_socket_hup(handle->socket))
    {
      handle->connected= 0;
      NDB_CLOSE_SOCKET(handle->socket);
    }
  }
  return handle->connected;
}

extern "C"
int ndb_mgm_set_connect_timeout(NdbMgmHandle handle, unsigned int seconds)
{
  return ndb_mgm_set_timeout(handle, seconds*1000);
  return 0;
}

extern "C"
int ndb_mgm_set_timeout(NdbMgmHandle handle, unsigned int timeout_ms)
{
  if(!handle)
    return -1;

  handle->timeout= timeout_ms;
  return 0;
}

extern "C"
int ndb_mgm_number_of_mgmd_in_connect_string(NdbMgmHandle handle)
{
  int count=0;
  Uint32 i;
  LocalConfig &cfg= handle->cfg;

  for (i = 0; i < cfg.ids.size(); i++)
  {
    if (cfg.ids[i].type != MgmId_TCP)
      continue;
    count++;
  }
  return count;
}

/**
 * Connect to a management server
 */
extern "C"
int
ndb_mgm_connect(NdbMgmHandle handle, int no_retries,
		int retry_delay_in_seconds, int verbose)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_connect");
  CHECK_HANDLE(handle, -1);

  DBUG_ENTER("ndb_mgm_connect");
#ifdef MGMAPI_LOG
  /**
  * Open the log file
  */
  char logname[64];
  BaseString::snprintf(logname, 64, "mgmapi.log");
  handle->logfile = fopen(logname, "w");
#endif
  char buf[1024];

  /**
   * Do connect
   */
  LocalConfig &cfg= handle->cfg;
  NDB_SOCKET_TYPE sockfd= NDB_INVALID_SOCKET;
  Uint32 i;
  SocketClient s(0, 0);
  s.set_connect_timeout((handle->timeout+999)/1000);
  if (!s.init())
  {
    fprintf(handle->errstream, 
	    "Unable to create socket, "
	    "while trying to connect with connect string: %s\n",
	    cfg.makeConnectString(buf,sizeof(buf)));

    setError(handle, NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, __LINE__,
	    "Unable to create socket, "
	    "while trying to connect with connect string: %s\n",
	    cfg.makeConnectString(buf,sizeof(buf)));
    DBUG_RETURN(-1);
  }

  if (handle->m_bindaddress)
  {
    BaseString::snprintf(buf, sizeof(buf), handle->m_bindaddress);
    unsigned short portno = 0;
    char * port = strchr(buf, ':');
    if (port != 0)
    {
      portno = atoi(port+1);
      * port = 0;
    }
    int err;
    if ((err = s.bind(buf, portno)) != 0)
    {
      fprintf(handle->errstream, 
	      "Unable to bind local address %s errno: %d, "
	      "while trying to connect with connect string: %s\n",
	      handle->m_bindaddress, err,
	      cfg.makeConnectString(buf,sizeof(buf)));
      
      setError(handle, NDB_MGM_BIND_ADDRESS, __LINE__,
	       "Unable to bind local address %s errno: %d, "
	       "while trying to connect with connect string: %s\n",
	       handle->m_bindaddress, err,
	       cfg.makeConnectString(buf,sizeof(buf)));
      DBUG_RETURN(-1);
    }
  }
  
  while (sockfd == NDB_INVALID_SOCKET)
  {
    // do all the mgmt servers
    for (i = 0; i < cfg.ids.size(); i++)
    {
      if (cfg.ids[i].type != MgmId_TCP)
	continue;
      sockfd = s.connect(cfg.ids[i].name.c_str(), cfg.ids[i].port);
      if (sockfd != NDB_INVALID_SOCKET)
	break;
    }
    if (sockfd != NDB_INVALID_SOCKET)
      break;
#ifndef DBUG_OFF
    {
      DBUG_PRINT("info",("Unable to connect with connect string: %s",
			 cfg.makeConnectString(buf,sizeof(buf))));
    }
#endif
    if (verbose > 0) {
      fprintf(handle->errstream, 
	      "Unable to connect with connect string: %s\n",
	      cfg.makeConnectString(buf,sizeof(buf)));
      verbose= -1;
    }
    if (no_retries == 0) {
      setError(handle, NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, __LINE__,
	       "Unable to connect with connect string: %s",
	       cfg.makeConnectString(buf,sizeof(buf)));
      if (verbose == -2)
	fprintf(handle->errstream, ", failed.\n");
      DBUG_RETURN(-1);
    }
    if (verbose == -1) {
      fprintf(handle->errstream, "Retrying every %d seconds", 
	      retry_delay_in_seconds);
      if (no_retries > 0)
	fprintf(handle->errstream, ". Attempts left:");
      else
	fprintf(handle->errstream, ", until connected.");
      fflush(handle->errstream);
      verbose= -2;
    }
    if (no_retries > 0) {
      if (verbose == -2) {
	fprintf(handle->errstream, " %d", no_retries);
	fflush(handle->errstream);
      }
      no_retries--;
    }
    NdbSleep_SecSleep(retry_delay_in_seconds);
  }
  if (verbose == -2)
  {
    fprintf(handle->errstream, "\n");
    fflush(handle->errstream);
  }
  handle->cfg_i = i;
  
  handle->socket    = sockfd;
  handle->connected = 1;

  DBUG_RETURN(0);
}

/**
 * Only used for low level testing
 * Never to be used by end user.
 * Or anybody who doesn't know exactly what they're doing.
 */
extern "C"
int
ndb_mgm_get_fd(NdbMgmHandle handle)
{
  return handle->socket;
}

/**
 * Disconnect from mgm server without error checking
 * Should be used internally only.
 * e.g. on timeout, we leave NdbMgmHandle disconnected
 */
extern "C"
int
ndb_mgm_disconnect_quiet(NdbMgmHandle handle)
{
  NDB_CLOSE_SOCKET(handle->socket);
  handle->socket = NDB_INVALID_SOCKET;
  handle->connected = 0;

  return 0;
}

/**
 * Disconnect from a mgm server
 */
extern "C"
int
ndb_mgm_disconnect(NdbMgmHandle handle)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_disconnect");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  return ndb_mgm_disconnect_quiet(handle);
}

struct ndb_mgm_type_atoi 
{
  const char * str;
  const char * alias;
  enum ndb_mgm_node_type value;
};

static struct ndb_mgm_type_atoi type_values[] = 
{
  { "NDB", "ndbd", NDB_MGM_NODE_TYPE_NDB},
  { "API", "mysqld", NDB_MGM_NODE_TYPE_API },
  { "MGM", "ndb_mgmd", NDB_MGM_NODE_TYPE_MGM }
};

const int no_of_type_values = (sizeof(type_values) / 
			       sizeof(ndb_mgm_type_atoi));

extern "C"
ndb_mgm_node_type
ndb_mgm_match_node_type(const char * type)
{
  if(type == 0)
    return NDB_MGM_NODE_TYPE_UNKNOWN;
  
  for(int i = 0; i<no_of_type_values; i++)
    if(strcmp(type, type_values[i].str) == 0)
      return type_values[i].value;
    else if(strcmp(type, type_values[i].alias) == 0)
      return type_values[i].value;
  
  return NDB_MGM_NODE_TYPE_UNKNOWN;
}

extern "C"
const char * 
ndb_mgm_get_node_type_string(enum ndb_mgm_node_type type)
{
  for(int i = 0; i<no_of_type_values; i++)
    if(type_values[i].value == type)
      return type_values[i].str;
  return 0;
}

extern "C"
const char * 
ndb_mgm_get_node_type_alias_string(enum ndb_mgm_node_type type, const char** str)
{
  for(int i = 0; i<no_of_type_values; i++)
    if(type_values[i].value == type)
      {
	if (str)
	  *str= type_values[i].str;
	return type_values[i].alias;
      }
  return 0;
}

struct ndb_mgm_status_atoi {
  const char * str;
  enum ndb_mgm_node_status value;
};

static struct ndb_mgm_status_atoi status_values[] = 
{
  { "UNKNOWN", NDB_MGM_NODE_STATUS_UNKNOWN },
  { "NO_CONTACT", NDB_MGM_NODE_STATUS_NO_CONTACT },
  { "NOT_STARTED", NDB_MGM_NODE_STATUS_NOT_STARTED },
  { "STARTING", NDB_MGM_NODE_STATUS_STARTING },
  { "STARTED", NDB_MGM_NODE_STATUS_STARTED },
  { "SHUTTING_DOWN", NDB_MGM_NODE_STATUS_SHUTTING_DOWN },
  { "RESTARTING", NDB_MGM_NODE_STATUS_RESTARTING },
  { "SINGLE USER MODE", NDB_MGM_NODE_STATUS_SINGLEUSER }
};

const int no_of_status_values = (sizeof(status_values) / 
				 sizeof(ndb_mgm_status_atoi));

extern "C"
ndb_mgm_node_status
ndb_mgm_match_node_status(const char * status)
{
  if(status == 0)
    return NDB_MGM_NODE_STATUS_UNKNOWN;
  
  for(int i = 0; i<no_of_status_values; i++)
    if(strcmp(status, status_values[i].str) == 0)
      return status_values[i].value;

  return NDB_MGM_NODE_STATUS_UNKNOWN;
}

extern "C"
const char * 
ndb_mgm_get_node_status_string(enum ndb_mgm_node_status status)
{
  int i;
  for(i = 0; i<no_of_status_values; i++)
    if(status_values[i].value == status)
      return status_values[i].str;

  for(i = 0; i<no_of_status_values; i++)
    if(status_values[i].value == NDB_MGM_NODE_STATUS_UNKNOWN)
      return status_values[i].str;
  
  return 0;
}

static int
status_ackumulate(struct ndb_mgm_node_state * state,
		  const char * field,
		  const char * value)
{
  if(strcmp("type", field) == 0){
    state->node_type = ndb_mgm_match_node_type(value);
  } else if(strcmp("status", field) == 0){
    state->node_status = ndb_mgm_match_node_status(value);
  } else if(strcmp("startphase", field) == 0){
    state->start_phase = atoi(value);
  } else if(strcmp("dynamic_id", field) == 0){
    state->dynamic_id = atoi(value);
  } else if(strcmp("node_group", field) == 0){
    state->node_group = atoi(value);
  } else if(strcmp("version", field) == 0){
    state->version = atoi(value);
  } else if(strcmp("connect_count", field) == 0){
    state->connect_count = atoi(value);    
  } else if(strcmp("address", field) == 0){
    strncpy(state->connect_address, value, sizeof(state->connect_address));
    state->connect_address[sizeof(state->connect_address)-1]= 0;
  } else {
    ndbout_c("Unknown field: %s", field);
  }
  return 0;
}

/**
 * Compare function for qsort() that sorts ndb_mgm_node_state in
 * node_id order
 */
static int
cmp_state(const void *_a, const void *_b) 
{
  struct ndb_mgm_node_state *a, *b;

  a = (struct ndb_mgm_node_state *)_a;
  b = (struct ndb_mgm_node_state *)_b;

  if (a->node_id > b->node_id)
    return 1;
  return -1;
}

extern "C"
struct ndb_mgm_cluster_state * 
ndb_mgm_get_status(NdbMgmHandle handle)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_status");
  CHECK_HANDLE(handle, NULL);
  CHECK_CONNECTED(handle, NULL);

  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);

  out.println("get status");
  out.println("");

  CHECK_TIMEDOUT_RET(handle, in, out, NULL);

  char buf[1024];
  if(!in.gets(buf, sizeof(buf)))
  {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, "Probably disconnected");
    return NULL;
  }
  if(strcmp("node status\n", buf) != 0) {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL);
    ndbout << in.timedout() << " " << out.timedout() << buf << endl;
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    return NULL;
  }
  if(!in.gets(buf, sizeof(buf)))
  {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, "Probably disconnected");
    return NULL;
  }

  BaseString tmp(buf);
  Vector<BaseString> split;
  tmp.split(split, ":");
  if(split.size() != 2){
    CHECK_TIMEDOUT_RET(handle, in, out, NULL);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    return NULL;
  }

  if(!(split[0].trim() == "nodes")){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    return NULL;
  }

  const int noOfNodes = atoi(split[1].c_str());

  ndb_mgm_cluster_state *state = (ndb_mgm_cluster_state*)
    malloc(sizeof(ndb_mgm_cluster_state)+
	   noOfNodes*(sizeof(ndb_mgm_node_state)+sizeof("000.000.000.000#")));

  if(!state)
  {
    SET_ERROR(handle, NDB_MGM_OUT_OF_MEMORY,
              "Allocating ndb_mgm_cluster_state");
    return NULL;
  }

  state->no_of_nodes= noOfNodes;
  ndb_mgm_node_state * ptr = &state->node_states[0];
  int nodeId = 0;
  int i;
  for (i= 0; i < noOfNodes; i++) {
    state->node_states[i].connect_address[0]= 0;
  }
  i = -1; ptr--;
  for(; i<noOfNodes; ){
    if(!in.gets(buf, sizeof(buf)))
    {
      free(state);
      if(in.timedout() || out.timedout())
        SET_ERROR(handle, ETIMEDOUT,
                  "Time out talking to management server");
      else
        SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
                  "Probably disconnected");
      return NULL;
    }
    tmp.assign(buf);

    if(tmp.trim() == ""){
      break;
    }
    
    Vector<BaseString> split2;
    tmp.split(split2, ":.", 4);
    if(split2.size() != 4)
      break;
    
    const int id = atoi(split2[1].c_str());
    if(id != nodeId){
      ptr++;
      i++;
      nodeId = id;
      ptr->node_id = id;
    }

    split2[3].trim(" \t\n");

    if(status_ackumulate(ptr,split2[2].c_str(), split2[3].c_str()) != 0) {
      break;
    }
  }

  if(i+1 != noOfNodes){
    free(state);
    CHECK_TIMEDOUT_RET(handle, in, out, NULL);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, "Node count mismatch");
    return NULL;
  }

  qsort(state->node_states, state->no_of_nodes, sizeof(state->node_states[0]),
	cmp_state);
  return state;
}

extern "C"
int 
ndb_mgm_enter_single_user(NdbMgmHandle handle,
			  unsigned int nodeId,
			  struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_enter_single_user");
  const ParserRow<ParserDummy> enter_single_reply[] = {
    MGM_CMD("enter single user reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("nodeId", nodeId);
  const Properties *reply;
  reply = ndb_mgm_call(handle, enter_single_reply, "enter single user", &args);
  CHECK_REPLY(handle, reply, -1);

  BaseString result;
  reply->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE, 
	      result.c_str());
    delete reply;
    return -1;
  }

  delete reply;
  return 0;
}


extern "C"
int 
ndb_mgm_exit_single_user(NdbMgmHandle handle, struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_exit_single_user");
  const ParserRow<ParserDummy> exit_single_reply[] = {
    MGM_CMD("exit single user reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  const Properties *reply;
  reply = ndb_mgm_call(handle, exit_single_reply, "exit single user", 0);
  CHECK_REPLY(handle, reply, -1);

  const char * buf;
  reply->get("result", &buf);
  if(strcmp(buf,"Ok")!=0) {
    SET_ERROR(handle, NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE, buf);
    delete reply;    
    return -1;
  }

  delete reply;
  return 0;
}

extern "C"
int 
ndb_mgm_stop(NdbMgmHandle handle, int no_of_nodes, const int * node_list)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_stop");
  return ndb_mgm_stop2(handle, no_of_nodes, node_list, 0);
}

extern "C"
int
ndb_mgm_stop2(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
	      int abort)
{
  int disconnect;
  return ndb_mgm_stop3(handle, no_of_nodes, node_list, abort, &disconnect);
}


extern "C"
int
ndb_mgm_stop3(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
	      int abort, int *disconnect)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_stop3");
  const ParserRow<ParserDummy> stop_reply_v1[] = {
    MGM_CMD("stop reply", NULL, ""),
    MGM_ARG("stopped", Int, Optional, "No of stopped nodes"),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  const ParserRow<ParserDummy> stop_reply_v2[] = {
    MGM_CMD("stop reply", NULL, ""),
    MGM_ARG("stopped", Int, Optional, "No of stopped nodes"),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_ARG("disconnect", Int, Mandatory, "Need to disconnect"),
    MGM_END()
  };

  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  if(handle->mgmd_version_build==-1)
  {
    char verstr[50];
    if(!ndb_mgm_get_version(handle,
                        &(handle->mgmd_version_major),
                        &(handle->mgmd_version_minor),
                        &(handle->mgmd_version_build),
                        sizeof(verstr),
                            verstr))
    {
      return -1;
    }
  }
  int use_v2= ((handle->mgmd_version_major==5)
    && (
        (handle->mgmd_version_minor==0 && handle->mgmd_version_build>=21)
        ||(handle->mgmd_version_minor==1 && handle->mgmd_version_build>=12)
        ||(handle->mgmd_version_minor>1)
        )
               )
    || (handle->mgmd_version_major>5);

  if(no_of_nodes < -1){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NUMBER_OF_NODES, 
	      "Negative number of nodes requested to stop");
    return -1;
  }

  Uint32 stoppedNoOfNodes = 0;
  if(no_of_nodes <= 0){
    /**
     * All nodes should be stopped (all or just db)
     */
    Properties args;
    args.put("abort", abort);
    if(use_v2)
      args.put("stop", (no_of_nodes==-1)?"mgm,db":"db");
    const Properties *reply;
    if(use_v2)
      reply = ndb_mgm_call(handle, stop_reply_v2, "stop all", &args);
    else
      reply = ndb_mgm_call(handle, stop_reply_v1, "stop all", &args);
    CHECK_REPLY(handle, reply, -1);

    if(!reply->get("stopped", &stoppedNoOfNodes)){
      SET_ERROR(handle, NDB_MGM_STOP_FAILED, 
		"Could not get number of stopped nodes from mgm server");
      delete reply;
      return -1;
    }
    if(use_v2)
      reply->get("disconnect", (Uint32*)disconnect);
    else
      *disconnect= 0;
    BaseString result;
    reply->get("result", result);
    if(strcmp(result.c_str(), "Ok") != 0) {
      SET_ERROR(handle, NDB_MGM_STOP_FAILED, result.c_str());
      delete reply;
      return -1;
    }
    delete reply;
    return stoppedNoOfNodes;
  }

  /**
   * A list of database nodes should be stopped
   */
  Properties args;

  BaseString node_list_str;
  node_list_str.assfmt("%d", node_list[0]);
  for(int node = 1; node < no_of_nodes; node++)
    node_list_str.appfmt(" %d", node_list[node]);
  
  args.put("node", node_list_str.c_str());
  args.put("abort", abort);

  const Properties *reply;
  if(use_v2)
    reply = ndb_mgm_call(handle, stop_reply_v2, "stop v2", &args);
  else
    reply = ndb_mgm_call(handle, stop_reply_v1, "stop", &args);

  CHECK_REPLY(handle, reply, stoppedNoOfNodes);
  if(!reply->get("stopped", &stoppedNoOfNodes)){
    SET_ERROR(handle, NDB_MGM_STOP_FAILED, 
	      "Could not get number of stopped nodes from mgm server");
    delete reply;
    return -1;
  }
  if(use_v2)
    reply->get("disconnect", (Uint32*)disconnect);
  else
    *disconnect= 0;
  BaseString result;
  reply->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_STOP_FAILED, result.c_str());
    delete reply;
    return -1;
  }
  delete reply;
  return stoppedNoOfNodes;
}

extern "C"
int
ndb_mgm_restart(NdbMgmHandle handle, int no_of_nodes, const int *node_list) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_restart");
  return ndb_mgm_restart2(handle, no_of_nodes, node_list, 0, 0, 0);
}

extern "C"
int
ndb_mgm_restart2(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
		 int initial, int nostart, int abort)
{
  int disconnect;

  return ndb_mgm_restart3(handle, no_of_nodes, node_list, initial, nostart,
                          abort, &disconnect);
}

extern "C"
int
ndb_mgm_restart3(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
		 int initial, int nostart, int abort, int *disconnect)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_restart3");
  Uint32 restarted = 0;
  const ParserRow<ParserDummy> restart_reply_v1[] = {
    MGM_CMD("restart reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_ARG("restarted", Int, Optional, "No of restarted nodes"),
    MGM_END()
  };
  const ParserRow<ParserDummy> restart_reply_v2[] = {
    MGM_CMD("restart reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_ARG("restarted", Int, Optional, "No of restarted nodes"),
    MGM_ARG("disconnect", Int, Optional, "Disconnect to apply"),
    MGM_END()
  };

  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  if(handle->mgmd_version_build==-1)
  {
    char verstr[50];
    if(!ndb_mgm_get_version(handle,
                        &(handle->mgmd_version_major),
                        &(handle->mgmd_version_minor),
                        &(handle->mgmd_version_build),
                        sizeof(verstr),
                            verstr))
    {
      return -1;
    }
  }
  int use_v2= ((handle->mgmd_version_major==5)
    && (
        (handle->mgmd_version_minor==0 && handle->mgmd_version_build>=21)
        ||(handle->mgmd_version_minor==1 && handle->mgmd_version_build>=12)
        ||(handle->mgmd_version_minor>1)
        )
               )
    || (handle->mgmd_version_major>5);

  if(no_of_nodes < 0){
    SET_ERROR(handle, NDB_MGM_RESTART_FAILED, 
	      "Restart requested of negative number of nodes");
    return -1;
  }
  
  if(no_of_nodes == 0) {
    Properties args;    
    args.put("abort", abort);
    args.put("initialstart", initial);
    args.put("nostart", nostart);
    const Properties *reply;
    const int timeout = handle->timeout;
    handle->timeout= 5*60*1000; // 5 minutes
    reply = ndb_mgm_call(handle, restart_reply_v1, "restart all", &args);
    handle->timeout= timeout;
    CHECK_REPLY(handle, reply, -1);

    BaseString result;
    reply->get("result", result);
    if(strcmp(result.c_str(), "Ok") != 0) {
      SET_ERROR(handle, NDB_MGM_RESTART_FAILED, result.c_str());
      delete reply;
      return -1;
    }
    if(!reply->get("restarted", &restarted)){
      SET_ERROR(handle, NDB_MGM_RESTART_FAILED, 
		"Could not get restarted number of nodes from mgm server");
      delete reply;
      return -1;
    }
    delete reply;
    return restarted;
  }      

  BaseString node_list_str;
  node_list_str.assfmt("%d", node_list[0]);
  for(int node = 1; node < no_of_nodes; node++)
    node_list_str.appfmt(" %d", node_list[node]);

  Properties args;
  
  args.put("node", node_list_str.c_str());
  args.put("abort", abort);
  args.put("initialstart", initial);
  args.put("nostart", nostart);

  const Properties *reply;
  const int timeout = handle->timeout;
  handle->timeout= 5*60*1000; // 5 minutes
  if(use_v2)
    reply = ndb_mgm_call(handle, restart_reply_v2, "restart node v2", &args);
  else
    reply = ndb_mgm_call(handle, restart_reply_v1, "restart node", &args);
  handle->timeout= timeout;
  if(reply != NULL) {
    BaseString result;
    reply->get("result", result);
    if(strcmp(result.c_str(), "Ok") != 0) {
      SET_ERROR(handle, NDB_MGM_RESTART_FAILED, result.c_str());
      delete reply;
      return -1;
    }
    reply->get("restarted", &restarted);
    if(use_v2)
      reply->get("disconnect", (Uint32*)disconnect);
    else
      *disconnect= 0;
    delete reply;
  } 
  
  return restarted;
}

static const char *clusterlog_severity_names[]=
  { "enabled", "debug", "info", "warning", "error", "critical", "alert" };

struct ndb_mgm_event_severities 
{
  const char* name;
  enum ndb_mgm_event_severity severity;
} clusterlog_severities[] = {
  { clusterlog_severity_names[0], NDB_MGM_EVENT_SEVERITY_ON },
  { clusterlog_severity_names[1], NDB_MGM_EVENT_SEVERITY_DEBUG },
  { clusterlog_severity_names[2], NDB_MGM_EVENT_SEVERITY_INFO },
  { clusterlog_severity_names[3], NDB_MGM_EVENT_SEVERITY_WARNING },
  { clusterlog_severity_names[4], NDB_MGM_EVENT_SEVERITY_ERROR },
  { clusterlog_severity_names[5], NDB_MGM_EVENT_SEVERITY_CRITICAL },
  { clusterlog_severity_names[6], NDB_MGM_EVENT_SEVERITY_ALERT },
  { "all",                        NDB_MGM_EVENT_SEVERITY_ALL },
  { 0,                            NDB_MGM_ILLEGAL_EVENT_SEVERITY },
};

extern "C"
ndb_mgm_event_severity
ndb_mgm_match_event_severity(const char * name)
{
  if(name == 0)
    return NDB_MGM_ILLEGAL_EVENT_SEVERITY;
  
  for(int i = 0; clusterlog_severities[i].name !=0 ; i++)
    if(strcasecmp(name, clusterlog_severities[i].name) == 0)
      return clusterlog_severities[i].severity;

  return NDB_MGM_ILLEGAL_EVENT_SEVERITY;
}

extern "C"
const char * 
ndb_mgm_get_event_severity_string(enum ndb_mgm_event_severity severity)
{
  int i= (int)severity;
  if (i >= 0 && i < (int)NDB_MGM_EVENT_SEVERITY_ALL)
    return clusterlog_severity_names[i];
  for(i = (int)NDB_MGM_EVENT_SEVERITY_ALL; clusterlog_severities[i].name != 0; i++)
    if(clusterlog_severities[i].severity == severity)
      return clusterlog_severities[i].name;
  return 0;
}

extern "C"
int
ndb_mgm_get_clusterlog_severity_filter(NdbMgmHandle handle, 
				       struct ndb_mgm_severity* severity,
				       unsigned int severity_size) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_clusterlog_severity_filter");
  const ParserRow<ParserDummy> getinfo_reply[] = {
    MGM_CMD("clusterlog", NULL, ""),
    MGM_ARG(clusterlog_severity_names[0], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[1], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[2], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[3], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[4], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[5], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[6], Int, Mandatory, ""),
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getinfo_reply, "get info clusterlog", &args);
  CHECK_REPLY(handle, reply, -1);
  
  for(unsigned int i=0; i < severity_size; i++) {
    reply->get(clusterlog_severity_names[severity[i].category], &severity[i].value);
  }
  return severity_size;
}

extern "C"
const unsigned int *
ndb_mgm_get_clusterlog_severity_filter_old(NdbMgmHandle handle) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_clusterlog_severity_filter");
  static unsigned int enabled[(int)NDB_MGM_EVENT_SEVERITY_ALL]=
    {0,0,0,0,0,0,0};
  const ParserRow<ParserDummy> getinfo_reply[] = {
    MGM_CMD("clusterlog", NULL, ""),
    MGM_ARG(clusterlog_severity_names[0], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[1], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[2], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[3], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[4], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[5], Int, Mandatory, ""),
    MGM_ARG(clusterlog_severity_names[6], Int, Mandatory, ""),
  };
  CHECK_HANDLE(handle, NULL);
  CHECK_CONNECTED(handle, NULL);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getinfo_reply, "get info clusterlog", &args);
  CHECK_REPLY(handle, reply, NULL);
  
  for(int i=0; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++) {
    reply->get(clusterlog_severity_names[i], &enabled[i]);
  }
  return enabled;
}

extern "C"
int 
ndb_mgm_set_clusterlog_severity_filter(NdbMgmHandle handle, 
				       enum ndb_mgm_event_severity severity,
				       int enable,
				       struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR,
	    "Executing: ndb_mgm_set_clusterlog_severity_filter");
  const ParserRow<ParserDummy> filter_reply[] = {
    MGM_CMD("set logfilter reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("level", severity);
  args.put("enable", enable);
  
  const Properties *reply;
  reply = ndb_mgm_call(handle, filter_reply, "set logfilter", &args);
  CHECK_REPLY(handle, reply, retval);

  BaseString result;
  reply->get("result", result);

  if (strcmp(result.c_str(), "1") == 0)
    retval = 1;
  else if (strcmp(result.c_str(), "0") == 0)
    retval = 0;
  else
  {
    SET_ERROR(handle, EINVAL, result.c_str());
  }
  delete reply;
  return retval;
}

struct ndb_mgm_event_categories 
{
  const char* name;
  enum ndb_mgm_event_category category;
} categories[] = {
  { "STARTUP", NDB_MGM_EVENT_CATEGORY_STARTUP },
  { "SHUTDOWN", NDB_MGM_EVENT_CATEGORY_SHUTDOWN },
  { "STATISTICS", NDB_MGM_EVENT_CATEGORY_STATISTIC },
  { "NODERESTART", NDB_MGM_EVENT_CATEGORY_NODE_RESTART },
  { "CONNECTION", NDB_MGM_EVENT_CATEGORY_CONNECTION },
  { "CHECKPOINT", NDB_MGM_EVENT_CATEGORY_CHECKPOINT },
  { "DEBUG", NDB_MGM_EVENT_CATEGORY_DEBUG },
  { "INFO", NDB_MGM_EVENT_CATEGORY_INFO },
  { "ERROR", NDB_MGM_EVENT_CATEGORY_ERROR },
  { "BACKUP", NDB_MGM_EVENT_CATEGORY_BACKUP },
  { "CONGESTION", NDB_MGM_EVENT_CATEGORY_CONGESTION },
  { 0, NDB_MGM_ILLEGAL_EVENT_CATEGORY }
};

extern "C"
ndb_mgm_event_category
ndb_mgm_match_event_category(const char * status)
{
  if(status == 0)
    return NDB_MGM_ILLEGAL_EVENT_CATEGORY;
  
  for(int i = 0; categories[i].name !=0 ; i++)
    if(strcmp(status, categories[i].name) == 0)
      return categories[i].category;

  return NDB_MGM_ILLEGAL_EVENT_CATEGORY;
}

extern "C"
const char * 
ndb_mgm_get_event_category_string(enum ndb_mgm_event_category status)
{
  int i;
  for(i = 0; categories[i].name != 0; i++)
    if(categories[i].category == status)
      return categories[i].name;
  
  return 0;
}

static const char *clusterlog_names[]=
  { "startup", "shutdown", "statistics", "checkpoint", "noderestart", "connection", "info", "warning", "error", "congestion", "debug", "backup" };

extern "C"
int
ndb_mgm_get_clusterlog_loglevel(NdbMgmHandle handle, 
				struct ndb_mgm_loglevel* loglevel,
				unsigned int loglevel_size)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_clusterlog_loglevel");
  int loglevel_count = loglevel_size;
  const ParserRow<ParserDummy> getloglevel_reply[] = {
    MGM_CMD("get cluster loglevel", NULL, ""),
    MGM_ARG(clusterlog_names[0], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[1], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[2], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[3], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[4], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[5], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[6], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[7], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[8], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[9], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[10], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[11], Int, Mandatory, ""),
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getloglevel_reply, "get cluster loglevel", &args);
  CHECK_REPLY(handle, reply, -1);

  for(int i=0; i < loglevel_count; i++) {
    reply->get(clusterlog_names[loglevel[i].category], &loglevel[i].value);
  }
  return loglevel_count;
}

extern "C"
const unsigned int *
ndb_mgm_get_clusterlog_loglevel_old(NdbMgmHandle handle)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_clusterlog_loglevel");
  int loglevel_count = CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1 ;
  static unsigned int loglevel[CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1] = {0,0,0,0,0,0,0,0,0,0,0,0};
  const ParserRow<ParserDummy> getloglevel_reply[] = {
    MGM_CMD("get cluster loglevel", NULL, ""),
    MGM_ARG(clusterlog_names[0], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[1], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[2], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[3], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[4], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[5], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[6], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[7], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[8], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[9], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[10], Int, Mandatory, ""),
    MGM_ARG(clusterlog_names[11], Int, Mandatory, ""),
  };
  CHECK_HANDLE(handle, NULL);
  CHECK_CONNECTED(handle, NULL);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getloglevel_reply, "get cluster loglevel", &args);
  CHECK_REPLY(handle, reply, NULL);

  for(int i=0; i < loglevel_count; i++) {
    reply->get(clusterlog_names[i], &loglevel[i]);
  }
  return loglevel;
}

extern "C"
int 
ndb_mgm_set_clusterlog_loglevel(NdbMgmHandle handle, int nodeId,
				enum ndb_mgm_event_category cat,
				int level,
				struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, 
	    "Executing: ndb_mgm_set_clusterlog_loglevel");
  const ParserRow<ParserDummy> clusterlog_reply[] = {
    MGM_CMD("set cluster loglevel reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("category", cat);
  args.put("level", level);
  
  const Properties *reply;
  reply = ndb_mgm_call(handle, clusterlog_reply, 
		       "set cluster loglevel", &args);
  CHECK_REPLY(handle, reply, -1);
  
  DBUG_ENTER("ndb_mgm_set_clusterlog_loglevel");
  DBUG_PRINT("enter",("node=%d, category=%d, level=%d", nodeId, cat, level));

  BaseString result;
  reply->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, EINVAL, result.c_str());
    delete reply;
    DBUG_RETURN(-1);
  }
  delete reply;
  DBUG_RETURN(0);
}

extern "C"
int 
ndb_mgm_set_loglevel_node(NdbMgmHandle handle, int nodeId,
			  enum ndb_mgm_event_category category,
			  int level,
			  struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_loglevel_node");
  const ParserRow<ParserDummy> loglevel_reply[] = {
    MGM_CMD("set loglevel reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("category", category);
  args.put("level", level);
  const Properties *reply;
  reply = ndb_mgm_call(handle, loglevel_reply, "set loglevel", &args);
  CHECK_REPLY(handle, reply, -1);

  BaseString result;
  reply->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, EINVAL, result.c_str());
    delete reply;
    return -1;
  }

  delete reply;
  return 0;
}

int
ndb_mgm_listen_event_internal(NdbMgmHandle handle, const int filter[],
			      int parsable)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_listen_event");
  const ParserRow<ParserDummy> stat_reply[] = {
    MGM_CMD("listen event", NULL, ""),
    MGM_ARG("result", Int, Mandatory, "Error message"),
    MGM_ARG("msg", String, Optional, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);

  const char *hostname= ndb_mgm_get_connected_host(handle);
  int port= ndb_mgm_get_connected_port(handle);
  SocketClient s(hostname, port);
  const NDB_SOCKET_TYPE sockfd = s.connect();
  if (sockfd == NDB_INVALID_SOCKET) {
    setError(handle, NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, __LINE__,
	     "Unable to connect to");
    return -1;
  }

  Properties args;

  if (parsable)
    args.put("parsable", parsable);
  {
    BaseString tmp;
    for(int i = 0; filter[i] != 0; i += 2){
      tmp.appfmt("%d=%d ", filter[i+1], filter[i]);
    }
    args.put("filter", tmp.c_str());
  }

  int tmp = handle->socket;
  handle->socket = sockfd;

  const Properties *reply;
  reply = ndb_mgm_call(handle, stat_reply, "listen event", &args);

  handle->socket = tmp;

  if(reply == NULL) {
    close(sockfd);
    CHECK_REPLY(handle, reply, -1);
  }
  delete reply;
  return sockfd;
}

extern "C"
int
ndb_mgm_listen_event(NdbMgmHandle handle, const int filter[])
{
  return ndb_mgm_listen_event_internal(handle,filter,0);
}

extern "C"
int 
ndb_mgm_dump_state(NdbMgmHandle handle, int nodeId, const int * _args,
		   int _num_args, struct ndb_mgm_reply* /* reply */) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_dump_state");
  const ParserRow<ParserDummy> dump_state_reply[] = {
    MGM_CMD("dump state reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  char buf[256];
  buf[0] = 0;
  for (int i = 0; i < _num_args; i++){
    unsigned n = strlen(buf);
    if (n + 20 > sizeof(buf)) {
      SET_ERROR(handle, NDB_MGM_USAGE_ERROR, "arguments too long");
      return -1;
    }
    sprintf(buf + n, "%s%d", i ? " " : "", _args[i]);
  }

  Properties args;
  args.put("node", nodeId);
  args.put("args", buf);

  const Properties *prop;
  prop = ndb_mgm_call(handle, dump_state_reply, "dump state", &args);
  CHECK_REPLY(handle, prop, -1);

  BaseString result;
  prop->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, EINVAL, result.c_str());
    delete prop;
    return -1;
  }

  delete prop;
  return 0;
}

extern "C"
int 
ndb_mgm_start_signallog(NdbMgmHandle handle, int nodeId, 
			struct ndb_mgm_reply* reply) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start_signallog");
  const ParserRow<ParserDummy> start_signallog_reply[] = {
    MGM_CMD("start signallog reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);

  const Properties *prop;
  prop = ndb_mgm_call(handle,
		       start_signallog_reply,
		       "start signallog",
		       &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop != NULL) {
    BaseString result;
    prop->get("result", result);
    if(strcmp(result.c_str(), "Ok") == 0) {
      retval = 0;
    } else {
      SET_ERROR(handle, EINVAL, result.c_str());
      retval = -1;
    }
    delete prop;
  }

  return retval;
}

extern "C"
int 
ndb_mgm_stop_signallog(NdbMgmHandle handle, int nodeId,
		       struct ndb_mgm_reply* reply) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_stop_signallog");
  const ParserRow<ParserDummy> stop_signallog_reply[] = {
    MGM_CMD("stop signallog reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
  Properties args;
  args.put("node", nodeId);

  const Properties *prop;
  prop = ndb_mgm_call(handle, stop_signallog_reply, "stop signallog", &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop != NULL) {
    BaseString result;
    prop->get("result", result);
    if(strcmp(result.c_str(), "Ok") == 0) {
      retval = 0;
    } else {
      SET_ERROR(handle, EINVAL, result.c_str());
      retval = -1;
    }
    delete prop;
  }

  return retval;
}

struct ndb_mgm_signal_log_modes 
{
  const char* name;
  enum ndb_mgm_signal_log_mode mode;
};

extern "C"
int 
ndb_mgm_log_signals(NdbMgmHandle handle, int nodeId, 
		    enum ndb_mgm_signal_log_mode mode, 
		    const char* blockNames,
		    struct ndb_mgm_reply* reply) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_log_signals");
  const ParserRow<ParserDummy> stop_signallog_reply[] = {
    MGM_CMD("log signals reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("blocks", blockNames);

  switch(mode) {
  case NDB_MGM_SIGNAL_LOG_MODE_IN:
    args.put("in", (Uint32)1);
    args.put("out", (Uint32)0);
    break;
  case NDB_MGM_SIGNAL_LOG_MODE_OUT:
    args.put("in", (Uint32)0);
    args.put("out", (Uint32)1);
    break;
  case NDB_MGM_SIGNAL_LOG_MODE_INOUT:
    args.put("in", (Uint32)1);
    args.put("out", (Uint32)1);
    break;
  case NDB_MGM_SIGNAL_LOG_MODE_OFF:
    args.put("in", (Uint32)0);
    args.put("out", (Uint32)0);
    break;
  }

  const Properties *prop;
  prop = ndb_mgm_call(handle, stop_signallog_reply, "log signals", &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop != NULL) {
    BaseString result;
    prop->get("result", result);
    if(strcmp(result.c_str(), "Ok") == 0) {
      retval = 0;
    } else {
      SET_ERROR(handle, EINVAL, result.c_str());
      retval = -1;
    }
    delete prop;
  }

  return retval;
}

extern "C"
int 
ndb_mgm_set_trace(NdbMgmHandle handle, int nodeId, int traceNumber,
		  struct ndb_mgm_reply* reply) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_trace");
  const ParserRow<ParserDummy> set_trace_reply[] = {
    MGM_CMD("set trace reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("trace", traceNumber);

  const Properties *prop;
  prop = ndb_mgm_call(handle, set_trace_reply, "set trace", &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop != NULL) {
    BaseString result;
    prop->get("result", result);
    if(strcmp(result.c_str(), "Ok") == 0) {
      retval = 0;
    } else {
      SET_ERROR(handle, EINVAL, result.c_str());
      retval = -1;
    }
    delete prop;
  }

  return retval;
}

extern "C"
int 
ndb_mgm_insert_error(NdbMgmHandle handle, int nodeId, int errorCode,
		     struct ndb_mgm_reply* reply) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_insert_error");
  const ParserRow<ParserDummy> insert_error_reply[] = {
    MGM_CMD("insert error reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("error", errorCode);

  const Properties *prop;
  prop = ndb_mgm_call(handle, insert_error_reply, "insert error", &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop != NULL) {
    BaseString result;
    prop->get("result", result);
    if(strcmp(result.c_str(), "Ok") == 0) {
      retval = 0;
    } else {
      SET_ERROR(handle, EINVAL, result.c_str());
      retval = -1;
    }
    delete prop;
  }

  return retval;
}

extern "C"
int 
ndb_mgm_start(NdbMgmHandle handle, int no_of_nodes, const int * node_list)
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start");
  const ParserRow<ParserDummy> start_reply[] = {
    MGM_CMD("start reply", NULL, ""),
    MGM_ARG("started", Int, Optional, "No of started nodes"),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int started = 0;
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  if(no_of_nodes < 0){
    SET_ERROR(handle, EINVAL, "");
    return -1;
  }

  if(no_of_nodes == 0){
    Properties args;
    const Properties *reply;
    reply = ndb_mgm_call(handle, start_reply, "start all", &args);
    CHECK_REPLY(handle, reply, -1);

    Uint32 count = 0;
    if(!reply->get("started", &count)){
      delete reply;
      return -1;
    }
    delete reply;
    return count;
  }

  for(int node = 0; node < no_of_nodes; node++) {
    Properties args;
    args.put("node", node_list[node]);

    const Properties *reply;
    reply = ndb_mgm_call(handle, start_reply, "start", &args);

    if(reply != NULL) {
      BaseString result;
      reply->get("result", result);
      if(strcmp(result.c_str(), "Ok") == 0) {
	started++;
      } else {
	SET_ERROR(handle, EINVAL, result.c_str());
	delete reply;
	return -1;
      }
    }
    delete reply;
  }

  return started;
}

/*****************************************************************************
 * Backup
 *****************************************************************************/
extern "C"
int 
ndb_mgm_start_backup(NdbMgmHandle handle, int wait_completed,
		     unsigned int* _backup_id,
		     struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start_backup");
  const ParserRow<ParserDummy> start_backup_reply[] = {
    MGM_CMD("start backup reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_ARG("id", Int, Optional, "Id of the started backup"),
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("completed", wait_completed);
  const Properties *reply;
  { // start backup can take some time, set timeout high
    Uint64 old_timeout= handle->timeout;
    if (wait_completed == 2)
      handle->timeout= 48*60*60*1000; // 48 hours
    else if (wait_completed == 1)
      handle->timeout= 10*60*1000; // 10 minutes
    reply = ndb_mgm_call(handle, start_backup_reply, "start backup", &args);
    handle->timeout= old_timeout;
  }
  CHECK_REPLY(handle, reply, -1);

  BaseString result;
  reply->get("result", result);
  reply->get("id", _backup_id);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_COULD_NOT_START_BACKUP, result.c_str());
    delete reply;
    return -1;
  }

  delete reply;
  return 0;
}

extern "C"
int
ndb_mgm_abort_backup(NdbMgmHandle handle, unsigned int backupId,
		     struct ndb_mgm_reply* /*reply*/) 
{
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_abort_backup");
  const ParserRow<ParserDummy> stop_backup_reply[] = {
    MGM_CMD("abort backup reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),    
    MGM_END()
  };
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
  Properties args;
  args.put("id", backupId);

  const Properties *prop;
  prop = ndb_mgm_call(handle, stop_backup_reply, "abort backup", &args);
  CHECK_REPLY(handle, prop, -1);

  const char * buf;
  prop->get("result", &buf);
  if(strcmp(buf,"Ok")!=0) {
    SET_ERROR(handle, NDB_MGM_COULD_NOT_ABORT_BACKUP, buf);
    delete prop;    
    return -1;
  }

  delete prop;
  return 0;
}

extern "C"
struct ndb_mgm_configuration *
ndb_mgm_get_configuration(NdbMgmHandle handle, unsigned int version) {
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_configuration");
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);

  Properties args;
  args.put("version", version);

  const ParserRow<ParserDummy> reply[] = {
    MGM_CMD("get config reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),    
    MGM_ARG("Content-Length", Int, Optional, "Content length in bytes"),
    MGM_ARG("Content-Type", String, Optional, "Type (octet-stream)"),
    MGM_ARG("Content-Transfer-Encoding", String, Optional, "Encoding(base64)"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get config", &args);
  CHECK_REPLY(handle, prop, 0);
  
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n\n", buf);
      break;
    }

    buf = "<Unspecified>";
    if(!prop->get("Content-Type", &buf) || 
       strcmp(buf, "ndbconfig/octet-stream") != 0){
      fprintf(handle->errstream, "Unhandled response type: %s\n", buf);
      break;
    }

    buf = "<Unspecified>";
    if(!prop->get("Content-Transfer-Encoding", &buf) 
       || strcmp(buf, "base64") != 0){
      fprintf(handle->errstream, "Unhandled encoding: %s\n", buf);
      break;
    }

    buf = "<Content-Length Unspecified>";
    Uint32 len = 0;
    if(!prop->get("Content-Length", &len)){
      fprintf(handle->errstream, "Invalid response: %s\n\n", buf);
      break;
    }

    len += 1; // Trailing \n
        
    char* buf64 = new char[len];
    int read = 0;
    size_t start = 0;
    do {
      if((read = read_socket(handle->socket, handle->timeout,
			     &buf64[start], len-start)) < 1){
	delete[] buf64;
	buf64 = 0;
        if(read==0)
          SET_ERROR(handle, ETIMEDOUT, "Timeout reading packed config");
        else
          SET_ERROR(handle, errno, "Error reading packed config");
        ndb_mgm_disconnect_quiet(handle);
	break;
      }
      start += read;
    } while(start < len);
    if(buf64 == 0)
      break;

    void *tmp_data = malloc(base64_needed_decoded_length((size_t) (len - 1)));
    const int res = base64_decode(buf64, len-1, tmp_data, NULL);
    delete[] buf64;
    UtilBuffer tmp;
    tmp.append((void *) tmp_data, res);
    free(tmp_data);
    if (res < 0)
    {
      fprintf(handle->errstream, "Failed to decode buffer\n");
      break;
    }

    ConfigValuesFactory cvf;
    const int res2 = cvf.unpack(tmp);
    if(!res2){
      fprintf(handle->errstream, "Failed to unpack buffer\n");
      break;
    }

    delete prop;
    return (ndb_mgm_configuration*)cvf.getConfigValues();
  } while(0);

  delete prop;
  return 0;
}

extern "C"
void
ndb_mgm_destroy_configuration(struct ndb_mgm_configuration *cfg)
{
  if (cfg) {
    ((ConfigValues *)cfg)->~ConfigValues();
    free((void *)cfg);
  }
}

extern "C"
int
ndb_mgm_set_configuration_nodeid(NdbMgmHandle handle, int nodeid)
{
  CHECK_HANDLE(handle, -1);
  handle->cfg._ownNodeId= nodeid;
  return 0;
}

extern "C"
int
ndb_mgm_get_configuration_nodeid(NdbMgmHandle handle)
{
  CHECK_HANDLE(handle, 0);
  return handle->cfg._ownNodeId;
}

extern "C"
int ndb_mgm_get_connected_port(NdbMgmHandle handle)
{
  if (handle->cfg_i >= 0)
    return handle->cfg.ids[handle->cfg_i].port;
  else
    return 0;
}

extern "C"
const char *ndb_mgm_get_connected_host(NdbMgmHandle handle)
{
  if (handle->cfg_i >= 0)
    return handle->cfg.ids[handle->cfg_i].name.c_str();
  else
    return 0;
}

extern "C"
const char *ndb_mgm_get_connectstring(NdbMgmHandle handle, char *buf, int buf_sz)
{
  return handle->cfg.makeConnectString(buf,buf_sz);
}

extern "C"
int
ndb_mgm_alloc_nodeid(NdbMgmHandle handle, unsigned int version, int nodetype,
                     int log_event)
{
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  union { long l; char c[sizeof(long)]; } endian_check;

  endian_check.l = 1;

  int nodeid= handle->cfg._ownNodeId;

  Properties args;
  args.put("version", version);
  args.put("nodetype", nodetype);
  args.put("nodeid", nodeid);
  args.put("user", "mysqld");
  args.put("password", "mysqld");
  args.put("public key", "a public key");
  args.put("endian", (endian_check.c[sizeof(long)-1])?"big":"little");
  if (handle->m_name)
    args.put("name", handle->m_name);
  args.put("log_event", log_event);

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get nodeid reply", NULL, ""),
      MGM_ARG("error_code", Int, Optional, "Error code"),
      MGM_ARG("nodeid", Int, Optional, "Error message"),
      MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "get nodeid", &args);
  CHECK_REPLY(handle, prop, -1);

  nodeid= -1;
  do {
    const char * buf;
    if (!prop->get("result", &buf) || strcmp(buf, "Ok") != 0)
    {
      const char *hostname= ndb_mgm_get_connected_host(handle);
      unsigned port=  ndb_mgm_get_connected_port(handle);
      BaseString err;
      Uint32 error_code= NDB_MGM_ALLOCID_ERROR;
      err.assfmt("Could not alloc node id at %s port %d: %s",
		 hostname, port, buf);
      prop->get("error_code", &error_code);
      setError(handle, error_code, __LINE__, err.c_str());
      break;
    }
    Uint32 _nodeid;
    if(!prop->get("nodeid", &_nodeid) != 0){
      fprintf(handle->errstream, "ERROR Message: <nodeid Unspecified>\n");
      break;
    }
    nodeid= _nodeid;
  }while(0);

  delete prop;
  return nodeid;
}

extern "C"
int
ndb_mgm_set_int_parameter(NdbMgmHandle handle,
			  int node, 
			  int param,
			  unsigned value,
			  struct ndb_mgm_reply*){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  
  Properties args;
  args.put("node", node);
  args.put("param", param);
  args.put("value", value);
  
  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("set parameter reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "set parameter", &args);
  CHECK_REPLY(handle, prop, -1);

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    res= 0;
  } while(0);
  
  delete prop;
  return res;
}

extern "C"
int 
ndb_mgm_set_int64_parameter(NdbMgmHandle handle,
			    int node, 
			    int param,
			    unsigned long long value,
			    struct ndb_mgm_reply*){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  
  Properties args;
  args.put("node", node);
  args.put("param", param);
  args.put("value", value);
  
  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("set parameter reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "set parameter", &args);
  CHECK_REPLY(handle, prop, 0);

  if(prop == NULL) {
    SET_ERROR(handle, EIO, "Unable set parameter");
    return -1;
  }

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    res= 0;
  } while(0);
  
  delete prop;
  return res;
}

extern "C"
int
ndb_mgm_set_string_parameter(NdbMgmHandle handle,
			     int node, 
			     int param,
			     const char * value,
			     struct ndb_mgm_reply*){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  
  Properties args;
  args.put("node", node);
  args.put("parameter", param);
  args.put("value", value);
  
  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("set parameter reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "set parameter", &args);
  CHECK_REPLY(handle, prop, 0);
  
  if(prop == NULL) {
    SET_ERROR(handle, EIO, "Unable set parameter");
    return -1;
  }

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    res= 0;
  } while(0);
  
  delete prop;
  return res;
}

extern "C"
int
ndb_mgm_purge_stale_sessions(NdbMgmHandle handle, char **purged){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  
  Properties args;
  
  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("purge stale sessions reply", NULL, ""),
    MGM_ARG("purged", String, Optional, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "purge stale sessions", &args);
  CHECK_REPLY(handle, prop, -1);

  if(prop == NULL) {
    SET_ERROR(handle, EIO, "Unable to purge stale sessions");
    return -1;
  }

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    if (purged) {
      if (prop->get("purged", &buf))
	*purged= strdup(buf);
      else
	*purged= 0;
    }
    res= 0;
  } while(0);
  delete prop;
  return res;
}

extern "C"
int
ndb_mgm_check_connection(NdbMgmHandle handle){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);
  char buf[32];
  if (out.println("check connection"))
    goto ndb_mgm_check_connection_error;

  if (out.println(""))
    goto ndb_mgm_check_connection_error;

  in.gets(buf, sizeof(buf));
  if(strcmp("check connection reply\n", buf))
    goto ndb_mgm_check_connection_error;

  in.gets(buf, sizeof(buf));
  if(strcmp("result: Ok\n", buf))
    goto ndb_mgm_check_connection_error;

  in.gets(buf, sizeof(buf));
  if(strcmp("\n", buf))
    goto ndb_mgm_check_connection_error;

  return 0;

ndb_mgm_check_connection_error:
  ndb_mgm_disconnect(handle);
  return -1;
}

extern "C"
int
ndb_mgm_set_connection_int_parameter(NdbMgmHandle handle,
				     int node1,
				     int node2,
				     int param,
				     int value,
				     struct ndb_mgm_reply* mgmreply){
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  DBUG_ENTER("ndb_mgm_set_connection_int_parameter");
  
  Properties args;
  args.put("node1", node1);
  args.put("node2", node2);
  args.put("param", param);
  args.put("value", (Uint32)value);
  
  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("set connection parameter reply", NULL, ""),
    MGM_ARG("message", String, Mandatory, "Error Message"),
    MGM_ARG("result", String, Mandatory, "Status Result"),
    MGM_END()
  };
  
  const Properties *prop;
  prop= ndb_mgm_call(handle, reply, "set connection parameter", &args);
  DBUG_CHECK_REPLY(handle, prop, -1);

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    res= 0;
  } while(0);
  
  delete prop;
  DBUG_RETURN(res);
}

extern "C"
int
ndb_mgm_get_connection_int_parameter(NdbMgmHandle handle,
				     int node1,
				     int node2,
				     int param,
				     int *value,
				     struct ndb_mgm_reply* mgmreply){
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -2);
  DBUG_ENTER("ndb_mgm_get_connection_int_parameter");
  
  Properties args;
  args.put("node1", node1);
  args.put("node2", node2);
  args.put("param", param);

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get connection parameter reply", NULL, ""),
    MGM_ARG("value", Int, Mandatory, "Current Value"),
    MGM_ARG("result", String, Mandatory, "Result"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get connection parameter", &args);
  DBUG_CHECK_REPLY(handle, prop, -3);

  int res= -1;
  do {
    const char * buf;
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n", buf);
      break;
    }
    res= 0;
  } while(0);

  if(!prop->get("value",(Uint32*)value)){
    fprintf(handle->errstream, "Unable to get value\n");
    res = -4;
  }

  delete prop;
  DBUG_RETURN(res);
}

extern "C"
NDB_SOCKET_TYPE
ndb_mgm_convert_to_transporter(NdbMgmHandle *handle)
{
  NDB_SOCKET_TYPE s;

  CHECK_HANDLE((*handle), NDB_INVALID_SOCKET);
  CHECK_CONNECTED((*handle), NDB_INVALID_SOCKET);

  (*handle)->connected= 0;   // we pretend we're disconnected
  s= (*handle)->socket;

  SocketOutputStream s_output(s, (*handle)->timeout);
  s_output.println("transporter connect");
  s_output.println("");

  ndb_mgm_destroy_handle(handle); // set connected=0, so won't disconnect

  return s;
}

extern "C"
Uint32
ndb_mgm_get_mgmd_nodeid(NdbMgmHandle handle)
{
  Uint32 nodeid=0;

  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  DBUG_ENTER("ndb_mgm_get_mgmd_nodeid");
  
  Properties args;

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get mgmd nodeid reply", NULL, ""),
    MGM_ARG("nodeid", Int, Mandatory, "Node ID"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get mgmd nodeid", &args);
  DBUG_CHECK_REPLY(handle, prop, 0);

  if(!prop->get("nodeid",&nodeid)){
    fprintf(handle->errstream, "Unable to get value\n");
    return 0;
  }

  delete prop;
  DBUG_RETURN(nodeid);
}

extern "C"
int ndb_mgm_report_event(NdbMgmHandle handle, Uint32 *data, Uint32 length)
{
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  DBUG_ENTER("ndb_mgm_report_event");

  Properties args;
  args.put("length", length);
  BaseString data_string;

  for (int i = 0; i < (int) length; i++)
    data_string.appfmt(" %lu", (ulong) data[i]);

  args.put("data", data_string.c_str());

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("report event reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Result"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "report event", &args);
  DBUG_CHECK_REPLY(handle, prop, -1);

  DBUG_RETURN(0);
}

extern "C"
int ndb_mgm_end_session(NdbMgmHandle handle)
{
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  DBUG_ENTER("ndb_mgm_end_session");

  SocketOutputStream s_output(handle->socket, handle->timeout);
  s_output.println("end session");
  s_output.println("");

  SocketInputStream in(handle->socket, handle->timeout);
  char buf[32];
  in.gets(buf, sizeof(buf));
  CHECK_TIMEDOUT_RET(handle, in, s_output, -1);

  DBUG_RETURN(0);
}

extern "C"
int ndb_mgm_get_version(NdbMgmHandle handle,
                        int *major, int *minor, int *build, int len, char* str)
{
  DBUG_ENTER("ndb_mgm_get_version");
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);

  Properties args;

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("version", NULL, ""),
    MGM_ARG("id", Int, Mandatory, "ID"),
    MGM_ARG("major", Int, Mandatory, "Major"),
    MGM_ARG("minor", Int, Mandatory, "Minor"),
    MGM_ARG("string", String, Mandatory, "String"),
    MGM_END()
  };

  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get version", &args);
  CHECK_REPLY(handle, prop, 0);

  Uint32 id;
  if(!prop->get("id",&id)){
    fprintf(handle->errstream, "Unable to get value\n");
    return 0;
  }
  *build= getBuild(id);

  if(!prop->get("major",(Uint32*)major)){
    fprintf(handle->errstream, "Unable to get value\n");
    return 0;
  }

  if(!prop->get("minor",(Uint32*)minor)){
    fprintf(handle->errstream, "Unable to get value\n");
    return 0;
  }

  BaseString result;
  if(!prop->get("string", result)){
    fprintf(handle->errstream, "Unable to get value\n");
    return 0;
  }

  strncpy(str, result.c_str(), len);

  delete prop;
  DBUG_RETURN(1);
}

extern "C"
Uint64
ndb_mgm_get_session_id(NdbMgmHandle handle)
{
  Uint64 session_id=0;

  DBUG_ENTER("ndb_mgm_get_session_id");
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);
  
  Properties args;

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get session id reply", NULL, ""),
    MGM_ARG("id", Int, Mandatory, "Node ID"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get session id", &args);
  CHECK_REPLY(handle, prop, 0);

  if(!prop->get("id",&session_id)){
    fprintf(handle->errstream, "Unable to get session id\n");
    return 0;
  }

  delete prop;
  DBUG_RETURN(session_id);
}

extern "C"
int
ndb_mgm_get_session(NdbMgmHandle handle, Uint64 id,
                    struct NdbMgmSession *s, int *len)
{
  int retval= 0;
  DBUG_ENTER("ndb_mgm_get_session");
  CHECK_HANDLE(handle, 0);
  CHECK_CONNECTED(handle, 0);

  Properties args;
  args.put("id", id);

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get session reply", NULL, ""),
    MGM_ARG("id", Int, Mandatory, "Node ID"),
    MGM_ARG("m_stopSelf", Int, Optional, "m_stopSelf"),
    MGM_ARG("m_stop", Int, Optional, "stop session"),
    MGM_ARG("nodeid", Int, Optional, "allocated node id"),
    MGM_ARG("parser_buffer_len", Int, Optional, "waiting in buffer"),
    MGM_ARG("parser_status", Int, Optional, "parser status"),
    MGM_END()
  };

  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get session", &args);
  CHECK_REPLY(handle, prop, 0);

  Uint64 r_id;
  int rlen= 0;

  if(!prop->get("id",&r_id)){
    fprintf(handle->errstream, "Unable to get session id\n");
    goto err;
  }

  s->id= r_id;
  rlen+=sizeof(s->id);

  if(prop->get("m_stopSelf",&(s->m_stopSelf)))
    rlen+=sizeof(s->m_stopSelf);
  else
    goto err;

  if(prop->get("m_stop",&(s->m_stop)))
    rlen+=sizeof(s->m_stop);
  else
    goto err;

  if(prop->get("nodeid",&(s->nodeid)))
    rlen+=sizeof(s->nodeid);
  else
    goto err;

  if(prop->get("parser_buffer_len",&(s->parser_buffer_len)))
  {
    rlen+=sizeof(s->parser_buffer_len);
    if(prop->get("parser_status",&(s->parser_status)))
      rlen+=sizeof(s->parser_status);
  }

  *len= rlen;
  retval= 1;

err:
  delete prop;
  DBUG_RETURN(retval);
}

template class Vector<const ParserRow<ParserDummy>*>;
