/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include <LocalConfig.hpp>

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
#include <ConfigObject.hpp>
#include <ndb_base64.h>

//#define MGMAPI_LOG
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
   ParserRow<ParserDummy>::End, \
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

  int mgmd_version(void) const {
    // Must be connected
    assert(connected);
    // Check that version has been read
    assert(mgmd_version_major >= 0 &&
           mgmd_version_minor >= 0 &&
           mgmd_version_build >= 0);
    return NDB_MAKE_VERSION(mgmd_version_major,
                            mgmd_version_minor,
                            mgmd_version_build);
  }

  char * m_bindaddress;
  int m_bindaddress_port;
  bool ignore_sigpipe;
};


/*
  Check if version "curr" is new relative a list of given versions.

  curr is regarded new relative a list of versions if
  either, curr is greater than or equal a version in list
  with same major and minor version, or, curr is greater
  than all versions in list.

  NOTE! The list of versions to check against must be listed
  with the highest version first, and at most one entry per
  major and minor version, and terminated with version 0
*/
static inline
bool check_version_new(Uint32 curr, ...)
{
  Uint32 version, last = ~0U;

  va_list versions;
  va_start(versions, curr);
  while ((version= va_arg(versions, Uint32)) != 0U)
  {
    // check that version list is descending
    assert(version < last);
    // check at most one entry per major.minor
    assert(!(ndbGetMajor(version) == ndbGetMajor(last) && ndbGetMinor(version) == ndbGetMinor(last)));

    if (curr >= version)
    {
      va_end(versions);
      if (last == ~0U)
      {
        return true; // curr is greater than all versions in list (or equal the first and greatest)
      }
      return ndbGetMajor(curr) == ndbGetMajor(version) && ndbGetMinor(curr) == ndbGetMinor(version); 
    }

    last = version;
  }
  va_end(versions);

  return false;
}


static
void
setError(NdbMgmHandle h, int error, int error_line, const char * msg, ...)
 ATTRIBUTE_FORMAT(printf, 4, 5);

static
void
setError(NdbMgmHandle h, int error, int error_line, const char * msg, ...)
{

  h->last_error = error;
  h->last_error_line = error_line;

  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(h->last_error_desc, sizeof(h->last_error_desc), msg, ap);
  va_end(ap);
}

#define SET_ERROR(h, e, s) setError((h), (e), __LINE__, "%s", (s))
#define SET_ERROR_CMD(h, e, s, cmd, t) setError((h), (e), __LINE__, "cmd: %s, error: %s, timeout: %d", (cmd), (s), (t))

#define CHECK_HANDLE(handle, ret) \
  if(handle == 0) {   \
    DBUG_RETURN(ret); \
  } 

#define CHECK_CONNECTED(handle, ret) \
  if (handle->connected != 1) { \
    SET_ERROR(handle, NDB_MGM_SERVER_NOT_CONNECTED , ""); \
    DBUG_RETURN(ret);                                     \
  }

#define CHECK_REPLY(handle, reply, ret) \
  if (reply == NULL) { \
    if(!handle->last_error) \
      SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, ""); \
    DBUG_RETURN(ret);                                    \
  }

#define CHECK_TIMEDOUT(in, out) \
  if(in.timedout() || out.timedout()) \
    SET_ERROR(handle, ETIMEDOUT, \
              "Time out talking to management server");

#define CHECK_TIMEDOUT_RET(h, in, out, ret, cmd) \
  if(in.timedout() || out.timedout()) { \
    SET_ERROR_CMD(h, ETIMEDOUT, \
              "Time out talking to management server", cmd, h->timeout); \
    ndb_mgm_disconnect_quiet(h); \
    DBUG_RETURN(ret);            \
  }

/*****************************************************************************
 * Handles
 *****************************************************************************/

extern "C"
NdbMgmHandle
ndb_mgm_create_handle()
{
  DBUG_ENTER("ndb_mgm_create_handle");
  NdbMgmHandle h = (NdbMgmHandle)malloc(sizeof(ndb_mgm_handle));
  if (!h)
    DBUG_RETURN(NULL);

  h->connected       = 0;
  h->last_error      = 0;
  h->last_error_line = 0;
  ndb_socket_invalidate(&(h->socket));
  h->timeout         = 60000;
  h->cfg_i           = -1;
  h->errstream       = stdout;
  h->m_name          = NULL;
  h->m_bindaddress   = 0;
  h->m_bindaddress_port = 0;
  h->ignore_sigpipe  = true;

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
  free(handle->m_name);
  handle->m_name= strdup(name);
}

extern "C"
const char *
ndb_mgm_get_name(const NdbMgmHandle handle)
{
  return handle->m_name;
}

extern "C"
int
ndb_mgm_set_connectstring(NdbMgmHandle handle, const char* connect_string)
{
  DBUG_ENTER("ndb_mgm_set_connectstring");
  DBUG_PRINT("info", ("handle: 0x%lx", (long) handle));
  handle->cfg.~LocalConfig();
  new (&(handle->cfg)) LocalConfig;
  if (!handle->cfg.init(connect_string, 0) ||
      handle->cfg.ids.size() == 0)
  {
    handle->cfg.~LocalConfig();
    new (&(handle->cfg)) LocalConfig;
    handle->cfg.init(0, 0); /* reset the LocalConfig */
    SET_ERROR(handle, NDB_MGM_ILLEGAL_CONNECT_STRING,
              connect_string ? connect_string : "");
    DBUG_RETURN(-1);
  }
  handle->cfg_i= -1;
  handle->cfg.bind_address_port= handle->m_bindaddress_port;
  handle->cfg.bind_address.assign(handle->m_bindaddress ? handle->m_bindaddress : "");
  DBUG_RETURN(0);
}

extern "C"
int
ndb_mgm_set_bindaddress(NdbMgmHandle handle, const char * arg)
{
  DBUG_ENTER("ndb_mgm_set_bindaddress");
  free(handle->m_bindaddress);

  if (arg)
  {
    handle->m_bindaddress = strdup(arg);
    char *port = strchr(handle->m_bindaddress, ':');
    if (port != 0)
    {
      handle->m_bindaddress_port = atoi(port+1);
      *port = 0;
    }
    else
      handle->m_bindaddress_port = 0;
  }
  else
  {
    handle->m_bindaddress = 0;
    handle->m_bindaddress_port = 0;
  }
  if (handle->cfg.ids.size() != 0)
  {
    handle->cfg.bind_address_port= handle->m_bindaddress_port;
    handle->cfg.bind_address.assign(handle->m_bindaddress ? handle->m_bindaddress : "");
  }
  DBUG_RETURN(0);
}

extern "C"
int
ndb_mgm_set_ignore_sigpipe(NdbMgmHandle handle, int val)
{
  DBUG_ENTER("ndb_mgm_set_ignore_sigpipe");
  CHECK_HANDLE(handle, -1);
  if (handle->connected){
    SET_ERROR(handle, EINVAL, "Can't change 'ignore_sigpipe' while connected");
    DBUG_RETURN(-1);
  }
  handle->ignore_sigpipe = (val != 0);
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
  free((*handle)->m_name);
  free((*handle)->m_bindaddress);
  free(*handle);
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
  if (!h)
    return NDB_MGM_ILLEGAL_SERVER_HANDLE;

  return h->last_error;
}

extern "C"
const char *
ndb_mgm_get_latest_error_desc(const NdbMgmHandle h)
{
  if (!h)
    return "";
  return h->last_error_desc;
}

extern "C"
int
ndb_mgm_get_latest_error_line(const NdbMgmHandle h)
{
  if (!h)
    return 0;
  return h->last_error_line;
}

extern "C"
const char *
ndb_mgm_get_latest_error_msg(const NdbMgmHandle h)
{
  const int last_err = ndb_mgm_get_latest_error(h);

  for (int i=0; i<ndb_mgm_noOfErrorMsgs; i++) {
    if (ndb_mgm_error_msgs[i].code == last_err)
      return ndb_mgm_error_msgs[i].msg;
  }

  return "Error"; // Unknown Error message
}


/*
  ndb_mgm_call

  Send command, command arguments and any command bulk data to
  ndb_mgmd.
  Read and return result

  @param The mgmapi handle
  @param List describing the expected reply
  @param Name of the command to call
  @param Arguments for the command
  @param Any bulk data to send after the command

 */
static const Properties *
ndb_mgm_call(NdbMgmHandle handle,
             const ParserRow<ParserDummy> *command_reply,
             const char *cmd, const Properties *cmd_args,
             const char* cmd_bulk= NULL)
{
  DBUG_ENTER("ndb_mgm_call");
  DBUG_PRINT("enter",("handle->socket: " MY_SOCKET_FORMAT ", cmd: %s",
		      MY_SOCKET_FORMAT_VALUE(handle->socket), cmd));
  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);

  out.println("%s", cmd);
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
	out.println("%s: %u", name, val_i);
	break;
      case PropertiesType_Uint64:
	cmd_args->get(name, &val_64);
	out.println("%s: %llu", name, val_64);
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
  out.println("%s", "");

  if (cmd_bulk)
  {
    out.write(cmd_bulk, strlen(cmd_bulk));
    out.write("\n", 1);
  }

  CHECK_TIMEDOUT_RET(handle, in, out, NULL, cmd);

  Parser_t::Context ctx;
  ParserDummy session(handle->socket);
  Parser_t parser(command_reply, in);

  const Properties* p = parser.parse(ctx, session);
  if (p == NULL){
    if(!ndb_mgm_is_connected(handle)) {
      CHECK_TIMEDOUT_RET(handle, in, out, NULL, cmd);
      DBUG_RETURN(NULL);
    }
    else
    {
      CHECK_TIMEDOUT_RET(handle, in, out, NULL, cmd);
      if(ctx.m_status==Parser_t::Eof
	 || ctx.m_status==Parser_t::NoLine)
      {
	ndb_mgm_disconnect(handle);
        CHECK_TIMEDOUT_RET(handle, in, out, NULL, cmd);
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
  CHECK_TIMEDOUT_RET(handle, in, out, NULL, cmd);
  DBUG_RETURN(p);
}

/*
  ndb_mgm_call_slow

  Some commands are synchronous and known to take longer time
  to complete(for example restart and stop). Increase the timeout
  value before sending command if the timeout value is set lower
  than what is normal.

  Unfortunately the restart or stop may take longer than the
  defalt min timeout value selected, mgmapi users can workaround
  this problem by setting an even larger timeout for all commands
  or only around restart and stop.

*/

static inline
const Properties *
ndb_mgm_call_slow(NdbMgmHandle handle,
                  const ParserRow<ParserDummy> *command_reply,
                  const char *cmd, const Properties *cmd_args,
                  unsigned int min_timeout = 5*60*1000, // ms
                  const char* cmd_bulk= NULL)
{
  const unsigned int save_timeout = handle->timeout;
  if (min_timeout > save_timeout)
    handle->timeout = min_timeout;
  const Properties* reply = ndb_mgm_call(handle, command_reply,
                                         cmd, cmd_args, cmd_bulk);

  // Restore saved timeout value
  handle->timeout = save_timeout;

  return reply;
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
      ndb_socket_close(handle->socket);
    }
  }
  return handle->connected;
}

extern "C"
int ndb_mgm_set_connect_timeout(NdbMgmHandle handle, unsigned int seconds)
{
  return ndb_mgm_set_timeout(handle, seconds*1000);
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


static inline
bool get_mgmd_version(NdbMgmHandle handle)
{
  assert(handle->connected);

  if (handle->mgmd_version_major >= 0)
    return true; // Already fetched version of mgmd

  char buf[2]; // Not used -> keep short
  if (!ndb_mgm_get_version(handle,
                           &(handle->mgmd_version_major),
                           &(handle->mgmd_version_minor),
                           &(handle->mgmd_version_build),
                           sizeof(buf), buf))
    return false;
  return true;
}


/**
 * Connect to a management server
 * no_retries = 0, return immediately,
 * no_retries < 0, retry infinitely,
 * else retry no_retries times.
 */
extern "C"
int
ndb_mgm_connect(NdbMgmHandle handle, int no_retries,
		int retry_delay_in_seconds, int verbose)
{
  DBUG_ENTER("ndb_mgm_connect");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_connect");

#ifdef MGMAPI_LOG
  /**
  * Open the log file
  */
  char logname[64];
  BaseString::snprintf(logname, 64, "mgmapi.log");
  handle->logfile = fopen(logname, "w");
#endif
  char buf[1024];

#if defined SIGPIPE && !defined _WIN32
  if (handle->ignore_sigpipe)
    (void)signal(SIGPIPE, SIG_IGN);
#endif

  /**
   * Do connect
   */
  LocalConfig &cfg= handle->cfg;
  NDB_SOCKET_TYPE sockfd;
  ndb_socket_invalidate(&sockfd);
  Uint32 i = Uint32(~0);
  while (!ndb_socket_valid(sockfd))
  {
    // do all the mgmt servers
    for (i = 0; i < cfg.ids.size(); i++)
    {
      if (cfg.ids[i].type != MgmId_TCP)
	continue;

      SocketClient s;
      const char *bind_address= NULL;
      unsigned short bind_address_port= 0;
      s.set_connect_timeout(handle->timeout);
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
        bind_address= handle->m_bindaddress;
        bind_address_port= handle->m_bindaddress_port;
      }
      else if (cfg.ids[i].bind_address.length())
      {
        bind_address= cfg.ids[i].bind_address.c_str();
        bind_address_port= cfg.ids[i].bind_address_port;
      }
      if (bind_address)
      {
        int err;
        if ((err = s.bind(bind_address, bind_address_port)) != 0)
        {
          if (!handle->m_bindaddress)
          {
            // retry with next mgmt server
            continue;
          }
          fprintf(handle->errstream, 
                  "Unable to bind local address '%s:%d' errno: %d, "
                  "while trying to connect with connect string: '%s'\n",
                  bind_address, (int)bind_address_port, err,
                  cfg.makeConnectString(buf,sizeof(buf)));
      
          setError(handle, NDB_MGM_BIND_ADDRESS, __LINE__,
                   "Unable to bind local address '%s:%d' errno: %d, "
                   "while trying to connect with connect string: '%s'\n",
                   bind_address, (int)bind_address_port, err,
                   cfg.makeConnectString(buf,sizeof(buf)));
          DBUG_RETURN(-1);
        }
      }
      sockfd = s.connect(cfg.ids[i].name.c_str(), cfg.ids[i].port);
      if (ndb_socket_valid(sockfd))
	break;
    }
    if (ndb_socket_valid(sockfd))
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
    else
    {
      // no_retries < 0, retrying infinitely
      if (verbose == -2)
      {
        fprintf(handle->errstream, ".");
        fflush(handle->errstream);
      }
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

  // Version of the connected ndb_mgmd is not yet known
  handle->mgmd_version_major= -1;
  handle->mgmd_version_minor= -1;
  handle->mgmd_version_build= -1;

  DBUG_RETURN(0);
}

/**
 * Only used for low level testing
 * Never to be used by end user.
 * Or anybody who doesn't know exactly what they're doing.
 */
extern "C"
ndb_native_socket_t
ndb_mgm_get_fd(NdbMgmHandle handle)
{
  return ndb_socket_get_native(handle->socket);
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
  ndb_socket_close(handle->socket);
  ndb_socket_invalidate(&(handle->socket));
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
  DBUG_ENTER("ndb_mgm_disconnect");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_disconnect");
  CHECK_CONNECTED(handle, -1);

  DBUG_RETURN(ndb_mgm_disconnect_quiet(handle));
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
  { "SINGLE USER MODE", NDB_MGM_NODE_STATUS_SINGLEUSER },
  { "SINGLE USER MODE", NDB_MGM_NODE_STATUS_SINGLEUSER },
  { "RESUME", NDB_MGM_NODE_STATUS_RESUME },
  { "CONNECTED", NDB_MGM_NODE_STATUS_CONNECTED }
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
  } else if(strcmp("mysql_version", field) == 0){
    state->mysql_version = atoi(value);
  } else if(strcmp("connect_count", field) == 0){
    state->connect_count = atoi(value);    
  } else if(strcmp("address", field) == 0){
    strncpy(state->connect_address, value, sizeof(state->connect_address));
    state->connect_address[sizeof(state->connect_address)-1]= 0;
  } else if(strcmp("is_single_user", field) == 0){
    state->is_single_user = atoi(value);
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
  return ndb_mgm_get_status2(handle, 0);
}

extern "C"
struct ndb_mgm_cluster_state * 
ndb_mgm_get_status2(NdbMgmHandle handle, const enum ndb_mgm_node_type types[])
{
  DBUG_ENTER("ndb_mgm_get_status2");
  CHECK_HANDLE(handle, NULL);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_status");
  CHECK_CONNECTED(handle, NULL);

  char typestring[1024];
  typestring[0] = 0;
  if (types != 0)
  {
    int pos = 0;
    for (Uint32 i = 0; types[i] != NDB_MGM_NODE_TYPE_UNKNOWN; i++)
    {
      if (int(types[i]) < NDB_MGM_NODE_TYPE_MIN ||
          int(types[i]) > NDB_MGM_NODE_TYPE_MAX)
      {
        SET_ERROR(handle, EINVAL, 
                  "Incorrect node type for ndb_mgm_get_status2");
        DBUG_RETURN(0);
      }
      /**
       * Check for duplicates
       */
      for (Int32 j = i - 1; j >= 0; j--)
      {
        if (types[i] == types[j])
        {
          SET_ERROR(handle, EINVAL, 
                    "Duplicate types for ndb_mgm_get_status2");
          DBUG_RETURN(0);
        }
      }
      
      int left = sizeof(typestring) - pos;
      int len = BaseString::snprintf(typestring+pos, left, "%s ", 
                                     ndb_mgm_get_node_type_string(types[i]));
      
      if (len >= left)
      {
        SET_ERROR(handle, EINVAL, 
                  "Out of memory for type-string for ndb_mgm_get_status2");
        DBUG_RETURN(0);
      }
      pos += len;
    }
  }
  
  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);

  const char *get_status_str = "get status";
  out.println("%s", get_status_str);
  if (types)
  {
    out.println("types: %s", typestring);
  }
  out.println("%s", "");

  CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);

  char buf[1024];
  if(!in.gets(buf, sizeof(buf)))
  {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, "Probably disconnected");
    DBUG_RETURN(NULL);
  }
  if(strcmp("node status\n", buf) != 0) {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);
    ndbout << in.timedout() << " " << out.timedout() << buf << endl;
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    DBUG_RETURN(NULL);
  }
  if(!in.gets(buf, sizeof(buf)))
  {
    CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, "Probably disconnected");
    DBUG_RETURN(NULL);
  }

  BaseString tmp(buf);
  Vector<BaseString> split;
  tmp.split(split, ":");
  if(split.size() != 2){
    CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    DBUG_RETURN(NULL);
  }

  if(!(split[0].trim() == "nodes")){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, buf);
    DBUG_RETURN(NULL);
  }

  const int noOfNodes = atoi(split[1].c_str());

  ndb_mgm_cluster_state *state = (ndb_mgm_cluster_state*)
    malloc(sizeof(ndb_mgm_cluster_state)+
	   noOfNodes*(sizeof(ndb_mgm_node_state)+sizeof("000.000.000.000#")));

  if(!state)
  {
    SET_ERROR(handle, NDB_MGM_OUT_OF_MEMORY,
              "Allocating ndb_mgm_cluster_state");
    DBUG_RETURN(NULL);
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
      DBUG_RETURN(NULL);
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
    CHECK_TIMEDOUT_RET(handle, in, out, NULL, get_status_str);
    SET_ERROR(handle, NDB_MGM_ILLEGAL_NODE_STATUS, "Node count mismatch");
    DBUG_RETURN(NULL);
  }

  qsort(state->node_states, state->no_of_nodes, sizeof(state->node_states[0]),
	cmp_state);
  DBUG_RETURN(state);
}

