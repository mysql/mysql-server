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
#include <m_string.h>

//******************************************************************************
//******************************************************************************
CommandInterpreter::CommandInterpreter(MgmtSrvr& mgmtSrvr) :
  _mgmtSrvr(mgmtSrvr) {
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

const char *CommandInterpreter::get_error_text(int err_no)
{
  return _mgmtSrvr.getErrorText(err_no, m_err_str, sizeof(m_err_str));
}

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
  
  if (strcmp(firstToken, "ALL") == 0) {
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

//*****************************************************************************
//*****************************************************************************

void
stopCallback(int nodeId, void * anyData, int errCode){
  if(errCode == 0){
    if(nodeId == 0)
      ndbout << "\nCluster has shutdown" << endl;
    else
      ndbout << "\nNode " << nodeId << " has shutdown" << endl;
  } else {
    MgmtSrvr * mgm = (MgmtSrvr *)anyData;
    char err_str[1024];
    ndbout << "Node " << nodeId << " has not shutdown: " 
	   << mgm->getErrorText(errCode,err_str,sizeof(err_str)) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
  int error;
  while(item != NULL){
	  if (0x0 <= my_strtoll10(item, NULL, &error) && my_strtoll10(item, NULL, &error) <= 0xffffffff) {
      pars[no] = my_strtoll10(item, NULL, &error); 
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
    ndbout << get_error_text(result) << endl;
  }
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
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
    ndbout << get_error_text(result) << endl;
  }

}
