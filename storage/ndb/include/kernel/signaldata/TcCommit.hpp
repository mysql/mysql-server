/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef TCCOMMITCONF_HPP
#define TCCOMMITCONF_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 163


/**
 * This is signal is sent from TC to API
 * It means that the transaction was committed
 */
class TcCommitConf {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  
  /**
   *  Reciver(s)
   */
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbTransaction;

  friend class TransporterFacade;

public:
  static constexpr Uint32 SignalLength = 5;
private:

  /**
   * apiConnectPtr
   *
   * Bit 0 (lowest) is used as indicator 
   *                if == 1 then tc expects a commit ack
   */
  Uint32 apiConnectPtr;
  
  Uint32 transId1;
  Uint32 transId2;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

class TcCommitRef {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  
  /**
   *  Reciver(s)
   */
  friend class NdbTransaction;

public:
  static constexpr Uint32 SignalLength = 4;
private:
  
  Uint32 apiConnectPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif
