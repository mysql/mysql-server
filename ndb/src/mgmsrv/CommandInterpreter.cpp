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

#include <string.h>
#include <ctype.h>

#include "MgmtSrvr.hpp"
#include "MgmtErrorReporter.hpp"
#include <NdbOut.hpp>
#include "convertStrToInt.hpp"
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include "ConfigInfo.hpp"

#include <version.h>


static const char* helpTexts[] = {
  "HELP                                        Print help text",
  "HELP SHOW                                   Help for the SHOW command",
#ifdef VM_TRACE // DEBUG ONLY
  "HELP DEBUG                                  Help for debug compiled version",
#endif
  "SHOW                                        Print information about cluster",
  "SHOW CONFIG                                 Print configuration",
  "SHOW PARAMETERS                             Print configuration parameters",
  "START BACKUP                                Start backup\n"
  "ABORT BACKUP <backup id>                    Aborts backup\n"
  "CLUSTERLOG ON                               Enable Cluster logging",
  "CLUSTERLOG OFF                              Disable Cluster logging",
  "CLUSTERLOG FILTER <severity>                Toggle severity filter on/off",
  "CLUSTERLOG INFO                             Print cluster log information",
  "{<id>|ALL} START                            Start DB node (started with -n)",
  "{<id>|ALL} RESTART [-n] [-i]                Restart DB node",
  "{<id>|ALL} STOP                             Stop DB node",
  "{<id>|ALL} STATUS                           Print status",
  "{<id>|ALL} CLUSTERLOG {<category>=<level>}+ Set log level for cluster log",
  "QUIT                                        Quit management server",
};
static const unsigned noOfHelpTexts = sizeof(helpTexts)/sizeof(const char*);

static const char* helpTextShow =
"SHOW prints NDB Cluster information\n\n"
"SHOW               Print information about cluster\n" 
"SHOW CONFIG        Print configuration (in initial config file format)\n" 
"SHOW PARAMETERS    Print information about configuration parameters\n\n"
;

#ifdef VM_TRACE // DEBUG ONLY
static const char* helpTextDebug =
"SHOW PROPERTIES                             Print config properties object\n"
"{<id>|ALL} LOGLEVEL {<category>=<level>}+   Set log level\n"
"{<id>|ALL} ERROR <errorNo>                  Inject error into NDB node\n"
"{<id>|ALL} TRACE <traceNo>                  Set trace number\n"
"{<id>|ALL} LOG [BLOCK = {ALL|<block>+}]     Set logging on in & out signals\n"
"{<id>|ALL} LOGIN [BLOCK = {ALL|<block>+}]   Set logging on in signals\n"
"{<id>|ALL} LOGOUT [BLOCK = {ALL|<block>+}]  Set logging on out signals\n"
"{<id>|ALL} LOGOFF [BLOCK = {ALL|<block>+}]  Unset signal logging\n"
"{<id>|ALL} TESTON                           Start signal logging\n"
"{<id>|ALL} TESTOFF                          Stop signal logging\n"
"{<id>|ALL} SET <configParamName> <value>    Update configuration variable\n"
"{<id>|ALL} DUMP <arg>                       Dump system state to cluster.log\n"
"{<id>|ALL} GETSTAT                          Print statistics\n"
"\n"
;
#endif



//******************************************************************************
//******************************************************************************
CommandInterpreter::CommandInterpreter(MgmtSrvr& mgmtSrvr) :
  _mgmtSrvr(mgmtSrvr) {

  //  _mgmtSrvr.setCallback(CmdBackupCallback);
}


