/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef SIGNAL_DROPPED_HPP
#define SIGNAL_DROPPED_HPP

#include "SignalData.hpp"

class SignalDroppedRep {

  /**
   * Receiver(s)
   */
  friend class SimulatedBlock;
  friend class Dbtc;
  friend class Dblqh;

  /**
   * Sender (TransporterCallback.cpp)
   */
  friend class TransporterFacade;
  friend class TransporterCallbackKernel;

  friend bool printSIGNAL_DROPPED_REP(FILE *, const Uint32 *, Uint32, Uint16);  
public:
private:
  Uint32 originalGsn;
  Uint32 originalLength;
  Uint32 originalSectionCount;
  Uint32 originalData[1];
};

#endif
