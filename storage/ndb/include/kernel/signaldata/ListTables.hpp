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

#ifndef LIST_TABLES_HPP
#define LIST_TABLES_HPP

#include <Bitmask.hpp>
#include "SignalData.hpp"

/**
 * It is convenient to pack request/response data per table in one
 * 32-bit word...
 */
class ListTablesData {
public:
  static Uint32 getTableId(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 0, 12);
  }
  static void setTableId(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 0, 12, val);
  }
  static Uint32 getTableType(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 12, 8);
  }
  static void setTableType(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 12, 8, val);
  }
  static Uint32 getTableStore(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 20, 3);
  }
  static void setTableStore(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 20, 3, val);
  }
  static Uint32 getTableTemp(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 23, 1);
  }
  static void setTableTemp(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 23, 1, val);
  }
  static Uint32 getTableState(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 24, 4);
  }
  static void setTableState(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 24, 4, val);
  }
  static Uint32 getListNames(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 28, 1);
  }
  static void setListNames(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 28, 1, val);
  }
  static Uint32 getListIndexes(Uint32 data) {
    return BitmaskImpl::getField(1, &data, 29, 1);
  }
  static void setListIndexes(Uint32& data, Uint32 val) {
    BitmaskImpl::setField(1, &data, 29, 1, val);
  }
};

class ListTablesReq {
  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Table;
  friend class Suma;
  
  /**
   * Reciver(s)
   */
  friend class Dbdict;

public:
  STATIC_CONST( SignalLength = 3 );

public:  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestData;

  Uint32 getTableId() {
    return ListTablesData::getTableId(requestData);
  }
  void setTableId(Uint32 val) {
    ListTablesData::setTableId(requestData, val);
  }
  Uint32 getTableType() const {
    return ListTablesData::getTableType(requestData);
  }
  void setTableType(Uint32 val) {
    ListTablesData::setTableType(requestData, val);
  }
  Uint32 getListNames() const {
    return ListTablesData::getListNames(requestData);
  }
  void setListNames(Uint32 val) {
    ListTablesData::setListNames(requestData, val);
  }
  Uint32 getListIndexes() const {
    return ListTablesData::getListIndexes(requestData);
  }
  void setListIndexes(Uint32 val) {
    ListTablesData::setListIndexes(requestData, val);
  }
};

class ListTablesConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class Table;
  friend class Suma;

public:
  /**
   * Note: last signal is indicated by having length < 25
   */
  STATIC_CONST( SignalLength = 25 );
  STATIC_CONST( HeaderLength = 2  );
  STATIC_CONST( DataLength   = 23 );

public:  
  Uint32 senderData;
  Uint32 counter;
  Uint32 tableData[DataLength];

  static Uint32 getTableId(Uint32 data) {
    return ListTablesData::getTableId(data);
  }
  void setTableId(unsigned pos, Uint32 val) {
    ListTablesData::setTableId(tableData[pos], val);
  }
  static Uint32 getTableType(Uint32 data) {
    return ListTablesData::getTableType(data);
  }
  void setTableType(unsigned pos, Uint32 val) {
    ListTablesData::setTableType(tableData[pos], val);
  }
  static Uint32 getTableStore(Uint32 data) {
    return ListTablesData::getTableStore(data);
  }
  void setTableStore(unsigned pos, Uint32 val) {
    ListTablesData::setTableStore(tableData[pos], val);
  }
  static Uint32 getTableState(Uint32 data) {
    return ListTablesData::getTableState(data);
  }
  void setTableState(unsigned pos, Uint32 val) {
    ListTablesData::setTableState(tableData[pos], val);
  }
  static Uint32 getTableTemp(Uint32 data) {
    return ListTablesData::getTableTemp(data);
  }
  void setTableTemp(unsigned pos, Uint32 val) {
    ListTablesData::setTableTemp(tableData[pos], val);
  }
};

#endif