extern "C"
int 
ndb_mgm_enter_single_user(NdbMgmHandle handle,
			  unsigned int nodeId,
			  struct ndb_mgm_reply* /*reply*/) 
{
  DBUG_ENTER("ndb_mgm_enter_single_user");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_enter_single_user");
  const ParserRow<ParserDummy> enter_single_reply[] = {
    MGM_CMD("enter single user reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
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
    DBUG_RETURN(-1);
  }

  delete reply;
  DBUG_RETURN(0);
}


extern "C"
int 
ndb_mgm_exit_single_user(NdbMgmHandle handle, struct ndb_mgm_reply* /*reply*/) 
{
  DBUG_ENTER("ndb_mgm_exit_single_user");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_exit_single_user");
  const ParserRow<ParserDummy> exit_single_reply[] = {
    MGM_CMD("exit single user reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  const Properties *reply;
  reply = ndb_mgm_call(handle, exit_single_reply, "exit single user", 0);
  CHECK_REPLY(handle, reply, -1);

  const char * buf;
  reply->get("result", &buf);
  if(strcmp(buf,"Ok")!=0) {
    SET_ERROR(handle, NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE, buf);
    delete reply;    
    DBUG_RETURN(-1);
  }

  delete reply;
  DBUG_RETURN(0);
}

extern "C"
int 
ndb_mgm_stop(NdbMgmHandle handle, int no_of_nodes, const int * node_list)
{
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
  return ndb_mgm_stop4(handle, no_of_nodes, node_list, abort,
                       false, disconnect);
}

extern "C"
int
ndb_mgm_stop4(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
	      int abort, int force, int *disconnect)
{
  DBUG_ENTER("ndb_mgm_stop4");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_stop4");
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

  CHECK_CONNECTED(handle, -1);

  if (!get_mgmd_version(handle))
    DBUG_RETURN(-1);

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
    DBUG_RETURN(-1);
  }

  if(no_of_nodes <= 0){
    /**
     * All nodes should be stopped (all or just db)
     */
    Properties args;
    args.put("abort", abort);
    if(use_v2)
      args.put("stop", (no_of_nodes==-1)?"mgm,db":"db");
    // force has no effect, continue anyway for consistency
    const Properties *reply;
    if(use_v2)
      reply = ndb_mgm_call_slow(handle, stop_reply_v2, "stop all", &args);
    else
      reply = ndb_mgm_call_slow(handle, stop_reply_v1, "stop all", &args);
    CHECK_REPLY(handle, reply, -1);

    Uint32 stopped = 0;
    if(!reply->get("stopped", &stopped))
    {
      SET_ERROR(handle, NDB_MGM_STOP_FAILED, 
		"Could not get number of stopped nodes from mgm server");
      delete reply;
      DBUG_RETURN(-1);
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
      DBUG_RETURN(-1);
    }
    delete reply;
    DBUG_RETURN(stopped);
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
  if (check_version_new(handle->mgmd_version(),
                        NDB_MAKE_VERSION(7,1,8),
                        NDB_MAKE_VERSION(7,0,19),
                        0))
    args.put("force", force);
  else
    SET_ERROR(handle, NDB_MGM_STOP_FAILED,
	      "The connected mgm server does not support 'stop --force'");

  const Properties *reply;
  if(use_v2)
    reply = ndb_mgm_call_slow(handle, stop_reply_v2, "stop v2", &args);
  else
    reply = ndb_mgm_call_slow(handle, stop_reply_v1, "stop", &args);
  CHECK_REPLY(handle, reply, -1);

  Uint32 stopped;
  if(!reply->get("stopped", &stopped))
  {
    SET_ERROR(handle, NDB_MGM_STOP_FAILED, 
	      "Could not get number of stopped nodes from mgm server");
    delete reply;
    DBUG_RETURN(-1);
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
    DBUG_RETURN(-1);
  }
  delete reply;
  DBUG_RETURN(stopped);
}

extern "C"
int
ndb_mgm_restart(NdbMgmHandle handle, int no_of_nodes, const int *node_list) 
{
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
  return ndb_mgm_restart4(handle, no_of_nodes, node_list, initial,
                          nostart, abort, false, disconnect);
}

extern "C"
int
ndb_mgm_restart4(NdbMgmHandle handle, int no_of_nodes, const int * node_list,
                 int initial, int nostart, int abort, int force,
                 int *disconnect)
{
  DBUG_ENTER("ndb_mgm_restart");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_restart4");

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

  CHECK_CONNECTED(handle, -1);

  if (!get_mgmd_version(handle))
    DBUG_RETURN(-1);

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
    DBUG_RETURN(-1);
  }
  
  if(no_of_nodes == 0) {
    Properties args;    
    args.put("abort", abort);
    args.put("initialstart", initial);
    args.put("nostart", nostart);
    // force has no effect, continue anyway for consistency
    const Properties *reply =
      ndb_mgm_call_slow(handle, restart_reply_v1, "restart all", &args);
    CHECK_REPLY(handle, reply, -1);

    BaseString result;
    reply->get("result", result);
    if(strcmp(result.c_str(), "Ok") != 0) {
      SET_ERROR(handle, NDB_MGM_RESTART_FAILED, result.c_str());
      delete reply;
      DBUG_RETURN(-1);
    }

    Uint32 restarted;
    if(!reply->get("restarted", &restarted)){
      SET_ERROR(handle, NDB_MGM_RESTART_FAILED, 
		"Could not get restarted number of nodes from mgm server");
      delete reply;
      DBUG_RETURN(-1);
    }
    delete reply;
    DBUG_RETURN(restarted);
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

  if (check_version_new(handle->mgmd_version(),
                        NDB_MAKE_VERSION(7,1,8),
                        NDB_MAKE_VERSION(7,0,19),
                        0))
    args.put("force", force);
  else
    SET_ERROR(handle, NDB_MGM_RESTART_FAILED,
	      "The connected mgm server does not support 'restart --force'");

  const Properties *reply;
  if(use_v2)
    reply = ndb_mgm_call_slow(handle, restart_reply_v2,
                              "restart node v2", &args);
  else
    reply = ndb_mgm_call_slow(handle, restart_reply_v1,
                              "restart node", &args);
  CHECK_REPLY(handle, reply, -1);

  BaseString result;
  reply->get("result", result);
  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_RESTART_FAILED, result.c_str());
    delete reply;
    DBUG_RETURN(-1);
  }
  Uint32 restarted;
  reply->get("restarted", &restarted);
  if(use_v2)
    reply->get("disconnect", (Uint32*)disconnect);
  else
    *disconnect= 0;
  delete reply;
  DBUG_RETURN(restarted);
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
    if(native_strcasecmp(name, clusterlog_severities[i].name) == 0)
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
  DBUG_ENTER("ndb_mgm_get_clusterlog_severity_filter");
  CHECK_HANDLE(handle, -1);
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
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getinfo_reply, "get info clusterlog", &args);
  CHECK_REPLY(handle, reply, -1);
  
