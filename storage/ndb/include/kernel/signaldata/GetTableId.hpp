/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef GET_TABLEID_HPP
#define GET_TABLEID_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 10


/**
 * Convert tabname to table id
 */
class GetTableIdReq {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;

  friend bool printGET_TABLEID_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 len;
  SECTION( TABLE_NAME = 0 );
};


/**
 * Convert tabname to table id
 */
class GetTableIdRef {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;

  friend bool printGET_TABLEID_REF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 err;

  enum ErrorCode {
    InvalidTableId  = 709,
    TableNotDefined = 723,
    TableNameTooLong = 702,
    EmptyTable = 1111
  };
};


/**
 * Convert tabname to table id
 */
class GetTableIdConf {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Dbdict;

  friend bool printGET_TABLEID_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 4 );
public:
  Uint32 senderData; 
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 schemaVersion;
  
};



#undef JAM_FILE_ID

#endif
