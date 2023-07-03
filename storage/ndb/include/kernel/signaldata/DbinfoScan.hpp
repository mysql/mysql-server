/* 
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DBINFO_SCAN_H
#define DBINFO_SCAN_H

#include "SignalData.hpp"

#define JAM_FILE_ID 122


struct DbinfoScanCursor
{
  Uint32 data[11];
};

struct DbinfoScan
{
  static constexpr Uint32 SignalLength = 12;

  // API identifiers
  Uint32 resultData;      // Will be returned in TransIdAI::connectPtr
  Uint32 transId[2];      // ID unique to API
  Uint32 resultRef;       // Where to send result rows

  // Parameters for the scan
  Uint32 tableId;         // DBINFO table ID
  Uint32 colBitmap[2];     // bitmap of what columns you want. (64bit)
  Uint32 requestInfo;     // flags
  Uint32 maxRows;         // Max number of rows to return per REQ
  Uint32 maxBytes;        // Max number of bytes to return per REQ

  // Result from the scan
  Uint32 returnedRows;    // Number of rows returned for this CONF

  // Cursor that contains data used by the kernel for keeping track
  // of where it is, how many bytes or rows it has sent etc.
  // Set to zero in last CONF to indicate that scan is finished
  Uint32 cursor_sz;
  // Cursor data of cursor_sz size follows
  DbinfoScanCursor cursor;

  static const Uint32* getCursorPtr(const DbinfoScan* sig) {
    return sig->cursor.data;
  }
  static Uint32* getCursorPtrSend(DbinfoScan* sig) {
    return sig->cursor.data;
  }

};

typedef DbinfoScan DbinfoScanReq;
typedef DbinfoScan DbinfoScanConf;

struct DbinfoScanRef
{
  static constexpr Uint32 SignalLength = 5;

  // API identifiers
  Uint32 resultData;      // Will be returned in TransIdAI::connectPtr
  Uint32 transId[2];      // ID unique to API
  Uint32 resultRef;       // Where to send result rows

  Uint32 errorCode;       // Error Code
  enum ErrorCode
  {
    NoError = 0,
    NoTable = 4800
  };
};


#undef JAM_FILE_ID

#endif
