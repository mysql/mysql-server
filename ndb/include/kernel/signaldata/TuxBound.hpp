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

#ifndef TUX_BOUND_HPP
#define TUX_BOUND_HPP

#include "SignalData.hpp"

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
    BoundEQ = 4
  };
  enum ErrorCode {
    InvalidAttrInfo = 4110,
    InvalidBounds = 4259,
    OutOfBuffers = 873
  };
  STATIC_CONST( SignalLength = 3 );
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
};

#endif
