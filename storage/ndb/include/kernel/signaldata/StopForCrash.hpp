/* Copyright (C) 2008 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef STOP_FOR_CRASH_HPP
#define STOP_FOR_CRASH_HPP

#include "SignalData.hpp"

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
  STATIC_CONST( SignalLength = 1 );

public:
  Uint32 flags;                 // No information in this signal atm.
};

#endif
