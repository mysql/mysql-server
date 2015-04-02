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
  STATIC_CONST( SignalLength = 5 );
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
  STATIC_CONST( SignalLength = 4 );
private:
  
  Uint32 apiConnectPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif
