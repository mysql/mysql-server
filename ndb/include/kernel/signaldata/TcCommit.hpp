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

#ifndef TCCOMMITCONF_HPP
#define TCCOMMITCONF_HPP

#include "SignalData.hpp"

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
  friend class NdbConnection;

public:
  STATIC_CONST( SignalLength = 3 );
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
};

class TcCommitRef {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  
  /**
   *  Reciver(s)
   */
  friend class NdbConnection;

public:
  STATIC_CONST( SignalLength = 4 );
private:
  
  Uint32 apiConnectPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 errorCode;
};

#endif
