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

#include "RepCommandInterpreter.hpp"

static const char* 
helpTextRep =
"+-------------------------------------------------------------------------+\n"
"|                        MySQL Replication Server                         |\n"
"|      Commands should be executed on the standby Replication Server      |\n"
"+-------------------------------------------------------------------------+\n"
"|                             Simple Commands                             |\n"
"+-------------------------------------------------------------------------+\n"
"| START              Start replication                                    |\n"
"| STATUS             Show replication status                              |\n"
"+-------------------------------------------------------------------------+\n"
"|                            Advanced Commands                            |\n"
"+-------------------------------------------------------------------------+\n"
"| STOP <epoch no>    Stop replication after epoch number <epoch no>       |\n"
"| STOP IMMEDIATELY   Stop replication after applying the current epoch    |\n"
"| ADD TABLE <db>/<schema>/<tablename>                                     |\n"
"|                    Note: <db>/<schema>/<tablename> is case sensitive!   |\n"
"|                    Use 'STATUS' to see added tables.                    |\n"
"| REMOVE TABLE <db>/<schema>/<tablename>                                  |\n"
"|                    Note: <db>/<schema>/<tablename> is case sensitive!   |\n"
"| ENABLE <protocol>  Starts protocol                                      |\n"
"| DISABLE <protocol> Stops protocol                                       |\n"
"| DEBUG              Toggle logging of replication messages on console    |\n"
"|                                                                         |\n"
"| <protocol>  ::=    REQUESTOR | TRANSFER | APPLY | DELETE                |\n"
"+-------------------------------------------------------------------------+\n"
;

/**
 * @todo
"| <protocol>  ::=    SUBID | SUBSCRIPTION                                 |\n"
"| <protocol>  ::=    METALOG | METASCAN | DATALOG | DATASCAN              |\n"
"| <system>    ::=    PRIMARY | STANDBY | TWOWAY                           |\n"
"| CONNECT <system>   Connects to NDB Cluster and other replication server |\n"
"| DELETE             Removes all epochs stored in replication servers     |\n"
"| DROP <tableid>     Drops table in standby system identified by table id |\n"
"| <epoch>     ::=    Any integer (naming the last epoch to be applied)    |\n"
*/

RepCommandInterpreter::RepCommandInterpreter(RepComponents * comps)
{
  m_repComponents = comps;
  m_repState = comps->getRepState();
}

RepCommandInterpreter::~RepCommandInterpreter()
{
}

/**
 *   Read a string, and return a pointer to it.  
 *
 *   @return NULL on EOF. 
 */
char *
RepCommandInterpreter::readline_gets() const
{
  static char *line_read = (char *)NULL;
  
  // Disable the default file-name completion action of TAB
  // rl_bind_key ('\t', rl_insert);
  
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
  {
    NdbMem_Free(line_read);
    line_read = (char *)NULL;
  }
  
  /* Get a line from the user. */
  line_read = readline ("REP> ");
  
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
  
  return (line_read);
}

bool emptyString(const char* s) 
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

/**
 * Converts a string to a Uint32 pointed value!
 */
bool convert(const char* s, Uint32 * val) 
{
  if (s == NULL) {
    return false;
  }

  if (strlen(s) == 0) {
    return false;
  }

  errno = 0;
  char* p;
  long v = strtol(s, &p, 10);
  if (errno != 0) {
    return false;
  }
  if (p != &s[strlen(s)]) {
    return false;
  }

  *val = v;
  return true;
}

void
printError(GrepError::Code err) 
{
  if (err == GrepError::NO_ERROR) { ndbout << "Ok" << endl; } 
  else { ndbout << GrepError::getErrorDesc(err) << endl; }
}

