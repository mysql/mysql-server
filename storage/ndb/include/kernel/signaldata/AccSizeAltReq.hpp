/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ACC_SIZE_ALT_REQ_H
#define ACC_SIZE_ALT_REQ_H

#include "SignalData.hpp"

#define JAM_FILE_ID 109


class AccSizeAltReq  {
  /**
   * Sender(s)
   */
  friend class ClusterConfiguration;

  /**
   * Reciver(s)
   */
  friend class Dbacc;
private:
  /**
   * Indexes in theData
   */
  STATIC_CONST( IND_BLOCK_REF     = 0 );
  STATIC_CONST( IND_DIR_RANGE     = 1 );
  STATIC_CONST( IND_DIR_ARRAY     = 2 );
  STATIC_CONST( IND_FRAGMENT      = 3 );
  STATIC_CONST( IND_OP_RECS       = 4 );
  STATIC_CONST( IND_OVERFLOW_RECS = 5 );
  STATIC_CONST( IND_PAGE8         = 6 );
  STATIC_CONST( IND_ROOT_FRAG     = 7 );
  STATIC_CONST( IND_TABLE         = 8 );
  STATIC_CONST( IND_SCAN          = 9 );
  
  /**
   * Use the index definitions to use the signal data
   */
  UintR theData[10];
};


#undef JAM_FILE_ID

#endif
