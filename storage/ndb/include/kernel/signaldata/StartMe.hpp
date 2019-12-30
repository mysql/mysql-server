/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef START_ME_HPP
#define START_ME_HPP

#define JAM_FILE_ID 152


/**
 * This signal is sent...
 *
 * It also contains the Sysfile.
 * Since the Sysfile can be larger than on StartMeConf signal,
 *   there might be more than on of these signals sent before
 *   the entire sysfile is transfered
 *
 */
class StartMeReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 2 );
private:
  
  Uint32 startingRef;
  Uint32 startingVersion;
};

class StartMeConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength_v1 = 25 );
  STATIC_CONST( SignalLength_v2 = 2 );
private:
  
  Uint32 startingNodeId;
  Uint32 startWord;
  
  /**
   * No of free words to carry data
   */
  STATIC_CONST( DATA_SIZE = 23 );
  
  Uint32 data[DATA_SIZE];
};

#undef JAM_FILE_ID

#endif
