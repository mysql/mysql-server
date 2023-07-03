/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef TAKE_OVERTCCONF_H
#define TAKE_OVERTCCONF_H

#include "ndb_types.h"

#define JAM_FILE_ID 515

class TakeOverTcConf
{
  /* Sender */
  friend class Dbtc;
  friend class DbtcProxy;
  friend class Qmgr;

  /* Receiver */
  friend class Dbtc;
  friend class DbtcProxy;

  static constexpr Uint32 SignalLength_v8_0_17 = 2; // Last use in 8.0.17
  static constexpr Uint32 SignalLength = 3; // In use since 8.0.18

  Uint32 failedNode;
  Uint32 senderRef;
  Uint32 tcFailNo;
};

#undef JAM_FILE_ID

#endif
