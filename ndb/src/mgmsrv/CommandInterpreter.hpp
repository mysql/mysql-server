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

#ifndef CommandInterpreter_H
#define CommandInterpreter_H

//*****************************************************************************
// Author: Peter Lind
//*****************************************************************************

#include <ndb_global.h>
#include <Vector.hpp>
#include <BaseString.hpp>

class MgmtSrvr;

/** 
 *  @class CommandInterpreter
 *  @brief Reads command line in management client
 *
 *  This class has one public method which reads a command line 
 *  from a stream. It then interpret that commmand line and calls a suitable 
 *  method in the MgmtSrvr class which executes the command.
 *
 *  For command syntax, see the HELP command.
 */ 
class CommandInterpreter {
public:
  /**
   *   Constructor
   *   @param mgmtSrvr: Management server to use when executing commands
   */
  CommandInterpreter(MgmtSrvr& mgmtSrvr);
  
  /**
   *   Reads one line from the stream, parse the line to find 
   *   a command and then calls a suitable method which executes 
   *   the command.
   *
   *   @return true until quit/bye/exit has been typed
   */
  int readAndExecute();

private:
  char m_err_str[1024];
  const char *get_error_text(int err_no);

  /**
   *   Read a string, and return a pointer to it.  
   *
   *   @return NULL on EOF. 
   */
  char *readline_gets ()
  {
    static char linebuffer[254];
    static char *line_read = (char *)NULL;

    /* If the buffer has already been allocated, return the memory
       to the free pool. */
    if (line_read)
    {
      free (line_read);
      line_read = (char *)NULL;
    }
    
    /* Get a line from the user. */
    fputs("ndb_mgmd> ", stdout);
    linebuffer[sizeof(linebuffer)-1]=0;
    line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
    if (line_read == linebuffer) {
      char *q=linebuffer;
      while (*q > 31) q++;
      *q=0;
      line_read= strdup(linebuffer);
    }
    return (line_read);
  }
  
  /**
   *   Analyse the command line, after the first token.
   *
   *   @param  processId:           DB process id to send command to or -1 if
   *                                command will be sent to all DB processes.
   *   @param  allAfterFirstToken:  What the client gave after the 
   *                                first token on the command line
   */
  void analyseAfterFirstToken(int processId, char* allAfterFirstTokenCstr);

  /**
   *   Parse the block specification part of the LOG* commands,
   *   things after LOG*: [BLOCK = {ALL|<blockName>+}]
   *
   *   @param  allAfterLog: What the client gave after the second token 
   *                        (LOG*) on the command line
   *   @param  blocks, OUT: ALL or name of all the blocks
   *   @return: true if correct syntax, otherwise false
   */
  bool parseBlockSpecification(const char* allAfterLog, 
			       Vector<BaseString>& blocks);
  
  /**
   *   A bunch of execute functions: Executes one of the commands
   *
   *   @param  processId:   DB process id to send command to
   *   @param  parameters:  What the client gave after the command name 
   *                        on the command line.
   *   For example if complete input from user is: "1 LOGLEVEL 22" then the
   *   parameters argument is the string with everything after LOGLEVEL, in this
   *   case "22". Each function is responsible to check the parameters argument.
   */
  void executeHelp(char* parameters);
  void executeShow(char* parameters);
  void executeRun(char* parameters);
  void executeInfo(char* parameters);
  void executeClusterLog(char* parameters);

public:
  void executeStop(int processId, const char* parameters, bool all);
  void executeStart(int processId, const char* parameters, bool all);
  void executeRestart(int processId, const char* parameters, bool all);
  void executeLogLevel(int processId, const char* parameters, bool all);
  void executeError(int processId, const char* parameters, bool all);
  void executeTrace(int processId, const char* parameters, bool all);
  void executeLog(int processId, const char* parameters, bool all);
  void executeLogIn(int processId, const char* parameters, bool all);
  void executeLogOut(int processId, const char* parameters, bool all);
  void executeLogOff(int processId, const char* parameters, bool all);
  void executeTestOn(int processId, const char* parameters, bool all);
  void executeTestOff(int processId, const char* parameters, bool all);
  void executeStatus(int processId, const char* parameters, bool all);
  void executeEnterSingleUser(char* parameters);
  void executeExitSingleUser(char* parameters);
  void executeEventReporting(int processId, const char* parameters, bool all);
  void executeDumpState(int processId, const char* parameters, bool all);
  void executeStartBackup(char * pars);
  void executeAbortBackup(char * pars);

  void jonas(int processId, const char* parameters, bool all);  

  /**
   * A execute function definition
   */
public:
  typedef void (CommandInterpreter::* ExecuteFunction)(int processId, 
						       const char * param, 
						       bool all);
  
  struct CommandFunctionPair {
    const char * command;
    ExecuteFunction executeFunction;
  };
private:
  /**
   * 
   */
  void executeForAll(const char * cmd, ExecuteFunction fun, const char * param);

  /**
   *   Management server to use when executing commands
   */
  MgmtSrvr& _mgmtSrvr; 
};

#endif // CommandInterpreter_H
