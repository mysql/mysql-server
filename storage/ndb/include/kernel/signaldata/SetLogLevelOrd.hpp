/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SET_LOGLEVEL_ORD_HPP
#define SET_LOGLEVEL_ORD_HPP

#include <LogLevel.hpp>
#include "EventSubscribeReq.hpp"
#include "SignalData.hpp"

#define JAM_FILE_ID 195


/**
 * 
 */
class SetLogLevelOrd {
  /**
   * Sender(s)
   */
  friend class MgmtSrvr; /* XXX can probably be removed */
  friend class MgmApiSession;
  friend class CommandInterpreter;
  
  /**
   * Reciver(s)
   */
  friend class Cmvmi;

  friend class NodeLogLevel;
  
private:
  STATIC_CONST( SignalLength = 1 + LogLevel::LOGLEVEL_CATEGORIES  );
  
  Uint32 noOfEntries;
  Uint32 theData[LogLevel::LOGLEVEL_CATEGORIES];
  
  void clear();
  
  /**
   * Note level is valid as 0-15
   */
  void setLogLevel(LogLevel::EventCategory ec, int level = 7);
  
  SetLogLevelOrd& assign (const LogLevel& ll){
    noOfEntries = LogLevel::LOGLEVEL_CATEGORIES;
    for(Uint32 i = 0; i<noOfEntries; i++){
      theData[i] = (i << 16) | ll.getLogLevel((LogLevel::EventCategory)i);
    }
    return * this;
  }

  SetLogLevelOrd& assign (const EventSubscribeReq& ll){
    noOfEntries = ll.noOfEntries;
    for(Uint32 i = 0; i<noOfEntries; i++){
      theData[i] = ll.theData[i];
    }
    return * this;
  }
};

inline
void
SetLogLevelOrd::clear(){
  noOfEntries = 0;
}

inline
void
SetLogLevelOrd::setLogLevel(LogLevel::EventCategory ec, int level){
  theData[noOfEntries] = (ec << 16) | level;
  noOfEntries++;
}


#undef JAM_FILE_ID

#endif