bool emptyString(const char* s) {
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

class AutoPtr {
public:
  AutoPtr(void * ptr) : m_ptr(ptr) {}
  ~AutoPtr() { free(m_ptr);}
private:
  void * m_ptr;
};

//*****************************************************************************
//*****************************************************************************
int CommandInterpreter::readAndExecute() {

  char* _line = readline_gets(); 
  char * line;
  if(_line == NULL) {
    ndbout << endl;
    return true;
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
  char* allAfterFirstToken = strtok(NULL, "\0");
  
  if (strcmp(firstToken, "HELP") == 0) {
    executeHelp(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "?") == 0) {
    executeHelp(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "SHOW") == 0) {
    executeShow(allAfterFirstToken);
    return true;
  }
  else if (strcmp(firstToken, "CLUSTERLOG") == 0) {
    executeClusterLog(allAfterFirstToken);
    return true;
  }
  else if(strcmp(firstToken, "START") == 0 &&
	  allAfterFirstToken != 0 &&
	  strncmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeStartBackup(allAfterFirstToken);
    return true;
  }
  else if(strcmp(firstToken, "ABORT") == 0 &&
	  allAfterFirstToken != 0 &&
	  strncmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeAbortBackup(allAfterFirstToken);
    return true;
  }

  else if(strcmp(firstToken, "ENTER") == 0 &&
	  allAfterFirstToken != 0 &&
	  strncmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeEnterSingleUser(allAfterFirstToken);
    return true;
  }

  else if(strcmp(firstToken, "EXIT") == 0 &&
	  allAfterFirstToken != 0 &&
	  strncmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeExitSingleUser(allAfterFirstToken);
    return true;
  }

  else if (strcmp(firstToken, "ALL") == 0) {
    analyseAfterFirstToken(-1, allAfterFirstToken);
  } 
  else if(strcmp(firstToken, "QUIT") == 0 ||
	  strcmp(firstToken, "EXIT") == 0 ||
	  strcmp(firstToken, "BYE") == 0){
    return false;
  } else {
    // First token should be a digit, process ID
    
    int processId;
    if (! convert(firstToken, processId)) {
      ndbout << "Invalid command: " << _line << "." << endl;
      ndbout << "Type HELP for help." << endl << endl;
      return true;
    }
    if (processId < 0) {
      ndbout << "Invalid process ID: " << firstToken << "." << endl;
      return true;
    }
    
    analyseAfterFirstToken(processId, allAfterFirstToken);
    
  } // else
  return true;
}


static const CommandInterpreter::CommandFunctionPair commands[] = {
  { "START", &CommandInterpreter::executeStart }
  ,{ "RESTART", &CommandInterpreter::executeRestart }
  ,{ "STOP", &CommandInterpreter::executeStop }
  ,{ "STATUS", &CommandInterpreter::executeStatus }
  ,{ "LOGLEVEL", &CommandInterpreter::executeLogLevel }
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
  ,{ "CLUSTERLOG", &CommandInterpreter::executeEventReporting }
  ,{ "DUMP", &CommandInterpreter::executeDumpState }
  ,{ "JONAS", &CommandInterpreter::jonas }
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
      ndbout << "Expected a command after process ID." << endl;
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
    ndbout << "Invalid command: " << secondToken << "." << endl;
    ndbout << "Type HELP for help." << endl << endl;
    return;
  }
  
  if(processId == -1){
    executeForAll(command, fun, allAfterSecondToken);
  } else {
    if(strcmp(command, "STATUS") != 0)
      ndbout << "Executing " << command << " on node: " 
	     << processId << endl << endl;
    (this->*fun)(processId, allAfterSecondToken, false);
    ndbout << endl;
  }
}

void
CommandInterpreter::executeForAll(const char * cmd, ExecuteFunction fun, 
				  const char * allAfterSecondToken){

  NodeId nodeId = 0;
  if(strcmp(cmd, "STOP") == 0 ||
     strcmp(cmd, "RESTART") == 0){
    ndbout << "Executing " << cmd << " on all nodes" << endl << "\n";
    (this->*fun)(nodeId, allAfterSecondToken, true);
  } else {
    while(_mgmtSrvr.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
      if(strcmp(cmd, "STATUS") != 0)
	ndbout << "Executing " << cmd << " on node: " 
	       << nodeId << endl << endl;
      (this->*fun)(nodeId, allAfterSecondToken, true);
      ndbout << endl;
    } // for
  }
}

//*****************************************************************************
//*****************************************************************************
bool CommandInterpreter::parseBlockSpecification(const char* allAfterLog,
						 Vector<BaseString>& blocks) {
  
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
    blocks.push_back(BaseString(blockName));
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



//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeHelp(char* parameters) {

  (void)parameters;  // Don't want compiler warning

  if (emptyString(parameters)) {
    unsigned i;
    for (i = 0; i<noOfHelpTexts; i++) {
      ndbout << helpTexts[i] << endl;
    }
    
    ndbout << endl 
	   << "<severity> = " 
	   << "ALERT | CRITICAL | ERROR | WARNING | INFO | DEBUG"
	   << endl;

    ndbout << "<category> = ";
    for(i = 0; i<CFG_MIN_LOGLEVEL; i++){
      ndbout << ndb_mgm_get_event_category_string((ndb_mgm_event_category)i);
      if (i < CFG_MIN_LOGLEVEL - 1) {
	ndbout << " | ";
      }
    }
    ndbout << endl;
    
    ndbout << "<level>    = " << "0 - 15"
	   << endl;
    
    ndbout << endl;
  } else if (strcmp(parameters, "SHOW") == 0) {
    ndbout << helpTextShow;
#ifdef VM_TRACE // DEBUG ONLY
  } else if (strcmp(parameters, "DEBUG") == 0) {
    ndbout << helpTextDebug;
#endif
  } else {
    ndbout << "Invalid argument." << endl;
  }
}

//*****************************************************************************
//*****************************************************************************

void CommandInterpreter::executeShow(char* parameters) {

  if (emptyString(parameters)) {
    ndbout << "Cluster Configuration" << endl
	   << "---------------------" << endl;
    
    NodeId nodeId = 0;
    ndbout << _mgmtSrvr.getNodeCount(NDB_MGM_NODE_TYPE_NDB) 
	   << " NDB Node(s) with" 
	   << endl;
    while (_mgmtSrvr.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
      ndbout << "       Node Id = " << nodeId << endl;
    }
    ndbout << endl;
    
    nodeId = 0;
    ndbout << _mgmtSrvr.getNodeCount(NDB_MGM_NODE_TYPE_API) 
	   << " API Node(s) with" 
	   << endl;
    while (_mgmtSrvr.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_API)){
      ndbout << "       Node Id = " << nodeId << endl;
    }
    ndbout << endl;
    
    nodeId = 0;
    ndbout << _mgmtSrvr.getNodeCount(NDB_MGM_NODE_TYPE_MGM) 
	   << " MGM Node(s) with" 
	   << endl;
    while (_mgmtSrvr.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_MGM)){
      ndbout << "       Node Id = " << nodeId << endl;
    }
    ndbout << endl;

    ndbout << helpTextShow;

    return;
  } else if (strcmp(parameters, "PROPERTIES") == 0 ||
	     strcmp(parameters, "PROP") == 0) {
    ndbout << "_mgmtSrvr.getConfig()->print();" << endl; /* XXX */
  } else if (strcmp(parameters, "CONFIGURATION") == 0 ||
	     strcmp(parameters, "CONFIG") == 0){
    ndbout << "_mgmtSrvr.getConfigFile()->print();" << endl; /* XXX */
    _mgmtSrvr.getConfig()->printConfigFile();
  } else if (strcmp(parameters, "PARAMETERS") == 0 ||
	     strcmp(parameters, "PARAMS") == 0 ||
	     strcmp(parameters, "PARAM") == 0) {
    ndbout << "_mgmtSrvr.getConfigInfo()->print();" << endl; /* XXX */
  } else {
    ndbout << "Invalid argument." << endl;
  }
}


