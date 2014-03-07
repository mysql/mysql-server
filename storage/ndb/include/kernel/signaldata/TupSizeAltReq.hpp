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

#ifndef TUP_SIZE_ALT_REQ_H
#define TUP_SIZE_ALT_REQ_H



#include "SignalData.hpp"

#define JAM_FILE_ID 115


class TupSizeAltReq  {
  /**
   * Sender(s)
   */
  friend class ClusterConfiguration;

  /**
   * Reciver(s)
   */
  friend class Dbtup;
private:
  /**
   * Indexes in theData
   */
  STATIC_CONST( IND_BLOCK_REF           = 0 );
  STATIC_CONST( IND_DISK_PAGE_ARRAY     = 1 );
  STATIC_CONST( IND_DISK_PAGE_REPRESENT = 2 );
  STATIC_CONST( IND_FRAG                = 3 );
  STATIC_CONST( IND_PAGE_CLUSTER        = 4 );
  STATIC_CONST( IND_LOGIC_PAGE          = 5 );
  STATIC_CONST( IND_OP_RECS             = 6 );
  STATIC_CONST( IND_PAGE                = 7 );
  STATIC_CONST( IND_PAGE_RANGE          = 8 );
  STATIC_CONST( IND_TABLE               = 9 );
  STATIC_CONST( IND_TABLE_DESC          = 10 );
  STATIC_CONST( IND_DELETED_BLOCKS      = 11 );
  STATIC_CONST( IND_STORED_PROC         = 12 );
  
  /**
   * Use the index definitions to use the signal data
   */
  UintR theData[13];
};


#undef JAM_FILE_ID

#endif
