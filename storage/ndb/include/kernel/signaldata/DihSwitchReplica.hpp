/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef DIH_SWITCH_REPLICA_HPP
#define DIH_SWITCH_REPLICA_HPP

/**
 * This signal is sent from master DIH to all DIH's 
 *   switches primary / backup nodes for replica(s)
 *
 */
class DihSwitchReplicaReq {
  /**
   * Sender/Reciver
   */
  friend class Dbdih;

public:
  STATIC_CONST( SignalLength = 4 + MAX_REPLICAS );
  
private:
  /**
   * Request Info
   *
   */
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragNo; 
  Uint32 noOfReplicas;
  Uint32 newNodeOrder[MAX_REPLICAS];
};

class DihSwitchReplicaRef {
  /**
   * Sender/Reciver
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 2 );
  
private:
  Uint32 senderNode;
  Uint32 errorCode; // See StopPermRef::ErrorCode
};

class DihSwitchReplicaConf {
  /**
   * Sender/Reciver
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 1 );
  
private:
  Uint32 senderNode;
};
#endif
