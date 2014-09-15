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

#ifndef TCKEYFAILCONF_HPP
#define TCKEYFAILCONF_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 94


/**
 * This is signal is sent from "Take-Over" TC after a node crash
 * It means that the transaction was committed
 */
class TcKeyFailConf {
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


#undef JAM_FILE_ID

#endif
