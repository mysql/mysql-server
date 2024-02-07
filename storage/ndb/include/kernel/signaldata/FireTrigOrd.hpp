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

#ifndef FIRE_TRIG_ORD_HPP
#define FIRE_TRIG_ORD_HPP

#include <string.h>
#include <trigger_definitions.h>
#include <NodeBitmask.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 69

/**
 * FireTrigOrd
 *
 * This signal is sent by TUP to signal
 * that a trigger has fired
 */
class FireTrigOrd {
  /**
   * Sender(s)
   */
  // API

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbtup;

  /**
   * Reciver(s)
   */
  friend class Dbtc;
  friend class Backup;
  friend class SumaParticipant;
  friend class Suma;

  /**
   * For printing
   */
  friend bool printFIRE_TRIG_ORD(FILE *output, const Uint32 *theData,
                                 Uint32 len, Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 SignalLength = 11;
  static constexpr Uint32 SignalWithGCILength = 9;
  static constexpr Uint32 SignalLengthSuma = 14;

 private:
  Uint32 m_connectionPtr;
  Uint32 m_userRef;
  Uint32 m_triggerId;
  Uint32 m_triggerEvent;
  Uint32 m_noPrimKeyWords;
  Uint32 m_noBeforeValueWords;
  Uint32 m_noAfterValueWords;
  Uint32 fragId;
  union {
    Uint32 m_gci_hi;
    Uint32 m_triggerType;
  };
  Uint32 m_transId1;
  Uint32 m_transId2;
  Uint32 m_gci_lo;
  Uint32 m_hashValue;
  Uint32 m_any_value;
  // Public methods
 public:
  Uint32 getConnectionPtr() const;
  void setConnectionPtr(Uint32);
  Uint32 getUserRef() const;
  void setUserRef(Uint32);
  Uint32 getTriggerId() const;
  void setTriggerId(Uint32 anIndxId);
  TriggerEvent::Value getTriggerEvent() const;
  void setTriggerEvent(TriggerEvent::Value);
  Uint32 getNoOfPrimaryKeyWords() const;
  void setNoOfPrimaryKeyWords(Uint32);
  Uint32 getNoOfBeforeValueWords() const;
  void setNoOfBeforeValueWords(Uint32);
  Uint32 getNoOfAfterValueWords() const;
  void setNoOfAfterValueWords(Uint32);
  Uint32 getGCI() const;
  void setGCI(Uint32);
  Uint32 getHashValue() const;
  void setHashValue(Uint32);
  Uint32 getAnyValue() const;
  void setAnyValue(Uint32);
};

inline Uint32 FireTrigOrd::getConnectionPtr() const { return m_connectionPtr; }

inline void FireTrigOrd::setConnectionPtr(Uint32 aConnectionPtr) {
  m_connectionPtr = aConnectionPtr;
}

inline Uint32 FireTrigOrd::getUserRef() const { return m_userRef; }

inline void FireTrigOrd::setUserRef(Uint32 aUserRef) { m_userRef = aUserRef; }

inline Uint32 FireTrigOrd::getTriggerId() const { return m_triggerId; }

inline void FireTrigOrd::setTriggerId(Uint32 aTriggerId) {
  m_triggerId = aTriggerId;
}

inline TriggerEvent::Value FireTrigOrd::getTriggerEvent() const {
  return (TriggerEvent::Value)m_triggerEvent;
}

inline void FireTrigOrd::setTriggerEvent(TriggerEvent::Value aTriggerEvent) {
  m_triggerEvent = aTriggerEvent;
}

inline Uint32 FireTrigOrd::getNoOfPrimaryKeyWords() const {
  return m_noPrimKeyWords;
}

inline void FireTrigOrd::setNoOfPrimaryKeyWords(Uint32 noPrim) {
  m_noPrimKeyWords = noPrim;
}

inline Uint32 FireTrigOrd::getNoOfBeforeValueWords() const {
  return m_noBeforeValueWords;
}

inline void FireTrigOrd::setNoOfBeforeValueWords(Uint32 noBefore) {
  m_noBeforeValueWords = noBefore;
}

inline Uint32 FireTrigOrd::getNoOfAfterValueWords() const {
  return m_noAfterValueWords;
}

inline void FireTrigOrd::setNoOfAfterValueWords(Uint32 noAfter) {
  m_noAfterValueWords = noAfter;
}

inline Uint32 FireTrigOrd::getGCI() const { return m_gci_hi; }

inline void FireTrigOrd::setGCI(Uint32 aGCI) { m_gci_hi = aGCI; }

inline Uint32 FireTrigOrd::getHashValue() const { return m_hashValue; }

inline void FireTrigOrd::setHashValue(Uint32 flag) { m_hashValue = flag; }

inline Uint32 FireTrigOrd::getAnyValue() const { return m_any_value; }

inline void FireTrigOrd::setAnyValue(Uint32 any_value) {
  m_any_value = any_value;
}

struct FireTrigReq {
  static constexpr Uint32 SignalLength = 4;

  Uint32 tcOpRec;
  Uint32 transId[2];
  Uint32 pass;
};

struct FireTrigRef {
  static constexpr Uint32 SignalLength = 4;

  Uint32 tcOpRec;
  Uint32 transId[2];
  Uint32 errCode;

  enum ErrorCode { FTR_UnknownOperation = 1235, FTR_IncorrectState = 1236 };
};

struct FireTrigConf {
  static constexpr Uint32 SignalLength = 4;

  Uint32 tcOpRec;
  Uint32 transId[2];
  Uint32 numFiredTriggers;  // bit 31 deferred trigger

  static Uint32 getFiredCount(Uint32 v) {
    return NoOfFiredTriggers::getFiredCount(v);
  }
  static Uint32 getDeferredUKBit(Uint32 v) {
    return NoOfFiredTriggers::getDeferredUKBit(v);
  }
  static void setDeferredUKBit(Uint32 &v) {
    NoOfFiredTriggers::setDeferredUKBit(v);
  }
  static Uint32 getDeferredFKBit(Uint32 v) {
    return NoOfFiredTriggers::getDeferredFKBit(v);
  }
  static void setDeferredFKBit(Uint32 &v) {
    NoOfFiredTriggers::setDeferredFKBit(v);
  }
};

#undef JAM_FILE_ID

#endif
