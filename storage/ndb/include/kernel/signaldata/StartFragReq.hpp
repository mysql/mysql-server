/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
  STATIC_CONST( SignalLength = 20 );

  friend bool printSTART_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
  
  enum
  {
    SFR_RESTORE_LCP = 1,
    SFR_COPY_FRAG = 2
  };

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
};

#undef JAM_FILE_ID

#endif
