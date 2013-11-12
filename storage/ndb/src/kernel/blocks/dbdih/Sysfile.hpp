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

#ifndef SYSFILE_HPP
#define SYSFILE_HPP

#include <ndb_types.h>
#include <ndb_limits.h>
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 357


/**
 * No bits in Sysfile to represent nodeid
 */
#define NODEID_BITS 8

/**
 * Constant representing that node do not belong to
 * any node group
 */
#define NO_NODE_GROUP_ID ((1 << NODEID_BITS) - 1)

/**
 * No of 32 bits word in sysfile
 *
 *   6 +                                           // was 5 in < version 5.1
 *   MAX_NDB_NODES +                               // lastCompletedGCI
 *   NODE_ARRAY_SIZE(MAX_NDB_NODES, 4) +           // nodeStatus
 *   NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + // nodeGroups
 *   NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + // takeOver
 *   NodeBitmask::NDB_NODE_BITMASK_SIZE            // Lcp Active
 */
#define _SYSFILE_SIZE32 (6 + \
                         MAX_NDB_NODES + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, 4) + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + \
                         _NDB_NODE_BITMASK_SIZE)

/**
 * This struct defines the format of P<X>.sysfile
 */
struct Sysfile {
public:

  /**
   * No of 32 bits words in the sysfile
   */
  STATIC_CONST( SYSFILE_SIZE32 = _SYSFILE_SIZE32 );
  
  Uint32 systemRestartBits;

  /**
   * Restart seq for _this_ node...
   */
  Uint32 m_restart_seq;

  static bool getInitialStartOngoing(const Uint32 & systemRestartBits);
  static void setInitialStartOngoing(Uint32 & systemRestartBits);
  static void clearInitialStartOngoing(Uint32 & systemRestartBits);

  static bool getRestartOngoing(const Uint32 & systemRestartBits);
  static void setRestartOngoing(Uint32 & systemRestartBits);
  static void clearRestartOngoing(Uint32 & systemRestartBits);

  static bool getLCPOngoing(const Uint32 & systemRestartBits);
  static void setLCPOngoing(Uint32 & systemRestartBits);
  static void clearLCPOngoing(Uint32 & systemRestartBits);
  
  Uint32 keepGCI;
  Uint32 oldestRestorableGCI;
  Uint32 newestRestorableGCI;
  Uint32 latestLCP_ID;
  
  /**
   * Last completed GCI for each node
   */
  Uint32 lastCompletedGCI[MAX_NDB_NODES];
  
  /**
   * Active status bits
   *
   *  It takes 4 bits to represent it
   */
  enum ActiveStatus {
    NS_Active                  = 0
    ,NS_ActiveMissed_1         = 1
    ,NS_ActiveMissed_2         = 2
    ,NS_ActiveMissed_3         = 3
    ,NS_NotActive_NotTakenOver = 5
    ,NS_TakeOver               = 6
    ,NS_NotActive_TakenOver    = 7
    ,NS_NotDefined             = 8
    ,NS_Configured             = 9
  };
  STATIC_CONST( NODE_STATUS_SIZE = NODE_ARRAY_SIZE(MAX_NDB_NODES, 4) );
  Uint32 nodeStatus[NODE_STATUS_SIZE];

  static Uint32 getNodeStatus(NodeId, const Uint32 nodeStatus[]);
  static void   setNodeStatus(NodeId, Uint32 nodeStatus[], Uint32 status);
  
  /**
   * The node group of each node
   *   Sizeof(NodeGroup) = 8 Bit
   */
  STATIC_CONST( NODE_GROUPS_SIZE = NODE_ARRAY_SIZE(MAX_NDB_NODES, 
							 NODEID_BITS) );
  Uint32 nodeGroups[NODE_GROUPS_SIZE];
  
  static Uint16 getNodeGroup(NodeId, const Uint32 nodeGroups[]);
  static void   setNodeGroup(NodeId, Uint32 nodeGroups[], Uint16 group);

