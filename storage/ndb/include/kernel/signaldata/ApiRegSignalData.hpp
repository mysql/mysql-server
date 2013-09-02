/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef API_REGCONF_HPP
#define API_REGCONF_HPP

#include <NodeState.hpp>

#define JAM_FILE_ID 77


class ApiRegReq {
  /**
   * Sender(s)
   */
  friend class ClusterMgr;
  
  /**
   * Reciver(s)
   */
  friend class Qmgr;

public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 ref;
  Uint32 version; // Version of API node
  Uint32 mysql_version;
};

/**
 * 
 */
class ApiRegRef {
  /**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Reciver(s)
   */
  friend class ClusterMgr;

public:
  STATIC_CONST( SignalLength = 4 );
  
  enum ErrorCode {
    WrongType = 1,
    UnsupportedVersion = 2
  };
private:
  Uint32 ref; // Qmgr ref
  Uint32 version; // Version of NDB node
  Uint32 errorCode;
  Uint32 mysql_version;
};

/**
 * 
 */
class ApiRegConf {
  /**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Reciver(s)
   */
  friend class ClusterMgr;

public:
  STATIC_CONST( SignalLength = 5 + NodeState::DataLength );
private:
  
  Uint32 qmgrRef;
  Uint32 version; // Version of NDB node
  Uint32 apiHeartbeatFrequency;
  Uint32 mysql_version;
  Uint32 minDbVersion;
  NodeStatePOD nodeState;
};


#undef JAM_FILE_ID

#endif
