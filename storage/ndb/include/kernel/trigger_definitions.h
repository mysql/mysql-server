/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_TRIGGER_DEFINITIONS_H
#define NDB_TRIGGER_DEFINITIONS_H

#include <ndb_global.h>
#include "ndb_limits.h"
#include <Bitmask.hpp>
#include <signaldata/DictTabInfo.hpp>

#define ILLEGAL_TRIGGER_ID ((Uint32)(~0))

struct TriggerType {
  enum Value {
    //CONSTRAINT            = 0,
    SECONDARY_INDEX         = DictTabInfo::HashIndexTrigger,
    FK_PARENT               = DictTabInfo::FKParentTrigger,
    FK_CHILD                = DictTabInfo::FKChildTrigger,
    //SCHEMA_UPGRADE        = 3,
    //API_TRIGGER           = 4,
    //SQL_TRIGGER           = 5,
    SUBSCRIPTION          = DictTabInfo::SubscriptionTrigger,
    READ_ONLY_CONSTRAINT  = DictTabInfo::ReadOnlyConstraint,
    ORDERED_INDEX         = DictTabInfo::IndexTrigger,
    
    SUBSCRIPTION_BEFORE   = 9, // Only used by TUP/SUMA, should be REMOVED!!
    REORG_TRIGGER         = DictTabInfo::ReorgTrigger
    ,FULLY_REPLICATED_TRIGGER = DictTabInfo::FullyReplicatedTrigger
  };
};

struct TriggerActionTime {
  enum Value {
    TA_BEFORE   = 0, /* Immediate, before operation */
    TA_AFTER    = 1, /* Immediate, after operation */
    TA_DEFERRED = 2, /* Before commit */
    TA_DETACHED = 3, /* After commit in a separate transaction, NYI */
    TA_CUSTOM   = 4  /* Hardcoded per TriggerType */
  };
};

struct TriggerEvent {
  /** TableEvent must match 1 << TriggerEvent */
  enum Value {
    TE_INSERT = 0,
    TE_DELETE = 1,
    TE_UPDATE = 2,
    TE_CUSTOM = 3    /* Hardcoded per TriggerType */
  };
};

struct TriggerInfo {
  TriggerType::Value triggerType;
  TriggerActionTime::Value triggerActionTime;
  TriggerEvent::Value triggerEvent;
  bool monitorReplicas;
  bool monitorAllAttributes;
  bool reportAllMonitoredAttributes;

  // static methods

