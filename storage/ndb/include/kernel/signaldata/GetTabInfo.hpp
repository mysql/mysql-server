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

#ifndef GET_INFO_TAB_HPP
#define GET_INFO_TAB_HPP

#include "SignalData.hpp"

/**
 * GetTabInfo - Get table info from DICT
 *
 * Successfull return = series of DICTTABINFO-signals
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
  STATIC_CONST( SignalLength = 5 );
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType; // Bitmask of GetTabInfoReq::RequestType
  union {
    Uint32 tableId;
    Uint32 tableNameLen;
  };
  Uint32 unused; // This is located here so that Req & Ref have the same format

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
  STATIC_CONST( SignalLength = 5 );
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType; // Bitmask of GetTabInfoReq::RequestType
  union {
    Uint32 tableId;
    Uint32 tableNameLen;
  };
  Uint32 errorCode;

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
  STATIC_CONST( SignalLength = 6 );

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

#endif
