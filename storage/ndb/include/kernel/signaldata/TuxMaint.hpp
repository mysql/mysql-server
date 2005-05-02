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

#ifndef TUX_MAINT_HPP
#define TUX_MAINT_HPP

#include "SignalData.hpp"

/*
 * Ordered index maintenance operation.
 */

class TuxMaintReq {
  friend class Dbtup;
  friend class Dbtux;
  friend bool printTUX_MAINT_REQ(FILE*, const Uint32*, Uint32, Uint16);
public:
  enum OpCode {         // first byte of opInfo
    OpAdd = 1,
    OpRemove = 2
  };
  enum OpFlag {         // second byte of opInfo
  };
  enum ErrorCode {
    NoError = 0,        // must be zero
    SearchError = 901,  // add + found or remove + not found
    NoMemError = 902
  };
  STATIC_CONST( SignalLength = 8 );
private:
  /*
   * Error code set by TUX.  Zero means no error.
   */
  Uint32 errorCode;
  /*
   * Table, index, fragment.
   */
  Uint32 tableId;
  Uint32 indexId;
  Uint32 fragId;
  /*
   * Tuple version identified by physical address of "original" tuple
   * and version number.
   */
  Uint32 pageId;
  Uint32 pageOffset;
  Uint32 tupVersion;
  /*
   * Operation code and flags.
   */
  Uint32 opInfo;
};

#endif