  for(unsigned int i=0; i < severity_size; i++) {
    reply->get(clusterlog_severity_names[severity[i].category], &severity[i].value);
  }
  delete reply;
  DBUG_RETURN(severity_size);
}

extern "C"
const unsigned int *
ndb_mgm_get_clusterlog_severity_filter_old(NdbMgmHandle handle) 
{
  DBUG_ENTER("ndb_mgm_get_clusterlog_severity_filter_old");
  CHECK_HANDLE(handle, NULL);
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
    MGM_END()
  };
  CHECK_CONNECTED(handle, NULL);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getinfo_reply, "get info clusterlog", &args);
  CHECK_REPLY(handle, reply, NULL);
  
  for(int i=0; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++) {
    reply->get(clusterlog_severity_names[i], &enabled[i]);
  }
  delete reply;
  DBUG_RETURN(enabled);
}

extern "C"
int 
ndb_mgm_set_clusterlog_severity_filter(NdbMgmHandle handle, 
				       enum ndb_mgm_event_severity severity,
				       int enable,
				       struct ndb_mgm_reply* /*reply*/) 
{
  DBUG_ENTER("ndb_mgm_set_clusterlog_severity_filter");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR,
	    "Executing: ndb_mgm_set_clusterlog_severity_filter");
  const ParserRow<ParserDummy> filter_reply[] = {
    MGM_CMD("set logfilter reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
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
  DBUG_RETURN(retval);
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
  { "SCHEMA", NDB_MGM_EVENT_CATEGORY_SCHEMA },
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
  DBUG_ENTER("ndb_mgm_get_clusterlog_loglevel");
  CHECK_HANDLE(handle, -1);
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
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getloglevel_reply, "get cluster loglevel", &args);
  CHECK_REPLY(handle, reply, -1);

  for(int i=0; i < loglevel_count; i++) {
    reply->get(clusterlog_names[loglevel[i].category], &loglevel[i].value);
  }
  delete reply;
  DBUG_RETURN(loglevel_count);
}

extern "C"
const unsigned int *
ndb_mgm_get_clusterlog_loglevel_old(NdbMgmHandle handle)
{
  DBUG_ENTER("ndb_mgm_get_clusterlog_loglevel_old");
  CHECK_HANDLE(handle, NULL);
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
    MGM_END()
  };
  CHECK_CONNECTED(handle, NULL);

  Properties args;
  const Properties *reply;
  reply = ndb_mgm_call(handle, getloglevel_reply, "get cluster loglevel", &args);
  CHECK_REPLY(handle, reply, NULL);

  for(int i=0; i < loglevel_count; i++) {
    reply->get(clusterlog_names[i], &loglevel[i]);
  }
  delete reply;
  DBUG_RETURN(loglevel);
}

