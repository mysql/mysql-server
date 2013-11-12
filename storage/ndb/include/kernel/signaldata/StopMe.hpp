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

  STATIC_CONST( SignalLength = 2 );
  
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

  STATIC_CONST( SignalLength = 2 );
  
  Uint32 senderRef;
  Uint32 senderData;
};



#undef JAM_FILE_ID

#endif
