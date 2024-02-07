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

#ifndef START_FRAGREQ_HPP
#define START_FRAGREQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 67

class StartFragReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dblqh;

 public:
  static constexpr Uint32 SignalOldLength = 20;
  static constexpr Uint32 SignalLength = 21;

  friend bool printSTART_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  enum { SFR_RESTORE_LCP = 1, SFR_COPY_FRAG = 2 };

  Uint32 userPtr;
  Uint32 userRef;
  Uint32 lcpNo;
  Uint32 lcpId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 noOfLogNodes;
  Uint32 lqhLogNode[4];
  Uint32 startGci[4];
  Uint32 lastGci[4];
  Uint32 requestInfo;
  Uint32 nodeRestorableGci;
};

#undef JAM_FILE_ID

#endif
