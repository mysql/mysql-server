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

#include "CommandInterpreter.hpp"

#include <mgmapi.h>
#include <mgmapi_debug.h>
#include <version.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/GrepImpl.hpp>
#ifdef HAVE_GLOBAL_REPLICATION

#endif // HAVE_GLOBAL_REPLICATION
#include "MgmtErrorReporter.hpp"
#include "CpcClient.hpp"


/*****************************************************************************
 * HELP
 *****************************************************************************/
static const char* helpText =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help\n"
"---------------------------------------------------------------------------\n"
"HELP                                   Print help text\n"
"HELP SHOW                              Help for SHOW command\n"
#ifdef HAVE_GLOBAL_REPLICATION
"HELP REPLICATION                       Help for global replication\n"
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
"HELP DEBUG                             Help for debug compiled version\n"
#endif
"SHOW                                   Print information about cluster\n"
"SHOW CONFIG                            Print configuration\n"
"SHOW PARAMETERS                        Print configuration parameters\n"
"START BACKUP                           Start backup\n"
"ABORT BACKUP <backup id>               Abort backup\n"
"CLUSTERLOG ON                          Enable Cluster logging\n"
"CLUSTERLOG OFF                         Disable Cluster logging\n"
"CLUSTERLOG FILTER <severity>           Toggle severity filter on/off\n"
"CLUSTERLOG INFO                        Print cluster log information\n"
"<id> START                             Start DB node (started with -n)\n"
"<id> RESTART [-n] [-i]                 Restart DB node\n"
"<id> STOP                              Stop DB node\n"
"ENTER SINGLE USER MODE <api-node>      Enter single user mode\n"
"EXIT SINGLE USER MODE                  Exit single user mode\n"
"<id> STATUS                            Print status\n"
"<id> CLUSTERLOG {<category>=<level>}+  Set log level for cluster log\n"
"REP CONNECT <host:port>                Connect to REP server on host:port\n"
"QUIT                                   Quit management client\n"
;

static const char* helpTextShow =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for SHOW command\n"
"---------------------------------------------------------------------------\n"
"SHOW prints NDB Cluster information\n\n"
"SHOW               Print information about cluster\n" 
"SHOW CONFIG        Print configuration (in initial config file format)\n" 
"SHOW PARAMETERS    Print information about configuration parameters\n\n"
;

#ifdef HAVE_GLOBAL_REPLICATION
static const char* helpTextRep =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Global Replication\n"
"---------------------------------------------------------------------------\n"
"Commands should be executed on the standby NDB Cluster\n"
"These features are in an experimental release state.\n"
"\n"
"Simple Commands:\n"
"REP START              Start Global Replication\n" 
"REP START REQUESTOR    Start Global Replication Requestor\n" 
"REP STATUS             Show Global Replication status\n" 
"REP STOP               Stop Global Replication\n"
"REP STOP REQUESTOR     Stop Global Replication Requestor\n"
"\n" 
"Advanced Commands:\n"
"REP START <protocol>   Starts protocol\n"
"REP STOP <protocol>    Stops protocol\n"
"<protocol> = TRANSFER | APPLY | DELETE\n"
"\n"
#ifdef VM_TRACE // DEBUG ONLY
"Debugging commands:\n"
"REP DELETE             Removes epochs stored in primary and standy systems\n"
"REP DROP <tableid>     Drop a table in SS identified by table id\n"
"REP SLOWSTOP           Stop Replication (Tries to synchonize with primary)\n" 
"REP FASTSTOP           Stop Replication (Stops in consistent state)\n" 
"<component> = SUBSCRIPTION\n"
"              METALOG | METASCAN | DATALOG | DATASCAN\n"
"              REQUESTOR | TRANSFER | APPLY | DELETE\n"
#endif
;
#endif // HAVE_GLOBAL_REPLICATION

#ifdef VM_TRACE // DEBUG ONLY
static const char* helpTextDebug =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Debugging (Internal use only)\n"
"---------------------------------------------------------------------------\n"
"SHOW PROPERTIES                       Print config properties object\n"
"<id> LOGLEVEL {<category>=<level>}+   Set log level\n"
#ifdef ERROR_INSERT
"<id> ERROR <errorNo>                  Inject error into NDB node\n"
#endif
"<id> TRACE <traceNo>                  Set trace number\n"
"<id> LOG [BLOCK = {ALL|<block>+}]     Set logging on in & out signals\n"
"<id> LOGIN [BLOCK = {ALL|<block>+}]   Set logging on in signals\n"
"<id> LOGOUT [BLOCK = {ALL|<block>+}]  Set logging on out signals\n"
"<id> LOGOFF [BLOCK = {ALL|<block>+}]  Unset signal logging\n"
"<id> TESTON                           Start signal logging\n"
"<id> TESTOFF                          Stop signal logging\n"
"<id> SET <configParamName> <value>    Update configuration variable\n"
"<id> DUMP <arg>                       Dump system state to cluster.log\n"
"<id> GETSTAT                          Print statistics\n"
"\n"
"<id>       = ALL | Any database node id\n"
;
#endif

static bool
convert(const char* s, int& val) {
  
  if (s == NULL)
    return false;

  if (strlen(s) == 0)
    return false;

  errno = 0;
  char* p;
  long v = strtol(s, &p, 10);
  if (errno != 0)
    return false;

  if (p != &s[strlen(s)])
    return false;
  
  val = v;
  return true;
}

/*
 * Constructor
 */
CommandInterpreter::CommandInterpreter(const char *_host) 
{
  m_mgmsrv = ndb_mgm_create_handle();
  if(m_mgmsrv == NULL) {
    ndbout_c("Cannot create handle to management server.");
    printError();
  }

  connected = false;
  try_reconnect = 0;

  host = strdup(_host);
#ifdef HAVE_GLOBAL_REPLICATION
  rep_host = NULL;
  m_repserver = NULL;
  rep_connected = false;
#endif
}

/*
 * Destructor
 */
CommandInterpreter::~CommandInterpreter() 
{
  connected = false;
  ndb_mgm_destroy_handle(&m_mgmsrv);
  free((char *)host);
  host = NULL;
}

bool 
emptyString(const char* s) 
{
  if (s == NULL) {
    return true;
  }

  for (unsigned int i = 0; i < strlen(s); ++i) {
    if (! isspace(s[i])) {
      return false;
    }
  }

  return true;
}

class AutoPtr 
{
public:
  AutoPtr(void * ptr) : m_ptr(ptr) {}
  ~AutoPtr() { free(m_ptr);}
private:
  void * m_ptr;
};


void
CommandInterpreter::printError() 
{
  ndbout_c("* %5d: %s", 
	   ndb_mgm_get_latest_error(m_mgmsrv),
	   ndb_mgm_get_latest_error_msg(m_mgmsrv));
  ndbout_c("*        %s", ndb_mgm_get_latest_error_desc(m_mgmsrv));
}

//*****************************************************************************
//*****************************************************************************

