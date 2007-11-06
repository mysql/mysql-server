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
#include <ctype.h>

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

#include <base64.h>
#include <ndberror.h>

extern bool g_StopServer;
extern bool g_RestartServer;
extern EventLogger g_eventLogger;

static const unsigned int MAX_READ_TIMEOUT = 1000 ;
static const unsigned int MAX_WRITE_TIMEOUT = 100 ;

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
   ParserRow<MgmApiSession>::Arg, \
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

  MGM_CMD("restart all", &MgmApiSession::restartAll, ""),
    MGM_ARG("initialstart", Int, Optional, "Initial start"),
    MGM_ARG("nostart", Int, Optional, "No start"),
    MGM_ARG("abort", Int, Optional, "Abort"),

  MGM_CMD("insert error", &MgmApiSession::insertError, ""),
    MGM_ARG("node", Int, Mandatory, "Node to receive error"),
    MGM_ARG("error", Int, Mandatory, "Errorcode to insert"),

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

  MGM_CMD("abort backup", &MgmApiSession::abortBackup, ""),
    MGM_ARG("id", Int, Mandatory, "Backup id"),

  MGM_CMD("stop", &MgmApiSession::stop_v1, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("abort", Int, Mandatory, "Node"),

  MGM_CMD("stop v2", &MgmApiSession::stop_v2, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("abort", Int, Mandatory, "Node"),

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
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("parameter", String, Mandatory, "Parameter"),
    MGM_ARG("value", String, Mandatory, "Value"),

  MGM_CMD("set connection parameter",
	  &MgmApiSession::setConnectionParameter, ""),
    MGM_ARG("node1", String, Mandatory, "Node1 ID"),
    MGM_ARG("node2", String, Mandatory, "Node2 ID"),
    MGM_ARG("param", String, Mandatory, "Parameter"),
    MGM_ARG("value", String, Mandatory, "Value"),

  MGM_CMD("get connection parameter",
	  &MgmApiSession::getConnectionParameter, ""),
    MGM_ARG("node1", String, Mandatory, "Node1 ID"),
    MGM_ARG("node2", String, Mandatory, "Node2 ID"),
    MGM_ARG("param", String, Mandatory, "Parameter"),

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

  MGM_END()
};

struct PurgeStruct
{
  NodeBitmask free_nodes;/* free nodes as reported
			  * by ndbd in apiRegReqConf
			  */
  BaseString *str;
  NDB_TICKS tick;
};

#define ERROR_INSERTED(x) (g_errorInsert == x || m_errorInsert == x)

#define SLEEP_ERROR_INSERTED(x) if(ERROR_INSERTED(x)){NdbSleep_SecSleep(10);}

MgmApiSession::MgmApiSession(class MgmtSrvr & mgm, NDB_SOCKET_TYPE sock, Uint64 session_id)
  : SocketServer::Session(sock), m_mgmsrv(mgm)
{
  DBUG_ENTER("MgmApiSession::MgmApiSession");
  m_input = new SocketInputStream(sock, 30000);
  m_output = new SocketOutputStream(sock, 30000);
  m_parser = new Parser_t(commands, *m_input, true, true, true);
  m_allocated_resources= new MgmtSrvr::Allocated_resources(m_mgmsrv);
  m_stopSelf= 0;
  m_ctx= NULL;
  m_session_id= session_id;
  m_mutex= NdbMutex_Create();
  m_errorInsert= 0;
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
  if (m_allocated_resources)
    delete m_allocated_resources;
  if(m_socket != NDB_INVALID_SOCKET)
  {
    NDB_CLOSE_SOCKET(m_socket);
    m_socket= NDB_INVALID_SOCKET;
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

  Parser_t::Context ctx;
  ctx.m_mutex= m_mutex;
  m_ctx= &ctx;
  bool stop= false;
  while(!stop) {
    NdbMutex_Lock(m_mutex);

    m_input->reset_timeout();
    m_output->reset_timeout();

    m_parser->run(ctx, *this);

    if(ctx.m_currentToken == 0)
    {
      NdbMutex_Unlock(m_mutex);
      break;
    }

    switch(ctx.m_status) {
    case Parser_t::UnknownCommand:
      break;
    default:
      break;
    }

    stop= m_stop;
    NdbMutex_Unlock(m_mutex);
  };

  NdbMutex_Lock(m_mutex);
  m_ctx= NULL;
  if(m_socket != NDB_INVALID_SOCKET)
  {
    NDB_CLOSE_SOCKET(m_socket);
    m_socket= NDB_INVALID_SOCKET;
  }
  NdbMutex_Unlock(m_mutex);
  DBUG_VOID_RETURN;
}

static Properties *
backward(const char * base, const Properties* reply){
  Properties * ret = new Properties();
  Properties::Iterator it(reply);
  for(const char * name = it.first(); name != 0; name=it.next()){
    PropertiesType type;
    reply->getTypeOf(name, &type);
    switch(type){
    case PropertiesType_Uint32:{
      Uint32 val;
      reply->get(name, &val);
      ret->put(name, val);
    }
      break;
    case PropertiesType_char:
      {
	const char * val;
	reply->get(name, &val);
	ret->put(name, val);
	if(!strcmp(name, "Type") && !strcmp(val, "DB")){
	  ret->put("NoOfDiskBufferPages", (unsigned)0);
	  ret->put("NoOfDiskFiles", (unsigned)0);
	  ret->put("NoOfDiskClusters", (unsigned)0);
	  ret->put("NoOfFreeDiskClusters", (unsigned)0);
	  ret->put("NoOfDiskClustersPerDiskFile", (unsigned)0);
	  ret->put("NoOfConcurrentCheckpointsDuringRestart", (unsigned)1);
	  ret->put("NoOfConcurrentCheckpointsAfterRestart", (unsigned)1);
	  ret->put("NoOfConcurrentProcessesHandleTakeover", (unsigned)1);
	}
      }
      break;
    case PropertiesType_Properties:
      {
	const Properties * recurse;
	reply->get(name, &recurse);
	Properties * val = backward(name, recurse);
	ret->put(name, val);
      }
      break;
    case PropertiesType_Uint64:
      break;
    }
  }
  return ret;
}

void
MgmApiSession::get_nodeid(Parser_t::Context &,
			  const class Properties &args)
{
  const char *cmd= "get nodeid reply";
  Uint32 version, nodeid= 0, nodetype= 0xff;
  Uint32 timeout= 20;  // default seconds timeout
  const char * transporter;
  const char * user;
  const char * password;
  const char * public_key;
  const char * endian= NULL;
  const char * name= NULL;
  Uint32 log_event= 1;
  bool log_event_version;
  union { long l; char c[sizeof(long)]; } endian_check;

  args.get("version", &version);
  args.get("nodetype", &nodetype);
  args.get("transporter", &transporter);
  args.get("nodeid", &nodeid);
  args.get("user", &user);
  args.get("password", &password);
  args.get("public key", &public_key);
  args.get("endian", &endian);
  args.get("name", &name);
  args.get("timeout", &timeout);
  /* for backwards compatability keep track if client uses new protocol */
  log_event_version= args.get("log_event", &log_event);

  endian_check.l = 1;
  if(endian 
     && strcmp(endian,(endian_check.c[sizeof(long)-1])?"big":"little")!=0) {
    m_output->println(cmd);
    m_output->println("result: Node does not have the same endianness as the management server.");
    m_output->println("");
    return;
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
    m_output->println(cmd);
    m_output->println("result: unknown nodetype %d", nodetype);
    m_output->println("");
    return;
  }

  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  int r = getpeername(m_socket, (struct sockaddr*)&addr, &addrlen);
  if (r != 0 ) {
    m_output->println(cmd);
    m_output->println("result: getpeername(%d) failed, err= %d", m_socket, r);
    m_output->println("");
    return;
  }

  NodeId tmp= nodeid;
  if(tmp == 0 || !m_allocated_resources->is_reserved(tmp)){
    BaseString error_string;
    int error_code;
    NDB_TICKS tick= 0;
    /* only report error on second attempt as not to clog the cluster log */
    while (!m_mgmsrv.alloc_node_id(&tmp, (enum ndb_mgm_node_type)nodetype, 
                                   (struct sockaddr*)&addr, &addrlen,
                                   error_code, error_string,
                                   tick == 0 ? 0 : log_event))
    {
      /* NDB_MGM_ALLOCID_CONFIG_MISMATCH is a non retriable error */
      if (tick == 0 && error_code != NDB_MGM_ALLOCID_CONFIG_MISMATCH)
      {
        // attempt to free any timed out reservations
        tick= NdbTick_CurrentMillisecond();
        struct PurgeStruct ps;
        m_mgmsrv.get_connected_nodes(ps.free_nodes);
        // invert connected_nodes to get free nodes
        ps.free_nodes.bitXORC(NodeBitmask());
        ps.str= 0;
        ps.tick= tick;
        m_mgmsrv.get_socket_server()->
          foreachSession(stop_session_if_timed_out,&ps);
	m_mgmsrv.get_socket_server()->checkSessions();
        error_string = "";
        continue;
      }
      const char *alias;
      const char *str;
      alias= ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)
						nodetype, &str);
      m_output->println(cmd);
      m_output->println("result: %s", error_string.c_str());
      /* only use error_code protocol if client knows about it */
      if (log_event_version)
        m_output->println("error_code: %d", error_code);
      m_output->println("");
      return;
    }
  }    
  
#if 0
  if (!compatible){
    m_output->println(cmd);
    m_output->println("result: incompatible version mgmt 0x%x and node 0x%x",
		      NDB_VERSION, version);
    m_output->println("");
    return;
  }
#endif
  
  m_output->println(cmd);
  m_output->println("nodeid: %u", tmp);
  m_output->println("result: Ok");
  m_output->println("");
  m_allocated_resources->reserve_node(tmp, timeout*1000);
  
  if (name)
    g_eventLogger.info("Node %d: %s", tmp, name);

  return;
}

