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

#ifndef NEXT_SCAN_HPP
#define NEXT_SCAN_HPP

#include "SignalData.hpp"

class NextScanReq {
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtux;
public:
  // two sets of defs picked from lqh/acc
  enum ScanFlag {
    ZSCAN_NEXT = 1,
    ZSCAN_NEXT_COMMIT = 2,
    ZSCAN_COMMIT = 3,           // new
    ZSCAN_CLOSE = 6,
    ZSCAN_NEXT_ABORT = 12
  };
  enum CopyFlag {
    todo_ZCOPY_NEXT = 1,
    todo_ZCOPY_NEXT_COMMIT = 2,
    todo_ZCOPY_COMMIT = 3,
    todo_ZCOPY_REPEAT = 4,
    todo_ZCOPY_ABORT = 5,
    todo_ZCOPY_CLOSE = 6
  };
  STATIC_CONST( SignalLength = 3 );
private:
  Uint32 accPtr;                // scan record in ACC/TUX
  Uint32 accOperationPtr;
  Uint32 scanFlag;
};

class NextScanConf {
  friend class Dbacc;
  friend class Dbtux;
  friend class Dblqh;
public:
  // length is less if no keyinfo or no next result
  STATIC_CONST( SignalLength = 11 );
private:
  Uint32 scanPtr;               // scan record in LQH
  Uint32 accOperationPtr;
  Uint32 fragId;
  Uint32 localKey[2];
  Uint32 localKeyLength;
  Uint32 keyLength;
  Uint32 key[4];
};

#endif
