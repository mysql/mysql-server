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
#include <my_sys.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include <Config.hpp>
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

#include <NdbAutoPtr.hpp>

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
  int interactive;
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

/**
 * Global variables
 */
bool g_StopServer;
extern EventLogger g_EventLogger;

extern int global_mgmt_server_check;
int _print_version = 0;
#ifndef DBUG_OFF
const char *debug_option= 0;
#endif

struct getargs args[] = {
  { "version", 'v', arg_flag, &_print_version,
    "Print ndb_mgmd version"},
  { "config-file", 'c', arg_string, &glob.config_filename,
    "Specify cluster configuration file", "filename" },
#ifndef DBUG_OFF
  { "debug", 0, arg_string, &debug_option,
    "Specify debug options e.g. d:t:i:o,out.trace", "options" },
#endif
  { "daemon", 'd', arg_flag, &glob.daemon,
    "Run ndb_mgmd in daemon mode (default)" },
  { NULL, 'l', arg_string, &glob.local_config_filename,
    "Specify configuration file connect string (will default use Ndb.cfg if available)",
    "filename" },
  { "interactive", 0, arg_flag, &glob.interactive,
    "Run interactive. Not supported but provided for testing purposes", "" },
  { "nodaemon", 'n', arg_flag, &glob.non_interactive,
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

  if (glob.interactive ||
      glob.non_interactive) {
    glob.daemon= 0;
  }

#ifndef DBUG_OFF
  my_init();
  if (debug_option)
    DBUG_PUSH(debug_option);
#endif

  if (_print_version) {
    ndbPrintVersion();
    exit(0);
  }

  if(glob.config_filename == NULL) {
    glob.config_filename= "config.ini";
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

  glob.mgmObject = new MgmtSrvr(glob.localNodeId,
				BaseString(glob.config_filename),
				BaseString(glob.local_config_filename == 0 ?
					   "" : glob.local_config_filename),
				glob.cluster_config);

  chdir(NdbConfig_get_path(0));

  glob.cluster_config = 0;
  glob.localNodeId= glob.mgmObject->getOwnNodeId();

  if (glob.localNodeId == 0) {
    goto error_end;
  }

  glob.port= glob.mgmObject->getPort();

  if (glob.port == 0)
    goto error_end;

  glob.interface_name = 0;
  glob.use_specific_ip = false;

  if(!glob.use_specific_ip){
    int count= 5; // no of retries for tryBind
    while(!glob.socketServer->tryBind(glob.port, glob.interface_name)){
      if (--count > 0) {
	NdbSleep_MilliSleep(1000);
	continue;
      }
      ndbout_c("Unable to setup port: %s:%d!\n"
	       "Please check if the port is already used,\n"
	       "(perhaps a ndb_mgmd is already running),\n"
	       "and if you are executing on the correct computer", 
	       (glob.interface_name ? glob.interface_name : "*"), glob.port);
      goto error_end;
    }
    free(glob.interface_name);
    glob.interface_name = 0;
  }

  if(!glob.socketServer->setup(mapi, glob.port, glob.interface_name)){
    ndbout_c("Unable to setup management port: %d!\n"
	     "Please check if the port is already used,\n"
	     "(perhaps a ndb_mgmd is already running),\n"
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

  if(!glob.mgmObject->check_start()){
    ndbout_c("Unable to check start management server.");
    ndbout_c("Probably caused by illegal initial configuration file.");
    goto error_end;
  }

  if (glob.daemon) {
    // Become a daemon
    char *lockfile= NdbConfig_PidFileName(glob.localNodeId);
    char *logfile=  NdbConfig_StdoutFileName(glob.localNodeId);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

    if (NdbDaemon_Make(lockfile, logfile, 0) == -1) {
      ndbout << "Cannot become daemon: " << NdbDaemon_ErrorText << endl;
      return 1;
    }
  }

  signal(SIGPIPE, SIG_IGN);
  {
    BaseString error_string;
    if(!glob.mgmObject->start(error_string)){
      ndbout_c("Unable to start management server.");
      ndbout_c("Probably caused by illegal initial configuration file.");
      ndbout_c(error_string.c_str());
      goto error_end;
    }
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

  snprintf(msg, 256, "Id: %d, Command port: %d, Statistics port: %d",
	   glob.localNodeId, glob.port, glob.port_stats);
  ndbout_c(msg);
  g_EventLogger.info(msg);
  
  g_StopServer = false;
  glob.socketServer->startServer();

#if ! defined NDB_OSE && ! defined NDB_SOFTOSE
  if(glob.interactive) {
    CommandInterpreter com(* glob.mgmObject);
    while(com.readAndExecute());
  } else 
#endif
    {
      while(g_StopServer != true)
	NdbSleep_MilliSleep(500);
    }
  
  g_EventLogger.info("Shutting down server...");
  glob.socketServer->stopServer();
  glob.socketServer->stopSessions();
  g_EventLogger.info("Shutdown complete");
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
  daemon = 1;
  non_interactive = 0;
  interactive = 0;
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
  LocalConfig lc;
  if(!lc.init(glob.local_config_filename))
    return false;
  
  glob.localNodeId = lc._ownNodeId;
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
    return false;
  }
  return true;
}
