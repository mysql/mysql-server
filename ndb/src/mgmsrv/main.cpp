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

#include <signal.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include "Config.hpp"
#include "InitConfigFileParser.hpp"
#include <SocketServer.hpp>
#include "Services.hpp"
#include <version.h>
#include <kernel_types.h>
#include <Properties.hpp>
#include <NdbOut.hpp>
#include <NdbMain.h>
#include <NdbDaemon.h>
#include <NdbConfig.h>
#include <NdbHost.h>
#include <ndb_version.h>
#include <ConfigRetriever.hpp>
#include <mgmapi_config_parameters.h>
#include <getarg.h>

#if defined NDB_OSE || defined NDB_SOFTOSE
#include <efs.h>
#else
#include "CommandInterpreter.hpp"
#endif

#undef DEBUG
#define DEBUG(x) ndbout << x << endl;

const char progname[] = "mgmtsrvr";


/**
 * @struct  MgmGlobals
 * @brief   Global Variables used in the management server
 ******************************************************************************/
struct MgmGlobals {
  MgmGlobals();
  ~MgmGlobals();
  
  /** Command line arguments  */
  int daemon;   // NOT bool, bool need not be int
  int non_interactive;
  const char * config_filename;
  const char * local_config_filename;
  
  /** Stuff found in environment or in local config  */
  NodeId localNodeId;
  bool use_specific_ip;
  char * interface_name;
  int port;
  int port_stats;
  
  /** The configuration of the cluster */
  Config * cluster_config;
  
  /** The Mgmt Server */
  MgmtSrvr * mgmObject;
  
  /** The Socket Server */
  SocketServer * socketServer;
};

static MgmGlobals glob;


/******************************************************************************
 * Function prototypes
 ******************************************************************************/
static bool readLocalConfig();
static bool readGlobalConfig();
static bool setPortNo();

/**
 * Global variables
 */
bool g_StopServer;
extern EventLogger g_EventLogger;

extern int global_mgmt_server_check;
int _print_version = 0;

struct getargs args[] = {
  { "version", 0, arg_flag, &_print_version,
    "Print versions"},
  { NULL, 'c', arg_string, &glob.config_filename,
    "Running cluster configuration file", "filename" },
  { NULL, 'd', arg_flag, &glob.daemon,
    "Daemon mode" },
  { NULL, 'l', arg_string, &glob.local_config_filename,
    "Local configuration file (Ndb.cfg)",
    "filename" },
  { NULL, 'n', arg_flag, &glob.non_interactive,
    "Don't run as daemon, but don't read from stdin", "non-interactive" }
};

int num_args = sizeof(args) / sizeof(args[0]);

/*
 *  MAIN 
 */
