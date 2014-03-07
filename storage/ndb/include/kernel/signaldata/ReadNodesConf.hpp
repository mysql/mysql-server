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

#ifndef READ_NODESCONF_HPP
#define READ_NODESCONF_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 199


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

  friend bool printREAD_NODES_CONF(FILE*, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 + 5*NdbNodeBitmask::Size );
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
    Uint32 allNodes[NdbNodeBitmask::Size];
    Uint32 definedNodes[NdbNodeBitmask::Size];
  };  

  /**
   * This array describes wheather the nodes are currently active
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 inactiveNodes[NdbNodeBitmask::Size];

  Uint32 clusterNodes[NdbNodeBitmask::Size];  // From Qmgr
  Uint32 startingNodes[NdbNodeBitmask::Size]; // From Cntr
  Uint32 startedNodes[NdbNodeBitmask::Size];  // From Cntr
};


#undef JAM_FILE_ID

#endif