extern "C"
int 
ndb_mgm_set_clusterlog_loglevel(NdbMgmHandle handle, int nodeId,
				enum ndb_mgm_event_category cat,
				int level,
				struct ndb_mgm_reply* /*reply*/) 
{
  DBUG_ENTER("ndb_mgm_set_clusterlog_loglevel");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, 
	    "Executing: ndb_mgm_set_clusterlog_loglevel");
  const ParserRow<ParserDummy> clusterlog_reply[] = {
    MGM_CMD("set cluster loglevel reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("category", cat);
  args.put("level", level);
  
  const Properties *reply;
  reply = ndb_mgm_call(handle, clusterlog_reply, 
		       "set cluster loglevel", &args);
  CHECK_REPLY(handle, reply, -1);
  
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
  DBUG_ENTER("ndb_mgm_set_loglevel_node");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_loglevel_node");
  const ParserRow<ParserDummy> loglevel_reply[] = {
    MGM_CMD("set loglevel reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
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
    DBUG_RETURN(-1);
  }

  delete reply;
  DBUG_RETURN(0);
}

int
ndb_mgm_listen_event_internal(NdbMgmHandle handle, const int filter[],
			      int parsable, NDB_SOCKET_TYPE* sock)
{
  DBUG_ENTER("ndb_mgm_listen_event_internal");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_listen_event");
  const ParserRow<ParserDummy> stat_reply[] = {
    MGM_CMD("listen event", NULL, ""),
    MGM_ARG("result", Int, Mandatory, "Error message"),
    MGM_ARG("msg", String, Optional, "Error message"),
    MGM_END()
  };

  const char *hostname= ndb_mgm_get_connected_host(handle);
  int port= ndb_mgm_get_connected_port(handle);
  const char *bind_address= ndb_mgm_get_connected_bind_address(handle);
  SocketClient s;
  s.set_connect_timeout(handle->timeout);
  if (!s.init())
  {
    fprintf(handle->errstream, "Unable to create socket");
    setError(handle, NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, __LINE__,
             "Unable to create socket");
    DBUG_RETURN(-1);
  }
  if (bind_address)
  {
    int err;
    if ((err = s.bind(bind_address, 0)) != 0)
    {
      fprintf(handle->errstream,
              "Unable to bind local address '%s:0' err: %d, errno: %d, "
              "while trying to connect with connect string: '%s:%d'\n",
              bind_address, err, errno, hostname, port);
      setError(handle, NDB_MGM_BIND_ADDRESS, __LINE__,
               "Unable to bind local address '%s:0' errno: %d, errno: %d, "
               "while trying to connect with connect string: '%s:%d'\n",
               bind_address, err, errno, hostname, port);
      DBUG_RETURN(-1);
    }
  }
  const NDB_SOCKET_TYPE sockfd = s.connect(hostname, port);
  if (!ndb_socket_valid(sockfd))
  {
    setError(handle, NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, __LINE__,
	     "Unable to connect to");
    DBUG_RETURN(-2);
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

  NDB_SOCKET_TYPE tmp = handle->socket;
  handle->socket = sockfd;

  const Properties *reply;
  reply = ndb_mgm_call(handle, stat_reply, "listen event", &args);

  handle->socket = tmp;

  if(reply == NULL) {
    ndb_socket_close(sockfd);
    CHECK_REPLY(handle, reply, -1);
  }
  delete reply;

  *sock= sockfd;
  DBUG_RETURN(1);
}

/*
  This API function causes ugly code in mgmapi - it returns native socket
  type as we can't force everybody to use our abstraction or break current
  applications.
 */
extern "C"
ndb_native_socket_t
ndb_mgm_listen_event(NdbMgmHandle handle, const int filter[])
{
  NDB_SOCKET_TYPE s;
  if(ndb_mgm_listen_event_internal(handle,filter,0,&s)<0)
    ndb_socket_invalidate(&s);
  return ndb_socket_get_native(s);
}

extern "C"
int 
ndb_mgm_dump_state(NdbMgmHandle handle, int nodeId, const int * _args,
		   int _num_args, struct ndb_mgm_reply* /* reply */) 
{
  DBUG_ENTER("ndb_mgm_dump_state");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_dump_state");
  const ParserRow<ParserDummy> dump_state_reply[] = {
    MGM_CMD("dump state reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  char buf[256];
  buf[0] = 0;
  for (int i = 0; i < _num_args; i++){
    unsigned n = (unsigned)strlen(buf);
    if (n + 20 > sizeof(buf)) {
      SET_ERROR(handle, NDB_MGM_USAGE_ERROR, "arguments too long");
      DBUG_RETURN(-1);
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
    DBUG_RETURN(-1);
  }

  delete prop;
  DBUG_RETURN(0);
}

extern "C"
struct ndb_mgm_configuration *
ndb_mgm_get_configuration_from_node(NdbMgmHandle handle,
                                    int nodeid)
{
  return ndb_mgm_get_configuration2(handle, 0,
                                    NDB_MGM_NODE_TYPE_UNKNOWN, nodeid);
}

extern "C"
int 
ndb_mgm_start_signallog(NdbMgmHandle handle, int nodeId, 
			struct ndb_mgm_reply* reply) 
{
  DBUG_ENTER("ndb_mgm_start_signallog");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start_signallog");
  const ParserRow<ParserDummy> start_signallog_reply[] = {
    MGM_CMD("start signallog reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
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

  DBUG_RETURN(retval);
}

extern "C"
int 
ndb_mgm_stop_signallog(NdbMgmHandle handle, int nodeId,
		       struct ndb_mgm_reply* reply) 
{
  DBUG_ENTER("ndb_mgm_stop_signallog");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_stop_signallog");
  const ParserRow<ParserDummy> stop_signallog_reply[] = {
    MGM_CMD("stop signallog reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
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

  DBUG_RETURN(retval);
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
  DBUG_ENTER("ndb_mgm_log_signals");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_log_signals");
  const ParserRow<ParserDummy> stop_signallog_reply[] = {
    MGM_CMD("log signals reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
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

  DBUG_RETURN(retval);
}

extern "C"
int 
ndb_mgm_set_trace(NdbMgmHandle handle, int nodeId, int traceNumber,
		  struct ndb_mgm_reply* reply) 
{
  DBUG_ENTER("ndb_mgm_set_trace");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_trace");
  const ParserRow<ParserDummy> set_trace_reply[] = {
    MGM_CMD("set trace reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
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

  DBUG_RETURN(retval);
}

int
ndb_mgm_insert_error_impl(NdbMgmHandle handle, int nodeId,
                          int errorCode,
                          int * extra,
                          struct ndb_mgm_reply* reply)
{
  DBUG_ENTER("ndb_mgm_insert_error");

  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_insert_error");
  const ParserRow<ParserDummy> insert_error_reply[] = {
    MGM_CMD("insert error reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int retval = -1;
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("node", nodeId);
  args.put("error", errorCode);
  if (extra)
  {
    args.put("extra", * extra);
  }

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

  DBUG_RETURN(retval);
}

extern "C"
int
ndb_mgm_insert_error(NdbMgmHandle handle, int nodeId, int errorCode,
		     struct ndb_mgm_reply* reply)
{
  return ndb_mgm_insert_error_impl(handle, nodeId, errorCode, 0, reply);
}

extern "C"
int
ndb_mgm_insert_error2(NdbMgmHandle handle, int nodeId,
                      int errorCode, int extra,
		     struct ndb_mgm_reply* reply)
{
  return ndb_mgm_insert_error_impl(handle, nodeId, errorCode, &extra, reply);
}

extern "C"
int 
ndb_mgm_start(NdbMgmHandle handle, int no_of_nodes, const int * node_list)
{
  DBUG_ENTER("ndb_mgm_start");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start");
  const ParserRow<ParserDummy> start_reply[] = {
    MGM_CMD("start reply", NULL, ""),
    MGM_ARG("started", Int, Optional, "No of started nodes"),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_END()
  };
  int started = 0;
  CHECK_CONNECTED(handle, -1);

  if(no_of_nodes < 0){
    SET_ERROR(handle, EINVAL, "");
    DBUG_RETURN(-1);
  }

  if(no_of_nodes == 0){
    Properties args;
    const Properties *reply;
    reply = ndb_mgm_call(handle, start_reply, "start all", &args);
    CHECK_REPLY(handle, reply, -1);

    Uint32 count = 0;
    if(!reply->get("started", &count)){
      delete reply;
      DBUG_RETURN(-1);
    }
    delete reply;
    DBUG_RETURN(count);
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
        DBUG_RETURN(-1);
      }
    }
    delete reply;
  }

  DBUG_RETURN(started);
}

/*****************************************************************************
 * Backup
 *****************************************************************************/
extern "C"
int 
ndb_mgm_start_backup3(NdbMgmHandle handle, int wait_completed,
		     unsigned int* _backup_id,
		     struct ndb_mgm_reply*, /*reply*/
		     unsigned int input_backupId,
		     unsigned int backuppoint) 
{
  DBUG_ENTER("ndb_mgm_start_backup");

  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_start_backup");
  const ParserRow<ParserDummy> start_backup_reply[] = {
    MGM_CMD("start backup reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),
    MGM_ARG("id", Int, Optional, "Id of the started backup"),
    MGM_END()
  };
  CHECK_CONNECTED(handle, -1);

  if (!get_mgmd_version(handle))
    DBUG_RETURN(-1);

  bool sendBackupPoint = (handle->mgmd_version() >= NDB_MAKE_VERSION(6,4,0));

  Properties args;
  args.put("completed", wait_completed);
  if(input_backupId > 0)
    args.put("backupid", input_backupId);
  if (sendBackupPoint)
    args.put("backuppoint", backuppoint);

  const Properties *reply;
  { // start backup can take some time, set timeout high
    int old_timeout= handle->timeout;
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
    DBUG_RETURN(-1);
  }

  delete reply;
  DBUG_RETURN(0);
}

extern "C"
int 
ndb_mgm_start_backup2(NdbMgmHandle handle, int wait_completed,
		     unsigned int* _backup_id,
		     struct ndb_mgm_reply* reply,
		     unsigned int input_backupId)
{
  return ndb_mgm_start_backup3(handle, wait_completed, _backup_id, reply, input_backupId, 0);
}

extern "C"
int 
ndb_mgm_start_backup(NdbMgmHandle handle, int wait_completed,
		     unsigned int* _backup_id,
		     struct ndb_mgm_reply* reply)
{
  return ndb_mgm_start_backup2(handle, wait_completed, _backup_id, reply, 0);
}

extern "C"
int
ndb_mgm_abort_backup(NdbMgmHandle handle, unsigned int backupId,
		     struct ndb_mgm_reply* /*reply*/) 
{
  DBUG_ENTER("ndb_mgm_abort_backup");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_abort_backup");
  const ParserRow<ParserDummy> stop_backup_reply[] = {
    MGM_CMD("abort backup reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),    
    MGM_END()
  };
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
    DBUG_RETURN(-1);
  }

  delete prop;
  DBUG_RETURN(0);
}

extern "C"
struct ndb_mgm_configuration *
ndb_mgm_get_configuration2(NdbMgmHandle handle, unsigned int version,
                           enum ndb_mgm_node_type nodetype, int from_node)
{
  DBUG_ENTER("ndb_mgm_get_configuration2");

  CHECK_HANDLE(handle, NULL);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_get_configuration");
  CHECK_CONNECTED(handle, NULL);

  if (!get_mgmd_version(handle))
    DBUG_RETURN(NULL);

  bool getConfigUsingNodetype =
    (handle->mgmd_version() >= NDB_MAKE_VERSION(6,4,0));

  Properties args;
  args.put("version", version);
  if (getConfigUsingNodetype)
  {
    args.put("nodetype", nodetype);
  }

  bool v2 = ndb_config_version_v2(handle->mgmd_version());
  if (from_node != 0)
  {
    if (check_version_new(handle->mgmd_version(),
                          NDB_MAKE_VERSION(7,1,16),
                          NDB_MAKE_VERSION(7,0,27),
                          0))
    {
      args.put("from_node", from_node);
    }
    else
    {
      SET_ERROR(handle, NDB_MGM_GET_CONFIG_FAILED,
                "The mgm server does not support getting config from_node");
      DBUG_RETURN(0);
    }
  }
  else if (v2)
  {
    Uint32 node_id = ndb_mgm_get_configuration_nodeid(handle);
    args.put("node", node_id);
  }

  const ParserRow<ParserDummy> reply[] = {
    MGM_CMD("get config reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Error message"),    
    MGM_ARG("Content-Length", Int, Optional, "Content length in bytes"),
    MGM_ARG("Content-Type", String, Optional, "Type (octet-stream)"),
    MGM_ARG("Content-Transfer-Encoding", String, Optional, "Encoding(base64)"),
    MGM_END()
  };

  const Properties *prop;
  prop = v2 ?
    ndb_mgm_call(handle, reply, "get config_v2", &args) :
    ndb_mgm_call(handle, reply, "get config", &args);
  CHECK_REPLY(handle, prop, 0);
  
  do {
    const char * buf = "<unknown error>";
    if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0){
      fprintf(handle->errstream, "ERROR Message: %s\n\n", buf);
      SET_ERROR(handle, NDB_MGM_GET_CONFIG_FAILED, buf);
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
			     &buf64[start], (int)(len-start))) < 1){
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
    const int res = ndb_base64_decode(buf64, len-1, tmp_data, NULL);
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
    const int res2 = v2 ?
      cvf.unpack_v2_buf(tmp) :
      cvf.unpack_v1_buf(tmp);
    if(!res2){
      fprintf(handle->errstream, "Failed to unpack buffer\n");
      break;
    }

    delete prop;
    DBUG_RETURN((ndb_mgm_configuration*)cvf.getConfigValues());
  } while(0);

  delete prop;
  DBUG_RETURN(0);
}

extern "C"
struct ndb_mgm_configuration *
ndb_mgm_get_configuration(NdbMgmHandle handle, unsigned int version)
{
  return ndb_mgm_get_configuration2(handle, version,
                                    NDB_MGM_NODE_TYPE_UNKNOWN);
}

extern "C"
void
ndb_mgm_destroy_configuration(struct ndb_mgm_configuration *cfg)
{
    delete cfg;
}

extern "C"
int
ndb_mgm_set_configuration_nodeid(NdbMgmHandle handle, int nodeid)
{
  DBUG_ENTER("ndb_mgm_set_configuration_nodeid");
  CHECK_HANDLE(handle, -1);
  handle->cfg._ownNodeId= nodeid;
  DBUG_RETURN(0);
}

extern "C"
int
ndb_mgm_get_configuration_nodeid(NdbMgmHandle handle)
{
  DBUG_ENTER("ndb_mgm_get_configuration_nodeid");
  CHECK_HANDLE(handle, 0); // Zero is an invalid nodeid
  DBUG_RETURN(handle->cfg._ownNodeId);
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
const char *ndb_mgm_get_connected_bind_address(NdbMgmHandle handle)
{
  if (handle->cfg_i >= 0)
  {
    if (handle->m_bindaddress)
      return handle->m_bindaddress;
    if (handle->cfg.ids[handle->cfg_i].bind_address.length())
      return handle->cfg.ids[handle->cfg_i].bind_address.c_str();
  }
  return 0;
}

extern "C"
int
ndb_mgm_alloc_nodeid(NdbMgmHandle handle, unsigned int version, int nodetype,
                     int log_event)
{
  DBUG_ENTER("ndb_mgm_alloc_nodeid");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_alloc_nodeid");
  CHECK_CONNECTED(handle, -1);
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
      Uint32 error_code= NDB_MGM_ALLOCID_ERROR;
      prop->get("error_code", &error_code);
      setError(handle, error_code, __LINE__,
              "Could not alloc node id at %s port %d: %s",
               hostname, port, buf);
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
  DBUG_RETURN(nodeid);
}

extern "C"
int
ndb_mgm_set_int_parameter(NdbMgmHandle handle,
			  int node, 
			  int param,
			  unsigned value,
			  struct ndb_mgm_reply*){
  DBUG_ENTER("ndb_mgm_set_int_parameter");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
  Properties args;
  args.put("node", node);
  args.put("param", param);
  args.put64("value", value);
  
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
  DBUG_RETURN(res);
}

extern "C"
int 
ndb_mgm_set_int64_parameter(NdbMgmHandle handle,
			    int node, 
			    int param,
			    unsigned long long value,
			    struct ndb_mgm_reply*){
  DBUG_ENTER("ndb_mgm_set_int64_parameter");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
  Properties args;
  args.put("node", node);
  args.put("param", param);
  args.put64("value", value);
  
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
    DBUG_RETURN(-1);
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
  DBUG_RETURN(res);
}

extern "C"
int
ndb_mgm_set_string_parameter(NdbMgmHandle handle,
			     int node, 
			     int param,
			     const char * value,
			     struct ndb_mgm_reply*){
  DBUG_ENTER("ndb_mgm_set_string_parameter");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
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
    DBUG_RETURN(-1);
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
  DBUG_RETURN(res);
}

extern "C"
int
ndb_mgm_purge_stale_sessions(NdbMgmHandle handle, char **purged)
{
  DBUG_ENTER("ndb_mgm_purge_stale_sessions");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
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
    DBUG_RETURN(-1);
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
  DBUG_RETURN(res);
}

extern "C"
int
ndb_mgm_check_connection(NdbMgmHandle handle)
{
  DBUG_ENTER("ndb_mgm_check_connection");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  SocketOutputStream out(handle->socket, handle->timeout);
  SocketInputStream in(handle->socket, handle->timeout);
  char buf[32];
  if (out.println("check connection"))
    goto ndb_mgm_check_connection_error;

  if (out.println("%s", ""))
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

  DBUG_RETURN(0);

ndb_mgm_check_connection_error:
  ndb_mgm_disconnect(handle);
  DBUG_RETURN(-1);
}

extern "C"
int
ndb_mgm_set_connection_int_parameter(NdbMgmHandle handle,
				     int node1,
				     int node2,
				     int param,
				     int value,
				     struct ndb_mgm_reply* mgmreply){
  DBUG_ENTER("ndb_mgm_set_connection_int_parameter");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
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
  DBUG_ENTER("ndb_mgm_get_connection_int_parameter");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);
  
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
  CHECK_REPLY(handle, prop, -3);

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
  DBUG_ENTER("ndb_mgm_convert_to_transporter");
  NDB_SOCKET_TYPE s;

  if(handle == 0)
  {
    SET_ERROR(*handle, NDB_MGM_ILLEGAL_SERVER_HANDLE, "");
    ndb_socket_invalidate(&s);
    DBUG_RETURN(s);
  }

  if ((*handle)->connected != 1)
  {
    SET_ERROR(*handle, NDB_MGM_SERVER_NOT_CONNECTED , "");
    ndb_socket_invalidate(&s);
    DBUG_RETURN(s);
  }

  (*handle)->connected= 0;   // we pretend we're disconnected
  s= (*handle)->socket;

  SocketOutputStream s_output(s, (*handle)->timeout);
  s_output.println("transporter connect");
  s_output.println("%s", "");

  ndb_mgm_destroy_handle(handle); // set connected=0, so won't disconnect

  DBUG_RETURN(s);
}

extern "C"
Uint32
ndb_mgm_get_mgmd_nodeid(NdbMgmHandle handle)
{
  DBUG_ENTER("ndb_mgm_get_mgmd_nodeid");
  Uint32 nodeid=0;

  CHECK_HANDLE(handle, 0); // Zero is an invalid nodeid
  CHECK_CONNECTED(handle, 0); // Zero is an invalid nodeid
  
  Properties args;

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("get mgmd nodeid reply", NULL, ""),
    MGM_ARG("nodeid", Int, Mandatory, "Node ID"),
    MGM_END()
  };
  
  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get mgmd nodeid", &args);
  CHECK_REPLY(handle, prop, 0);

  if(!prop->get("nodeid",&nodeid)){
    fprintf(handle->errstream, "Unable to get value\n");
    DBUG_RETURN(0);
  }

  delete prop;
  DBUG_RETURN(nodeid);
}

extern "C"
int ndb_mgm_report_event(NdbMgmHandle handle, Uint32 *data, Uint32 length)
{
  DBUG_ENTER("ndb_mgm_report_event");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

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
  CHECK_REPLY(handle, prop, -1);

  delete prop;
  DBUG_RETURN(0);
}

extern "C"
int ndb_mgm_end_session(NdbMgmHandle handle)
{
  DBUG_ENTER("ndb_mgm_end_session");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  SocketOutputStream s_output(handle->socket, handle->timeout);
  const char *end_session_str = "end session";
  s_output.println("%s", end_session_str);
  s_output.println("%s", "");

  SocketInputStream in(handle->socket, handle->timeout);
  char buf[32];
  in.gets(buf, sizeof(buf));
  CHECK_TIMEDOUT_RET(handle, in, s_output, -1, end_session_str);

  DBUG_RETURN(0);
}

extern "C"
int ndb_mgm_get_version(NdbMgmHandle handle,
                        int *major, int *minor, int *build, int len, char* str)
{
  DBUG_ENTER("ndb_mgm_get_version");
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("version", NULL, ""),
    MGM_ARG("id", Int, Mandatory, "ID"),
    MGM_ARG("major", Int, Mandatory, "Major"),
    MGM_ARG("minor", Int, Mandatory, "Minor"),
    MGM_ARG("build", Int, Optional, "Build"),
    MGM_ARG("string", String, Mandatory, "String"),
    MGM_ARG("mysql_major", Int, Optional, "MySQL major"),
    MGM_ARG("mysql_minor", Int, Optional, "MySQL minor"),
    MGM_ARG("mysql_build", Int, Optional, "MySQL build"),
    MGM_END()
  };

  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "get version", &args);
  CHECK_REPLY(handle, prop, 0);

  Uint32 id;
  if(!prop->get("id",&id)){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
              "Unable to get version id");
    DBUG_RETURN(0);
  }
  *build= getBuild(id);

  if(!prop->get("major",(Uint32*)major)){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
              "Unable to get version major");
    DBUG_RETURN(0);
  }

  if(!prop->get("minor",(Uint32*)minor)){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
              "Unable to get version minor");
    DBUG_RETURN(0);
  }

  BaseString result;
  if(!prop->get("string", result)){
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
              "Unable to get version string");
    DBUG_RETURN(0);
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
    DBUG_RETURN(0);
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
  CHECK_HANDLE(handle, -1);
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("id", (Uint32)id);

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

extern "C"
int
ndb_mgm_set_configuration(NdbMgmHandle h, ndb_mgm_configuration *c)
{
  DBUG_ENTER("ndb_mgm_set_configuration");
  CHECK_HANDLE(h, -1);
  SET_ERROR(h, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_configuration");
  CHECK_CONNECTED(h, -1);

  const ConfigValues * cfg = (ConfigValues*)c;

  UtilBuffer buf;
  bool v2 = ndb_config_version_v2(h->mgmd_version());
  bool ret = v2 ? cfg->pack_v2(buf) :
                  cfg->pack_v1(buf);
  if (!ret)
  {
    SET_ERROR(h, NDB_MGM_OUT_OF_MEMORY, "Packing config");
    DBUG_RETURN(-1);
  }

  BaseString encoded;
  /*
    The base64 encoded data of BaseString can be of max length (1024*1024)/3*4
    hence using int to store the length.
  */
  encoded.assfmt("%*s", (int)base64_needed_encoded_length(buf.length()), "Z");
  (void) base64_encode(buf.get_data(), buf.length(), (char*)encoded.c_str());

  Properties args;
  args.put("Content-Length", (Uint32)strlen(encoded.c_str()));
  args.put("Content-Type",  "ndbconfig/octet-stream");
  args.put("Content-Transfer-Encoding", "base64");

  const ParserRow<ParserDummy> set_config_reply[]= {
    MGM_CMD("set config reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Result"),
    MGM_END()
  };

  const Properties *reply;
  const char *cmd_str = v2 ? "set config_v2" : "set config";
  reply= ndb_mgm_call(h, set_config_reply, cmd_str, &args,
                      encoded.c_str());
  CHECK_REPLY(h, reply, -1);

  BaseString result;
  reply->get("result",result);

  delete reply;

  if(strcmp(result.c_str(), "Ok") != 0) {
    SET_ERROR(h, NDB_MGM_CONFIG_CHANGE_FAILED, result.c_str());
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}


extern "C"
int ndb_mgm_create_nodegroup(NdbMgmHandle handle,
                             int *nodes,
                             int *ng,
                             struct ndb_mgm_reply* mgmreply)
{
  DBUG_ENTER("ndb_mgm_create_nodegroup");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_create_nodegroup");
  CHECK_CONNECTED(handle, -1);

  BaseString nodestr;
  for (int i = 0; nodes[i] != 0; i++)
    nodestr.appfmt("%u ", nodes[i]);

  Properties args;
  args.put("nodes", nodestr.c_str());

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("create nodegroup reply", NULL, ""),
    MGM_ARG("ng", Int, Mandatory, "NG Id"),
    MGM_ARG("error_code", Int, Optional, "error_code"),
    MGM_ARG("result", String, Mandatory, "Result"),
    MGM_END()
  };

  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "create nodegroup", &args);
  CHECK_REPLY(handle, prop, -3);

  int res = 0;
  const char * buf = 0;
  if (!prop->get("result", &buf) || strcmp(buf, "Ok") != 0)
  {
    res = -1;
    Uint32 err = NDB_MGM_ILLEGAL_SERVER_REPLY;
    prop->get("error_code", &err);
    setError(handle, err, __LINE__, "%s", buf ? buf : "Illegal reply");
  }
  else if (!prop->get("ng",(Uint32*)ng))
  {
    res = -1;
    setError(handle, NDB_MGM_ILLEGAL_SERVER_REPLY, __LINE__,
             "Nodegroup not sent back in reply");
  }

  delete prop;
  DBUG_RETURN(res);
}

extern "C"
int ndb_mgm_drop_nodegroup(NdbMgmHandle handle,
                           int ng,
                           struct ndb_mgm_reply* mgmreply)
{
  DBUG_ENTER("ndb_mgm_drop_nodegroup");
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_create_nodegroup");
  CHECK_CONNECTED(handle, -1);

  Properties args;
  args.put("ng", ng);

  const ParserRow<ParserDummy> reply[]= {
    MGM_CMD("drop nodegroup reply", NULL, ""),
    MGM_ARG("error_code", Int, Optional, "error_code"),
    MGM_ARG("result", String, Mandatory, "Result"),
    MGM_END()
  };

  const Properties *prop;
  prop = ndb_mgm_call(handle, reply, "drop nodegroup", &args);
  CHECK_REPLY(handle, prop, -3);

  int res= 0;
  const char * buf = 0;
  if(!prop->get("result", &buf) || strcmp(buf, "Ok") != 0)
  {
    res = -1;
    Uint32 err = NDB_MGM_ILLEGAL_SERVER_REPLY;
    prop->get("error_code", &err);
    setError(handle, err, __LINE__, "%s", buf ? buf : "Illegal reply");
  }

  delete prop;
  DBUG_RETURN(res);
}


NDB_SOCKET_TYPE _ndb_mgm_get_socket(NdbMgmHandle h)
{
  return h->socket;
}


/*
  Compare function for qsort() to sort events in
  "source_node_id" order
*/

static int
cmp_event(const void *_a, const void *_b)
{
  const ndb_logevent *a = (const ndb_logevent*)_a;
  const ndb_logevent *b = (const ndb_logevent*)_b;

  // So far all events are of same type
  assert(a->type == b->type);

  // Primarily sort on source_nodeid
  const unsigned diff = (a->source_nodeid - b->source_nodeid);
  if (diff)
    return diff;

  // Equal nodeid, go into more detailed compare
  // for some event types where order is important
  switch(a->type){
  case NDB_LE_MemoryUsage:
    // Return DataMemory before IndexMemory (ie. TUP vs ACC)
    return (b->MemoryUsage.block - a->MemoryUsage.block);
    break;

  default:
    break;
  }

  return 0;
}

NdbLogEventHandle
ndb_mgm_create_logevent_handle_same_socket(NdbMgmHandle mh);

// Free memory allocated by 'ndb_mgm_create_logevent_handle_same_socket'
// without closing the socket
static void
free_log_handle(NdbLogEventHandle log_handle)
{
  free(log_handle);
}


extern "C"
struct ndb_mgm_events*
ndb_mgm_dump_events(NdbMgmHandle handle, enum Ndb_logevent_type type,
                    int no_of_nodes, const int * node_list)
{
  DBUG_ENTER("ndb_mgm_dump_events");
  CHECK_HANDLE(handle, NULL);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_dump_events");
  CHECK_CONNECTED(handle, NULL);

  Properties args;
  args.put("type", (Uint32)type);

  if (no_of_nodes)
  {
    const char* separator = "";
    BaseString nodes;
    for(int node = 0; node < no_of_nodes; node++)
    {
      nodes.appfmt("%s%d", separator, node_list[node]);
      separator = ",";
    }
    args.put("nodes", nodes.c_str());
  }

  const ParserRow<ParserDummy> dump_events_reply[] = {
    MGM_CMD("dump events reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Ok or error message"),
    MGM_ARG("events", Int, Optional, "Number of events that follows"),
    MGM_END()
  };
  const Properties *reply = ndb_mgm_call(handle, dump_events_reply,
                                         "dump events", &args);
  CHECK_REPLY(handle, reply, NULL);

  // Check the result for Ok or error
  const char * result;
  reply->get("result", &result);
  if (strcmp(result, "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_USAGE_ERROR, result);
    delete reply;
    DBUG_RETURN(NULL);
  }

  // Get number of events to read
  Uint32 num_events;
  if (!reply->get("events", &num_events))
  {
    SET_ERROR(handle, NDB_MGM_ILLEGAL_SERVER_REPLY,
              "Number of events missing");
    delete reply;
    DBUG_RETURN(NULL);
  }

  delete reply;

  // Read the streamed events
  ndb_mgm_events *events =
    (ndb_mgm_events*)
      malloc(sizeof(ndb_mgm_events) +
             num_events*sizeof(ndb_logevent));
  if(!events)
  {
    SET_ERROR(handle, NDB_MGM_OUT_OF_MEMORY,
              "Allocating ndb_mgm_events struct");
    DBUG_RETURN(NULL);
  }

  // Initialize log event handle to read the requested events
  NdbLogEventHandle log_handle =
    ndb_mgm_create_logevent_handle_same_socket(handle);
  if(!log_handle)
  {
    SET_ERROR(handle, NDB_MGM_OUT_OF_MEMORY, "Creating logevent handle");
    DBUG_RETURN(NULL);
  }

  Uint32 i = 0;
  while (i < num_events)
  {
    int res = ndb_logevent_get_next(log_handle,
                                    &(events->events[i]),
                                    handle->timeout);
    if (res == 0)
    {
      free(events);
      free_log_handle(log_handle);
      SET_ERROR(handle, ETIMEDOUT,
                "Time out talking to management server");
      DBUG_RETURN(NULL);
    }
    if (res == -1)
    {
      free(events);
      free_log_handle(log_handle);
      SET_ERROR(handle,
                ndb_logevent_get_latest_error(log_handle),
                ndb_logevent_get_latest_error_msg(log_handle));
      DBUG_RETURN(NULL);
    }

    i++;
  }
  free_log_handle(log_handle);

  // Successfully parsed the list of events, sort on nodeid and return them
  events->no_of_events= num_events;
  qsort(events->events, events->no_of_events,
        sizeof(events->events[0]), cmp_event);
  DBUG_RETURN(events);
}


static
int
set_dynamic_ports_batched(NdbMgmHandle handle, int nodeid,
                          struct ndb_mgm_dynamic_port* ports,
                          unsigned num_ports)
{
  DBUG_ENTER("set_dynamic_ports_batched");

  Properties args;
  args.put("node", (Uint32)nodeid);
  args.put("num_ports", (Uint32)num_ports);

  /*
    Build the list of nodeid/port pairs which is sent as
    name value pairs in bulk part of request
  */
  BaseString port_list;
  for(unsigned i = 0; i < num_ports; i++)
  {
    port_list.appfmt("%d=%d\n", ports[i].nodeid, ports[i].port);
  }

  const ParserRow<ParserDummy> set_ports_reply[] = {
    MGM_CMD("set ports reply", NULL, ""),
    MGM_ARG("result", String, Mandatory, "Ok or error message"),
    MGM_END()
  };
  const Properties *reply = ndb_mgm_call(handle, set_ports_reply,
                                         "set ports", &args,
                                         port_list.c_str());
  CHECK_REPLY(handle, reply, -1);

  // Check the result for Ok or error
  const char * result;
  reply->get("result", &result);
  if (strcmp(result, "Ok") != 0) {
    SET_ERROR(handle, NDB_MGM_USAGE_ERROR, result);
    delete reply;
    DBUG_RETURN(-1);
  }

  delete reply;
  DBUG_RETURN(0);
}


extern "C"
int
ndb_mgm_set_dynamic_ports(NdbMgmHandle handle, int nodeid,
                          struct ndb_mgm_dynamic_port* ports,
                          unsigned num_ports)
{
  DBUG_ENTER("ndb_mgm_set_dynamic_ports");
  DBUG_PRINT("enter", ("nodeid: %d, num_ports: %u", nodeid, num_ports));
  CHECK_HANDLE(handle, -1);
  SET_ERROR(handle, NDB_MGM_NO_ERROR, "Executing: ndb_mgm_set_dynamic_ports");
  CHECK_CONNECTED(handle, -1);

  if (num_ports == 0)
  {
    SET_ERROR(handle, NDB_MGM_USAGE_ERROR,
              "Illegal number of dynamic ports given in num_ports");
    DBUG_RETURN(-1);
  }

  // Check that the ports seems to contain reasonable numbers
  for (unsigned i = 0; i < num_ports; i++)
  {
    if (ports[i].nodeid == 0)
    {
      SET_ERROR(handle, NDB_MGM_USAGE_ERROR,
                "Illegal nodeid specfied in ports array");
      DBUG_RETURN(-1);
    }

    if (ports[i].port >= 0)
    {
      // Only negative dynamic ports allowed
      SET_ERROR(handle, NDB_MGM_USAGE_ERROR,
                "Illegal port specfied in ports array");
      DBUG_RETURN(-1);
    }
  }

  if (!get_mgmd_version(handle))
    DBUG_RETURN(-1);

  if (check_version_new(handle->mgmd_version(),
                        NDB_MAKE_VERSION(7,3,3),
                        NDB_MAKE_VERSION(7,2,14),
                        NDB_MAKE_VERSION(7,1,28),
                        NDB_MAKE_VERSION(7,0,40),
                        0))
  {
    // The ndb_mgmd supports reporting all ports at once
    DBUG_RETURN(set_dynamic_ports_batched(handle, nodeid,
                                          ports, num_ports));
  }

  // Report the ports one at a time
  for (unsigned i = 0; i < num_ports; i++)
  {
    struct ndb_mgm_reply mgm_reply;
    const int err = ndb_mgm_set_connection_int_parameter(handle,
                                                         nodeid,
                                                         ports[i].nodeid,
                                                         CFG_CONNECTION_SERVER_PORT,
                                                         ports[i].port,
                                                         &mgm_reply);
    if (err < 0)
    {
      setError(handle, handle->last_error, __LINE__,
               "Could not set dynamic port for %d->%d",
               nodeid, ports[i].nodeid);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}


template class Vector<const ParserRow<ParserDummy>*>;