  /**
   * Any node can take over for any node
   */
  STATIC_CONST( TAKE_OVER_SIZE = NODE_ARRAY_SIZE(MAX_NDB_NODES, 
						 NODEID_BITS) );
  Uint32 takeOver[TAKE_OVER_SIZE];

  static NodeId getTakeOverNode(NodeId, const Uint32 takeOver[]);
  static void   setTakeOverNode(NodeId, Uint32 takeOver[], NodeId toNode);
  
  /**
   * Is a node running a LCP
   */
  Uint32 lcpActive[NdbNodeBitmask::Size];
};

#if (MAX_NDB_NODES > (1<<NODEID_BITS))
#error "Sysfile node id is too small"
#endif

/**
 * Restart Info
 *
 * i = Initial start completed
 * r = Crash during system restart
 * l = Crash during local checkpoint

 *           1111111111222222222233
 * 01234567890123456789012345678901
 * irl
 */
inline
bool 
Sysfile::getInitialStartOngoing(const Uint32 & systemRestartBits){
  return systemRestartBits & 1;
}

inline 
void 
Sysfile::setInitialStartOngoing(Uint32 & systemRestartBits){
  systemRestartBits |= 1;
}

inline
void
Sysfile::clearInitialStartOngoing(Uint32 & systemRestartBits){
  systemRestartBits &= ~1;
}

inline 
bool 
Sysfile::getRestartOngoing(const Uint32 & systemRestartBits){
  return (systemRestartBits & 2) != 0;
}

inline
void 
Sysfile::setRestartOngoing(Uint32 & systemRestartBits){
  systemRestartBits |= 2;
}

inline
void 
Sysfile::clearRestartOngoing(Uint32 & systemRestartBits){
  systemRestartBits &= ~2;
}

inline 
bool
Sysfile::getLCPOngoing(const Uint32 & systemRestartBits){
  return systemRestartBits & 4;
}

inline
void 
Sysfile::setLCPOngoing(Uint32 & systemRestartBits){
  systemRestartBits |= 4;
}

inline
void 
Sysfile::clearLCPOngoing(Uint32 & systemRestartBits){
  systemRestartBits &= ~4;
}

inline
Uint32 
Sysfile::getNodeStatus(NodeId nodeId, const Uint32 nodeStatus[]){
  const int word  = nodeId >> 3;
  const int shift = (nodeId & 7) << 2;
  
  return (nodeStatus[word] >> shift) & 15;
}

inline
void
Sysfile::setNodeStatus(NodeId nodeId, Uint32 nodeStatus[], Uint32 status){
  const int word  = nodeId >> 3;
  const int shift = (nodeId & 7) << 2;

  const Uint32 mask = ~(((Uint32)15) << shift);
  const Uint32 tmp = nodeStatus[word];
  
  nodeStatus[word] = (tmp & mask) | ((status & 15) << shift);
}

inline
Uint16
Sysfile::getNodeGroup(NodeId nodeId, const Uint32 nodeGroups[]){
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;
  
  return (nodeGroups[word] >> shift) & 255;
}

inline
void
Sysfile::setNodeGroup(NodeId nodeId, Uint32 nodeGroups[], Uint16 group){
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;
  
  const Uint32 mask = ~(((Uint32)255) << shift);
  const Uint32 tmp = nodeGroups[word];
  
  nodeGroups[word] = (tmp & mask) | ((group & 255) << shift);  
}

inline 
NodeId 
Sysfile::getTakeOverNode(NodeId nodeId, const Uint32 takeOver[]){
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;
  
  return (takeOver[word] >> shift) & 255;
}

inline
void
Sysfile::setTakeOverNode(NodeId nodeId, Uint32 takeOver[], NodeId toNode){
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;
  
  const Uint32 mask = ~(((Uint32)255) << shift);
  const Uint32 tmp = takeOver[word];
  
  takeOver[word] = (tmp & mask) | ((toNode & 255) << shift);  
}



#undef JAM_FILE_ID

#endif