void
MgmApiSession::getConfig(Parser_t::Context &,
                         const class Properties &args)
{
  Uint32 version, node = 0;

  args.get("version", &version);
  args.get("node", &node);

  const Config *conf = m_mgmsrv.getConfig();
  if(conf == NULL) {
    m_output->println("get config reply");
    m_output->println("result: Could not fetch configuration");
    m_output->println("");
    return;
  }

  if(node != 0){
    bool compatible;
    switch (m_mgmsrv.getNodeType(node)) {
    case NDB_MGM_NODE_TYPE_NDB:
      compatible = ndbCompatible_mgmt_ndb(NDB_VERSION, version);
      break;
    case NDB_MGM_NODE_TYPE_API:
    case NDB_MGM_NODE_TYPE_MGM:
      compatible = ndbCompatible_mgmt_api(NDB_VERSION, version);
      break;
    default:
      m_output->println("get config");
      m_output->println("result: unrecognignized node type");
      m_output->println("");
      return;
    }
    
    if (!compatible){
      m_output->println("get config");
      m_output->println("result: incompatible version mgmt 0x%x and node 0x%x",
			NDB_VERSION, version);
      m_output->println("");
      return;
    }
  }  
  
  NdbMutex_Lock(m_mgmsrv.m_configMutex);
  const ConfigValues * cfg = &conf->m_configValues->m_config;
  
  UtilBuffer src;
  cfg->pack(src);
  NdbMutex_Unlock(m_mgmsrv.m_configMutex);
  
  char *tmp_str = (char *) malloc(base64_needed_encoded_length(src.length()));
  (void) base64_encode(src.get_data(), src.length(), tmp_str);

  SLEEP_ERROR_INSERTED(1);

  m_output->println("get config reply");
  m_output->println("result: Ok");
  m_output->println("Content-Length: %d", strlen(tmp_str));
  m_output->println("Content-Type: ndbconfig/octet-stream");
  SLEEP_ERROR_INSERTED(2);
  m_output->println("Content-Transfer-Encoding: base64");
  m_output->println("");
  if(ERROR_INSERTED(3))
  {
    int l= strlen(tmp_str);
    tmp_str[l/2]='\0';
    m_output->println(tmp_str);
    NdbSleep_SecSleep(10);
  }
  m_output->println(tmp_str);

  free(tmp_str);
  return;
}

