/*
   Copyright (C) 2003, 2005-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef DROP_TABLE_HPP
#define DROP_TABLE_HPP

#include "SignalData.hpp"

struct DropTableReq {
  STATIC_CONST( SignalLength = 7 );

  union { Uint32 clientRef, senderRef; };
  union { Uint32 clientData, senderData; };
  Uint32 requestInfo;
  Uint32 transId;
  Uint32 transKey;
  Uint32 tableId;
  Uint32 tableVersion;
};

struct DropTableConf {
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId; 
  Uint32 tableVersion;
};

struct DropTableRef {
  STATIC_CONST( SignalLength = 9 );
  
  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId; 
  Uint32 tableVersion;
  Uint32 errorCode; 
  Uint32 errorLine;
  Uint32 errorNodeId;
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
    SingleUser = 299,
    ActiveSchemaTrans = 785
  };
};

#endif
