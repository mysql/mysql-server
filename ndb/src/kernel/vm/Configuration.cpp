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

#include "Configuration.hpp"
#include <ErrorHandlingMacros.hpp>
#include "GlobalData.hpp"

#include <ConfigRetriever.hpp>
#include <IPCConfig.hpp>
#include <ndb_version.h>
#include <NdbMem.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>

#include <getarg.h>

extern "C" {
  void ndbSetOwnVersion();
}

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

bool
Configuration::init(int argc, const char** argv){

  /**
   * Default values for arguments
   */
  int _start = 1;
  int _initial = 0;
  const char* _connect_str = NULL;
  int _deamon = 0;
  int _help = 0;
  int _print_version = 0;
  
  /**
   * Arguments to NDB process
   */ 
  struct getargs args[] = {
    { "version", 'v', arg_flag, &_print_version, "Print version", "" },
    { "start", 's', arg_flag, &_start, "Start ndb immediately", "" },
    { "nostart", 'n', arg_negative_flag, &_start, "Don't start ndb immediately", "" },
    { "deamon", 'd', arg_flag, &_deamon, "Start ndb as deamon", "" },
    { "initial", 'i', arg_flag, &_initial, "Start ndb immediately", "" },

    { "connect-string", 'c', arg_string, &_connect_str, "\"nodeid=<id>;host=<hostname:port>\"\n", "constr" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "The MySQL Cluster kernel";
  
  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return false;
  }

#if 0  
  ndbout << "start=" <<_start<< endl;
  ndbout << "initial=" <<_initial<< endl;
  ndbout << "deamon=" <<_deamon<< endl;
  ndbout << "connect_str="<<_connect_str<<endl;
  arg_printusage(args, num_args, argv[0], desc);
  return false;
#endif

  ndbSetOwnVersion();

  if (_print_version) {
    ndbPrintVersion();
    return false;
  }

  // Check the start flag
  if (_start)
    globalData.theRestartFlag = perform_start;
  else 
    globalData.theRestartFlag = initial_state;

  // Check the initial flag
  if (_initial)
    _initialStart = true;
  
  // Check connectstring
  if (_connect_str){

    if(_connect_str[0] == '-' || 
       strstr(_connect_str, "host") == 0 || 
       strstr(_connect_str, "nodeid") == 0) {
      ndbout << "Illegal/empty connectString: " << _connect_str << endl;
      arg_printusage(args, num_args, argv[0], desc);
      return false;
    } 
    _connectString = strdup(_connect_str);
  } 
  
  // Check deamon flag
  if (_deamon)
    _daemonMode = true;

  // Save programname
  if(argc > 0 && argv[0] != 0)
    _programName = strdup(argv[0]);
  else
    _programName = strdup("");
  
  return true;
}

Configuration::Configuration():
  the_clusterConfigurationData()
{
  m_ownProperties = 0;
  _programName = 0;
  _connectString = 0;
  _fsPath = 0;
  _initialStart = false;
  _daemonMode = false;
}

Configuration::~Configuration(){
  delete m_ownProperties;
  
  if(_programName != NULL)
    free(_programName);

  if(_fsPath != NULL)
    free(_fsPath);
}

const
ClusterConfiguration& 
Configuration::clusterConfiguration() const {
  return the_clusterConfigurationData;
}

void
Configuration::setupConfiguration(){
  /**
   * Fetch configuration from management server
   */
  ConfigRetriever cr;
  cr.setConnectString(_connectString);
  stopOnError(true); 
  Properties * p = cr.getConfig("DB", NDB_VERSION);
  if(p == 0){
    const char * s = cr.getErrorString();
    if(s == 0)
      s = "No error given!";

    /* Set stop on error to true otherwise NDB will
       go into an restart loop...
     */

    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Could not fetch configuration"
	      "/invalid configuration", s);
  }

  /**
   * Configure transporters
   */
  {  
    IPCConfig * theIPC = new IPCConfig(p);
    
    if(theIPC->init() != 0){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", "");
    }
    
    if(theIPC->configureTransporters(&globalTransporterRegistry) <= 0){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"No transporters configured");
    }
    
    globalData.ownId = theIPC->ownId();
    delete theIPC;
  }

  /**
   * Setup cluster configuration data
   */
  const Properties * db = 0;
  if (!p->get("Node", globalData.ownId, &db)) {
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }
  const char * type;
  if(!(db->get("Type", &type) && strcmp(type, "DB") == 0)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched",
	      "I'm wrong type of node");
  }
  
  /**
   * Save properties object to use in getOwnProperties()
   */
  m_ownProperties = new Properties(* db);

  the_clusterConfigurationData.init(* p, * db);
  
  if(!db->get("MaxNoOfSavedMessages", &_maxErrorLogs)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "MaxNoOfSavedMessages missing");
  }
  
  if(!db->get("LockPagesInMainMemory", &_lockPagesInMainMemory)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "LockPagesInMainMemory missing");
  }

  if(!db->get("TimeBetweenWatchDogCheck", &_timeBetweenWatchDogCheck)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "TimeBetweenWatchDogCheck missing");
  }

  /**
   * Get filesystem path
   */  
  { 
    const char* pFileSystemPath = NULL;
    if(!db->get("FileSystemPath", &pFileSystemPath)){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"FileSystemPath missing");
    } 

    if(pFileSystemPath == 0 || strlen(pFileSystemPath) == 0){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"Configuration does not contain valid filesystem path");
    }

    if(pFileSystemPath[strlen(pFileSystemPath) - 1] == '/')
      _fsPath = strdup(pFileSystemPath);
    else {
      _fsPath = (char *)malloc(strlen(pFileSystemPath) + 2);
      strcpy(_fsPath, pFileSystemPath);
      strcat(_fsPath, "/");
    }
  }

  if(!db->get("StopOnError", &_stopOnError)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "StopOnError missing");
  }

  if(!db->get("RestartOnErrorInsert", &m_restartOnErrorInsert)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "RestartOnErrorInsert missing");
  }

  delete p;

  /**
   * Create the watch dog thread
   */
  { 
    Uint32 t = _timeBetweenWatchDogCheck;
    t = globalEmulatorData.theWatchDog ->setCheckInterval(t);
    _timeBetweenWatchDogCheck = t;
  }

}

bool 
Configuration::lockPagesInMainMemory() const {
  return _lockPagesInMainMemory;
}

int 
Configuration::timeBetweenWatchDogCheck() const {
  return _timeBetweenWatchDogCheck;
}

const 
ClusterConfiguration::ClusterData&
Configuration::clusterConfigurationData() const {
  return the_clusterConfigurationData.clusterData();
}

void 
Configuration::timeBetweenWatchDogCheck(int value) {
  _timeBetweenWatchDogCheck = value;
}

int 
Configuration::maxNoOfErrorLogs() const {
  return _maxErrorLogs;
}

void 
Configuration::maxNoOfErrorLogs(int val){
  _maxErrorLogs = val;
}

bool
Configuration::stopOnError() const {
  return _stopOnError;
}

void 
Configuration::stopOnError(bool val){
  _stopOnError = val;
}

const Properties * 
Configuration::getOwnProperties() const {
  return m_ownProperties;
}

int
Configuration::getRestartOnErrorInsert() const {
  return m_restartOnErrorInsert;
}

void
Configuration::setRestartOnErrorInsert(int i){
  m_restartOnErrorInsert = i;
}

char *
Configuration::getConnectStringCopy() const {
  if(_connectString != 0)
    return strdup(_connectString);
  return 0;
}

void
Configuration::setInitialStart(bool val){
  _initialStart = val;
}
