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

#ifndef GET_INFO_TAB_HPP
#define GET_INFO_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 29


/**
 * GetTabInfo - Get table info from DICT
 *
 * Successful return = series of DICTTABINFO-signals
 */
class GetTabInfoReq {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;
  friend class Backup;
  friend class Trix;
  friend class DbUtil;
  // API
  friend class Table;

  friend bool printGET_TABINFO_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  static constexpr Uint32 SignalLength = 5;
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType; // Bitmask of GetTabInfoReq::RequestType
  union {
    Uint32 tableId;
    Uint32 tableNameLen;
  };
  Uint32 schemaTransId; // To see own schema trans

  enum RequestType {
    RequestById = 0,
    RequestByName = 1,
    LongSignalConf = 2
  };
  SECTION( TABLE_NAME = 0 );
};

class GetTabInfoRef {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;
  friend class Backup;
  friend class Trix;
  friend class DbUtil;
  // API
  friend class Table;

  friend bool printGET_TABINFO_REF(FILE *, const Uint32 *, Uint32, Uint16);    
public:
  static constexpr Uint32 SignalLength = 7;
  /* 6.3 <-> 7.0 upgrade code */
  static constexpr Uint32 OriginalSignalLength = 5;
  static constexpr Uint32 OriginalErrorOffset = 4;
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType; // Bitmask of GetTabInfoReq::RequestType
  union {
    Uint32 tableId;
    Uint32 tableNameLen;
  };
  Uint32 schemaTransId;
  Uint32 errorCode;
  Uint32 errorLine;

  enum ErrorCode {
    InvalidTableId = 709,
    TableNotDefined = 723,
    TableNameTooLong = 702,
    NoFetchByName = 710,
    Busy = 701
  };
};

class GetTabInfoConf {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;
  friend class Backup;
  friend class Trix;
  friend class DbUtil;
  friend class Suma;
  // API
  friend class Table;

  friend bool printGET_TABINFO_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  static constexpr Uint32 SignalLength = 6;

  SECTION( DICT_TAB_INFO = 0 );
public:
  Uint32 senderData;
  Uint32 tableId;
  union {
    Uint32 gci; // For table
    Uint32 freeWordsHi; // for logfile group m_free_file_words
  };
  union {
    Uint32 totalLen; // In words
    Uint32 freeExtents;
    Uint32 freeWordsLo; // for logfile group m_free_file_words
  };
  Uint32 tableType;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
