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

#ifndef FIRE_TRIG_ORD_HPP
#define FIRE_TRIG_ORD_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <trigger_definitions.h>
#include <string.h>

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
  friend bool printFIRE_TRIG_ORD(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 8 );
  STATIC_CONST( SignalWithGCILength = 9 );
  STATIC_CONST( SignalLengthSuma = 12 );

private:
  Uint32 m_connectionPtr;
  Uint32 m_userRef;
  Uint32 m_triggerId;
  TriggerEvent::Value m_triggerEvent;
  Uint32 m_noPrimKeyWords;
  Uint32 m_noBeforeValueWords;
  Uint32 m_noAfterValueWords;
  Uint32 fragId;
  Uint32 m_gci_hi;
  Uint32 m_hashValue;
  Uint32 m_any_value;
  Uint32 m_gci_lo;
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

inline
Uint32 FireTrigOrd::getConnectionPtr() const
{
  return m_connectionPtr;
}

inline 
void FireTrigOrd::setConnectionPtr(Uint32 aConnectionPtr)
{
  m_connectionPtr = aConnectionPtr;
}

inline
Uint32 FireTrigOrd::getUserRef() const
{
  return m_userRef;
}

inline 
void FireTrigOrd::setUserRef(Uint32 aUserRef)
{
  m_userRef = aUserRef;
}

inline
Uint32 FireTrigOrd::getTriggerId() const
{
  return m_triggerId;
}

inline
void FireTrigOrd::setTriggerId(Uint32 aTriggerId)
{
  m_triggerId = aTriggerId;
}

inline
TriggerEvent::Value FireTrigOrd::getTriggerEvent() const
{
  return m_triggerEvent;
}

inline
void FireTrigOrd::setTriggerEvent(TriggerEvent::Value aTriggerEvent)
{
  m_triggerEvent = aTriggerEvent;
}

inline
Uint32 FireTrigOrd::getNoOfPrimaryKeyWords() const
{
  return m_noPrimKeyWords;
}

inline
void FireTrigOrd::setNoOfPrimaryKeyWords(Uint32 noPrim)
{
  m_noPrimKeyWords = noPrim;
}

inline
Uint32 FireTrigOrd::getNoOfBeforeValueWords() const
{
  return m_noBeforeValueWords;
}

inline
void FireTrigOrd::setNoOfBeforeValueWords(Uint32 noBefore)
{
  m_noBeforeValueWords = noBefore;
}

inline
Uint32 FireTrigOrd::getNoOfAfterValueWords() const
{
  return m_noAfterValueWords;
}

inline
void FireTrigOrd::setNoOfAfterValueWords(Uint32 noAfter)
{
  m_noAfterValueWords = noAfter;
}

inline
Uint32 FireTrigOrd::getGCI() const
{
  return m_gci_hi;
}

inline
void FireTrigOrd::setGCI(Uint32 aGCI)
{
  m_gci_hi = aGCI;
}

inline
Uint32 FireTrigOrd::getHashValue() const
{
  return m_hashValue;
}

inline
void FireTrigOrd::setHashValue(Uint32 flag)
{
  m_hashValue = flag;
}

inline
Uint32 FireTrigOrd::getAnyValue() const
{
  return m_any_value;
}

inline
void FireTrigOrd::setAnyValue(Uint32 any_value)
{
  m_any_value = any_value;
}


#endif
