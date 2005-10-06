/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef SYSTEM_ERROR_HPP
#define SYSTEM_ERROR_HPP

#include "SignalData.hpp"

class SystemError {

  /**
   * Reciver(s)
   */
  friend class Ndbcntr;

  /**
   * Sender
   */
  friend class Dbtc;
  friend class Dbdih;

  /**
   * For printing
   */
  friend bool printSYSTEM_ERROR(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    GCPStopDetected = 3,
    CopyFragRefError = 5,
    TestStopOnError = 6
  };
  
private:
  Uint32 errorRef;
  Uint32 errorCode;
  Uint32 data1;
  Uint32 data2;
};

#endif