//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeClusterLog(char* parameters) {

  if (parameters != 0 && strlen(parameters) != 0) {
    int severity = 7;
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
	snprintf(name, 12, item);

	if (strcmp(item, "ALL") == 0) {
	  severity = 7;	
	} else if (strcmp(item, "ALERT") == 0) {
	  severity = 6;
	} else if (strcmp(item, "CRITICAL") == 0) { 
	  severity = 5;
	} else if (strcmp(item, "ERROR") == 0) {
	  severity = 4;
	} else if (strcmp(item, "WARNING") == 0) {
	  severity = 3;
	} else if (strcmp(item, "INFO") == 0) {
	  severity = 2;
	} else if (strcmp(item, "DEBUG") == 0) {
	  severity = 1;
	} else if (strcmp(item, "OFF") == 0) {
	  severity = 0;
	} else {
	  isOk = false;
	}      
	
	item = strtok_r(NULL, " ", &tmpPtr);	
      } //  while(item != NULL){
      free(tmpString);

      if (noArgs) {
	ndbout << "Missing argument(s)." << endl;
      } else if (isOk) {
	if (_mgmtSrvr.setEventLogFilter(severity)) {
	  if(strcmp(name, "ALL") == 0 || strcmp(name, "all") == 0) {
	    ndbout << "All severities levels enabled." << endl;
	  } else if(strcmp(name, "OFF") == 0 || strcmp(name, "off") == 0) {
	    ndbout << "Cluster logging disabled." << endl;
	  } else {
	    ndbout << name << " events enabled." << endl;
	  }
	} else {
	  if(strcmp(name, "ALL") == 0) {
	    ndbout << "All severities levels disabled." << endl;
	  } else if(strcmp(name, "OFF") == 0) {
	    ndbout << "Cluster logging enabled." << endl;
	  } else {
	    ndbout << name << " events disabled." << endl;
	  }
	}      
      } else {
	ndbout << "Invalid severity level." << endl;
      }

    /********************
     * CLUSTERLOG INFO
     ********************/
    } else if (strcmp(item, "INFO") == 0) {
      const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", 
			     "CRITICAL", "ALERT"};
      if (_mgmtSrvr.isEventLogFilterEnabled(0)) { // OFF
	ndbout << "Cluster logging is disabled." << endl;
      } 

      ndbout << "Severities enabled: ";
      for (int i = 0; i < 6; i++) {
	if (_mgmtSrvr.isEventLogFilterEnabled(i + 1)) {
	  ndbout << names[i] << " ";
	}	
      }
      ndbout << endl;

      /********************
       * CLUSTERLOG OFF
       ********************/
    } else if (strcmp(item, "OFF") == 0) {
      if (!_mgmtSrvr.isEventLogFilterEnabled(0)) { // ON
	if (_mgmtSrvr.setEventLogFilter(0));
	ndbout << "Cluster logging is disabled." << endl;	
      } else {
	ndbout << "Cluster logging is already disabled." << endl;	
      }
      
      /********************
       * CLUSTERLOG ON
       ********************/
    } else if (strcmp(item, "ON") == 0) {
      if (_mgmtSrvr.isEventLogFilterEnabled(0)) { // OFF
	if (_mgmtSrvr.setEventLogFilter(0));
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

void
stopCallback(int nodeId, void * anyData, int errCode){
  if(errCode == 0){
    if(nodeId == 0)
      ndbout << "\nCluster has shutdown" << endl;
    else
      ndbout << "\nNode " << nodeId << " has shutdown" << endl;
  } else {
    MgmtSrvr * mgm = (MgmtSrvr *)anyData;
    ndbout << "Node " << nodeId << " has not shutdown: " 
	   << mgm->getErrorText(errCode) << endl;
  }
}

void
versionCallback(int nodeId, int version, void * anyData, int errCode){
  if(errCode == 0){
    MgmtSrvr * mgm = (MgmtSrvr *)anyData;
    switch(mgm->getNodeType(nodeId)){
    case NDB_MGM_NODE_TYPE_MGM:
      {	
	ndbout << "MGMT node:\t" << nodeId << " ";      
	  ndbout_c(" (Version %d.%d.%d)", 
		   getMajor(version) ,
		   getMinor(version),
		   getBuild(version));	  
      }
      break;
    case NDB_MGM_NODE_TYPE_NDB:
      {
	ndbout << "DB node:\t" << nodeId << " ";      
	if(version == 0)
	  ndbout << "(no version information available)" << endl;
	else {
	  ndbout_c(" (Version %d.%d.%d)", 
		   getMajor(version) ,
		   getMinor(version),
		   getBuild(version));	  
	}
      }
      break;
    case NDB_MGM_NODE_TYPE_API:
      {
	ndbout << "API node:\t" << nodeId << " ";      
	if(version == 0)
	  ndbout << "(no version information available)" << endl;
	else {
	  ndbout_c(" (Version %d.%d.%d)", 
		   getMajor(version) ,
		   getMinor(version),
		   getBuild(version));
	}
	
      }
      break;
    case NDB_MGM_NODE_TYPE_UNKNOWN:
      abort();
    };
    
  } else {
    MgmtSrvr * mgm = (MgmtSrvr *)anyData;
    ndbout  << mgm->getErrorText(errCode) << endl;
  }
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeStop(int processId, 
				     const char* parameters, bool all) {
  
  (void)parameters;  // Don't want compiler warning  

  int result = 0;
  if(all)
    result = _mgmtSrvr.stop((int *)0, false, stopCallback, this);
  else
    result = _mgmtSrvr.stopNode(processId, false, stopCallback, this);
  
  if(result != 0)
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
}


void CommandInterpreter::executeStart(int processId, const char* parameters,
				      bool all) {
  (void)all;  // Don't want compiler warning

  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  
  int result = _mgmtSrvr.start(processId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
}

void
CommandInterpreter::executeRestart(int processId, const char* parameters,
				   bool all) {
  
  bool nostart = false;
  bool initialstart = false;

  if(parameters != 0 && strlen(parameters) != 0){
    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, " ", &tmpPtr);
    while(item != NULL){
      if(strcmp(item, "-N") == 0)
	nostart = true;
      if(strcmp(item, "-I") == 0)
	initialstart = true;
      item = strtok_r(NULL, " ", &tmpPtr);
    }
    free(tmpString);
  }
  int result;
  if(all)
    result = _mgmtSrvr.restart(nostart, initialstart, false, 
			       0, stopCallback, this);
  else
    result = _mgmtSrvr.restartNode(processId, nostart, initialstart, false, 
			       stopCallback,
			       this);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
}

void
CommandInterpreter::executeDumpState(int processId, const char* parameters,
				     bool all) {
  
  (void)all;  // Don't want compiler warning 

  if(parameters == 0 || strlen(parameters) == 0){
    ndbout << "Expected argument" << endl;
    return;
  }

  Uint32 no = 0;
  Uint32 pars[25];
  
  char * tmpString = strdup(parameters);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  while(item != NULL){
    if (0x0 <= strtoll(item, NULL, 0) && strtoll(item, NULL, 0) <= 0xffffffff) {
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
  int result = _mgmtSrvr.dumpState(processId, pars, no);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
}

void CommandInterpreter::executeStatus(int processId, 
				       const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  
  ndb_mgm_node_status status;
  Uint32 startPhase, version, dynamicId, nodeGroup, connectCount;
  bool system;
  int result = _mgmtSrvr.status(processId, 
				&status, &version, &startPhase, &system,
				&dynamicId, &nodeGroup, &connectCount);
  if(result != 0){
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
    return;
  }
  
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
  if(status != NDB_MGM_NODE_STATUS_NO_CONTACT){
    
    ndbout_c(" (Version %d.%d.%d)", 
	     getMajor(version) ,
	     getMinor(version),
	     getBuild(version));
    
    // NOTE It's possible to print dynamicId  and nodeGroup here ...
    //  ndbout << ", " <<dynamicId<<", "<<nodeGroup<<endl;
  }
}



//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeLogLevel(int processId, 
					 const char* parameters, bool all) {
#if 0
  (void)all;  // Don't want compiler warning
  SetLogLevelOrd logLevel; logLevel.clear();
  
  if (emptyString(parameters) || (strcmp(parameters, "ALL") == 0)) {
    for(Uint32 i = 0; i<EventLoggerBase::noOfEventCategoryNames; i++)
      logLevel.setLogLevel(EventLoggerBase::eventCategoryNames[i].category, 7);
  } else {

    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, ", ", &tmpPtr);
    while(item != NULL){
      char categoryTxt[255];
      int level;
      const int m = sscanf(item, "%[^=]=%d", categoryTxt, &level);
      if(m != 2){
	free(tmpString);
	ndbout << "Invalid loglevel specification category=level" << endl;
	return;
      }
      LogLevel::EventCategory cat;
      if(!EventLoggerBase::matchEventCategory(categoryTxt,
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

  int result = _mgmtSrvr.setNodeLogLevel(processId, logLevel);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}



//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeError(int processId, 
				      const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

  if (emptyString(parameters)) {
    ndbout << "Missing error number." << endl;
    return;
  }
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

  int result = _mgmtSrvr.insertError(processId, errorNo);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
  free(newpar);
}



//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeTrace(int processId, 
				      const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

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
}



//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeLog(int processId, 
				    const char* parameters, bool all) {
  
  (void)all;  // Don't want compiler warning

  Vector<BaseString> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }
  
  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::InOut, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }

}



//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeLogIn(int processId, 
				      const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

  Vector<BaseString> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }

  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::In, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
}

