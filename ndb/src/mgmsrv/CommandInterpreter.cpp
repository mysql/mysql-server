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
  { "TRACE", &CommandInterpreter::executeTrace }
  ,{ "LOGIN", &CommandInterpreter::executeLogIn }
  ,{ "LOGOUT", &CommandInterpreter::executeLogOut }
  ,{ "LOGOFF", &CommandInterpreter::executeLogOff }
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
    return;
  }
  
  if(processId == -1){
    executeForAll(command, fun, allAfterSecondToken);
  } else {
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
  while(_mgmtSrvr.getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    ndbout << "Executing " << cmd << " on node: " 
	   << nodeId << endl << endl;
    (this->*fun)(nodeId, allAfterSecondToken, true);
    ndbout << endl;
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
