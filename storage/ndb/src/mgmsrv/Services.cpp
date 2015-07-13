/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <uucode.h>
#include <socket_io.h>
#include <util/version.h>
#include <mgmapi.h>
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <LogLevel.hpp>
#include <BaseString.hpp>

#include <ConfigValues.hpp>
#include <mgmapi_configuration.hpp>
#include <Vector.hpp>
#include "Services.hpp"
#include "../mgmapi/ndb_logevent.hpp"

#include "ndb_mgmd_error.h"

#include <ndb_base64.h>
#include <ndberror.h>

extern bool g_StopServer;
extern bool g_RestartServer;
extern EventLogger * g_eventLogger;

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

#define MGM_CMD(name, fun, desc) \
 { name, \
   0, \
   ParserRow<MgmApiSession>::Cmd, \
   ParserRow<MgmApiSession>::String, \
   ParserRow<MgmApiSession>::Optional, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   0, 0, \
   fun, \
   desc, 0 }

#define MGM_ARG(name, type, opt, desc) \
 { name, \
   0, \
   ParserRow<MgmApiSession>::Arg, \
   ParserRow<MgmApiSession>::type, \
   ParserRow<MgmApiSession>::opt, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
  desc, 0 }

#define MGM_ARG2(name, type, opt, min, max, desc) \
 { name, \
   0, \
   ParserRow<MgmApiSession>::Arg, \
   ParserRow<MgmApiSession>::type, \
   ParserRow<MgmApiSession>::opt, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   min, max, \
   0, \
  desc, 0 }