//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeLogOut(int processId, 
				       const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

  Vector<BaseString> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }


  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::Out, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }

}


//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeLogOff(int processId, 
				       const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning 

  Vector<BaseString> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }


  int result = _mgmtSrvr.setSignalLoggingMode(processId, MgmtSrvr::Off, blocks);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }

}

//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeTestOn(int processId, 
				       const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning

  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

  int result = _mgmtSrvr.startSignalTracing(processId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }

}

//******************************************************************************
//******************************************************************************
void CommandInterpreter::executeTestOff(int processId, 
					const char* parameters, bool all) {

  (void)all;  // Don't want compiler warning 

  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

  int result = _mgmtSrvr.stopSignalTracing(processId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }

}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeEventReporting(int processId, 
					       const char* parameters, 
					       bool all) {
#if 0
  (void)all;  // Don't want compiler warning
  SetLogLevelOrd logLevel; logLevel.clear();
  
  if (emptyString(parameters) || (strcmp(parameters, "ALL") == 0)) {
    for(Uint32 i = 0; i<EventLoggerBase::noOfEventCategoryNames; i++)
      logLevel.setLogLevel(EventLoggerBase::eventCategoryNames[i].category, 7);
  } else {

    char * tmpString = strdup(parameters);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, ", ", &tmpPtr);
    while(item != NULL){
      char categoryTxt[255];
      int level;
      const int m = sscanf(item, "%[^=]=%d", categoryTxt, &level);
      if(m != 2){
	free(tmpString);
	ndbout << "Invalid loglevel specification category=level" << endl;
	return;
      }
      LogLevel::EventCategory cat;
      if(!EventLoggerBase::matchEventCategory(categoryTxt,
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
  ndbout_c("processId %d", processId);
  int result = _mgmtSrvr.setEventReportingLevel(processId, logLevel);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  }
#endif
}

void
CommandInterpreter::executeStartBackup(char* parameters) {
  Uint32 backupId;
  int result = _mgmtSrvr.startBackup(backupId);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  } else {
    //    ndbout << "Start of backup ordered" << endl;
  }
}

void
CommandInterpreter::executeAbortBackup(char* parameters) {
  strtok(parameters, " ");
  char* id = strtok(NULL, "\0");
  int bid = -1;
  if(id == 0 || sscanf(id, "%d", &bid) != 1){
    ndbout << "Invalid arguments: expected <BackupId>" << endl;
    return;
  }
  int result = _mgmtSrvr.abortBackup(bid);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  } else {
    ndbout << "Abort of backup " << bid << " ordered" << endl;
  }
}



void
CommandInterpreter::executeEnterSingleUser(char* parameters) {
  strtok(parameters, " ");
  char* id = strtok(NULL, " ");
  id = strtok(NULL, " ");
  id = strtok(NULL, "\0");
  int nodeId = -1;
  if(id == 0 || sscanf(id, "%d", &nodeId) != 1){
    ndbout << "Invalid arguments: expected <NodeId>" << endl;
    return;
  }
  int result = _mgmtSrvr.enterSingleUser(0, nodeId,0,0);
  if (result != 0) {
    ndbout << _mgmtSrvr.getErrorText(result) << endl;
  } else {
    ndbout << "Entering single user mode, granting access for node " 
	   << nodeId << " OK." << endl;
  }
}

void CommandInterpreter::executeExitSingleUser(char* parameters) {
  _mgmtSrvr.exitSingleUser(0,0,0,0);
}


#include <NdbApiSignal.hpp>

void 
CommandInterpreter::jonas(int processId, const char* parameters, bool all) {

  MgmtSrvr::Area51 tmp = _mgmtSrvr.getStuff();
  
  NdbApiSignal signal(0);
  Uint32 * theData = signal.getDataPtrSend();
  Uint32 data[25];
  Uint32 sec0[70];
  Uint32 sec1[123];

  data[0] = 12;
  data[1] = 13;

  unsigned i; 
  for(i = 0; i<70; i++)
    sec0[i] = i;
  
  for(i = 0; i<123; i++)
    sec1[i] = 70+i;
  
  signal.set(0, CMVMI, GSN_TESTSIG, 3);  
  signal.m_noOfSections = 2;
  signal.m_fragmentInfo = 1;
  
  LinearSectionPtr ptr[3];

  theData[0] = 3;
  theData[1] = 0;
  theData[2] = 7; // FragmentId

  ptr[0].sz = 2;
  ptr[0].p = &data[0];
  
  ptr[1].sz = 60;
  ptr[1].p = &sec0[0];
  
  tmp.theFacade->lock_mutex();
  tmp.theRegistry->prepareSend(&signal, 1, theData, processId, ptr);
  tmp.theFacade->unlock_mutex();
  
  signal.set(0, CMVMI, GSN_TESTSIG, 3);
  signal.m_noOfSections = 2;
  signal.m_fragmentInfo = 3;
  
  theData[0] = 0;
  theData[1] = 1;
  theData[2] = 7; // FragmentId
  
  ptr[0].sz = 10;
  ptr[0].p = &sec0[60];

  ptr[1].sz = 123;
  ptr[1].p = &sec1[0];

  tmp.theFacade->lock_mutex();
  tmp.theRegistry->prepareSend(&signal, 1, theData, processId, ptr);
  tmp.theFacade->unlock_mutex();
}
