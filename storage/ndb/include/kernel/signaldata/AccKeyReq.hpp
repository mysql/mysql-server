/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ACCKEYREQ_HPP
#define ACCKEYREQ_HPP

#include <assert.h>

#include "ndb_limits.h"

struct AccKeyReq
{
  STATIC_CONST( SignalLength_localKey = 10 );
  STATIC_CONST( SignalLength_keyInfo  =  8 /* + keyLen */ );

  Uint32 connectPtr;
  Uint32 fragmentPtr;
  Uint32 requestInfo;
  Uint32 hashValue;
  Uint32 keyLen;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 lockConnectPtr; /* For lock take over operation */
  union
  {
    /* if keyLen == 0 use localKey */
    Uint32 localKey[2];
    Uint32 keyInfo[MAX_KEY_SIZE_IN_WORDS /*keyLen*/];
  };

  static Uint32 getOperation(Uint32 requestInfo);
  static Uint32 getLockType(Uint32 requestInfo);
  static bool getDirtyOp(Uint32 requestInfo);
  static Uint32 getReplicaType(Uint32 requestInfo);
  static bool getTakeOver(Uint32 requestInfo);
  static bool getLockReq(Uint32 requestInfo);

  static Uint32 setOperation(Uint32 requestInfo, Uint32 op);
  static Uint32 setLockType(Uint32 requestInfo, Uint32 locktype);
  static Uint32 setDirtyOp(Uint32 requestInfo, bool dirtyop);
  static Uint32 setReplicaType(Uint32 requestInfo, Uint32 replicatype);
  static Uint32 setTakeOver(Uint32 requestInfo, bool takeover);
  static Uint32 setLockReq(Uint32 requestInfo, bool lockreq);

private:
  enum RequestInfo {
    RI_OPERATION_SHIFT    =  0, RI_OPERATION_MASK    = 15,
    RI_LOCK_TYPE_SHIFT    =  4, RI_LOCK_TYPE_MASK    =  3,
    RI_DIRTY_OP_SHIFT     =  6, RI_DIRTY_OP_MASK     =  1,
    RI_REPLICA_TYPE_SHIFT =  7, RI_REPLICA_TYPE_MASK =  3,
    RI_TAKE_OVER_SHIFT    =  9, RI_TAKE_OVER_MASK    =  1,
    RI_LOCK_REQ_SHIFT     = 31, RI_LOCK_REQ_MASK     =  1,
  };
};

inline
Uint32
AccKeyReq::getOperation(Uint32 requestInfo)
{
  return (requestInfo >> RI_OPERATION_SHIFT) & RI_OPERATION_MASK;
}

inline
Uint32
AccKeyReq::getLockType(Uint32 requestInfo)
{
  return (requestInfo >> RI_LOCK_TYPE_SHIFT) & RI_LOCK_TYPE_MASK;
}

inline
bool
AccKeyReq::getDirtyOp(Uint32 requestInfo)
{
  return (requestInfo >> RI_DIRTY_OP_SHIFT) & RI_DIRTY_OP_MASK;
}

inline
Uint32
AccKeyReq::getReplicaType(Uint32 requestInfo)
{
  return (requestInfo >> RI_REPLICA_TYPE_SHIFT) & RI_REPLICA_TYPE_MASK;
}

inline
bool
AccKeyReq::getTakeOver(Uint32 requestInfo)
{
  return (requestInfo >> RI_TAKE_OVER_SHIFT) & RI_TAKE_OVER_MASK;
}

inline
bool
AccKeyReq::getLockReq(Uint32 requestInfo)
{
  return (requestInfo >> RI_LOCK_REQ_SHIFT) & RI_LOCK_REQ_MASK;
}

inline
Uint32
AccKeyReq::setOperation(Uint32 requestInfo, Uint32 op)
{
  assert(op <= RI_OPERATION_MASK);
  return (requestInfo & ~(RI_OPERATION_MASK << RI_OPERATION_SHIFT))
    | (op << RI_OPERATION_SHIFT);
}

inline
Uint32
AccKeyReq::setLockType(Uint32 requestInfo, Uint32 locktype)
{
  assert(locktype <= RI_LOCK_TYPE_MASK);
  return (requestInfo & ~(RI_LOCK_TYPE_MASK << RI_LOCK_TYPE_SHIFT))
    | (locktype << RI_LOCK_TYPE_SHIFT);
}

inline
Uint32
AccKeyReq::setDirtyOp(Uint32 requestInfo, bool dirtyop)
{
  return (requestInfo & ~(RI_DIRTY_OP_MASK << RI_DIRTY_OP_SHIFT))
    | (dirtyop ? 1U << RI_DIRTY_OP_SHIFT : 0);
}

inline
Uint32
AccKeyReq::setReplicaType(Uint32 requestInfo, Uint32 replicatype)
{
  assert(replicatype <= RI_REPLICA_TYPE_MASK);
  return (requestInfo & ~(RI_REPLICA_TYPE_MASK << RI_REPLICA_TYPE_SHIFT))
    | (replicatype << RI_REPLICA_TYPE_SHIFT);
}

inline
Uint32
AccKeyReq::setTakeOver(Uint32 requestInfo, bool takeover)
{
  return (requestInfo & ~(RI_TAKE_OVER_MASK << RI_TAKE_OVER_SHIFT))
    | (takeover ? 1U << RI_TAKE_OVER_SHIFT : 0);
}

inline
Uint32
AccKeyReq::setLockReq(Uint32 requestInfo, bool lockreq)
{
  return (requestInfo & ~(RI_LOCK_REQ_MASK << RI_LOCK_REQ_SHIFT))
    | (lockreq ? 1U << RI_LOCK_REQ_SHIFT : 0);
}

#endif