bool 
CommandInterpreter::connect() 
{
  if(!connected) {
    int tries = try_reconnect; // tries == 0 => infinite
    while(!connected) {
      if(ndb_mgm_connect(m_mgmsrv, host) == -1) {
	ndbout << "Cannot connect to management server (" << host << ").";
	tries--;
	if (tries == 0)
	  break;
	ndbout << "Retrying in 5 seconds." << endl;
	NdbSleep_SecSleep(5);
      } else
	connected = true;
    }
  }
  return connected;
}

bool 
CommandInterpreter::disconnect() 
{
  if (ndb_mgm_disconnect(m_mgmsrv) == -1) {
    ndbout_c("Could not disconnect from management server");
    printError();
  }
  connected = false;
  return true;
}

//*****************************************************************************
//*****************************************************************************

int 
CommandInterpreter::readAndExecute(int _try_reconnect) 
{
  if (_try_reconnect >= 0)
    try_reconnect=_try_reconnect;

  char* _line = readline_gets(); 
  char * line;
  if(_line == NULL) {
    //   ndbout << endl;
    return false;
  }

  line = strdup(_line);
  
  AutoPtr ptr(line);
  
  if (emptyString(line)) {
    return true;
  }
  
  for (unsigned int i = 0; i < strlen(line); ++i) {
    line[i] = toupper(line[i]);
  }

  // if there is anything in the line proceed
  char* firstToken = strtok(line, " ");
  char* allAfterFirstToken = strtok(NULL, "");
  
  if (strcmp(firstToken, "HELP") == 0 ||
      strcmp(firstToken, "?") == 0) {
    executeHelp(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "SHOW") == 0) {
    executeShow(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "CLUSTERLOG") == 0){
    executeClusterLog(allAfterFirstToken);
    return true;
  }
  else if(strcmp(firstToken, "START") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeStartBackup(allAfterFirstToken);
    return true;
  }
  else if(strcmp(firstToken, "ABORT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeAbortBackup(allAfterFirstToken);
    return true;
  }
#ifdef HAVE_GLOBAL_REPLICATION
  else if(strcmp(firstToken, "REPLICATION") == 0 ||
	  strcmp(firstToken, "REP") == 0) {
    executeRep(allAfterFirstToken);
    return true;
  }
#endif // HAVE_GLOBAL_REPLICATION
  else if(strcmp(firstToken, "ENTER") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeEnterSingleUser(allAfterFirstToken);
    return true;
  }
  else if(strcmp(firstToken, "EXIT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeExitSingleUser(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "ALL") == 0) {
    analyseAfterFirstToken(-1, allAfterFirstToken);
  } 
  else if((strcmp(firstToken, "QUIT") == 0 ||
	  strcmp(firstToken, "EXIT") == 0 ||
	  strcmp(firstToken, "BYE") == 0) && 
	  allAfterFirstToken == NULL){
    return false;
#if 0
  } else if(strcmp(firstToken, "CPC") == 0) {
    executeCpc(allAfterFirstToken);
#endif
  } else {
    /**
     * First token should be a digit, node ID
     */
    int nodeId;

    if (! convert(firstToken, nodeId)) {
      ndbout << "Invalid command: " << line << endl;
      ndbout << "Type HELP for help." << endl << endl;
      return true;
    }

    if (nodeId < 0) {
      ndbout << "Invalid node ID: " << firstToken << "." << endl;
      return true;
    }
    
    analyseAfterFirstToken(nodeId, allAfterFirstToken);
    
  }
  return true;
}


/**
 * List of commands used as second command argument
 */
static const CommandInterpreter::CommandFunctionPair commands[] = {
  { "START", &CommandInterpreter::executeStart }
  ,{ "RESTART", &CommandInterpreter::executeRestart }
  ,{ "STOP", &CommandInterpreter::executeStop }
  ,{ "STATUS", &CommandInterpreter::executeStatus }
  ,{ "LOGLEVEL", &CommandInterpreter::executeLogLevel }
  ,{ "CLUSTERLOG", &CommandInterpreter::executeEventReporting }
#ifdef ERROR_INSERT
  ,{ "ERROR", &CommandInterpreter::executeError }
#endif
  ,{ "TRACE", &CommandInterpreter::executeTrace }
  ,{ "LOG", &CommandInterpreter::executeLog }
  ,{ "LOGIN", &CommandInterpreter::executeLogIn }
  ,{ "LOGOUT", &CommandInterpreter::executeLogOut }
  ,{ "LOGOFF", &CommandInterpreter::executeLogOff }
  ,{ "TESTON", &CommandInterpreter::executeTestOn }
  ,{ "TESTOFF", &CommandInterpreter::executeTestOff }
  ,{ "SET", &CommandInterpreter::executeSet }
  ,{ "GETSTAT", &CommandInterpreter::executeGetStat }
  ,{ "DUMP", &CommandInterpreter::executeDumpState }
};


//*****************************************************************************
//*****************************************************************************
void
CommandInterpreter::analyseAfterFirstToken(int processId,
					   char* allAfterFirstToken) {
  
  if (emptyString(allAfterFirstToken)) {
    if (processId == -1) {
      ndbout << "Expected a command after ALL." << endl;
    }
    else {
      ndbout << "Expected a command after node ID." << endl;
    }
    return;
  }
  
  char* secondToken = strtok(allAfterFirstToken, " ");
  char* allAfterSecondToken = strtok(NULL, "\0");

  const int tmpSize = sizeof(commands)/sizeof(CommandFunctionPair);
  ExecuteFunction fun = 0;
  const char * command = 0;
  for(int i = 0; i<tmpSize; i++){
    if(strcmp(secondToken, commands[i].command) == 0){
      fun = commands[i].executeFunction;
      command = commands[i].command;
      break;
    }
  }
  
  if(fun == 0){
    ndbout << "Invalid command: " << secondToken << endl;
    ndbout << "Type HELP for help." << endl << endl;
    return;
  }
  
  if(processId == -1){
    executeForAll(command, fun, allAfterSecondToken);
  } else {
    if(strcmp(command, "STATUS") != 0)
      ndbout_c("Executing %s on node %d.", command, processId);
    (this->*fun)(processId, allAfterSecondToken, false);
    ndbout << endl;
  }
}

/**
 * Get next nodeid larger than the give node_id. node_id will be
 * set to the next node_id in the list. node_id should be set
 * to 0 (zero) on the first call.
 *
 * @param handle the NDB management handle
 * @param node_id last node_id retreived, 0 at first call
 * @param type type of node to look for
 * @return 1 if a node was found, 0 if no more node exist
 */
static 
int 
get_next_nodeid(struct ndb_mgm_cluster_state *cl,
		int *node_id,
		enum ndb_mgm_node_type type)
{
  int i;
  
  if(cl == NULL)
    return 0;
  
  i=0;
  while((i < cl->no_of_nodes)) {
    if((*node_id < cl->node_states[i].node_id) &&
       (cl->node_states[i].node_type == type)) {
      
      if(i >= cl->no_of_nodes)
	return 0;
      
      *node_id = cl->node_states[i].node_id;
      return 1;
    }
    i++;
  }
  
  return 0;
}

void
CommandInterpreter::executeForAll(const char * cmd, ExecuteFunction fun, 
				  const char * allAfterSecondToken)
{
  int nodeId = 0;
  if(strcmp(cmd, "STOP") == 0) {
    ndbout_c("Executing STOP on all nodes.");
    (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if(strcmp(cmd, "RESTART") == 0) {
    ndbout_c("Executing RESTART on all nodes.");
    ndbout_c("Starting shutdown. This may take a while. Please wait...");
    (this->*fun)(nodeId, allAfterSecondToken, true);
    ndbout_c("Trying to start all nodes of system.");
    ndbout_c("Use ALL STATUS to see the system start-up phases.");
  } else {
    connect();
    struct ndb_mgm_cluster_state *cl;
    cl = ndb_mgm_get_status(m_mgmsrv);
    if(cl == 0){
      ndbout_c("Unable get status from management server");
      printError();
      return;
    }
    while(get_next_nodeid(cl, &nodeId, NDB_MGM_NODE_TYPE_NDB)) {
      if(strcmp(cmd, "STATUS") != 0)
	ndbout_c("Executing %s on node %d.", cmd, nodeId);
      (this->*fun)(nodeId, allAfterSecondToken, true);
      ndbout << endl;
    } // while
    free(cl);
  }
}

//*****************************************************************************
//*****************************************************************************
bool 
CommandInterpreter::parseBlockSpecification(const char* allAfterLog,
					    Vector<const char*>& blocks) 
{
  // Parse: [BLOCK = {ALL|<blockName>+}]

  if (emptyString(allAfterLog)) {
    return true;
  }

  // Copy allAfterLog since strtok will modify it  
  char* newAllAfterLog = strdup(allAfterLog);
  char* firstTokenAfterLog = strtok(newAllAfterLog, " ");
  for (unsigned int i = 0; i < strlen(firstTokenAfterLog); ++i) {
    firstTokenAfterLog[i] = toupper(firstTokenAfterLog[i]);
  }
  
  if (strcmp(firstTokenAfterLog, "BLOCK") != 0) {
    ndbout << "Unexpected value: " << firstTokenAfterLog 
	   << ". Expected BLOCK." << endl;
    free(newAllAfterLog);
    return false;
  }

  char* allAfterFirstToken = strtok(NULL, "\0");
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected =." << endl;
    free(newAllAfterLog);
    return false;
  }

  char* secondTokenAfterLog = strtok(allAfterFirstToken, " ");
  if (strcmp(secondTokenAfterLog, "=") != 0) {
    ndbout << "Unexpected value: " << secondTokenAfterLog 
	   << ". Expected =." << endl;
    free(newAllAfterLog);
    return false;
  }

  char* blockName = strtok(NULL, " ");
  bool all = false;
  if (blockName != NULL && (strcmp(blockName, "ALL") == 0)) {
    all = true;
  }
  while (blockName != NULL) {
    blocks.push_back(strdup(blockName));
    blockName = strtok(NULL, " ");
  }

  if (blocks.size() == 0) {
    ndbout << "No block specified." << endl;
    free(newAllAfterLog);
    return false;
  }
  if (blocks.size() > 1 && all) {
    // More than "ALL" specified
    ndbout << "Nothing expected after ALL." << endl;
    free(newAllAfterLog);
    return false;
  }
  
  free(newAllAfterLog);
  return true;
}



/*****************************************************************************
 * HELP
 *****************************************************************************/
void 
CommandInterpreter::executeHelp(char* parameters)
{
  if (emptyString(parameters)) {
    ndbout << helpText;

    ndbout << endl 
	   << "<severity> = " 
	   << "ALERT | CRITICAL | ERROR | WARNING | INFO | DEBUG"
	   << endl;

    ndbout << "<category> = ";
    for(Uint32 i = 0; i<EventLogger::noOfEventCategoryNames; i++){
      ndbout << EventLogger::eventCategoryNames[i].name;
      if (i < EventLogger::noOfEventCategoryNames - 1) {
	ndbout << " | ";
      }
    }
    ndbout << endl;

    ndbout << "<level>    = " << "0 - 15" << endl;
    ndbout << "<id>       = " << "ALL | Any database node id" << endl;
    ndbout << endl;
  } else if (strcmp(parameters, "SHOW") == 0) {
    ndbout << helpTextShow;
#ifdef HAVE_GLOBAL_REPLICATION
  } else if (strcmp(parameters, "REPLICATION") == 0 ||
	     strcmp(parameters, "REP") == 0) {
    ndbout << helpTextRep;
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
  } else if (strcmp(parameters, "DEBUG") == 0) {
    ndbout << helpTextDebug;
#endif
  } else {
    ndbout << "Invalid argument: " << parameters << endl;
    ndbout << "Type HELP for help." << endl << endl;
  }
}


/*****************************************************************************
 * SHOW
 *****************************************************************************/

void
CommandInterpreter::executeShow(char* parameters) 
{ 
  int i;
  connect();
  if (emptyString(parameters)) {
    ndbout << "Cluster Configuration" << endl
	   << "---------------------" << endl;
    
    ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
    if(state == NULL) {
      ndbout_c("Could not get status");
      printError();
      return;
    }

    int
      master_id= 0,
      ndb_nodes= 0,
      api_nodes= 0,
      mgm_nodes= 0;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB &&
	 state->node_states[i].version != 0){
	master_id= state->node_states[i].dynamic_id;
	break;
      }
    }
    
    for(i=0; i < state->no_of_nodes; i++) {
      switch(state->node_states[i].node_type) {
      case NDB_MGM_NODE_TYPE_API:
	api_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_NDB:
	if (state->node_states[i].dynamic_id < master_id)
	  master_id= state->node_states[i].dynamic_id;
	ndb_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_MGM:
	mgm_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_UNKNOWN:
        ndbout << "Error: Unknown Node Type" << endl;
        return;
      }
    }

    ndbout << ndb_nodes
	   << " NDB Node(s)" 
	   << endl;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB) {
	ndbout << "DB node:\t" << state->node_states[i].node_id;
	if(state->node_states[i].version != 0) {
	  ndbout << "  (Version: "
		 << getMajor(state->node_states[i].version) << "."
		 << getMinor(state->node_states[i].version) << "."
		 << getBuild(state->node_states[i].version) << ","
		 << " Nodegroup: " << state->node_states[i].node_group;
	  if (state->node_states[i].dynamic_id == master_id)
	    ndbout << ", Master";
	  ndbout << ")" << endl;
	} else
	  {
	    ndbout << "  (not connected) " << endl;
	  }

      }
    }
    ndbout << endl;
    
    ndbout << mgm_nodes
	   << " MGM Node(s)" 
	   << endl;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_MGM) {
	ndbout << "MGM node:\t" << state->node_states[i].node_id;
	if(state->node_states[i].version != 0) {
	  ndbout << "  (Version: "
		 << getMajor(state->node_states[i].version) << "."
		 << getMinor(state->node_states[i].version) << "."
		 << getBuild(state->node_states[i].version) << ")" << endl;
	  
	} else
	  {
	    ndbout << "  (no version information available) " << endl;
	  }
      }
    }
    ndbout << endl;

    ndbout << api_nodes
	   << " API Node(s)" 
	   << endl;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_API) {
	ndbout << "API node:\t" << state->node_states[i].node_id;
	if(state->node_states[i].version != 0) {
	  ndbout << "  (Version: "
		 << getMajor(state->node_states[i].version) << "."
		 << getMinor(state->node_states[i].version) << "."
		 << getBuild(state->node_states[i].version) << ")" << endl;
	  
	} else
	  {
	    ndbout << "  (not connected) " << endl;
	  }
      }
    }
    ndbout << endl;
    
    //    ndbout << helpTextShow;
    return;
  } else if (strcmp(parameters, "PROPERTIES") == 0 ||
	     strcmp(parameters, "PROP") == 0) {
    ndbout << "SHOW PROPERTIES is not yet implemented." << endl;
    //  ndbout << "_mgmtSrvr.getConfig()->print();" << endl; /* XXX */
  } else if (strcmp(parameters, "CONFIGURATION") == 0 ||
	     strcmp(parameters, "CONFIG") == 0){
    ndbout << "SHOW CONFIGURATION is not yet implemented." << endl;
    //nbout << "_mgmtSrvr.getConfig()->printConfigFile();" << endl; /* XXX */
  } else if (strcmp(parameters, "PARAMETERS") == 0 ||
	     strcmp(parameters, "PARAMS") == 0 ||
	     strcmp(parameters, "PARAM") == 0) {
    ndbout << "SHOW PARAMETERS is not yet implemented." << endl;
    //    ndbout << "_mgmtSrvr.getConfig()->getConfigInfo()->print();" 
    //           << endl; /* XXX */
  } else {
    ndbout << "Invalid argument." << endl;
  }
}


