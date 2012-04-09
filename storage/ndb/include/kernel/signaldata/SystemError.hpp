/*
   Copyright (C) 2003, 2005-2007 MySQL AB
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
    TestStopOnError = 6,
    CopySubscriptionRef = 7,
    CopySubscriberRef = 8,
    StartFragRefError = 9
  };
  
  Uint32 errorRef;
  Uint32 errorCode;
  Uint32 data[1];
};

#endif

