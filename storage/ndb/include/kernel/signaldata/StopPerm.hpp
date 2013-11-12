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

#ifndef STOP_PERM_HPP
#define STOP_PERM_HPP

#define JAM_FILE_ID 144


/**
 * This signal is sent by ndbcntr to local DIH
 *
 * If local DIH is not master, it forwards it to master DIH
 *   and start acting as a proxy
 *
 * @see StopMeReq
 * @see StartMeReq
 * @see StartPermReq
 */
class StopPermReq {
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Sender
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 2 );
public:
  
  Uint32 senderRef;
  Uint32 senderData;
};

class StopPermConf {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 1 );
  
private:
  Uint32 senderData;
};

class StopPermRef {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 2 );

  enum ErrorCode {
    StopOK = 0,
    NodeStartInProgress = 1,
    NodeShutdownInProgress = 2,
    NF_CausedAbortOfStopProcedure = 3
  };
  
private:
  Uint32 errorCode;
  Uint32 senderData;
};


#undef JAM_FILE_ID

#endif