//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeClusterLog(char* parameters) 
{
  int i;
  connect();
  if (parameters != 0 && strlen(parameters) != 0) {
  enum ndb_mgm_clusterlog_level severity = NDB_MGM_CLUSTERLOG_ALL;
    int isOk = true;
    char name[12]; 
    bool noArgs = false;
    
    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, " ", &tmpPtr);
    
    /********************
     * CLUSTERLOG FILTER 
     ********************/
    if (strcmp(item, "FILTER") == 0) {
      
      item = strtok_r(NULL, " ", &tmpPtr);
      if (item == NULL) {
	noArgs = true;
      }
      while (item != NULL) {
	snprintf(name, sizeof(name), item);

	if (strcmp(item, "ALL") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_ALL;	
	} else if (strcmp(item, "ALERT") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_ALERT;
	} else if (strcmp(item, "CRITICAL") == 0) { 
	  severity = NDB_MGM_CLUSTERLOG_CRITICAL;
	} else if (strcmp(item, "ERROR") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_ERROR;
	} else if (strcmp(item, "WARNING") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_WARNING;
	} else if (strcmp(item, "INFO") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_INFO;
	} else if (strcmp(item, "DEBUG") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_DEBUG;
	} else if (strcmp(item, "OFF") == 0) {
	  severity = NDB_MGM_CLUSTERLOG_OFF;
	} else {
	  isOk = false;
	}      
	
	item = strtok_r(NULL, " ", &tmpPtr);	
      } //  while(item != NULL){
      free(tmpString);

      if (noArgs) {
	ndbout << "Missing argument(s)." << endl;
      } else if (isOk) {
	if(ndb_mgm_filter_clusterlog(m_mgmsrv, severity, NULL)) {
	  if(strcmp(name, "ALL") == 0 || strcmp(name, "all") == 0) {
	    ndbout << "All severities levels enabled." << endl;
	  } else if(strcmp(name, "OFF") == 0 || strcmp(name, "off") == 0) {
	    ndbout << "Cluster logging enabled." << endl;
	  } else {
	    ndbout << name << " events disabled." << endl;
	  }
	} else {
	  if(strcmp(name, "ALL") == 0) {
	    ndbout << "All severities levels disabled." << endl;
	  } else if(strcmp(name, "OFF") == 0) {
	    ndbout << "Cluster logging disabled." << endl;
	  } else {
	    ndbout << name << " events enabled." << endl;
	  }
	}      
      } else {
	ndbout << "Invalid severity level." << endl;
      }

    /********************
     * CLUSTERLOG INFO
     ********************/
    } else if (strcmp(item, "INFO") == 0) {
      Uint32 *enabled = ndb_mgm_get_logfilter(m_mgmsrv);
      if(enabled == NULL) {
	ndbout << "Couldn't get status" << endl;
	printError();
	return;
      }
      const char* names[] = {"ENABLED", "DEBUG", "INFO", "WARNING", "ERROR", 
			     "CRITICAL", "ALERT"};
      if(enabled[0])
	ndbout << "Cluster logging is disabled." << endl;

      
      for(i = 0; i<7;i++)
	printf("enabled[%d] = %d\n", i, enabled[i]);
      ndbout << "Severities enabled: ";
      for(i = 1; i < 7; i++) {
	if(enabled[i])
	  ndbout << names[i] << " ";
      }
      ndbout << endl;

      /********************
       * CLUSTERLOG OFF
       ********************/
    } else if (strcmp(item, "OFF") == 0) {
      Uint32 *enabled = ndb_mgm_get_logfilter(m_mgmsrv);
      if(enabled == NULL) {
	ndbout << "Couldn't get status" << endl;
	printError();
	return;
      }
      if(!enabled[0]) {
	ndb_mgm_filter_clusterlog(m_mgmsrv, NDB_MGM_CLUSTERLOG_OFF, NULL);
	ndbout << "Cluster logging is disabled." << endl;	
      } else {
	ndbout << "Cluster logging is already disabled." << endl;	
	
      }
      
      /********************
       * CLUSTERLOG ON
       ********************/
    } else if (strcmp(item, "ON") == 0) {
      Uint32 *enabled = ndb_mgm_get_logfilter(m_mgmsrv);
      if(enabled == NULL) {
	ndbout << "Could not get status" << endl;
	printError();
	return;
      }
      if(enabled[0]) {
	ndb_mgm_filter_clusterlog(m_mgmsrv, NDB_MGM_CLUSTERLOG_OFF, NULL);
	ndbout << "Cluster logging is enabled." << endl;	
      } else {
	ndbout << "Cluster logging is already enabled." << endl;	
	
      }
    } else {
      ndbout << "Invalid argument." << endl;
    }
    
  } else {
    ndbout << "Missing argument." << endl;
  }
} 

