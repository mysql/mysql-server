/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
  static constexpr Uint32 SignalLength = 3;

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
  static constexpr Uint32 SignalLength = 4;
  
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
  static constexpr Uint32 SignalLength = 6 + NodeState::DataLength;
private:
  
  Uint32 qmgrRef;
  Uint32 version; // Version of NDB node
  Uint32 apiHeartbeatFrequency;
  Uint32 mysql_version;
  Uint32 minDbVersion;
  NodeStatePOD nodeState;
  Uint32 minApiVersion;
};


#undef JAM_FILE_ID

#endif
