/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ISOLATE_ORD_HPP
#define ISOLATE_ORD_HPP

#include "SignalData.hpp"
#include "NodeBitmask.hpp"

#define JAM_FILE_ID 494

/**
 *
 */

class IsolateOrd
{
  /**
   * Sender(s) & Receiver(s)
   */
  friend class Dbdih;
  friend class Dblqh;
  friend class Qmgr;
  
  /**
   * For printing
   */
  friend bool printISOLATE_ORD(FILE *, const Uint32*, Uint32, Uint16);

private:
  STATIC_CONST(SignalLength = 3 + NdbNodeBitmask::Size);

  enum IsolateStep 
  {
    IS_REQ = 0,
    IS_BROADCAST = 1,
    IS_DELAY = 2
  };

  Uint32 senderRef;
  Uint32 isolateStep;
  Uint32 delayMillis;           /* 0 = immediate */
  Uint32 nodesToIsolate[NdbNodeBitmask::Size];

};
  
#undef JAM_FILE_ID

#endif
