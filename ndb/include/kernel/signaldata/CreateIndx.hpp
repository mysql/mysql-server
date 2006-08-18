/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CREATE_INDX_HPP
#define CREATE_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <signaldata/DictTabInfo.hpp>

/**
 * CreateIndxReq.
 */
class CreateIndxReq {
  friend bool printCREATE_INDX_REQ(FILE*, const Uint32*, Uint32, Uint16);

public:
  enum RequestType {
    RT_UNDEFINED = 0,
    RT_USER = 1,
    RT_DICT_PREPARE = 1 << 4,
    RT_DICT_COMMIT = 0xC << 4,
    RT_DICT_ABORT = 0xF << 4,
    RT_TC = 5 << 8
  };
  STATIC_CONST( SignalLength = 8 );
  SECTION( ATTRIBUTE_LIST_SECTION = 0 );
  SECTION( INDEX_NAME_SECTION = 1 );

private:
  Uint32 m_connectionPtr;       // user "schema connection"
  Uint32 m_userRef;             // user block reference
  Uint32 m_requestInfo;
  Uint32 m_tableId;             // table to index
  Uint32 m_indexType;           // from DictTabInfo::TableType
  Uint32 m_indexId;             // index table id set by DICT
  Uint32 m_indexVersion;        // index table version set by DICT
  Uint32 m_online;              // alter online
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
  CreateIndxReq::RequestType getRequestType() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_requestInfo, 0, 16);
    return (CreateIndxReq::RequestType)val;
  }
  void setRequestType(CreateIndxReq::RequestType val) {
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
  DictTabInfo::TableType getIndexType() const {
    return (DictTabInfo::TableType)m_indexType;
  }
  void setIndexType(DictTabInfo::TableType val) {
    m_indexType = (Uint32)val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
  Uint32 getOnline() const {
    return m_online;
  }
  void setOnline(Uint32 val) {
    m_online = val;
  }
  Uint32 getIndexVersion() const {
    return m_indexVersion;
  }
  void setIndexVersion(Uint32 val) {
    m_indexVersion = val;
  }
  Uint32 getOpKey() const {
    return m_opKey;
  }
  void setOpKey(Uint32 val) {
    m_opKey = val;
  }
};

/**
 * CreateIndxConf.
 */
class CreateIndxConf {
  friend bool printCREATE_INDX_CONF(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( InternalLength = 3 );
  STATIC_CONST( SignalLength = 7 );

private:
  Uint32 m_connectionPtr;
  Uint32 m_userRef;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexType;
  Uint32 m_indexId;
  Uint32 m_indexVersion;

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
  CreateIndxReq::RequestType getRequestType() const {
    return (CreateIndxReq::RequestType)m_requestInfo;
  }
  void setRequestType(CreateIndxReq::RequestType val) {
    m_requestInfo = (Uint32)val;
  }
  Uint32 getTableId() const {
    return m_tableId;
  }
  void setTableId(Uint32 val) {
    m_tableId = val;
  }
  DictTabInfo::TableType getIndexType() const {
    return (DictTabInfo::TableType)m_indexType;
  }
  void setIndexType(DictTabInfo::TableType val) {
    m_indexType = (Uint32)val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
  Uint32 getIndexVersion() const {
    return m_indexVersion;
  }
  void setIndexVersion(Uint32 val) {
    m_indexVersion = val;
  }
};

/**
 * CreateIndxRef.
 */
struct CreateIndxRef {
  friend bool printCREATE_INDX_REF(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = CreateIndxReq::SignalLength + 3 );
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    TriggerNotFound = 4238,
    TriggerExists = 4239,
    IndexNameTooLong = 4241,
    TooManyIndexes = 4242,
    IndexExists = 4244,
    AttributeNullable = 4246,
    BadRequestType = 4247,
    InvalidName = 4248,
    InvalidPrimaryTable = 4249,
    InvalidIndexType = 4250,
    NotUnique = 4251,
    AllocationError = 4252,
    CreateIndexTableFailed = 4253,
    DuplicateAttributes = 4258
  };

  CreateIndxConf m_conf;
  //Uint32 m_userRef;
  //Uint32 m_connectionPtr;
  //Uint32 m_requestInfo;
  //Uint32 m_tableId;
  //Uint32 m_indexType;
  //Uint32 m_indexId;
  //Uint32 m_indexVersion;
  Uint32 m_errorCode;
  Uint32 m_errorLine;
  union {
    Uint32 m_errorNode;
    Uint32 masterNodeId; // If NotMaster
  };
public:
  CreateIndxConf* getConf() {
    return &m_conf;
  }
  const CreateIndxConf* getConf() const {
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
  CreateIndxReq::RequestType getRequestType() const {
    return m_conf.getRequestType();
  }
  void setRequestType(CreateIndxReq::RequestType val) {
    m_conf.setRequestType(val);
  }
  Uint32 getTableId() const {
    return m_conf.getTableId();
  }
  void setTableId(Uint32 val) {
    m_conf.setTableId(val);
  }
  DictTabInfo::TableType getIndexType() const {
    return m_conf.getIndexType();
  }
  void setIndexType(DictTabInfo::TableType val) {
    m_conf.setIndexType(val);
  }
  Uint32 getIndexId() const {
    return m_conf.getIndexId();
  }
  void setIndexId(Uint32 val) {
    m_conf.setIndexId(val);
  }
  Uint32 getIndexVersion() const {
    return m_conf.getIndexVersion();
  }
  void setIndexVersion(Uint32 val) {
    m_conf.setIndexVersion(val);
  }
  CreateIndxRef::ErrorCode getErrorCode() const {
    return (CreateIndxRef::ErrorCode)m_errorCode;
  }
  void setErrorCode(CreateIndxRef::ErrorCode val) {
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