//*****************************************************************************
//*****************************************************************************

void
CommandInterpreter::executeStop(int processId, const char *, bool all) 
{
  connect();
  int result = 0;
  if(all) {
    result = ndb_mgm_stop(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_stop(m_mgmsrv, 1, &processId);
  }
  if (result <= 0) {
    ndbout << "Shutdown failed." << endl;
    printError();
  } else
    {
      if(all)
	ndbout << "NDB Cluster has shutdown." << endl;
      else
	ndbout << "Node " << processId << " has shutdown." << endl;
    }
}

void
CommandInterpreter::executeEnterSingleUser(char* parameters) 
{
  connect();
  strtok(parameters, " ");
  struct ndb_mgm_reply reply;
  char* id = strtok(NULL, " ");
  id = strtok(NULL, " ");
  id = strtok(NULL, "\0");
  int nodeId = -1;
  if(id == 0 || sscanf(id, "%d", &nodeId) != 1){
    ndbout_c("Invalid arguments: expected <NodeId>");
    ndbout_c("Use SHOW to see what API nodes are configured");
    return;
  }
  int result = ndb_mgm_enter_single_user(m_mgmsrv, nodeId, &reply);
  
  if (result != 0) {
    ndbout_c("Entering single user mode for node %d failed", nodeId);
    printError();
  } else {
    ndbout_c("Entering single user mode");
    ndbout_c("Access will be granted for API node %d only.", nodeId);
    ndbout_c("Use ALL STATUS to see when single user mode has been entered.");
  }
}

void 
CommandInterpreter::executeExitSingleUser(char* parameters) 
{
  connect();
  int result = ndb_mgm_exit_single_user(m_mgmsrv, 0);
  if (result != 0) {
    ndbout_c("Exiting single user mode failed.");
    printError();
  } else {
    ndbout_c("Exiting single user mode in progress.");
    ndbout_c("Use ALL STATUS to see when single user mode has been exited.");
  }
}

void
CommandInterpreter::executeStart(int processId, const char* parameters,
				 bool all) 
{
  connect();
  int result;
  if(all) {
    result = ndb_mgm_start(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_start(m_mgmsrv, 1, &processId);
  }

  if (result <= 0) {
    ndbout << "Start failed." << endl;
    printError();
  } else
    {
      if(all)
	ndbout_c("NDB Cluster is being started.");
      else
	ndbout_c("Database node %d is being started.", processId);
    }
}

void
CommandInterpreter::executeRestart(int processId, const char* parameters,
				   bool all) 
{
  connect();
  int result;
  int nostart = 0;
  int initialstart = 0;
  int abort = 0;

  if(parameters != 0 && strlen(parameters) != 0){
    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, " ", &tmpPtr);
    while(item != NULL){
      if(strcmp(item, "-N") == 0)
	nostart = 1;
      if(strcmp(item, "-I") == 0)
	initialstart = 1;
      if(strcmp(item, "-A") == 0)
	abort = 1;
      item = strtok_r(NULL, " ", &tmpPtr);
    }
    free(tmpString);
  }

  if(all) {
    result = ndb_mgm_restart2(m_mgmsrv, 0, NULL, initialstart, nostart, abort);
  } else {
    int v[1];
    v[0] = processId;
    result = ndb_mgm_restart2(m_mgmsrv, 1, v, initialstart, nostart, abort);
  }
  
  if (result <= 0) {
    ndbout.println("Restart failed.", result);
    printError();
  } else
    {
      if(all)
	ndbout << "NDB Cluster is being restarted." << endl;
      else
	ndbout_c("Database node %d is being restarted.", processId);
    }
}

void
CommandInterpreter::executeDumpState(int processId, const char* parameters,
				     bool all) 
{
  if(parameters == 0 || strlen(parameters) == 0){
    ndbout << "Expected argument" << endl;
    return;
  }
  connect();

  Uint32 no = 0;
  int pars[25];
  
  char * tmpString = strdup(parameters);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  while(item != NULL){
    if (0x0 <= strtoll(item, NULL, 0) && strtoll(item, NULL, 0) <= 0xffffffff){
      pars[no] = strtoll(item, NULL, 0); 
    } else {
      ndbout << "Illegal value in argument to signal." << endl
	     << "(Value must be between 0 and 0xffffffff.)" 
	     << endl;
      return;
    }
    no++;
    item = strtok_r(NULL, " ", &tmpPtr);
  }
  ndbout << "Sending dump signal with data:" << endl;
  for (Uint32 i=0; i<no; i++) {
    ndbout.setHexFormat(1) << pars[i] << " ";
    if (!(i+1 & 0x3)) ndbout << endl;
  }
  free(tmpString);
  
  struct ndb_mgm_reply reply;
  ndb_mgm_dump_state(m_mgmsrv, processId, pars, no, &reply);
}

void 
CommandInterpreter::executeStatus(int processId, 
				  const char* parameters, bool all) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

  connect();
  ndb_mgm_node_status status;
  Uint32 startPhase, version;
  bool system;
  
  struct ndb_mgm_cluster_state *cl;
  cl = ndb_mgm_get_status(m_mgmsrv);
  if(cl == NULL) {
    ndbout_c("Cannot get status of node %d.", processId);
    printError();
    return;
  }

  int i = 0;
  while((i < cl->no_of_nodes) && cl->node_states[i].node_id != processId)
    i++;
  if(cl->node_states[i].node_id != processId) {
    ndbout << processId << ": Node not found" << endl;
    return;
  }
  status = cl->node_states[i].node_status;
  startPhase = cl->node_states[i].start_phase;
  version = cl->node_states[i].version;

  ndbout << "Node " << processId << ": ";
  switch(status){
  case NDB_MGM_NODE_STATUS_NO_CONTACT:
    ndbout << "No contact" << endl;
    break;
  case NDB_MGM_NODE_STATUS_NOT_STARTED:
    ndbout << "Not started" ;
    break;
  case NDB_MGM_NODE_STATUS_STARTING:
    ndbout << "Starting (Start phase " << startPhase << ")" ;
    break;
  case NDB_MGM_NODE_STATUS_STARTED:
    ndbout << "Started" ;
    break;
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    ndbout << "Shutting down " << (system == false ? "node" : "system")
	   << " (Phase " << startPhase << ")"
	   ;
    break;
  case NDB_MGM_NODE_STATUS_RESTARTING:
    ndbout << "Restarting" ;
    break;
  case NDB_MGM_NODE_STATUS_SINGLEUSER:
    ndbout << "Single user mode" ;
    break;
  default:
    ndbout << "Unknown state" ;
    break;
  }
  if(status != NDB_MGM_NODE_STATUS_NO_CONTACT)
    ndbout_c(" (Version %d.%d.%d)", 
	     getMajor(version) ,
	     getMinor(version),
	     getBuild(version));
}


