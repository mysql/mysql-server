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

#ifndef STOP_ME_HPP
#define STOP_ME_HPP

#define JAM_FILE_ID 51


/**
 * This signal is sent by ndbcntr to local DIH
 *
 * If local DIH then sends it to all DIH's
 *
 * @see StopPermReq
 * @see StartMeReq
 * @see StartPermReq
 */
struct StopMeReq
{
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  friend class Suma;
  
  /**
   * Sender
   */
  friend class Ndbcntr;

  static constexpr Uint32 SignalLength = 2;
  
  Uint32 senderRef;
  Uint32 senderData;
};

struct StopMeConf
{

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;

  static constexpr Uint32 SignalLength = 2;
  
  Uint32 senderRef;
  Uint32 senderData;
};



#undef JAM_FILE_ID

#endif