  // get/set bits in Uint32
  static TriggerType::Value
  getTriggerType(const Uint32& info) {
    const Uint32 val = BitmaskImpl::getField(1, &info, 0, 8);
    return (TriggerType::Value)val;
  }
  static void
  setTriggerType(Uint32& info, TriggerType::Value val) {
    BitmaskImpl::setField(1, &info, 0, 8, (Uint32)val);
  }
  static TriggerActionTime::Value
  getTriggerActionTime(const Uint32& info) {
    const Uint32 val = BitmaskImpl::getField(1, &info, 8, 8);
    return (TriggerActionTime::Value)val;
  }
  static void
  setTriggerActionTime(Uint32& info, TriggerActionTime::Value val) {
    BitmaskImpl::setField(1, &info, 8, 8, (Uint32)val);
  }
  static TriggerEvent::Value
  getTriggerEvent(const Uint32& info) {
    const Uint32 val = BitmaskImpl::getField(1, &info, 16, 8);
    return (TriggerEvent::Value)val;
  }
  static void
  setTriggerEvent(Uint32& info, TriggerEvent::Value val) {
    BitmaskImpl::setField(1, &info, 16, 8, (Uint32)val);
  }
  static bool
  getMonitorReplicas(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 24, 1);
  }
  static void
  setMonitorReplicas(Uint32& info, bool val) {
    BitmaskImpl::setField(1, &info, 24, 1, val);
  }
  static bool
  getMonitorAllAttributes(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 25, 1);
  }
  static void
  setMonitorAllAttributes(Uint32& info, bool val) {
    BitmaskImpl::setField(1, &info, 25, 1, val);
  }
  static bool
  getReportAllMonitoredAttributes(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 26, 1);
  }
  static void
  setReportAllMonitoredAttributes(Uint32& info, bool val) {
    BitmaskImpl::setField(1, &info, 26, 1, val);
  }

  // convert between Uint32 and struct
  static void
  packTriggerInfo(Uint32& val, const TriggerInfo& str) {
    val = 0;
    setTriggerType(val, str.triggerType);
    setTriggerActionTime(val, str.triggerActionTime);
    setTriggerEvent(val, str.triggerEvent);
    setMonitorReplicas(val, str.monitorReplicas);
    setMonitorAllAttributes(val, str.monitorAllAttributes);
    setReportAllMonitoredAttributes(val, str.reportAllMonitoredAttributes);
  }
  static void
  unpackTriggerInfo(const Uint32& val, TriggerInfo& str) {
    str.triggerType = getTriggerType(val);
    str.triggerActionTime = getTriggerActionTime(val);
    str.triggerEvent = getTriggerEvent(val);
    str.monitorReplicas = getMonitorReplicas(val);
    str.monitorAllAttributes = getMonitorAllAttributes(val);
    str.reportAllMonitoredAttributes = getReportAllMonitoredAttributes(val);
  }

  // for debug print
  static const char*
  triggerTypeName(Uint32 val) {
    switch (val) {
    case TriggerType::SECONDARY_INDEX:
      return "SECONDARY_INDEX";
    case TriggerType::FK_PARENT:
      return "FK_PARENT";
    case TriggerType::FK_CHILD:
      return "FK_CHILD";
    case TriggerType::SUBSCRIPTION:
      return "SUBSCRIPTION";
    case TriggerType::READ_ONLY_CONSTRAINT:
      return "READ_ONLY_CONSTRAINT";
    case TriggerType::ORDERED_INDEX:
      return "ORDERED_INDEX";
    case TriggerType::SUBSCRIPTION_BEFORE:
      return "SUBSCRIPTION_BEFORE";
    case TriggerType::REORG_TRIGGER:
      return "REORG_TRIGGER";
    case TriggerType::FULLY_REPLICATED_TRIGGER:
      return "FULLY_REPLICATED";
    }
    return "UNKNOWN";
  }
  static const char*
  triggerActionTimeName(Uint32 val) {
    switch (val) {
    case TriggerActionTime::TA_BEFORE:
      return "TA_BEFORE";
    case TriggerActionTime::TA_AFTER:
      return "TA_AFTER";
    case TriggerActionTime::TA_DEFERRED:
      return "TA_DEFERRED";
    case TriggerActionTime::TA_DETACHED:
      return "TA_DETACHED";
    case TriggerActionTime::TA_CUSTOM:
      return "TA_CUSTOM";
    }
    return "UNKNOWN";
  }
  static const char*
  triggerEventName(Uint32 val) {
    switch (val) {
    case TriggerEvent::TE_INSERT:
      return "TE_INSERT";
    case TriggerEvent::TE_DELETE:
      return "TE_DELETE";
    case TriggerEvent::TE_UPDATE:
      return "TE_UPDATE";
    case TriggerEvent::TE_CUSTOM:
      return "TE_CUSTOM";
    }
    return "UNKNOWN";
  }
};

struct NoOfFiredTriggers
{
  static constexpr Uint32 DeferredUKBit = (Uint32(1) << 31);
  static constexpr Uint32 DeferredFKBit = (Uint32(1) << 30);
  static constexpr Uint32 DeferredBits = (DeferredUKBit | DeferredFKBit);

  static Uint32 getFiredCount(Uint32 v) {
    return v & ~(Uint32(DeferredBits));
  }
  static Uint32 getDeferredUKBit(Uint32 v) {
    return (v & Uint32(DeferredUKBit)) != 0;
  }
  static void setDeferredUKBit(Uint32 & v) {
    v |= Uint32(DeferredUKBit);
  }
  static Uint32 getDeferredFKBit(Uint32 v) {
    return (v & Uint32(DeferredFKBit)) != 0;
  }
  static void setDeferredFKBit(Uint32 & v) {
    v |= Uint32(DeferredFKBit);
  }

  static bool getDeferredAllSet(Uint32 v) {
    return (v & Uint32(DeferredBits)) == DeferredBits;
  }
};

struct TriggerPreCommitPass
{
  /**
   * When using deferred triggers...
   * - UK are split into 2 passes...
   * - FK needs to be evaluated *after* UK has been processed
   *   as it (can) use UK
   *
   * When having cascadeing FK's they can provoke UK updates
   *   in such cases...the passes are
   *   N * (PASS_MAX + 1) + PASS
   */
  enum
  {
    UK_PASS_0 = 0,
    UK_PASS_1 = 1,
    FK_PASS_0 = 7, // leave some room...(unsure if it's needed)
    TPCP_PASS_MAX = 15
  };
};

#endif
