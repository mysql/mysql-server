/* Copyright (C) 2007 MySQL AB

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

#ifndef DBINFO_SCAN_H
#define DBINFO_SCAN_H

#include "SignalData.hpp"

/**
 * SENDER:  API,MGM
 * RECIVER: DBINFO
 */
struct DbinfoScanReq
{
  /* Reciver(s) */
  friend class Dbinfo;

  /* Sender(s) */

  friend bool printDBINFO_SCANREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( SignalLengthWithCursor = 14 );
//private:
  Uint32 tableId;         // DBINFO table ID
  Uint32 senderRef;       // API doing scan
  Uint32 apiTxnId;        // ID unique to API.
  Uint32 colBitmapLo;     // bitmap of what columns you want. (64bit)
  Uint32 colBitmapHi;
  Uint32 requestInfo;     // start, endofdata

  STATIC_CONST( StartScan  = 0x1 );
  STATIC_CONST( AllColumns = 0x2 );

  Uint32 maxRows;
  Uint32 maxBytes;
  Uint32 rows_total;
  Uint32 word_total;

  Uint32 cursor[0];
  Uint32 cur_requestInfo;
  Uint32 cur_node;
  Uint32 cur_block;
  Uint32 cur_item;
};

/**
 * SENDER:  DBINFO
 * RECIVER: API,MGM
 */
class DbinfoScanConf {
  /* Reciver(s) */

  /* Sender(s) */
  friend class Dbinfo;

  friend bool printDBINFO_SCANCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( SignalLengthWithCursor = 14 );

  Uint32 tableId;         // DBINFO table ID
  Uint32 senderRef;       // API doing scan
  Uint32 apiTxnId;        // ID unique to API.
  Uint32 colBitmapLo;     // bitmap of what columns you want. (64bit)
  Uint32 colBitmapHi;
  Uint32 requestInfo;     // start, endofdata

  STATIC_CONST( MoreData  = 0x1 );
  STATIC_CONST( AllColumns = 0x2 );

  Uint32 maxRows;
  Uint32 maxBytes;
  Uint32 rows_total;
  Uint32 word_total;

  Uint32 cursor[0];
  Uint32 cur_requestInfo;
  Uint32 cur_node;
  Uint32 cur_block;
  Uint32 cur_item;

};

/**
 * SENDER:  DBINFO
 * RECIVER: API,MGM
 */
class DbinfoScanRef {
  /* Reciver(s) */

  /* Sender(s) */
  friend class Dbinfo;

  friend bool printDBINFO_SCANREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 tableId;         // DBINFO table ID
  Uint32 apiTxnId;        // ID unique to API.
  Uint32 errorCode;       // Error Code
};

/*
  SENDER:  API
  RECIVER: Dbtc

  This signal is sent by API to acknowledge the reception of batches of rows
  from one or more fragment scans, and to request the fetching of the next
  batches of rows.
 */
class DbinfoScanNextReq {
  /* Reciver(s) */
  friend class Dbinfo;

  /* Sender(s) */

  friend bool printDBINFO_SCANNEXTREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 4 );

private:
};

#endif