//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLogLevel(int processId, const char* parameters, 
				    bool all) 
{
  connect();
  (void) all;
  (void) parameters;
  
  SetLogLevelOrd logLevel; logLevel.clear();
  LogLevel::EventCategory cat;  
  int level;
  if (emptyString(parameters) || (strcmp(parameters, "ALL") == 0)) {
    for(Uint32 i = 0; i<EventLogger::noOfEventCategoryNames; i++)
      logLevel.setLogLevel(EventLogger::eventCategoryNames[i].category, 7);
  } else {
    
    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, ", ", &tmpPtr);
    while(item != NULL){
      char categoryTxt[255];
      const int m = sscanf(item, "%[^=]=%d", categoryTxt, &level);
      if(m != 2){
	free(tmpString);
	ndbout << "Invalid loglevel specification category=level" << endl;
	return;
      }

      if(!EventLogger::matchEventCategory(categoryTxt,
			     &cat)){
	ndbout << "Invalid loglevel specification, unknown category: " 
	       << categoryTxt << endl;
	free(tmpString);
	return ;
      }
      if(level < 0 || level > 15){
	ndbout << "Invalid loglevel specification row, level 0-15" << endl;
	free(tmpString);
	return ;
      }
      logLevel.setLogLevel(cat, level);	
      
      item = strtok_r(NULL, ", ", &tmpPtr);
    }
    free(tmpString);
  }

  struct ndb_mgm_reply reply;
  int result;
  result = ndb_mgm_set_loglevel_node(m_mgmsrv, 
				     processId,  // fast fix - pekka
				     (char*)EventLogger::getEventCategoryName(cat),
				     level, 
				     &reply);

  if (result < 0) {
    ndbout_c("Executing LOGLEVEL on node %d failed.", processId);
    printError();
  } else {
    ndbout << "Executing LOGLEVEL on node " << processId << " OK!" 
	   << endl;
  }  

}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeError(int processId, 
				      const char* parameters, bool /* all */) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing error number." << endl;
    return;
  }

  connect();
  // Copy parameters since strtok will modify it
  char* newpar = strdup(parameters); 
  char* firstParameter = strtok(newpar, " ");

  int errorNo;
  if (! convert(firstParameter, errorNo)) {
    ndbout << "Expected an integer." << endl;
    free(newpar);
    return;
  }

  char* allAfterFirstParameter = strtok(NULL, "\0");
  if (! emptyString(allAfterFirstParameter)) {
    ndbout << "Nothing expected after error number." << endl;
    free(newpar);
    return;
  }

  ndb_mgm_insert_error(m_mgmsrv, processId, errorNo, NULL);

  free(newpar);
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTrace(int /*processId*/,
				 const char* /*parameters*/, bool /*all*/) 
{
#if 0
  if (emptyString(parameters)) {
    ndbout << "Missing trace number." << endl;
    return;
  }

  char* newpar = strdup(parameters);
  char* firstParameter = strtok(newpar, " ");


  int traceNo;
  if (! convert(firstParameter, traceNo)) {
    ndbout << "Expected an integer." << endl;
    free(newpar);
    return;
  }
  char* allAfterFirstParameter = strtok(NULL, "\0");  

  if (! emptyString(allAfterFirstParameter)) {
    ndbout << "Nothing expected after trace number." << endl;
    free(newpar);
    return;
  }

  int result = _mgmtSrvr.setTraceNo(processId, traceNo);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
  free(newpar);
#endif
}