#define MGM_END() \
 { 0, \
   0, \
   ParserRow<MgmApiSession>::End, \
   ParserRow<MgmApiSession>::Int, \
   ParserRow<MgmApiSession>::Optional, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

#define MGM_CMD_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<MgmApiSession>::CmdAlias, \
   ParserRow<MgmApiSession>::Int, \
   ParserRow<MgmApiSession>::Optional, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

#define MGM_ARG_ALIAS(name, realName, fun) \
 { name, \
   realName, \
   ParserRow<MgmApiSession>::ArgAlias, \
   ParserRow<MgmApiSession>::Int, \
   ParserRow<MgmApiSession>::Optional, \
   ParserRow<MgmApiSession>::IgnoreMinMax, \
   0, 0, \
   0, \
   0, 0 }

const
ParserRow<MgmApiSession> commands[] = {
  MGM_CMD("get config", &MgmApiSession::getConfig, ""),
    MGM_ARG("version", Int, Mandatory, "Configuration version number"),
    MGM_ARG("node", Int, Optional, "Node ID"),
    MGM_ARG("nodetype", Int, Optional, "Type of requesting node"),
    MGM_ARG("from_node", Int, Optional, "Node to get config from"),

  MGM_CMD("get nodeid", &MgmApiSession::get_nodeid, ""),
    MGM_ARG("version", Int, Mandatory, "Configuration version number"),
    MGM_ARG("nodetype", Int, Mandatory, "Node type"),
    MGM_ARG("transporter", String, Optional, "Transporter type"),
    MGM_ARG("nodeid", Int, Optional, "Node ID"),
    MGM_ARG("user", String, Mandatory, "Password"),
    MGM_ARG("password", String, Mandatory, "Password"),
    MGM_ARG("public key", String, Mandatory, "Public key"),
    MGM_ARG("endian", String, Optional, "Endianness"),
    MGM_ARG("name", String, Optional, "Name of connection"),
    MGM_ARG("timeout", Int, Optional, "Timeout in seconds"),
    MGM_ARG("log_event", Int, Optional, "Log failure in cluster log"),

  MGM_CMD("get version", &MgmApiSession::getVersion, ""),

  MGM_CMD("get status", &MgmApiSession::getStatus, ""),
    MGM_ARG("types", String, Optional, "Types"), 
 
  MGM_CMD("get info clusterlog", &MgmApiSession::getInfoClusterLog, ""),
  MGM_CMD("get cluster loglevel", &MgmApiSession::getClusterLogLevel, ""),

  MGM_CMD("restart node", &MgmApiSession::restart_v1, ""),
    MGM_ARG("node", String, Mandatory, "Nodes to restart"),
    MGM_ARG("initialstart", Int, Optional, "Initial start"),
    MGM_ARG("nostart", Int, Optional, "No start"),
    MGM_ARG("abort", Int, Optional, "Abort"),

  MGM_CMD("restart node v2", &MgmApiSession::restart_v2, ""),
    MGM_ARG("node", String, Mandatory, "Nodes to restart"),
    MGM_ARG("initialstart", Int, Optional, "Initial start"),
    MGM_ARG("nostart", Int, Optional, "No start"),
    MGM_ARG("abort", Int, Optional, "Abort"),
    MGM_ARG("force", Int, Optional, "Force"),

  MGM_CMD("restart all", &MgmApiSession::restartAll, ""),
    MGM_ARG("initialstart", Int, Optional, "Initial start"),
    MGM_ARG("nostart", Int, Optional, "No start"),
    MGM_ARG("abort", Int, Optional, "Abort"),

  MGM_CMD("insert error", &MgmApiSession::insertError, ""),
    MGM_ARG("node", Int, Mandatory, "Node to receive error"),
    MGM_ARG("error", Int, Mandatory, "Errorcode to insert"),
    MGM_ARG("extra", Int, Optional, "Extra info to error insert"),

  MGM_CMD("set trace", &MgmApiSession::setTrace, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("trace", Int, Mandatory, "Trace number"),

  MGM_CMD("log signals", &MgmApiSession::logSignals, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("blocks", String, Mandatory, "Blocks (space separated)"),
    MGM_ARG("in", Int, Mandatory, "Log input signals"),
    MGM_ARG("out", Int, Mandatory, "Log output signals"),

  MGM_CMD("start signallog", &MgmApiSession::startSignalLog, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),

  MGM_CMD("stop signallog", &MgmApiSession::stopSignalLog, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),

  MGM_CMD("dump state", &MgmApiSession::dumpState, ""),
    MGM_ARG("node", Int, Mandatory ,"Node"),
    MGM_ARG("args", String, Mandatory, "Args(space separated int's)"),

  MGM_CMD("start backup", &MgmApiSession::startBackup, ""),
    MGM_ARG("completed", Int, Optional ,"Wait until completed"),
    MGM_ARG("backupid", Int, Optional ,"User input backup id"),
    MGM_ARG("backuppoint", Int, Optional ,"backup snapshot at start time or complete time"),

  MGM_CMD("abort backup", &MgmApiSession::abortBackup, ""),
    MGM_ARG("id", Int, Mandatory, "Backup id"),

  MGM_CMD("stop", &MgmApiSession::stop_v1, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("abort", Int, Mandatory, "Node"),

  MGM_CMD("stop v2", &MgmApiSession::stop_v2, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("abort", Int, Mandatory, "Node"),
    MGM_ARG("force", Int, Optional, "Force"),

  MGM_CMD("stop all", &MgmApiSession::stopAll, ""),
    MGM_ARG("abort", Int, Mandatory, "Node"),
    MGM_ARG("stop", String, Optional, "MGM/DB or both"),

  MGM_CMD("enter single user", &MgmApiSession::enterSingleUser, ""),
    MGM_ARG("nodeId", Int, Mandatory, "Node"),
  
  MGM_CMD("exit single user", &MgmApiSession::exitSingleUser, ""),
  

  MGM_CMD("start", &MgmApiSession::start, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),

  MGM_CMD("start all", &MgmApiSession::startAll, ""),

  MGM_CMD("bye", &MgmApiSession::bye, ""),

  MGM_CMD("end session", &MgmApiSession::endSession, ""),

  MGM_CMD("set loglevel", &MgmApiSession::setLogLevel, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("category", Int, Mandatory, "Event category"),
    MGM_ARG("level", Int, Mandatory, "Log level (0-15)"),

  MGM_CMD("set cluster loglevel", &MgmApiSession::setClusterLogLevel, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("category", Int, Mandatory, "Event category"),
    MGM_ARG("level", Int, Mandatory, "Log level (0-15)"),

  MGM_CMD("set logfilter", &MgmApiSession::setLogFilter, ""),
    MGM_ARG("level", Int, Mandatory, "Severety level"),
    MGM_ARG("enable", Int, Mandatory, "1=disable, 0=enable, -1=toggle"),

  MGM_CMD("set parameter", &MgmApiSession::setParameter, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("parameter", Int, Mandatory, "Parameter"),
    MGM_ARG("value", String, Mandatory, "Value"),

  MGM_CMD("set connection parameter",
	  &MgmApiSession::setConnectionParameter, ""),
    MGM_ARG("node1", Int, Mandatory, "Node1 ID"),
    MGM_ARG("node2", Int, Mandatory, "Node2 ID"),
    MGM_ARG("param", Int, Mandatory, "Parameter"),
    MGM_ARG("value", Int, Mandatory, "Value"),

  MGM_CMD("get connection parameter",
	  &MgmApiSession::getConnectionParameter, ""),
    MGM_ARG("node1", Int, Mandatory, "Node1 ID"),
    MGM_ARG("node2", Int, Mandatory, "Node2 ID"),
    MGM_ARG("param", Int, Mandatory, "Parameter"),

  MGM_CMD("listen event", &MgmApiSession::listen_event, ""),
    MGM_ARG("node", Int, Optional, "Node"),  
    MGM_ARG("parsable", Int, Optional, "Parsable"),  
    MGM_ARG("filter", String, Mandatory, "Event category"),

  MGM_CMD("purge stale sessions", &MgmApiSession::purge_stale_sessions, ""),

  MGM_CMD("check connection", &MgmApiSession::check_connection, ""),

  MGM_CMD("transporter connect", &MgmApiSession::transporter_connect, ""),

  MGM_CMD("get mgmd nodeid", &MgmApiSession::get_mgmd_nodeid, ""),

  MGM_CMD("report event", &MgmApiSession::report_event, ""),
    MGM_ARG("length", Int, Mandatory, "Length"),
    MGM_ARG("data", String, Mandatory, "Data"),

  MGM_CMD("list sessions", &MgmApiSession::listSessions, ""),

  MGM_CMD("get session id", &MgmApiSession::getSessionId, ""),

  MGM_CMD("get session", &MgmApiSession::getSession, ""),
    MGM_ARG("id", Int, Mandatory, "SessionID"),

  MGM_CMD("set config", &MgmApiSession::setConfig, ""),
    MGM_ARG("Content-Length", Int, Mandatory, "Length of config"),
    MGM_ARG("Content-Type", String, Mandatory, "Type of config"),
    MGM_ARG("Content-Transfer-Encoding", String, Mandatory, "encoding"),

  MGM_CMD("create nodegroup", &MgmApiSession::create_nodegroup, ""),
    MGM_ARG("nodes", String, Mandatory, "Nodes"),

  MGM_CMD("drop nodegroup", &MgmApiSession::drop_nodegroup, ""),
    MGM_ARG("ng", Int, Mandatory, "Nodegroup"),

  MGM_CMD("show config", &MgmApiSession::showConfig, ""),
    MGM_ARG("Section", String, Optional, "Section name"),
    MGM_ARG("NodeId", Int, Optional, "Nodeid"),
    MGM_ARG("Name", String, Optional, "Parameter name"),

  MGM_CMD("reload config", &MgmApiSession::reloadConfig, ""),
    MGM_ARG("config_filename", String, Optional, "Reload from path"),
    MGM_ARG("mycnf", Int, Optional, "Reload from my.cnf"),
    MGM_ARG("force", Int, Optional, "Force reload"),

  MGM_CMD("show variables", &MgmApiSession::show_variables, ""),

  MGM_CMD("dump events", &MgmApiSession::dump_events, ""),
    MGM_ARG("type", Int, Mandatory, "Type of event"),
    MGM_ARG("nodes", String, Optional, "Nodes to include"),

  MGM_CMD("set ports", &MgmApiSession::set_ports, ""),
    MGM_ARG("node", Int, Mandatory, "Node which port list concerns"),
    MGM_ARG("num_ports", Int, Mandatory, "Number of ports being set"),

  MGM_END()
};

extern int g_errorInsert;
#define ERROR_INSERTED(x) (g_errorInsert == x || m_errorInsert == x)

#define SLEEP_ERROR_INSERTED(x) if(ERROR_INSERTED(x)){NdbSleep_SecSleep(10);}

MgmApiSession::MgmApiSession(class MgmtSrvr & mgm, NDB_SOCKET_TYPE sock, Uint64 session_id)
  : SocketServer::Session(sock), m_mgmsrv(mgm),
    m_session_id(session_id), m_name("unknown:0")
{
  DBUG_ENTER("MgmApiSession::MgmApiSession");
  m_input = new SocketInputStream(sock, SOCKET_TIMEOUT);
  m_output = new BufferedSockOutputStream(sock, SOCKET_TIMEOUT);
  m_parser = new Parser_t(commands, *m_input);
  m_stopSelf= 0;
  m_ctx= NULL;
  m_mutex= NdbMutex_Create();
  m_errorInsert= 0;

  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  if (my_getpeername(sock, (struct sockaddr*)&addr, &addrlen) == 0)
  {
    char addr_buf[NDB_ADDR_STRLEN];
    char *addr_str = Ndb_inet_ntop(AF_INET,
                                   static_cast<void*>(&addr.sin_addr),
                                   addr_buf,
                                   (socklen_t)sizeof(addr_buf));
    m_name.assfmt("%s:%d", addr_str, ntohs(addr.sin_port));
  }
  DBUG_PRINT("info", ("new connection from: %s", m_name.c_str()));

  DBUG_VOID_RETURN;
}

MgmApiSession::~MgmApiSession()
{
  DBUG_ENTER("MgmApiSession::~MgmApiSession");
  if (m_input)
    delete m_input;
  if (m_output)
    delete m_output;
  if (m_parser)
    delete m_parser;
  if(my_socket_valid(m_socket))
  {
    NDB_CLOSE_SOCKET(m_socket);
    my_socket_invalidate(&m_socket);
  }
  if(m_stopSelf < 0)
    g_RestartServer= true;
  if(m_stopSelf)
    g_StopServer= true;
  NdbMutex_Destroy(m_mutex);
  DBUG_VOID_RETURN;
}

void
MgmApiSession::runSession()
{
  DBUG_ENTER("MgmApiSession::runSession");

  g_eventLogger->debug("%s: Connected!", name());

  Parser_t::Context ctx;
  ctx.m_mutex= m_mutex;
  m_ctx= &ctx;
  bool stop= false;
  while(!stop) {
    NdbMutex_Lock(m_mutex);

    m_input->reset_timeout();
    m_output->reset_timeout();

    if (m_parser->run(ctx, *this))
    {
      stop= m_stop; // Has session been stopped
      assert(ctx.m_status == Parser_t::Ok);
    }
    else
    {
      stop= m_stop; // Has session been stopped
      const char* msg= NULL;
      switch(ctx.m_status) {
      case Parser_t::Eof:    // Client disconnected
        stop= true;
        g_eventLogger->debug("%s: Eof!", name());
        break;

      case Parser_t::ExternalStop: // Stopped by other thread
        stop= true;
        g_eventLogger->debug("%s: ExternalStop!", name());
        break;

      case Parser_t::NoLine: // Normal read timeout
      case Parser_t::EmptyLine:
        break;

      case Parser_t::UnknownCommand: msg= "Unknown command"; break;
      case Parser_t::UnknownArgument: msg= "Unknown argument"; break;
      case Parser_t::TypeMismatch: msg= "Type mismatch"; break;
      case Parser_t::InvalidArgumentFormat: msg= "Invalid arg. format"; break;
      case Parser_t::UnknownArgumentType: msg= "Unknown argument type"; break;
      case Parser_t::ArgumentGivenTwice: msg= "Argument given twice"; break;
      case Parser_t::MissingMandatoryArgument: msg= "Missing arg."; break;

      case Parser_t::Ok: // Should never happen here
      case Parser_t::CommandWithoutFunction:
        abort();
        break;
      }

      if (msg){
        g_eventLogger->debug("%s: %s, '%s'",
                             name(),
                             msg,
                             ctx.m_currentToken != 0 ?
                             ctx.m_currentToken : "<NULL>");

        // Send result to client
        m_output->println("result: %s, '%s'",
                          msg,
                          ctx.m_currentToken != 0 ?
                          ctx.m_currentToken : "<NULL>");
        m_output->print("\n");
      }
    }

    NdbMutex_Unlock(m_mutex);

    // Send output from command to the client
    m_output->flush();

  }

  g_eventLogger->debug("%s: Stopped!", name());

  NdbMutex_Lock(m_mutex);
  m_ctx= NULL;
  if(my_socket_valid(m_socket))
  {
    my_socket_close(m_socket);
    my_socket_invalidate(&m_socket);
  }
  NdbMutex_Unlock(m_mutex);

  g_eventLogger->debug("%s: Disconnected!", name());

  DBUG_VOID_RETURN;
}

void
MgmApiSession::get_nodeid(Parser_t::Context &,
			  const class Properties &args)
{
  Uint32 version, nodeid= 0, nodetype= 0xff;
  Uint32 timeout= 20;  // timeout in seconds
  const char * endian= NULL;
  const char * name= NULL;
  Uint32 log_event= 1;

  args.get("version", &version);
  args.get("nodetype", &nodetype);
  // transporter
  args.get("nodeid", &nodeid);
  // user
  // password
  // public key
  args.get("endian", &endian);
  args.get("name", &name);
  args.get("timeout", &timeout);
  /* for backwards compatability keep track if client uses new protocol */
  const bool log_event_version= args.get("log_event", &log_event);

  m_output->println("get nodeid reply");

  // Check that client says it's using same endian
  {
    union { long l; char c[sizeof(long)]; } endian_check;
    endian_check.l = 1;
    if (endian &&
        strcmp(endian,(endian_check.c[sizeof(long)-1])?"big":"little")!=0)
    {
      m_output->println("result: Node does not have the same "
                        "endianness as the management server.");
      m_output->println("%s", "");
      return;
    }
  }

  bool compatible;
  switch (nodetype) {
  case NODE_TYPE_MGM:
  case NODE_TYPE_API:
    compatible = ndbCompatible_mgmt_api(NDB_VERSION, version);
    break;
  case NODE_TYPE_DB:
    compatible = ndbCompatible_mgmt_ndb(NDB_VERSION, version);
    break;
  default:
    m_output->println("result: unknown nodetype %d", nodetype);
    m_output->println("%s", "");
    return;
  }

  struct sockaddr_in addr;
  {
    SOCKET_SIZE_TYPE addrlen= sizeof(addr);
    int r = my_getpeername(m_socket, (struct sockaddr*)&addr, &addrlen);
    if (r != 0 )
    {
      m_output->println("result: getpeername(" MY_SOCKET_FORMAT \
                        ") failed, err= %d",
                        MY_SOCKET_FORMAT_VALUE(m_socket), r);
      m_output->println("%s", "");
      return;
    }
  }

  /* Check nodeid parameter */
  if (nodeid > MAX_NODES_ID)
  {
    m_output->println("result: illegal nodeid %u", nodeid);
    m_output->println("%s", "");
    return;
  }

  NodeId tmp= nodeid;
  BaseString error_string;
  int error_code;
  if (!m_mgmsrv.alloc_node_id(tmp,
                              (ndb_mgm_node_type)nodetype,
                              (struct sockaddr*)&addr,
                              error_code, error_string,
                              log_event,
                              timeout))
  {
    m_output->println("result: %s", error_string.c_str());
    /* only use error_code in reply if client knows about it */
    if (log_event_version)
      m_output->println("error_code: %d", error_code);
    m_output->println("%s", "");
    return;
  }

  m_output->println("nodeid: %u", tmp);
  m_output->println("result: Ok");
  m_output->println("%s", "");

  if (name)
    g_eventLogger->info("Node %d: %s", tmp, name);

  return;
}

void
MgmApiSession::getConfig(Parser_t::Context &,
                         const class Properties &args)
{
  Uint32 nodetype = NDB_MGM_NODE_TYPE_UNKNOWN;
  Uint32 from_node = 0;

  // Ignoring mandatory parameter "version"
  // Ignoring optional parameter "node"
  args.get("nodetype", &nodetype);
  args.get("from_node", &from_node);

  SLEEP_ERROR_INSERTED(1);
  m_output->println("get config reply");

  BaseString pack64, error;

  UtilBuffer packed;

  bool success = (from_node > 0) ?
                 m_mgmsrv.get_packed_config_from_node(from_node,
                                                      pack64, error) :
                 m_mgmsrv.get_packed_config((ndb_mgm_node_type)nodetype,
                                            pack64, error);

  if (!success)
  {
    m_output->println("result: %s", error.c_str());
    m_output->print("\n");
    return;
  }

  m_output->println("result: Ok");
  m_output->println("Content-Length: %u", pack64.length());
  m_output->println("Content-Type: ndbconfig/octet-stream");
  SLEEP_ERROR_INSERTED(2);
  m_output->println("Content-Transfer-Encoding: base64");
  m_output->print("\n");

  unsigned len = (unsigned)strlen(pack64.c_str());
  if(ERROR_INSERTED(3))
  {
    // Return only half the packed config
    BaseString half64 = pack64.substr(0, pack64.length());
    m_output->write(half64.c_str(), (unsigned)strlen(half64.c_str()));
    m_output->write("\n", 1);
    return;
  }
  m_output->write(pack64.c_str(), len);
  m_output->write("\n\n", 2);
  return;
}

void
MgmApiSession::insertError(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 extra = 0;
  Uint32 node = 0, error = 0;
  int result= 0;

  args.get("node", &node);
  args.get("error", &error);
  const bool hasExtra = args.get("extra", &extra);
  Uint32 * extraptr = hasExtra ? &extra : 0;

  if(node==m_mgmsrv.getOwnNodeId()
     && error < MGM_ERROR_MAX_INJECT_SESSION_ONLY)
  {
    m_errorInsert= error;
    if(error==0)
      g_errorInsert= error;
  }
  else
  {
    result= m_mgmsrv.insertError(node, error, extraptr);
  }

  m_output->println("insert error reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::setTrace(Parser<MgmApiSession>::Context &,
			Properties const &args) {
  Uint32 node = 0, trace = 0;

  args.get("node", &node);
  args.get("trace", &trace);

  int result = m_mgmsrv.setTraceNo(node, trace);

  m_output->println("set trace reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::getVersion(Parser<MgmApiSession>::Context &,
			  Properties const &) {
  m_output->println("version");
  m_output->println("id: %d", NDB_VERSION_D);
  m_output->println("major: %d", NDB_VERSION_MAJOR);
  m_output->println("minor: %d", NDB_VERSION_MINOR);
  m_output->println("build: %d", NDB_VERSION_BUILD);
  m_output->println("string: %s", m_mgmsrv.get_version_string());
  m_output->println("mysql_major: %d", NDB_MYSQL_VERSION_MAJOR);
  m_output->println("mysql_minor: %d", NDB_MYSQL_VERSION_MINOR);
  m_output->println("mysql_build: %d", NDB_MYSQL_VERSION_BUILD);
  m_output->println("%s", "");
}

void
MgmApiSession::startBackup(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  DBUG_ENTER("MgmApiSession::startBackup");
  unsigned backupId;
  unsigned input_backupId= 0;
  unsigned backuppoint= 0;
  Uint32 completed= 2;
  int result;

  args.get("completed", &completed);

  if(args.contains("backupid"))
    args.get("backupid", &input_backupId);
  if(args.contains("backuppoint"))
    args.get("backuppoint", &backuppoint);

  result = m_mgmsrv.startBackup(backupId, completed, input_backupId, backuppoint);

  m_output->println("start backup reply");
  if(result != 0)
  {
    m_output->println("result: %s", get_error_text(result));
  }
  else{
    m_output->println("result: Ok");
    if (completed)
      m_output->println("id: %d", backupId);
  }
  m_output->println("%s", "");
  DBUG_VOID_RETURN;
}

void
MgmApiSession::abortBackup(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 id = 0;

  args.get("id", &id);

  int result = m_mgmsrv.abortBackup(id);

  m_output->println("abort backup reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

/*****************************************************************************/

void
MgmApiSession::dumpState(Parser<MgmApiSession>::Context &,
			 Properties const &args) {
  Uint32 node;
  BaseString args_str;

  args.get("node", &node);
  args.get("args", args_str);

  int result = m_mgmsrv.dumpState(node, args_str.c_str());
  m_output->println("dump state reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}


void
MgmApiSession::bye(Parser<MgmApiSession>::Context &,
                   Properties const &) {
  m_stop = true;
}

void
MgmApiSession::endSession(Parser<MgmApiSession>::Context &,
                          Properties const &)
{
  SLEEP_ERROR_INSERTED(4);
  m_output->println("end session reply");
}

void
MgmApiSession::getClusterLogLevel(Parser<MgmApiSession>::Context &			, Properties const &) {
  const char* names[] = { "startup",
			  "shutdown", 
			  "statistics", 
			  "checkpoint", 
			  "noderestart", 
			  "connection", 
			  "info", 
			  "warning", 
			  "error", 
			  "congestion", 
			  "debug", 
                          "backup",
                          "schema" };

  int const loglevel_count = (CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1);
  NDB_STATIC_ASSERT(NDB_ARRAY_SIZE(names) == loglevel_count);
  LogLevel::EventCategory category;

  m_output->println("get cluster loglevel");
  for(int i = 0; i < loglevel_count; i++) {
    category = (LogLevel::EventCategory) i;
    m_output->println("%s: %d", names[i], m_mgmsrv.m_event_listner[0].m_logLevel.getLogLevel(category));
  }
  m_output->println("%s", "");
}

void
MgmApiSession::setClusterLogLevel(Parser<MgmApiSession>::Context &,
				  Properties const &args) {
  Uint32 node, level, cat;
  BaseString errorString;
  DBUG_ENTER("MgmApiSession::setClusterLogLevel");
  args.get("node", &node);
  args.get("category", &cat);
  args.get("level", &level);

  DBUG_PRINT("enter",("node=%d, category=%d, level=%d", node, cat, level));

  m_output->println("set cluster loglevel reply");

  if(level > NDB_MGM_MAX_LOGLEVEL) {
    m_output->println("result: Invalid loglevel %d", level);
    m_output->println("%s", "");
    DBUG_VOID_RETURN;
  }

  LogLevel::EventCategory category= 
    (LogLevel::EventCategory)(cat-(int)CFG_MIN_LOGLEVEL);

  m_mgmsrv.m_event_listner.lock();
  if (m_mgmsrv.m_event_listner[0].m_logLevel.setLogLevel(category,level))
  {
    m_output->println("result: Invalid category %d", category);
    m_output->println("%s", "");
    m_mgmsrv.m_event_listner.unlock();
    DBUG_VOID_RETURN;
  }
  m_mgmsrv.m_event_listner.unlock();

  {
    LogLevel tmp;
    m_mgmsrv.m_event_listner.update_max_log_level(tmp);
  }

  m_output->println("result: Ok");
  m_output->println("%s", "");
  DBUG_VOID_RETURN;
}

void
MgmApiSession::setLogLevel(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 node = 0, level = 0, cat;
  BaseString errorString;
  SetLogLevelOrd logLevel;
  logLevel.clear();
  args.get("node", &node);
  args.get("category", &cat);
  args.get("level", &level);

  if(level > NDB_MGM_MAX_LOGLEVEL) {
    m_output->println("set loglevel reply");
    m_output->println("result: Invalid loglevel: %s", errorString.c_str());
    m_output->println("%s", "");
    return;
  }

  LogLevel::EventCategory category= 
    (LogLevel::EventCategory)(cat-(int)CFG_MIN_LOGLEVEL);

  {
    LogLevel ll;
    ll.setLogLevel(category,level);
    m_mgmsrv.m_event_listner.update_max_log_level(ll);
  }

  m_output->println("set loglevel reply");
  m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::stopSignalLog(Parser<MgmApiSession>::Context &,
			     Properties const &args) {
  Uint32 node;

  args.get("node", &node);

  int result = m_mgmsrv.stopSignalTracing(node);

  m_output->println("stop signallog");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::restart_v1(Parser<MgmApiSession>::Context &,
		       Properties const &args) {
  restart(args,1);
}

void
MgmApiSession::restart_v2(Parser<MgmApiSession>::Context &,
		       Properties const &args) {
  restart(args,2);
}

void
MgmApiSession::restart(Properties const &args, int version) {
  Uint32
    nostart = 0,
    initialstart = 0,
    abort = 0, force = 0;
  char *nodes_str;
  Vector<NodeId> nodes;
    
  args.get("initialstart", &initialstart);
  args.get("nostart", &nostart);
  args.get("abort", &abort);
  args.get("node", (const char **)&nodes_str);
  args.get("force", &force);

  char *p, *last;
  for((p = my_strtok_r(nodes_str, " ", &last));
      p;
      (p = my_strtok_r(NULL, " ", &last))) {
    nodes.push_back(atoi(p));
  }

  int restarted = 0;
  int result= m_mgmsrv.restartNodes(nodes,
                                    &restarted,
                                    nostart != 0,
                                    initialstart != 0,
                                    abort != 0,
                                    force != 0,
                                    &m_stopSelf);

  if (result == UNSUPPORTED_NODE_SHUTDOWN && nodes.size() > 1 && force)
  {
    /**
     * We don't support multi node graceful shutdown...
     *   add "-a" and try again
     */
    abort = 1;
    result= m_mgmsrv.restartNodes(nodes,
                                  &restarted,
                                  nostart != 0,
                                  initialstart != 0,
                                  abort != 0,
                                  force != 0,
                                  &m_stopSelf);
  }

  if (force &&
      (result == NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH ||
       result == UNSUPPORTED_NODE_SHUTDOWN))
  {
    // Force restart by restarting all nodes
    result = m_mgmsrv.restartDB(nostart, initialstart, false, &restarted);
  }

  m_output->println("restart reply");
  if(result != 0){
    m_output->println("result: %d-%s", result, get_error_text(result));
  } else
    m_output->println("result: Ok");
  m_output->println("restarted: %d", restarted);
  if(version>1)
    m_output->println("disconnect: %d", (m_stopSelf)?1:0);
  m_output->println("%s", "");
}

void
MgmApiSession::restartAll(Parser<MgmApiSession>::Context &, 
			  Properties const &args)
{
  Uint32 nostart = 0;
  Uint32 initialstart = 0;
  Uint32 abort = 0;
  
  args.get("initialstart", &initialstart);
  args.get("abort", &abort);
  args.get("nostart", &nostart);
  
  int count = 0;
  int result = m_mgmsrv.restartDB(nostart, initialstart, abort, &count);

  m_output->println("restart reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("restarted: %d", count);
  m_output->println("%s", "");
}

static void
printNodeStatus(OutputStream *output,
		MgmtSrvr &mgmsrv,
		enum ndb_mgm_node_type type) {
  NodeId nodeId = 0;
  while(mgmsrv.getNextNodeId(&nodeId, type)) {
    enum ndb_mgm_node_status status;
    Uint32 startPhase = 0, 
      version = 0, mysql_version = 0,
      dynamicId = 0, 
      nodeGroup = 0,
      connectCount = 0;
    bool system;
    const char *address= NULL;
    char addr_buf[NDB_ADDR_STRLEN];

    mgmsrv.status(nodeId, &status, &version, &mysql_version, &startPhase,
		  &system, &dynamicId, &nodeGroup, &connectCount,
		  &address,
                  addr_buf,
                  sizeof(addr_buf));
    output->println("node.%d.type: %s",
		      nodeId,
		      ndb_mgm_get_node_type_string(type));
    output->println("node.%d.status: %s",
		      nodeId,
		    ndb_mgm_get_node_status_string(status));
    output->println("node.%d.version: %d", nodeId, version);
    output->println("node.%d.mysql_version: %d", nodeId, mysql_version);
    output->println("node.%d.startphase: %d", nodeId, startPhase);
    output->println("node.%d.dynamic_id: %d", nodeId, dynamicId);
    output->println("node.%d.node_group: %d", nodeId, nodeGroup);
    output->println("node.%d.connect_count: %d", nodeId, connectCount);
    output->println("node.%d.address: %s", nodeId, address ? address : "");
  }
}

void
MgmApiSession::getStatus(Parser<MgmApiSession>::Context &,
			 Properties const & args) {
  Uint32 i;
  int noOfNodes = 0;
  BaseString typestring;

  enum ndb_mgm_node_type types[10];
  if (args.get("types", typestring))
  {
    Vector<BaseString> tmp;
    typestring.split(tmp, " ");
    for (i = 0; i < tmp.size(); i++)
    {
      types[i] = ndb_mgm_match_node_type(tmp[i].c_str());
    }
    types[i] = NDB_MGM_NODE_TYPE_UNKNOWN;    
  }
  else
  {
    types[0] = NDB_MGM_NODE_TYPE_NDB;
    types[1] = NDB_MGM_NODE_TYPE_MGM;
    types[2] = NDB_MGM_NODE_TYPE_API;
    types[3] = NDB_MGM_NODE_TYPE_UNKNOWN;
  }
  
  for (i = 0; types[i] != NDB_MGM_NODE_TYPE_UNKNOWN; i++)
  {
    NodeId nodeId = 0;
    while(m_mgmsrv.getNextNodeId(&nodeId, types[i]))
      noOfNodes++;
  }
  
  SLEEP_ERROR_INSERTED(5);
  m_output->println("node status");
  SLEEP_ERROR_INSERTED(6);
  m_output->println("nodes: %d", noOfNodes);
  for (i = 0; types[i] != NDB_MGM_NODE_TYPE_UNKNOWN; i++)
  {
    SLEEP_ERROR_INSERTED(int(7+i));
    printNodeStatus(m_output, m_mgmsrv, types[i]);
  }
  m_output->println("%s", "");
}


static bool
isEventLogFilterEnabled(int severity)
{
  return g_eventLogger->isEnable((Logger::LoggerLevel)severity);
}

void
MgmApiSession::getInfoClusterLog(Parser<MgmApiSession>::Context &,
		    Properties const &) {
  const char* names[] = { "enabled",
			  "debug",
			  "info",
			  "warning",
			  "error",
			  "critical",
			  "alert" };

  m_output->println("clusterlog");
  for(int i = 0; i < 7; i++) {
    m_output->println("%s: %d",
                      names[i],
                      isEventLogFilterEnabled(i));
  }
  m_output->println("%s", "");
}

void
MgmApiSession::stop_v1(Parser<MgmApiSession>::Context &,
                       Properties const &args) {
  stop(args,1);
}

void
MgmApiSession::stop_v2(Parser<MgmApiSession>::Context &,
                       Properties const &args) {
  stop(args,2);
}

void
MgmApiSession::stop(Properties const &args, int version) {
  Uint32 abort, force = 0;
  char *nodes_str;
  Vector<NodeId> nodes;

  args.get("node", (const char **)&nodes_str);
  if(nodes_str == NULL)
  {
    m_output->println("stop reply");
    m_output->println("result: empty node list");
    m_output->println("%s", "");
    return;
  }
  args.get("abort", &abort);
  args.get("force", &force);

  char *p, *last;
  for((p = my_strtok_r(nodes_str, " ", &last));
      p;
      (p = my_strtok_r(NULL, " ", &last))) {
    nodes.push_back(atoi(p));
  }

  int stopped= 0;
  int result= 0;
  if (nodes.size())
  {
    result= m_mgmsrv.stopNodes(nodes, &stopped, abort != 0, force != 0,
                               &m_stopSelf);

    if (result == UNSUPPORTED_NODE_SHUTDOWN && nodes.size() > 1 && force)
    {
      /**
       * We don't support multi node graceful shutdown...
       *   add "-a" and try again
       */
      abort = 1;
      result= m_mgmsrv.stopNodes(nodes, &stopped, abort != 0, force != 0,
                                 &m_stopSelf);
    }

    if (force &&
        (result == NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH ||
         result == UNSUPPORTED_NODE_SHUTDOWN))
    {
      // Force stop and shutdown all remaining nodes
      result = m_mgmsrv.shutdownDB(&stopped, false);
    }
  }

  m_output->println("stop reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("stopped: %d", stopped);
  if(version>1)
    m_output->println("disconnect: %d", (m_stopSelf)?1:0);
  m_output->println("%s", "");
}

void
MgmApiSession::stopAll(Parser<MgmApiSession>::Context &,
                       Properties const &args) {
  int stopped[2] = {0,0};
  Uint32 abort;
  args.get("abort", &abort);

  BaseString stop;
  const char* tostop= "db";
  int ver=1;
  if (args.get("stop", stop))
  {
    tostop= stop.c_str();
    ver= 2;
  }

  int result= 0;
  if(strstr(tostop,"db"))
    result= m_mgmsrv.shutdownDB(&stopped[0], abort != 0);
  if(!result && strstr(tostop,"mgm"))
    result= m_mgmsrv.shutdownMGM(&stopped[1], abort!=0, &m_stopSelf);

  m_output->println("stop reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("stopped: %d", stopped[0]+stopped[1]);
  if(ver >1)
    m_output->println("disconnect: %d", (m_stopSelf)?1:0);
  m_output->println("%s", "");
}

void
MgmApiSession::enterSingleUser(Parser<MgmApiSession>::Context &,
			  Properties const &args) {
  int stopped = 0;
  Uint32 nodeId = 0;
  int result= 0;
  args.get("nodeId", &nodeId);

  result = m_mgmsrv.enterSingleUser(&stopped, nodeId);
  m_output->println("enter single user reply");
  if(result != 0) {
    m_output->println("result: %s", get_error_text(result));
  }
  else {
    m_output->println("result: Ok");
  }
  m_output->println("%s", "");
}

void
MgmApiSession::exitSingleUser(Parser<MgmApiSession>::Context &,
			      Properties const &args) {
  int stopped = 0;
  int result = m_mgmsrv.exitSingleUser(&stopped, false);
  m_output->println("exit single user reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}


void
MgmApiSession::startSignalLog(Parser<MgmApiSession>::Context &,
			      Properties const &args) {
  Uint32 node;

  args.get("node", &node);

  int result = m_mgmsrv.startSignalTracing(node);

  m_output->println("start signallog reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::logSignals(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 node = 0, in = 0, out = 0;
  //  BaseString blocks;
  BaseString blockList;
  char * blockName;
  args.get("node", &node);
  args.get("in", &in);
  args.get("out", &out);
  args.get("blocks", blockList);
  // fast fix - pekka
  char buf[200];
  BaseString::snprintf(buf, 200, "%s", blockList.c_str());
  Vector<BaseString> blocks;  

  blockName=strtok(buf,"|");
  while( blockName != NULL)
    {
      blocks.push_back(blockName);
      blockName=strtok(NULL,"|");
    }
  

  if(in > 1 || out > 1)
    return; /* Invalid arguments */
  
  const MgmtSrvr::LogMode modes[] = {
    MgmtSrvr::Off,
    MgmtSrvr::Out,
    MgmtSrvr::In,
    MgmtSrvr::InOut,
  };
  MgmtSrvr::LogMode mode = modes[in<<1 | out];

  int result = m_mgmsrv.setSignalLoggingMode(node, mode, blocks);

  m_output->println("log signals reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::start(Parser<MgmApiSession>::Context &,
		     Properties const &args) {
  Uint32 node;

  args.get("node", &node);
  
  int result = m_mgmsrv.sendSTART_ORD(node);

  m_output->println("start reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::startAll(Parser<MgmApiSession>::Context &,
			Properties const &) {
  NodeId node = 0;
  int started = 0;

  while(m_mgmsrv.getNextNodeId(&node, NDB_MGM_NODE_TYPE_NDB))
    if(m_mgmsrv.sendSTART_ORD(node) == 0)
      started++;

  m_output->println("start reply");
  m_output->println("result: Ok");
  m_output->println("started: %d", started);
  m_output->println("%s", "");
}


static bool
setEventLogFilter(int severity, int enable)
{
  Logger::LoggerLevel level = (Logger::LoggerLevel)severity;
  if (enable > 0) {
    g_eventLogger->enable(level);
  } else if (enable == 0) {
    g_eventLogger->disable(level);
  }
  else
  {
    /* enable is < 0 => toggle the filter value */
    if (g_eventLogger->isEnable(level))
      g_eventLogger->disable(level);
    else
      g_eventLogger->enable(level);
  }
  return g_eventLogger->isEnable(level);
}


void
MgmApiSession::setLogFilter(Parser_t::Context &ctx,
			    const class Properties &args) {
  Uint32 severity;
  Uint32 enable;

  args.get("level", &severity);
  args.get("enable", &enable);

  bool result = setEventLogFilter(severity, enable);

  m_output->println("set logfilter reply");
  m_output->println("result: %d", result);
  m_output->println("%s", "");
}

#ifdef NOT_USED

static NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<LogLevel::LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
  return out;
}
#endif


static void
logevent2str(BaseString& str, int eventType,
             const Uint32* theData,
             Uint32 len,
             NodeId nodeId, Uint32 timeval,
             char* pretty_text, size_t pretty_text_size)
{
  str.assign("log event reply\n");
  str.appfmt("type=%d\n", eventType);
  str.appfmt("time=%d\n", timeval);
  str.appfmt("source_nodeid=%d\n", nodeId);
  unsigned i;
  for (i = 0; ndb_logevent_body[i].token; i++)
  {
    if ( ndb_logevent_body[i].type == eventType)
      break;
  }

  if (ndb_logevent_body[i].token)
  {
    do {
      int val= theData[ndb_logevent_body[i].index];
      if (ndb_logevent_body[i].index_fn)
        val= (*(ndb_logevent_body[i].index_fn))(val);
      str.appfmt("%s=%d\n",ndb_logevent_body[i].token, val);
      if(strcmp(ndb_logevent_body[i].token,"error") == 0)
      {
        int pretty_text_len= (int)strlen(pretty_text);
        if(pretty_text_size-pretty_text_len-3 > 0)
        {
          BaseString::snprintf(pretty_text+pretty_text_len, 4 , " - ");
          ndb_error_string(val, pretty_text+(pretty_text_len+3),
                           (int)(pretty_text_size-pretty_text_len-3));
        }
      }
    } while (ndb_logevent_body[++i].type == eventType);
  }
  else
  {
    str.append("data=");
    for (i = 1; i<len; i++)
      str.appfmt("%u ", theData[i]);
    str.append("\n");
  }
}


void
Ndb_mgmd_event_service::log(int eventType, const Uint32* theData, 
			    Uint32 len, NodeId nodeId){
  
  Uint32 threshold;
  LogLevel::EventCategory cat;
  Logger::LoggerLevel severity;
  EventLoggerBase::EventTextFunction textF;
  int i, n;
  DBUG_ENTER("Ndb_mgmd_event_service::log");
  DBUG_PRINT("enter",("eventType=%d, nodeid=%d", eventType, nodeId));

  if (EventLoggerBase::event_lookup(eventType,cat,threshold,severity,textF))
    DBUG_VOID_RETURN;

  // Generate the message for pretty format clients
  char pretty_text[512];
  EventLogger::getText(pretty_text, sizeof(pretty_text),
		       textF, theData, len, nodeId);

  // Generate the message for parseable format clients
  // and if there is a field named "error" append the ndb_error_string
  // for that error number to the end of the pretty format message
  BaseString str;
  logevent2str(str, eventType, theData, len, nodeId, 0,
               pretty_text, sizeof(pretty_text));

  Vector<NDB_SOCKET_TYPE> copy;
  m_clients.lock();
  for(i = m_clients.size() - 1; i >= 0; i--)
  {
    if(threshold <= m_clients[i].m_logLevel.getLogLevel(cat))
    {
      if(!my_socket_valid(m_clients[i].m_socket))
        continue;

      SocketOutputStream out(m_clients[i].m_socket);

      int r;
      if (m_clients[i].m_parsable)
      {
        unsigned len = str.length();
        r= out.write(str.c_str(), len);
      }
      else
      {
        unsigned len = (unsigned)strlen(pretty_text);
        r= out.write(pretty_text, len);
      }

      if (! (r < 0))
      {
        r = out.write("\n", 1);
      }

      if (r<0)
      {
        copy.push_back(m_clients[i].m_socket);
        m_clients.erase(i, false);
      }
    }
  }
  m_clients.unlock();
  
  if ((n= (int)copy.size()))
  {
    for(i= 0; i < n; i++)
      NDB_CLOSE_SOCKET(copy[i]);

    LogLevel tmp; tmp.clear();
    m_clients.lock();
    for(i= m_clients.size() - 1; i >= 0; i--)
      tmp.set_max(m_clients[i].m_logLevel);
    m_clients.unlock();
    update_log_level(tmp);
  }
  DBUG_VOID_RETURN;
}

void
Ndb_mgmd_event_service::update_max_log_level(const LogLevel &log_level)
{
  LogLevel tmp = log_level;
  m_clients.lock();
  for(int i = m_clients.size() - 1; i >= 0; i--)
    tmp.set_max(m_clients[i].m_logLevel);
  m_clients.unlock();
  update_log_level(tmp);
}

void
Ndb_mgmd_event_service::update_log_level(const LogLevel &tmp)
{
  m_logLevel = tmp;
  EventSubscribeReq req;
  req.assign(tmp);
  // send update to all nodes
  req.blockRef = 0;
  m_mgmsrv->m_log_level_requests.push_back(req);
}

void
Ndb_mgmd_event_service::check_listeners()
{
  int i, n= 0;
  DBUG_ENTER("Ndb_mgmd_event_service::check_listeners");
  m_clients.lock();
  for(i= m_clients.size() - 1; i >= 0; i--)
  {
    if(!my_socket_valid(m_clients[i].m_socket))
      continue;

    SocketOutputStream out(m_clients[i].m_socket);

    DBUG_PRINT("info",("%d " MY_SOCKET_FORMAT,
                       i,
                       MY_SOCKET_FORMAT_VALUE(m_clients[i].m_socket)));

    if(out.println("<PING>") < 0)
    {
      NDB_CLOSE_SOCKET(m_clients[i].m_socket);
      m_clients.erase(i, false);
      n=1;
    }
  }
  if (n)
  {
    LogLevel tmp; tmp.clear();
    for(i= m_clients.size() - 1; i >= 0; i--)
      tmp.set_max(m_clients[i].m_logLevel);
    update_log_level(tmp);
  }
  m_clients.unlock();
  DBUG_VOID_RETURN;
}

void
Ndb_mgmd_event_service::add_listener(const Event_listener& client)
{
  DBUG_ENTER("Ndb_mgmd_event_service::add_listener");
  DBUG_PRINT("enter",("client.m_socket: " MY_SOCKET_FORMAT,
                      MY_SOCKET_FORMAT_VALUE(client.m_socket)));

  check_listeners();

  m_clients.push_back(client);
  update_max_log_level(client.m_logLevel);

  DBUG_VOID_RETURN;
}

void
Ndb_mgmd_event_service::stop_sessions(){
  m_clients.lock();
  for(int i = m_clients.size() - 1; i >= 0; i--){
    if(my_socket_valid(m_clients[i].m_socket))
    {
      NDB_CLOSE_SOCKET(m_clients[i].m_socket);
      m_clients.erase(i);
    }
  }
  m_clients.unlock();
}

void
MgmApiSession::setParameter(Parser_t::Context &,
			    Properties const &args)
{
  Uint32 node, param;
  BaseString value;
  args.get("node", &node);
  args.get("parameter", &param);
  args.get("value", value);

  BaseString result;
  int ret = m_mgmsrv.setDbParameter(node,
				    param,
				    value.c_str(),
				    result);

  m_output->println("set parameter reply");
  m_output->println("message: %s", result.c_str());
  m_output->println("result: %d", ret);
  m_output->println("%s", "");
}

void
MgmApiSession::setConnectionParameter(Parser_t::Context &ctx,
				      Properties const &args)
{
  Uint32 node1, node2, param, value;
  args.get("node1", &node1);
  args.get("node2", &node2);
  args.get("param", &param);
  args.get("value", &value);

  BaseString result;
  int ret = m_mgmsrv.setConnectionDbParameter(node1,
					      node2,
					      param,
					      value,
					      result);

  m_output->println("set connection parameter reply");
  m_output->println("message: %s", result.c_str());
  m_output->println("result: %s", (ret>0)?"Ok":"Failed");
  m_output->println("%s", "");
}

void
MgmApiSession::getConnectionParameter(Parser_t::Context &ctx,
				      Properties const &args)
{
  Uint32 node1, node2, param;
  int value = 0;

  args.get("node1", &node1);
  args.get("node2", &node2);
  args.get("param", &param);

  BaseString result;
  int ret = m_mgmsrv.getConnectionDbParameter(node1,
					      node2,
					      param,
					      &value,
					      result);

  m_output->println("get connection parameter reply");
  m_output->println("value: %d", value);
  m_output->println("result: %s", (ret>0)?"Ok":result.c_str());
  m_output->println("%s", "");
}

void
MgmApiSession::listen_event(Parser<MgmApiSession>::Context & ctx,
			    Properties const & args) {
  Uint32 parsable= 0;
  BaseString node, param, value;
  args.get("node", node);
  args.get("filter", param);
  args.get("parsable", &parsable);

  int result = 0;
  BaseString msg;

  Ndb_mgmd_event_service::Event_listener le;
  le.m_parsable = parsable;
  le.m_socket = m_socket;

  Vector<BaseString> list;
  param.trim();
  param.split(list, " ,");
  for(unsigned i = 0; i<list.size(); i++){
    Vector<BaseString> spec;
    list[i].trim();
    list[i].split(spec, "=:");
    if(spec.size() != 2){
      msg.appfmt("Invalid filter specification: >%s< >%s< %d", 
		 param.c_str(), list[i].c_str(), spec.size());
      result = -1;
      goto done;
    }

    spec[0].trim().ndb_toupper();
    int category = ndb_mgm_match_event_category(spec[0].c_str());
    if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
      category = atoi(spec[0].c_str());
      if(category < NDB_MGM_MIN_EVENT_CATEGORY ||
	 category > NDB_MGM_MAX_EVENT_CATEGORY){
	msg.appfmt("Unknown category: >%s<", spec[0].c_str());
	result = -1;
	goto done;
      }
    }
    
    int level = atoi(spec[1].c_str());
    if(level < 0 || level > NDB_MGM_MAX_LOGLEVEL){
      msg.appfmt("Invalid level: >%s<", spec[1].c_str());
      result = -1;
      goto done;
    }
    category -= CFG_MIN_LOGLEVEL;
    le.m_logLevel.setLogLevel((LogLevel::EventCategory)category, level);
  }
  
  if(list.size() == 0){
    msg.appfmt("Empty filter specification");
    result = -1;
    goto done;
  }

done:
  m_output->println("listen event");
  m_output->println("result: %d", result);
  if(result != 0)
    m_output->println("msg: %s", msg.c_str());
  m_output->println("%s", "");

  /*
    Flush output from command before adding the new event listener.
    This makes sure that the client receives the reply before the
    loglevel thread starts to check the connection by sending <PING>'s.
    The client is expecting <PING>'s but not until after the reply has been
    received.
  */
  NdbMutex_Unlock(m_mutex);   
  m_output->flush();
  NdbMutex_Lock(m_mutex);   

  if(result==0)
  {
    m_mgmsrv.m_event_listner.add_listener(le);
    m_stop = true;
    my_socket_invalidate(&m_socket);
  }
}


void
MgmApiSession::purge_stale_sessions(Parser_t::Context &ctx,
				    const class Properties &args)
{
  m_mgmsrv.get_socket_server()->checkSessions();

  m_output->println("purge stale sessions reply");
  m_output->println("result: Ok");
  m_output->println("%s", "");
}

void
MgmApiSession::check_connection(Parser_t::Context &ctx,
				const class Properties &args)
{
  SLEEP_ERROR_INSERTED(1);
  m_output->println("check connection reply");
  SLEEP_ERROR_INSERTED(2);
  m_output->println("result: Ok");
  SLEEP_ERROR_INSERTED(3);
  m_output->println("%s", "");
}

void
MgmApiSession::transporter_connect(Parser_t::Context &ctx,
				   Properties const &args)
{
  bool close_with_reset = true;
  BaseString errormsg;
  if (!m_mgmsrv.transporter_connect(m_socket, errormsg, close_with_reset))
  {
    // Connection not allowed or failed
    g_eventLogger->warning("Failed to convert connection "
                           "from '%s' to transporter: %s",
                           name(),
                           errormsg.c_str());
    // Close the socket to indicate failure to client
    ndb_socket_close(m_socket, close_with_reset);
    my_socket_invalidate(&m_socket); // Already closed
  }
  else
  {
    /*
      Conversion to transporter suceeded
      Stop this session thread and release resources
      but don't close the socket, it's been taken over
      by the transporter
    */
    my_socket_invalidate(&m_socket);   // so nobody closes it
  }

  m_stop= true; // Stop the session
}

void
MgmApiSession::get_mgmd_nodeid(Parser_t::Context &ctx,
			       Properties const &args)
{
  m_output->println("get mgmd nodeid reply");
  m_output->println("nodeid:%u",m_mgmsrv.getOwnNodeId());
  SLEEP_ERROR_INSERTED(1);

  m_output->println("%s", "");
}

void
MgmApiSession::report_event(Parser_t::Context &ctx,
			    Properties const &args)
{
  Uint32 length;
  const char *data_string;
  Uint32 data[25];

  args.get("length", &length);
  args.get("data", &data_string);

  BaseString tmp(data_string);
  Vector<BaseString> item;
  tmp.split(item, " ");
  for (int i = 0; (Uint32) i < length ; i++)
  {
    sscanf(item[i].c_str(), "%u", data+i);
  }

  m_mgmsrv.eventReport(data, length);
  m_output->println("report event reply");
  m_output->println("result: ok");
  m_output->println("%s", "");
}

void
MgmApiSession::create_nodegroup(Parser_t::Context &ctx,
                                Properties const &args)
{
  int res = 0;
  BaseString nodestr;
  BaseString retval;
  int ng = -1;
  Vector<int> nodes;
  BaseString result("Ok");

  args.get("nodes", nodestr);
  Vector<BaseString> list;
  nodestr.split(list, " ");
  for (Uint32 i = 0; i < list.size() ; i++)
  {
    int res;
    int node;
    if ((res = sscanf(list[i].c_str(), "%u", &node)) != 1)
    {
      nodes.clear();
      result = "FAIL: Invalid format for nodes";
      break;
    }
    nodes.push_back(node);
  }
  
  res = m_mgmsrv.createNodegroup(nodes.getBase(), nodes.size(), &ng);

  m_output->println("create nodegroup reply");
  m_output->println("ng: %d", ng);
  if (res)
  {
    m_output->println("error_code: %d", res);
    m_output->println("result: %d-%s", res, get_error_text(res));
  }
  else
  {
    m_output->println("result: Ok");
  }
  m_output->println("%s", "");
}

void
MgmApiSession::drop_nodegroup(Parser_t::Context &ctx,
                              Properties const &args)
{
  BaseString result("Ok");

  unsigned ng;
  args.get("ng", &ng);

  int res;
  if((res = m_mgmsrv.dropNodegroup(ng)) != 0)
  {
    result.assfmt("error: %d", res);
  }

//end:
  m_output->println("drop nodegroup reply");
  m_output->println("result: %s", result.c_str());
  m_output->println("%s", "");
}

void
MgmApiSession::list_session(SocketServer::Session *_s, void *data)
{
  MgmApiSession *s= (MgmApiSession *)_s;
  MgmApiSession *lister= (MgmApiSession*) data;

  if(s!=lister)
    NdbMutex_Lock(s->m_mutex);

  Uint64 id= s->m_session_id;
  lister->m_output->println("session: %llu",id);
  lister->m_output->println("session.%llu.m_stopSelf: %d",id,s->m_stopSelf);
  lister->m_output->println("session.%llu.m_stop: %d",id,s->m_stop);
  if(s->m_ctx)
  {
    int l= (int)strlen(s->m_ctx->m_tokenBuffer);
    char *buf= (char*) malloc(2*l+1);
    char *b= buf;
    for(int i=0; i<l;i++)
      if(s->m_ctx->m_tokenBuffer[i]=='\n')
      {
        *b++='\\';
        *b++='n';
      }
      else
      {
        *b++= s->m_ctx->m_tokenBuffer[i];
      }
    *b= '\0';

    lister->m_output->println("session.%llu.parser.buffer.len: %u",id,l);
    lister->m_output->println("session.%llu.parser.buffer: %s",id,buf);
    lister->m_output->println("session.%llu.parser.status: %d",id,s->m_ctx->m_status);

    free(buf);
  }

  if(s!=lister)
    NdbMutex_Unlock(s->m_mutex);
}

void
MgmApiSession::listSessions(Parser_t::Context &ctx,
                            Properties const &args) {
  m_mgmsrv.get_socket_server()->foreachSession(list_session,(void*)this);

  m_output->println("%s", "");
}

void
MgmApiSession::getSessionId(Parser_t::Context &ctx,
                                 Properties const &args) {
  m_output->println("get session id reply");
  m_output->println("id: %llu",m_session_id);
  m_output->println("%s", "");
}

struct get_session_param {
  MgmApiSession *l;
  Uint64 id;
  int found;
};

void
MgmApiSession::get_session(SocketServer::Session *_s, void *data)
{
  struct get_session_param *p= (struct get_session_param*)data;
  MgmApiSession *s= (MgmApiSession *)_s;

  if(s!=p->l)
    NdbMutex_Lock(s->m_mutex);

  if(p->id != s->m_session_id)
  {
    if(s!=p->l)
      NdbMutex_Unlock(s->m_mutex);
    return;
  }

  p->found= true;
  p->l->m_output->println("id: %llu",s->m_session_id);
  p->l->m_output->println("m_stopSelf: %d",s->m_stopSelf);
  p->l->m_output->println("m_stop: %d",s->m_stop);
  if(s->m_ctx)
  {
    int l= (int)strlen(s->m_ctx->m_tokenBuffer);
    p->l->m_output->println("parser_buffer_len: %u",l);
    p->l->m_output->println("parser_status: %d",s->m_ctx->m_status);
  }

  if(s!=p->l)
    NdbMutex_Unlock(s->m_mutex);
}

void
MgmApiSession::getSession(Parser_t::Context &ctx,
                          Properties const &args) {
  Uint64 id;
  struct get_session_param p;

  args.get("id", &id);

  p.l= this;
  p.id= id;
  p.found= false;

  m_output->println("get session reply");
  m_mgmsrv.get_socket_server()->foreachSession(get_session,(void*)&p);

  if(p.found==false)
    m_output->println("id: 0");

  m_output->println("%s", "");
}


static bool
clear_dynamic_ports_from_config(Config* config)
{
  ConfigIter iter(config, CFG_SECTION_CONNECTION);

  for(;iter.valid();iter.next()) {
    Uint32 n1, n2;
    if (iter.get(CFG_CONNECTION_NODE_1, &n1) != 0 &&
        iter.get(CFG_CONNECTION_NODE_2, &n2) != 0)
      return false;

    Uint32 port_value;
    if (iter.get(CFG_CONNECTION_SERVER_PORT, &port_value) != 0)
      return false;

    int port = (int)port_value;
    if (port < 0)
    {
      // Found a dynamic port with value in config, clear it by updating
      // the already existing value
      Uint32 zero_port = 0;
      ConfigValues::Iterator i2(config->m_configValues->m_config,
                                iter.m_config);
      if (!i2.set(CFG_CONNECTION_SERVER_PORT, zero_port))
        return false;
    }
  }
  return true;
}


void MgmApiSession::setConfig(Parser_t::Context &ctx, Properties const &args)
{
  BaseString result("Ok");
  Uint32 len64 = 0;

  {
    const char* buf;
    args.get("Content-Type", &buf);
    if(strcmp(buf, "ndbconfig/octet-stream")) {
      result.assfmt("Unhandled content type '%s'", buf);
      goto done;
    }

    args.get("Content-Transfer-Encoding", &buf);
    if(strcmp(buf, "base64")) {
      result.assfmt("Unhandled content encoding '%s'", buf);
      goto done;
    }
  }

  args.get("Content-Length", &len64);
  if(len64 ==0 || len64 > (1024*1024)) {
    result.assfmt("Illegal config length size %d", len64);
    goto done;
  }
  len64 += 1; // Trailing \n

  {
    char* buf64 = new char[len64];
    int r = 0;
    size_t start = 0;
    do {
      if((r= read_socket(m_socket,
                         SOCKET_TIMEOUT,
                         &buf64[start],
                         (int)(len64-start))) < 1)
      {
        delete[] buf64;
        result.assfmt("read_socket failed, errno: %d", errno);
        goto done;
      }
      start += r;
    } while(start < len64);

    char* decoded = new char[base64_needed_decoded_length((size_t)len64 - 1)];
    int decoded_len= ndb_base64_decode(buf64, len64-1, decoded, NULL);
    delete[] buf64;

    if (decoded_len == -1)
    {
      result.assfmt("Failed to unpack config");
      delete[] decoded;
      goto done;
    }

    ConfigValuesFactory cvf;
    if(!cvf.unpack(decoded, decoded_len))
    {
      delete[] decoded;
      result.assfmt("Failed to unpack config");
      goto done;
    }
    delete[] decoded;

    Config new_config(cvf.getConfigValues());

    // Remove any dynamic ports from the new config
    if (!clear_dynamic_ports_from_config(&new_config))
    {
      result.assfmt("INTERNAL ERROR: Failed to clear dynamic "
                    "ports from config");
      goto done;
    }

    (void)m_mgmsrv.change_config(new_config, result);
  }

done:

  m_output->println("set config reply");
  m_output->println("result: %s", result.c_str());
  m_output->println("%s", "");
}


void MgmApiSession::showConfig(Parser_t::Context &ctx, Properties const &args)
{
  const char* section = NULL;
  const char* name = NULL;
  Uint32 nodeid = 0;

  args.get("Section", &section);
  args.get("NodeId", &nodeid);
  args.get("Name", &name);

  NdbOut socket_out(*m_output, false /* turn off autoflush */);
  m_output->println("show config reply");
  m_mgmsrv.print_config(section, nodeid, name,
                        socket_out);
  m_output->println("%s", "");
}


void
MgmApiSession::reloadConfig(Parser_t::Context &,
                            const class Properties &args)
{
  const char* config_filename= NULL;
  Uint32 mycnf = 0;

  args.get("config_filename", &config_filename);
  args.get("mycnf", &mycnf);

  g_eventLogger->debug("config_filename: %s, mycnf: %s",
                       str_null(config_filename),
                       yes_no(mycnf));

  m_output->println("reload config reply");

  BaseString msg;
  if (!m_mgmsrv.reload_config(config_filename, (mycnf != 0), msg))
    m_output->println("result: %s", msg.c_str());
  else
    m_output->println("result: Ok");

  m_output->println("%s", "");
}


void
MgmApiSession::show_variables(Parser_t::Context &,
                              const class Properties &args)
{
  m_output->println("show variables reply");
  NdbOut socket_out(*m_output, false /* turn off autoflush */);
  m_mgmsrv.show_variables(socket_out);
  m_output->println("%s", "");
}


static bool
valid_nodes(const NdbNodeBitmask& nodes, unsigned max_nodeid)
{
  unsigned nodeid = 0;
  while((nodeid = nodes.find(nodeid)) != NdbNodeBitmask::NotFound)
  {
    if (nodeid == 0 || nodeid > max_nodeid)
      return false;
    nodeid++;
  }
  return true;
}


#include <signaldata/DumpStateOrd.hpp>
#include "../common/util/parse_mask.hpp"

static const
struct dump_request {
  Ndb_logevent_type type;
  DumpStateOrd::DumpStateType dump_type;
  // Number of reports to wait for from each node
  Uint32 reports_per_node;
} dump_requests [] =
{
  { NDB_LE_BackupStatus,
    DumpStateOrd::BackupStatus,
    1 },

  { NDB_LE_MemoryUsage,
    DumpStateOrd::DumpPageMemory,
    2},

  { NDB_LE_SavedEvent,
    DumpStateOrd::DumpEventLog,
    0},

  { NDB_LE_ILLEGAL_TYPE, (DumpStateOrd::DumpStateType)0, 0 }
};

void
MgmApiSession::dump_events(Parser_t::Context &,
                           const class Properties &args)
{
  m_output->println("dump events reply");

  // Check "type" argument
  Uint32 type;
  args.get("type", &type);

  const dump_request* request = dump_requests;

  for (; request->type != NDB_LE_ILLEGAL_TYPE; request++)
  {
    if (request->type == (Ndb_logevent_type)type)
      break;
  }

  if (request->type == NDB_LE_ILLEGAL_TYPE)
  {
    m_output->println("result: ndb_logevent_type %u not supported", type);
    m_output->println("%s", "");
    return;
  }

  // Check "nodes" argument
  NdbNodeBitmask nodes;
  const char* nodes_str = NULL;
  args.get("nodes", &nodes_str);
  if (nodes_str)
  {
    int res = parse_mask(nodes_str, nodes);
    if (res < 0 || !valid_nodes(nodes, MAX_NDB_NODES-1))
    {
      m_output->println("result: invalid nodes: '%s'", nodes_str);
      m_output->println("%s", "");
      return;
    }
  }

  // Request the events
  Vector<SimpleSignal> events;
  if (!m_mgmsrv.request_events(nodes,
                               request->reports_per_node,
                               request->dump_type,
                               events))
  {
    m_output->println("result: failed to dump events");
    m_output->println("%s", "");
    return;
  }

  // Return result
  m_output->println("result: Ok");
  m_output->println("events: %u", events.size());
  m_output->println("%s", ""); // Empty line between header and first event
  for (unsigned i = 0; i < events.size(); i++)
  {
    const EventReport * const event =
      (const EventReport*)events[i].getDataPtrSend();
    const NodeId nodeid = refToNode(events[i].header.theSendersBlockRef);

    // Check correct EVENT_REP type returned
    assert(event->getEventType() == request->type);

    BaseString str;
    char pretty_text[512];
    Uint32 tmpData[256];
    const Uint32 * dataPtr = events[i].getDataPtr();
    Uint32 dataLen = events[i].getLength();
    if (events[i].header.m_noOfSections == 1)
    {
      if (dataLen + events[i].ptr[0].sz > NDB_ARRAY_SIZE(tmpData))
      {
        events[i].ptr[0].sz = NDB_ARRAY_SIZE(tmpData) - dataLen;
      }
      memcpy(tmpData, dataPtr, 4 * dataLen);
      memcpy(tmpData + dataLen, events[i].ptr[0].p, 4*events[i].ptr[0].sz);
      dataPtr = tmpData;
      dataLen += events[i].ptr[0].sz;
    }
    logevent2str(str,
                 event->getEventType(),
                 dataPtr,
                 dataLen,
                 nodeid, 0,
                 pretty_text, sizeof(pretty_text));

    m_output->println("%s", str.c_str());
  }
}


/*
  Read and discard the "bulk data" until
   - nothing more to read
   - empty line found

  When error detected, the command part is already read, but the bulk data
  is still pending on the socket, it need to be consumed(read and discarded).

  Example:
  set ports
  nodeid: 1 // << Error detected here
  num_ports: 2
  <new_line> // Parser always reads to first new line
  bulk line 1
  bulk line 2
  <new line> // discard_bulk_data() discards until here
*/

static
void
discard_bulk_data(InputStream* in)
{
  char buf[256];
  while (true)
  {
    if (in->gets(buf, sizeof(buf)) == 0)
    {
      // Nothing more to read
      break;
    }

    if (buf[0] == 0)
    {
      // Got eof
      break;
    }

    if (buf[0] == '\n')
    {
      // Found empty line
      break;
    }
  }
  return;
}


static
bool
read_dynamic_ports(InputStream* in,
                   Uint32 num_ports,
                   MgmtSrvr::DynPortSpec ports[],
                   Uint32& ports_read,
                   BaseString& msg)
{
  char buf[256];
  Uint32 counter = 0;
  while (counter < num_ports)
  {
    if (in->gets(buf, sizeof(buf)) == 0)
    {
      msg.assign("Read of ports failed");
      return false;
    }

    if (buf[0] == 0)
    {
      msg.assign("Got eof instead of port");
      return false;
    }

    if (buf[0] == '\n')
    {
      // Found empty line, list of ports ended too early
      msg.assign("Failed to parse line, expected name=value pair");
      return false;
    }

    int node, port;
    if (sscanf(buf, "%d=%d", &node, &port) != 2)
    {
      msg.assign("Failed to parse line, expected name=value pair");
      discard_bulk_data(in);
      return false;
    }

    ports[counter].port = port;
    ports[counter].node = node;
    counter++;
  }

  // Read ending empty line
  if (in->gets(buf, sizeof(buf)) == 0)
  {
    msg.assign("Read of ending empty line failed");
    return false;
  }

  if (buf[0] == 0)
  {
    msg.assign("Got eof instead of ending new line");
    return false;
  }

  if (buf[0] != '\n')
  {
    msg.assign("Failed to parse line, expected empty line");
    discard_bulk_data(in);
    return false;
  }

  ports_read= counter;
  return true;
}


void
MgmApiSession::set_ports(Parser_t::Context &,
                         Properties const &args)
{  
  m_output->println("set ports reply");

  // Check node argument
  Uint32 node;
  args.get("node", &node);
  if (node == 0 || node >= MAX_NODES)
  {
    m_output->println("result: Illegal value for argument node: %u", node);
    m_output->println("%s", "");
    discard_bulk_data(m_input);
    return;
  }

  Uint32 num_ports;
  args.get("num_ports", &num_ports);
  if (num_ports == 0 || num_ports >= MAX_NODES)
  {
    m_output->println("result: Illegal value for argument num_ports: %u",
                      num_ports);
    m_output->println("%s", "");
    discard_bulk_data(m_input);
    return;
  }

  // Read the name value pair list of ports to set from bulk data
  MgmtSrvr::DynPortSpec ports[MAX_NODES];
  {
    Uint32 ports_read;
    BaseString msg;
    if (!read_dynamic_ports(m_input, num_ports, ports, ports_read, msg))
    {
      m_output->println("result: %s", msg.c_str());
      m_output->println("%s", "");
      return;
    }

    if (ports_read != num_ports)
    {
      m_output->println("result: Only read %d ports of expected %d",
                        ports_read, num_ports);
      m_output->println("%s", "");
      return;
    }
  }
  // All bulk data consumed!

  // Set all the received ports
  BaseString msg;
  if (!m_mgmsrv.setDynamicPorts(node, ports, num_ports, msg))
  {
    m_output->println("result: %s", msg.c_str());
    m_output->println("%s", "");
    return;
  }

  m_output->println("result: Ok");
  m_output->println("%s", "");
  return;
}

template class MutexVector<int>;
template class Vector<ParserRow<MgmApiSession> const*>;
template class Vector<NDB_SOCKET_TYPE>;
template class Vector<SimpleSignal>;
