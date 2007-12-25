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

#ifndef SCHEMA_TRANS_IMPL_HPP
#define SCHEMA_TRANS_IMPL_HPP

#include <Bitmask.hpp>
#include "SignalData.hpp"
#include "GlobalSignalNumbers.h"

struct SchemaTransImplReq {
  STATIC_CONST( SignalLength = 9 );
  Uint32 senderRef;
  Uint32 transKey;
  Uint32 opKey;
  Uint32 phaseInfo;     // mode | phase subphase | piggy-backed gsn
  Uint32 requestInfo;   // 0 | op extra | global flags | local flags
  Uint32 operationInfo; // op index | op depth
  Uint32 iteratorInfo;  // list id | list index | repeat | 0
  Uint32 clientRef;
  Uint32 transId;

  // phaseInfo
  static Uint32 getMode(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 0, 8);
  }
  static void setMode(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 0, 8, val);
  }
  static Uint32 getPhase(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 8, 4);
  }
  static void setPhase(Uint32& info, Uint32 val) {
    assert(val < (1 << 4));
    BitmaskImpl::setField(1, &info, 8, 4, val);
  }
  static Uint32 getSubphase(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 12, 4);
  }
  static void setSubphase(Uint32& info, Uint32 val) {
    assert(val < (1 << 4));
    BitmaskImpl::setField(1, &info, 12, 4, val);
  }
  static Uint32 getGsn(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 16, 16);
  }
  static void setGsn(Uint32& info, Uint32 val) {
    assert(val < (1 << 16));
    BitmaskImpl::setField(1, &info, 16, 16, val);
  }

  // requestInfo is defined by DictSignal

  // operation info
  static Uint32 getOpIndex(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 0, 24);
  }
  static void setOpIndex(Uint32& info, Uint32 val) {
    BitmaskImpl::setField(1, &info, 0, 24, val);
  }
  static Uint32 getOpDepth(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 24, 8);
  }
  static void setOpDepth(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 24, 8, val);
  }

  // iteratorInfo
  static Uint32 getListId(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 0, 8);
  }
  static void setListId(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 0, 8, val);
  }
  static Uint32 getListIndex(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 8, 8);
  }
  static void setListIndex(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 8, 8, val);
  }
  static Uint32 getItRepeat(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 16, 8);
  }
  static void setItRepeat(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 16, 8, val);
  }
};

struct SchemaTransImplConf {
  enum {
    IT_REPEAT = (1 << 1)
  };

  STATIC_CONST( SignalLength = 3 );
  Uint32 senderRef;
  Uint32 transKey;
  Uint32 itFlags;
};

struct SchemaTransImplRef {
  STATIC_CONST( SignalLength = 6 );
  STATIC_CONST( GSN = GSN_SCHEMA_TRANS_IMPL_REF );
  enum ErrorCode {
    NoError = 0,
    NotMaster = 702,
    TooManySchemaTrans = 780,
    InvalidTransKey = 781,
    InvalidTransId = 782,
    TooManySchemaOps = 783,
    InvalidTransState = 784,
    NF_FakeErrorREF = 1
  };
  Uint32 senderRef;
  union { Uint32 transKey, senderData; };
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif
