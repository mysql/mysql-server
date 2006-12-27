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

#ifndef DROP_TRIG_HPP
#define DROP_TRIG_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <trigger_definitions.h>

/**
 * DropTrigReq.
 */
class DropTrigReq {
  friend bool printDROP_TRIG_REQ(FILE*, const Uint32*, Uint32, Uint16);

public:
  enum RequestType {
    RT_UNDEFINED = 0,
    RT_USER = 1,
    RT_ALTER_INDEX = 2,
    RT_BUILD_INDEX = 3,
    RT_DICT_PREPARE = 1 << 4,
    RT_DICT_COMMIT = 0xC << 4,
    RT_DICT_ABORT = 0xF << 4,
    RT_TC = 5 << 8,
    RT_LQH = 6 << 8
  };
  STATIC_CONST( SignalLength = 7 );
  SECTION( TRIGGER_NAME_SECTION = 0 ); // optional
  enum KeyValues {
    TriggerNameKey = 0xa1
  };

private:
  Uint32 m_userRef;
  Uint32 m_connectionPtr;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexId;             // set by DICT if index trigger
  Uint32 m_triggerId;           // set by DICT based on name
  Uint32 m_triggerInfo;         // only for TUP
  // extra
  Uint32 m_opKey;

public:
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  DropTrigReq::RequestType getRequestType() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_requestInfo, 0, 16);
    return (DropTrigReq::RequestType)val;
  }
  void setRequestType(DropTrigReq::RequestType val) {
    m_requestInfo = (Uint32)val;
  }
  Uint32 getRequestFlag() const {
    return BitmaskImpl::getField(1, &m_requestInfo, 16, 16);
  };
  void addRequestFlag(Uint32 val) {
    val |= BitmaskImpl::getField(1, &m_requestInfo, 16, 16);
    BitmaskImpl::setField(1, &m_requestInfo, 16, 16, val);
  };
  Uint32 getTableId() const {
    return m_tableId;
  }
  void setTableId(Uint32 val) {
    m_tableId = val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
  Uint32 getTriggerId() const {
    return m_triggerId;
  }
  void setTriggerId(Uint32 val) {
    m_triggerId = val;
  }
  Uint32 getTriggerInfo() const {
    return m_triggerInfo;
  }
  void setTriggerInfo(Uint32 val) {
    m_triggerInfo = val;
  }
  TriggerType::Value getTriggerType() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_triggerInfo, 0, 8);
    return (TriggerType::Value)val;
  }
  void setTriggerType(TriggerType::Value val) {
    BitmaskImpl::setField(1, &m_triggerInfo, 0, 8, (Uint32)val);
  }
  TriggerActionTime::Value getTriggerActionTime() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_triggerInfo, 8, 8);
    return (TriggerActionTime::Value)val;
  }
  void setTriggerActionTime(TriggerActionTime::Value val) {
    BitmaskImpl::setField(1, &m_triggerInfo, 8, 8, (Uint32)val);
  }
  TriggerEvent::Value getTriggerEvent() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_triggerInfo, 16, 8);
    return (TriggerEvent::Value)val;
  }
  void setTriggerEvent(TriggerEvent::Value val) {
    BitmaskImpl::setField(1, &m_triggerInfo, 16, 8, (Uint32)val);
  }
  bool getMonitorReplicas() const {
    return BitmaskImpl::getField(1, &m_triggerInfo, 24, 1);
  }
  void setMonitorReplicas(bool val) {
    BitmaskImpl::setField(1, &m_triggerInfo, 24, 1, val);
  }
  bool getMonitorAllAttributes() const {
    return BitmaskImpl::getField(1, &m_triggerInfo, 25, 1);
  }
  void setMonitorAllAttributes(bool val) {
    BitmaskImpl::setField(1, &m_triggerInfo, 25, 1, val);
  }
  Uint32 getOpKey() const {
    return m_opKey;
  }
  void setOpKey(Uint32 val) {
    m_opKey = val;
  }
};

/**
 * DropTrigConf.
 */
class DropTrigConf {
  friend bool printDROP_TRIG_CONF(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( InternalLength = 3 );
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 m_userRef;
  Uint32 m_connectionPtr;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexId;
  Uint32 m_triggerId;

  // Public methods
public:
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  DropTrigReq::RequestType getRequestType() const {
    return (DropTrigReq::RequestType)m_requestInfo;
  }
  void setRequestType(DropTrigReq::RequestType val) {
    m_requestInfo = (Uint32)val;
  }
  Uint32 getTableId() const {
    return m_tableId;
  }
  void setTableId(Uint32 val) {
    m_tableId = val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
  Uint32 getTriggerId() const {
    return m_triggerId;
  }
  void setTriggerId(Uint32 val) {
    m_triggerId = val;
  }
};

/**
 * DropTrigRef.
 */
class DropTrigRef {
  friend bool printDROP_TRIG_REF(FILE*, const Uint32*, Uint32, Uint16);

public:
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    TriggerNotFound = 4238,
    BadRequestType = 4247,
    InvalidName = 4248
  };
  STATIC_CONST( SignalLength = DropTrigConf::SignalLength + 3 );

private:
  DropTrigConf m_conf;
  //Uint32 m_userRef;
  //Uint32 m_connectionPtr;
  //Uint32 m_requestInfo;
  //Uint32 m_tableId;
  //Uint32 m_indexId;
  //Uint32 m_triggerId;
  Uint32 m_errorCode;
  Uint32 m_errorLine;
  Uint32 m_errorNode;

public:
  DropTrigConf* getConf() {
    return &m_conf;
  }
  const DropTrigConf* getConf() const {
    return &m_conf;
  }
  Uint32 getUserRef() const {
    return m_conf.getUserRef();
  }
  void setUserRef(Uint32 val) {
    m_conf.setUserRef(val);
  }
  Uint32 getConnectionPtr() const {
    return m_conf.getConnectionPtr();
  }
  void setConnectionPtr(Uint32 val) {
    m_conf.setConnectionPtr(val);
  }
  DropTrigReq::RequestType getRequestType() const {
    return m_conf.getRequestType();
  }
  void setRequestType(DropTrigReq::RequestType val) {
    m_conf.setRequestType(val);
  }
  Uint32 getTableId() const {
    return m_conf.getTableId();
  }
  void setTableId(Uint32 val) {
    m_conf.setTableId(val);
  }
  Uint32 getIndexId() const {
    return m_conf.getIndexId();
  }
  void setIndexId(Uint32 val) {
    m_conf.setIndexId(val);
  }
  Uint32 getTriggerId() const {
    return m_conf.getTriggerId();
  }
  void setTriggerId(Uint32 val) {
    m_conf.setTriggerId(val);
  }
  DropTrigRef::ErrorCode getErrorCode() const {
    return (DropTrigRef::ErrorCode)m_errorCode;
  }
  void setErrorCode(DropTrigRef::ErrorCode val) {
    m_errorCode = (Uint32)val;
  }
  Uint32 getErrorLine() const {
    return m_errorLine;
  }
  void setErrorLine(Uint32 val) {
    m_errorLine = val;
  }
  Uint32 getErrorNode() const {
    return m_errorNode;
  }
  void setErrorNode(Uint32 val) {
    m_errorNode = val;
  }
};

#endif
