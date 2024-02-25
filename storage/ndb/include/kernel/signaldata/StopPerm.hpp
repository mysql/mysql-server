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
  static constexpr Uint32 SignalLength = 2;
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
  static constexpr Uint32 SignalLength = 1;
  
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
  static constexpr Uint32 SignalLength = 2;

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
