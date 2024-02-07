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

#ifndef TUP_COMMIT_H
#define TUP_COMMIT_H

#include "SignalData.hpp"

#define JAM_FILE_ID 85

class TupCommitReq {
  /**
   * Reciver(s)
   */
  friend class Dbtup;

  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * For printing
   */
  friend bool printTUPCOMMITREQ(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 SignalLength = 7;

 private:
  /**
   * DATA VARIABLES
   */
  Uint32 opPtr;
  Uint32 gci_hi;
  Uint32 hashValue;
  Uint32 diskpage;
  Uint32 gci_lo;
  Uint32 transId1;
  Uint32 transId2;
};

#undef JAM_FILE_ID

#endif