NDB_MAIN(mgmsrv){
  /**
   * OSE specific. Enable shared ownership of file system resources. 
   * This is needed in order to use the cluster log since the events 
   * from the cluster is written from the 'ndb_receive'(NDBAPI) thread/process.
   */
#if defined NDB_OSE || defined NDB_SOFTOSE
  efs_segment_share();
#endif

  global_mgmt_server_check = 1;

  int optind = 0;
  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, progname, "");
    exit(1);
  }

  if (_print_version) {
    ndbPrintVersion();
    exit(0);
  }

  if(glob.config_filename == NULL) {
    fprintf(stderr, "No configuration file specified\n");
    exit(1);
  }
  glob.socketServer = new SocketServer();
  MgmApiService * mapi = new MgmApiService();
  MgmStatService * mstat = new MgmStatService();

  /****************************
   * Read configuration files *
   ****************************/
  if (!readLocalConfig())
    goto error_end;
  if (!readGlobalConfig())
    goto error_end;

  if (!setPortNo())
    goto error_end;
  
  if(!glob.use_specific_ip){
    if(!glob.socketServer->tryBind(glob.port, glob.interface_name)){
      ndbout_c("Unable to setup port: %s:%d!\n"
	       "Please check if the port is already used,\n"
	       "(perhaps a mgmtsrvr is already running),\n"
	       "and if you are executing on the correct computer", 
	       glob.interface_name, glob.port);
      goto error_end;
    }
    free(glob.interface_name);
    glob.interface_name = 0;
  }

  if(!glob.socketServer->setup(mapi, glob.port, glob.interface_name)){
    ndbout_c("Unable to setup management port: %d!\n"
	     "Please check if the port is already used,\n"
	     "(perhaps a mgmtsrvr is already running),\n"
	     "and if you are executing on the correct computer", 
	     glob.port);
    delete mapi;
    goto error_end;
  }
  
  if(!glob.socketServer->setup(mstat, glob.port_stats, glob.interface_name)){
    ndbout_c("Unable to setup statistic port: %d!\nPlease check if the port"
	     " is already used.", glob.port_stats);
    delete mstat;
    goto error_end;
  }

  glob.mgmObject = new MgmtSrvr(glob.localNodeId,
				BaseString(glob.config_filename),
				BaseString(glob.local_config_filename == 0 ? "" : glob.local_config_filename),
				glob.cluster_config);
  
  glob.cluster_config = 0;

  if(!glob.mgmObject->check_start()){
    ndbout_c("Unable to start management server.");
    ndbout_c("Probably caused by illegal initial configuration file.");
    goto error_end;
  }

  if (glob.daemon) {
    // Become a daemon
    char homePath[255],lockfile[255], logfile[255];
    NdbConfig_HomePath(homePath, 255);
    snprintf(lockfile, 255, "%snode%d.pid", homePath, glob.localNodeId);
    snprintf(logfile, 255, "%snode%d.out", homePath, glob.localNodeId);
    if (NdbDaemon_Make(lockfile, logfile, 0) == -1) {
      ndbout << "Cannot become daemon: " << NdbDaemon_ErrorText << endl;
      return 1;
    }
  }

  if(!glob.mgmObject->start()){
    ndbout_c("Unable to start management server.");
    ndbout_c("Probably caused by illegal initial configuration file.");
    goto error_end;
  }

  //glob.mgmObject->saveConfig();

  mstat->setMgm(glob.mgmObject);
  mapi->setMgm(glob.mgmObject);
  glob.mgmObject->setStatisticsListner(mstat);

  char msg[256];
  snprintf(msg, sizeof(msg),
	   "NDB Cluster Management Server. %s", NDB_VERSION_STRING);
  ndbout_c(msg);
  g_EventLogger.info(msg);

  snprintf(msg, 256, "Command port: %d, Statistics port: %d",
	   glob.port, glob.port_stats);
  ndbout_c(msg);
  g_EventLogger.info(msg);
  
  g_StopServer = false;
  glob.socketServer->startServer();

#if ! defined NDB_OSE && ! defined NDB_SOFTOSE
  if(!glob.daemon && !glob.non_interactive){
    CommandInterpreter com(* glob.mgmObject);
    while(com.readAndExecute());
  } else 
#endif
    {
      while(g_StopServer != true)
	NdbSleep_MilliSleep(500);
    }
  
  glob.socketServer->stopServer();
  glob.socketServer->stopSessions();
  
  return 0;
 error_end:
  return 1;
}

MgmGlobals::MgmGlobals(){
  // Default values
  port = 0;
  port_stats = 0;
  config_filename = NULL;
  local_config_filename = NULL;
  interface_name = 0;
  cluster_config = 0;
  daemon = false;
  non_interactive = 0;
  socketServer = 0;
  mgmObject = 0;
}

MgmGlobals::~MgmGlobals(){
  if (socketServer)
    delete socketServer;
  if (mgmObject)
    delete mgmObject;
  if (cluster_config) 
    delete cluster_config;
  if (interface_name)
    free(interface_name);
}

/**
 * @fn      readLocalConfig
 * @param   glob : Global variables
 * @return  true if success, false otherwise.
 *
 * How to get LOCAL CONFIGURATION FILE:
 * 1. Use local config file name (-l)
 * 2. Use environment NDB_HOME + Ndb.cfg
 *    If NDB_HOME is not set this results in reading from local dir
 */
static bool
readLocalConfig(){
  // Read local config file
  ConfigRetriever cr;
  cr.setLocalConfigFileName(glob.local_config_filename);
  int nodeid = cr.init(true);
  if(nodeid == -1){
    return false;
  }
  
  glob.localNodeId = (NodeId)nodeid;
  return true;
}