void
MgmApiSession::insertError(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 node = 0, error = 0;
  int result= 0;

  args.get("node", &node);
  args.get("error", &error);

  if(node==m_mgmsrv.getOwnNodeId()
     && error < MGM_ERROR_MAX_INJECT_SESSION_ONLY)
  {
    m_errorInsert= error;
    if(error==0)
      g_errorInsert= error;
  }
  else
  {
    result= m_mgmsrv.insertError(node, error);
  }

  m_output->println("insert error reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("");
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
  m_output->println("");
}

void
MgmApiSession::getVersion(Parser<MgmApiSession>::Context &,
			  Properties const &) {
  m_output->println("version");
  m_output->println("id: %d", NDB_VERSION);
  m_output->println("major: %d", getMajor(NDB_VERSION));
  m_output->println("minor: %d", getMinor(NDB_VERSION));
  m_output->println("string: %s", NDB_VERSION_STRING);
  m_output->println("");
}

void
MgmApiSession::startBackup(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  DBUG_ENTER("MgmApiSession::startBackup");
  unsigned backupId;
  Uint32 completed= 2;
  int result;

  args.get("completed", &completed);

  result = m_mgmsrv.startBackup(backupId, completed);

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
  m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
}


void
MgmApiSession::bye(Parser<MgmApiSession>::Context &,
                   Properties const &) {
  m_stop = true;
}

void
MgmApiSession::endSession(Parser<MgmApiSession>::Context &,
                          Properties const &) {
  if(m_allocated_resources)
    delete m_allocated_resources;

  m_allocated_resources= new MgmtSrvr::Allocated_resources(m_mgmsrv);

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
			  "backup" };

  int loglevel_count = (CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1) ;
  LogLevel::EventCategory category;

  m_output->println("get cluster loglevel");
  for(int i = 0; i < loglevel_count; i++) {
    category = (LogLevel::EventCategory) i;
    m_output->println("%s: %d", names[i], m_mgmsrv.m_event_listner[0].m_logLevel.getLogLevel(category));
  }
  m_output->println("");
}

