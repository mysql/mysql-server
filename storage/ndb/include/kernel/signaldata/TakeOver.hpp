/* 
   Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TAKE_OVER_HPP
#define TAKE_OVER_HPP

#define JAM_FILE_ID 164


struct StartCopyReq
{
  enum Flags {
    WAIT_LCP = 1
  };

  Uint32 startingNodeId;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 flags;
  
  STATIC_CONST( SignalLength = 4 );
};

struct StartCopyRef
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct StartCopyConf
{
  Uint32 startingNodeId;
  Uint32 senderRef;
  Uint32 senderData;

  STATIC_CONST( SignalLength = 3 );
};

struct StartToReq 
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 startingNodeId;
};

struct StartToRef 
{
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra;
};

struct StartToConf 
{
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct UpdateToReq 
{
  STATIC_CONST( SignalLength = 7 );

  enum RequestType 
  {
    BEFORE_STORED = 7
    ,AFTER_STORED = 8
    ,BEFORE_COMMIT_STORED = 9
    ,AFTER_COMMIT_STORED = 10
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType;
  Uint32 startingNodeId;
  Uint32 copyNodeId;
  Uint32 tableId;
  Uint32 fragmentNo;
};

struct UpdateToRef 
{
  STATIC_CONST( SignalLength = 4 );
  
  enum ErrorCode {
    CopyNodeInProgress = 1  // StartMe++
    ,CopyFragInProgress = 2 // NG busy
    ,UnknownRequest = 3
    ,InvalidRequest = 4
    ,UnknownTakeOver = 5
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra;
};

struct UpdateToConf 
{
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct UpdateFragStateReq 
{
  STATIC_CONST( SignalLength = 9 );

  enum ReplicaType {
    STORED = 7,
    COMMIT_STORED = 9,
    START_LOGGING = 10
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 startingNodeId;
  Uint32 copyNodeId;
  Uint32 startGci;
  Uint32 replicaType;
  Uint32 failedNodeId;
};

struct UpdateFragStateConf 
{
  STATIC_CONST( SignalLength = 6 );
  
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 failedNodeId;
};

struct EndToReq 
{
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 startingNodeId;
  Uint32 flags; 
};

struct EndToRef
{
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra;
};

struct EndToConf 
{
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct EndToRep
{
public:
  STATIC_CONST ( SignalLength = 1 );

  Uint32 nodeId;
};

#undef JAM_FILE_ID

#endif
