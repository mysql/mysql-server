/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef READ_NODESCONF_HPP
#define READ_NODESCONF_HPP

#include <NodeBitmask.hpp>

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
  friend class Grep;

public:
  STATIC_CONST( SignalLength = 2 + 6*NodeBitmask::Size );
private:
  
  Uint32 noOfNodes;

  /**
   * 
   * NOTE Not valid when send from Qmgr
   */
  Uint32 masterNodeId;

  /**
   * This array defines all the ndb nodes in the system
   */
  Uint32 allNodes[NodeBitmask::Size];
  
  /**
   * This array describes wheather the nodes are currently active
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 inactiveNodes[NodeBitmask::Size];

  /**
   * This array describes the version id of the nodes
   *  The version id is a 4 bit number
   *
   * NOTE Not valid when send from Qmgr
   */
  Uint32 theVersionIds[4*NodeBitmask::Size];

  static void  setVersionId(NodeId, Uint8 versionId, Uint32 theVersionIds[]);
  static Uint8 getVersionId(NodeId, const Uint32 theVersionIds[]);
};

inline
void
ReadNodesConf::setVersionId(NodeId nodeId, Uint8 versionId,
			    Uint32 theVersionIds[]){
  const int word  = nodeId >> 3;
  const int shift = (nodeId & 7) << 2;

  const Uint32 mask = ~(((Uint32)15) << shift);
  const Uint32 tmp = theVersionIds[word];
  
  theVersionIds[word] = (tmp & mask) | ((((Uint32)versionId) & 15) << shift);
}

inline
Uint8
ReadNodesConf::getVersionId(NodeId nodeId, const Uint32 theVersionIds[]){
  const int word  = nodeId >> 3;
  const int shift = (nodeId & 7) << 2;
  
  return (theVersionIds[word] >> shift) & 15;
}

#endif