/**
 * @fn      readGlobalConfig
 * @param   glob : Global variables
 * @return  true if success, false otherwise.
 *
 * How to get the GLOBAL CONFIGURATION:
 * 1. Use config file name (this is a text file)(-c)
 * 2. Use name from line 2 of local config file, ex: file:///c/ndb/Ndb_cfg.bin
 */
static bool
readGlobalConfig() {
  if(glob.config_filename == NULL)
    return false;

  /* Use config file */
  InitConfigFileParser parser;
  glob.cluster_config = parser.parseConfig(glob.config_filename);
  if(glob.cluster_config == 0){
    /**
     * Try to get configuration from other MGM server 
     * Note: Only new format
     */
    glob.cluster_config = new Config();
    
    ConfigRetriever cr;
    cr.setLocalConfigFileName(glob.local_config_filename);
    glob.cluster_config->m_configValues = cr.getConfig(NDB_VERSION,
						       NODE_TYPE_MGM);
    if (glob.cluster_config->m_configValues == NULL)
      return false;
  }
  return true;
}

/**
 * @fn      setPortNo
 * @param   glob : Global variables
 * @return  true if success, false otherwise.
 *
 * Port number:
 * 2. Use port number from global configuration file
 * 4. Use port number for statistics from global configuration file 
 */
static bool
setPortNo(){
  const Properties *mgmProps;
  
  ndb_mgm_configuration_iterator * iter = 
    ndb_mgm_create_configuration_iterator(glob.cluster_config->m_configValues, 
					  CFG_SECTION_NODE);
  if(iter == 0)
    return false;

  if(ndb_mgm_find(iter, CFG_NODE_ID, glob.localNodeId) != 0){
    ndbout << "Could not retrieve configuration for Node " 
	   << glob.localNodeId << " in config file." << endl 
	   << "Have you set correct NodeId for this node?" << endl;
    ndb_mgm_destroy_iterator(iter);
    return false;
  }

  unsigned type;
  if(ndb_mgm_get_int_parameter(iter, CFG_TYPE_OF_SECTION, &type) != 0 ||
     type != NODE_TYPE_MGM){
    ndbout << "Local node id " << glob.localNodeId 
	   << " is not defined as management server" << endl
	   << "Have you set correct NodeId for this node?" << endl;
    return false;
  }
  
  /************
   * Set Port *
   ************/
  Uint32 tmp = 0;
  if(ndb_mgm_get_int_parameter(iter, CFG_MGM_PORT, &tmp) != 0){
    ndbout << "Could not find PortNumber in the configuration file." << endl;
    return false;
  }
  glob.port = tmp;    
  
  /*****************
   * Set Stat Port *
   *****************/
#if 0
  if (!mgmProps->get("PortNumberStats", &tmp)){
    ndbout << "Could not find PortNumberStats in the configuration file." 
	   << endl;
    return false;
  }
  glob.port_stats = tmp;
#endif

#if 0
  const char * host;
  if(ndb_mgm_get_string_parameter(iter, mgmProps->get("ExecuteOnComputer", host)){
    ndbout << "Failed to find \"ExecuteOnComputer\" for my node" << endl;
    ndbout << "Unable to verify own hostname" << endl;
    return false;
  }

  const char * hostname;
  {
    const Properties * p;
    char buf[255];
    snprintf(buf, sizeof(buf), "Computer_%s", host.c_str());
    if(!glob.cluster_config->get(buf, &p)){
      ndbout << "Failed to find computer " << host << " in config" << endl;
      ndbout << "Unable to verify own hostname" << endl;
      return false;
    }
    if(!p->get("HostName", &hostname)){
      ndbout << "Failed to find \"HostName\" for computer " << host 
	     << " in config" << endl;
      ndbout << "Unable to verify own hostname" << endl;
      return false;
    }
    if(NdbHost_GetHostName(buf) != 0){
      ndbout << "Unable to get own hostname" << endl;
      ndbout << "Unable to verify own hostname" << endl;
      return false;
    }
  }
  
  const char * ip_address;
  if(mgmProps->get("IpAddress", &ip_address)){
    glob.use_specific_ip = true;
    glob.interface_name = strdup(ip_address);
    return true;
  }
  
  glob.interface_name = strdup(hostname);
#endif

  glob.interface_name = 0;  
  glob.use_specific_ip = false;

  return true;
}
