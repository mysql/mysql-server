/* 
   Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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
  
  static constexpr Uint32 SignalLength = 4;
};

struct StartCopyRef
{
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct StartCopyConf
{
  Uint32 startingNodeId;
  Uint32 senderRef;
  Uint32 senderData;

  static constexpr Uint32 SignalLength = 3;
};

struct StartToReq 
{
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 startingNodeId;
};

struct StartToRef 
{
  static constexpr Uint32 SignalLength = 3;
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra;
};

struct StartToConf 
{
  static constexpr Uint32 SignalLength = 3;
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct UpdateToReq 
{
  static constexpr Uint32 SignalLength = 7;

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
  static constexpr Uint32 SignalLength = 4;
  
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
  static constexpr Uint32 SignalLength = 3;
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct UpdateFragStateReq 
{
  static constexpr Uint32 SignalLength = 9;

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
  static constexpr Uint32 SignalLength = 6;
  
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 failedNodeId;
};

struct EndToReq 
{
  static constexpr Uint32 SignalLength = 4;
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 startingNodeId;
  Uint32 flags; 
};

struct EndToRef
{
  static constexpr Uint32 SignalLength = 4;
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra;
};

struct EndToConf 
{
  static constexpr Uint32 SignalLength = 3;
  
  Uint32 senderData;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};

struct EndToRep
{
public:
  static constexpr Uint32 SignalLength = 1;

  Uint32 nodeId;
};

#undef JAM_FILE_ID

#endif
