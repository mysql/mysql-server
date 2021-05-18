/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TUX_MAINT_HPP
#define TUX_MAINT_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 95


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
    NoMemError = 902,
    NoTransMemError = 922
  };
  STATIC_CONST( SignalLength = 8 );

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
  Uint32 pageIndex;
  Uint32 tupVersion;
  /*
   * Operation code and flags.
   */
  Uint32 opInfo;

  Uint32 tupFragPtrI;
  Uint32 fragPageId;
};


#undef JAM_FILE_ID

#endif
