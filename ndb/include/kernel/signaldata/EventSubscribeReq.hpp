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

#ifndef SD_EVENT_SUB_REQ_H
#define SD_EVENT_SUB_REQ_H

#include "SignalData.hpp"

/**
 * Requests change (new, update, delete) of event subscription,
 * i.e. forwarding of events.
 *
 * SENDER:  Mgm server
 * RECIVER: SimBlockCMCtrBlck
 */

struct EventSubscribeReq {
  /**
   * Receiver(s)
   */
  friend class Cmvmi;

  /**
   * Sender(s)
   */
  friend class MgmtSrvr;

  STATIC_CONST( SignalLength = 22 );

  /**
   * Note: If you use the same blockRef as you have used earlier, 
   *       you update your ongoing subscription
   */
  Uint32 blockRef;

  /**
   * If you specify 0 entries, it's the same as cancelling an
   * subscription
   */
  Uint32 noOfEntries;
  
  Uint32 theCategories[10];
  Uint32 theLevels[10];

  EventSubscribeReq& operator= (const LogLevel& ll){
    noOfEntries = _LOGLEVEL_CATEGORIES;
    for(size_t i = 0; i<noOfEntries; i++){
      theCategories[i] = i;
      theLevels[i] = ll.getLogLevel((LogLevel::EventCategory)i);
    }
    return * this;
  }
};

#endif
