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

#ifndef BLOCK_COMMIT_ORD_HPP
#define BLOCK_COMMIT_ORD_HPP

#define JAM_FILE_ID 89


/**
 * These two signals are sent via EXECUTE_DIRECT
 *   to DBDIH from QMGR
 *
 * Block make sure that no commit is performed
 * Unblock turns on commit again
 */

class BlockCommitOrd {
  /**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Reciver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 1 );
  
private:
  Uint32 failNo; // As used by Qmgr
};

class UnblockCommitOrd {
  /**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Reciver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 1 );
  
private:
  Uint32 failNo; // As used by Qmgr  
};


#undef JAM_FILE_ID

#endif
