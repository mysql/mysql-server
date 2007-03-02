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

#ifndef DROP_INDX_HPP
#define DROP_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * DropIndxReq.
 */
class DropIndxReq {
  friend bool printDROP_INDX_REQ(FILE*, const Uint32*, Uint32, Uint16);

public:
  enum RequestType {
    RT_UNDEFINED = 0,
    RT_USER = 1,
    RT_DICT_PREPARE = 1 << 4,
    RT_DICT_COMMIT = 0xC << 4,
    RT_DICT_ABORT = 0xF << 4,
    RT_TC = 5 << 8
  };
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 m_connectionPtr;
  Uint32 m_userRef;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexId;
  Uint32 m_indexVersion;
  // extra
  Uint32 m_opKey;

public:
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  DropIndxReq::RequestType getRequestType() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_requestInfo, 0, 16);
    return (DropIndxReq::RequestType)val;
  }
  void setRequestType(DropIndxReq::RequestType val) {
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
 * DropIndxConf.
 */
class DropIndxConf {
  friend bool printDROP_INDX_CONF(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( InternalLength = 3 );
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 m_connectionPtr;
  Uint32 m_userRef;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexId;
  Uint32 m_indexVersion;

public:
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  DropIndxReq::RequestType getRequestType() const {
    return (DropIndxReq::RequestType)m_requestInfo;
  }
  void setRequestType(DropIndxReq::RequestType val) {
    m_requestInfo = val;
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
  Uint32 getIndexVersion() const {
    return m_indexVersion;
  }
  void setIndexVersion(Uint32 val) {
    m_indexVersion = val;
  }
};

/**
 * DropIndxRef.
 */
struct DropIndxRef {
  friend bool printDROP_INDX_REF(FILE*, const Uint32*, Uint32, Uint16);

public:
  enum ErrorCode {
    NoError = 0,
    InvalidIndexVersion = 241,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    IndexNotFound = 4243,
    BadRequestType = 4247,
    InvalidName = 4248,
    NotAnIndex = 4254,
    SingleUser = 299
  };
  STATIC_CONST( SignalLength = DropIndxConf::SignalLength + 3 );

  DropIndxConf m_conf;
  //Uint32 m_userRef;
  //Uint32 m_connectionPtr;
  //Uint32 m_requestInfo;
  //Uint32 m_tableId;
  //Uint32 m_indexId;
  //Uint32 m_indexVersion;
  Uint32 m_errorCode;
  Uint32 m_errorLine;
  union {
    Uint32 m_errorNode;
    Uint32 masterNodeId;
  };
public:
  DropIndxConf* getConf() {
    return &m_conf;
  }
  const DropIndxConf* getConf() const {
    return &m_conf;
  }
  Uint32 getConnectionPtr() const {
    return m_conf.getConnectionPtr();
  }
  void setConnectionPtr(Uint32 val) {
    m_conf.setConnectionPtr(val);
  }
  Uint32 getUserRef() const {
    return m_conf.getUserRef();
  }
  void setUserRef(Uint32 val) {
    m_conf.setUserRef(val);
  }
  DropIndxReq::RequestType getRequestType() const {
    return m_conf.getRequestType();
  }
  void setRequestType(DropIndxReq::RequestType val) {
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
  Uint32 getIndexVersion() const {
    return m_conf.getIndexVersion();
  }
  void setIndexVersion(Uint32 val) {
    m_conf.setIndexVersion(val);
  }
  DropIndxRef::ErrorCode getErrorCode() const {
    return (DropIndxRef::ErrorCode)m_errorCode;
  }
  void setErrorCode(DropIndxRef::ErrorCode val) {
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
