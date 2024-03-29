/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef TC_HB_REP_H
#define TC_HB_REP_H

#include "SignalData.hpp"

#define JAM_FILE_ID 93


/**
 * @class TcHbRep
 * @brief Order tc refresh(exetend) the timeout counters for this 
 *        transaction
 *
 * - SENDER:    API
 * - RECEIVER:  TC
 */
class TcHbRep {
  /**
   * Receiver(s)
   */
  friend class Dbtc;         // Receiver

  /**
   * Sender(s)
   */
  friend class NdbTransaction;      

  /**
   * For printing
   */
  friend bool printTC_HBREP(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
  static constexpr Uint32 SignalLength = 3;

private:

  /**
   * DATA VARIABLES
   */

  Uint32 apiConnectPtr;       // DATA 0
  UintR transId1;             // DATA 1
  UintR transId2;             // DATA 2
};



#undef JAM_FILE_ID

#endif
