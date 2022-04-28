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

#ifndef TUX_BOUND_HPP
#define TUX_BOUND_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 169


class TuxBoundInfo {
  friend class Dblqh;
  friend class Dbtux;
public:
  // must match API (0-4 and no changes expected)
  enum BoundType {
    BoundLE = 0,        // bit 1 for less/greater
    BoundLT = 1,        // bit 0 for strict
    BoundGE = 2,
    BoundGT = 3,
    BoundEQ = 4,
    // stats scan parameter ids
    StatSaveSize = 11,
    StatSaveScale = 12,

    // Invalid bound
    InvalidBound = 0xFFFFFFFF
  };
  enum ErrorCode {
    InvalidAttrInfo = 4110,
    InvalidBounds = 4259,
    OutOfBuffers = 873,
    InvalidCharFormat = 744,
    TooMuchAttrInfo = 823
  };
  static constexpr Uint32 SignalLength = 3;
private:
  /*
   * Error code set by TUX.  Zero means no error.
   */
  Uint32 errorCode;
  /*
   * Pointer (i-value) to scan operation in TUX.
   */
  Uint32 tuxScanPtrI;
  /*
   * Number of words of bound info included after fixed signal data.
   */
  Uint32 boundAiLength;
  
  Uint32 data[1];
};


#undef JAM_FILE_ID

#endif