//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLog(int processId,
			       const char* parameters, bool all) 
{
  connect();
  struct ndb_mgm_reply reply;
  Vector<const char *> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }
  int len=0;  
  Uint32 i;
  for(i=0; i<blocks.size(); i++) {
    ndbout_c("blocks %s %d",blocks[i], strlen(blocks[i]));
    len +=  strlen(blocks[i]);
  }
  len += blocks.size()*2;
  char * blockNames = (char*)malloc(len);
  
  for(i=0; i<blocks.size(); i++) {
    strcat(blockNames, blocks[i]);
    strcat(blockNames, "|");
  }
  strcat(blockNames, "\0");
  ndbout_c("blocknames %s", blockNames);
  
  /*int res =*/ndb_mgm_log_signals(m_mgmsrv,
				   processId, 
				   NDB_MGM_SIGNAL_LOG_MODE_INOUT, 
				   blockNames,
				   &reply);

#if 0  
  int result = 
    _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::InOut, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogIn(int /* processId */,
				 const char* parameters, bool /* all */) 
{
  Vector<const char*> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }
  
#if 0
  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::In, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOut(int /*processId*/, 
				  const char* parameters, bool /*all*/) 
{
  Vector<const char*> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }


#if 0
  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::Out, 
					      blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOff(int /*processId*/,
				  const char* parameters, bool /*all*/) 
{
  Vector<const char*> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }

  
#if 0
  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::Off, 
					      blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOn(int /*processId*/, 
				  const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

#if 0
  int result = _mgmtSrvr.startSignalTracing(processId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOff(int /*processId*/, 
				   const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

#if 0
  int result = _mgmtSrvr.stopSignalTracing(processId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}


//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeSet(int /*processId*/, 
			       const char* parameters, bool /*all*/) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing parameter name." << endl;
    return;
  }
#if 0
  // Copy parameters since strtok will modify it
  char* newpar = strdup(parameters);
  char* configParameterName = strtok(newpar, " ");

  char* allAfterParameterName = strtok(NULL, "\0");
  if (emptyString(allAfterParameterName)) {
    ndbout << "Missing parameter value." << endl;
    free(newpar);
    return;
  }

  char* value = strtok(allAfterParameterName, " ");

  char* allAfterValue = strtok(NULL, "\0");
  if (! emptyString(allAfterValue)) {
    ndbout << "Nothing expected after parameter value." << endl;
    free(newpar);
    return;
  }

  bool configBackupFileUpdated;
  bool configPrimaryFileUpdated;
  
  // TODO The handling of the primary and backup config files should be 
  // analysed further.
  // How it should be handled if only the backup is possible to write.

  int result = _mgmtSrvr.updateConfigParam(processId, configParameterName, 
					   value, configBackupFileUpdated, 
					   configPrimaryFileUpdated);
  if (result == 0) {
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration is updated." << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration is updated but it was only possible " 
	     << "to update the backup configuration file, not the primary." 
	     << endl;
    }
    else {
      assert(false);
    }
  }
  else {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration files are however updated and "
	     << "the value will be used next time the process is restarted." 
	     << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "It was only possible to update the backup "
	     << "configuration file, not the primary." << endl;
    }
    else if (!configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration files are not updated." << endl;
    }
    else {
      // The primary is not tried to write if the write of backup file fails
      abort();
    }
  }
  free(newpar);
#endif
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeGetStat(int /*processId*/,
					const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

#if 0
  MgmtSrvr::Statistics statistics;
  int result = _mgmtSrvr.getStatistics(processId, statistics);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
    return;
  }
#endif
  // Print statistic...
  /*
  ndbout << "Number of GETSTAT commands: " 
  << statistics._test1 << endl;
  */
}

//*****************************************************************************
//*****************************************************************************
				 
