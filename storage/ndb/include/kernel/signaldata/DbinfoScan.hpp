/* 
   Copyright 2008, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef DBINFO_SCAN_H
#define DBINFO_SCAN_H

#include "SignalData.hpp"

struct DbinfoScanCursor
{
  Uint32 data[11];
};

struct DbinfoScan
{
  STATIC_CONST( SignalLength = 12 );

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
  STATIC_CONST( SignalLength = 5 );

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

#endif
