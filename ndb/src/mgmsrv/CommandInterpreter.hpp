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

#include <ndb_global.h>
#include <Vector.hpp>
#include <BaseString.hpp>

class MgmtSrvr;

class CommandInterpreter {
public:
  CommandInterpreter(MgmtSrvr& mgmtSrvr);
  int readAndExecute();

private:
  char m_err_str[1024];
  const char *get_error_text(int err_no);

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
  
  void analyseAfterFirstToken(int processId, char* allAfterFirstTokenCstr);
  bool parseBlockSpecification(const char* allAfterLog, 
			       Vector<BaseString>& blocks);

public:
  void executeTrace(int processId, const char* parameters, bool all);
  void executeLogIn(int processId, const char* parameters, bool all);
  void executeLogOut(int processId, const char* parameters, bool all);
  void executeLogOff(int processId, const char* parameters, bool all);

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
