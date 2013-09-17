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

#ifndef TAMPERORD_H
#define TAMPERORD_H

#include "SignalData.hpp"

#define JAM_FILE_ID 117


class TamperOrd  {
  /**
   * Sender
   */
  friend class MgmtSrvr;
  
  /**
   * Receiver
   */
  friend class Cmvmi;
  
private:
  STATIC_CONST( SignalLength = 1 );
  
  UintR errorNo;
};


#undef JAM_FILE_ID

#endif // TAMPERORD_H

