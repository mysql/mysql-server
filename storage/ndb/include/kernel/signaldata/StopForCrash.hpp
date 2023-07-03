/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef STOP_FOR_CRASH_HPP
#define STOP_FOR_CRASH_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 171


/*
  The GSN_STOP_FOR_CRASH signal is only used in multi-threaded ndbd.

  It is used during crash handling, sent as a prio A signal from the
  crashing thread to all other threads to make sure that they stop before
  generating the crash dump (to avoid dumping an inconsistent state of jam()
  or signal buffer).
*/

class StopForCrash {
  friend class SimulatedBlock;

  friend bool printSTOP_FOR_CRASH(FILE *,const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 1;

public:
  Uint32 flags;                 // No information in this signal atm.
};


#undef JAM_FILE_ID

#endif
