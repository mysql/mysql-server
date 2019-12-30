/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SYSFILE_HPP
#define SYSFILE_HPP

#include <ndb_types.h>
#include <ndb_limits.h>
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 357


/**
 * No bits in Sysfile to represent nodeid
 */
#define NODEID_BITS 16

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
#define _SYSFILE_SIZE32_v1 (6 + 49 + 7 + 13 + 13 + 2)

#define _SYSFILE_SIZE32_v2 (7 + \
                         MAX_NDB_NODES + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, 4) + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + \
                         NODE_ARRAY_SIZE(MAX_NDB_NODES, NODEID_BITS) + \
                         _NDB_NODE_BITMASK_SIZE)

#define _SYSFILE_FILE_SIZE 1536

#if (_SYSFILE_FILE_SIZE < _SYSFILE_SIZE32_v2)
#error "File size of sysfile is to small compared to Sysfile size"
#endif

/**
 * This struct defines the format of P<X>.sysfile
 */
struct Sysfile {
public:

  /**
   * No of 32 bits words in the sysfile
   */
  STATIC_CONST( SYSFILE_SIZE32_v1 = _SYSFILE_SIZE32_v1 );
  STATIC_CONST( SYSFILE_SIZE32_v2 = _SYSFILE_SIZE32_v2 );
  STATIC_CONST( SYSFILE_FILE_SIZE = _SYSFILE_FILE_SIZE);
  // MAGIC_v2 is set to {'N', 'D', 'B', 'S', 'Y', 'S', 'F', '2'} in Sysfile.cpp.
  static const char MAGIC_v2[8];
  static constexpr size_t MAGIC_SIZE_v2 = 8;
  static_assert(sizeof(MAGIC_v2) == MAGIC_SIZE_v2,
                "MAGIC_v2 and MAGIC_SIZE_v2 mismatch");
  

  Uint32 systemRestartBits;

  /**
   * Restart seq for _this_ node...
   */
  Uint32 m_restart_seq;

  static void initSysFile(Uint32 nodeStatus[], Uint16 nodeGroups[]);

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
  Uint16 nodeGroups[MAX_NDB_NODES];
  
  static NodeId getNodeGroup(NodeId, const Uint16 nodeGroups[]);
  static void   setNodeGroup(NodeId, Uint16 nodeGroups[], Uint16 group);

  static NodeId getNodeGroup_v1(NodeId nodeId, const Uint32 *nodeGroups);
  static void   setNodeGroup_v1(NodeId nodeId,
                                Uint32 *nodeGroups,
                                Uint8 group);

  /**
   * Any node can take over for any node
   */
  Uint16 takeOver[MAX_NDB_NODES];

  static NodeId getTakeOverNode(NodeId, const Uint16 takeOver[]);
  static void   setTakeOverNode(NodeId, Uint16 takeOver[], NodeId toNode);
  static void   setTakeOverNode_v1(NodeId nodeId,
                                   Uint32* takeOver,
                                   Uint8 toNode);
  static NodeId getTakeOverNode_v1(NodeId nodeId, const Uint32* takeOver);
  
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
void
Sysfile::initSysFile(Uint32 nodeStatus[], Uint16 nodeGroups[])
{
  for(Uint32 i = 0; i < MAX_NDB_NODES; i++)
  {
    setNodeGroup(i, nodeGroups, NO_NODE_GROUP_ID);
    setNodeStatus(i, nodeStatus,Sysfile::NS_NotDefined);
  }
}

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
NodeId
Sysfile::getNodeGroup(NodeId nodeId, const Uint16 nodeGroups[])
{
  return nodeGroups[nodeId];
}

inline
void
Sysfile::setNodeGroup(NodeId nodeId, Uint16 nodeGroups[], Uint16 group)
{
  nodeGroups[nodeId] = group;
}

inline
NodeId
Sysfile::getTakeOverNode(NodeId nodeId, const Uint16 takeOver[])
{
  return takeOver[nodeId];
}

inline
void
Sysfile::setTakeOverNode(NodeId nodeId, Uint16 takeOver[], NodeId toNode)
{
  takeOver[nodeId] = toNode;
}

inline
NodeId
Sysfile::getNodeGroup_v1(NodeId nodeId, const Uint32 *nodeGroups)
{
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;

  return (nodeGroups[word] >> shift) & 255;
}

inline
void
Sysfile::setNodeGroup_v1(NodeId nodeId, Uint32 *nodeGroups, Uint8 group)
{
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;

  const Uint32 mask = ~(((Uint32)255) << shift);
  const Uint32 tmp = nodeGroups[word];

  nodeGroups[word] = (tmp & mask) | ((group & 255) << shift);
}

inline
NodeId
Sysfile::getTakeOverNode_v1(NodeId nodeId, const Uint32* takeOver)
{
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;

  return (takeOver[word] >> shift) & 255;
}


inline
void
Sysfile::setTakeOverNode_v1(NodeId nodeId, Uint32* takeOver, Uint8 toNode)
{
  const int word = nodeId >> 2;
  const int shift = (nodeId & 3) << 3;

  const Uint32 mask = ~(((Uint32)255) << shift);
  const Uint32 tmp = takeOver[word];

  takeOver[word] = (tmp & mask) | ((toNode & 255) << shift);
}



#undef JAM_FILE_ID

#endif