void 
CommandInterpreter::executeEventReporting(int processId,
					  const char* parameters, 
					  bool all) 
{
  connect();
  SetLogLevelOrd logLevel; logLevel.clear();
  char categoryTxt[255];
  int level;  
  LogLevel::EventCategory cat;
  if (emptyString(parameters) || (strcmp(parameters, "ALL") == 0)) {
    for(Uint32 i = 0; i<EventLogger::noOfEventCategoryNames; i++)
      logLevel.setLogLevel(EventLogger::eventCategoryNames[i].category, 7);
  } else {

    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, ", ", &tmpPtr);
    while(item != NULL){
      const int m = sscanf(item, "%[^=]=%d", categoryTxt, &level);
      if(m != 2){
	free(tmpString);
	ndbout << "Invalid loglevel specification category=level" << endl;
	return;
      }
      
      if(!EventLogger::matchEventCategory(categoryTxt,
					  &cat)){
	ndbout << "Invalid loglevel specification, unknown category: " 
	       << categoryTxt << endl;
	free(tmpString);
	return ;
      }
      if(level < 0 || level > 15){
	ndbout << "Invalid loglevel specification row, level 0-15" << endl;
	free(tmpString);
	return ;
      }
      logLevel.setLogLevel(cat, level);	
      
      item = strtok_r(NULL, ", ", &tmpPtr);
    }
    free(tmpString);
  }
  struct ndb_mgm_reply reply;
  int result;

  result = 
    ndb_mgm_set_loglevel_clusterlog(m_mgmsrv, 
				    processId, // fast fix - pekka
				    (char*)
                                      EventLogger::getEventCategoryName(cat),
				    level, 
				    &reply);
  
  if (result != 0) {
    ndbout_c("Executing CLUSTERLOG on node %d failed", processId);
    printError();
  } else {
    ndbout << "Executing CLUSTERLOG on node " << processId << " OK!" 
	   << endl;
  }  
}

/*****************************************************************************
 * Backup
 *****************************************************************************/
void
CommandInterpreter::executeStartBackup(char* /*parameters*/) 
{
  connect();
  struct ndb_mgm_reply reply;
  unsigned int backupId;
  int result = ndb_mgm_start_backup(m_mgmsrv, &backupId, &reply);
  if (result != 0) {
    ndbout << "Start of backup failed" << endl;
    printError();
  } else {
    ndbout << "Backup started. Backup id " << backupId << "." << endl;
  }
}

void
CommandInterpreter::executeAbortBackup(char* parameters) 
{
  connect();
  strtok(parameters, " ");
  struct ndb_mgm_reply reply;
  char* id = strtok(NULL, "\0");
  int bid = -1;
  if(id == 0 || sscanf(id, "%d", &bid) != 1){
    ndbout << "Invalid arguments: expected <BackupId>" << endl;
    return;
  }
  int result = ndb_mgm_abort_backup(m_mgmsrv, bid, &reply);
  if (result != 0) {
    ndbout << "Abort of backup " << bid << " failed" << endl;
    printError();
  } else {
    ndbout << "Abort of backup " << bid << " ordered" << endl;
  }
}

#ifdef HAVE_GLOBAL_REPLICATION
/*****************************************************************************
 * Global Replication
 *
 * For information about the different commands, see
 * GrepReq::Request in file signaldata/grepImpl.cpp.
 *
 * Below are commands as of 2003-07-05 (may change!):
 * START = 0,            ///< Start Global Replication (all phases)
 * START_METALOG = 1,    ///< Start Global Replication (all phases)
 * START_METASCAN = 2,   ///< Start Global Replication (all phases)
 * START_DATALOG = 3,    ///< Start Global Replication (all phases)
 * START_DATASCAN = 4,   ///< Start Global Replication (all phases)
 * START_REQUESTOR = 5,  ///< Start Global Replication (all phases)
 * ABORT = 6,            ///< Immediate stop (removes subscription)
 * SLOW_STOP = 7,        ///< Stop after finishing applying current GCI epoch
 * FAST_STOP = 8,        ///< Stop after finishing applying all PS GCI epochs
 * START_TRANSFER = 9,   ///< Start SS-PS transfer
 * STOP_TRANSFER = 10,   ///< Stop SS-PS transfer
 * START_APPLY = 11,     ///< Start applying GCI epochs in SS
 * STOP_APPLY = 12,      ///< Stop applying GCI epochs in SS
 * STATUS = 13,           ///< Status
 * START_SUBSCR = 14,
 * REMOVE_BUFFERS = 15,
 * DROP_TABLE = 16

 *****************************************************************************/

void
CommandInterpreter::executeRep(char* parameters) 
{
  if (emptyString(parameters)) {
    ndbout << helpTextRep;
    return;
  }

  connect();
  char * line = strdup(parameters);
  char * firstToken = strtok(line, " ");
  
  struct ndb_rep_reply  reply;
  unsigned int          repId;


  if (!strcmp(firstToken, "CONNECT")) {
    char * host = strtok(NULL, "\0");
    for (unsigned int i = 0; i < strlen(host); ++i) {
      host[i] = tolower(host[i]);
    }
    
    if(host == NULL)
    {
      ndbout_c("host:port must be specified.");
      return;
    }
    
    if(rep_connected) {
      if(m_repserver != NULL) {
	ndb_rep_disconnect(m_repserver);
	rep_connected = false;
      }       
    }
          
    if(m_repserver == NULL)
      m_repserver = ndb_rep_create_handle();
    if(ndb_rep_connect(m_repserver, host) < 0)
      ndbout_c("Failed to connect to %s", host); 
    else
      rep_connected=true;
    return;
    
    if(!rep_connected) {
      ndbout_c("Not connected to REP server");
    }
  }
    
  /********
   * START 
   ********/
  if (!strcmp(firstToken, "START")) {
    
    unsigned int          req;
    char *startType = strtok(NULL, "\0");
    
    if (startType == NULL) {                
      req = GrepReq::START;
    } else if (!strcmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::START_SUBSCR;
    } else if (!strcmp(startType, "METALOG")) { 
      req = GrepReq::START_METALOG;
    } else if (!strcmp(startType, "METASCAN")) {
      req = GrepReq::START_METASCAN;
    } else if (!strcmp(startType, "DATALOG")) {
      req = GrepReq::START_DATALOG;
    } else if (!strcmp(startType, "DATASCAN")) {
      req = GrepReq::START_DATASCAN;
    } else if (!strcmp(startType, "REQUESTOR")) {
      req = GrepReq::START_REQUESTOR;
    } else if (!strcmp(startType, "TRANSFER")) {
      req = GrepReq::START_TRANSFER;
    } else if (!strcmp(startType, "APPLY")) {
      req = GrepReq::START_APPLY;
    } else if (!strcmp(startType, "DELETE")) {
      req = GrepReq::START_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION START'");
      return;
    }

    int result = ndb_rep_command(m_repserver, req, &repId, &reply);
    
    if (result != 0) {
      ndbout << "Start of Global Replication failed" << endl;
    } else {
      ndbout << "Start of Global Replication ordered" << endl;
    }
    return;
  }

  /********
   * STOP
   ********/
  if (!strcmp(firstToken, "STOP")) {    
    unsigned int          req;
    char *startType = strtok(NULL, " ");
    unsigned int epoch = 0;
    
    if (startType == NULL) {                 
      /**
       * Stop immediately
       */
      req = GrepReq::STOP;
    } else if (!strcmp(startType, "EPOCH")) {  
      char *strEpoch = strtok(NULL, "\0");
      if(strEpoch == NULL) {
	ndbout_c("Epoch expected!");
	return;
      }
      req = GrepReq::STOP;
      epoch=atoi(strEpoch);      
    } else if (!strcmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::STOP_SUBSCR;
    } else if (!strcmp(startType, "METALOG")) { 
      req = GrepReq::STOP_METALOG;
    } else if (!strcmp(startType, "METASCAN")) {
      req = GrepReq::STOP_METASCAN;
    } else if (!strcmp(startType, "DATALOG")) {
      req = GrepReq::STOP_DATALOG;
    } else if (!strcmp(startType, "DATASCAN")) {
      req = GrepReq::STOP_DATASCAN;
    } else if (!strcmp(startType, "REQUESTOR")) {
      req = GrepReq::STOP_REQUESTOR;
    } else if (!strcmp(startType, "TRANSFER")) {
      req = GrepReq::STOP_TRANSFER;
    } else if (!strcmp(startType, "APPLY")) {
      req = GrepReq::STOP_APPLY;
    } else if (!strcmp(startType, "DELETE")) {
      req = GrepReq::STOP_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION STOP'");
      return;
    }
    int result = ndb_rep_command(m_repserver, req, &repId, &reply, epoch);
    
    if (result != 0) {
      ndbout << "Stop command failed" << endl;
    } else {
      ndbout << "Stop ordered" << endl;
    }
    return;
  }

  /*********
   * STATUS
   *********/
  if (!strcmp(firstToken, "STATUS")) {
    struct rep_state repstate;
    int result = 
      ndb_rep_get_status(m_repserver, &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Status request of Global Replication failed" << endl;
    } else {
      ndbout << "Status request of Global Replication ordered" << endl;
      ndbout << "See printout at one of the DB nodes" << endl;
      ndbout << "(Better status report is under development.)" << endl;
      ndbout << " SubscriptionId " << repstate.subid 
	     << " SubscriptionKey " << repstate.subkey << endl;
    }
    return;
  }

  /*********
   * QUERY (see repapi.h for querable counters)
   *********/
  if (!strcmp(firstToken, "QUERY")) {
    char *query = strtok(NULL, "\0");
    int queryCounter=-1;
    if(query != NULL) {
      queryCounter = atoi(query);
    }
    struct rep_state repstate;
    unsigned repId = 0;
    int result = ndb_rep_query(m_repserver, (QueryCounter)queryCounter,
			       &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Query repserver failed" << endl;
    } else {
      ndbout << "Query repserver sucessful" << endl;
      ndbout_c("repstate : QueryCounter %d, f=%d l=%d"
	       " nodegroups %d" , 
	       repstate.queryCounter,
	       repstate.first[0], repstate.last[0],
	       repstate.no_of_nodegroups );
    }
    return;
  }
}
#endif // HAVE_GLOBAL_REPLICATION