bool
RepCommandInterpreter::readAndExecute() 
{
  GrepError::Code err;

  char* _line = readline_gets(); 
  char * line;
  if(_line == NULL) {
    ndbout << endl;
    return true;
  }
  
  line = strdup(_line);
  
  if (emptyString(line)) {
    return true;
  }
  
  /* I have to uncomment this, since otherwise <db>/<schema>/<table>
     is converted to capitals, but it is case sensitive!
  for (unsigned int i = 0; i < strlen(line); ++i) {
    line[i] = toupper(line[i]);
  }
  */
  // if there is anything in the line proceed
  char* firstToken = strtok(line, " ");
  for (unsigned int i = 0; i < strlen(firstToken); ++i) {
    firstToken[i] = toupper(firstToken[i]);
  }
  char* allAfterFirstToken = strtok(NULL, "\0");
  
  /**
   * Commands for REP Client only
   */
  if (strcmp(firstToken, "ADD") == 0) {    
    if (m_repState->m_channel.getStateSub() != 
	Channel::NO_SUBSCRIPTION_EXISTS) {
      ndbout_c("Subscription already exists");
      ndbout_c("Tables must be added before subscription exists");
      return true;
    }
    char * secondToken = strtok(allAfterFirstToken, " ");
    char * fullTableName = strtok(NULL, "\0");
    if(fullTableName == NULL) {
      ndbout_c("Table name not specified");
      return true;
    }
    for (unsigned int i = 0; i < strlen(secondToken); ++i) {
      secondToken[i] = toupper(secondToken[i]);
    }
    
    if (strcmp(secondToken, "TABLE") == 0) {
      err = m_repState->protectedAddTable(fullTableName);
      printError(err);
      return true;
    }
    return true;
  }
  if (strcmp(firstToken, "REMOVE") == 0) { 
    if (m_repState->m_channel.getStateSub() != 
	Channel::NO_SUBSCRIPTION_EXISTS) {
      ndbout_c("Subscription already exists");
      ndbout_c("Tables can not be removed after subscription is created");
      return true;
    }
    char * secondToken = strtok(allAfterFirstToken, " ");
    char * fullTableName = strtok(NULL, "\0");
    if(fullTableName == NULL) {
      ndbout_c("Table name not specified");
      return true;
    }
    for (unsigned int i = 0; i < strlen(secondToken); ++i) {
      secondToken[i] = toupper(secondToken[i]);
    }
    
    if (strcmp(secondToken, "TABLE") == 0) {
      err = m_repState->protectedRemoveTable(fullTableName);
      printError(err);
      return true;
    }
    return true;
  }
  /**
   * now, we can convert allAfterFirstToken to capitals
   */
  if(allAfterFirstToken != 0) {
    for (unsigned int i = 0; i < strlen(allAfterFirstToken); ++i) {
      allAfterFirstToken[i] = toupper(allAfterFirstToken[i]);
    }
  }
  if (strcmp(firstToken, "CONNECT") == 0) {

    if (strcmp(allAfterFirstToken, "PRIMARY") == 0) {
      m_repComponents->connectPS();
      return true;
    }
    if (strcmp(allAfterFirstToken, "STANDBY") == 0) {
      m_repComponents->connectPS();
      return true;
    }
    if (strcmp(allAfterFirstToken, "TWOWAY") == 0) {
      m_repComponents->connectPS();
      return true;
    }
    ndbout_c("Unknown argument: %s to command: %s", 
	     allAfterFirstToken, firstToken);
    return true;
  }

  if (strcmp(firstToken, "HELP") == 0) {
    ndbout << helpTextRep;
    return true;
  }

  if (strcmp(firstToken, "QUIT") == 0 || 
      strcmp(firstToken, "BYE") == 0 || 
      strcmp(firstToken, "EXIT") == 0) {
    return false;
  }

  /**
   * Commands for REP Server API
   */
  if (strcmp(firstToken, "STATUS") == 0 ||
      strcmp(firstToken, "INFO") == 0 ||
      strcmp(firstToken, "I") == 0) {
    m_repState->protectedRequest(GrepReq::STATUS, 0);
    return true;
  }

  if (strcmp(firstToken, "DEBUG") == 0) {
    if (replogEnabled) 
    {
      ndbout_c("Debugging disabled.");
      replogEnabled = false; 
    }
    else 
    {
      ndbout_c("Debugging enabled.");
      replogEnabled = true;
    }
    return true;
  }

  if (strcmp(firstToken, "ENABLE") == 0) {
    if (strcmp(allAfterFirstToken, "REQUESTOR") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_REQUESTOR, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "TRANSFER") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_TRANSFER, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "APPLY") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_APPLY, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DELETE") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_DELETE, 0);
      printError(err);
      return true;
    }
    ndbout_c("Unknown argument: %s to command: %s", 
	     allAfterFirstToken, firstToken);
    return true;
  }

  if (strcmp(firstToken, "DISABLE") == 0) {
    if (strcmp(allAfterFirstToken, "REQUESTOR") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_REQUESTOR, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "TRANSFER") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_TRANSFER, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "APPLY") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_APPLY, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DELETE") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_DELETE, 0);
      printError(err);
      return true;
    }
    ndbout_c("Unknown argument: %s to command: %s", 
	     allAfterFirstToken, firstToken);
    return true;
  }

  if (strcmp(firstToken, "START") == 0) {
    if (allAfterFirstToken == NULL) {
      err = m_repState->protectedRequest(GrepReq::START, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "SUBID") == 0) {
      err = m_repState->protectedRequest(GrepReq::CREATE_SUBSCR, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "SUBSCR") == 0 ||
        strcmp(allAfterFirstToken, "SUBSCRIPTION") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_SUBSCR, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "METALOG") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_METALOG, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "METASCAN") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_METASCAN, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DATALOG") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_DATALOG, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DATASCAN") == 0) {
      err = m_repState->protectedRequest(GrepReq::START_DATASCAN, 0);
      printError(err);
      return true;
    }
    ndbout_c("Unknown argument: %s to command: %s", 
	     allAfterFirstToken, firstToken);
    return true;
  }

  if (strcmp(firstToken, "STOP") == 0) {
    if (allAfterFirstToken == NULL) {
      ndbout_c("Please use either 'STOP IMMEDIATELY' or 'STOP <epoch no>', "
	       "where\n<epoch no> is greater than or equal to "
	       "the last applied epoch.");
      return true;
    }
    
    char * secondToken = strtok(allAfterFirstToken, " ");
    char * subscription = strtok(NULL, "\0");
    if (strcmp(secondToken, "SUBSCR") == 0 ||
        strcmp(secondToken, "SUBSCRIPTION") == 0) {
      char * sSubId  = strtok(subscription," ");
      char * sSubKey = strtok(NULL, "\0");
      int subId = atoi(sSubId);
      int subKey = atoi(sSubKey);
      err = m_repState->protectedRequest(GrepReq::STOP_SUBSCR, subId, subKey );
      printError(err);
      return true;
    }

    if (strcmp(allAfterFirstToken, "SUBID") == 0) {
      ndbout_c("Not implemented");
      return true;
    }
    
    
    if (strcmp(allAfterFirstToken, "METALOG") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_METALOG, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "METASCAN") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_METASCAN, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DATALOG") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_DATALOG, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "DATASCAN") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP_DATASCAN, 0);
      printError(err);
      return true;
    }
    if (strcmp(allAfterFirstToken, "IM") == 0 ||
	strcmp(allAfterFirstToken, "IMM") == 0 ||
	strcmp(allAfterFirstToken, "IMMEDIATELY") == 0) {
      err = m_repState->protectedRequest(GrepReq::STOP, 0);
      printError(err);
      return true;
    }
    Uint32 stopEpoch;
    if (convert(allAfterFirstToken, &stopEpoch)) {
      err = m_repState->protectedRequest(GrepReq::STOP, stopEpoch);
      printError(err);
      return true;
    }

    ndbout_c("Unknown argument: %s to command: %s", 
	     allAfterFirstToken, firstToken);
    return true;
  }

  ndbout_c("Unknown Command: %s", firstToken);
  ndbout_c("Type HELP for help.");
  ndbout << endl;
  return true;
}