void
MgmApiSession::setClusterLogLevel(Parser<MgmApiSession>::Context &,
				  Properties const &args) {
  const char *reply= "set cluster loglevel reply";
  Uint32 node, level, cat;
  BaseString errorString;
  DBUG_ENTER("MgmApiSession::setClusterLogLevel");
  args.get("node", &node);
  args.get("category", &cat);
  args.get("level", &level);

  DBUG_PRINT("enter",("node=%d, category=%d, level=%d", node, cat, level));

  if(level > NDB_MGM_MAX_LOGLEVEL) {
    m_output->println(reply);
    m_output->println("result: Invalid loglevel %d", level);
    m_output->println("");
    DBUG_VOID_RETURN;
  }

  LogLevel::EventCategory category= 
    (LogLevel::EventCategory)(cat-(int)CFG_MIN_LOGLEVEL);

  m_mgmsrv.m_event_listner.lock();
  if (m_mgmsrv.m_event_listner[0].m_logLevel.setLogLevel(category,level))
  {
    m_output->println(reply);
    m_output->println("result: Invalid category %d", category);
    m_output->println("");
    m_mgmsrv.m_event_listner.unlock();
    DBUG_VOID_RETURN;
  }
  m_mgmsrv.m_event_listner.unlock();

  {
    LogLevel tmp;
    m_mgmsrv.m_event_listner.update_max_log_level(tmp);
  }

  m_output->println(reply);
  m_output->println("result: Ok");
  m_output->println("");
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
    m_output->println("result: Invalid loglevel", errorString.c_str());
    m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
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
    abort = 0;
  char *nodes_str;
  Vector<NodeId> nodes;
    
  args.get("initialstart", &initialstart);
  args.get("nostart", &nostart);
  args.get("abort", &abort);
  args.get("node", (const char **)&nodes_str);

  char *p, *last;
  for((p = strtok_r(nodes_str, " ", &last));
      p;
      (p = strtok_r(NULL, " ", &last))) {
    nodes.push_back(atoi(p));
  }

  int restarted = 0;
  int result= m_mgmsrv.restartNodes(nodes,
                                    &restarted,
                                    nostart != 0,
                                    initialstart != 0,
                                    abort != 0,
                                    &m_stopSelf);
  
  m_output->println("restart reply");
  if(result != 0){
    m_output->println("result: %d-%s", result, get_error_text(result));
  } else
    m_output->println("result: Ok");
  m_output->println("restarted: %d", restarted);
  if(version>1)
    m_output->println("disconnect: %d", (m_stopSelf)?1:0);
  m_output->println("");
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
  m_output->println("");
}

