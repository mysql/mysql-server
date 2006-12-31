/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef CNTR_START_HPP
#define CNTR_START_HPP

#include <NodeBitmask.hpp>

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
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 nodeId;
  Uint32 startType;
  Uint32 lastGci;
};

class CntrStartRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REF(FILE*, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

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
  STATIC_CONST( SignalLength = 4 + 2 * NdbNodeBitmask::Size );
  
private:
  
  Uint32 startType;
  Uint32 startGci;
  Uint32 masterNodeId;
  Uint32 noStartNodes;
  Uint32 startedNodes[NdbNodeBitmask::Size];
  Uint32 startingNodes[NdbNodeBitmask::Size];
};

#endif
