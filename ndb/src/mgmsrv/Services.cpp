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
#include <ctype.h>

#include <uucode.h>
#include <socket_io.h>
#include <ndb_version.h>
#include <mgmapi.h>
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <LogLevel.hpp>
#include <BaseString.hpp>
#include <Base64.hpp>

#include <ConfigValues.hpp>
#include <mgmapi_configuration.hpp>

#include "Services.hpp"

extern bool g_StopServer;

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
  MGM_CMD("get statport", &MgmApiSession::getStatPort, ""),
  
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

  MGM_CMD("get version", &MgmApiSession::getVersion, ""),
  
  MGM_CMD("get status", &MgmApiSession::getStatus, ""),

  MGM_CMD("get info clusterlog", &MgmApiSession::getInfoClusterLog, ""),

  MGM_CMD("restart node", &MgmApiSession::restart, ""),
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

  MGM_CMD("abort backup", &MgmApiSession::abortBackup, ""),
    MGM_ARG("id", Int, Mandatory, "Backup id"),

  /**
   *  Global Replication
   */
  MGM_CMD("rep", &MgmApiSession::repCommand, ""),
    MGM_ARG("request", Int, Mandatory, "Command"),

  MGM_CMD("stop", &MgmApiSession::stop, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("abort", Int, Mandatory, "Node"),

  MGM_CMD("stop all", &MgmApiSession::stopAll, ""),
    MGM_ARG("abort", Int, Mandatory, "Node"),
  
  MGM_CMD("enter single user", &MgmApiSession::enterSingleUser, ""),
    MGM_ARG("nodeId", Int, Mandatory, "Node"),
  
  MGM_CMD("exit single user", &MgmApiSession::exitSingleUser, ""),
  

  MGM_CMD("start", &MgmApiSession::start, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),

  MGM_CMD("start all", &MgmApiSession::startAll, ""),

  MGM_CMD("bye", &MgmApiSession::bye, ""),

  MGM_CMD("set loglevel", &MgmApiSession::setLogLevel, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("category", String, Mandatory, "Event category"),
    MGM_ARG("level", Int, Mandatory, "Log level (0-15)"),

  MGM_CMD("set cluster loglevel", &MgmApiSession::setClusterLogLevel, ""),
    MGM_ARG("node", Int, Mandatory, "Node"),
    MGM_ARG("category", String, Mandatory, "Event category"),
    MGM_ARG("level", Int, Mandatory, "Log level (0-15)"),

  MGM_CMD("set logfilter", &MgmApiSession::setLogFilter, ""),
    MGM_ARG("level", Int, Mandatory, "Severety level"),

  MGM_CMD("config lock", &MgmApiSession::configLock, ""),

  MGM_CMD("config unlock", &MgmApiSession::configUnlock, ""),
    MGM_ARG("commit", Int, Mandatory, "Commit changes"),

  MGM_CMD("config change", &MgmApiSession::configChange, ""),
    MGM_ARG("section", String, Mandatory, "Section"),
    MGM_ARG("parameter", String, Mandatory, "Parameter"),
    MGM_ARG("value", String, Mandatory, "Value"),

  MGM_CMD("config lock", &MgmApiSession::configLock, ""),

  MGM_CMD("config unlock", &MgmApiSession::configUnlock, ""),
    MGM_ARG("commit", Int, Mandatory, "Commit changes"),

  MGM_CMD("set parameter", &MgmApiSession::setParameter, ""),
    MGM_ARG("node", String, Mandatory, "Node"),
    MGM_ARG("parameter", String, Mandatory, "Parameter"),
    MGM_ARG("value", String, Mandatory, "Value"),

  MGM_CMD("listen event", &MgmApiSession::listen_event, ""),
    MGM_ARG("node", Int, Optional, "Node"),  
    MGM_ARG("filter", String, Mandatory, "Event category"),

  MGM_END()
};

MgmApiSession::MgmApiSession(class MgmtSrvr & mgm, NDB_SOCKET_TYPE sock)
  : SocketServer::Session(sock), m_mgmsrv(mgm) {
  m_input = new SocketInputStream(sock);
  m_output = new SocketOutputStream(sock);
  m_parser = new Parser_t(commands, *m_input, true, true, true);
  m_allocated_resources= new MgmtSrvr::Allocated_resources(m_mgmsrv);
}

MgmApiSession::~MgmApiSession()
{
  if (m_input)
    delete m_input;
  if (m_output)
    delete m_output;
  if (m_parser)
    delete m_parser;
  if (m_allocated_resources)
    delete m_allocated_resources;
}

void
MgmApiSession::runSession() {
  Parser_t::Context ctx;
  while(!m_stop) {
    m_parser->run(ctx, *this);

    if(ctx.m_currentToken == 0)
      break;

    switch(ctx.m_status) {
    case Parser_t::UnknownCommand:
#ifdef MGM_GET_CONFIG_BACKWARDS_COMPAT
      /* Backwards compatibility for old NDBs that still use
       * the old "GET CONFIG" command.
       */

      for(size_t i=0; i<strlen(ctx.m_currentToken); i++)
	ctx.m_currentToken[i] = toupper(ctx.m_currentToken[i]);

      if(strncmp("GET CONFIG ", 
		 ctx.m_currentToken,
		 strlen("GET CONFIG ")) == 0)
	getConfig_old(ctx);
#endif /* MGM_GET_CONFIG_BACKWARDS_COMPAT */
      break;
    default:
      break;
    }
  }
  if(m_socket >= 0)
    NDB_CLOSE_SOCKET(m_socket);
}

#ifdef MGM_GET_CONFIG_BACKWARDS_COMPAT
void
MgmApiSession::getConfig_old(Parser_t::Context &ctx) {
  Properties args;

  Uint32 version, node;

  if(sscanf(ctx.m_currentToken, "GET CONFIG %d %d",
	    (int *)&version, (int *)&node) != 2) {
    m_output->println("Expected 2 arguments for GET CONFIG");
    return;
  }

  /* Put arguments in properties object so we can call the real function */  
  args.put("version", version);
  args.put("node", node);
  getConfig_common(ctx, args, true);
}
#endif /* MGM_GET_CONFIG_BACKWARDS_COMPAT */

inline void require(bool b){ if(!b) abort(); }

void
MgmApiSession::getConfig(Parser_t::Context &ctx, 
			 const class Properties &args) {
  getConfig_common(ctx, args);
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
  const char * transporter;
  const char * user;
  const char * password;
  const char * public_key;

  args.get("version", &version);
  args.get("nodetype", &nodetype);
  args.get("transporter", &transporter);
  args.get("nodeid", &nodeid);
  args.get("user", &user);
  args.get("password", &password);
  args.get("public key", &public_key);
  
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

  struct sockaddr addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  int r = getpeername(m_socket, &addr, &addrlen);
  if (r != 0 ) {
    m_output->println(cmd);
    m_output->println("result: getpeername(%d) failed, err= %d", m_socket, r);
    m_output->println("");
    return;
  }

  NodeId tmp= nodeid;
  if(tmp == 0 || !m_allocated_resources->is_reserved(tmp)){
    BaseString error_string;
    if (!m_mgmsrv.alloc_node_id(&tmp, (enum ndb_mgm_node_type)nodetype, 
				&addr, &addrlen, error_string)){
      const char *alias;
      const char *str;
      alias= ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)nodetype, &str);
      m_output->println(cmd);
      m_output->println("result: %s", error_string.c_str());
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
  m_allocated_resources->reserve_node(tmp);
  
  return;
}

void
MgmApiSession::getConfig_common(Parser_t::Context &,
				const class Properties &args,
				bool compat) {
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

  if(version > 0 && version < makeVersion(3, 5, 0) && compat){
    Properties *reply = backward("", conf->m_oldConfig);
    reply->put("Version", version);
    reply->put("LocalNodeId", node);

    backward("", reply);
    //reply->print();
    
    const Uint32 size = reply->getPackedSize();
    Uint32 *buffer = new Uint32[size/4+1];
    
    reply->pack(buffer);
    delete reply;
    
    const int uurows = (size + 44)/45;
    char * uubuf = new char[uurows * 62+5];
      
    const int uusz = uuencode_mem(uubuf, (char *)buffer, size);
    delete[] buffer;
      
    m_output->println("GET CONFIG %d %d %d %d %d",
		      0, version, node, size, uusz);
    
    m_output->println("begin 664 Ndb_cfg.bin");
      
    /* XXX Need to write directly to the socket, because the uubuf is not
     * NUL-terminated. This could/should probably be done in a nicer way.
     */
    write_socket(m_socket, MAX_WRITE_TIMEOUT, uubuf, uusz);
    delete[] uubuf;
      
    m_output->println("end");
    m_output->println("");
    return;
  }

  if(compat){
    m_output->println("GET CONFIG %d %d %d %d %d",1, version, 0, 0, 0);
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
  
  const ConfigValues * cfg = &conf->m_configValues->m_config;
  const Uint32 size = cfg->getPackedSize();
  
  UtilBuffer src;
  cfg->pack(src);
  
  BaseString str;
  int res = base64_encode(src, str);
  
  m_output->println("get config reply");
  m_output->println("result: Ok");
  m_output->println("Content-Length: %d", str.length());
  m_output->println("Content-Type: ndbconfig/octet-stream");
  m_output->println("Content-Transfer-Encoding: base64");
  m_output->println("");
  m_output->println(str.c_str());

  return;
}

void
MgmApiSession::getStatPort(Parser_t::Context &, 
			   const class Properties &) {

  m_output->println("get statport reply");
  m_output->println("tcpport: %d", 0);
  m_output->println("");
}

void
MgmApiSession::insertError(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 node = 0, error = 0;

  args.get("node", &node);
  args.get("error", &error);

  int result = m_mgmsrv.insertError(node, error);

  m_output->println("insert error reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
#if 0

/*****************************************************************************
 * BACKUP
 *****************************************************************************/

int completed;
MgmtSrvr::BackupEvent globalEvent;

static void
completedCallback(const MgmtSrvr::BackupEvent & event){

  ndbout << "WaitCallback" << endl;
  // Save event in the latestEvent var 

  switch(event.Event){
  case MgmtSrvr::BackupEvent::BackupCompleted:
  case MgmtSrvr::BackupEvent::BackupFailedToStart:
    globalEvent = event;
    completed = 1;
    break;
  }
}

void
MgmApiSession::startBackup(Parser<MgmApiSession>::Context &,
			   Properties const &) {
  unsigned backupId;
  int result;

  MgmtSrvr::BackupCallback prevCallback;
  prevCallback = m_mgmsrv.setCallback(completedCallback);
  completed = 0;
  result = m_mgmsrv.startBackup(backupId);
  if (result == 0){

    // Wait for the callback to call our condition
    //  waitFor();
    while (completed == 0)
      NdbSleep_SecSleep(0);
  
    if (globalEvent.Event == MgmtSrvr::BackupEvent::BackupFailedToStart)
      result = globalEvent.FailedToStart.ErrorCode;
    else      
      backupId = globalEvent.Completed.BackupId;    
  }

  // restore old callback
  m_mgmsrv.setCallback(prevCallback);

  m_output->println("start backup reply");
  if(result != 0)
    m_output->println("result: %s(%d)", m_mgmsrv.getErrorText(result), result);
  else{
    m_output->println("result: Ok");
    m_output->println("id: %d", backupId);
  }
  m_output->println("");

}
#endif

void
MgmApiSession::startBackup(Parser<MgmApiSession>::Context &,
			   Properties const &) {
  unsigned backupId;
  int result;

  result = m_mgmsrv.startBackup(backupId, true);

  m_output->println("start backup reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else{
    m_output->println("result: Ok");
    m_output->println("id: %d", backupId);
  }
  m_output->println("");

}

void
MgmApiSession::abortBackup(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 id = 0;

  args.get("id", &id);

  int result = m_mgmsrv.abortBackup(id);

  m_output->println("abort backup reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else
    m_output->println("result: Ok");
  m_output->println("");
}

/*****************************************************************************
 * Global Replication
 *****************************************************************************/

void
MgmApiSession::repCommand(Parser<MgmApiSession>::Context &,
			  Properties const &args) {
  
  Uint32 request = 0;
  args.get("request", &request);
  
  Uint32 repReqId;
  int result = m_mgmsrv.repCommand(&repReqId, request, true);
  
  m_output->println("global replication reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else{
    m_output->println("result: Ok");
    m_output->println("id: %d", repReqId);
  }
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
MgmApiSession::setClusterLogLevel(Parser<MgmApiSession>::Context &,
				  Properties const &args) {
  Uint32 node, level;
  BaseString categoryName, errorString;
  SetLogLevelOrd logLevel;
  int result;
  args.get("node", &node);
  args.get("category", categoryName);
  args.get("level", &level);

  /* XXX should use constants for this value */
  if(level > 15) {
    errorString.assign("Invalied loglevel");
    goto error;
  }

  categoryName.ndb_toupper();
 
  LogLevel::EventCategory category;
  if(!EventLogger::matchEventCategory(categoryName.c_str(), &category)) {
    errorString.assign("Unknown category");
    goto error;
  }

  EventSubscribeReq req;
  req.blockRef = 0;
  req.noOfEntries = 1;
  req.theCategories[0] = category;
  req.theLevels[0] = level;
  m_mgmsrv.m_log_level_requests.push_back(req);

  m_output->println("set cluster loglevel reply");
  m_output->println("result: Ok");
  m_output->println("");
  return;
 error:
  m_output->println("set cluster loglevel reply");
  m_output->println("result: %s", errorString.c_str());
  m_output->println("");
}

void
MgmApiSession::setLogLevel(Parser<MgmApiSession>::Context &,
			   Properties const &args) {
  Uint32 node = 0, level = 0;
  BaseString categoryName, errorString;
  SetLogLevelOrd logLevel;
  int result;
  logLevel.clear();
  args.get("node", &node);
  args.get("category", categoryName);
  args.get("level", &level);

  /* XXX should use constants for this value */
  if(level > 15) {
    errorString.assign("Invalied loglevel");
    goto error;
  }

  categoryName.ndb_toupper();
 
  LogLevel::EventCategory category;
  if(!EventLogger::matchEventCategory(categoryName.c_str(), &category)) {
    errorString.assign("Unknown category");
    goto error;
  }

  EventSubscribeReq req;
  req.blockRef = node;
  req.noOfEntries = 1;
  req.theCategories[0] = category;
  req.theLevels[0] = level;
  m_mgmsrv.m_log_level_requests.push_back(req);
  
  m_output->println("set loglevel reply");
  m_output->println("result: Ok");
  m_output->println("");
  return;
 error:
  m_output->println("set loglevel reply");
  m_output->println("result: %s", errorString.c_str());
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else
    m_output->println("result: Ok");
  m_output->println("");
}

void
MgmApiSession::restart(Parser<MgmApiSession>::Context &,
		       Properties const &args) {
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
  int result = 0;

  for(size_t i = 0; i < nodes.size(); i++)
    if((result = m_mgmsrv.restartNode(nodes[i],
				      nostart != 0,
				      initialstart != 0,
				      abort != 0)) == 0)
      restarted++;
  
  m_output->println("restart reply");
  if(result != 0){
    m_output->println("result: %d-%s", result, m_mgmsrv.getErrorText(result));
  } else
    m_output->println("result: Ok");
  m_output->println("restarted: %d", restarted);
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
  int result = m_mgmsrv.restart(nostart, initialstart, abort, &count);

  m_output->println("restart reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
    mgmsrv.status(nodeId, &status, &version, &startPhase, 
		  &system, &dynamicId, &nodeGroup, &connectCount);
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
    output->println("node.%d.address: %s", nodeId, mgmsrv.get_connect_address(nodeId));
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
  
  m_output->println("node status");
  m_output->println("nodes: %d", noOfNodes);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_NDB);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_MGM);
  printNodeStatus(m_output, m_mgmsrv, NDB_MGM_NODE_TYPE_API);

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
MgmApiSession::stop(Parser<MgmApiSession>::Context &,
		    Properties const &args) {
  Uint32 abort;
  char *nodes_str;
  Vector<NodeId> nodes;

  args.get("node", (const char **)&nodes_str);
  if(nodes_str == NULL)
    return;
  args.get("abort", &abort);

  char *p, *last;
  for((p = strtok_r(nodes_str, " ", &last));
      p;
      (p = strtok_r(NULL, " ", &last))) {
    nodes.push_back(atoi(p));
  }

  int stop_self= 0;

  for(size_t i=0; i < nodes.size(); i++) {
    if (nodes[i] == m_mgmsrv.getOwnNodeId()) {
      stop_self= 1;
      if (i != nodes.size()-1) {
	m_output->println("stop reply");
	m_output->println("result: server must be stopped last");
	m_output->println("");
	return;
      }
    }
  }

  int stopped = 0, result = 0;
  
  for(size_t i=0; i < nodes.size(); i++)
    if (nodes[i] != m_mgmsrv.getOwnNodeId()) {
      if((result = m_mgmsrv.stopNode(nodes[i], abort != 0)) == 0)
	stopped++;
    } else
      stopped++;

  m_output->println("stop reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else
    m_output->println("result: Ok");
  m_output->println("stopped: %d", stopped);
  m_output->println("");

  if (stop_self)
    g_StopServer= true;
}


void
MgmApiSession::stopAll(Parser<MgmApiSession>::Context &,
			      Properties const &args) {
  int stopped = 0;
  Uint32 abort;
  args.get("abort", &abort);

  int result = m_mgmsrv.stop(&stopped, abort != 0);

  m_output->println("stop reply");
  if(result != 0)
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
  else
    m_output->println("result: Ok");
  m_output->println("stopped: %d", stopped);
  m_output->println("");
}

void
MgmApiSession::enterSingleUser(Parser<MgmApiSession>::Context &,
			  Properties const &args) {
  int stopped = 0;
  Uint32 nodeId = 0;
  args.get("nodeId", &nodeId);
  int result = m_mgmsrv.enterSingleUser(&stopped, nodeId);
  m_output->println("enter single user reply");
  if(result != 0) {
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
  snprintf(buf, 200, "%s", blockList.c_str());
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
    m_output->println("result: %s", m_mgmsrv.getErrorText(result));
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
  Uint32 level;

  args.get("level", &level);

  int result = m_mgmsrv.setEventLogFilter(level);

  m_output->println("set logfilter reply");
  m_output->println("result: %d", result);
  m_output->println("");
}

void
MgmApiSession::configLock(Parser_t::Context &,
			   Properties const &) {
  int ret = m_mgmsrv.lockConf();
  m_output->println("config lock reply");
  m_output->println("result: %d", ret);
  m_output->println("");
}

void
MgmApiSession::configUnlock(Parser_t::Context &,
			   Properties const &args) {
  Uint32 commit;
  args.get("commit", &commit);
  int ret = m_mgmsrv.unlockConf(commit == 1);
  m_output->println("config unlock reply");
  m_output->println("result: %d", ret);
  m_output->println("");
}

void
MgmApiSession::configChange(Parser_t::Context &,
			    Properties const &args) {
  BaseString section, param, value;
  args.get("section", section);
  args.get("parameter", param);
  args.get("value", value);

  int ret = m_mgmsrv.changeConfig(section.c_str(),
				  param.c_str(),
				  value.c_str());
  m_output->println("config change reply");
  m_output->println("result: %d", ret);
  m_output->println("");
}

NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<_LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
}

void
MgmStatService::log(int eventType, const Uint32* theData, NodeId nodeId){
  
  Uint32 threshold = 0;
  LogLevel::EventCategory cat;
  
  for(unsigned i = 0; i<EventLogger::matrixSize; i++){
    if(EventLogger::matrix[i].eventType == eventType){
      cat = EventLogger::matrix[i].eventCategory;
      threshold = EventLogger::matrix[i].threshold;
      break;
    }
  }

  char m_text[256];
  EventLogger::getText(m_text, sizeof(m_text), eventType, theData, nodeId);

  Vector<NDB_SOCKET_TYPE> copy; 
  m_clients.lock();
  int i;
  for(i = m_clients.size() - 1; i >= 0; i--){
    if(threshold <= m_clients[i].m_logLevel.getLogLevel(cat)){
      if(m_clients[i].m_socket >= 0 &&
	 println_socket(m_clients[i].m_socket, 
			MAX_WRITE_TIMEOUT, m_text) == -1){
	copy.push_back(m_clients[i].m_socket);
	m_clients.erase(i, false);
      }
    }
  }
  m_clients.unlock();
  
  for(i = 0; (unsigned)i<copy.size(); i++){
    NDB_CLOSE_SOCKET(copy[i]);
  }

  if(copy.size()){
    LogLevel tmp; tmp.clear();
    m_clients.lock();
    for(i = 0; i < m_clients.size(); i++){
      tmp.set_max(m_clients[i].m_logLevel);
    }
    m_clients.unlock();

    if(!(tmp == m_logLevel)){
      m_logLevel = tmp;
      EventSubscribeReq req; 
      req = tmp;
      req.blockRef = 0;
      m_mgmsrv->m_log_level_requests.push_back(req);
    }
  }
}

void
MgmStatService::add_listener(const StatListener& client){
  m_clients.push_back(client);
  LogLevel tmp = m_logLevel;
  tmp.set_max(client.m_logLevel);
  
  if(!(tmp == m_logLevel)){
    m_logLevel = tmp;
    EventSubscribeReq req;
    req = tmp;
    req.blockRef = 0;
    m_mgmsrv->m_log_level_requests.push_back(req);
  }
}

void
MgmStatService::stopSessions(){
  for(int i = m_clients.size() - 1; i >= 0; i--){
    if(m_clients[i].m_socket >= 0){
      NDB_CLOSE_SOCKET(m_clients[i].m_socket);
      m_clients.erase(i);
    }
  }
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
MgmApiSession::listen_event(Parser<MgmApiSession>::Context & ctx,
			    Properties const & args) {
  
  BaseString node, param, value;
  args.get("node", node);
  args.get("filter", param);

  int result = 0;
  BaseString msg;

  MgmStatService::StatListener le;
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
    
    spec[0].trim();
    spec[0].ndb_toupper();
    
    LogLevel::EventCategory category;
    if(!EventLogger::matchEventCategory(spec[0].c_str(), &category)) {
      msg.appfmt("Unknown category: >%s<", spec[0].c_str());
      result = -1;
      goto done;
    }

    int level = atoi(spec[1].c_str());
    if(level < 0 || level > 15){
      msg.appfmt("Invalid level: >%s<", spec[1].c_str());
      result = -1;
      goto done;
    }
    le.m_logLevel.setLogLevel(category, level);
  }
  
  if(list.size() == 0){
    msg.appfmt("Empty filter specification");
    result = -1;
    goto done;
  }
  
  m_mgmsrv.m_statisticsListner.add_listener(le);
  
  m_stop = true;
  m_socket = -1;

done:
  m_output->println("listen event");
  m_output->println("result: %d", result);
  if(result != 0)
    m_output->println("msg: %s", msg.c_str());
}

template class MutexVector<int>;
template class Vector<ParserRow<MgmApiSession> const*>;
template class Vector<unsigned short>;