static void
printNodeStatus(OutputStream *output,
		MgmtSrvr &mgmsrv,
		enum ndb_mgm_node_type type) {
  NodeId nodeId = 0;
  while(mgmsrv.getNextNodeId(&nodeId, type)) {
    enum ndb_mgm_node_status status;
    Uint32 startPhase = 0, 
      version = 0, 
      dynamicId = 0, 
      nodeGroup = 0,
      connectCount = 0;
    bool system;
    const char *address= NULL;
    mgmsrv.status(nodeId, &status, &version, &startPhase,
		  &system, &dynamicId, &nodeGroup, &connectCount,
		  &address);
    output->println("node.%d.type: %s",
		      nodeId,
		      ndb_mgm_get_node_type_string(type));
    output->println("node.%d.status: %s",
		      nodeId,
		      ndb_mgm_get_node_status_string(status));
    output->println("node.%d.version: %d", nodeId, version);
    output->println("node.%d.startphase: %d", nodeId, startPhase);
    output->println("node.%d.dynamic_id: %d", nodeId, dynamicId);
    output->println("node.%d.node_group: %d", nodeId, nodeGroup);
    output->println("node.%d.connect_count: %d", nodeId, connectCount);
    output->println("node.%d.address: %s", nodeId, address ? address : "");
  }

}

void
MgmApiSession::getStatus(Parser<MgmApiSession>::Context &,
			 Properties const &) {
  int noOfNodes = 0;

  NodeId nodeId = 0;
  while(m_mgmsrv.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    noOfNodes++;
  }
  nodeId = 0;
  while(m_mgmsrv.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_API)){
    noOfNodes++;
  }
  nodeId = 0;
  while(m_mgmsrv.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_MGM)){
    noOfNodes++;
  }
  SLEEP_ERROR_INSERTED(5);
  m_output->println("node status");
  SLEEP_ERROR_INSERTED(6);
  m_output->println("nodes: %d", noOfNodes);
  m_mgmsrv.updateStatus();
  SLEEP_ERROR_INSERTED(7);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_NDB);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_MGM);
  SLEEP_ERROR_INSERTED(8);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_API);
  SLEEP_ERROR_INSERTED(9);

  nodeId = 0;

  m_output->println("");
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
		      names[i], m_mgmsrv.isEventLogFilterEnabled(i));
  }
  m_output->println("");
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
  Uint32 abort;
  char *nodes_str;
  Vector<NodeId> nodes;

  args.get("node", (const char **)&nodes_str);
  if(nodes_str == NULL)
  {
    m_output->println("stop reply");
    m_output->println("result: empty node list");
    m_output->println("");
    return;
  }
  args.get("abort", &abort);

  char *p, *last;
  for((p = strtok_r(nodes_str, " ", &last));
      p;
      (p = strtok_r(NULL, " ", &last))) {
    nodes.push_back(atoi(p));
  }

  int stopped= 0;
  int result= 0;
  if (nodes.size())
    result= m_mgmsrv.stopNodes(nodes, &stopped, abort != 0, &m_stopSelf);

  m_output->println("stop reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("stopped: %d", stopped);
  if(version>1)
    m_output->println("disconnect: %d", (m_stopSelf)?1:0);
  m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
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
  m_output->println("");
}

void
MgmApiSession::start(Parser<MgmApiSession>::Context &,
		     Properties const &args) {
  Uint32 node;

  args.get("node", &node);
  
  int result = m_mgmsrv.start(node);

  m_output->println("start reply");
  if(result != 0)
    m_output->println("result: %s", get_error_text(result));
  else
    m_output->println("result: Ok");
  m_output->println("");
}

void
MgmApiSession::startAll(Parser<MgmApiSession>::Context &,
			Properties const &) {
  NodeId node = 0;
  int started = 0;

  while(m_mgmsrv.getNextNodeId(&node, NDB_MGM_NODE_TYPE_NDB))
    if(m_mgmsrv.start(node) == 0)
      started++;

  m_output->println("start reply");
  m_output->println("result: Ok");
  m_output->println("started: %d", started);
  m_output->println("");
}