/*****************************************************************************
 * CPC
 *****************************************************************************/

#if 0

#if 0
//#ifdef NDB_SOLARIS      // XXX fix me
static char* strsep(char** x, const char* y) { return 0; }
#endif

// Note this code has not been verified
#if 0
static char * my_strsep(char **stringp, const char *delim)
{
  char *tmp= *stringp;
  if (tmp == 0)
    return 0;
  *stringp = strtok(tmp, delim);
  return tmp;
}
#endif

void
CommandInterpreter::executeCpc(char *parameters) 
{
  char *host_str = NULL, *port_str = NULL, *end;
  long port = 1234; /* XXX */

  while((host_str = my_strsep(&parameters, " \t:")) != NULL &&
	host_str[0] == '\0');

  if(parameters && parameters[0] != '\0') {
    while((port_str = my_strsep(&parameters, " \t:")) != NULL &&
	  port_str[0] == '\0');
    
    errno = 0;
    port = strtol(port_str, &end, 0);
    if(end[0] != '\0')
      goto error;
    if((port == LONG_MAX || port == LONG_MIN) &&
       errno == ERANGE)
      goto error;
  }

  {
    SimpleCpcClient cpc(host_str, port);
    bool done = false;
    
    if(cpc.connect() < 0) {
      ndbout_c("Cannot connect to %s:%d.", cpc.getHost(), cpc.getPort());
      switch(errno) {
      case ENOENT:
	ndbout << ": " << "No such host" << endl;
	break;
      default:
	ndbout << ": " << strerror(errno) << endl;
	break;
      }
      return;
    }
    
    while(!done) {
      char *line = readline("CPC> ");
      if(line != NULL) {
	add_history(line);
	
	char *cmd = strtok(line, " ");
	char *arg = strtok(NULL, "");
	
	if(arg != NULL) {
	  while(arg[0] == ' ')
	    arg++;
	  if(strlen(arg) == 0)
	    arg = NULL;
	}
	
	if(cmd != NULL) {
	  if(strcmp(cmd, "exit") == 0)
	    done = true;
	  else if(strcmp(cmd, "list") == 0)
	    cpc.cmd_list(arg);
	  else if(strcmp(cmd, "start") == 0)
	    cpc.cmd_start(arg);
	  else if(strcmp(cmd, "stop") == 0)
	    cpc.cmd_stop(arg);
	  else if(strcmp(cmd, "help") == 0)
	    cpc.cmd_help(arg);
	}
      } else {
	done = true;
	ndbout << endl;
      }
    }
  }
  return;
  
 error:
  ndbout << "Error: expected a tcp port number, got '" << port_str << "'."
	 << endl;
  return;
}
#endif

#if 0
static
void
CmdBackupCallback(const MgmtSrvr::BackupEvent & event){
  char str[255];

  ndbout << endl;
  
  bool ok = false;
  switch(event.Event){
  case MgmtSrvr::BackupEvent::BackupStarted:
    ok = true;
    snprintf(str, sizeof(str), 
	     "Backup %d started", event.Started.BackupId);
    break;
  case MgmtSrvr::BackupEvent::BackupFailedToStart:
    ok = true;
    snprintf(str, sizeof(str), 
	     "Backup failed to start (Error %d)",
	     event.FailedToStart.ErrorCode);
    break;
  case MgmtSrvr::BackupEvent::BackupCompleted:
    ok = true;
    snprintf(str, sizeof(str), 
	     "Backup %d completed", 
	     event.Completed.BackupId);
    ndbout << str << endl;

    snprintf(str, sizeof(str), 
	     " StartGCP: %d StopGCP: %d", 
	     event.Completed.startGCP, event.Completed.stopGCP);
    ndbout << str << endl;

    snprintf(str, sizeof(str), 
	     " #Records: %d #LogRecords: %d", 
	     event.Completed.NoOfRecords, event.Completed.NoOfLogRecords);
    ndbout << str << endl;

    snprintf(str, sizeof(str), 
	     " Data: %d bytes Log: %d bytes", 
	     event.Completed.NoOfBytes, event.Completed.NoOfLogBytes);
    break;
  case MgmtSrvr::BackupEvent::BackupAborted:
    ok = true;
    snprintf(str, sizeof(str), 
	     "Backup %d has been aborted reason %d",
	     event.Aborted.BackupId,
	     event.Aborted.Reason);
    break;
  }
  if(!ok){
    snprintf(str, sizeof(str), 
	     "Unknown backup event: %d",
	     event.Event);
    
  }
  ndbout << str << endl;
}
#endif

template class Vector<char const*>;
