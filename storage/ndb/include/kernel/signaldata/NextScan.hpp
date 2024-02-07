/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NEXT_SCAN_HPP
#define NEXT_SCAN_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 11

class NextScanReq {
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtux;
  friend class Dbtup;

 public:
  // two sets of defs picked from lqh/acc
  enum ScanFlag {
    ZSCAN_NEXT = 1,
    ZSCAN_NEXT_COMMIT = 2,
    ZSCAN_COMMIT = 3,  // new
    ZSCAN_CLOSE = 6,
    ZSCAN_NEXT_ABORT = 12
  };
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 accPtr;  // scan record in ACC/TUX
  Uint32 accOperationPtr;
  Uint32 scanFlag;
};

class NextScanConf {
  friend class Dbacc;
  friend class Dbtux;
  friend class Dbtup;
  friend class Dblqh;

 public:
  // length is less if no keyinfo or no next result
  static constexpr Uint32 SignalLengthNoKeyInfo = 6;
  static constexpr Uint32 SignalLengthNoTuple = 3;
  static constexpr Uint32 SignalLengthNoGCI = 5;

 private:
  Uint32 scanPtr;  // scan record in LQH
  Uint32 accOperationPtr;
  Uint32 fragId;
  Uint32 localKey[2];
  Uint32 gci;
};

class NextScanRef {
  friend class Dbtux;
  friend class Dbtup;
  friend class Dblqh;

 public:
  static constexpr Uint32 SignalLength = 4;

 private:
  Uint32 scanPtr;
  Uint32 accOperationPtr;
  Uint32 fragId;
  Uint32 errorCode;
};

#undef JAM_FILE_ID

#endif