void
MgmApiSession::setLogFilter(Parser_t::Context &ctx,
			    const class Properties &args) {
  Uint32 severity;
  Uint32 enable;

  args.get("level", &severity);
  args.get("enable", &enable);

  int result = m_mgmsrv.setEventLogFilter(severity, enable);

  m_output->println("set logfilter reply");
  m_output->println("result: %d", result);
  m_output->println("");
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

void
Ndb_mgmd_event_service::log(int eventType, const Uint32* theData, NodeId nodeId){
  
  Uint32 threshold;
  LogLevel::EventCategory cat;
  Logger::LoggerLevel severity;
  EventLoggerBase::EventTextFunction textF;
  int i, n;
  DBUG_ENTER("Ndb_mgmd_event_service::log");
  DBUG_PRINT("enter",("eventType=%d, nodeid=%d", eventType, nodeId));

  if (EventLoggerBase::event_lookup(eventType,cat,threshold,severity,textF))
    DBUG_VOID_RETURN;

  char m_text[512];
  EventLogger::getText(m_text, sizeof(m_text),
		       textF, theData, nodeId);

  BaseString str("log event reply\n");
  str.appfmt("type=%d\n", eventType);
  str.appfmt("time=%d\n", 0);
  str.appfmt("source_nodeid=%d\n", nodeId);
  for (i= 0; ndb_logevent_body[i].token; i++)
  {
    if ( ndb_logevent_body[i].type != eventType)
      continue;
    int val= theData[ndb_logevent_body[i].index];
    if (ndb_logevent_body[i].index_fn)
      val= (*(ndb_logevent_body[i].index_fn))(val);
    str.appfmt("%s=%d\n",ndb_logevent_body[i].token, val);
    if(strcmp(ndb_logevent_body[i].token,"error") == 0)
    {
      int m_text_len= strlen(m_text);
      if(sizeof(m_text)-m_text_len-3 > 0)
      {
        BaseString::snprintf(m_text+m_text_len, 4 , " - ");
        ndb_error_string(val, m_text+(m_text_len+3), sizeof(m_text)-m_text_len-3);
      }
    }
  }
  
  Vector<NDB_SOCKET_TYPE> copy;
  m_clients.lock();
  for(i = m_clients.size() - 1; i >= 0; i--)
  {
    if(threshold <= m_clients[i].m_logLevel.getLogLevel(cat))
    {
      if(m_clients[i].m_socket==NDB_INVALID_SOCKET)
        continue;

      SocketOutputStream out(m_clients[i].m_socket);

      int r;
      if (m_clients[i].m_parsable)
        r= out.println(str.c_str());
      else
        r= out.println(m_text);

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
  req = tmp;
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
    if(m_clients[i].m_socket==NDB_INVALID_SOCKET)
      continue;

    SocketOutputStream out(m_clients[i].m_socket);

    DBUG_PRINT("info",("%d %d",i,m_clients[i].m_socket));

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
  DBUG_PRINT("enter",("client.m_socket: %d", client.m_socket));

  check_listeners();

  m_clients.push_back(client);
  update_max_log_level(client.m_logLevel);

  DBUG_VOID_RETURN;
}

void
Ndb_mgmd_event_service::stop_sessions(){
  m_clients.lock();
  for(int i = m_clients.size() - 1; i >= 0; i--){
    if(m_clients[i].m_socket != NDB_INVALID_SOCKET){
      NDB_CLOSE_SOCKET(m_clients[i].m_socket);
      m_clients.erase(i);
    }
  }
  m_clients.unlock();
}

void
MgmApiSession::setParameter(Parser_t::Context &,
			    Properties const &args) {
  BaseString node, param, value;
  args.get("node", node);
  args.get("parameter", param);
  args.get("value", value);
  
  BaseString result;
  int ret = m_mgmsrv.setDbParameter(atoi(node.c_str()), 
				    atoi(param.c_str()),
				    value.c_str(),
				    result);
  
  m_output->println("set parameter reply");
  m_output->println("message: %s", result.c_str());
  m_output->println("result: %d", ret);
  m_output->println("");
}

void
MgmApiSession::setConnectionParameter(Parser_t::Context &ctx,
				      Properties const &args) {
  BaseString node1, node2, param, value;
  args.get("node1", node1);
  args.get("node2", node2);
  args.get("param", param);
  args.get("value", value);
  
  BaseString result;
  int ret = m_mgmsrv.setConnectionDbParameter(atoi(node1.c_str()),
					      atoi(node2.c_str()),
					      atoi(param.c_str()),
					      atoi(value.c_str()),
					      result);
  
  m_output->println("set connection parameter reply");
  m_output->println("message: %s", result.c_str());
  m_output->println("result: %s", (ret>0)?"Ok":"Failed");
  m_output->println("");
}

void
MgmApiSession::getConnectionParameter(Parser_t::Context &ctx,
				      Properties const &args) {
  BaseString node1, node2, param;
  int value = 0;

  args.get("node1", node1);
  args.get("node2", node2);
  args.get("param", param);
  
  BaseString result;
  int ret = m_mgmsrv.getConnectionDbParameter(atoi(node1.c_str()),
					      atoi(node2.c_str()),
					      atoi(param.c_str()),
					      &value,
					      result);
  
  m_output->println("get connection parameter reply");
  m_output->println("value: %d", value);
  m_output->println("result: %s", (ret>0)?"Ok":result.c_str());
  m_output->println("");
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
  for(size_t i = 0; i<list.size(); i++){
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
  m_output->println("");

  if(result==0)
  {
    m_mgmsrv.m_event_listner.add_listener(le);
    m_stop = true;
    m_socket = NDB_INVALID_SOCKET;
  }
}

void
MgmApiSession::stop_session_if_not_connected(SocketServer::Session *_s, void *data)
{
  MgmApiSession *s= (MgmApiSession *)_s;
  struct PurgeStruct &ps= *(struct PurgeStruct *)data;
  if (s->m_allocated_resources->is_reserved(ps.free_nodes))
  {
    if (ps.str)
      ps.str->appfmt(" %d", s->m_allocated_resources->get_nodeid());
    s->stopSession();
  }
}

void
MgmApiSession::stop_session_if_timed_out(SocketServer::Session *_s, void *data)
{
  MgmApiSession *s= (MgmApiSession *)_s;
  struct PurgeStruct &ps= *(struct PurgeStruct *)data;
  if (s->m_allocated_resources->is_reserved(ps.free_nodes) &&
      s->m_allocated_resources->is_timed_out(ps.tick))
  {
    s->stopSession();
  }
}

void
MgmApiSession::purge_stale_sessions(Parser_t::Context &ctx,
				    const class Properties &args)
{
  struct PurgeStruct ps;
  BaseString str;
  ps.str = &str;

  m_mgmsrv.get_connected_nodes(ps.free_nodes);
  ps.free_nodes.bitXORC(NodeBitmask()); // invert connected_nodes to get free nodes

  m_mgmsrv.get_socket_server()->foreachSession(stop_session_if_not_connected,&ps);
  m_mgmsrv.get_socket_server()->checkSessions();

  m_output->println("purge stale sessions reply");
  if (str.length() > 0)
    m_output->println("purged:%s",str.c_str());
  m_output->println("result: Ok");
  m_output->println("");
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
  m_output->println("");
}

void
MgmApiSession::transporter_connect(Parser_t::Context &ctx,
				   Properties const &args)
{
  m_mgmsrv.transporter_connect(m_socket);

  m_stop= true;
  m_stopped= true; // force a stop (no closing socket)
  m_socket= NDB_INVALID_SOCKET;   // so nobody closes it
}

void
MgmApiSession::get_mgmd_nodeid(Parser_t::Context &ctx,
			       Properties const &args)
{
  m_output->println("get mgmd nodeid reply");
  m_output->println("nodeid:%u",m_mgmsrv.getOwnNodeId());
  SLEEP_ERROR_INSERTED(1);

  m_output->println("");
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

  m_mgmsrv.eventReport(data);
  m_output->println("report event reply");
  m_output->println("result: ok");
  m_output->println("");
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
  lister->m_output->println("session.%llu.allocated.nodeid: %d",id,s->m_allocated_resources->get_nodeid());
  if(s->m_ctx)
  {
    int l= strlen(s->m_ctx->m_tokenBuffer);
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

  m_output->println("");
}

void
MgmApiSession::getSessionId(Parser_t::Context &ctx,
                                 Properties const &args) {
  m_output->println("get session id reply");
  m_output->println("id: %llu",m_session_id);
  m_output->println("");
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
  p->l->m_output->println("nodeid: %d",s->m_allocated_resources->get_nodeid());
  if(s->m_ctx)
  {
    int l= strlen(s->m_ctx->m_tokenBuffer);
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

  m_output->println("");
}

template class MutexVector<int>;
template class Vector<ParserRow<MgmApiSession> const*>;
