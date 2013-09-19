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

#ifndef EXEC_FRAGREQ_HPP
#define EXEC_FRAGREQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 92


class ExecFragReq {
  /**
   * Sender & Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
public:
  STATIC_CONST( SignalLength = 7 );

private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 startGci;
  Uint32 lastGci;
  Uint32 dst; // Final destination
};

#undef JAM_FILE_ID

#endif
