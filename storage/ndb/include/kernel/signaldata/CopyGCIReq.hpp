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

#ifndef COPY_GCI_REQ_HPP
#define COPY_GCI_REQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 212


/**
 * This signal is used for transfering the sysfile 
 * between Dih on different nodes.
 *
 * The master will distributes the file to the other nodes
 *
 * Since the Sysfile can be larger than on StartMeConf signal,
 *   there might be more than on of these signals sent before
 *   the entire sysfile is transfered
 */
class CopyGCIReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  friend bool printCOPY_GCI_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  enum CopyReason {
    IDLE                    = 0,
    LOCAL_CHECKPOINT        = 1,
    RESTART                 = 2,
    GLOBAL_CHECKPOINT       = 3,
    INITIAL_START_COMPLETED = 4,
    RESTART_NR              = 5
  };
  
private:
  
  Uint32 anyData;
  Uint32 copyReason;
  Uint32 startWord;

  /**
   * No of free words to carry data
   */
  STATIC_CONST( SignalLength = 3);
  STATIC_CONST( DATA_SIZE = 22 );
  
  Uint32 data[DATA_SIZE];
};


#undef JAM_FILE_ID

#endif
