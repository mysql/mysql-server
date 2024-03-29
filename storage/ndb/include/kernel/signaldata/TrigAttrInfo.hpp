/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef TRIG_ATTRINFO_HPP
#define TRIG_ATTRINFO_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <trigger_definitions.h>
#include <string.h>

#define JAM_FILE_ID 189


/**
 * TrigAttrInfo
 *
 * This signal is sent by TUP to signal
 * that a trigger has fired
 */
class TrigAttrInfo {
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

  /**
   * For printing
   */
  friend bool printTRIG_ATTRINFO(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
enum AttrInfoType { 
  PRIMARY_KEY = 0, 
  BEFORE_VALUES = 1, 
  AFTER_VALUES = 2 
};

  static constexpr Uint32 DataLength = 22;
  static constexpr Uint32 StaticLength = 3;

private:
  Uint32 m_connectionPtr; 
  Uint32 m_trigId;
  Uint32 m_type;
  Uint32 m_data[DataLength];

  // Public methods
public:
  Uint32 getConnectionPtr() const;
  void setConnectionPtr(Uint32);  
  AttrInfoType getAttrInfoType() const;
  void setAttrInfoType(AttrInfoType anAttrType);
  Uint32 getTriggerId() const;
  void setTriggerId(Uint32 aTriggerId);
  Uint32 getTransactionId1() const;
  void setTransactionId1(Uint32 aTransId);
  Uint32 getTransactionId2() const;
  void setTransactionId2(Uint32 aTransId);
  Uint32* getData();
  const Uint32* getData() const;
  int setData(Uint32* aDataBuf, Uint32 aDataLen);
};

inline
Uint32 TrigAttrInfo::getConnectionPtr() const
{
  return m_connectionPtr;
}

inline 
void TrigAttrInfo::setConnectionPtr(Uint32 aConnectionPtr)
{
  m_connectionPtr = aConnectionPtr;
}

inline
TrigAttrInfo::AttrInfoType TrigAttrInfo::getAttrInfoType() const
{
  return  (TrigAttrInfo::AttrInfoType) m_type;
}

inline
void TrigAttrInfo::setAttrInfoType(TrigAttrInfo::AttrInfoType anAttrType)
{
  m_type = (Uint32) anAttrType;
}

inline
Uint32 TrigAttrInfo::getTriggerId() const
{
  return m_trigId;
}

inline
void TrigAttrInfo::setTriggerId(Uint32 aTriggerId)
{
  m_trigId = aTriggerId;
}

inline Uint32* TrigAttrInfo::getData() { return m_data; }

inline const Uint32* TrigAttrInfo::getData() const { return m_data; }

inline
int TrigAttrInfo::setData(Uint32* aDataBuf, Uint32 aDataLen)
{
  if (aDataLen > DataLength)
    return -1;
  memcpy(m_data, aDataBuf, aDataLen*sizeof(Uint32));

  return 0;
}


#undef JAM_FILE_ID

#endif
