/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DROP_TABLE_HPP
#define DROP_TABLE_HPP

#include "SignalData.hpp"

class DropTableReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  
public:
  STATIC_CONST( SignalLength = 4 );
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 tableId; 
  Uint32 tableVersion;
};

class DropTableRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  
public:
  STATIC_CONST( SignalLength = 6 );
  
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 tableId; 
  Uint32 tableVersion;
  Uint32 errorCode; 
  Uint32 masterNodeId;
  
  enum ErrorCode {
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    NoSuchTable         = 709,
    InvalidTableVersion = 241,
    DropInProgress      = 283,
    NoDropTableRecordAvailable = 1229,
    BackupInProgress = 761,
    SingleUser = 299
  };
};

class DropTableConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  
public:
  STATIC_CONST( SignalLength = 4 );
  
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 tableId; 
  Uint32 tableVersion;
};

#endif
