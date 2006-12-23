/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LQH_TRANS_CONF_H
#define LQH_TRANS_CONF_H

#include "SignalData.hpp"

/**
 * This signal is sent as response to a LQH_TRANSREQ
 * which is sent as by a take-over TC
 */
class LqhTransConf {
  /**
   * Reciver(s)
   */
  friend class Dbtc;

  /**
   * Sender(s)
   */
  friend class Dblqh;      

  friend bool printLQH_TRANSCONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 15 );
private:

  /**
   * This type describes the state of the operation returned in this signal
   */
  enum OperationStatus {
    InvalidStatus = 0, /**< This status should never be sent in a signal
			  it is only used for initializing variables so that
			  you can easily later check if they have changed */
    LastTransConf = 4, /**< This status indicates that LQH has finished the scan
			  of operations belonging to the died TC.
			  Data 0 - 2 is valid */
    
    Prepared  = 2,
    Committed = 3,
    Aborted   = 1,
    Marker    = 5 /**< This means that the only thing left is a marker,
		     Data 0 - 6 is valid */
  };
  
  /**
   * DATA VARIABLES
   */
  Uint32 tcRef;           // 0
  Uint32 lqhNodeId;       // 1   
  Uint32 operationStatus; // 2 See enum OperationStatus
  Uint32 transId1;        // 3
  Uint32 transId2;        // 4
  Uint32 apiRef;          // 5
  Uint32 apiOpRec;        // 6
  Uint32 lqhConnectPtr;
  Uint32 oldTcOpRec;
  Uint32 requestInfo;
  Uint32 gci;
  Uint32 nextNodeId1;
  Uint32 nextNodeId2;
  Uint32 nextNodeId3;
  Uint32 tableId;

  /**
   * Getters
   */
  static Uint32 getReplicaNo(Uint32 & requestInfo);
  static Uint32 getReplicaType(Uint32 & requestInfo);
  static Uint32 getLastReplicaNo(Uint32 & requestInfo);
  static Uint32 getSimpleFlag(Uint32 & requestInfo);
  static Uint32 getDirtyFlag(Uint32 & requestInfo);
  static Uint32 getOperation(Uint32 & requestInfo);
  static Uint32 getMarkerFlag(Uint32 & requestInfo);

  static void setReplicaNo(UintR & requestInfo, UintR val);
  static void setReplicaType(UintR & requestInfo, UintR val);
  static void setLastReplicaNo(UintR & requestInfo, UintR val);
  static void setSimpleFlag(UintR & requestInfo, UintR val);
  static void setDirtyFlag(UintR & requestInfo, UintR val);
  static void setOperation(UintR & requestInfo, UintR val);
  static void setMarkerFlag(Uint32 & requestInfo, Uint32 val);
};

/**
 * Request Info
 *
 * t = replica type           - 2  Bits (0-1)
 * r = Replica No             - 2  Bits (2-3)
 * l = Last Replica No        - 2  Bits (4-5)
 * s = Simple                 - 1  Bits (6)
 * d = Dirty                  - 1  Bit  (7)
 * o = Operation              - 3  Bit  (8-9)
 * m = Marker present         - 1  Bit  (10)
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * ttrrllsdooom
 */
#define LTC_REPLICA_TYPE_SHIFT    (0)
#define LTC_REPLICA_TYPE_MASK     (3)
#define LTC_REPLICA_NO_SHIFT      (2)
#define LTC_REPLICA_NO_MASK       (3)
#define LTC_LAST_REPLICA_SHIFT    (4)
#define LTC_LAST_REPLICA_MASK     (3)
#define LTC_SIMPLE_SHIFT          (6)
#define LTC_DIRTY_SHIFT           (7)
#define LTC_OPERATION_SHIFT       (8)
#define LTC_OPERATION_MASK        (7)
#define LTC_MARKER_SHIFT          (10)

inline
Uint32
LqhTransConf::getReplicaType(Uint32 & requestInfo){
  return (requestInfo >> LTC_REPLICA_TYPE_SHIFT) & LTC_REPLICA_TYPE_MASK;
}

inline
Uint32
LqhTransConf::getReplicaNo(Uint32 & requestInfo){
  return (requestInfo >> LTC_REPLICA_NO_SHIFT) & LTC_REPLICA_NO_MASK;
}

inline
Uint32
LqhTransConf::getLastReplicaNo(Uint32 & requestInfo){
  return (requestInfo >> LTC_LAST_REPLICA_SHIFT) & LTC_LAST_REPLICA_MASK;
}

inline
Uint32
LqhTransConf::getSimpleFlag(Uint32 & requestInfo){
  return (requestInfo >> LTC_SIMPLE_SHIFT) & 1;
}

inline
Uint32
LqhTransConf::getDirtyFlag(Uint32 & requestInfo){
  return (requestInfo >> LTC_DIRTY_SHIFT) & 1;
}

inline
Uint32
LqhTransConf::getOperation(Uint32 & requestInfo){
  return (requestInfo >> LTC_OPERATION_SHIFT) & LTC_OPERATION_MASK;
}

inline
Uint32
LqhTransConf::getMarkerFlag(Uint32 & requestInfo){
  return (requestInfo >> LTC_MARKER_SHIFT) & 1;
}


inline
void
LqhTransConf::setReplicaNo(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, LTC_REPLICA_NO_MASK, "LqhTransConf::setReplicaNo");
  requestInfo |= (val << LTC_REPLICA_NO_SHIFT);
}

inline
void
LqhTransConf::setReplicaType(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, LTC_REPLICA_TYPE_MASK, "LqhTransConf::setReplicaType");
  requestInfo |= (val << LTC_REPLICA_TYPE_SHIFT);
}

inline
void
LqhTransConf::setLastReplicaNo(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, LTC_LAST_REPLICA_MASK, "LqhTransConf::setLastReplicaNo");
  requestInfo |= (val << LTC_LAST_REPLICA_SHIFT);
}

inline
void
LqhTransConf::setSimpleFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "LqhTransConf::setSimpleFlag");
  requestInfo |= (val << LTC_SIMPLE_SHIFT);
}

inline
void
LqhTransConf::setDirtyFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "LqhTransConf::setDirtyFlag");
  requestInfo |= (val << LTC_DIRTY_SHIFT);
}

inline
void
LqhTransConf::setOperation(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, LTC_OPERATION_MASK, "LqhTransConf::setOperation");
  requestInfo |= (val << LTC_OPERATION_SHIFT);
}

inline
void
LqhTransConf::setMarkerFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "LqhTransConf::setMarkerFlag");
  requestInfo |= (val << LTC_MARKER_SHIFT);
}

#endif
