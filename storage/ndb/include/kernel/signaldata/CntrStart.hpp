/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#ifndef CNTR_START_HPP
#define CNTR_START_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 191


/**
 * 
 */
class CntrStartReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REQ(FILE*, const Uint32 *, Uint32, Uint16);
  
public:
  static constexpr Uint32 OldSignalLength = 3;
  static constexpr Uint32 SignalLength = 4;
private:
  
  Uint32 nodeId;
  Uint32 startType;
  Uint32 lastGci;
  Uint32 lastLcpId;
};

class CntrStartRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REF(FILE*, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 2;

  enum ErrorCode {
    OK = 0,
    NotMaster = 1,
    StopInProgress = 2
  };
private:
  
  Uint32 errorCode;
  Uint32 masterNodeId;
};

class CntrStartConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend struct UpgradeStartup;
  
  friend bool printCNTR_START_CONF(FILE*, const Uint32 *, Uint32, Uint16);

public:
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 SignalLength_v1 = 4 + 2 * 2;
  
private:
  
  Uint32 startType;
  Uint32 startGci;
  Uint32 masterNodeId;
  Uint32 noStartNodes;
  Uint32 startedNodes_v1[NdbNodeBitmask48::Size];
  Uint32 startingNodes_v1[NdbNodeBitmask48::Size];
};

struct CntrWaitRep
{
  Uint32 nodeId;
  Uint32 waitPoint;

  // Below words only used for Grant not for WaitFor.
  // WaitFor ZWAITPOINT_4_2 also pass node bitmask in section.
  // For old versions ZWAITPOINT_4_2 pass a two word bitmask in signal here.
  Uint32 request;
  Uint32 sp;

  enum Request
  {
    WaitFor = 1,
    Grant = 2
  };

  static constexpr Uint32 SignalLength = 4;

  enum WaitPos
  {
    ZWAITPOINT_4_1  = 1
    ,ZWAITPOINT_4_2 = 2
    ,ZWAITPOINT_5_1 = 3
    ,ZWAITPOINT_5_2 = 4
    ,ZWAITPOINT_6_1 = 5
    ,ZWAITPOINT_6_2 = 6
    ,ZWAITPOINT_7_1 = 7
    ,ZWAITPOINT_7_2 = 8
    ,ZWAITPOINT_4_2_TO = 9 // We are forced to TO (during SR)
  };
};


#undef JAM_FILE_ID

#endif
