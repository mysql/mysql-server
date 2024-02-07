/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef READ_NODESCONF_HPP
#define READ_NODESCONF_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 199

class ReadNodesReq {
  friend class Qmgr;
  friend class Ndbcntr;

 public:
  static constexpr Uint32 OldSignalLength = 1;
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 myRef;
  Uint32 myVersion;
};

/**
 * This signals is sent by Qmgr to NdbCntr
 *   and then from NdbCntr sent to: dih, dict, lqh, tc
 *
 * NOTE Only noOfNodes & allNodes are valid when sent from Qmgr
 */
class ReadNodesConf {
  /**
   * Sender(s)
   */
  friend class Qmgr;

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;

  /**
   * Reciver(s)
   */
  friend class Dbdih;
  friend class Dbdict;
  friend class Dblqh;
  friend class Dbtc;
  friend class Trix;
  friend class Backup;
  friend class Suma;
  friend class LocalProxy;
  friend class Dbinfo;
  friend class Dbspj;

  friend bool printREAD_NODES_CONF(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 noOfNodes;
  Uint32 ndynamicId;

  /**
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 masterNodeId;

  // Below bitmasks are not part of signal.
  // All five are sent in first section.

  /**
   * This array defines all the ndb nodes in the system
   */
  NdbNodeBitmask definedNodes;

  /**
   * This array describes whether the nodes are currently active
   *
   * NOTE Not valid when send from Qmgr
   */
  NdbNodeBitmask inactiveNodes;

  NdbNodeBitmask clusterNodes;   // From Qmgr
  NdbNodeBitmask startingNodes;  // From Cntr
  NdbNodeBitmask startedNodes;   // From Cntr
};

class ReadNodesConf_v1 {
  /**
   * Sender(s)
   */
  friend class Qmgr;

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;

  /**
   * Reciver(s)
   */
  friend class Dbdih;
  friend class Dbdict;
  friend class Dblqh;
  friend class Dbtc;
  friend class Trix;
  friend class Backup;
  friend class Suma;
  friend class LocalProxy;
  friend class Dbinfo;
  friend class Dbspj;

  friend bool printREAD_NODES_CONF(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 3 + 5 * NdbNodeBitmask48::Size;

 private:
  Uint32 noOfNodes;
  Uint32 ndynamicId;

  /**
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 masterNodeId;

  /**
   * This array defines all the ndb nodes in the system
   */
  union {
    Uint32 allNodes[NdbNodeBitmask48::Size];
    Uint32 definedNodes[NdbNodeBitmask48::Size];
  };

  /**
   * This array describes whether the nodes are currently active
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 inactiveNodes[NdbNodeBitmask48::Size];

  Uint32 clusterNodes[NdbNodeBitmask48::Size];   // From Qmgr
  Uint32 startingNodes[NdbNodeBitmask48::Size];  // From Cntr
  Uint32 startedNodes[NdbNodeBitmask48::Size];   // From Cntr
};

#undef JAM_FILE_ID

#endif
