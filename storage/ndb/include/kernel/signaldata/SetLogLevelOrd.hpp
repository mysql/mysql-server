/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SET_LOGLEVEL_ORD_HPP
#define SET_LOGLEVEL_ORD_HPP

#include <LogLevel.hpp>
#include "EventSubscribeReq.hpp"
#include "SignalData.hpp"

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

#endif
